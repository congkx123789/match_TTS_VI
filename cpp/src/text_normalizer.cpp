#include "text_normalizer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace matcha {

static const std::vector<std::string> UNITS = {
    "không", "một", "hai", "ba", "bốn", "năm", "sáu", "bảy", "tám", "chín"
};

// 1. Đọc số nguyên tiếng Việt chuẩn ngôn ngữ học
std::string TextNormalizer::read_integer(int64_t n) {
    if (n < 0) {
        return "âm " + read_integer(-n);
    }
    if (n < 10) {
        return UNITS[n];
    }
    if (n < 100) {
        int64_t ten = n / 10;
        int64_t unit = n % 10;
        std::string ten_str = (ten == 1) ? "mười" : (UNITS[ten] + " mươi");
        if (unit == 0) return ten_str;
        if (unit == 1) return (ten == 1) ? (ten_str + " một") : (ten_str + " mốt");
        if (unit == 4) return (ten == 1) ? (ten_str + " bốn") : (ten_str + " tư");
        if (unit == 5) return ten_str + " lăm";
        return ten_str + " " + UNITS[unit];
    }
    if (n < 1000) {
        int64_t hundred = n / 100;
        int64_t rem = n % 100;
        std::string hundred_str = UNITS[hundred] + " trăm";
        if (rem == 0) return hundred_str;
        if (rem < 10) return hundred_str + " lẻ " + UNITS[rem];
        return hundred_str + " " + read_integer(rem);
    }
    if (n < 1000000) {
        int64_t thousand = n / 1000;
        int64_t rem = n % 1000;
        std::string thousand_str = read_integer(thousand) + " ngàn";
        if (rem == 0) return thousand_str;
        if (rem < 10) return thousand_str + " không trăm lẻ " + read_integer(rem);
        if (rem < 100) return thousand_str + " không trăm " + read_integer(rem);
        return thousand_str + " " + read_integer(rem);
    }
    if (n < 1000000000) {
        int64_t million = n / 1000000;
        int64_t rem = n % 1000000;
        std::string million_str = read_integer(million) + " triệu";
        if (rem == 0) return million_str;
        if (rem < 10) return million_str + " không trăm lẻ " + read_integer(rem);
        if (rem < 1000) return million_str + " không trăm " + read_integer(rem);
        return million_str + " " + read_integer(rem);
    }
    // Hàng tỷ trở lên
    int64_t billion = n / 1000000000;
    int64_t rem = n % 1000000000;
    std::string billion_str = read_integer(billion) + " tỷ";
    if (rem == 0) return billion_str;
    if (rem < 10) return billion_str + " không trăm lẻ " + read_integer(rem);
    if (rem < 1000) return billion_str + " không trăm " + read_integer(rem);
    return billion_str + " " + read_integer(rem);
}

// 2. Đọc từng chữ số rời rạc
std::string TextNormalizer::read_digits_one_by_one(const std::string& digit_str) {
    std::string res;
    for (char c : digit_str) {
        if (c >= '0' && c <= '9') {
            if (!res.empty()) res += " ";
            res += UNITS[c - '0'];
        }
    }
    return res;
}

static inline bool is_utf8_continuation_or_lead(unsigned char c) {
    return c >= 0x80;
}

static inline bool is_word_char_utf8(const std::string& text, size_t pos) {
    if (pos >= text.size()) return false;
    unsigned char c = static_cast<unsigned char>(text[pos]);
    if (c >= 0x80) return true; // Thuộc ký tự đa byte UTF-8
    return std::isalnum(c) || c == '_';
}

// 3. Quy đổi chữ cái Latin đơn độc lập (V, A, a, d, g...) bảo toàn 100% tiếng Việt UTF-8
std::string TextNormalizer::replace_isolated_latin_letters(const std::string& text) {
    static const std::unordered_map<char, std::string> LATIN_MAP = {
        {'A', "a"}, {'B', "bê"}, {'C', "xê"}, {'D', "dê"}, {'E', "ee"},
        {'F', "ép"}, {'G', "gờ"}, {'H', "hắt"}, {'I', "i"}, {'J', "gi"},
        {'K', "ca"}, {'L', "lờ"}, {'M', "mờ"}, {'N', "nờ"}, {'O', "o"},
        {'P', "pê"}, {'Q', "quy"}, {'R', "rờ"}, {'S', "ét"}, {'T', "tê"},
        {'U', "uu"}, {'V', "vê"}, {'W', "vê kép"}, {'X', "ích"}, {'Y', "y"}, {'Z', "zét"}
    };

    std::string result;
    result.reserve(text.size() * 2);

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        unsigned char uc = static_cast<unsigned char>(c);

        if (std::isalpha(uc) && uc < 0x80) {
            bool prev_is_word = (i > 0) && is_word_char_utf8(text, i - 1);
            bool next_is_word = (i + 1 < text.size()) && is_word_char_utf8(text, i + 1);

            if (!prev_is_word && !next_is_word) {
                // Đây là chữ cái Latin đứng hoàn toàn độc lập!
                char upper_c = static_cast<char>(std::toupper(uc));
                auto it = LATIN_MAP.find(upper_c);
                if (it != LATIN_MAP.end()) {
                    result += it->second;
                    continue;
                }
            }
        }
        result += c;
    }

    return result;
}

