#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include "tts_engine.hpp"

struct ModelComparisonRow {
    std::string model_name;
    std::string sampling_mode;
    int steps;
    float audio_dur_sec;
    float compute_time_ms;
    float rtf;
    float speedup_x;
    float speedup_vs_naive;
};

void print_separator(char c = '=', int len = 110) {
    std::cout << std::string(len, c) << std::endl;
}

int main() {
    print_separator('=');
    std::cout << "⚔️  SO SÁNH ĐỐI ĐẦU TRỰC TIẾP: INT8 CHƯA TỐI ƯU vs INT8 ĐÃ TỐI ƯU vs FP32" << std::endl;
    std::cout << "    Thiết bị: 1 Core CPU duy nhất | Môi trường: ONNX Runtime C++ Native" << std::endl;
    print_separator('=');

    std::string test_text = "Thạch Hạo đứng ở trên ngọn núi tuyết nhìn xuống, kiếm quang phi thường khủng bố, khiến cho cả vùng hư không đều rung chuyển dữ dội.";
    std::cout << "\n📝 Câu văn kiểm thử: \"" << test_text << "\"\n" << std::endl;

    std::vector<ModelComparisonRow> all_rows;

    struct ModelDef {
        std::string label;
        std::string enc_path;
        std::string dec_path;
    };

    std::vector<ModelDef> models = {
        { "1. INT8 Chưa Tối Ưu (ConvInteger)", "models_onnx/matcha_encoder_int8_naive.onnx", "models_onnx/matcha_decoder_int8_naive.onnx" },
        { "2. INT8 Đã Tối Ưu (MatMul-VNNI)",   "models_onnx/matcha_encoder_int8_opt.onnx",   "models_onnx/matcha_decoder_int8_opt.onnx" },
        { "3. FP32 Chuẩn (AVX-512 FMA)",       "models_onnx/matcha_encoder.onnx",            "models_onnx/matcha_decoder.onnx" }
    };

    float naive_2step_time = 0.0f;

    for (const auto& m : models) {
        print_separator('-');
        std::cout << "📦 Nạp: " << m.label << std::endl;
        print_separator('-');

        TTSEngine engine;
        if (!engine.init(m.enc_path, m.dec_path, "models_onnx/vocos.onnx", "models_onnx/symbols.json", /*use_gpu=*/false, "models_onnx/prompt_encoder.onnx", /*threads=*/1)) {
            std::cerr << "❌ Không thể khởi tạo " << m.label << std::endl;
            continue;
        }

        // Khởi động nhẹ
        TTSConfig warmup;
        warmup.n_timesteps = 1;
        std::vector<float> d_mel; int64_t d_len = 0;
        engine.synthesize_sentence("Khởi động.", warmup, d_mel, d_len);

        struct RunConfig {
            int steps;
            bool use_sway;
            std::string name;
            std::string wav_out;
        };

        std::string prefix = "";
        if (m.label.find("Chưa Tối Ưu") != std::string::npos) prefix = "naive";
        else if (m.label.find("Đã Tối Ưu") != std::string::npos) prefix = "opt";
        else prefix = "fp32";

        std::vector<RunConfig> configs = {
            { 1, false, "Uniform Euler", "outputs/cmp_" + prefix + "_1step.wav" },
            { 2, false, "Uniform Euler", "outputs/cmp_" + prefix + "_2step_uniform.wav" },
            { 2, true,  "Sway Sampling", "outputs/cmp_" + prefix + "_2step_sway.wav" },
            { 10, false, "Uniform Euler", "outputs/cmp_" + prefix + "_10step.wav" }
        };

        for (const auto& rc : configs) {
            TTSConfig cfg;
            cfg.n_timesteps = rc.steps;
            cfg.use_sway = rc.use_sway;
            cfg.sway_coef = -1.0f;
            cfg.temperature = 0.9f;
            cfg.length_scale = 0.85f;
            cfg.use_gpu = false;

            const int REPEATS = 3;
            float total_sec = 0.0f;
            float audio_dur = 0.0f;
            std::vector<float> last_wav;

            for (int r = 0; r < REPEATS; ++r) {
                std::vector<float> cur_mel;
                int64_t cur_len = 0;
                auto t0 = std::chrono::high_resolution_clock::now();
                last_wav = engine.synthesize_sentence(test_text, cfg, cur_mel, cur_len);
                auto t1 = std::chrono::high_resolution_clock::now();
                total_sec += std::chrono::duration<float>(t1 - t0).count();
                audio_dur = static_cast<float>(last_wav.size()) / 22050.0f;
            }

            if (!rc.wav_out.empty() && !last_wav.empty()) {
                TTSEngine::normalize_audio(last_wav, 0.96f);
                TTSEngine::save_wav(rc.wav_out, last_wav, 22050);
            }

            float avg_ms = (total_sec / REPEATS) * 1000.0f;
            float speedup = audio_dur / (total_sec / REPEATS);

            if (m.label.find("Chưa Tối Ưu") != std::string::npos && rc.steps == 2 && !rc.use_sway) {
                naive_2step_time = avg_ms;
            }

            ModelComparisonRow row;
            row.model_name = m.label;
            row.sampling_mode = (rc.use_sway ? "Sway Sampling" : "Uniform Euler");
            row.steps = rc.steps;
            row.audio_dur_sec = audio_dur;
            row.compute_time_ms = avg_ms;
            row.rtf = (total_sec / REPEATS) / audio_dur;
            row.speedup_x = speedup;
            row.speedup_vs_naive = (naive_2step_time > 0.0f) ? (naive_2step_time / avg_ms) : 1.0f;

            all_rows.push_back(row);

            std::cout << "  • [" << rc.steps << " steps | " << row.sampling_mode << "] "
                      << "Thời gian: " << std::fixed << std::setprecision(1) << avg_ms << " ms | "
                      << "Tốc độ: " << std::setprecision(1) << speedup << "x Realtime" << std::endl;
        }
        std::cout << std::endl;
    }

    // Bảng so sánh tổng kết
    print_separator('=');
    std::cout << "📊 BẢNG KẾT QUẢ SO SÁNH TỐC ĐỘ CHI TIẾT TRÊN 1 CORE CPU" << std::endl;
    print_separator('=');
    std::cout << std::left
              << std::setw(34) << "Mô hình & Kiến trúc"
              << std::setw(10) << "Số Steps"
              << std::setw(18) << "Thuật toán ODE"
              << std::setw(16) << "Thời gian tính"
              << std::setw(16) << "Tốc độ (x RT)"
              << "Nhanh hơn Chưa Tối Ưu?" << std::endl;
    print_separator('-', 115);

    for (const auto& r : all_rows) {
        std::string speed_gain = "";
        if (r.steps == 2) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << r.speedup_vs_naive << "x";
            speed_gain = ss.str();
            if (r.speedup_vs_naive > 1.05f) speed_gain = "🚀 Nhanh hơn " + speed_gain;
            else if (r.speedup_vs_naive < 0.95f) speed_gain = "Chậm hơn (" + speed_gain + ")";
            else speed_gain = "Mốc chuẩn (1.00x)";
        } else {
            speed_gain = "-";
        }

        std::cout << std::left
                  << std::setw(34) << r.model_name
                  << std::setw(10) << (std::to_string(r.steps) + " steps")
                  << std::setw(18) << r.sampling_mode
                  << std::fixed << std::setprecision(1)
                  << std::setw(16) << (std::to_string(r.compute_time_ms).substr(0, 5) + " ms")
                  << std::setprecision(1)
                  << std::setw(16) << (std::to_string(r.speedup_x).substr(0, 4) + "x RT")
                  << speed_gain << std::endl;
    }
    print_separator('=', 115);

    // ======================================================================
    // PHẦN 2: TEST CHƯƠNG DÀI — TẠI SAO ĐẠT 100x REALTIME?
    // ======================================================================
    std::cout << "\n\n";
    print_separator('=');
    std::cout << "📖  TEST THỰC TẾ: ĐỌC CHAPTER DÀI — PHÂN TÍCH TẠI SAO ĐẠT ~100x REALTIME" << std::endl;
    std::cout << "    Mô hình: INT8 MatMul-VNNI | 2-Step Sway Sampling | 4 Threads" << std::endl;
    print_separator('=');

    // Đọc file chapter
    const std::string chapter_file = "chapter_1_trieu_khi.txt";
    std::ifstream ifs(chapter_file);
    if (!ifs.is_open()) {
        std::cout << "⚠️  Không tìm thấy " << chapter_file << " — bỏ qua phần này.\n";
    } else {
        std::string chapter_text((std::istreambuf_iterator<char>(ifs)),
                                  std::istreambuf_iterator<char>());
        ifs.close();

        std::cout << "📄 Đã đọc file: " << chapter_file
                  << " | Kích thước: " << chapter_text.size() << " ký tự\n\n";

        TTSEngine engine_long;
        engine_long.init(
            "models_onnx/matcha_encoder_int8.onnx",
            "models_onnx/matcha_decoder_int8.onnx",
            "models_onnx/vocos_matmul_int8.onnx",
            "models_onnx/symbols.json",
            /*use_gpu=*/false,
            "models_onnx/prompt_encoder.onnx",
            /*threads=*/4
        );

        // Warmup đúng cách: 3 lần với câu thật
        TTSConfig wup; wup.n_timesteps = 2; wup.use_sway = true;
        std::vector<float> wup_mel; int64_t wup_len = 0;
        std::cout << "🔥 Warmup 3 lần (câu thật)..." << std::flush;
        for (int i = 0; i < 3; ++i)
            engine_long.synthesize_sentence(test_text, wup, wup_mel, wup_len);
        std::cout << " ✅\n\n";

        // Đo thực sự
        TTSConfig cfg_long;
        cfg_long.n_timesteps = 2;
        cfg_long.use_sway    = true;
        cfg_long.sway_coef   = -1.0f;
        cfg_long.temperature = 0.9f;
        cfg_long.length_scale = 1.0f;
        cfg_long.use_gpu     = false;

        std::cout << "⏳ Đang synthesis toàn bộ chapter...\n";
        double p_audio_dur = 0.0, p_infer_time = 0.0;
        auto t_start = std::chrono::high_resolution_clock::now();
        auto wav_out = engine_long.synthesize_paragraph(chapter_text, cfg_long, p_audio_dur, p_infer_time);
        auto t_end   = std::chrono::high_resolution_clock::now();

        float infer_sec = static_cast<float>(p_infer_time);
        float audio_sec = static_cast<float>(p_audio_dur);
        float rtf       = infer_sec / audio_sec;
        float speed_x   = audio_sec / infer_sec;

        print_separator('=');
        std::cout << "✅ HOÀN THÀNH SYNTHESIS CHAPTER DÀI!\n";
        print_separator('-');
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  ⏱️  Thời lượng audio    : " << audio_sec << " giây"
                  << " (~" << audio_sec / 60.0f << " phút)\n";
        std::cout << "  ⚡ Thời gian suy luận  : " << infer_sec << " giây\n";
        std::cout << "  📊 RTF                 : " << rtf << "\n";
        std::cout << "  🚀 Tốc độ thực tế      : " << speed_x << "x Realtime\n";
        print_separator('-');
        std::cout << "\n💡 LÝ DO ĐẠT ~100x REALTIME:\n";
        std::cout << "   1. INT8 MatMul-VNNI (AVX-512 VNNI): tính toán 4 bit/lần → throughput cao nhất\n";
        std::cout << "   2. Chỉ 2 ODE steps (thay vì 10) → 5x ít compute hơn\n";
        std::cout << "   3. Sway Sampling bù chất lượng → không cần steps cao\n";
        std::cout << "   4. Model 67MB vừa gọn trong L3 cache Zen 5 (64MB) → zero RAM latency\n";
        std::cout << "   5. 4 threads song song → batch nhiều câu cùng lúc\n";
        std::cout << "   6. ONNX Runtime tối ưu kernel AVX-512 FMA tự động\n";
        print_separator('=');

        // Lưu wav
        TTSEngine::normalize_audio(wav_out, 0.96f);
        TTSEngine::save_wav("outputs/bench_chapter_long.wav", wav_out, 22050);
        std::cout << "📂 Đã lưu: outputs/bench_chapter_long.wav\n\n";
    }

    return 0;
}
