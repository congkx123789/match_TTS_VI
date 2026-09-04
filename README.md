# 🎙️ Matcha-TTS Vietnamese — C++ Native Runtime

> **TTS tiếng Việt tốc độ cực cao trên CPU thuần** — đạt ~100x Realtime trên AMD Ryzen 9 9900X (Zen 5)  
> Không cần GPU. Không cần Python runtime. Chạy hoàn toàn bằng **C++ + ONNX Runtime**.

---

## ⚡ Hiệu năng đo thực tế

| Cấu hình | Steps | Tốc độ | Thời gian/Chapter |
|----------|-------|--------|------------------|
| INT8 MatMul-VNNI · 1 core | 2 steps · Sway | **37.6x RT** | ~15 giây |
| INT8 MatMul-VNNI · 4 core | 2 steps · Sway | **🏆 ~100x RT** | ~6 giây |
| INT8 MatMul-VNNI · 1 core | 1 step | 64.9x RT | ~9 giây |

> **Test trên:** AMD Ryzen 9 9900X (Zen 5, 12 core), 64MB L3 Cache, DDR5  
> **File test:** Chapter 1 "Thế Giới Hoàn Mỹ" — 11,849 ký tự → 584 giây audio (~9.7 phút)

---

## 📁 Cấu trúc Repository

```
match_TTS_VI/
├── cpp/
│   ├── include/
│   │   ├── tts_engine.hpp          # API chính của TTS Engine
│   │   ├── text_normalizer.hpp     # Chuẩn hóa văn bản tiếng Việt
│   │   ├── text_processor.hpp      # Xử lý phoneme / symbol
│   │   ├── matcha_tts.hpp          # Interface ONNX Matcha-TTS
│   │   ├── sonic.h                 # Google Sonic (time-stretch)
│   │   ├── dr_wav.h                # WAV I/O (single-header)
│   │   └── json.hpp                # nlohmann/json (single-header)
│   ├── src/
│   │   ├── main.cpp                # CLI chính
│   │   ├── tts_engine.cpp          # Core TTS Engine
│   │   ├── text_normalizer.cpp     # Pipeline chuẩn hóa 11 bước
│   │   ├── text_processor.cpp      # G2P / phoneme mapping
│   │   ├── sonic.c                 # Google Sonic WSOLA
│   │   ├── quantization_benchmark.cpp  # Benchmark INT8 vs FP32
│   │   ├── single_core_test.cpp    # Test 1-core vs multi-core
│   │   ├── bench_long_1core.cpp    # Test chapter dài
│   │   ├── bench_prompt_speed.cpp  # Test latency câu ngắn
│   │   ├── scenario_test.cpp       # Test các tình huống thực tế
│   │   └── inspect_steps.cpp       # Kiểm tra pipeline chuẩn hóa text
│   └── Makefile
├── models_onnx/
│   ├── matcha_encoder_int8.onnx    # Encoder INT8 MatMul-VNNI (28MB)
│   ├── matcha_decoder_int8.onnx    # Decoder INT8 MatMul-VNNI (24MB)
│   ├── vocos_matmul_int8.onnx      # Vocos Vocoder INT8 (15MB)
│   ├── prompt_encoder.onnx         # Prompt/Style encoder (152KB)
│   └── symbols.json                # Bảng từ điển phoneme tiếng Việt
├── pause_config.json               # Cấu hình ngắt nghỉ theo dấu câu
├── chapter_1_trieu_khi.txt         # File text mẫu để test
├── run_cpp.sh                      # Script build + chạy nhanh
└── README.md
```

---

## 🚀 Cài đặt & Build

### Yêu cầu hệ thống

- **CPU:** x86_64 với AVX-512 (Intel Ice Lake trở lên / AMD Zen 4+)
- **RAM:** Tối thiểu 4GB
- **Compiler:** GCC 10+ hoặc Clang 12+
- **ONNX Runtime:** ≥ 1.18.0

### 1. Cài ONNX Runtime

```bash
pip install onnxruntime
# Hoặc cài trực tiếp:
pip install onnxruntime==1.20.1
```

### 2. Build toàn bộ

```bash
cd cpp
make -j$(nproc)
```

Binary sẽ xuất ra thư mục `bin/`.

### 3. Chạy thử nhanh

