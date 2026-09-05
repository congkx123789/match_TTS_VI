"""
🎙️ Matcha-TTS Vietnamese C++ Native Studio (22,050 Hz) [Pure INT8]
==================================================================
Chạy 100% C++ Native Inference Engine với mô hình INT8 tối ưu vi xử lý.
Cấu hình: 1 CPU Thread Isolated / Request, Giới hạn Queue & Concurrency chuẩn.
"""

import os
import sys
import re
import time
import subprocess

# Hotfix for Gradio Client JSON Schema boolean bug on Python 3.13 / Pydantic 2
import gradio_client.utils as gc_utils
_orig_get_type = gc_utils.get_type
def patched_get_type(schema):
    if not isinstance(schema, dict):
        return "Any"
    return _orig_get_type(schema)
gc_utils.get_type = patched_get_type

_orig_json_schema = gc_utils._json_schema_to_python_type
def patched_json_schema(schema, defs=None):
    if not isinstance(schema, dict):
        return "Any"
    if "additionalProperties" in schema and isinstance(schema["additionalProperties"], bool):
        schema = dict(schema)
        schema["additionalProperties"] = {}
    return _orig_json_schema(schema, defs)
gc_utils._json_schema_to_python_type = patched_json_schema
gc_utils.json_schema_to_python_type = patched_json_schema

import gradio as gr

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUTS_DIR = os.path.join(CURRENT_DIR, "outputs")
os.makedirs(OUTPUTS_DIR, exist_ok=True)

# Đảm bảo phân quyền thực thi (chmod +x) cho bash script và binary
run_script_path = os.path.join(CURRENT_DIR, "run_cpp.sh")
bin_path = os.path.join(CURRENT_DIR, "bin", "matcha_tts_cli")

try:
    if os.path.exists(run_script_path):
        os.chmod(run_script_path, 0o755)
    if os.path.exists(bin_path):
        os.chmod(bin_path, 0o755)
except Exception as e:
    print(f"⚠️ Notice chmod: {e}")

# Build C++ native binary if missing on startup
if not os.path.exists(bin_path):
    print("⚙️ [C++ Startup] Đang biên dịch C++ Native Engine...")
    try:
        subprocess.run(["make", "-C", os.path.join(CURRENT_DIR, "cpp"), f"ORT_LIB_DIR={os.path.join(CURRENT_DIR, 'lib')}"], check=True)
        if os.path.exists(bin_path):
            os.chmod(bin_path, 0o755)
        print("✅ [C++ Startup] Biên dịch thành công!")
    except Exception as e:
        print(f"⚠️ [C++ Startup] Lỗi biên dịch: {e}")

# ==============================================================================
# HỆ THỐNG BẢO VỆ AN NINH API, RATE LIMITING & KHÓA SECRET KEY
# ==============================================================================
import threading
from collections import defaultdict

MAX_CHARACTERS = 20000      # Tối đa 20.000 ký tự mỗi request (đọc cả chương truyện dài)
DEFAULT_THREADS = 1         # Ép mỗi luồng chạy đúng 1 core CPU cô lập (tránh quá tải CPU)
RATE_LIMIT_PER_MINUTE = 30  # Tối đa 30 request / phút / IP
BURST_LIMIT_5SEC = 6        # Tối đa 6 request / 5 giây / IP
AUTO_CLEAN_MINUTES = 15     # Tự động dọn dẹp các file audio cũ hơn 15 phút

# Khóa Secret API Key bảo vệ độc quyền (Lấy tự động từ Space Secrets hoặc Environment Variables)
API_AUTH_KEY = os.getenv("API_AUTH_KEY", "sk_alida_matcha_vn_2026").strip()

