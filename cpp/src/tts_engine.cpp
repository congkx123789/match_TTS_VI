#include "tts_engine.hpp"
#include <iostream>
#include <cmath>
#include <random>
#include <chrono>
#include <algorithm>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "sonic.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TTSEngine::TTSEngine()
    : env_(ORT_LOGGING_LEVEL_WARNING, "matcha_vocos_tts"),
      memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
}

bool TTSEngine::init(
    const std::string& encoder_path,
    const std::string& decoder_path,
    const std::string& vocos_path,
    const std::string& symbols_path,
    bool use_gpu,
    const std::string& prompt_enc_path,
    int num_threads
) {
    std::cout << "===========================================================================" << std::endl;
    std::cout << "⚙️  [C++ TTS Engine] Đang nạp hệ thống mô hình ONNX..." << std::endl;
    std::cout << "===========================================================================" << std::endl;

    // 1. Nạp từ điển ký tự
    if (!text_processor_.load_symbols(symbols_path)) {
        std::cerr << "❌ [C++ TTS Engine] Thất bại khi nạp: " << symbols_path << std::endl;
        return false;
    }
    std::cout << "  • Đã nạp bảng từ điển symbols.json thành công." << std::endl;

    // 2. Cấu hình SessionOptions
    int actual_threads = (num_threads > 0) ? num_threads : 4;
    session_options_ = Ort::SessionOptions();
    session_options_.SetIntraOpNumThreads(actual_threads);
    session_options_.SetInterOpNumThreads(actual_threads);
    session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    session_options_.SetLogSeverityLevel(3); // Ẩn cảnh báo memcpy phụ

    is_gpu_active_ = false;
    if (use_gpu) {
        try {
            OrtCUDAProviderOptions cuda_options{};
            cuda_options.device_id = 0;
            session_options_.AppendExecutionProvider_CUDA(cuda_options);
            is_gpu_active_ = true;
            std::cout << "  • Đã kích hoạt phần cứng GPU (NVIDIA CUDA Execution Provider)!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  ⚠️ Không thể nạp CUDA, chuyển về chế độ CPU: " << e.what() << std::endl;
            session_options_ = Ort::SessionOptions();
            session_options_.SetIntraOpNumThreads(actual_threads);
            session_options_.SetInterOpNumThreads(actual_threads);
            session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            session_options_.SetLogSeverityLevel(3);
            is_gpu_active_ = false;
        }
    } else {
        std::cout << "  • Sử dụng chế độ CPU thuần (Luồng thực thi: " << actual_threads << " threads)." << std::endl;
    }

    // 3. Khởi tạo các Session ONNX
    try {
        std::cout << "  • Nạp Matcha Encoder  : " << encoder_path << std::endl;
        enc_session_ = std::make_unique<Ort::Session>(env_, encoder_path.c_str(), session_options_);

        std::cout << "  • Nạp Matcha Decoder  : " << decoder_path << std::endl;
        dec_session_ = std::make_unique<Ort::Session>(env_, decoder_path.c_str(), session_options_);

        std::cout << "  • Nạp Vocos Vocoder   : " << vocos_path << std::endl;
        vocos_session_ = std::make_unique<Ort::Session>(env_, vocos_path.c_str(), session_options_);

        std::cout << "  • Nạp Prompt Encoder  : " << prompt_enc_path << std::endl;
        prompt_enc_session_ = std::make_unique<Ort::Session>(env_, prompt_enc_path.c_str(), session_options_);
    } catch (const Ort::Exception& e) {
        std::cerr << "❌ [C++ TTS Engine] Lỗi ONNX Runtime: " << e.what() << std::endl;
        return false;
    }

    initialized_ = true;
    std::cout << "✅ [C++ TTS Engine] Toàn bộ mô hình đã sẵn sàng hoạt động!" << std::endl;
    std::cout << "---------------------------------------------------------------------------" << std::endl;
    return true;
}

