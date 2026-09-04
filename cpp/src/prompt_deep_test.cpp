#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include "tts_engine.hpp"

// Cấu trúc lưu vết kết quả của từng câu
struct SentenceMetric {
    int id;
    std::string text_preview;
    float audio_duration_sec;
    float cpu_time_sec;
    int64_t mel_frames;
    int64_t prompt_frames;
    float rtf;
    float speed_ratio;
};

// Cấu trúc lưu vết toàn bộ bài test
struct TestRunResult {
    std::string test_name;
    std::string wav_filename;
    std::vector<SentenceMetric> sentence_metrics;
    double total_audio_dur;
    double total_cpu_time;
    double overall_rtf;
    double overall_speedup;
};

// Hàm đọc file text UTF-8
std::string read_file_content(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// In tiêu đề phân cách
void print_separator(char c = '=', int len = 80) {
    std::cout << std::string(len, c) << std::endl;
}

// In tiêu đề mục
void print_header(const std::string& title) {
    std::cout << "\n";
    print_separator('=');
    std::cout << "🎯 " << title << std::endl;
    print_separator('=');
}

int main(int argc, char* argv[]) {
    print_separator('=');
    std::cout << "🎙️  KIỂM THỬ CHUYÊN SÂU PROMPT TRÊN VĂN BẢN CỰC DÀI (C++ NATIVE CPU)" << std::endl;
    std::cout << "    Mô hình: Matcha-TTS (10 steps) + Vocos | Thiết bị: CPU Đa luồng | Temp: 0.9" << std::endl;
    print_separator('=');

    std::string input_path = "extra_long_story.txt";
    if (argc > 1) {
        input_path = argv[1];
    }

    std::string full_story = read_file_content(input_path);
    if (full_story.empty()) {
        std::cerr << "❌ Không thể mở hoặc đọc file: " << input_path << std::endl;
        return 1;
    }

    // 1. Khởi tạo TTSEngine trên CPU thuần
    TTSEngine engine;
    if (!engine.init(
        "models_onnx/matcha_encoder.onnx",
        "models_onnx/matcha_decoder.onnx",
        "models_onnx/vocos.onnx",
        "models_onnx/symbols.json",
        /*use_gpu=*/false,
        "models_onnx/prompt_encoder.onnx"
    )) {
        std::cerr << "❌ Khởi tạo TTS Engine thất bại!" << std::endl;
        return 1;
    }

    // 2. Phân đoạn câu tiếng Việt
    auto sentences = engine.text_processor().split_sentences(full_story);
    std::cout << "\n📚 Văn bản đầu vào: \"" << input_path << "\"" << std::endl;
    std::cout << "  • Tổng số câu bóc tách : " << sentences.size() << " câu" << std::endl;

    TTSConfig config;
    config.n_timesteps = 10;
    config.temperature = 0.9f;
    config.length_scale = 0.85f;
    config.pause_sec = 0.16f;
    config.use_gpu = false;
    config.tail_words_prompt = 4;

    const int SAMPLE_RATE = 22050;
    const int N_FEATS = 80;
    std::vector<float> silence_gap(static_cast<size_t>(config.pause_sec * SAMPLE_RATE), 0.0f);

    std::vector<TestRunResult> all_results;

    // =========================================================================
    // TEST 1: CHẾ ĐỘ DYNAMIC TAIL PROMPT CHAINING (Nối tiếp đuôi 4 từ liên tục)
    // =========================================================================
    {
        print_header("CHẾ ĐỘ 1: DYNAMIC TAIL PROMPT CHAINING (Nối tiếp 4 từ đuôi câu trước)");
        std::cout << "👉 Cơ chế: Lấy ~4 từ cuối của câu N-1 (chiết xuất mel) làm Prompt cho câu N.\n"
                  << "   Mục đích: Giữ luồng cảm xúc, nhịp thở và ngữ điệu chuyển tiếp tự nhiên giữa các câu.\n" << std::endl;

        TestRunResult run1;
        run1.test_name = "Dynamic Tail Chaining (4 words)";
        run1.wav_filename = "outputs/deep_test_1_dynamic_chaining.wav";

        std::vector<float> full_audio;
        std::vector<float> prev_norm_mel;
        int64_t prev_mel_len = 0;
        std::string prev_sentence = "";

        auto t_start_total = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < sentences.size(); ++i) {
            const auto& sentence = sentences[i];

            // Trích xuất Mel Prompt từ đuôi câu trước
            std::vector<float> prompt_norm_mel;
            int64_t prompt_mel_frames = 0;

            if (prev_mel_len > 0 && !prev_norm_mel.empty()) {
                int word_count = 1;
                for (char c : prev_sentence) if (c == ' ') word_count++;
                int actual_n = std::min(word_count, std::max(3, config.tail_words_prompt));
                float ratio = static_cast<float>(actual_n) / static_cast<float>(word_count);
                prompt_mel_frames = std::max<int64_t>(30, static_cast<int64_t>(static_cast<float>(prev_mel_len) * ratio));
                prompt_mel_frames = std::min<int64_t>(prompt_mel_frames, prev_mel_len);

                prompt_norm_mel.resize(N_FEATS * prompt_mel_frames);
                for (int c = 0; c < N_FEATS; ++c) {
                    for (int64_t f = 0; f < prompt_mel_frames; ++f) {
                        int64_t src = (prev_mel_len - prompt_mel_frames) + f;
                        prompt_norm_mel[c * prompt_mel_frames + f] = prev_norm_mel[c * prev_mel_len + src];
                    }
                }
            }

            std::vector<float> cur_norm_mel;
            int64_t cur_mel_len = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            auto chunk_wav = engine.synthesize_sentence(
                sentence, config, cur_norm_mel, cur_mel_len, prompt_norm_mel, prompt_mel_frames
            );
            auto t1 = std::chrono::high_resolution_clock::now();

            float sec_cpu = std::chrono::duration<float>(t1 - t0).count();
            float sec_audio = static_cast<float>(chunk_wav.size()) / static_cast<float>(SAMPLE_RATE);
            float rtf = (sec_audio > 0.0f) ? (sec_cpu / sec_audio) : 0.0f;
            float speed_x = (sec_cpu > 0.0f) ? (sec_audio / sec_cpu) : 0.0f;

            full_audio.insert(full_audio.end(), chunk_wav.begin(), chunk_wav.end());
            if (i + 1 < sentences.size()) {
                full_audio.insert(full_audio.end(), silence_gap.begin(), silence_gap.end());
            }

            std::string preview = sentence.substr(0, std::min<size_t>(sentence.size(), 42));
            if (sentence.size() > 42) preview += "...";

            SentenceMetric m;
            m.id = static_cast<int>(i + 1);
            m.text_preview = preview;
            m.audio_duration_sec = sec_audio;
            m.cpu_time_sec = sec_cpu;
            m.mel_frames = cur_mel_len;
            m.prompt_frames = prompt_mel_frames;
            m.rtf = rtf;
            m.speed_ratio = speed_x;
            run1.sentence_metrics.push_back(m);

            std::cout << "  [Câu " << std::setw(2) << m.id << "] "
                      << "Prompt: " << std::setw(3) << prompt_mel_frames << "f | "
                      << "Mel: " << std::setw(3) << cur_mel_len << "f | "
                      << "Audio: " << std::fixed << std::setprecision(2) << sec_audio << "s | "
                      << "CPU: " << std::setprecision(3) << sec_cpu << "s (" << std::setprecision(1) << speed_x << "x RT) | "
                      << "\"" << preview << "\"" << std::endl;

            prev_norm_mel = std::move(cur_norm_mel);
            prev_mel_len = cur_mel_len;
            prev_sentence = sentence;
        }

        auto t_end_total = std::chrono::high_resolution_clock::now();
        run1.total_cpu_time = std::chrono::duration<double>(t_end_total - t_start_total).count();
        run1.total_audio_dur = static_cast<double>(full_audio.size()) / static_cast<double>(SAMPLE_RATE);
        run1.overall_rtf = run1.total_cpu_time / run1.total_audio_dur;
        run1.overall_speedup = run1.total_audio_dur / run1.total_cpu_time;

        TTSEngine::normalize_audio(full_audio, 0.96f);
        TTSEngine::save_wav(run1.wav_filename, full_audio, SAMPLE_RATE);
        std::cout << "  💾 Đã lưu file: " << run1.wav_filename << " (" << run1.total_audio_dur << " giây)\n";
        all_results.push_back(run1);
    }

    // =========================================================================
    // TEST 2: CHẾ ĐỘ FIXED ANCHOR PROMPT (Neo giữ giọng mẫu mở đầu)
    // =========================================================================
    {
        print_header("CHẾ ĐỘ 2: FIXED ANCHOR VOICE PROMPT (Neo chuẩn giọng câu mở đầu)");
        std::cout << "👉 Cơ chế: Lấy đặc trưng Mel của Câu 1 làm mẫu mỏ neo (Anchor) áp dụng cho TẤT CẢ câu sau.\n"
                  << "   Mục đích: Đảm bảo toàn bộ câu chuyện 100% đồng nhất về màu giọng và cao độ F0.\n" << std::endl;

        TestRunResult run2;
        run2.test_name = "Fixed Anchor Reference Prompt";
        run2.wav_filename = "outputs/deep_test_2_anchor_prompt.wav";

        std::vector<float> full_audio;
        std::vector<float> anchor_mel;
        int64_t anchor_mel_frames = 0;

        auto t_start_total = std::chrono::high_resolution_clock::now();

        // Bước chuẩn bị: Tạo câu 1 làm Anchor
        {
            std::vector<float> out_mel;
            int64_t out_len = 0;
            auto t0 = std::chrono::high_resolution_clock::now();
            auto chunk_wav = engine.synthesize_sentence(sentences[0], config, out_mel, out_len, {}, 0);
            auto t1 = std::chrono::high_resolution_clock::now();

            float sec_cpu = std::chrono::duration<float>(t1 - t0).count();
            float sec_audio = static_cast<float>(chunk_wav.size()) / static_cast<float>(SAMPLE_RATE);

            full_audio.insert(full_audio.end(), chunk_wav.begin(), chunk_wav.end());
            full_audio.insert(full_audio.end(), silence_gap.begin(), silence_gap.end());

            // Chọn 60 frames ổn định ở giữa câu 1 làm Anchor Prompt
            anchor_mel_frames = std::min<int64_t>(60, out_len);
            anchor_mel.resize(N_FEATS * anchor_mel_frames);
            int64_t start_f = std::max<int64_t>(0, (out_len - anchor_mel_frames) / 2);
            for (int c = 0; c < N_FEATS; ++c) {
                for (int64_t f = 0; f < anchor_mel_frames; ++f) {
                    anchor_mel[c * anchor_mel_frames + f] = out_mel[c * out_len + (start_f + f)];
                }
            }

            std::string preview = sentences[0].substr(0, std::min<size_t>(sentences[0].size(), 42)) + "...";
            SentenceMetric m{ 1, preview, sec_audio, sec_cpu, out_len, 0, sec_cpu / sec_audio, sec_audio / sec_cpu };
            run2.sentence_metrics.push_back(m);

            std::cout << "  [Câu  1] (Anchor Root) Prompt:   0f | Mel: " << out_len << "f | "
                      << "Audio: " << std::fixed << std::setprecision(2) << sec_audio << "s | "
                      << "CPU: " << std::setprecision(3) << sec_cpu << "s (" << (sec_audio/sec_cpu) << "x RT)" << std::endl;
        }

        // Chạy các câu từ 2 đến N với Anchor Prompt cố định
        for (size_t i = 1; i < sentences.size(); ++i) {
            const auto& sentence = sentences[i];
            std::vector<float> cur_norm_mel;
            int64_t cur_mel_len = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            auto chunk_wav = engine.synthesize_sentence(
                sentence, config, cur_norm_mel, cur_mel_len, anchor_mel, anchor_mel_frames
            );
            auto t1 = std::chrono::high_resolution_clock::now();

            float sec_cpu = std::chrono::duration<float>(t1 - t0).count();
            float sec_audio = static_cast<float>(chunk_wav.size()) / static_cast<float>(SAMPLE_RATE);
            float rtf = (sec_audio > 0.0f) ? (sec_cpu / sec_audio) : 0.0f;
            float speed_x = (sec_cpu > 0.0f) ? (sec_audio / sec_cpu) : 0.0f;

            full_audio.insert(full_audio.end(), chunk_wav.begin(), chunk_wav.end());
            if (i + 1 < sentences.size()) {
                full_audio.insert(full_audio.end(), silence_gap.begin(), silence_gap.end());
            }

            std::string preview = sentence.substr(0, std::min<size_t>(sentence.size(), 42));
            if (sentence.size() > 42) preview += "...";

            SentenceMetric m{ static_cast<int>(i + 1), preview, sec_audio, sec_cpu, cur_mel_len, anchor_mel_frames, rtf, speed_x };
            run2.sentence_metrics.push_back(m);

            std::cout << "  [Câu " << std::setw(2) << m.id << "] "
                      << "Prompt: " << std::setw(3) << anchor_mel_frames << "f | "
                      << "Mel: " << std::setw(3) << cur_mel_len << "f | "
                      << "Audio: " << std::fixed << std::setprecision(2) << sec_audio << "s | "
                      << "CPU: " << std::setprecision(3) << sec_cpu << "s (" << std::setprecision(1) << speed_x << "x RT) | "
                      << "\"" << preview << "\"" << std::endl;
        }

        auto t_end_total = std::chrono::high_resolution_clock::now();
        run2.total_cpu_time = std::chrono::duration<double>(t_end_total - t_start_total).count();
        run2.total_audio_dur = static_cast<double>(full_audio.size()) / static_cast<double>(SAMPLE_RATE);
        run2.overall_rtf = run2.total_cpu_time / run2.total_audio_dur;
        run2.overall_speedup = run2.total_audio_dur / run2.total_cpu_time;

        TTSEngine::normalize_audio(full_audio, 0.96f);
        TTSEngine::save_wav(run2.wav_filename, full_audio, SAMPLE_RATE);
        std::cout << "  💾 Đã lưu file: " << run2.wav_filename << " (" << run2.total_audio_dur << " giây)\n";
        all_results.push_back(run2);
    }

    // =========================================================================
    // TEST 3: CHẾ ĐỘ VANILLA NO-PROMPT (Độc lập từng câu - đối chứng)
    // =========================================================================
    {
        print_header("CHẾ ĐỘ 3: VANILLA NO-PROMPT (Độc lập từng câu - Nhóm đối chứng)");
        std::cout << "👉 Cơ chế: Mỗi câu hoàn toàn độc lập, không nạp bất kỳ Mel Prompt nào.\n"
                  << "   Mục đích: Làm mốc đối chứng kiểm tra tốc độ suy luận và độ biến thiên tự nhiên.\n" << std::endl;

        TestRunResult run3;
        run3.test_name = "Vanilla No-Prompt (Independent)";
        run3.wav_filename = "outputs/deep_test_3_no_prompt.wav";

        std::vector<float> full_audio;
        auto t_start_total = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < sentences.size(); ++i) {
            const auto& sentence = sentences[i];
            std::vector<float> cur_norm_mel;
            int64_t cur_mel_len = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            auto chunk_wav = engine.synthesize_sentence(
                sentence, config, cur_norm_mel, cur_mel_len, {}, 0
            );
            auto t1 = std::chrono::high_resolution_clock::now();

            float sec_cpu = std::chrono::duration<float>(t1 - t0).count();
            float sec_audio = static_cast<float>(chunk_wav.size()) / static_cast<float>(SAMPLE_RATE);
            float rtf = (sec_audio > 0.0f) ? (sec_cpu / sec_audio) : 0.0f;
            float speed_x = (sec_cpu > 0.0f) ? (sec_audio / sec_cpu) : 0.0f;

            full_audio.insert(full_audio.end(), chunk_wav.begin(), chunk_wav.end());
            if (i + 1 < sentences.size()) {
                full_audio.insert(full_audio.end(), silence_gap.begin(), silence_gap.end());
            }

            std::string preview = sentence.substr(0, std::min<size_t>(sentence.size(), 42));
            if (sentence.size() > 42) preview += "...";

            SentenceMetric m{ static_cast<int>(i + 1), preview, sec_audio, sec_cpu, cur_mel_len, 0, rtf, speed_x };
            run3.sentence_metrics.push_back(m);

            std::cout << "  [Câu " << std::setw(2) << m.id << "] "
                      << "Prompt:   0f | "
                      << "Mel: " << std::setw(3) << cur_mel_len << "f | "
                      << "Audio: " << std::fixed << std::setprecision(2) << sec_audio << "s | "
                      << "CPU: " << std::setprecision(3) << sec_cpu << "s (" << std::setprecision(1) << speed_x << "x RT) | "
                      << "\"" << preview << "\"" << std::endl;
        }

        auto t_end_total = std::chrono::high_resolution_clock::now();
        run3.total_cpu_time = std::chrono::duration<double>(t_end_total - t_start_total).count();
        run3.total_audio_dur = static_cast<double>(full_audio.size()) / static_cast<double>(SAMPLE_RATE);
        run3.overall_rtf = run3.total_cpu_time / run3.total_audio_dur;
        run3.overall_speedup = run3.total_audio_dur / run3.total_cpu_time;

        TTSEngine::normalize_audio(full_audio, 0.96f);
        TTSEngine::save_wav(run3.wav_filename, full_audio, SAMPLE_RATE);
        std::cout << "  💾 Đã lưu file: " << run3.wav_filename << " (" << run3.total_audio_dur << " giây)\n";
        all_results.push_back(run3);
    }

    // =========================================================================
    // TỔNG KẾT & SO SÁNH ĐỐI CHIẾU
    // =========================================================================
    print_header("BẢNG TỔNG HỢP SO SÁNH CÁC CHẾ ĐỘ PROMPT TRÊN CPU");
    std::cout << std::left 
              << std::setw(32) << "Chế độ Prompt"
              << std::setw(16) << "Thời lượng (s)"
              << std::setw(16) << "Thời gian CPU (s)"
              << std::setw(14) << "RTF"
              << std::setw(16) << "Tốc độ (x RT)"
              << "File âm thanh" << std::endl;
    print_separator('-', 95);

    for (const auto& r : all_results) {
        std::cout << std::left
                  << std::setw(32) << r.test_name
                  << std::fixed << std::setprecision(2)
                  << std::setw(16) << r.total_audio_dur
                  << std::setw(16) << r.total_cpu_time
                  << std::setprecision(4)
                  << std::setw(14) << r.overall_rtf
                  << std::setprecision(1)
                  << std::setw(16) << (std::to_string(r.overall_speedup).substr(0, 4) + "x RT")
                  << r.wav_filename << std::endl;
    }
    print_separator('=', 95);

    std::cout << "\n🎉 KIỂM THỬ THÀNH CÔNG RỰC RỠ! Cả 3 file âm thanh đã sẵn sàng để nghe kiểm tra.\n" << std::endl;

    return 0;
}
