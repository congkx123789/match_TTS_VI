#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include "tts_engine.hpp"

void print_help(const char* prog) {
    std::cout << "===========================================================================\n"
              << "🔊 GOOGLE SONIC TIME-SCALE MODIFICATION (WSOLA) TOOL - C++ NATIVE\n"
              << "===========================================================================\n"
              << "Cách sử dụng:\n"
              << "  " << prog << " <input.wav> <output.wav> [Tùy chọn]\n\n"
              << "Các tùy chọn:\n"
              << "  -s, --speed, --tempo <tỷ lệ> : Tốc độ phát lại (0.5x đến 6.0x, ví dụ: 2.0, mặc định: 2.0)\n"
              << "  -p, --pitch <cao độ>         : Tỷ lệ cao độ giọng nói (0.5 đến 2.0, mặc định: 1.0)\n"
              << "  -h, --help                   : Xem hướng dẫn này\n"
              << "===========================================================================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_help(argv[0]);
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = argv[2];
    float speed = 2.0f;
    float pitch = 1.0f;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-s" || arg == "--speed" || arg == "--tempo") && i + 1 < argc) {
            speed = std::stof(argv[++i]);
        } else if ((arg == "-p" || arg == "--pitch") && i + 1 < argc) {
            pitch = std::stof(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
    }

    std::cout << "===========================================================================\n"
              << "🔊 [Google Sonic] Đang xử lý biến đổi thời lượng âm thanh...\n"
              << "  • File gốc      : " << input_path << "\n"
              << "  • File xuất     : " << output_path << "\n"
              << "  • Tốc độ tua    : " << speed << "x\n"
              << "  • Cao độ (pitch): " << pitch << "x\n"
              << "---------------------------------------------------------------------------\n";

    std::vector<float> audio_in;
    int sample_rate = 22050;
    if (!TTSEngine::load_wav(input_path, audio_in, sample_rate)) {
        std::cerr << "❌ Không thể nạp file âm thanh: " << input_path << std::endl;
        return 1;
    }

    double dur_in = static_cast<double>(audio_in.size()) / static_cast<double>(sample_rate);
    std::cout << "  • Thời lượng gốc: " << dur_in << " giây (" << audio_in.size() << " mẫu, " << sample_rate << " Hz)\n";

    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<float> audio_out = TTSEngine::apply_sonic(audio_in, speed, pitch, sample_rate);
    auto t1 = std::chrono::high_resolution_clock::now();

    double proc_time = std::chrono::duration<double>(t1 - t0).count();
    double dur_out = static_cast<double>(audio_out.size()) / static_cast<double>(sample_rate);

    if (!TTSEngine::save_wav(output_path, audio_out, sample_rate)) {
        std::cerr << "❌ Lưu file thất bại: " << output_path << std::endl;
        return 1;
    }

    std::cout << "✅ [Google Sonic] Xử lý hoàn tất thành công!\n"
              << "  • Thời lượng sau tua : " << dur_out << " giây (giảm " << (dur_in / dur_out) << " lần)\n"
              << "  • Thời gian xử lý DSP: " << (proc_time * 1000.0) << " ms\n"
              << "  • Tốc độ tính toán   : " << (dur_in / proc_time) << "x Realtime\n"
              << "  • File kết quả       : " << output_path << "\n"
              << "===========================================================================\n";

    return 0;
}