// 4. Quy đổi đơn vị đo lường, viết tắt công nghệ & ký hiệu nước ngoài
std::string TextNormalizer::translate_foreign_symbols_and_words(const std::string& input_text) {
    std::string text = input_text;

    // A. Bảng đơn vị đo lường & tiền tệ nước ngoài
    static const std::vector<std::pair<std::regex, std::string>> UNITS_MAP = {
        { std::regex(R"((\d|\s|^)km/h\b)", std::regex_constants::icase), "$1 ki lô mét trên giờ" },
        { std::regex(R"((\d|\s|^)km/giờ\b)", std::regex_constants::icase), "$1 ki lô mét trên giờ" },
        { std::regex(R"((\d|\s|^)m/s\b)", std::regex_constants::icase), "$1 mét trên giây" },
        { std::regex(R"((\d|\s|^)MB/s\b)", std::regex_constants::icase), "$1 mê ga bai trên giây" },
        { std::regex(R"((\d|\s|^)GB/s\b)", std::regex_constants::icase), "$1 ghi ga bai trên giây" },
        { std::regex(R"((\d|\s|^)KB/s\b)", std::regex_constants::icase), "$1 ki lô bai trên giây" },
        { std::regex(R"((\d|\s|^)km\b)", std::regex_constants::icase), "$1 ki lô mét" },
        { std::regex(R"((\d|\s|^)cm\b)", std::regex_constants::icase), "$1 xen ti mét" },
        { std::regex(R"((\d|\s|^)mm\b)", std::regex_constants::icase), "$1 mi li mét" },
        { std::regex(R"((\d|\s|^)kg\b)", std::regex_constants::icase), "$1 ki lô gam" },
        { std::regex(R"((\d|\s|^)ml\b)", std::regex_constants::icase), "$1 mi li lít" },
        { std::regex(R"((\d|\s|^)kWh\b)", std::regex_constants::icase), "$1 ki lô oát giờ" },
        { std::regex(R"((\d|\s|^)kW\b)", std::regex_constants::icase), "$1 ki lô oát" },
        { std::regex(R"((\d|\s|^)mW\b)", std::regex_constants::icase), "$1 mê ga oát" },
        { std::regex(R"((\d|\s|^)GHz\b)", std::regex_constants::icase), "$1 ghi ga hét" },
        { std::regex(R"((\d|\s|^)MHz\b)", std::regex_constants::icase), "$1 mê ga hét" },
        { std::regex(R"((\d|\s|^)kHz\b)", std::regex_constants::icase), "$1 ki lô hét" },
        { std::regex(R"((\d|\s|^)Hz\b)", std::regex_constants::icase), "$1 hét" },
        { std::regex(R"((\d|\s|^)TB\b)", std::regex_constants::icase), "$1 tê ra bai" },
        { std::regex(R"((\d|\s|^)GB\b)", std::regex_constants::icase), "$1 ghi ga bai" },
        { std::regex(R"((\d|\s|^)MB\b)", std::regex_constants::icase), "$1 mê ga bai" },
        { std::regex(R"((\d|\s|^)KB\b)", std::regex_constants::icase), "$1 ki lô bai" },
        { std::regex(R"((\d|\s|^)USD\b)", std::regex_constants::icase), "$1 đô la" },
        { std::regex(R"((\d|\s|^)EUR\b)", std::regex_constants::icase), "$1 ơ rô" },
        { std::regex(R"((\d|\s|^)GBP\b)", std::regex_constants::icase), "$1 bảng anh" },
        { std::regex(R"((\d|\s|^)JPY\b)", std::regex_constants::icase), "$1 yên Nhật" },
        { std::regex(R"((\d|\s|^)VND\b)", std::regex_constants::icase), "$1 đồng" },
        { std::regex(R"((\d|\s|^)VNĐ\b)", std::regex_constants::icase), "$1 đồng" }
    };

    for (const auto& kv : UNITS_MAP) {
        text = std::regex_replace(text, kv.first, kv.second);
    }

    // B. Bảng viết tắt công nghệ & thuật ngữ thông dụng
    static const std::vector<std::pair<std::regex, std::string>> ACRONYMS_MAP = {
        { std::regex(R"(\bAI\b)"), "a i" },
        { std::regex(R"(\bAPI\b)"), "a p i" },
        { std::regex(R"(\bIP\b)"), "i p" },
        { std::regex(R"(\bCPU\b)"), "c p u" },
        { std::regex(R"(\bRAM\b)"), "ram" },
        { std::regex(R"(\bROM\b)"), "rom" },
        { std::regex(R"(\bUSB\b)"), "u s b" },
        { std::regex(R"(\bSMS\b)"), "s m s" },
        { std::regex(R"(\bOTP\b)"), "o t p" },
        { std::regex(R"(\bVIP\b)"), "v i p" },
        { std::regex(R"(\bCEO\b)"), "c e o" },
        { std::regex(R"(\bCFO\b)"), "c f o" },
        { std::regex(R"(\bPR\b)"), "p r" },
        { std::regex(R"(\bOK\b)"), "ô kê" },
        { std::regex(R"(\bID\b)"), "i d" },
        { std::regex(R"(\bURL\b)"), "u r l" },
        { std::regex(R"(\bHTML\b)"), "h t m l" },
        { std::regex(R"(\bCSS\b)"), "c s s" },
        { std::regex(R"(\bJS\b)"), "j s" },
        { std::regex(R"(\bPDF\b)"), "p d f" },
        { std::regex(R"(\bMP3\b)"), "m p 3" },
        { std::regex(R"(\bMP4\b)"), "m p 4" },
        { std::regex(R"(\bWiFi\b)", std::regex_constants::icase), "oai phai" },
        { std::regex(R"(\bBluetooth\b)", std::regex_constants::icase), "blum tút" },
        { std::regex(R"(\bApp\b)", std::regex_constants::icase), "áp" },
        { std::regex(R"(\bWebsite\b)", std::regex_constants::icase), "web site" },
        { std::regex(R"(\bFacebook\b)", std::regex_constants::icase), "phây bút" },
        { std::regex(R"(\bYoutube\b)", std::regex_constants::icase), "yêu tuýp" },
        { std::regex(R"(\bGoogle\b)", std::regex_constants::icase), "gồ gồ" }
    };

    for (const auto& kv : ACRONYMS_MAP) {
        text = std::regex_replace(text, kv.first, kv.second);
    }

    // C. Quy đổi chữ cái Latin đơn độc lập
    text = replace_isolated_latin_letters(text);

    // D. Ký tự đặc biệt toán học
    static const std::vector<std::pair<std::string, std::string>> SYMBOL_MAP = {
        { "@", " a còng " }, { "#", " thăng " }, { "$", " đô la " },
        { "€", " ơ rô " },   { "£", " bảng anh " }, { "¥", " yên " }, { "₩", " won " },
        { "%", " phần trăm " }, { "^", " mũ " },   { "&", " và " },
        { "*", " sao " },    { "+", " cộng " },    { "=", " bằng " },
        { "<", " nhỏ hơn " }, { ">", " lớn hơn " }, { "|", " hoặc " },
        { "~", " khoảng " },  { "/", " xuyệt " },   { "\\", " xuyệt ngược " },
        { "_", " gạch dưới " }
    };

    for (const auto& kv : SYMBOL_MAP) {
        size_t pos = 0;
        while ((pos = text.find(kv.first, pos)) != std::string::npos) {
            text.replace(pos, kv.first.length(), kv.second);
            pos += kv.second.length();
        }
    }

    return text;
}

