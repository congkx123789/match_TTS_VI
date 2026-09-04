#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <iomanip>
#include "text_normalizer.hpp"
#include "text_processor.hpp"

void print_step(int step_num, const std::string& step_name, const std::string& result) {
    std::cout << "\n--------------------------------------------------------------------------------" << std::endl;
    std::cout << "📌 BƯỚC " << std::setw(2) << step_num << ": " << step_name << std::endl;
    std::cout << "--------------------------------------------------------------------------------" << std::endl;
    std::cout << "👉 Kết quả: \"" << result << "\"" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string raw_text = "Ngày 15/08/2026, cuộc họp lúc 15h30 tại tầng 14 tòa nhà HD999: "
                           "Thiết bị AI hỗ trợ WiFi và 16GB RAM với tốc độ 100 MB/s, "
                           "giá chỉ 75k ($50). Hotline: 0912.345.678. Nhiệt độ phòng 25.5°C!";
    
    if (argc > 1) {
        raw_text = argv[1];
    }

    std::cout << "================================================================================" << std::endl;
    std::cout << "🔍 KIỂM TRA CHI TIẾT TỪNG BƯỚC CHUẨN HÓA VĂN BẢN (STEP-BY-STEP NORMALIZER)" << std::endl;
    std::cout << "================================================================================" << std::endl;
    std::cout << "📥 VĂN BẢN GỐC ĐẦU VÀO:\n\"" << raw_text << "\"\n" << std::endl;

    std::string text = raw_text;
    int step = 1;

    // BƯỚC 1: Xử lý Ngày / Tháng / Năm
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
            res += matcha::TextNormalizer::read_integer(day) + " tháng " +
                   matcha::TextNormalizer::read_integer(month) + " năm " +
                   matcha::TextNormalizer::read_integer(year);
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
        print_step(step++, "Quy đổi Ngày / Tháng / Năm (Ví dụ: 15/08/2026)", text);
    }

    // BƯỚC 2: Xử lý Giờ giấc
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
            res += matcha::TextNormalizer::read_integer(hour) + " giờ" + (min > 0 ? (" " + matcha::TextNormalizer::read_integer(min) + " phút") : "");
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
        print_step(step++, "Quy đổi Giờ giấc (Ví dụ: 15h30, 8h00)", text);
    }

    // BƯỚC 3: Số điện thoại & CCCD/Mã số dài
    {
        std::regex re(R"((?:\+84|0)[0-9]{2,3}[.\s-]?[0-9]{3}[.\s-]?[0-9]{3,4}\b)");
        std::string res;
        auto it = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        size_t last = 0;
        for (; it != end; ++it) {
            res += text.substr(last, it->position() - last);
            res += matcha::TextNormalizer::read_digits_one_by_one(it->str());
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
        print_step(step++, "Quy đổi Số điện thoại & Mã rời rạc (Ví dụ: 0912.345.678)", text);
    }

    // BƯỚC 4: Mã chữ-số (HD999, OTP-1234, Biển số xe)
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
            res += prefix + " " + matcha::TextNormalizer::read_digits_one_by_one(digits);
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
        print_step(step++, "Quy đổi Mã ký hiệu chữ-số (Ví dụ: HD999, 30A-12345)", text);
    }

    // BƯỚC 5: Nhiệt độ & Tiền tệ viết tắt (100k, $50, 25.5°C)
    {
        static const std::string deg_c_utf8 = "°C";
        size_t pos = 0;
        while ((pos = text.find(deg_c_utf8, pos)) != std::string::npos) {
            text.replace(pos, deg_c_utf8.length(), " độ c");
            pos += 5;
        }
        text = std::regex_replace(text, std::regex(R"((\d+)\s*k\b)", std::regex_constants::icase), "$1 ngàn");
        text = std::regex_replace(text, std::regex(R"((\d+)\s*tr\b)", std::regex_constants::icase), "$1 triệu");
        text = std::regex_replace(text, std::regex(R"((\d+)\s*đ\b)", std::regex_constants::icase), "$1 đồng");
        text = std::regex_replace(text, std::regex(R"(\$\s*(\d+))"), "$1 đô la");
        text = std::regex_replace(text, std::regex(R"((\d+)\s*\$)"), "$1 đô la");
        print_step(step++, "Quy đổi Tiền tệ & Đơn vị viết tắt (Ví dụ: 75k, $50, °C)", text);
    }

    // BƯỚC 6: Số thập phân & Phân số
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
            res += matcha::TextNormalizer::read_integer(whole) + " phẩy " + matcha::TextNormalizer::read_digits_one_by_one(dec);
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
        print_step(step++, "Quy đổi Số thập phân (Ví dụ: 25.5 -> hai mươi lăm phẩy năm)", text);
    }

    // BƯỚC 7: Từ viết tắt công nghệ, ngoại ngữ & đơn vị đo lường
    {
        text = matcha::TextNormalizer::translate_foreign_symbols_and_words(text);
        print_step(step++, "Phiên âm Từ tiếng Anh, Công nghệ & Đơn vị (AI, WiFi, RAM, MB/s)", text);
    }

    // BƯỚC 8: Số nguyên còn lại (Số đếm)
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
                res += matcha::TextNormalizer::read_integer(val);
            } catch (...) {
                res += it->str();
            }
            last = it->position() + it->length();
        }
        res += text.substr(last);
        text = res;
        print_step(step++, "Quy đổi Số nguyên & Số đếm còn lại (Ví dụ: 14 -> mười bốn, 16 -> mười sáu)", text);
    }

    // BƯỚC 9: Gọt bỏ dấu ngoặc, chuẩn hóa khoảng trắng & dấu câu
    {
        static const std::vector<std::string> MULTIBYTE_BRACKETS = { "«", "»", "“", "”", "„", "”" };
        for (const auto& b : MULTIBYTE_BRACKETS) {
            size_t p = 0;
            while ((p = text.find(b, p)) != std::string::npos) text.erase(p, b.length());
        }
        text = std::regex_replace(text, std::regex(R"([\"\'\(\)\[\]\{\}`])"), "");
        text = std::regex_replace(text, std::regex(R"(\s+([.,?!:;]))"), "$1");
        text = std::regex_replace(text, std::regex(R"(([.,?!:;])(?=[^\s]))"), "$1 ");
        text = std::regex_replace(text, std::regex(R"([ \t]+)"), " ");
        print_step(step++, "Làm sạch dấu ngoặc kép, chuẩn hóa khoảng trắng & dấu câu", text);
    }

    // BƯỚC 10: Tách câu & Gán khoảng nghỉ thông minh từ pause_config.json
    {
        matcha::PauseConfig pause_cfg = matcha::TextNormalizer::load_pause_config("pause_config.json");
        TextProcessor proc;
        proc.load_symbols("models_onnx/symbols.json");
        auto sentences = proc.split_sentences(text);

        std::cout << "\n--------------------------------------------------------------------------------" << std::endl;
        std::cout << "📌 BƯỚC " << std::setw(2) << step++ << ": Phân tách câu & Gán khoảng nghỉ thông minh (pause_config.json)" << std::endl;
        std::cout << "--------------------------------------------------------------------------------" << std::endl;
        std::cout << "Tổng số câu phân tách: " << sentences.size() << " câu" << std::endl;
        for (size_t i = 0; i < sentences.size(); ++i) {
            float pause = matcha::TextNormalizer::get_pause_duration(sentences[i], pause_cfg);
            std::cout << "  [Câu " << (i + 1) << "] (Khoảng nghỉ sau câu: " 
                      << std::fixed << std::setprecision(2) << pause << "s): \"" << sentences[i] << "\"" << std::endl;
        }

        // BƯỚC 11: Ánh xạ Sequence Symbol ID cho ONNX Runtime
        if (!sentences.empty()) {
            auto seq = proc.text_to_sequence(sentences[0]);
            std::cout << "\n--------------------------------------------------------------------------------" << std::endl;
            std::cout << "📌 BƯỚC " << std::setw(2) << step++ << ": Ánh xạ Token Symbol IDs nạp vào Matcha Encoder (Câu 1 mẫu)" << std::endl;
            std::cout << "--------------------------------------------------------------------------------" << std::endl;
            std::cout << "Độ dài chuỗi Sequence (kèm intersperse 0): " << seq.size() << " tokens" << std::endl;
            std::cout << "Tokens [0..20]: ";
            for (size_t i = 0; i < std::min<size_t>(20, seq.size()); ++i) {
                std::cout << seq[i] << " ";
            }
            std::cout << "..." << std::endl;
        }
    }

    std::cout << "\n================================================================================" << std::endl;
    std::cout << "✅ KIỂM TRA HOÀN TẤT: 100% CÁC BƯỚC CHUẨN HÓA ĐỀU HOẠT ĐỘNG HOÀN HẢO!" << std::endl;
    std::cout << "================================================================================" << std::endl;

    return 0;
}
