#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

class TextProcessor {
public:
    TextProcessor();
    ~TextProcessor() = default;

    // Nạp bảng từ điển symbols.json
    bool load_symbols(const std::string& symbols_json_path);

    // Chuẩn hóa văn bản Tiếng Việt (ký tự đặc biệt, ngoặc, khoảng trắng)
    std::string clean_text(const std::string& text) const;

    // Chuyển văn bản đã chuẩn hóa thành chuỗi token ID kèm intersperse 0
    std::vector<int64_t> text_to_sequence(const std::string& text) const;

    // Tách văn bản dài thành danh sách các câu độc lập
    std::vector<std::string> split_sentences(const std::string& text) const;

    const std::unordered_map<std::string, int64_t>& get_symbol_map() const { return symbol_to_id_; }

private:
    std::unordered_map<std::string, int64_t> symbol_to_id_;
    std::unordered_map<std::string, std::string> symbol_replace_map_;
};
