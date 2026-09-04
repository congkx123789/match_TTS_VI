#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include "tts_engine.hpp"

struct BenchmarkRow {
    std::string config_name;
    std::string model_type;
    int threads;
    int steps;
    float audio_dur_sec;
    float compute_time_ms;
    float rtf;
    float speedup_x;
    std::string is_realtime;
};

void print_separator(char c = '=', int len = 105) {
    std::cout << std::string(len, c) << std::endl;
}

int main() {
    print_separator('=');
    std::cout << "⚡ KIỂM THỬ ĐỘC LẬP TỐC ĐỘ TRÊN 1 CORE CPU & ĐA CORE (1 STEP vs 2 vs 5 vs 10 STEPS)" << std::endl;
    std::cout << "   Mục tiêu: Đánh giá chi tiết hiệu năng Realtime Factor (RTF) khi bị giới hạn 1 CPU Core." << std::endl;
    print_separator('=');

    std::string test_text = "Thạch Hạo đứng ở trên ngọn núi tuyết nhìn xuống, kiếm quang phi thường khủng bố, khiến cho cả vùng hư không đều rung chuyển dữ dội.";
    std::cout << "\n📝 Câu văn kiểm thử: \"" << test_text << "\"\n" << std::endl;

    std::vector<BenchmarkRow> results;

    auto run_bench = [&](
        const std::string& config_name,
        const std::string& model_type,
        int threads,
        const std::vector<int>& steps_list
    ) {
        std::string enc = (model_type == "int8") ? "models_onnx/matcha_encoder_int8.onnx" : "models_onnx/matcha_encoder.onnx";
        std::string dec = (model_type == "int8") ? "models_onnx/matcha_decoder_int8.onnx" : "models_onnx/matcha_decoder.onnx";
        std::string voc = "models_onnx/vocos.onnx";
        std::string sym = "models_onnx/symbols.json";
        std::string pe  = "models_onnx/prompt_encoder.onnx";

        TTSEngine engine;
        if (!engine.init(enc, dec, voc, sym, /*use_gpu=*/false, pe, threads)) {
            std::cerr << "❌ Không thể khởi tạo cho " << config_name << std::endl;
            return;
        }

        // Khởi động nhẹ (warmup 1 lượt)
        TTSConfig warmup_cfg;
        warmup_cfg.n_timesteps = 1;
        warmup_cfg.temperature = 0.9f;
        warmup_cfg.length_scale = 0.85f;
        std::vector<float> dummy_mel;
        int64_t dummy_len = 0;
        engine.synthesize_sentence("Khởi động hệ thống.", warmup_cfg, dummy_mel, dummy_len);

        for (int steps : steps_list) {
            TTSConfig cfg;
            cfg.n_timesteps = steps;
            cfg.temperature = 0.9f;
            cfg.length_scale = 0.85f;
            cfg.use_gpu = false;

            // Đo đạc chính xác 3 lần lấy trung bình
            const int NUM_RUNS = 3;
            float total_compute_sec = 0.0f;
            float audio_dur_sec = 0.0f;

            for (int r = 0; r < NUM_RUNS; ++r) {
                std::vector<float> cur_mel;
                int64_t cur_len = 0;

                auto t0 = std::chrono::high_resolution_clock::now();
                auto wav = engine.synthesize_sentence(test_text, cfg, cur_mel, cur_len);
                auto t1 = std::chrono::high_resolution_clock::now();

                total_compute_sec += std::chrono::duration<float>(t1 - t0).count();
                audio_dur_sec = static_cast<float>(wav.size()) / 22050.0f;
            }

            float avg_compute_sec = total_compute_sec / static_cast<float>(NUM_RUNS);
            float rtf = avg_compute_sec / audio_dur_sec;
            float speedup = audio_dur_sec / avg_compute_sec;

            BenchmarkRow row;
            row.config_name = config_name;
            row.model_type = model_type;
            row.threads = threads;
            row.steps = steps;
            row.audio_dur_sec = audio_dur_sec;
            row.compute_time_ms = avg_compute_sec * 1000.0f;
            row.rtf = rtf;
            row.speedup_x = speedup;
            row.is_realtime = (speedup >= 1.0f) ? "✅ Siêu Realtime" : "❌ Chậm hơn RT";
            results.push_back(row);

            std::cout << "  • [" << config_name << " | " << steps << " steps] "
                      << "Audio: " << std::fixed << std::setprecision(2) << audio_dur_sec << "s | "
                      << "Thời gian tính: " << std::setprecision(1) << row.compute_time_ms << " ms ("
                      << std::setprecision(3) << avg_compute_sec << "s) | "
                      << "Tốc độ: " << std::setprecision(1) << speedup << "x Realtime" << std::endl;
        }
        std::cout << std::endl;
    };

    // 1. Chạy trên 1 CORE duy nhất (threads = 1, FP32)
    print_separator('-');
    std::cout << "👉 PHẦN 1: THỬ NGHIỆM TRÊN 1 CORE CPU DUY NHẤT (THREADS = 1, FP32)" << std::endl;
    print_separator('-');
    run_bench("1 Core (Single-thread)", "fp32", 1, { 1, 2, 5, 10 });

    // 2. Chạy trên 1 CORE duy nhất với INT8 (threads = 1, INT8)
    print_separator('-');
    std::cout << "👉 PHẦN 2: THỬ NGHIỆM TRÊN 1 CORE VỚI MÔ HÌNH INT8 SIÊU NHẸ" << std::endl;
    print_separator('-');
    run_bench("1 Core (INT8)", "int8", 1, { 1, 2, 10 });

    // 3. Chạy trên ĐA LUỒNG CPU (threads = 4, FP32) để đối chiếu
    print_separator('-');
    std::cout << "👉 PHẦN 3: ĐỐI CHỨNG VỚI 4 LUỒNG CPU ĐA NHÂN (THREADS = 4, FP32)" << std::endl;
    print_separator('-');
    run_bench("4 Cores (Multi-thread)", "fp32", 4, { 1, 2, 10 });

    // In bảng so sánh trực quan toàn diện
    print_separator('=');
    std::cout << "📊 BẢNG TỔNG KẾT ĐO LƯỜNG TỐC ĐỘ VÀ REALTIME FACTOR (RTF)" << std::endl;
    print_separator('=');
    std::cout << std::left
              << std::setw(24) << "Cấu hình thử nghiệm"
              << std::setw(8)  << "Định dạng"
              << std::setw(8)  << "Số Cores"
              << std::setw(10) << "Số Steps"
              << std::setw(14) << "Độ dài Audio"
              << std::setw(16) << "Thời gian tính"
              << std::setw(12) << "RTF"
              << std::setw(16) << "Tốc độ (x RT)"
              << "Đạt Realtime?" << std::endl;
    print_separator('-', 115);

    for (const auto& r : results) {
        std::cout << std::left
                  << std::setw(24) << r.config_name
                  << std::setw(8)  << r.model_type
                  << std::setw(8)  << r.threads
                  << std::setw(10) << (std::to_string(r.steps) + " steps")
                  << std::fixed << std::setprecision(2)
                  << std::setw(14) << (std::to_string(r.audio_dur_sec).substr(0, 4) + " s")
                  << std::setprecision(1)
                  << std::setw(16) << (std::to_string(r.compute_time_ms).substr(0, 5) + " ms")
                  << std::setprecision(4)
                  << std::setw(12) << r.rtf
                  << std::setprecision(1)
                  << std::setw(16) << (std::to_string(r.speedup_x).substr(0, 4) + "x RT")
                  << r.is_realtime << std::endl;
    }
    print_separator('=', 115);

    return 0;
}
