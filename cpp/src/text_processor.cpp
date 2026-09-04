#include "text_processor.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

TextProcessor::TextProcessor() {
    symbol_replace_map_ = {
        {"/", " xuyệt "},
        {"\\", " xuyệt ngược "},
        {"_", " gạch dưới "},
        {"@", " a còng "},
        {"#", " thăng "},
        {"$", " đô la "},
        {"%", " phần trăm "},
        {"^", " mũ "},
        {"&", " và "},
        {"*", " sao "},
        {"+", " cộng "},
        {"=", " bằng "},
        {"<", " nhỏ hơn "},
        {">", " lớn hơn "},
        {"|", " hoặc "},
        {"~", " khoảng "}
    };
}

bool TextProcessor::load_symbols(const std::string& symbols_json_path) {
    std::ifstream file(symbols_json_path);
    if (!file.is_open()) {
        std::cerr << "❌ [TextProcessor] Không thể mở file symbols: " << symbols_json_path << std::endl;
        return false;
    }

    try {
        json j;
        file >> j;
        if (j.contains("_symbol_to_id")) {
            for (auto& [key, val] : j["_symbol_to_id"].items()) {
                symbol_to_id_[key] = val.get<int64_t>();
            }
        } else if (j.contains("symbols")) {
            int64_t id = 0;
            for (const auto& s : j["symbols"]) {
                symbol_to_id_[s.get<std::string>()] = id++;
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ [TextProcessor] Lỗi phân tích symbols.json: " << e.what() << std::endl;
        return false;
    }
}

// Thay thế chuỗi con trong std::string
static std::string string_replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

std::string TextProcessor::clean_text(const std::string& raw_text) const {
    std::string text = raw_text;

    // 1. Thay thế ký tự đặc biệt
    for (const auto& [sym, spoken] : symbol_replace_map_) {
        text = string_replace_all(text, sym, spoken);
    }

    // 2. Chuyển đổi một số ký tự gạch nối Unicode thành gạch nối ASCII
    text = string_replace_all(text, "–", "-");
    text = string_replace_all(text, "—", "-");
    text = string_replace_all(text, "‘", "'");
    text = string_replace_all(text, "’", "'");
    text = string_replace_all(text, "“", "");
    text = string_replace_all(text, "”", "");
    text = string_replace_all(text, "«", "");
    text = string_replace_all(text, "»", "");

    // 3. Xóa các dấu ngoặc đơn, ngoặc vuông, ngoặc nhọn
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
        if (c == '(' || c == ')' || c == '[' || c == ']' || 
            c == '{' || c == '}' || c == '"' || c == '`') {
            continue;
        }
        cleaned.push_back(c);
    }

    // 4. Chuẩn hóa khoảng trắng kép
    std::regex ws_re("[ \t\r\n]+");
    cleaned = std::regex_replace(cleaned, ws_re, " ");

    // Cắt khoảng trắng đầu và cuối
    size_t first = cleaned.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = cleaned.find_last_not_of(' ');
    return cleaned.substr(first, (last - first + 1));
}

// Hàm phân rã chuỗi UTF-8 thành mảng các ký tự (UTF-8 substrings)
static std::vector<std::string> split_utf8_chars(const std::string& str) {
    std::vector<std::string> chars;
    chars.reserve(str.size());
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        size_t len = 1;
        if ((c & 0x80) == 0x00) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (i + len <= str.size()) {
            chars.push_back(str.substr(i, len));
        }
        i += len;
    }
    return chars;
}

std::vector<int64_t> TextProcessor::text_to_sequence(const std::string& text) const {
    std::vector<int64_t> sequence;
    std::vector<std::string> utf8_chars = split_utf8_chars(text);

    // Chuyển ký tự hoa sang thường (đối với ASCII cơ bản)
    for (auto& s : utf8_chars) {
        if (s.size() == 1) {
            s[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
        }
    }

    sequence.reserve(utf8_chars.size());
    for (const auto& ch : utf8_chars) {
        auto it = symbol_to_id_.find(ch);
        if (it != symbol_to_id_.end()) {
            sequence.push_back(it->second);
        }
    }

    // Áp dụng intersperse(seq, 0)
    std::vector<int64_t> interspersed;
    interspersed.reserve(sequence.size() * 2 + 1);
    interspersed.push_back(0);
    for (int64_t id : sequence) {
        interspersed.push_back(id);
        interspersed.push_back(0);
    }

    return interspersed;
}

std::vector<std::string> TextProcessor::split_sentences(const std::string& text) const {
    std::vector<std::string> raw_sents;
    std::string current;
    current.reserve(256);

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        current.push_back(c);
        if (c == '.' || c == '?' || c == '!' || c == '\n') {
            // Kiểm tra xem ký tự kế tiếp có phải khoảng trắng hoặc kết thúc chuỗi không
            if (i + 1 == text.size() || text[i + 1] == ' ' || text[i + 1] == '\n' || text[i + 1] == '\r') {
                std::string s = clean_text(current);
                if (!s.empty()) {
                    raw_sents.push_back(s);
                }
                current.clear();
            }
        }
    }

    std::string last = clean_text(current);
    if (!last.empty()) {
        raw_sents.push_back(last);
    }

    // Gộp các câu hoàn chỉnh thành các chunk lớn ~20 - 30 giây (khoảng 300 - 450 ký tự)
    // Không để chunk quá ngắn (như vài chữ mở đầu hay câu thoại 1 từ)
    std::vector<std::string> chunks;
    std::string accumulated;
    const size_t TARGET_CHARS = 400; // ~25 - 30 giây âm thanh
    const size_t MIN_CHARS = 250;
    auto ensure_terminal_punct = [](std::string& c) {
        while (!c.empty() && (c.back() == ' ' || c.back() == '\t' || c.back() == '\r' || c.back() == '\n')) {
            c.pop_back();
        }
        if (!c.empty()) {
            char last_char = c.back();
            if (last_char != '.' && last_char != '!' && last_char != '?' && last_char != ':' && last_char != ';') {
                c.push_back('.');
            }
        }
    };

    for (const auto& s : raw_sents) {
        if (accumulated.empty()) {
            accumulated = s;
        } else {
            if (accumulated.size() + 1 + s.size() <= TARGET_CHARS || accumulated.size() < MIN_CHARS) {
                accumulated += " " + s;
            } else {
                ensure_terminal_punct(accumulated);
                chunks.push_back(accumulated);
                accumulated = s;
            }
        }
    }
    if (!accumulated.empty()) {
        ensure_terminal_punct(accumulated);
        chunks.push_back(accumulated);
    }

    return chunks;
}