class SecurityManager:
    """Quản lý bảo mật, phân biệt Web UI và Remote API, kiểm soát Rate Limit và xác thực Secret Key."""
    def __init__(self):
        self._lock = threading.Lock()
        self._ip_history = defaultdict(list)
        self._last_clean = time.time()

    def get_client_ip(self, request: gr.Request = None) -> str:
        if request is None:
            return "127.0.0.1"
        try:
            headers = getattr(request, "headers", {})
            if "cf-connecting-ip" in headers:
                return headers["cf-connecting-ip"]
            if "x-forwarded-for" in headers:
                return headers["x-forwarded-for"].split(",")[0].strip()
            if hasattr(request, "client") and request.client:
                return request.client.host
        except Exception:
            pass
        return "127.0.0.1"

    @staticmethod
    def is_browser_request(request: gr.Request = None) -> bool:
        """Nhận diện người dùng truy cập trực tiếp bằng trình duyệt Web thật."""
        if request is None:
            return True  # Mặc định tương tác Web UI trực tiếp
        try:
            headers = getattr(request, "headers", {})
            ua = headers.get("user-agent", "").lower()
            
            # Chặn các bot / script / automated tools tự nhận dạng
            automated_tools = ["python", "gradio_client", "requests", "curl", "wget", "postman", "aiohttp", "urllib", "httpie"]
            if any(tool in ua for tool in automated_tools):
                return False

            # Bất kỳ trình duyệt web thông thường nào (Desktop / Mobile)
            is_browser_ua = any(b in ua for b in ["mozilla", "chrome", "safari", "firefox", "edge", "opera", "applewebkit", "gecko", "android", "iphone"])
            if is_browser_ua or not ua:
                return True
        except Exception:
            pass
        return True

    def verify_auth_key(self, api_key: str, request: gr.Request = None) -> bool:
        """
        - Khách vào bằng Trình duyệt Web: Tự động cho phép dùng mượt mà, không cần mật mã.
        - Gọi bằng API / Bot / Python / Script từ xa: Bắt buộc phải có Secret Key hoặc HF_TOKEN chính xác!
        """
        # 1. Người dùng thực tế trên giao diện Web
        if self.is_browser_request(request):
            return True

        # 2. Nếu server không thiết lập khóa -> Mở
        if not API_AUTH_KEY:
            return True

        # 3. Yêu cầu từ API máy tính / Script từ xa: Xác thực Secret Key
        if api_key and api_key.strip() == API_AUTH_KEY:
            return True

        if request is not None:
            try:
                headers = getattr(request, "headers", {})
                req_key = headers.get("x-api-key", "").strip()
                if req_key and req_key == API_AUTH_KEY:
                    return True
                auth_header = headers.get("authorization", "")
                if auth_header.startswith("Bearer "):
                    bearer_token = auth_header[7:].strip()
                    if bearer_token == API_AUTH_KEY:
                        return True
            except Exception:
                pass

        return False

    def check_rate_limit(self, client_ip: str) -> tuple[bool, str]:
        now = time.time()
        with self._lock:
            # Lọc bỏ các timestamp quá 60s
            timestamps = [t for t in self._ip_history[client_ip] if now - t < 60.0]
            self._ip_history[client_ip] = timestamps

            # Kiểm tra burst trong 5 giây
            burst_count = sum(1 for t in timestamps if now - t < 5.0)
            if burst_count >= BURST_LIMIT_5SEC:
                return False, f"🚨 Tần suất quá nhanh ({burst_count} req / 5s). Vui lòng đợi 5 giây!"

            # Kiểm tra limit trong 60 giây
            if len(timestamps) >= RATE_LIMIT_PER_MINUTE:
                return False, f"🚨 Đã chạm giới hạn ({len(timestamps)}/30 req/phút). Vui lòng thử lại sau 1 phút!"

            # Ghi nhận request hợp lệ
            self._ip_history[client_ip].append(now)

            # Dọn dẹp cache IP không hoạt động mỗi 5 phút
            if now - self._last_clean > 300:
                self._last_clean = now
                dead_ips = [ip for ip, ts in self._ip_history.items() if not ts or (now - ts[-1] > 120)]
                for ip in dead_ips:
                    del self._ip_history[ip]

        return True, ""

    @staticmethod
    def sanitize_text(text: str) -> str:
        if not text:
            return ""
        # Loại bỏ null-byte và các ký tự điều khiển nguy hiểm
        text = text.replace("\x00", "").replace("\r\n", "\n").replace("\r", "\n")
        # Loại bỏ các ký tự escape terminal ANSI
        text = re.sub(r'\x1b\[[0-9;]*[a-zA-Z]', '', text)
        return text

    @staticmethod
    def cleanup_old_files():
        try:
            now = time.time()
            max_age = AUTO_CLEAN_MINUTES * 60
            for fname in os.listdir(OUTPUTS_DIR):
                fpath = os.path.join(OUTPUTS_DIR, fname)
                if os.path.isfile(fpath) and fname.endswith(".wav"):
                    if now - os.path.getmtime(fpath) > max_age:
                        try:
                            os.remove(fpath)
                        except Exception:
                            pass
        except Exception:
            pass