std::vector<float> TTSEngine::synthesize_sentence(
    const std::string& sentence,
    const TTSConfig& config,
    std::vector<float>& out_norm_mel,
    int64_t& out_mel_len,
    const std::vector<float>& prompt_norm_mel,
    int64_t prompt_mel_frames,
    bool apply_post_dsp
) {
    if (!initialized_) {
        std::cerr << "❌ Engine chưa được khởi tạo!" << std::endl;
        return {};
    }

    std::string cleaned_text = text_processor_.clean_text(sentence);
    if (cleaned_text.empty()) {
        out_norm_mel.clear();
        out_mel_len = 0;
        return {};
    }

    std::vector<int64_t> sequence = text_processor_.text_to_sequence(cleaned_text);
    if (sequence.empty()) {
        out_norm_mel.clear();
        out_mel_len = 0;
        return {};
    }

    int64_t text_len = static_cast<int64_t>(sequence.size());
    std::vector<int64_t> x_lengths_data = { text_len };
    float scale_val = config.length_scale;

    // -------------------------------------------------------------
    // BƯỚC 1: CHẠY MATCHA ENCODER
    // -------------------------------------------------------------
    std::vector<int64_t> x_shape = { 1, text_len };
    std::vector<int64_t> x_len_shape = { 1 };
    std::vector<int64_t> scale_shape = {}; // Scalar

    Ort::Value x_tensor = Ort::Value::CreateTensor<int64_t>(
        memory_info_, sequence.data(), sequence.size(), x_shape.data(), x_shape.size()
    );
    Ort::Value x_len_tensor = Ort::Value::CreateTensor<int64_t>(
        memory_info_, x_lengths_data.data(), x_lengths_data.size(), x_len_shape.data(), x_len_shape.size()
    );
    Ort::Value scale_tensor = Ort::Value::CreateTensor<float>(
        memory_info_, &scale_val, 1, scale_shape.data(), scale_shape.size()
    );

    const char* enc_input_names[] = { "x", "x_lengths", "length_scale" };
    const char* enc_output_names[] = { "mu_y", "y_mask" };

    Ort::Value enc_inputs[] = { std::move(x_tensor), std::move(x_len_tensor), std::move(scale_tensor) };
    auto enc_outputs = enc_session_->Run(
        Ort::RunOptions{nullptr},
        enc_input_names,
        enc_inputs,
        3,
        enc_output_names,
        2
    );

    auto mu_info = enc_outputs[0].GetTensorTypeAndShapeInfo();
    auto mask_info = enc_outputs[1].GetTensorTypeAndShapeInfo();

    std::vector<int64_t> mu_shape = mu_info.GetShape();
    int64_t mel_len = mu_shape[2];
    out_mel_len = mel_len;

    const float* mu_ptr = enc_outputs[0].GetTensorData<float>();
    const float* mask_ptr = enc_outputs[1].GetTensorData<float>();

    size_t total_mel_elements = static_cast<size_t>(1 * N_FEATS * mel_len);
    size_t total_mask_elements = static_cast<size_t>(1 * 1 * mel_len);

    std::vector<float> mu_data(mu_ptr, mu_ptr + total_mel_elements);
    std::vector<float> mask_data(mask_ptr, mask_ptr + total_mask_elements);

    // -------------------------------------------------------------
    // BƯỚC 1B: ĐIỀU KIỆN HÓA BẰNG PROMPT ENCODER (CHÍNH XÁC THEO TRAIN)
    // -------------------------------------------------------------
    // Theo cấu trúc model đã train: prompt_cond = prompt_encoder(mel_prompt)
    // Sau đó cộng điều kiện vào mu: mu_x = mu_x + prompt_cond (tương đương mu_y += prompt_cond)
    if (prompt_mel_frames > 0 && !prompt_norm_mel.empty() && prompt_enc_session_) {
        std::vector<int64_t> p_shape = { 1, N_FEATS, prompt_mel_frames };
        Ort::Value p_tensor = Ort::Value::CreateTensor<float>(
            memory_info_,
            const_cast<float*>(prompt_norm_mel.data()),
            prompt_norm_mel.size(),
            p_shape.data(),
            p_shape.size()
        );

        const char* pe_in_names[] = { "mel_prompt" };
        const char* pe_out_names[] = { "prompt_cond" };

        auto pe_outputs = prompt_enc_session_->Run(
            Ort::RunOptions{nullptr},
            pe_in_names,
            &p_tensor,
            1,
            pe_out_names,
            1
        );

        const float* p_cond_ptr = pe_outputs[0].GetTensorData<float>(); // shape (1, 80)
        for (int c = 0; c < N_FEATS; ++c) {
            float cond_val = p_cond_ptr[c];
            for (int64_t f = 0; f < mel_len; ++f) {
                mu_data[c * mel_len + f] += cond_val;
            }
        }
    }

    // -------------------------------------------------------------
    // BƯỚC 2: GIẢI THUẬT EULER ODE (FLOW MATCHING SOLVER)
    // -------------------------------------------------------------
    // Khởi tạo nhiễu ngẫu nhiên z ~ N(0, temp^2) chuẩn, KHÔNG GHI ĐÈ NHIỄU!
    std::vector<float> x_data(total_mel_elements);
    std::mt19937 rng(42);
    std::normal_distribution<float> normal_dist(0.0f, config.temperature);

    for (size_t i = 0; i < total_mel_elements; ++i) {
        x_data[i] = normal_dist(rng);
    }

    int steps = std::max(1, config.n_timesteps);
    float dt = 1.0f / static_cast<float>(steps);

    const char* dec_input_names[] = { "x", "mask", "mu", "t" };
    const char* dec_output_names[] = { "dphi_dt" };

    std::vector<int64_t> dec_x_shape = { 1, N_FEATS, mel_len };
    std::vector<int64_t> dec_mask_shape = { 1, 1, mel_len };
    std::vector<int64_t> dec_mu_shape = { 1, N_FEATS, mel_len };
    std::vector<int64_t> dec_t_shape = { 1 };

    for (int step = 0; step < steps; ++step) {
        float u_curr = static_cast<float>(step) / static_cast<float>(steps);
        float u_next = static_cast<float>(step + 1) / static_cast<float>(steps);

        float t_val = u_curr;
        float step_dt = dt;

        if (config.use_sway) {
            auto sway_fn = [&](float u) -> float {
                return u + config.sway_coef * (std::cos(static_cast<float>(M_PI) * 0.5f * u) - 1.0f + u);
            };
            t_val = sway_fn(u_curr);
            float t_next = sway_fn(u_next);
            step_dt = t_next - t_val;
        }

        Ort::Value dec_x_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, x_data.data(), x_data.size(), dec_x_shape.data(), dec_x_shape.size()
        );
        Ort::Value dec_mask_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, mask_data.data(), mask_data.size(), dec_mask_shape.data(), dec_mask_shape.size()
        );
        Ort::Value dec_mu_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, mu_data.data(), mu_data.size(), dec_mu_shape.data(), dec_mu_shape.size()
        );
        Ort::Value dec_t_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, &t_val, 1, dec_t_shape.data(), dec_t_shape.size()
        );

        Ort::Value dec_inputs[] = {
            std::move(dec_x_tensor),
            std::move(dec_mask_tensor),
            std::move(dec_mu_tensor),
            std::move(dec_t_tensor)
        };

        auto dec_outputs = dec_session_->Run(
            Ort::RunOptions{nullptr},
            dec_input_names,
            dec_inputs,
            4,
            dec_output_names,
            1
        );

        const float* dphi_ptr = dec_outputs[0].GetTensorData<float>();
        for (size_t i = 0; i < total_mel_elements; ++i) {
            x_data[i] += step_dt * dphi_ptr[i];
        }
    }

    // Denormalize Mel để nạp vào Vocoder: mel = (x * std) + mean
    std::vector<float> denorm_mel(total_mel_elements);
    for (size_t i = 0; i < total_mel_elements; ++i) {
        denorm_mel[i] = (x_data[i] * MEL_STD) + MEL_MEAN;
    }

    // Lưu lại Denormalized Mel (raw log-mel) làm Prompt chuẩn xác 100% theo kiến trúc train_full_22050.py
    out_norm_mel = denorm_mel;

    // -------------------------------------------------------------
    // BƯỚC 3: CHẠY VOCOS VOCODER (MEL -> WAVEFORM)
    // -------------------------------------------------------------
    std::vector<int64_t> vocos_mel_shape = { 1, N_FEATS, mel_len };
    Ort::Value vocos_in_tensor = Ort::Value::CreateTensor<float>(
        memory_info_, denorm_mel.data(), denorm_mel.size(), vocos_mel_shape.data(), vocos_mel_shape.size()
    );

    const char* vocos_input_names[] = { "mel" };
    const char* vocos_output_names[] = { "wav" };

    auto vocos_outputs = vocos_session_->Run(
        Ort::RunOptions{nullptr},
        vocos_input_names,
        &vocos_in_tensor,
        1,
        vocos_output_names,
        1
    );

    auto wav_info = vocos_outputs[0].GetTensorTypeAndShapeInfo();
    size_t audio_samples_count = wav_info.GetElementCount();
    const float* wav_ptr = vocos_outputs[0].GetTensorData<float>();

    // Khử triệt để hiện tượng "xịt / pop" ở đuôi câu:
    // Thuật toán ISTFT của Vocos (hop_length=256, padding='same') bị phản xạ biên ở 2 hops cuối (512 samples ~23ms).
    // Ta cắt bỏ 512 samples lỗi biên này và áp dụng đường cong tắt dần (smooth cosine fade 20ms) để câu kết thúc êm ái tự nhiên.
    size_t trim_edge_samples = 512;
    if (audio_samples_count > trim_edge_samples + 1000) {
        audio_samples_count -= trim_edge_samples;
    }

    std::vector<float> wav(wav_ptr, wav_ptr + audio_samples_count);
    apply_micro_fade(wav, SAMPLE_RATE, 20);

    if (apply_post_dsp && (config.sonic_speed != 1.0f || config.sonic_pitch != 1.0f)) {
        wav = apply_sonic(wav, config.sonic_speed, config.sonic_pitch, SAMPLE_RATE);
        normalize_audio(wav, 0.96f);
    }

    return wav;
}

