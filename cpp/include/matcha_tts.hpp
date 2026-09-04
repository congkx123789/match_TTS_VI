#pragma once

/**
 * ============================================================================
 * MatchaTTS C++ High-Level Clean API (Header-Only Wrapper)
 * ============================================================================
 * Thư viện giao tiếp C++ cấp cao, chuẩn hóa, độc lập và dễ nhúng vào bất kỳ
 * dự án C++ nào (Game Engine, Audio Application, Streaming Server, Robot, v.v.)
 *
 * Cách sử dụng cơ bản (3 dòng code):
 * ----------------------------------
 *   MatchaTTS tts;
 *   tts.init("models_onnx", false); // false: dùng CPU (mặc định), true: dùng GPU
 *   tts.speak_to_file("Xin chào, đây là giọng đọc C++!", "chao.wav");
 * ============================================================================
 */

#include "tts_engine.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>

class MatchaTTS {
public:
    MatchaTTS() : engine_(std::make_unique<TTSEngine>()) {}
    ~MatchaTTS() = default;

    /**
     * Khởi tạo hệ thống TTS.
     * @param models_dir Đường dẫn thư mục chứa các file ONNX (mặc định: "models_onnx")
     * @param use_gpu Kích hoạt NVIDIA CUDA (true) hoặc CPU đa luồng (false)
     * @param model_type "fp32" (chuẩn nhất), "fp16" (GPU Tensor Cores), "int8" (CPU siêu nhẹ)
     * @return true nếu nạp thành công toàn bộ mô hình
     */
    bool init(
        const std::string& models_dir = "models_onnx",
        bool use_gpu = false,
        const std::string& model_type = "fp32"
    ) {
        std::string enc = models_dir + "/matcha_encoder.onnx";
        std::string dec = models_dir + "/matcha_decoder.onnx";
        std::string voc = models_dir + "/vocos.onnx";
        std::string sym = models_dir + "/symbols.json";

        if (model_type == "int8") {
            enc = models_dir + "/matcha_encoder_int8.onnx";
            dec = models_dir + "/matcha_decoder_int8.onnx";
        } else if (model_type == "fp16") {
            enc = models_dir + "/matcha_encoder_fp16.onnx";
            dec = models_dir + "/matcha_decoder_fp16.onnx";
        }

        model_type_ = model_type;
        use_gpu_ = use_gpu;
        return engine_->init(enc, dec, voc, sym, use_gpu);
    }

    /**
     * Tổng hợp văn bản thành mảng âm thanh float32 (22,050 Hz).
     * Tự động phân đoạn câu, chạy chuỗi liên tục Voice Prompt Chaining và chuẩn hóa âm lượng.
     */
    std::vector<float> speak(
        const std::string& text,
        int steps = 10,
        float speed = 1.0f,
        float temp = 0.9f,
        float pause_sec = 0.15f
    ) {
        TTSConfig cfg;
        cfg.n_timesteps = steps;
        cfg.length_scale = speed;
        cfg.temperature = temp;
        cfg.pause_sec = pause_sec;
        cfg.use_gpu = use_gpu_;

        double audio_dur = 0.0;
        double proc_time = 0.0;
        return engine_->synthesize_paragraph(text, cfg, audio_dur, proc_time);
    }

    /**
     * Tổng hợp văn bản và lưu trực tiếp thành tệp WAV 16-bit PCM.
     */
    bool speak_to_file(
        const std::string& text,
        const std::string& output_wav,
        int steps = 10,
        float speed = 1.0f,
        float temp = 0.9f,
        float pause_sec = 0.15f,
        double* out_duration = nullptr,
        double* out_time = nullptr
    ) {
        TTSConfig cfg;
        cfg.n_timesteps = steps;
        cfg.length_scale = speed;
        cfg.temperature = temp;
        cfg.pause_sec = pause_sec;
        cfg.use_gpu = use_gpu_;

        double audio_dur = 0.0;
        double proc_time = 0.0;
        auto wav = engine_->synthesize_paragraph(text, cfg, audio_dur, proc_time);
        if (wav.empty()) return false;

        if (out_duration) *out_duration = audio_dur;
        if (out_time) *out_time = proc_time;

        return TTSEngine::save_wav(output_wav, wav, 22050);
    }

    /**
     * Đọc toàn bộ file văn bản dài (.txt) và xuất ra file WAV.
     */
    bool read_file_to_audio(
        const std::string& input_txt_path,
        const std::string& output_wav_path,
        int steps = 10,
        float speed = 1.0f,
        float pause_sec = 0.18f
    ) {
        std::ifstream file(input_txt_path);
        if (!file.is_open()) {
            std::cerr << "❌ Không thể đọc file: " << input_txt_path << std::endl;
            return false;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return speak_to_file(ss.str(), output_wav_path, steps, speed, 0.9f, pause_sec);
    }

private:
    std::unique_ptr<TTSEngine> engine_;
    std::string model_type_ = "fp32";
    bool use_gpu_ = false;
};