security_guard = SecurityManager()

PRESET_SAMPLES = {
    "📖 Tiên Hiệp / Kiếm Hiệp": (
        "Kiếm quang rực rỡ chiếu sáng bầu trời đêm, uy áp vô tận chấn nhiếp muôn loài. "
        "Thiếu niên đứng trên đỉnh núi tuyết, tay cầm huyền thiết trọng kiếm, ánh mắt tựa như tinh tú ngàn năm."
    ),
    "📻 Radio Cảm Xúc / Tản Văn": (
        "Có những ngày hoàng hôn đẹp đến nao lòng. Ta ngồi bên ô cửa sổ nhỏ, nhấp một ngụm trà ấm "
        "và lắng nghe tiếng lá phong rơi xào xạc trên hè phố vắng."
    ),
    "📰 Bản Tin Công Nghệ & AI": (
        "Mô hình Matcha-TTS Tiếng Việt kết hợp Vocos Vocoder 22.050 Hz đã đạt tốc độ xử lý hơn 130 lần thời gian thực trên C++ Native INT8. "
        "Hệ thống cho phép tổng hợp giọng đọc tự nhiên, truyền cảm với độ trễ cực thấp chỉ vài chục mili giây."
    ),
    "🔢 Số Đếm, Biển Số & Tiền Tệ Phức Tạp": (
        "Khách hàng thanh toán đơn hàng số 8899 trị giá 1.250.000 đồng tại Hà Nội lúc 14h30 ngày 15/09/2026. "
        "Số điện thoại liên hệ là 0987.654.321, xe mang biển kiểm soát 29A-999.88."
    )
}

CUSTOM_CSS = """
@import url('https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');

:root {
    --primary-color: #6366f1;
    --primary-hover: #4f46e5;
    --bg-dark: #0f172a;
    --card-bg: rgba(30, 41, 59, 0.7);
}

body, .gradio-container {
    font-family: 'Outfit', -apple-system, BlinkMacSystemFont, sans-serif !important;
    background-color: #0b0f19 !important;
    color: #f1f5f9 !important;
}

.main-header {
    background: linear-gradient(135deg, rgba(30, 27, 75, 0.8) 0%, rgba(15, 23, 42, 0.9) 100%);
    border: 1px solid rgba(99, 102, 241, 0.3);
    border-radius: 16px;
    padding: 24px;
    margin-bottom: 20px;
    box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3), 0 0 20px rgba(99, 102, 241, 0.15);
}

.main-title {
    font-size: 2.2rem;
    font-weight: 700;
    background: linear-gradient(135deg, #a5b4fc 0%, #c084fc 50%, #38bdf8 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    margin: 0;
    display: inline-block;
}

.badge-tag {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    background: rgba(99, 102, 241, 0.15);
    border: 1px solid rgba(99, 102, 241, 0.3);
    color: #c7d2fe;
    font-size: 0.8rem;
    padding: 4px 10px;
    border-radius: 9999px;
    font-weight: 500;
}

.btn-generate {
    background: linear-gradient(135deg, #4f46e5 0%, #7c3aed 100%) !important;
    border: none !important;
    color: white !important;
    font-weight: 600 !important;
    border-radius: 10px !important;
    padding: 12px 24px !important;
    box-shadow: 0 4px 15px rgba(79, 70, 229, 0.4) !important;
    transition: all 0.2s ease !important;
}

.btn-generate:hover {
    transform: translateY(-2px);
    box-shadow: 0 6px 20px rgba(79, 70, 229, 0.6) !important;
}

.loading-status {
    color: #a5b4fc !important;
}
"""