std::vector<float> TTSEngine::synthesize_paragraph(
    const std::string& text,
    const TTSConfig& config,
    double& out_audio_duration_sec,
    double& out_process_time_sec
) {
    std::string processed_text = text;
    if (config.enable_normalization) {
        std::unordered_set<std::string> allowed_symbols;
        for (const auto& kv : text_processor_.get_symbol_map()) {
            allowed_symbols.insert(kv.first);
        }
        processed_text = matcha::TextNormalizer::clean_and_normalize(text, allowed_symbols);
    }

    auto sentences = text_processor_.split_sentences(processed_text);
    if (sentences.empty()) {
        out_audio_duration_sec = 0.0;
        out_process_time_sec = 0.0;
        return {};
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<float> full_audio;
    std::vector<float> prev_norm_mel;
    int64_t prev_mel_len = 0;
    std::string prev_sentence = "";

    for (size_t idx = 0; idx < sentences.size(); ++idx) {
        const auto& sentence = sentences[idx];

        std::vector<float> prompt_norm_mel;
        int64_t prompt_mel_frames = 0;

        // Trích xuất Normalized Mel từ đuôi câu trước nạp làm Prompt
        if (prev_mel_len > 0 && !prev_norm_mel.empty()) {
            int word_count = 1;
            for (char c : prev_sentence) {
                if (c == ' ') word_count++;
            }
            int actual_n = std::min(word_count, std::max(3, config.tail_words_prompt));
            float ratio = static_cast<float>(actual_n) / static_cast<float>(word_count);
            prompt_mel_frames = std::max<int64_t>(30, static_cast<int64_t>(static_cast<float>(prev_mel_len) * ratio));
            prompt_mel_frames = std::min<int64_t>(prompt_mel_frames, prev_mel_len);

            prompt_norm_mel.resize(N_FEATS * prompt_mel_frames);
            for (int c = 0; c < N_FEATS; ++c) {
                for (int64_t f = 0; f < prompt_mel_frames; ++f) {
                    int64_t src_frame = (prev_mel_len - prompt_mel_frames) + f;
                    prompt_norm_mel[c * prompt_mel_frames + f] = prev_norm_mel[c * prev_mel_len + src_frame];
                }
            }
        }

        std::vector<float> cur_norm_mel;
        int64_t cur_mel_len = 0;

        std::vector<float> chunk_wav = synthesize_sentence(
            sentence,
            config,
            cur_norm_mel,
            cur_mel_len,
            prompt_norm_mel,
            prompt_mel_frames,
            false // Tắt post-DSP ở câu con để ghép nối liền mạch
        );

        full_audio.insert(full_audio.end(), chunk_wav.begin(), chunk_wav.end());
        if (idx + 1 < sentences.size()) {
            float pause_dur = matcha::TextNormalizer::get_pause_duration(sentence, config.pause_config);
            if (config.pause_sec > 0.0f && config.pause_sec != 0.15f) {
                pause_dur = config.pause_sec;
            }
            size_t silence_samples = static_cast<size_t>(pause_dur * static_cast<float>(SAMPLE_RATE));
            std::vector<float> silence_buffer(silence_samples, 0.0f);
            full_audio.insert(full_audio.end(), silence_buffer.begin(), silence_buffer.end());
        }

        prev_norm_mel = std::move(cur_norm_mel);
        prev_mel_len = cur_mel_len;
        prev_sentence = sentence;
    }

    normalize_audio(full_audio, 0.96f);

    // Áp dụng thuật toán Google Sonic WSOLA nếu người dùng yêu cầu tua nhanh/chỉnh pitch
    if (config.sonic_speed != 1.0f || config.sonic_pitch != 1.0f) {
        full_audio = apply_sonic(full_audio, config.sonic_speed, config.sonic_pitch, SAMPLE_RATE);
        normalize_audio(full_audio, 0.96f);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end_time - start_time;

    out_process_time_sec = diff.count();
    out_audio_duration_sec = static_cast<double>(full_audio.size()) / static_cast<double>(SAMPLE_RATE);

    return full_audio;
}

void TTSEngine::normalize_audio(std::vector<float>& audio, float target_peak) {
    if (audio.empty()) return;
    float peak = 0.0f;
    for (float s : audio) {
        float a = std::abs(s);
        if (a > peak) peak = a;
    }
    if (peak > 1e-6f) {
        float factor = target_peak / peak;
        for (float& s : audio) {
            s *= factor;
        }
    }
}

void TTSEngine::apply_micro_fade(std::vector<float>& audio, int sample_rate, int fade_ms) {
    size_t fade_len = static_cast<size_t>(fade_ms * sample_rate / 1000);
    if (audio.size() < 2 * fade_len) return;

    for (size_t i = 0; i < fade_len; ++i) {
        double angle = (static_cast<double>(i) / static_cast<double>(fade_len)) * (M_PI / 2.0);
        float fade_in = static_cast<float>(std::sin(angle) * std::sin(angle));
        float fade_out = static_cast<float>(std::cos(angle) * std::cos(angle));

        audio[i] *= fade_in;
        audio[audio.size() - fade_len + i] *= fade_out;
    }
}

bool TTSEngine::save_wav(const std::string& filepath, const std::vector<float>& audio_samples, int sample_rate) {
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 1;
    format.sampleRate = static_cast<drwav_uint32>(sample_rate);
    format.bitsPerSample = 16;

    drwav wav;
    if (!drwav_init_file_write(&wav, filepath.c_str(), &format, nullptr)) {
        std::cerr << "❌ Không thể mở file để ghi: " << filepath << std::endl;
        return false;
    }

    std::vector<drwav_int16> pcm_data(audio_samples.size());
    for (size_t i = 0; i < audio_samples.size(); ++i) {
        float s = std::clamp(audio_samples[i], -1.0f, 1.0f);
        pcm_data[i] = static_cast<drwav_int16>(s * 32767.0f);
    }

    drwav_uint64 frames_written = drwav_write_pcm_frames(&wav, pcm_data.size(), pcm_data.data());
    drwav_uninit(&wav);

    return frames_written == pcm_data.size();
}

std::vector<float> TTSEngine::apply_sonic(
    const std::vector<float>& input_audio,
    float speed,
    float pitch,
    int sample_rate
) {
    if (input_audio.empty() || (std::abs(speed - 1.0f) < 1e-4f && std::abs(pitch - 1.0f) < 1e-4f)) {
        return input_audio;
    }

    sonicStream stream = sonicCreateStream(sample_rate, 1);
    if (!stream) {
        std::cerr << "❌ Không thể khởi tạo Google Sonic stream!" << std::endl;
        return input_audio;
    }

    sonicSetSpeed(stream, speed);
    sonicSetPitch(stream, pitch);
    sonicSetQuality(stream, 1); // 1 = Chất lượng cao (sinc FIR filter)

    // Nạp toàn bộ dữ liệu audio vào stream
    sonicWriteFloatToStream(stream, input_audio.data(), static_cast<int>(input_audio.size()));
    sonicFlushStream(stream);

    std::vector<float> output_audio;
    output_audio.reserve(static_cast<size_t>(static_cast<float>(input_audio.size()) / speed + 2048));

    std::vector<float> buffer(4096);
    int samples_read = 0;
    do {
        samples_read = sonicReadFloatFromStream(stream, buffer.data(), static_cast<int>(buffer.size()));
        if (samples_read > 0) {
            output_audio.insert(output_audio.end(), buffer.begin(), buffer.begin() + samples_read);
        }
    } while (samples_read > 0);

    sonicDestroyStream(stream);
    return output_audio;
}

bool TTSEngine::load_wav(
    const std::string& filepath,
    std::vector<float>& out_samples,
    int& out_sample_rate
) {
    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 total_pcm_frames = 0;

    float* p_sample_data = drwav_open_file_and_read_pcm_frames_f32(
        filepath.c_str(), &channels, &sample_rate, &total_pcm_frames, nullptr
    );

    if (!p_sample_data) {
        std::cerr << "❌ Không thể mở file âm thanh WAV: " << filepath << std::endl;
        return false;
    }

    out_sample_rate = static_cast<int>(sample_rate);
    out_samples.resize(total_pcm_frames * channels);
    std::copy(p_sample_data, p_sample_data + total_pcm_frames * channels, out_samples.begin());

    drwav_free(p_sample_data, nullptr);
    return true;
}

