#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <filesystem>
#include "tts_engine.hpp"

void print_banner() {
    std::cout << "===========================================================================" << std::endl;
    std::cout << "🎙️  HỆ THỐNG ĐỌC TIẾNG NÓI C++ NATIVE (MATCHA-TTS + VOCOS ONNX RUNTIME)" << std::endl;
    std::cout << "===========================================================================" << std::endl;
}

void print_help(const char* prog_name) {
    std::cout << "Cách sử dụng:\n"
              << "  " << prog_name << " [Tùy chọn]\n\n"
              << "Các tùy chọn hỗ trợ:\n"
              << "  -t, --text <văn bản>    : Câu hoặc đoạn văn cần đọc\n"
              << "  -f, --file <đường dẫn>  : Đọc nội dung từ tệp văn bản (.txt)\n"
              << "  -o, --output <đường dẫn>: Đường dẫn tệp wav đầu ra (mặc định: outputs/output.wav)\n"
              << "  -s, --steps <số bước>   : Số bước giải vi phân ODE (1-10, mặc định: 2 cho Sway siêu tốc)\n"
              << "      --speed, -r <tỷ lệ> : Tốc độ đọc Google Sonic WSOLA (1.0 = chuẩn, 1.5, 2.0, 3.0, 4.0; mặc định: 1.0)\n"
              << "      --pitch <cao độ>    : Điều chỉnh cao độ bằng Google Sonic (0.5 - 2.0, mặc định: 1.0)\n"
              << "      --pause-config <tệp>: Cấu hình ngắt nghỉ thông minh theo dấu câu (.json, mặc định: pause_config.json)\n"
              << "      --pause <giây>      : Khoảng nghỉ cố định giữa các câu (ghi đè cấu hình dấu câu)\n"
              << "      --length-scale <tỷ lệ>: Co giãn âm vị AI thô (mặc định chuẩn: 1.0)\n"
              << "      --temp <nhiệt độ>   : Độ cảm xúc / pitch variation (mặc định: 0.9)\n"
              << "      --threads, -j <N>   : Số luồng CPU thực thi (1-N, mặc định: 4)\n"
              << "      --cpu / --gpu       : Chọn thiết bị tính toán (CPU hoặc GPU CUDA)\n"
              << "      --model-type <type> : Loại mô hình (int8, fp32, fp16; mặc định: int8)\n"
              << "      --no-norm           : Tắt bộ chuẩn hóa số & ngoại ngữ tự động\n"
              << "      --clean-only        : Chỉ in ra văn bản sau khi làm sạch (Clean Input) rồi thoát\n"
              << "  -h, --help              : Hiển thị hướng dẫn này\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::string text = "Chào mừng bạn đến với hệ thống đọc tiếng nói C++ Native siêu tốc!";
    std::string file_path = "";
    std::string output_path = "outputs/output.wav";
    std::string model_type = "int8"; // Cố định mặc định INT8 tối ưu AVX2/VNNI siêu tốc và ổn định
    std::string pause_config_file = "pause_config.json";
    int num_threads = 1; // Mặc định 1 core độc lập / request
    bool clean_only = false;
    
    TTSConfig config;
    config.n_timesteps = 2; // Chuẩn 2-step Sway Sampling INT8 tối ưu
    config.temperature = 0.9f;
    config.length_scale = 1.0f; // Cố định chuẩn phát âm tự nhiên
    config.pause_sec = -1.0f;   // Dưới 0 nghĩa là dùng cấu hình động theo dấu câu
    config.use_gpu = false;
    config.sonic_speed = 1.0f;  // Chuẩn tốc độ phát lại bằng Google Sonic WSOLA
    config.sonic_pitch = 1.0f;
    config.enable_normalization = true;

    // Phân tích đối số dòng lệnh
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-t" || arg == "--text") && i + 1 < argc) {
            text = argv[++i];
        } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            file_path = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_path = argv[++i];
        } else if ((arg == "-s" || arg == "--steps") && i + 1 < argc) {
            config.n_timesteps = std::max(1, std::min(std::stoi(argv[++i]), 6));
        } else if ((arg == "--speed" || arg == "--tempo" || arg == "--sonic-speed" || arg == "-r") && i + 1 < argc) {
            config.sonic_speed = std::max(0.5f, std::min(std::stof(argv[++i]), 2.5f));
        } else if (arg == "--length-scale" && i + 1 < argc) {
            config.length_scale = std::max(0.5f, std::min(std::stof(argv[++i]), 2.0f));
        } else if ((arg == "--pitch" || arg == "--sonic-pitch") && i + 1 < argc) {
            config.sonic_pitch = std::max(0.7f, std::min(std::stof(argv[++i]), 1.5f));
        } else if (arg == "--temp" && i + 1 < argc) {
            config.temperature = std::max(0.6f, std::min(std::stof(argv[++i]), 1.2f));
        } else if (arg == "--pause" && i + 1 < argc) {
            config.pause_sec = std::stof(argv[++i]);
        } else if (arg == "--pause-config" && i + 1 < argc) {
            pause_config_file = argv[++i];
        } else if (arg == "--tail-words" && i + 1 < argc) {
            config.tail_words_prompt = std::stoi(argv[++i]);
        } else if (arg == "--model-type" && i + 1 < argc) {
            model_type = "int8"; // Cố định INT8
            i++;
        } else if ((arg == "--threads" || arg == "-j") && i + 1 < argc) {
            num_threads = std::max(1, std::min(std::stoi(argv[++i]), 16));
        } else if (arg == "--no-norm") {
            config.enable_normalization = false;
        } else if (arg == "--clean-only") {
            clean_only = true;
        } else if (arg == "--gpu") {
            config.use_gpu = true;
        } else if (arg == "--cpu") {
            config.use_gpu = false;
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
    }

    print_banner();

    // Nếu chỉ định file, đọc nội dung file
    if (!file_path.empty()) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "❌ Không thể mở tệp văn bản: " << file_path << std::endl;
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        text = buffer.str();
        std::cout << "📖 Đọc nội dung từ tệp: " << file_path << std::endl;
    }

    if (clean_only) {
        std::cout << "\n🧹 [CLEAN INPUT] KẾT QUẢ LÀM SẠCH VĂN BẢN ĐẦU VÀO:" << std::endl;
        std::cout << "---------------------------------------------------------------------------" << std::endl;
        std::string cleaned = matcha::TextNormalizer::clean_and_normalize(text);
        std::cout << cleaned << std::endl;
        std::cout << "---------------------------------------------------------------------------" << std::endl;
        return 0;
    }

    config.pause_config = matcha::TextNormalizer::load_pause_config(pause_config_file);

    std::cout << "📝 Nội dung văn bản  : \"" << (text.size() > 80 ? text.substr(0, 77) + "..." : text) << "\"" << std::endl;
    std::cout << "⚡ Số bước ODE       : " << config.n_timesteps << std::endl;
    std::cout << "⏩ Tốc độ đọc (speed): " << config.sonic_speed << "x (Google Sonic WSOLA)" << std::endl;
    if (config.sonic_pitch != 1.0f) {
        std::cout << "🎼 Cao độ (pitch)    : " << config.sonic_pitch << "x" << std::endl;
    }
    if (config.length_scale != 1.0f) {
        std::cout << "📏 AI length_scale   : " << config.length_scale << std::endl;
    }
    std::cout << "🎭 Temperature       : " << config.temperature << std::endl;
    std::cout << "🖥️  Thiết bị          : " << (config.use_gpu ? "GPU (CUDA)" : "CPU (" + std::to_string(num_threads) + " threads)") << std::endl;
    std::cout << "💾 File xuất         : " << output_path << std::endl;
    std::cout << "📦 Kiểu mô hình      : " << model_type << std::endl;
    std::cout << "🧹 Chuẩn hóa text    : " << (config.enable_normalization ? "Bật (Tự động chuyển số & ngoại ngữ)" : "Tắt") << std::endl;
    std::cout << "---------------------------------------------------------------------------" << std::endl;

    // Chọn tệp mô hình theo model_type (int8, fp32, fp16)
    std::string base_dir = "models_onnx/" + model_type;
    if (!std::filesystem::exists(base_dir)) {
        base_dir = "models_onnx/int8";
    }
    std::string enc_file = base_dir + "/matcha_encoder.onnx";
    std::string dec_file = base_dir + "/matcha_decoder.onnx";
    std::string vocos_file = base_dir + "/vocos.onnx";
    std::string symbols_file = "models_onnx/symbols.json";
    std::string prompt_enc_file = "models_onnx/prompt_encoder.onnx";

    if (config.n_timesteps <= 2 && !config.use_gpu) {
        config.use_sway = true;
        config.sway_coef = -1.0f;
    }

    TTSEngine engine;
    if (!engine.init(enc_file, dec_file, vocos_file, symbols_file, config.use_gpu, "models_onnx/prompt_encoder.onnx", num_threads)) {
        std::cerr << "❌ Khởi tạo TTSEngine thất bại!" << std::endl;
        return 1;
    }

    if (daemon_mode) {
        std::cout << "READY_DAEMON" << std::endl;
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "EXIT" || line == "QUIT") break;
            if (line.empty()) continue;
            
            std::stringstream ss(line);
            std::string req_out, req_steps, req_speed, req_pitch, req_temp, req_norm, req_text_escaped;
            if (!std::getline(ss, req_out, '\t')) continue;
            if (!std::getline(ss, req_steps, '\t')) continue;
            if (!std::getline(ss, req_speed, '\t')) continue;
            if (!std::getline(ss, req_pitch, '\t')) continue;
            if (!std::getline(ss, req_temp, '\t')) continue;
            if (!std::getline(ss, req_norm, '\t')) continue;
            if (!std::getline(ss, req_text_escaped)) continue;

            // Unescape text
            std::string req_text;
            for (size_t i = 0; i < req_text_escaped.size(); ++i) {
                if (req_text_escaped[i] == '\\' && i + 1 < req_text_escaped.size()) {
                    if (req_text_escaped[i+1] == 'n') { req_text += '\n'; i++; }
                    else if (req_text_escaped[i+1] == '\\') { req_text += '\\'; i++; }
                    else req_text += req_text_escaped[i];
                } else {
                    req_text += req_text_escaped[i];
                }
            }

            TTSConfig req_cfg = config;
            try {
                req_cfg.n_timesteps = std::max(1, std::min(std::stoi(req_steps), 6));
                req_cfg.sonic_speed = std::max(0.5f, std::min(std::stof(req_speed), 2.5f));
                req_cfg.sonic_pitch = std::max(0.7f, std::min(std::stof(req_pitch), 1.5f));
                req_cfg.temperature = std::max(0.1f, std::min(std::stof(req_temp), 1.2f));
                req_cfg.enable_normalization = (req_norm == "1" || req_norm == "true");
            } catch (...) {}

            double dur_sec = 0.0;
            double proc_sec = 0.0;
            auto wav_data = engine.synthesize_paragraph(req_text, req_cfg, dur_sec, proc_sec);

            if (wav_data.empty()) {
                std::cout << "ERROR\tFailed to synthesize audio" << std::endl;
            } else {
                std::filesystem::path op(req_out);
                if (op.has_parent_path()) {
                    std::filesystem::create_directories(op.parent_path());
                }
                if (TTSEngine::save_wav(req_out, wav_data, 22050)) {
                    std::cout << "DONE\t" << std::fixed << std::setprecision(3) << dur_sec << "\t" << proc_sec << "\t" << req_out << std::endl;
                } else {
                    std::cout << "ERROR\tFailed to save wav file: " << req_out << std::endl;
                }
            }
        }
        return 0;
    }

    // Warmup tự động cho các tệp văn bản dài / chapter để kích hoạt bộ nhớ đệm CPU L3
    if (!file_path.empty()) {
        TTSConfig wup_cfg = config;
        wup_cfg.n_timesteps = 1;
        std::vector<float> wup_mel;
        int64_t wup_len = 0;
        engine.synthesize_sentence("Khởi động hệ thống.", wup_cfg, wup_mel, wup_len);
    }

    std::cout << "\n⏳ Đang tiến hành tổng hợp tiếng nói C++..." << std::endl;
    double duration_sec = 0.0;
    double process_time_sec = 0.0;

    auto wav = engine.synthesize_paragraph(text, config, duration_sec, process_time_sec);
    if (wav.empty()) {
        std::cerr << "❌ Không tạo được âm thanh!" << std::endl;
        return 1;
    }

    std::filesystem::path out_p(output_path);
    if (out_p.has_parent_path()) {
        std::filesystem::create_directories(out_p.parent_path());
    }

    if (!TTSEngine::save_wav(output_path, wav, 22050)) {
        std::cerr << "❌ Thất bại khi lưu file wav: " << output_path << std::endl;
        return 1;
    }

    std::cout << "---------------------------------------------------------------------------" << std::endl;
    std::cout << "✅ Hoàn tất tổng hợp âm thanh thành công!" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "⏱️  Thời lượng âm thanh : " << duration_sec << " giây" << std::endl;
    std::cout << std::setprecision(3);
    std::cout << "⚡ Thời gian suy luận  : " << process_time_sec << " giây" << std::endl;
    std::cout << std::setprecision(4);
    std::cout << "📊 Tỷ lệ RTF           : " << (process_time_sec / duration_sec) << std::endl;
    std::cout << std::setprecision(1);
    std::cout << "🚀 Tốc độ thực tế      : " << (duration_sec / process_time_sec) << "x Realtime" << std::endl;
    std::cout << "📂 File âm thanh đã lưu: " << output_path << std::endl;
    std::cout << "===========================================================================" << std::endl;

    return 0;
}