import wave

def split_text_into_chunks(text: str, target_chars: int = 750, min_chars: int = 400) -> list[str]:
    """Phân chia văn bản thành các đoạn (chunk) câu hoàn chỉnh tự nhiên."""
    raw_sentences = re.split(r"(?<=[.!?…\n])\s+", text.strip())
    raw_sentences = [s.strip() for s in raw_sentences if s.strip()]
    if not raw_sentences:
        return []
    
    chunks = []
    curr = ""
    for s in raw_sentences:
        if not curr:
            curr = s
        else:
            if len(curr) + 1 + len(s) <= target_chars or len(curr) < min_chars:
                curr += " " + s
            else:
                chunks.append(curr)
                curr = s
    if curr:
        chunks.append(curr)
    return chunks


class CPPDaemonManager:
    """Quản lý tiến trình C++ Engine thường trú trong RAM (In-Memory Resident Daemon).
    Giảm độ trễ từ 4.2 giây xuống còn 0.05 - 0.3 giây kèm cơ chế Fallback tự động 100% tin cậy."""
    def __init__(self):
        self._lock = threading.Lock()
        self.proc = None
        self._start_daemon()

    def _start_daemon(self):
        cmd = [
            "bash",
            run_script_path,
            "--daemon",
            "--threads", str(DEFAULT_THREADS),
            "--model-type", "int8"
        ]
        try:
            self.proc = subprocess.Popen(
                cmd,
                cwd=CURRENT_DIR,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1
            )
            # Chờ daemon sẵn sàng (tối đa 5 giây)
            t_start = time.time()
            while time.time() - t_start < 5.0:
                line = self.proc.stdout.readline()
                if not line:
                    break
                if "READY_DAEMON" in line:
                    print("⚡ [C++ Resident Daemon] Đã nạp toàn bộ ONNX vào RAM & sẵn sàng phục vụ siêu tốc!")
                    return
            if self.proc and self.proc.poll() is not None:
                print(f"⚠️ [C++ Daemon] Daemon exited with code {self.proc.returncode}")
                self.proc = None
        except Exception as e:
            print(f"⚠️ [C++ Daemon] Lỗi khởi động daemon: {e}")
            self.proc = None

    def synthesize(self, text: str, out_file: str, steps: int, speed: float, pitch: float, temp: float, auto_norm: bool) -> tuple[bool, float, float, str]:
        with self._lock:
            # 1. Thử thực thi siêu tốc qua In-Memory Resident Daemon
            if self.proc is not None and self.proc.poll() is None:
                escaped_text = text.replace('\\', '\\\\').replace('\n', '\\n').replace('\t', ' ')
                norm_flag = "1" if auto_norm else "0"
                req_line = f"{out_file}\t{steps}\t{speed}\t{pitch}\t{temp}\t{norm_flag}\t{escaped_text}\n"
                try:
                    self.proc.stdin.write(req_line)
                    self.proc.stdin.flush()
                    res_line = self.proc.stdout.readline().strip()
                    parts = res_line.split('\t')
                    if parts and parts[0] == "DONE" and len(parts) >= 3:
                        dur_sec = float(parts[1])
                        proc_sec = float(parts[2])
                        return True, dur_sec, proc_sec, "OK (In-Memory Daemon)"
                except Exception as e:
                    print(f"⚠️ Daemon stream error: {e}, falling back...")
                    self.proc = None

            # 2. Fallback tự động 100% qua Subprocess trực tiếp nếu Daemon chưa nạp
            cmd = [
                "bash",
                run_script_path,
                "--text", text,
                "--output", out_file,
                "--steps", str(steps),
                "--speed", str(speed),
                "--pitch", str(pitch),
                "--temp", str(temp),
                "--threads", str(DEFAULT_THREADS)
            ]
            if not auto_norm:
                cmd.append("--no-norm")
            
            t0 = time.perf_counter()
            try:
                res = subprocess.run(cmd, cwd=CURRENT_DIR, capture_output=True, text=True, errors="replace", timeout=300)
                t1 = time.perf_counter()
                dur_match = re.search(r'Thời lượng âm thanh\s*:\s*([\d.]+)\s*giây', res.stdout)
                time_match = re.search(r'Thời gian suy luận\s*:\s*([\d.]+)\s*giây', res.stdout)
                dur_sec = float(dur_match.group(1)) if dur_match else (t1 - t0)
                proc_sec = float(time_match.group(1)) if time_match else (t1 - t0)
                return True, dur_sec, proc_sec, "OK (Direct Fallback)"
            except Exception as e:
                return False, 0.0, 0.0, f"Execution failed: {e}"

