---
language:
- vi
license: mit
library_name: onnxruntime
tags:
- text-to-speech
- tts
- vietnamese
- matcha-tts
- vocos
- onnx
- int8
- audio
- speech-synthesis
- flow-matching
- 22050hz
- c++
metrics:
- rtf
pipeline_tag: text-to-speech
---

# 🎙️ Matcha-TTS Vietnamese (22.050 Hz) - Pure INT8 / ONNX Models

<p align="center">
  <a href="https://huggingface.co/spaces/Cong123779/matcha-tts"><img src="https://img.shields.io/badge/🤗%20Hugging%20Face%20Space-Live%20Demo-indigo?style=for-the-badge&logo=huggingface" alt="Space Demo"></a>
  <a href="https://github.com/congkx123789/match_TTS_VI"><img src="https://img.shields.io/badge/GitHub-Source%20Code-black?style=for-the-badge&logo=github" alt="GitHub"></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="License"></a>
  <img src="https://img.shields.io/badge/Sample%20Rate-22.050%20Hz-blue?style=for-the-badge" alt="Sample Rate">
  <img src="https://img.shields.io/badge/Model%20Size-~68%20MB%20(INT8)-orange?style=for-the-badge" alt="Model Size">
</p>

Bộ mô hình **Matcha-TTS Tiếng Việt (Flow-Matching ODE) + Vocos Vocoder (22.050 Hz)** đã được tối ưu hóa và lượng tử hóa **INT8** chuẩn xác cao, chạy siêu tốc trên **ONNX Runtime (C++ Native & Python)**.

---

## 🌟 Điểm Nổi Bật (Key Highlights)

- ⚡ **Siêu Nhẹ & Tối Ưu (Pure INT8):** Toàn bộ hệ thống mô hình chỉ nặng khoảng **68 MB**, tiết kiệm 70% dung lượng so với bản FP32 gốc.
- 🚀 **Tốc Độ Vượt Trội (15x - 75x Realtime):** 
  - Đạt **15x – 25x Realtime** trên CPU 1 Core (Hugging Face Free Tier).
  - Đạt **> 65x – 75x Realtime** trên CPU máy bàn / laptop cá nhân (AVX2/VNNI).
  - Độ trễ phản hồi cực thấp: **chỉ ~30ms - 250ms** cho một câu nói.
- 🔊 **Chất Lượng Âm Thanh Studio 22.050 Hz:** Kết hợp bộ giải mã sóng âm thần kinh **Vocos Vocoder** cho giọng đọc truyền cảm, trong trẻo và tự nhiên.
- 📖 **Hỗ Trợ Đọc Truyện Dài & Cắt Đoạn Tự Nhiên (~30s - 45s Chunks):** Thuật toán chia đoạn thông minh theo ngữ cảnh câu, ghép nối mượt mà bằng DSP Cross-Fade.
- 🇻🇳 **Bộ Chuẩn Hóa Tiếng Việt Toàn Diện:** Đọc chuẩn xác số đếm, ngày tháng, biển số xe, số CCCD, tiền tệ VNĐ/USD, ký hiệu toán học và từ mượn ngoại ngữ.

---

## 📂 Cấu Trúc File Mô Hình (Model Architecture & Files)

```
Cong123779/matcha-tts-vietnamese-onnx-int8/
├── int8/                                  # Mô hình INT8 CPU siêu nhẹ (~68 MB)
│   ├── matcha_encoder.onnx
│   ├── matcha_decoder.onnx
│   └── vocos.onnx
├── fp32/                                  # Mô hình FP32 chuẩn Studio (~124 MB)
├── fp16/                                  # Mô hình FP16 GPU Tensor Cores (~63 MB)
├── prompt_encoder.onnx                    # Bộ mã hóa Voice Mel Prompt (152 KB)
├── symbols.json                           # Bảng từ điển ký tự tiếng Việt
└── pytorch_checkpoints/                   # Checkpoint PyTorch gốc phục vụ Finetuning
    ├── matcha_epoch10_clean.pt            # Matcha Generator (~70 MB)
    ├── vocos_epoch10_pure_generator.ckpt  # Vocos Generator (~52 MB)
    ├── matcha_epoch_10_full.ckpt          # Full Checkpoint với Optimizer (~279 MB)
    └── vocos_epoch_10_full.ckpt           # Full Checkpoint với Discriminator (~641 MB)
```

---

## 🚀 Hướng Dẫn Sử Dụng (Quickstart & Usage)

