#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include "tts_engine.hpp"

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void print_sep(char c = '=', int len = 100) {
    std::cout << std::string(len, c) << std::endl;
}

struct StoryBenchResult {
    std::string config_name;
    int steps;
    int num_sentences;
    double total_audio_sec;
    double total_cpu_sec;
    double avg_rtf;
    double avg_speedup_x;
    std::string out_wav;
};

int main(int argc, char* argv[]) {
    print_sep('=');
    std::cout << "🎙️  ĐO TỐC ĐỘ THỰC TẾ TRÊN ĐOẠN VĂN SIÊU DÀI VỚI ĐÚNG 1 CORE CPU (PINNED THREAD=1)" << std::endl;
    std::cout << "    Văn bản: extra_long_story.txt (14 câu kiếm hiệp dài, Voice Prompt Chaining)" << std::endl;
    print_sep('=');

    std::string text_path = "extra_long_story.txt";
    if (argc > 1) text_path = argv[1];

    std::string full_story = read_file(text_path);
    if (full_story.empty()) {
        std::cerr << "❌ Không thể đọc file: " << text_path << std::endl;
        return 1;
    }

    struct ModelRun {
        std::string name;
        std::string enc;
        std::string dec;
        int steps;
        std::string wav_out;
    };

    std::vector<ModelRun> runs = {
        { "INT8 Đã Tối Ưu (2 steps siêu tốc)",  "models_onnx/matcha_encoder_int8.onnx", "models_onnx/matcha_decoder_int8.onnx", 2,  "outputs/bench_long_1core_int8_2step.wav" },
        { "INT8 Đã Tối Ưu (10 steps chuẩn)",    "models_onnx/matcha_encoder_int8.onnx", "models_onnx/matcha_decoder_int8.onnx", 10, "outputs/bench_long_1core_int8_10step.wav" },
        { "FP32 Chuẩn (10 steps Studio)",       "models_onnx/matcha_encoder.onnx",      "models_onnx/matcha_decoder.onnx",      10, "outputs/bench_long_1core_fp32_10step.wav" }
    };

    std::vector<StoryBenchResult> summary_results;

    for (const auto& r : runs) {
        print_sep('-');
        std::cout << "🚀 Đang đo kiểm: " << r.name << " (Số bước ODE: " << r.steps << ")" << std::endl;
        print_sep('-');

        TTSEngine engine;
        if (!engine.init(r.enc, r.dec, "models_onnx/vocos.onnx", "models_onnx/symbols.json", /*use_gpu=*/false, "models_onnx/prompt_encoder.onnx", /*threads=*/1)) {
            std::cerr << "❌ Lỗi khởi tạo mô hình: " << r.name << std::endl;
            continue;
        }

        TTSConfig cfg;
        cfg.n_timesteps = r.steps;
        cfg.temperature = 0.9f;
        cfg.length_scale = 0.85f;
        cfg.pause_sec = 0.15f;
        cfg.use_gpu = false;
        cfg.use_sway = (r.steps <= 2); // Kích hoạt Sway Sampling nếu 2 steps

        auto sentences = engine.text_processor().split_sentences(full_story);
        std::cout << "  • Tổng số câu: " << sentences.size() << " câu" << std::endl;

        std::vector<float> full_story_audio;
        std::vector<float> prev_norm_mel;
        int64_t prev_mel_len = 0;
        std::string prev_sentence = "";
        const int SAMPLE_RATE = 22050;
        const int N_FEATS = 80;
        std::vector<float> silence_buf(static_cast<size_t>(cfg.pause_sec * SAMPLE_RATE), 0.0f);

        auto t_story_start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < sentences.size(); ++i) {
            const auto& s = sentences[i];

            // Trích xuất Prompt từ câu trước
            std::vector<float> prompt_mel;
            int64_t prompt_frames = 0;
            if (prev_mel_len > 0 && !prev_norm_mel.empty()) {
                int word_count = 1;
                for (char c : prev_sentence) if (c == ' ') word_count++;
                int actual_n = std::min(word_count, std::max(3, cfg.tail_words_prompt));
                float ratio = static_cast<float>(actual_n) / static_cast<float>(word_count);
                prompt_frames = std::max<int64_t>(30, static_cast<int64_t>(static_cast<float>(prev_mel_len) * ratio));
                prompt_frames = std::min<int64_t>(prompt_frames, prev_mel_len);

                prompt_mel.resize(N_FEATS * prompt_frames);
                for (int c = 0; c < N_FEATS; ++c) {
                    for (int64_t f = 0; f < prompt_frames; ++f) {
                        int64_t src = (prev_mel_len - prompt_frames) + f;
                        prompt_mel[c * prompt_frames + f] = prev_norm_mel[c * prev_mel_len + src];
                    }
                }
            }

            std::vector<float> cur_norm_mel;
            int64_t cur_mel_len = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            auto chunk = engine.synthesize_sentence(s, cfg, cur_norm_mel, cur_mel_len, prompt_mel, prompt_frames);
            auto t1 = std::chrono::high_resolution_clock::now();

            float sec_cpu = std::chrono::duration<float>(t1 - t0).count();
            float sec_audio = static_cast<float>(chunk.size()) / static_cast<float>(SAMPLE_RATE);
            float speed_x = (sec_cpu > 0.0f) ? (sec_audio / sec_cpu) : 0.0f;

            full_story_audio.insert(full_story_audio.end(), chunk.begin(), chunk.end());
            if (i + 1 < sentences.size()) {
                full_story_audio.insert(full_story_audio.end(), silence_buf.begin(), silence_buf.end());
            }

            std::cout << "    [Câu " << std::setw(2) << (i + 1) << "/" << sentences.size() << "] "
                      << "Audio: " << std::fixed << std::setprecision(2) << sec_audio << "s | "
                      << "CPU 1-Core: " << std::setprecision(3) << sec_cpu << "s (" << (sec_cpu*1000.0f) << " ms) | "
                      << "Tốc độ: " << std::setprecision(1) << speed_x << "x RT" << std::endl;

            prev_norm_mel = std::move(cur_norm_mel);
            prev_mel_len = cur_mel_len;
            prev_sentence = s;
        }

        auto t_story_end = std::chrono::high_resolution_clock::now();
        double total_cpu = std::chrono::duration<double>(t_story_end - t_story_start).count();
        double total_audio = static_cast<double>(full_story_audio.size()) / static_cast<double>(SAMPLE_RATE);
        double avg_rtf = total_cpu / total_audio;
        double avg_speedup = total_audio / total_cpu;

        TTSEngine::normalize_audio(full_story_audio, 0.96f);
        TTSEngine::save_wav(r.wav_out, full_story_audio, SAMPLE_RATE);

        StoryBenchResult res;
        res.config_name = r.name;
        res.steps = r.steps;
        res.num_sentences = static_cast<int>(sentences.size());
        res.total_audio_sec = total_audio;
        res.total_cpu_sec = total_cpu;
        res.avg_rtf = avg_rtf;
        res.avg_speedup_x = avg_speedup;
        res.out_wav = r.wav_out;
        summary_results.push_back(res);

        std::cout << "  💾 Đã lưu file: " << r.wav_out << std::endl;
        std::cout << "  ⏱️  Tổng Audio: " << std::fixed << std::setprecision(2) << total_audio << " giây | "
                  << "Tổng thời gian CPU: " << total_cpu << " giây | "
                  << "TỐC ĐỘ TRUNG BÌNH: " << std::setprecision(1) << avg_speedup << "x Realtime\n" << std::endl;
    }

    print_sep('=');
    std::cout << "📊 BẢNG KẾT QUẢ ĐO LƯỜNG TỔNG THỂ TRÊN ĐOẠN VĂN SIÊU DÀI (1 CORE CPU)" << std::endl;
    print_sep('=');
    std::cout << std::left
              << std::setw(34) << "Cấu hình mô hình"
              << std::setw(10) << "Số Steps"
              << std::setw(18) << "Tổng Audio"
              << std::setw(20) << "Thời gian 1 Core CPU"
              << std::setw(14) << "RTF bình quân"
              << "Tốc độ trung bình thực tế" << std::endl;
    print_sep('-', 120);

    for (const auto& res : summary_results) {
        std::cout << std::left
                  << std::setw(34) << res.config_name
                  << std::setw(10) << (std::to_string(res.steps) + " steps")
                  << std::fixed << std::setprecision(2)
                  << std::setw(18) << (std::to_string(res.total_audio_sec).substr(0, 5) + " giây")
                  << std::setw(20) << (std::to_string(res.total_cpu_sec).substr(0, 5) + " giây")
                  << std::setprecision(4)
                  << std::setw(14) << res.avg_rtf
                  << std::setprecision(1)
                  << ("🚀 " + std::to_string(res.avg_speedup_x).substr(0, 4) + "x Realtime") << std::endl;
    }
    print_sep('=', 120);

    return 0;
}