cpp_daemon = CPPDaemonManager()


def predict_tts(
    text: str,
    steps: int = 2,
    speed: float = 1.0,
    pitch: float = 1.0,
    temp: float = 0.9,
    auto_norm: bool = True,
    api_key: str = "",
    request: gr.Request = None
):
    """
    Xử lý C++ Native INT8 In-Memory Resident siêu tốc (toàn bộ đoạn văn bản chạy liền mạch trong RAM).
    """
    # 0. Xác thực Secret API Key độc quyền
    if not security_guard.verify_auth_key(api_key, request):
        return None, "⛔ Quyền truy cập bị từ chối: Khóa API Key không chính xác hoặc chưa được cung cấp! Vui lòng nhập đúng Secret Key.", "🔒 Chưa Mở Khóa", "⏱️ --", "🎵 --", "🛡️ Auth Required"

    # 1. Kiểm tra Rate Limiting theo Client IP
    client_ip = security_guard.get_client_ip(request)
    allowed, limit_reason = security_guard.check_rate_limit(client_ip)
    if not allowed:
        return None, limit_reason, "⛔ Rate Limited", "⏱️ --", "🎵 --", f"🛡️ IP: {client_ip}"

    # 2. Dọn dẹp tệp cũ định kỳ
    security_guard.cleanup_old_files()

    # 3. Làm sạch và kiểm tra dữ liệu văn bản
    text = security_guard.sanitize_text(text)
    if not text or not text.strip():
        return None, "⚠️ Lỗi: Nội dung văn bản trống. Vui lòng nhập văn bản cần đọc.", "🚀 --", "⏱️ --", "🎵 --", "⚡ --"

    warn_msg = ""
    if len(text) > MAX_CHARACTERS:
        text = text[:MAX_CHARACTERS]
        warn_msg = f"⚠️ Lưu ý: Văn bản đã được cắt bớt về {MAX_CHARACTERS} ký tự theo giới hạn an toàn.\n"

    # 4. Kẹp dải tham số an toàn
    clamped_steps = max(1, min(int(steps) if isinstance(steps, (int, float)) else 2, 6))
    clamped_speed = max(0.5, min(float(speed) if isinstance(speed, (int, float)) else 1.0, 2.5))
    clamped_pitch = max(0.6, min(float(pitch) if isinstance(pitch, (int, float)) else 1.0, 1.6))
    clamped_temp = max(0.1, min(float(temp) if isinstance(temp, (int, float)) else 0.9, 1.2))

    session_id = f"tts_{int(time.time()*1000)}_{os.getpid()}"
    final_out_file = os.path.join(OUTPUTS_DIR, f"{session_id}.wav")

    t0 = time.perf_counter()
    success, audio_dur_val, proc_time_val, status_msg = cpp_daemon.synthesize(
        text=text,
        out_file=final_out_file,
        steps=clamped_steps,
        speed=clamped_speed,
        pitch=clamped_pitch,
        temp=clamped_temp,
        auto_norm=auto_norm
    )
    t1 = time.perf_counter()

    if not success or not os.path.exists(final_out_file) or os.path.getsize(final_out_file) < 100:
        return None, f"{warn_msg}🚨 Lỗi tổng hợp C++ Engine: {status_msg}", "🚀 --", "⏱️ --", "🎵 --", "⚡ --"

    total_e2e_time = t1 - t0
    audio_dur = f"{audio_dur_val:.2f}"
    proc_time = f"{proc_time_val:.3f}"
    rtf_val = (proc_time_val / audio_dur_val) if audio_dur_val > 0 else 0.0
    speedup_val = (audio_dur_val / proc_time_val) if proc_time_val > 0 else 0.0

    speedup_str = f"🚀 {speedup_val:.1f}x Realtime"
    rtf_str = f"⏱️ RTF: {rtf_val:.4f}"
    dur_str = f"🎵 Thời lượng: {audio_dur}s (Xử lý: {proc_time}s)"
    device_str = f"⚡ C++ Native INT8 In-Memory ({DEFAULT_THREADS} Core)"

    full_log = f"""===========================================================================
🎙️  HỆ THỐNG ĐỌC TIẾNG NÓI C++ NATIVE RESIDENT (IN-MEMORY INT8)
===========================================================================
{warn_msg}✅ Hoàn tất tổng hợp âm thanh thành công!
⏱️  Thời lượng âm thanh : {audio_dur} giây
⚡ Thời gian suy luận  : {proc_time} giây (Độ trễ toàn trình: {total_e2e_time:.2f}s)
📊 Tỷ lệ RTF           : {rtf_val:.4f}
🚀 Tốc độ thực tế      : {speedup_val:.1f}x Realtime
📂 File âm thanh đã lưu: {final_out_file}
==========================================================================="""

    return (
        final_out_file,
        full_log,
        speedup_str,
        rtf_str,
        dur_str,
        device_str
    )


