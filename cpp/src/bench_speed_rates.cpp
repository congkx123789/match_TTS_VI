#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include "tts_engine.hpp"

struct RateConfig {
    std::string method_name;
    float length_scale;
    float sonic_speed;
    std::string out_wav;
};

struct BenchResult {
    std::string method;
    float rate_x;
    double audio_sec;
    double compute_sec;
    double rtf;
    double speedup_x;
    std::string wav_path;
};

void print_sep(char c, int len = 115) {
    std::cout << std::string(len, c) << std::endl;
}

int main(int argc, char* argv[]) {
    int threads = 1;
    if (argc > 1) {
        threads = std::stoi(argv[1]);
    }

    std::string story_file = "extra_long_story.txt";
    std::ifstream f(story_file);
    if (!f.is_open()) {
        std::cerr << "❌ Không thể mở file: " << story_file << std::endl;
        return 1;
    }
    std::stringstream buf;
    buf << f.rdbuf();
    std::string story_text = buf.str();

    print_sep('=');
    std::cout << "🎙️  BENCHMARK ĐO LƯỜNG TỐC ĐỘ SINH ÂM THANH ĐOẠN DÀI (1.0x, x2, x3, x4)\n";
    std::cout << "🖥️  Cấu hình kiểm thử: 1 Core CPU duy nhất (1 Thread), INT8 2-Step Sway Sampling\n";
    print_sep('=');

    TTSEngine engine;
    if (!engine.init(
        "models_onnx/matcha_encoder_int8.onnx",
        "models_onnx/matcha_decoder_int8.onnx",
        "models_onnx/vocos.onnx",
        "models_onnx/symbols.json",
        /*use_gpu=*/false,
        "models_onnx/prompt_encoder.onnx",
        /*threads=*/threads
    )) {
        std::cerr << "❌ Lỗi khởi tạo mô hình ONNX!" << std::endl;
        return 1;
    }

    std::vector<RateConfig> test_cases = {
        { "Chuẩn (Normal 1.0x)",          1.00f, 1.0f, "outputs/story_1x_normal.wav" },
        { "Google Sonic WSOLA 1.5x",       1.00f, 1.5f, "outputs/story_sonic_1.5x.wav" },
        { "Google Sonic WSOLA 2.0x (x2)",  1.00f, 2.0f, "outputs/story_sonic_2x.wav" },
        { "Google Sonic WSOLA 2.5x",       1.00f, 2.5f, "outputs/story_sonic_2.5x.wav" },
        { "Google Sonic WSOLA 3.0x (x3)",  1.00f, 3.0f, "outputs/story_sonic_3x.wav" },
        { "Google Sonic WSOLA 4.0x (x4)",  1.00f, 4.0f, "outputs/story_sonic_4x.wav" },
    };

    std::vector<BenchResult> results;

    for (const auto& tc : test_cases) {
        print_sep('-');
        std::cout << "⏳ Đang chạy: " << tc.method_name << " ..." << std::endl;

        TTSConfig cfg;
        cfg.n_timesteps = 2;
        cfg.temperature = 0.9f;
        cfg.length_scale = tc.length_scale;
        cfg.sonic_speed = tc.sonic_speed;
        cfg.pause_sec = 0.15f;
        cfg.use_gpu = false;
        cfg.use_sway = true;
        cfg.sway_coef = -1.0f;

        double audio_dur = 0.0;
        double compute_time = 0.0;

        auto audio = engine.synthesize_paragraph(story_text, cfg, audio_dur, compute_time);

        TTSEngine::save_wav(tc.out_wav, audio, 22050);

        BenchResult br;
        br.method = tc.method_name;
        br.rate_x = (tc.sonic_speed > 1.0f) ? tc.sonic_speed : (1.0f / tc.length_scale);
        br.audio_sec = audio_dur;
        br.compute_sec = compute_time;
        br.rtf = compute_time / audio_dur;
        br.speedup_x = audio_dur / compute_time;
        br.wav_path = tc.out_wav;
        results.push_back(br);

        std::cout << "  • Thời lượng Audio : " << std::fixed << std::setprecision(2) << audio_dur << " giây\n"
                  << "  • Thời gian xử lý   : " << std::setprecision(3) << compute_time << " giây (" << (compute_time * 1000.0) << " ms)\n"
                  << "  • Tốc độ sinh (RT)  : " << std::setprecision(1) << br.speedup_x << "x Realtime (RTF: " << std::setprecision(4) << br.rtf << ")\n"
                  << "  • File xuất         : " << tc.out_wav << std::endl;
    }

    print_sep('=');
    std::cout << "📊 BẢNG TỔNG KẾT SO SÁNH TỐC ĐỘ SINH TOÀN BỘ ĐOẠN VĂN DÀI (1 CORE CPU)\n";
    print_sep('=');
    std::cout << std::left
              << std::setw(35) << "Thuật toán & Mức tốc độ"
              << std::setw(16) << "Tổng Audio"
              << std::setw(18) << "Thời gian chạy"
              << std::setw(14) << "RTF"
              << "Tốc độ sinh (xRT)" << std::endl;
    print_sep('-', 115);

    for (const auto& r : results) {
        std::cout << std::left
                  << std::setw(35) << r.method
                  << std::fixed << std::setprecision(2)
                  << std::setw(16) << (std::to_string(r.audio_sec).substr(0, 5) + " giây")
                  << std::setprecision(3)
                  << std::setw(18) << (std::to_string(r.compute_sec).substr(0, 5) + " giây")
                  << std::setprecision(4)
                  << std::setw(14) << r.rtf
                  << std::setprecision(1)
                  << ("🚀 " + std::to_string(r.speedup_x).substr(0, 4) + "x Realtime") << std::endl;
    }
    print_sep('=', 115);

    return 0;
}
