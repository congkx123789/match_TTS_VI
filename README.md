# 🎙️ Matcha-TTS CP10 + Vocos CP10 (Standalone GPU Inference)

Thư mục độc lập chuẩn hóa cao nhất để chạy suy luận (Inference) mô hình **Matcha-TTS Checkpoint 10** kết hợp **Vocos Vocoder Checkpoint 10** trên GPU (NVIDIA CUDA).

---

## 📁 Cấu Trúc Thư Mục

```text
matcha10_vocos10_standalone/
├── models/
│   ├── matcha_epoch10_clean.pt           # Checkpoint Matcha-TTS (73.15 MB, đã lọc sạch rác)
│   └── vocos_epoch10_pure_generator.ckpt # Checkpoint Vocos (54.25 MB, đã lọc sạch rác)
├── matcha/                               # Core module kiến trúc Matcha-TTS
├── vocos/                                # Core module kiến trúc Vocos Vocoder
├── model_builder.py                      # Bộ khởi tạo kiến trúc Matcha 22050Hz độc lập
├── load_model.py                         # Module nạp model & xử lý âm thanh chuẩn Studio
├── vietnamese_text_normalizer.py         # Bộ chuẩn hóa số & câu tiếng Việt chuẩn đĩa hát
├── foreign_symbols_translator.py         # Bộ dịch ký hiệu / từ mượn
├── infer_gpu.py                          # Script chạy tổng hợp tiếng nói 1 lần trên GPU
├── batch_infer_gpu.py                    # Script chạy hàng loạt (Batch Processing)
└── README.md                             # Hướng dẫn sử dụng
```

---

## ⚡ Hướng Dẫn Chạy Bằng GPU

### 1. Chạy với câu văn bản mặc định (~31.6s trong 1 lần input duy nhất):
```bash
python3 infer_gpu.py
```

### 2. Chạy với một đoạn văn bản tùy ý:
```bash
python3 infer_gpu.py --text "Hôm nay trời rất đẹp, Thạch Hạo bắt đầu bước vào con đường tu luyện đỉnh cao." --out "output.wav"
```

### 3. Đọc từ một file văn bản `.txt`:
```bash
python3 infer_gpu.py --file "doan_van.txt" --out "audio_doan_van.wav"
```

### 4. Tùy chỉnh các chế độ tốc độ & chất lượng:
* **Chuẩn Studio Audiobook (Khuyên dùng):** `--steps 10` *(Tốc độ ~390x)*
  ```bash
  python3 infer_gpu.py --steps 10 --out "audio_studio.wav"
  ```
* **Siêu tốc Cực nét:** `--steps 4` *(Tốc độ ~790x)*
  ```bash
  python3 infer_gpu.py --steps 4 --out "audio_fast.wav"
  ```
* **Tốc độ Giới hạn Cực đại:** `--steps 2` *(Tốc độ ~1,220x)*
  ```bash
  python3 infer_gpu.py --steps 2 --out "audio_max_speed.wav"
  ```

---

## 🚀 Chạy Hàng Loạt (Batch Processing)
Để sản xuất hàng chục phút audio trong vài giây:
```bash
python3 batch_infer_gpu.py --batch_size 32 --steps 10
```

---

## 📊 Thông Số & Hiệu Năng

* **Tổng số tham số toàn bộ hệ thống:** ~31.72 Triệu tham số (31.72M)
  * Matcha-TTS: 18.26M
  * Vocos Vocoder: 13.46M
* **Tổng dung lượng Checkpoints:** ~127 MB (Nhẹ hơn 86% so với bản gốc 935 MB)
* **Thời gian sinh đoạn audio 31.6s:** **81 ms** trên RTX 5060 Ti (~390x real-time).