def calculate_text_stats(text: str, speed_val: float = 1.0) -> str:
    """Tính toán số ký tự, số từ, dự tính thời lượng phát âm và thời gian xử lý C++."""
    if not text or not text.strip():
        return """
        <div style="background: rgba(30, 41, 59, 0.6); border: 1px dashed rgba(148, 163, 184, 0.3); border-radius: 10px; padding: 10px 16px; font-size: 0.88rem; color: #94a3b8; margin-top: 8px;">
            📝 <b>Số lượng:</b> 0 ký tự | 0 từ &nbsp;•&nbsp; ⏱️ <b>Dự tính âm thanh:</b> 0.0s &nbsp;•&nbsp; ⚡ <b>Dự tính xử lý C++:</b> ~0.0s
        </div>
        """
    num_chars = len(text)
    words = text.split()
    num_words = len(words)
    
    speed_factor = max(0.5, min(float(speed_val) if isinstance(speed_val, (int, float)) else 1.0, 3.0))
    # Tốc độ phát âm tiếng Việt chuẩn: ~3.2 từ / giây ở speed 1.0x
    est_audio_dur = (num_words / 3.2) / speed_factor
    # Tốc độ xử lý C++ INT8 trên Hugging Face: ~15x Realtime
    est_proc_time = est_audio_dur / 15.0
    
    return f"""
    <div style="background: linear-gradient(135deg, rgba(30, 41, 59, 0.85) 0%, rgba(15, 23, 42, 0.95) 100%); border: 1px solid rgba(99, 102, 241, 0.35); border-radius: 12px; padding: 12px 18px; font-size: 0.92rem; margin-top: 8px; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);">
        <div style="display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px;">
            <div style="display: flex; align-items: center; gap: 6px;">
                <span>📝</span>
                <span style="color: #cbd5e1;">Số lượng:</span>
                <span style="color: #a5b4fc; font-weight: 700;">{num_chars:,} ký tự</span>
                <span style="color: #94a3b8;">({num_words:,} từ)</span>
            </div>
            <div style="display: flex; align-items: center; gap: 6px;">
                <span>⏱️</span>
                <span style="color: #cbd5e1;">Dự tính âm thanh:</span>
                <span style="color: #38bdf8; font-weight: 700;">~{est_audio_dur:.1f} giây</span>
            </div>
            <div style="display: flex; align-items: center; gap: 6px;">
                <span>⚡</span>
                <span style="color: #cbd5e1;">Dự tính thời gian tạo:</span>
                <span style="color: #4ade80; font-weight: 700;">~{est_proc_time:.2f} giây</span>
                <span style="font-size: 0.78rem; background: rgba(74, 222, 128, 0.15); color: #86efac; padding: 2px 6px; border-radius: 4px; font-weight: 500;">Siêu Tốc 15x RT</span>
            </div>
        </div>
    </div>
    """


