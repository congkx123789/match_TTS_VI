#pragma once

#include <string>
#include <vector>
#include <memory>
#include "onnxruntime_cxx_api.h"
#include "text_processor.hpp"
#include "text_normalizer.hpp"

struct TTSConfig {
    int n_timesteps = 10;
    float temperature = 0.9f;
    float length_scale = 1.0f;
    float pause_sec = 0.15f;
    bool use_gpu = false;
    int tail_words_prompt = 4;
    bool use_sway = false;
    float sway_coef = -1.0f;
    float sonic_speed = 1.0f; // Tốc độ tua nhanh bằng Google Sonic WSOLA (1.0 = giữ nguyên, 2.0 = x2, 3.0 = x3, 4.0 = x4)
    float sonic_pitch = 1.0f; // Điều chỉnh cao độ bằng Google Sonic (1.0 = giữ nguyên)
    bool enable_normalization = true; // Tự động làm sạch & chuẩn hóa số/từ ngoại ngữ
    matcha::PauseConfig pause_config;  // Cấu hình ngắt nghỉ thông minh theo dấu câu
};

class TTSEngine {
public:
    TTSEngine();
    ~TTSEngine() = default;

    // Khởi tạo và nạp các mô hình ONNX
    bool init(
        const std::string& encoder_path,
        const std::string& decoder_path,
        const std::string& vocos_path,
        const std::string& symbols_path,
        bool use_gpu = false,
        const std::string& prompt_enc_path = "models_onnx/prompt_encoder.onnx",
        int num_threads = 0
    );

    // Tổng hợp âm thanh cho 1 câu đơn lẻ
    std::vector<float> synthesize_sentence(
        const std::string& sentence,
        const TTSConfig& config,
        std::vector<float>& out_norm_mel,
        int64_t& out_mel_len,
        const std::vector<float>& prompt_norm_mel = {},
        int64_t prompt_mel_frames = 0,
        bool apply_post_dsp = true
    );

    // Tổng hợp âm thanh cho toàn bộ đoạn văn dài (hỗ trợ Voice Prompt Chaining)
    std::vector<float> synthesize_paragraph(
        const std::string& text,
        const TTSConfig& config,
        double& out_audio_duration_sec,
        double& out_process_time_sec
    );

    // Áp dụng thuật toán Google Sonic WSOLA để tua nhanh x2, x3, x4 mà giữ nguyên cao độ
    static std::vector<float> apply_sonic(
        const std::vector<float>& audio_samples,
        float speed,
        float pitch = 1.0f,
        int sample_rate = 22050
    );

    // Đọc file .wav bất kỳ thành mảng float32
    static bool load_wav(
        const std::string& filepath,
        std::vector<float>& out_samples,
        int& out_sample_rate
    );

    // Ghi mảng audio float32 ra file .wav
    static bool save_wav(
        const std::string& filepath,
        const std::vector<float>& audio_samples,
        int sample_rate = 22050
    );

    // Chuẩn hóa biên độ âm lượng (Audiobook Peak Normalization)
    static void normalize_audio(std::vector<float>& audio, float target_peak = 0.96f);

    // Khử tiếng pop/click ở 2 đầu câu (Micro Fade-in / Fade-out)
    static void apply_micro_fade(std::vector<float>& audio, int sample_rate = 22050, int fade_ms = 6);

    const TextProcessor& text_processor() const { return text_processor_; }

private:
    Ort::Env env_;
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> enc_session_;
    std::unique_ptr<Ort::Session> dec_session_;
    std::unique_ptr<Ort::Session> vocos_session_;
    std::unique_ptr<Ort::Session> prompt_enc_session_;
    Ort::MemoryInfo memory_info_;

    TextProcessor text_processor_;
    bool initialized_ = false;
    bool is_gpu_active_ = false;

    // Các hằng số âm học chuẩn Matcha-TTS 22050Hz
    static constexpr int SAMPLE_RATE = 22050;
    static constexpr int N_FEATS = 80;
    static constexpr float MEL_MEAN = -5.205414772033691f;
    static constexpr float MEL_STD = 2.5967071056365967f;
};