```bash
# Đọc một câu
./bin/matcha_tts_cli -t "Xin chào, tôi là trợ lý đọc sách." -o outputs/test.wav

# Đọc file text dài (ví dụ: chapter truyện)
./bin/matcha_tts_cli -f chapter_1_trieu_khi.txt -o outputs/chapter_1.wav

# Tùy chỉnh thêm
./bin/matcha_tts_cli \
  -f chapter_1_trieu_khi.txt \
  -o outputs/chapter_1_fast.wav \
  --steps 2 \
  --speed 1.5 \
  --temp 0.9 \
  --threads 4
```

---

## 🎛️ Tham số CLI đầy đủ

```
Tùy chọn:
  -t, --text <văn bản>      Câu hoặc đoạn văn cần đọc
  -f, --file <đường dẫn>    Đọc nội dung từ tệp .txt
  -o, --output <đường dẫn>  File WAV đầu ra (mặc định: outputs/output.wav)
  -s, --steps <số>          Số bước ODE (1-10, mặc định: 2)
      --speed <tỷ lệ>       Tốc độ đọc WSOLA (1.0=chuẩn, 1.5, 2.0, 3.0)
      --pitch <giá trị>     Cao độ giọng (0.5-2.0, mặc định: 1.0)
      --temp <giá trị>      Temperature / cảm xúc (mặc định: 0.9)
      --threads <N>         Số CPU threads (mặc định: 4)
      --no-norm             Tắt chuẩn hóa số & ký tự ngoại ngữ
      --clean-only          Chỉ in text sau khi làm sạch rồi thoát
  -h, --help                Hiển thị hướng dẫn
```

---

## 🔬 Chạy Benchmark

```bash
# So sánh INT8 vs FP32, 1/2/10 steps (toàn diện nhất)
./bin/quantization_benchmark

# Test tốc độ 1 core vs đa core
./bin/single_core_test

# Test latency câu ngắn (phù hợp chatbot / trợ lý)
./bin/bench_prompt_speed

# Test chapter dài liên tục trên 1 core
./bin/bench_long_1core
```

---

## 🧹 Pipeline Chuẩn Hóa Văn Bản (11 bước)

Hệ thống tự động xử lý tiếng Việt trước khi TTS:

| Bước | Xử lý |
|------|-------|
| 1 | Loại bỏ ký tự điều khiển & HTML |
| 2 | Loại bỏ ngoặc nặng (【】, 【】, etc.) |
| 3 | Chuẩn hóa dấu câu & dấu nháy |
| 4 | Chuyển số → chữ (`123` → `một trăm hai mươi ba`) |
| 5 | Chuyển ngày tháng (`15/3/2024` → `ngày mười lăm tháng ba năm hai không hai bốn`) |
| 6 | Chuyển tiền tệ (`$100` → `một trăm đô la`) |
| 7 | Chuyển đơn vị kỹ thuật (`5km` → `năm kilômét`) |
| 8 | Dịch thuật ngữ ngoại lai / từ viết tắt |
| 9 | Loại bỏ ký tự đặc biệt không đọc được |
| 10 | Chuẩn hóa khoảng trắng |
| 11 | Tách câu thông minh theo dấu câu |

Kiểm tra pipeline:
```bash
./bin/inspect_steps  # Xem từng bước biến đổi
```

---

## 💡 Tại Sao Đạt ~100x Realtime?

1. **INT8 MatMul-VNNI** — tận dụng AVX-512 VNNI của Zen 4/5, nhanh ~2x so với INT8 naive
2. **2-Step Sway Sampling** — giảm 5x compute so với 10 steps, chất lượng tương đương
3. **Model 67MB ≤ L3 Cache** — weights nằm gọn trong cache, không bị RAM bottleneck
4. **4 threads pipeline** — batch nhiều câu song song, throughput ~2.7x
5. **ONNX Runtime** — tự động chọn kernel AVX-512 FMA tối ưu nhất

---

## 📊 Kiến Trúc Model

```
Text → [Text Normalizer] → [Phoneme Processor] → [Matcha Encoder]
                                                         ↓
                                             [Sway Sampling ODE Solver]
                                             2 steps: t = {0.5, 1.0}
                                                         ↓
                                             [Matcha Decoder INT8]
                                                         ↓
                                             [Vocos Vocoder INT8]
                                                         ↓
                                             [Google Sonic WSOLA]
                                                         ↓
                                                   WAV Audio
```

- **Matcha Encoder:** Transformer encoder, INT8 quantized
- **Matcha Decoder:** Flow matching ODE network, INT8 quantized
- **Vocos:** ConvNeXt + iSTFT vocoder, INT8 quantized
- **Sample rate:** 22050 Hz

---

## 📜 License

MIT License

---

*Được tối ưu hóa cho AMD Ryzen 9000 series (Zen 5) với AVX-512 VNNI support.*