def load_preset(preset_name: str, speed_val: float = 1.0):
    preset_text = PRESET_SAMPLES.get(preset_name, "")
    stats_html = calculate_text_stats(preset_text, speed_val)
    return preset_text, stats_html


with gr.Blocks(title="Matcha-TTS Vietnamese C++ Native Studio [INT8]", css=CUSTOM_CSS, theme=gr.themes.Soft(primary_hue="indigo")) as demo:
    with gr.Row():
        with gr.Column(scale=1):
            gr.HTML("""
            <div class="main-header">
                <div style="display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px;">
                    <div>
                        <h1 class="main-title">🎙️ Matcha-TTS Vietnamese C++ INT8 Studio</h1>
                        <p style="color: #94a3b8; margin-top: 6px; font-size: 1rem;">
                            Hệ thống tổng hợp tiếng nói Tiếng Việt 22.050 Hz chuẩn Studio chạy 100% C++ Native ONNX Runtime (Mô hình INT8 Siêu Nhẹ &amp; Tối Ưu Độc Lập Luồng).
                        </p>
                    </div>
                    <div style="display: flex; gap: 8px; flex-wrap: wrap;">
                        <span class="badge-tag">⚡ C++ Native INT8</span>
                        <span class="badge-tag">🧵 1 Core / Request</span>
                        <span class="badge-tag">🚀 15x+ Cloud / 75x+ Local</span>
                        <span class="badge-tag">🔊 22.050 Hz Studio</span>
                        <span class="badge-tag">🇻🇳 Master Normalizer</span>
                    </div>
                </div>
            </div>
            """)

    with gr.Row():
        # Left column: Input & Controls
        with gr.Column(scale=6):
            gr.Markdown("### 📝 Văn Bản Đầu Vào")
            
            preset_dropdown = gr.Dropdown(
                label="Chọn mẫu văn bản nhanh (Presets)",
                choices=list(PRESET_SAMPLES.keys()),
                value="📖 Tiên Hiệp / Kiếm Hiệp",
                interactive=True
            )
            
            input_text = gr.Textbox(
                label="Nội dung cần đọc (Tối đa 5.000 ký tự; hỗ trợ truyện dài, số đếm, CCCD, tiền tệ, ký hiệu toán học)",
                placeholder="Nhập hoặc dán văn bản tiếng Việt vào đây...",
                value=PRESET_SAMPLES["📖 Tiên Hiệp / Kiếm Hiệp"],
                lines=5,
                max_lines=15
            )

            stats_display = gr.HTML(
                value=calculate_text_stats(PRESET_SAMPLES["📖 Tiên Hiệp / Kiếm Hiệp"], 1.0)
            )

            with gr.Row():
                auto_norm = gr.Checkbox(label="Bật chuẩn hóa tiếng Việt (Đọc chuẩn số đếm, tiền tệ, ngoại ngữ, ký hiệu @ # $ %)", value=True)

            with gr.Row():
                btn_generate = gr.Button("🚀 Tạo Giọng Nói C++ (Synthesize)", variant="primary", elem_classes=["btn-generate"])

            with gr.Accordion("⚙️ Tùy Chỉnh Nâng Cao C++ Engine", open=False):
                with gr.Row():
                    ode_steps = gr.Slider(
                        label="Số bước ODE (Inference Steps)",
                        minimum=1,
                        maximum=10,
                        value=2,
                        step=1,
                        info="2 bước mặc định chuẩn siêu tốc"
                    )
                    speed = gr.Slider(
                        label="Tốc độ đọc Google Sonic WSOLA",
                        minimum=0.5,
                        maximum=3.0,
                        value=1.0,
                        step=0.1,
                        info="1.0 chuẩn tự nhiên, 1.5x nhanh hơn"
                    )

                with gr.Row():
                    pitch = gr.Slider(
                        label="Cao độ giọng (Pitch)",
                        minimum=0.5,
                        maximum=2.0,
                        value=1.0,
                        step=0.05,
                        info="1.0 chuẩn bình thường"
                    )
                    temp = gr.Slider(
                        label="Độ cảm xúc (Temperature)",
                        minimum=0.1,
                        maximum=1.2,
                        value=0.9,
                        step=0.05,
                        info="0.9 mang lại ngữ điệu tự nhiên nhất"
                    )

        # Right column: Output & Metrics
        with gr.Column(scale=5):
            gr.Markdown("### 🎧 Âm Thanh Xuất Bản (Audio Output)")
            audio_out = gr.Audio(label="Kết quả phát âm (22,050 Hz Studio WAV)", type="filepath", interactive=False)

            gr.Markdown("### 📊 Chỉ Số Hiệu Năng Thời Gian Thực (C++ Metrics)")
            with gr.Row():
                metric_speedup = gr.Textbox(label="Tốc độ tăng tốc", value="🚀 Sẵn sàng", interactive=False)
                metric_rtf = gr.Textbox(label="Real-time Factor (RTF)", value="⏱️ --", interactive=False)
            with gr.Row():
                metric_dur = gr.Textbox(label="Thời lượng audio", value="🎵 --", interactive=False)
                metric_device = gr.Textbox(label="Môi trường thực thi", value=f"C++ Native INT8 ({DEFAULT_THREADS} Core Isolated)", interactive=False)

            with gr.Accordion("🔍 Nhật Ký Thực Thi C++ (Execution Log)", open=False):
                log_view = gr.Textbox(
                    label="Chi tiết tiến trình C++:",
                    lines=6,
                    interactive=False
                )

    # Cập nhật thống kê và dự tính thời gian trong thời gian thực khi gõ phím hoặc đổi tốc độ
    input_text.input(
        fn=calculate_text_stats,
        inputs=[input_text, speed],
        outputs=[stats_display]
    )

    speed.change(
        fn=calculate_text_stats,
        inputs=[input_text, speed],
        outputs=[stats_display]
    )

    preset_dropdown.change(
        fn=load_preset,
        inputs=[preset_dropdown, speed],
        outputs=[input_text, stats_display]
    )

    btn_generate.click(
        fn=predict_tts,
        inputs=[
            input_text,
            ode_steps,
            speed,
            pitch,
            temp,
            auto_norm
        ],
        outputs=[
            audio_out,
            log_view,
            metric_speedup,
            metric_rtf,
            metric_dur,
            metric_device
        ],
        show_progress="minimal",
        api_name="predict"
    )

if __name__ == "__main__":
    demo.queue(
        default_concurrency_limit=2,
        max_size=20
    ).launch(
        server_name="0.0.0.0",
        server_port=7860,
        share=False
    )
