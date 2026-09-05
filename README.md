# 🎙️ Matcha-TTS + Vocos Vietnamese Standalone Pipeline (C++ Native & GPU)

<p align="center">
  <a href="https://huggingface.co/spaces/Cong123779/matcha-tts"><img src="https://img.shields.io/badge/🤗%20Hugging%20Face%20Space-Live%20Demo-indigo?style=for-the-badge&logo=huggingface" alt="Space Demo"></a>
  <a href="https://huggingface.co/Cong123779/matcha-tts-vietnamese-onnx-int8"><img src="https://img.shields.io/badge/🤗%20Model%20Hub-ONNX%20INT8-orange?style=for-the-badge&logo=huggingface" alt="Model Hub"></a>
  <a href="https://github.com/congkx123789/match_TTS_VI"><img src="https://img.shields.io/badge/GitHub-Source%20Code-black?style=for-the-badge&logo=github" alt="GitHub"></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="License"></a>
</p>

Hệ thống tổng hợp tiếng nói tiếng Việt (Vietnamese Text-to-Speech) **C++ Native Siêu Tốc & Standalone**, kết hợp **Matcha-TTS** và **Vocos Vocoder 22,050Hz**.

Hỗ trợ đồng thời:
- **C++ Native Standalone:** Chạy trực tiếp qua file nhị phân C++ (tự động biên dịch `g++ -O3`), không cần cài đặt Python, tương thích cả **CPU đa luồng AVX2/AVX-512** và **GPU NVIDIA CUDA Tensor Cores**.
- **PyTorch GPU Pipeline:** Chạy qua Python với tốc độ cực đại lên tới **>2,100x Realtime** nhờ kỹ thuật Dynamic Batching trên GPU Blackwell/Ada.
- **Web Demo & API:** Trải nghiệm trực tuyến trên [Hugging Face Space](https://huggingface.co/spaces/Cong123779/matcha-tts).


---

## 📁 Cấu Trúc Dự Án

```text
match_TTS_VI/
├── cpp/
│   ├── include/                   # Toàn bộ header C++ độc lập & tối ưu
│   │   ├── onnxruntime/           # Official ONNX Runtime C/C++ SDK (CUDA & CPU)
│   │   ├── matcha_tts.hpp         # C++ High-Level Clean API (nhúng vào App chỉ 3 dòng)
│   │   ├── tts_engine.hpp         # Core Inference Pipeline & Voice Prompt Chaining
│   │   ├── text_processor.hpp     # Bộ xử lý âm vị & symbols.json
│   │   ├── text_normalizer.hpp    # Bộ làm sạch & chuẩn hóa số/ngoại ngữ thuần C++
│   │   ├── dr_wav.h               # Mã hóa/giải mã file WAV 16-bit PCM độc lập
│   │   ├── sonic.h                # Thuật toán Google Sonic WSOLA tua tốc độ phát lại
│   │   └── json.hpp               # Nlohmann JSON parser (header-only)
│   ├── src/                       # Mã nguồn C++ Native
│   │   ├── main.cpp               # CLI Binary Interface
│   │   ├── tts_engine.cpp         # Thuật toán giải vi phân ODE (Euler / Sway)
│   │   ├── text_processor.cpp     # Grapheme cleaners & symbol mappings
│   │   ├── text_normalizer.cpp    # Chuẩn hóa số, ngày tháng, ngoại ngữ
│   │   └── sonic.c                # Google Sonic core implementation
│   └── Makefile                   # Tự động liên kết thư viện ONNX Runtime & NVIDIA CUDA
├── models_onnx/                   # Mô hình ONNX đóng gói trọn gói (Self-contained)
│   ├── fp32/                      # FP32 tối ưu cho CPU AVX-512 FMA & CUDA (~124 MB)
│   ├── int8/                      # INT8 MatMul-VNNI siêu nhẹ cho CPU yếu (~66 MB)
│   ├── fp16/                      # FP16 Tensor Cores cho GPU thế hệ mới (~63 MB)
│   ├── prompt_encoder.onnx        # Bộ mã hóa Voice Mel Prompt (152 KB)
│   └── symbols.json               # Bảng từ điển ký tự tiếng Việt
├── models/                        # Checkpoint PyTorch gốc sạch (<130 MB)
│   ├── matcha_epoch10_clean.pt
│   └── vocos_epoch10_pure_generator.ckpt
├── run_cpp.sh                     # Script 1-Click tự động phát hiện driver & chạy C++
├── pause_config.json              # Cấu hình thời gian ngắt nghỉ theo từng dấu câu
└── README.md                      # Hướng dẫn chi tiết
```

---

## ⚡ 1. Hướng Dẫn Sử Dụng C++ Native (Khuyên Dùng)

Chỉ cần clone về và chạy ngay bằng script `./run_cpp.sh`. Script sẽ **tự động biên dịch C++ native** lần đầu tiên chỉ trong 2 giây.

### 🎮 A. Chạy Trên GPU NVIDIA (CUDA):
```bash
# Đọc một câu trực tiếp:
./run_cpp.sh --gpu -t "Chào mừng bạn đến với hệ thống đọc tiếng nói C++ Native siêu tốc!" -o output.wav

# Đọc toàn bộ một chương truyện dài từ file .txt:
./run_cpp.sh --gpu -s 2 -f chapter_1_trieu_khi.txt -o chapter1.wav
```

### 🖥️ B. Chạy Trên CPU Đa Luồng (Mọi Máy Tính Không Cần GPU):
```bash
# Chạy với 4 luồng CPU và 2 bước Sway siêu tốc:
./run_cpp.sh --cpu -s 2 -j 4 -t "Xin chào bạn, tôi đang chạy hoàn toàn bằng CPU." -o cpu_output.wav

# Đọc file truyện trên CPU 8 luồng:
./run_cpp.sh --cpu -s 2 -j 8 -f chapter_1_trieu_khi.txt -o chapter1_cpu.wav
```

---

## ⚙️ Bảng Chi Tiết Toàn Bộ Tham Số C++ CLI

| Tham Số | Tùy Chọn Ngắn | Giá Trị Mặc Định | Mô Tả Chi Tiết & Ý Nghĩa |
| :--- | :---: | :---: | :--- |
| `--text <str>` | `-t` | *"..."* | Chuỗi văn bản cần đọc trực tiếp từ dòng lệnh. |
| `--file <path>` | `-f` | *None* | Đường dẫn tệp `.txt` chứa đoạn văn hoặc toàn bộ chương sách. |
| `--output <path>` | `-o` | `outputs/output.wav` | Đường dẫn lưu tệp âm thanh `.wav` đầu ra. |
| `--steps <int>` | `-s` | `2` | Số bước giải vi phân ODE (1: Cực đại, 2: Sway Siêu Tốc chuẩn Studio, 10: Euler). |
| `--gpu` | — | *false* | Kích hoạt phần cứng GPU qua **NVIDIA CUDA Execution Provider**. |
| `--cpu` | — | *true* | Chạy trên CPU thuần với tập lệnh **AVX-512 FMA**. |
| `--threads <int>` | `-j` | `4` | Số luồng CPU thực thi song song (tối ưu: bằng số core vật lý của CPU). |
| `--speed <float>` | `-r` | `1.0` | Tua nhanh/chậm bằng **Google Sonic WSOLA** (1.5x, 2.0x, 3.0x mà giữ nguyên cao độ). |
| `--pitch <float>` | — | `1.0` | Điều chỉnh cao độ giọng đọc (0.8: Trầm ấm, 1.0: Chuẩn, 1.2: Trong trẻo). |
| `--temp <float>` | — | `0.9` | Nhiệt độ biểu cảm (Độ tự nhiên và nhấn nhá cảm xúc của giọng nói). |
| `--length-scale <float>` | — | `1.0` | Tỷ lệ co giãn âm vị AI gốc (chuẩn con người: 1.0). |
| `--model-type <str>` | — | `fp32` | Định dạng mô hình ONNX (`fp32`: Chuẩn nhất, `int8`: Siêu nhẹ CPU, `fp16`: Tensor Cores). |
| `--pause-config <path>` | — | `pause_config.json` | Tệp JSON cấu hình thời lượng ngắt nghỉ động theo từng loại dấu câu. |
| `--pause <float>` | — | *Động* | Cố định khoảng nghỉ giữa các câu theo giây (ghi đè file JSON). |
| `--tail-words <int>` | — | `10` | Số từ đuôi câu trước được trích xuất làm **Voice Mel Prompt** cho câu sau. |
| `--prompt` | — | *true* | Bật cơ chế Voice Prompt Chaining nối câu tự nhiên. |
| `--no-prompt` | — | *false* | Tắt Voice Prompt, đọc từng câu độc lập. |
| `--no-norm` | — | *false* | Tắt bộ chuẩn hóa số, ngày tháng, ngoại ngữ tự động. |
| `--clean-only` | — | *false* | Chỉ in văn bản sau khi làm sạch ra màn hình rồi thoát. |

---

## 🎯 2. Nhúng C++ Native Vào Ứng Dụng (Chỉ 3 Dòng Code)

Nếu muốn nhúng Matcha-TTS vào Game Engine, Audio App, Robot hoặc Server streaming:

```cpp
#include "matcha_tts.hpp"

int main() {
    MatchaTTS tts;
    tts.init("models_onnx", /*use_gpu=*/true, "fp32");
    
    // Đọc ra file wav:
    tts.speak_to_file("Xin chào thế giới C++ Native!", "chao.wav");
    
    // Hoặc lấy mảng float32 audio samples (22050Hz):
    std::vector<float> audio = tts.speak("Dữ liệu âm thanh thô phục vụ streaming.");
    return 0;
}
```

---

## 📊 3. Bảng Đo Kiểm Hiệu Năng Thực Tế (Benchmark)

Đo kiểm trên cấu hình: **CPU AMD Ryzen 9 9900X (12C/24T)** & **GPU NVIDIA GeForce RTX 5060 Ti Blackwell**:

| Kịch Bản Chạy | Thiết Bị & Phương Thức | Thời Lượng Audio | Thời Gian Xử Lý | Tốc Độ Thực Tế (Realtime) | Tỷ Lệ RTF |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **GPU Batching 128-256** | **RTX 5060 Ti (FP16 Tensor Cores)** | **~29 Phút (1,735s)** | **0.82 giây** | **🔥 >2,100x Realtime** | **0.00047** |
| **C++ GPU 1-Step** | **RTX 5060 Ti (CUDA C++)** | **~10 Phút (582s)** | **1.87 giây** | **310.3x Realtime** | **0.0032** |
| **C++ GPU 2-Step Sway** | **RTX 5060 Ti (CUDA C++)** | **~10 Phút (582s)** | **1.98 giây** | **292.9x Realtime** | **0.0034** |
| **C++ CPU 4 Luồng** | **Ryzen 9 9900X (AVX-512 FMA)** | **~10 Phút (582s)** | **4.29 giây** | **136.6x Realtime** | **0.0073** |
| **C++ CPU 1 Câu Lẻ** | **Ryzen 9 9900X (1 Câu ~3s)** | **3.0 giây** | **0.03 giây** | **~100x Realtime** | **0.0100** |

---

## 📜 4. License
Dự án được phân phối phục vụ mục đích nghiên cứu và phát triển hệ thống tổng hợp tiếng nói tiếng Việt tốc độ cao.