// 5. Chuẩn hóa tiếng Việt toàn diện (Số, ngày, giờ, tiền, phân số, sđt)
std::string TextNormalizer::normalize_vietnamese_text(const std::string& input_text) {
    if (input_text.empty()) return "";
    std::string text = input_text;

    // A. Ngày / Tháng / Năm: 15/08/2026 hoặc 15-8-2026
    {
        std::regex re(R"(\b(\d{1,2})[/.-](\d{1,2})[/.-](\d{4})\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            int day = std::stoi((*it)[1].str());
            int month = std::stoi((*it)[2].str());
            int year = std::stoi((*it)[3].str());
            res += read_integer(day) + " tháng " + read_integer(month) + " năm " + read_integer(year);
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // B. Giờ giấc: 15h30, 8h00, 8:30
    {
        std::regex re(R"(\b(\d{1,2})h(\d{1,2})?\b)", std::regex_constants::icase);
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            int hour = std::stoi((*it)[1].str());
            int min = (*it)[2].matched ? std::stoi((*it)[2].str()) : 0;
            res += read_integer(hour) + " giờ" + (min > 0 ? (" " + read_integer(min) + " phút") : "");
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }
    {
        std::regex re(R"(\b(\d{1,2}):(\d{2})\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            int hour = std::stoi((*it)[1].str());
            int min = std::stoi((*it)[2].str());
            res += read_integer(hour) + " giờ " + read_integer(min) + " phút";
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // C. Số điện thoại (Bắt đầu bằng 0 hoặc +84, 9-11 số có định dạng)
    {
        std::regex re(R"((?:\+84|0)[0-9]{2,3}[.\s-]?[0-9]{3}[.\s-]?[0-9]{3,4}\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            res += read_digits_one_by_one(it->str());
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // D. CCCD / CMND / STK (10 - 14 chữ số liền nhau)
    {
        std::regex re(R"(\b\d{10,14}\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            res += read_digits_one_by_one(it->str());
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // E. Biển số xe: 30A-12345
    {
        std::regex re(R"(\b(\d{1,3})([A-Za-z]{1,2})[-.\s]?(\d{4,5})\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            int p = std::stoi((*it)[1].str());
            std::string let = (*it)[2].str();
            std::string code = (*it)[3].str();
            res += read_integer(p) + " " + let + " " + read_digits_one_by_one(code);
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // F. Mã chữ-số: OTP-4567, HD999
    {
        std::regex re(R"(\b([A-Za-z]{1,3})[-.\s]?(\d{3,6})\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            std::string prefix = (*it)[1].str();
            std::string digits = (*it)[2].str();
            res += prefix + " " + read_digits_one_by_one(digits);
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // G. Phân số: 3/4, 1/2
    {
        std::regex re(R"(\b(\d+)/(\d+)\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            int64_t num = std::stoll((*it)[1].str());
            int64_t den = std::stoll((*it)[2].str());
            std::string den_str = (den == 4) ? "tư" : read_integer(den);
            res += read_integer(num) + " phần " + den_str;
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // H. Nhiệt độ: 36.5°C hoặc 36.5 độ C
    {
        static const std::string deg_c_utf8 = "°C";
        size_t pos = 0;
        while ((pos = text.find(deg_c_utf8, pos)) != std::string::npos) {
            text.replace(pos, deg_c_utf8.length(), " độ C");
            pos += 5;
        }
    }

    // I. Tiền tệ viết tắt: 100k, 100tr, 100.000đ, $50
    {
        text = std::regex_replace(text, std::regex(R"((\d+)\s*k\b)", std::regex_constants::icase), "$1 ngàn");
        text = std::regex_replace(text, std::regex(R"((\d+)\s*tr\b)", std::regex_constants::icase), "$1 triệu");
        text = std::regex_replace(text, std::regex(R"((\d+)\s*đ\b)", std::regex_constants::icase), "$1 đồng");
        text = std::regex_replace(text, std::regex(R"(\$\s*(\d+))"), "$1 đô la");
        text = std::regex_replace(text, std::regex(R"((\d+)\s*\$)"), "$1 đô la");
    }

    // J. Số thập phân: 1.5 hoặc 1,5
    {
        std::regex re(R"(\b(\d+)[.,](\d{1,4})\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            int64_t whole = std::stoll((*it)[1].str());
            std::string dec = (*it)[2].str();
            res += read_integer(whole) + " phẩy " + read_digits_one_by_one(dec);
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // K. Quy đổi ký hiệu & thuật ngữ nước ngoài trước (để 16GB -> 16 ghi ga bai)
    text = translate_foreign_symbols_and_words(text);

    // L. Số nguyên thông thường: 10 -> mười, 14 -> mười bốn, 24 -> hai mươi tư, 1005 -> một ngàn không trăm lẻ năm
    {
        std::regex re(R"(\b\d+\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            try {
                int64_t val = std::stoll(it->str());
                res += read_integer(val);
            } catch (...) {
                res += it->str();
            }
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
    }

    // M. Chuẩn hóa khoảng trắng và dấu câu
    text = std::regex_replace(text, std::regex(R"(\s+([.,?!:;]))"), "$1");
    text = std::regex_replace(text, std::regex(R"(([.,?!:;])(?=[^\s]))"), "$1 ");
    text = std::regex_replace(text, std::regex(R"([ \t]+)"), " ");

    return text;
}

// 6. Làm sạch văn bản hoàn chỉnh trước khi chuyển thành Symbol IDs
std::string TextNormalizer::clean_and_normalize(
    const std::string& raw_text,
    const std::unordered_set<std::string>& allowed_symbols
) {
    if (raw_text.empty()) return "";

    // 1. Chạy qua bộ chuẩn hóa số và từ nước ngoài
    std::string text = normalize_vietnamese_text(raw_text);

    // 2. Loại bỏ các dấu mở ngoặc / đóng ngoặc / ngoặc kép không cần thiết (Bảo toàn 100% tiếng Việt UTF-8)
    static const std::vector<std::string> MULTIBYTE_BRACKETS = {
        "«", "»", "“", "”", "„", "”"
    };
    for (const auto& b : MULTIBYTE_BRACKETS) {
        size_t pos = 0;
        while ((pos = text.find(b, pos)) != std::string::npos) {
            text.erase(pos, b.length());
        }
    }
    // ASCII brackets: " ' ` ( ) [ ] { }
    static const std::regex ascii_bracket_re(R"([\"\'\(\)\[\]\{\}`])");
    text = std::regex_replace(text, ascii_bracket_re, "");

    // 3. Chuẩn hóa khoảng trắng
    text = std::regex_replace(text, std::regex(R"([ \t]+)"), " ");

    // 4. Nếu có allowed_symbols, lọc các ký tự nằm ngoài bộ symbols
    if (!allowed_symbols.empty()) {
        std::string filtered;
        for (size_t i = 0; i < text.size(); ) {
            unsigned char c = text[i];
            size_t len = 1;
            if ((c & 0x80) == 0) len = 1;
            else if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;

            if (i + len <= text.size()) {
                std::string ch = text.substr(i, len);
                if (ch == "\n" || ch == " " || allowed_symbols.count(ch) > 0) {
                    filtered += ch;
                } else {
                    // Thử chuyển sang chữ thường nếu là ký tự Latin
                    char lower_ch = static_cast<char>(std::tolower(ch[0]));
                    std::string lower_str(1, lower_ch);
                    if (allowed_symbols.count(lower_str) > 0) {
                        filtered += lower_str;
                    }
                }
            }
            i += len;
        }
        text = filtered;
    }

    text = std::regex_replace(text, std::regex(R"([ \t]+)"), " ");
    return text;
}

// 7. Nạp cấu hình ngắt nghỉ từ JSON
PauseConfig TextNormalizer::load_pause_config(const std::string& filepath) {
    PauseConfig cfg;
    std::ifstream f(filepath);
    if (!f.is_open()) {
        return cfg; // Dùng cấu hình mặc định
    }

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    auto extract_float = [&](const std::string& key, float default_val) -> float {
        std::regex re(R"(")" + key + R"(":\s*([0-9.]+))");
        std::smatch match;
        if (std::regex_search(content, match, re)) {
            try {
                return std::stof(match[1].str());
            } catch (...) {}
        }
        return default_val;
    };

    cfg.para = extract_float("para", cfg.para);
    cfg.ellipsis = extract_float("ellipsis", cfg.ellipsis);
    cfg.period = extract_float("period", cfg.period);
    cfg.comma = extract_float("comma", cfg.comma);
    cfg.question = extract_float("question", cfg.question);
    cfg.exclaim = extract_float("exclaim", cfg.exclaim);
    cfg.colon = extract_float("colon", cfg.colon);
    cfg.semicolon = extract_float("semicolon", cfg.semicolon);

    return cfg;
}

// 8. Tính toán khoảng nghỉ theo dấu câu kết thúc
float TextNormalizer::get_pause_duration(const std::string& sentence, const PauseConfig& cfg) {
    if (sentence.empty()) return cfg.period;

    int idx = static_cast<int>(sentence.size()) - 1;
    while (idx >= 0 && (sentence[idx] == ' ' || sentence[idx] == '\t' || sentence[idx] == '\r' || sentence[idx] == '\n')) {
        idx--;
    }
    if (idx < 0) return cfg.period;

    // Dấu ba chấm "..."
    if (idx >= 2 && sentence[idx] == '.' && sentence[idx - 1] == '.' && sentence[idx - 2] == '.') {
        return cfg.ellipsis;
    }

    char last_char = sentence[idx];
    switch (last_char) {
        case '.': return cfg.period;
        case ',': return cfg.comma;
        case '?': return cfg.question;
        case '!': return cfg.exclaim;
        case ':': return cfg.colon;
        case ';': return cfg.semicolon;
        default:  return cfg.default_pause;
    }
}

} // namespace matcha
