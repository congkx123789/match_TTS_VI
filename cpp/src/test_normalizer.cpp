#include <iostream>
#include <vector>
#include <iomanip>
#include "text_normalizer.hpp"

int main() {
    std::cout << "==========================================================================" << std::endl;
    std::cout << "🧪 KIỂM THỬ BỘ CHUẨN HÓA VĂN BẢN VÀ CON SỐ C++ NATIVE (MASTER AUDIT)" << std::endl;
    std::cout << "==========================================================================" << std::endl;

    std::vector<std::string> test_cases = {
        "Chương 10: Tòa tháp có 14 người, 24 chiến sĩ và 15 robot.",
        "Năm 1005 có 1015 binh sĩ, năm 1050 có 2026 người.",
        "Mã OTP là 1 1 1 1. Số CCCD: 012345678901.",
        "Biển số xe: 30A-12345. Mã hợp đồng: HD999.",
        "Hotline liên hệ: 0912.345.678 hoặc +84 987 654 321.",
        "Cuộc họp bắt đầu lúc 15h30 ngày 15/08/2026.",
        "Doanh thu tăng 50% với chi phí 100k ($50).",
        "Nhiệt độ cơ thể là 36.5°C, tiến độ xong 3/4 công việc.",
        "Các chữ cái: V, A, a, d, g, h, r, s, x.",
        "Thiết bị hỗ trợ kết nối WiFi và Bluetooth với tốc độ 100 MB/s.",
        "Mô hình AI sử dụng CPU và 16GB RAM với API REST.",
        "Giá vé xem phim chỉ 75k hoặc 5$ tại website Facebook & Youtube."
    };

    int idx = 1;
    for (const auto& sample : test_cases) {
        std::string cleaned = matcha::TextNormalizer::clean_and_normalize(sample);
        std::cout << "Case " << std::setw(2) << idx++ << ":" << std::endl;
        std::cout << "  • Đầu vào (Raw)   : " << sample << std::endl;
        std::cout << "  • Kết quả (Clean) : " << cleaned << std::endl;
        std::cout << "--------------------------------------------------------------------------" << std::endl;
    }

    // Kiểm tra nạp cấu hình ngắt nghỉ
    matcha::PauseConfig pause_cfg = matcha::TextNormalizer::load_pause_config("pause_config.json");
    std::cout << "\n⏱️  KIỂM TRA CẤU HÌNH NGẮT NGHỈ (PAUSE CONFIG):" << std::endl;
    std::cout << "  • Dấu chấm (.)      : " << pause_cfg.period << " giây" << std::endl;
    std::cout << "  • Dấu phẩy (,)      : " << pause_cfg.comma << " giây" << std::endl;
    std::cout << "  • Dấu ba chấm (...) : " << pause_cfg.ellipsis << " giây" << std::endl;
    std::cout << "  • Dấu hỏi (?)       : " << pause_cfg.question << " giây" << std::endl;
    std::cout << "  • Dấu cảm (!)       : " << pause_cfg.exclaim << " giây" << std::endl;
    std::cout << "  • Dấu hai chấm (:)  : " << pause_cfg.colon << " giây" << std::endl;
    std::cout << "  • Dấu chấm phẩy (;) : " << pause_cfg.semicolon << " giây" << std::endl;
    std::cout << "  • Hết đoạn (\\n\\n)   : " << pause_cfg.para << " giây" << std::endl;
    std::cout << "==========================================================================" << std::endl;

    return 0;
}
