#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <immintrin.h>
#include "tts_engine.hpp"

// Hàm ép CPU xóa sạch bộ nhớ đệm L1, L2, L3 bằng cách quét mảng 256MB trên DDR5 RAM
void flush_all_cpu_caches_to_ddr5() {
    const size_t CACHE_PURGE_SIZE = 256 * 1024 * 1024; // 256 MB (Gấp 4 lần tổng 64MB L3 của Ryzen 9 9900X)
    static std::vector<uint8_t> ddr5_buffer(CACHE_PURGE_SIZE, 0xAA);
    
    // Ghi và đọc đè lên toàn bộ 256MB để ép L1, L2, L3 phải evict toàn bộ dữ liệu model ra DDR5
    volatile uint8_t* ptr = ddr5_buffer.data();
    for (size_t i = 0; i < CACHE_PURGE_SIZE; i += 64) {
        ptr[i] += 1;
        _mm_clflush((const void*)&ptr[i]); // Lệnh phần cứng x86 ép flush cache line về DDR5
    }
    _mm_mfence(); // Rào cản bộ nhớ phần cứng đảm bảo dữ liệu đã về DDR5
}

int main(int argc, char* argv[]) {
    int threads = (argc > 1) ? std::stoi(argv[1]) : 1;
    int rounds = (argc > 2) ? std::stoi(argv[2]) : 3;

    std::string story_file = "extra_long_story.txt";
    std::ifstream f(story_file);
    if (!f.is_open()) {
        std::cerr << "❌ Không thể mở file: " << story_file << std::endl;
        return 1;
    }
    std::stringstream buf;
    buf << f.rdbuf();
    std::string text = buf.str();

    std::cout << "=================================================================================================" << std::endl;
    std::cout << "⚔️  THỬ NGHIỆM ĐỐI ĐẦU: TRONG L3 CACHE (HOT) vs ÉP CHẠY TRỰC TIẾP TỪ RAM DDR5 (COLD FLUSH)" << std::endl;
    std::cout << "🖥️  Phần cứng: AMD Ryzen 9 9900X (Zen 5) | Cấu hình: " << threads << " thread(s)" << std::endl;
    std::cout << "🎯 Phương pháp ép DDR5: Quét 256 MB RAM + lệnh phần cứng x86 _mm_clflush xóa sạch L1, L2, L3" << std::endl;
    std::cout << "=================================================================================================" << std::endl;

    TTSEngine engine;
    if (!engine.init(
        "models_onnx/matcha_encoder_int8.onnx",
        "models_onnx/matcha_decoder_int8.onnx",
        "models_onnx/vocos_matmul_int8.onnx",
        "models_onnx/symbols.json",
        false,
        "models_onnx/prompt_encoder.onnx",
        threads
    )) {
        std::cerr << "❌ Không thể nạp mô hình!" << std::endl;
        return 1;
    }

    TTSConfig config;
    config.n_timesteps = 2; // Sway Sampling 2-Step
    config.temperature = 0.9f;
    config.length_scale = 1.0f;
    config.pause_sec = 0.15f;

    // --- PHẦN 1: CHẠY TẬN DỤNG BỘ ĐỆM L3 CACHE (HOT CACHE) ---
    std::cout << "\n🔥 [PHẦN 1] CHẠY TẬN DỤNG L3 CACHE (Dữ liệu lưu trú trong SRAM CPU):" << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << " Vòng lặp       Thời lượng Audio     Thời gian tính     Tốc độ Realtime     Trạng thái Bộ nhớ" << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------" << std::endl;

    double hot_total_time = 0.0;
    double hot_audio_dur = 0.0;

    for (int r = 1; r <= rounds; ++r) {
        double audio_dur = 0.0;
        double compute_sec = 0.0;

        auto wav = engine.synthesize_paragraph(text, config, audio_dur, compute_sec);

        hot_total_time += compute_sec;
        hot_audio_dur = audio_dur;

        std::cout << " [HOT] Vòng " << r << " :     "
                  << std::fixed << std::setprecision(2) << std::setw(8) << audio_dur << " s        "
                  << std::setprecision(3) << std::setw(7) << compute_sec << " s        "
                  << std::setprecision(1) << std::setw(7) << (audio_dur / compute_sec) << "x RT         "
                  << "L3 Cache (Tốc độ tối đa)" << std::endl;
    }
    double hot_avg_time = hot_total_time / rounds;
    double hot_avg_speed = hot_audio_dur / hot_avg_time;

    // --- PHẦN 2: ÉP CPU XÓA TRẮNG CACHE -> PHẢI ĐỌC TỪ RAM DDR5 ---
    std::cout << "\n🧊 [PHẦN 2] ÉP CHẠY TỪ RAM DDR5 (Xóa sạch toàn bộ L1, L2, L3 Cache trước mỗi vòng):" << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << " Vòng lặp       Thời lượng Audio     Thời gian tính     Tốc độ Realtime     Trạng thái Bộ nhớ" << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------" << std::endl;

    double ddr5_total_time = 0.0;

    for (int r = 1; r <= rounds; ++r) {
        // Ép xóa sạch L1, L2, L3 Cache!
        flush_all_cpu_caches_to_ddr5();

        double audio_dur = 0.0;
        double compute_sec = 0.0;

        auto wav = engine.synthesize_paragraph(text, config, audio_dur, compute_sec);

        ddr5_total_time += compute_sec;

        std::cout << " [DDR5] Vòng " << r << ":    "
                  << std::fixed << std::setprecision(2) << std::setw(8) << audio_dur << " s        "
                  << std::setprecision(3) << std::setw(7) << compute_sec << " s        "
                  << std::setprecision(1) << std::setw(7) << (audio_dur / compute_sec) << "x RT         "
                  << "Ép đọc từ DDR5 RAM (Cache Miss)" << std::endl;
    }
    double ddr5_avg_time = ddr5_total_time / rounds;
    double ddr5_avg_speed = hot_audio_dur / ddr5_avg_time;

    // --- BẢNG SO SÁNH TỔNG KẾT ---
    std::cout << "\n=================================================================================================" << std::endl;
    std::cout << "📊 BẢNG SO SÁNH TRỰC DIỆN: L3 CACHE (SRAM) vs BỘ NHỚ RAM DDR5" << std::endl;
    std::cout << "=================================================================================================" << std::endl;
    std::cout << "  • Thời gian xử lý trung bình (L3 Cache)   : " << std::fixed << std::setprecision(3) << hot_avg_time << " giây (" << std::setprecision(1) << hot_avg_speed << "x RT)" << std::endl;
    std::cout << "  • Thời gian khi ép đọc từ RAM DDR5        : " << std::fixed << std::setprecision(3) << ddr5_avg_time << " giây (" << std::setprecision(1) << ddr5_avg_speed << "x RT)" << std::endl;
    
    double diff_ms = (ddr5_avg_time - hot_avg_time) * 1000.0;
    double percent_slow = ((ddr5_avg_time - hot_avg_time) / hot_avg_time) * 100.0;

    std::cout << "  • Chênh lệch thời gian do bus RAM DDR5    : +" << std::setprecision(1) << diff_ms << " ms (" 
              << (percent_slow >= 0 ? "+" : "") << std::setprecision(1) << percent_slow << "%)" << std::endl;
    std::cout << "=================================================================================================" << std::endl;

    return 0;
}
