#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace matcha {

struct PauseConfig {
    float para = 0.65f;
    float ellipsis = 0.35f;
    float period = 0.25f;
    float comma = 0.12f;
    float question = 0.30f;
    float exclaim = 0.25f;
    float colon = 0.18f;
    float semicolon = 0.20f;
    float default_pause = 0.20f;
};

class TextNormalizer {
public:
    // 1. Đọc số nguyên Tiếng Việt chuẩn ngôn ngữ học (hàng đơn vị, chục, trăm, ngàn, triệu, tỷ)
    static std::string read_integer(int64_t n);

    // 2. Đọc từng chữ số rời rạc (Mã OTP, CCCD, số điện thoại, STK ngân hàng)
    static std::string read_digits_one_by_one(const std::string& digit_str);

    // 3. Quy đổi các ký hiệu, đơn vị đo lường, từ viết tắt công nghệ nước ngoài sang âm Tiếng Việt
    static std::string translate_foreign_symbols_and_words(const std::string& text);

    // 4. Quy đổi chữ cái Latin đơn độc lập (V, A, a, d, g...) thành âm chữ thường
    static std::string replace_isolated_latin_letters(const std::string& text);

    // 5. Bộ chuẩn hóa tổng hợp toàn diện (Số, ngày tháng, giờ, điện thoại, phần trăm, tiền tệ, thập phân)
    static std::string normalize_vietnamese_text(const std::string& text);

    // 6. Làm sạch văn bản hoàn chỉnh trước khi nạp vào TTS Engine (Bao gồm chuẩn hóa + lọc ký hiệu ngoài từ điển)
    static std::string clean_and_normalize(
        const std::string& raw_text,
        const std::unordered_set<std::string>& allowed_symbols = {}
    );

    // 7. Nạp cấu hình ngắt nghỉ thông minh từ pause_config.json
    static PauseConfig load_pause_config(const std::string& filepath = "pause_config.json");

    // 8. Tính toán độ dài khoảng nghỉ (giây) dựa trên dấu câu kết thúc của câu văn
    static float get_pause_duration(const std::string& sentence, const PauseConfig& config);
};

} // namespace matcha