Mã nguồn đầy đủ có sẵn tại GitHub: [https://github.com/congkx123789/match_TTS_VI](https://github.com/congkx123789/match_TTS_VI)

### 1. 🖥️ Chạy Bằng C++ Native Engine (Hiệu Năng Tối Đa)

```bash
# Clone mã nguồn từ GitHub
git clone https://github.com/congkx123789/match_TTS_VI.git
cd match_TTS_VI

# Tải bộ mô hình INT8
mkdir -p models_onnx/int8
wget -O models_onnx/int8/matcha_encoder.onnx https://huggingface.co/Cong123779/matcha-tts-vietnamese-onnx-int8/resolve/main/int8/matcha_encoder.onnx
wget -O models_onnx/int8/matcha_decoder.onnx https://huggingface.co/Cong123779/matcha-tts-vietnamese-onnx-int8/resolve/main/int8/matcha_decoder.onnx
wget -O models_onnx/int8/vocos.onnx https://huggingface.co/Cong123779/matcha-tts-vietnamese-onnx-int8/resolve/main/int8/vocos.onnx
wget -O models_onnx/prompt_encoder.onnx https://huggingface.co/Cong123779/matcha-tts-vietnamese-onnx-int8/resolve/main/prompt_encoder.onnx
wget -O models_onnx/symbols.json https://huggingface.co/Cong123779/matcha-tts-vietnamese-onnx-int8/resolve/main/symbols.json

# Biên dịch nhị phân C++ CLI
cd cpp
make -j$(nproc)
cd ..

# Tổng hợp giọng nói siêu tốc
./bin/matcha_tts_cli --text "Kiếm quang rực rỡ chiếu sáng bầu trời đêm, uy áp vô tận chấn nhiếp muôn loài." --output output.wav --int8 --steps 2
```

### 2. 🐍 Chạy Bằng Python ONNX Runtime

```python
import onnxruntime as ort
import numpy as np

# Khởi tạo ONNX Runtime Session
opts = ort.SessionOptions()
opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
opts.intra_op_num_threads = 2

encoder = ort.InferenceSession("models_onnx/int8/matcha_encoder.onnx", opts)
decoder = ort.InferenceSession("models_onnx/int8/matcha_decoder.onnx", opts)
vocos = ort.InferenceSession("models_onnx/int8/vocos.onnx", opts)

print("✅ Nạp toàn bộ mô hình INT8 thành công!")
```

### 3. 🌐 Gọi Qua Hugging Face Space API từ xa

```python
from gradio_client import Client

client = Client("Cong123779/matcha-tts", headers={"x-api-key": "sk_alida_matcha_vn_2026"})
result = client.predict(
    text="Đơn hàng số 8899 trị giá 2.500.000 đồng đã giao thành công.",
    steps=2,
    speed=1.0,
    pitch=1.0,
    temp=0.9,
    auto_norm=True,
    api_name="/predict"
)
wav_path = result[0]
print(f"🎉 Tệp âm thanh đã tạo: {wav_path}")
```

---

## 📊 Bảng So Sánh Hiệu Năng (Benchmarks)

| Thiết Bị | Kiến Trúc Mô Hình | Dung Lượng | Thời Gian Xử Lý (Câu 10s) | Tốc Độ Thực Tế (Realtime) | RTF |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Hugging Face Space (1 vCPU)** | **INT8 (Pure)** | **~68 MB** | **~0.55s** | **18.2x RT** | **0.055** |
| **Intel Core i7 / AMD Ryzen (4 Cores)** | **INT8 (Pure)** | **~68 MB** | **~0.13s** | **75.0x RT** | **0.013** |
| **NVIDIA GPU (CUDA)** | **FP16** | **~110 MB** | **~0.04s** | **250.0x RT** | **0.004** |

---

## 🔗 Liên Kết Liên Quan (Related Links)

- 🌐 **Trải nghiệm trực tiếp trên Hugging Face Space:** [https://huggingface.co/spaces/Cong123779/matcha-tts](https://huggingface.co/spaces/Cong123779/matcha-tts)
- 🐙 **Kho mã nguồn GitHub:** [https://github.com/congkx123789/match_TTS_VI](https://github.com/congkx123789/match_TTS_VI)

---

## 📜 Giấy Phép (License)
Phát hành theo giấy phép **MIT License**. Bạn có thể tự do sử dụng cho mục đích cá nhân và thương mại.
