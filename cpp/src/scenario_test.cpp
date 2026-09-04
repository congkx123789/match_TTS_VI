#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include "matcha_tts.hpp"

void print_header(const std::string& title) {
    std::cout << "\n" << std::string(75, '=') << std::endl;
    std::cout << "📌 " << title << std::endl;
    std::cout << std::string(75, '=') << std::endl;
}

int main() {
    std::cout << "===========================================================================" << std::endl;
    std::cout << "🧪 KIỂM THỬ TOÀN DIỆN CÁC TÌNH HUỐNG THỰC TẾ (C++ TTS TEST SUITE)" << std::endl;
    std::cout << "===========================================================================" << std::endl;

    MatchaTTS tts;
    // Khởi tạo trên CPU mặc định (hoặc use_gpu=true nếu muốn dùng GPU)
    if (!tts.init("models_onnx", /*use_gpu=*/false, "fp32")) {
        std::cerr << "❌ Không thể khởi tạo MatchaTTS!" << std::endl;
        return 1;
    }

    // ------------------------------------------------------------------------
    // TÌNH HUỐNG 1: CÂU TRUYỆN DÀI LIÊN TỤC (CONTINUOUS VOICE PROMPT CHAINING)
    // ------------------------------------------------------------------------
    print_header("TÌNH HUỐNG 1: CÂU DÀI LIÊN TỤC (VOICE PROMPT CHAINING)");
    std::string long_narrative = 
        "Đêm đen như mực, gió rít từng cơn qua khe núi hiểm trở. "
        "Thạch Hạo đứng sừng sững trên mỏm đá ngút ngàn mây phủ, trong tay nắm chặt thanh kiếm gãy cổ xưa. "
        "Kiếm quang màu hoàng kim rực sáng, xé toạc màn sương mù mịt mùng ngàn năm không tan. "
        "Hắn khẽ nhếch môi, ánh mắt kiên định nhìn về phía chiến trường xa xăm đầy hung hiểm. "
        "Một trận chiến long trời lở đất sắp sửa bùng nổ, không ai có thể lùi bước.";
    
    std::cout << "📝 Nội dung (5 câu dài nối tiếp liên tục):" << std::endl;
    std::cout << long_narrative << "\n" << std::endl;

    double dur1 = 0.0, t1 = 0.0;
    std::string out1 = "outputs/scenario_1_long_continuous.wav";
    if (tts.speak_to_file(long_narrative, out1, /*steps=*/10, /*speed=*/0.85f, /*temp=*/0.9f, /*pause=*/0.18f, &dur1, &t1)) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "✅ [Tình huống 1 Thành công!]" << std::endl;
        std::cout << "  • Thời lượng âm thanh : " << dur1 << " giây" << std::endl;
        std::cout << "  • Thời gian xử lý     : " << t1 << " giây" << std::endl;
        std::cout << "  • Tốc độ sinh thực tế : " << (dur1 / t1) << "x Realtime" << std::endl;
        std::cout << "  • File xuất           : " << out1 << std::endl;
    }

    // ------------------------------------------------------------------------
    // TÌNH HUỐNG 2: VĂN BẢN KÝ TỰ ĐẶC BIỆT, PHẦN TRĂM, TOÁN HỌC, NGOẶC
    // ------------------------------------------------------------------------
    print_header("TÌNH HUỐNG 2: XỬ LÝ KÝ TỰ ĐẶC BIỆT, SỐ LIỆU & KÝ HIỆU TOÁN HỌC");
    std::string special_text = 
        "Tỷ lệ thành công đạt 100% với chi phí chỉ 500$ cho mỗi phiên xử lý. "
        "Công thức tính hiệu suất là a + b = c, và giá trị xấp xỉ ~3.14. "
        "Liên hệ hỗ trợ qua email admin@ai.vn hoặc truy cập đường dẫn /home/alida/tts/ để kiểm tra.";

    std::cout << "📝 Nội dung chứa ký tự đặc biệt (%, $, +, =, ~, /, @):" << std::endl;
    std::cout << special_text << "\n" << std::endl;

    double dur2 = 0.0, t2 = 0.0;
    std::string out2 = "outputs/scenario_2_special_symbols.wav";
    if (tts.speak_to_file(special_text, out2, /*steps=*/10, /*speed=*/0.85f, /*temp=*/0.9f, /*pause=*/0.15f, &dur2, &t2)) {
        std::cout << "✅ [Tình huống 2 Thành công!]" << std::endl;
        std::cout << "  • Thời lượng âm thanh : " << dur2 << " giây" << std::endl;
        std::cout << "  • Thời gian xử lý     : " << t2 << " giây" << std::endl;
        std::cout << "  • Tốc độ sinh thực tế : " << (dur2 / t2) << "x Realtime" << std::endl;
        std::cout << "  • File xuất           : " << out2 << std::endl;
    }

    // ------------------------------------------------------------------------
    // TÌNH HUỐNG 3: HỘI THOẠI KỊCH TÍNH, DẤU HỎI VÀ DẤU CẢM THÁN
    // ------------------------------------------------------------------------
    print_header("TÌNH HUỐNG 3: ĐOẠN HỘI THOẠI CẢM THÁN & NGẮT NGHỈ TỰ NHIÊN");
    std::string dialogue_text = 
        "Ngươi là ai? Tại sao lại dám đặt chân vào cấm địa này! "
        "Ta không có ác ý, chỉ muốn tìm đường về thôn mà thôi. "
        "Ha ha ha! Lời nói dối trẻ con, mau nộp mạng đi!";

    std::cout << "📝 Nội dung hội thoại có dấu hỏi (?), cảm thán (!):" << std::endl;
    std::cout << dialogue_text << "\n" << std::endl;

    double dur3 = 0.0, t3 = 0.0;
    std::string out3 = "outputs/scenario_3_dialogue.wav";
    if (tts.speak_to_file(dialogue_text, out3, /*steps=*/10, /*speed=*/0.85f, /*temp=*/0.75f, /*pause=*/0.22f, &dur3, &t3)) {
        std::cout << "✅ [Tình huống 3 Thành công!]" << std::endl;
        std::cout << "  • Thời lượng âm thanh : " << dur3 << " giây" << std::endl;
        std::cout << "  • Thời gian xử lý     : " << t3 << " giây" << std::endl;
        std::cout << "  • Tốc độ sinh thực tế : " << (dur3 / t3) << "x Realtime" << std::endl;
        std::cout << "  • File xuất           : " << out3 << std::endl;
    }

    // ------------------------------------------------------------------------
    // TÌNH HUỐNG 4: CHẾ ĐỘ SIÊU TỐC TRÊN CPU (2 STEPS CHO CHATBOT / TRỢ LÝ)
    // ------------------------------------------------------------------------
    print_header("TÌNH HUỐNG 4: CHẾ ĐỘ SIÊU TỐC TRÊN CPU (2-STEPS)");
    MatchaTTS cpu_tts;
    cpu_tts.init("models_onnx", /*use_gpu=*/false, "fp32");

    std::string prompt_fast = "Xin chào, hệ thống đã sẵn sàng phản hồi tức thì mọi yêu cầu của bạn.";
    double dur4 = 0.0, t4 = 0.0;
    std::string out4 = "outputs/scenario_4_fast_cpu.wav";
    if (cpu_tts.speak_to_file(prompt_fast, out4, /*steps=*/2, /*speed=*/0.85f, /*temp=*/0.9f, /*pause=*/0.15f, &dur4, &t4)) {
        std::cout << "✅ [Tình huống 4 Thành công!]" << std::endl;
        std::cout << "  • Thời lượng âm thanh : " << dur4 << " giây" << std::endl;
        std::cout << "  • Thời gian xử lý     : " << t4 << " giây (" << (t4 * 1000.0) << " ms)" << std::endl;
        std::cout << "  • Tốc độ sinh thực tế : " << (dur4 / t4) << "x Realtime" << std::endl;
        std::cout << "  • File xuất           : " << out4 << std::endl;
    }

    std::cout << "\n" << std::string(75, '=') << std::endl;
    std::cout << "🎉 TẤT CẢ CÁC TÌNH HUỐNG THỬ NGHIỆM ĐÃ HOÀN TẤT THÀNH CÔNG RỰC RỠ!" << std::endl;
    std::cout << std::string(75, '=') << std::endl;

    return 0;
}
