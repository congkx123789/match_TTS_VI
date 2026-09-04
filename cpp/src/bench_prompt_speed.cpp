#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include "tts_engine.hpp"

void print_sep(char c, int len = 110) {
    std::cout << std::string(len, c) << std::endl;
}

struct SentenceStat {
    int id;
    int prompt_frames;
    int mel_frames;
    float audio_sec;
    float cpu_sec;
    float speed_x;
    std::string preview;
};

struct ModeResult {
    std::string mode_name;
    std::string wav_path;
    std::vector<SentenceStat> stats;
    double total_audio_sec;
    double total_cpu_sec;
    double avg_rtf;
    double avg_speedup_x;
};

int main(int argc, char* argv[]) {
    int threads = 1;
    if (argc > 1) {
        threads = std::stoi(argv[1]);
    }

    std::string story_file = "extra_long_story.txt";
    std::ifstream f(story_file);
    if (!f.is_open()) {
        std::cerr << "❌ Không mở được file: " << story_file << std::endl;
        return 1;
    }
    std::stringstream buf;
    buf << f.rdbuf();
    std::string full_story = buf.str();

    print_sep('=');
    std::cout << "🎙️  ĐO KIỂM TỐC ĐỘ & HIỆU NĂNG SINH ÂM THANH LIÊN TỤC VỚI VOICE PROMPT\n";
    std::cout << "🖥️  Cấu hình: " << threads << " Core CPU | Mô hình: INT8 2-Step Sway Sampling\n";
    std::cout << "🎯 Tối ưu: Dữ liệu Mel Prompt chuẩn Denormalized Log-Mel theo train_full_22050.py\n";
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
        std::cerr << "❌ Khởi tạo mô hình thất bại!" << std::endl;
        return 1;
    }

    auto sentences = engine.text_processor().split_sentences(full_story);
    std::cout << "📖 Đoạn truyện dài gồm: " << sentences.size() << " câu hoàn chỉnh.\n" << std::endl;

    const int SAMPLE_RATE = 22050;
    const int N_FEATS = 80;
    std::vector<float> silence_buf(static_cast<size_t>(0.15f * SAMPLE_RATE), 0.0f);

    TTSConfig cfg;
    cfg.n_timesteps = 2;
    cfg.temperature = 0.9f;
    cfg.length_scale = 1.0f; // Chuẩn tự nhiên 1.0x
    cfg.pause_sec = 0.15f;
    cfg.use_gpu = false;
    cfg.use_sway = true;
    cfg.sway_coef = -1.0f;
    cfg.tail_words_prompt = 4; // Bóc tách 4 từ đuôi câu trước làm Prompt

    std::vector<ModeResult> all_modes;

    // -------------------------------------------------------------
    // CHẾ ĐỘ 1: DÙNG DYNAMIC PROMPT CHAINING LIÊN TỤC
    // -------------------------------------------------------------
    {
        print_sep('-');
        std::cout << "⚡ CHẾ ĐỘ 1: BẬT PROMPT CHAINING (Nối nhịp 4 từ đuôi câu trước)\n";
        print_sep('-');

        ModeResult m1;
        m1.mode_name = "Dynamic Prompt Chaining (Chuẩn Log-Mel)";
        m1.wav_path = "outputs/story_with_prompt_int8.wav";

        std::vector<float> full_audio;
        std::vector<float> prev_mel;
        int64_t prev_mel_len = 0;
        std::string prev_sentence = "";

        auto t_start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < sentences.size(); ++i) {
            const auto& s = sentences[i];

            std::vector<float> prompt_mel;
            int64_t prompt_frames = 0;

            if (prev_mel_len > 0 && !prev_mel.empty()) {
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
                        prompt_mel[c * prompt_frames + f] = prev_mel[c * prev_mel_len + src];
                    }
                }
            }

            std::vector<float> cur_mel;
            int64_t cur_mel_len = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            auto chunk = engine.synthesize_sentence(s, cfg, cur_mel, cur_mel_len, prompt_mel, prompt_frames, false);
            auto t1 = std::chrono::high_resolution_clock::now();

            float sec_cpu = std::chrono::duration<float>(t1 - t0).count();
            float sec_audio = static_cast<float>(chunk.size()) / static_cast<float>(SAMPLE_RATE);
            float speed_x = (sec_cpu > 0.0f) ? (sec_audio / sec_cpu) : 0.0f;

            full_audio.insert(full_audio.end(), chunk.begin(), chunk.end());
            if (i + 1 < sentences.size()) {
                full_audio.insert(full_audio.end(), silence_buf.begin(), silence_buf.end());
            }

            std::string preview = s.substr(0, std::min<size_t>(s.size(), 40));
            if (s.size() > 40) preview += "...";

            SentenceStat stat;
            stat.id = static_cast<int>(i + 1);
            stat.prompt_frames = static_cast<int>(prompt_frames);
            stat.mel_frames = static_cast<int>(cur_mel_len);
            stat.audio_sec = sec_audio;
            stat.cpu_sec = sec_cpu;
            stat.speed_x = speed_x;
            stat.preview = preview;
            m1.stats.push_back(stat);

            std::cout << "  [Câu " << std::setw(2) << stat.id << "] "
                      << "Prompt: " << std::setw(3) << prompt_frames << "f | "
                      << "Mel: " << std::setw(3) << cur_mel_len << "f | "
                      << "Audio: " << std::fixed << std::setprecision(2) << sec_audio << "s | "
                      << "CPU: " << std::setprecision(3) << sec_cpu << "s (" << (sec_cpu * 1000.0f) << " ms) | "
                      << "Tốc độ: " << std::setprecision(1) << speed_x << "x RT" << std::endl;

            prev_mel = std::move(cur_mel);
            prev_mel_len = cur_mel_len;
            prev_sentence = s;
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        m1.total_cpu_sec = std::chrono::duration<double>(t_end - t_start).count();
        m1.total_audio_sec = static_cast<double>(full_audio.size()) / static_cast<double>(SAMPLE_RATE);
        m1.avg_rtf = m1.total_cpu_sec / m1.total_audio_sec;
        m1.avg_speedup_x = m1.total_audio_sec / m1.total_cpu_sec;

        TTSEngine::normalize_audio(full_audio, 0.96f);
        TTSEngine::save_wav(m1.wav_path, full_audio, SAMPLE_RATE);
        all_modes.push_back(m1);

        std::cout << "  💾 Đã lưu: " << m1.wav_path << " (" << m1.total_audio_sec << " giây)\n";
        std::cout << "  🚀 TỐC ĐỘ TRUNG BÌNH CÓ PROMPT: " << std::setprecision(1) << m1.avg_speedup_x << "x Realtime\n" << std::endl;
    }

    // -------------------------------------------------------------
    // CHẾ ĐỘ 2: KHÔNG DÙNG PROMPT (ZERO PROMPT - BASELINE ĐỘC LẬP)
    // -------------------------------------------------------------
    {
        print_sep('-');
        std::cout << "⚡ CHẾ ĐỘ 2: TẮT PROMPT (Zero Prompt - Các câu chạy độc lập)\n";
        print_sep('-');

        ModeResult m2;
        m2.mode_name = "Zero Prompt (Độc lập từng câu)";
        m2.wav_path = "outputs/story_zero_prompt_int8.wav";

        std::vector<float> full_audio;
        auto t_start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < sentences.size(); ++i) {
            const auto& s = sentences[i];

            std::vector<float> cur_mel;
            int64_t cur_mel_len = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            auto chunk = engine.synthesize_sentence(s, cfg, cur_mel, cur_mel_len, {}, 0, false);
            auto t1 = std::chrono::high_resolution_clock::now();

            float sec_cpu = std::chrono::duration<float>(t1 - t0).count();
            float sec_audio = static_cast<float>(chunk.size()) / static_cast<float>(SAMPLE_RATE);
            float speed_x = (sec_cpu > 0.0f) ? (sec_audio / sec_cpu) : 0.0f;

            full_audio.insert(full_audio.end(), chunk.begin(), chunk.end());
            if (i + 1 < sentences.size()) {
                full_audio.insert(full_audio.end(), silence_buf.begin(), silence_buf.end());
            }

            SentenceStat stat;
            stat.id = static_cast<int>(i + 1);
            stat.prompt_frames = 0;
            stat.mel_frames = static_cast<int>(cur_mel_len);
            stat.audio_sec = sec_audio;
            stat.cpu_sec = sec_cpu;
            stat.speed_x = speed_x;
            stat.preview = s.substr(0, std::min<size_t>(s.size(), 40));
            m2.stats.push_back(stat);

            std::cout << "  [Câu " << std::setw(2) << stat.id << "] "
                      << "Prompt:   0f | "
                      << "Mel: " << std::setw(3) << cur_mel_len << "f | "
                      << "Audio: " << std::fixed << std::setprecision(2) << sec_audio << "s | "
                      << "CPU: " << std::setprecision(3) << sec_cpu << "s (" << (sec_cpu * 1000.0f) << " ms) | "
                      << "Tốc độ: " << std::setprecision(1) << speed_x << "x RT" << std::endl;
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        m2.total_cpu_sec = std::chrono::duration<double>(t_end - t_start).count();
        m2.total_audio_sec = static_cast<double>(full_audio.size()) / static_cast<double>(SAMPLE_RATE);
        m2.avg_rtf = m2.total_cpu_sec / m2.total_audio_sec;
        m2.avg_speedup_x = m2.total_audio_sec / m2.total_cpu_sec;

        TTSEngine::normalize_audio(full_audio, 0.96f);
        TTSEngine::save_wav(m2.wav_path, full_audio, SAMPLE_RATE);
        all_modes.push_back(m2);

        std::cout << "  💾 Đã lưu: " << m2.wav_path << " (" << m2.total_audio_sec << " giây)\n";
        std::cout << "  🚀 TỐC ĐỘ TRUNG BÌNH KHÔNG PROMPT: " << std::setprecision(1) << m2.avg_speedup_x << "x Realtime\n" << std::endl;
    }

    print_sep('=');
    std::cout << "📊 BẢNG TỔNG HỢP ĐỐI ĐẦU: CÓ PROMPT CHAINING vs KHÔNG DÙNG PROMPT\n";
    print_sep('=');
    std::cout << std::left
              << std::setw(36) << "Chế độ kiểm thử"
              << std::setw(16) << "Tổng Audio"
              << std::setw(18) << "Thời gian CPU"
              << std::setw(14) << "RTF"
              << "Tốc độ sinh (xRT)" << std::endl;
    print_sep('-', 110);

    for (const auto& m : all_modes) {
        std::cout << std::left
                  << std::setw(36) << m.mode_name
                  << std::fixed << std::setprecision(2)
                  << std::setw(16) << (std::to_string(m.total_audio_sec).substr(0, 5) + " giây")
                  << std::setprecision(3)
                  << std::setw(18) << (std::to_string(m.total_cpu_sec).substr(0, 5) + " giây")
                  << std::setprecision(4)
                  << std::setw(14) << m.avg_rtf
                  << std::setprecision(1)
                  << ("🚀 " + std::to_string(m.avg_speedup_x).substr(0, 4) + "x Realtime") << std::endl;
    }
    print_sep('=', 110);

    return 0;
}
