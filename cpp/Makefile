CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -Wno-unused-parameter -pthread
ORT_LIB_DIR := /home/alida/.local/lib/python3.12/site-packages/onnxruntime/capi

CUDA_LIB_DIR := /home/alida/.local/lib/python3.12/site-packages/nvidia/cu13/lib
CUDNN_LIB_DIR := /home/alida/.local/lib/python3.12/site-packages/nvidia/cudnn/lib

INCLUDES := -Iinclude -Iinclude/onnxruntime
LDFLAGS := -L$(ORT_LIB_DIR) -L$(CUDA_LIB_DIR) -L$(CUDNN_LIB_DIR) \
           -lonnxruntime \
           -Wl,-rpath,$(ORT_LIB_DIR) \
           -Wl,-rpath,$(CUDA_LIB_DIR) \
           -Wl,-rpath,$(CUDNN_LIB_DIR) \
           -lm

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := ../bin

COMMON_OBJS := $(OBJ_DIR)/text_processor.o $(OBJ_DIR)/text_normalizer.o $(OBJ_DIR)/tts_engine.o $(OBJ_DIR)/sonic.o
CLI_TARGET := $(BIN_DIR)/matcha_tts_cli
TEST_TARGET := $(BIN_DIR)/scenario_test
DEEP_TEST_TARGET := $(BIN_DIR)/prompt_deep_test
SINGLE_CORE_TARGET := $(BIN_DIR)/single_core_test
QUANT_BENCH_TARGET := $(BIN_DIR)/quantization_benchmark
BENCH_LONG_TARGET := $(BIN_DIR)/bench_long_1core
SONIC_TOOL_TARGET := $(BIN_DIR)/sonic_tool
BENCH_RATES_TARGET := $(BIN_DIR)/bench_speed_rates
BENCH_PROMPT_TARGET := $(BIN_DIR)/bench_prompt_speed
BENCH_RAM_TARGET := $(BIN_DIR)/bench_ram_1core
TEST_NORM_TARGET := $(BIN_DIR)/test_normalizer_cpp
INSPECT_TARGET := $(BIN_DIR)/inspect_steps

.PHONY: all clean

all: $(CLI_TARGET) $(INSPECT_TARGET) $(TEST_NORM_TARGET) $(TEST_TARGET) $(DEEP_TEST_TARGET) $(SINGLE_CORE_TARGET) $(QUANT_BENCH_TARGET) $(BENCH_LONG_TARGET) $(SONIC_TOOL_TARGET) $(BENCH_RATES_TARGET) $(BENCH_PROMPT_TARGET) $(BENCH_RAM_TARGET)

$(INSPECT_TARGET): $(OBJ_DIR)/inspect_steps.o $(OBJ_DIR)/text_normalizer.o $(OBJ_DIR)/text_processor.o | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Inspect Steps: $@"
	$(CXX) $^ -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(TEST_NORM_TARGET): $(OBJ_DIR)/test_normalizer.o $(OBJ_DIR)/text_normalizer.o | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Test Normalizer: $@"
	$(CXX) $^ -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(CLI_TARGET): $(OBJ_DIR)/main.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân CLI: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(TEST_TARGET): $(OBJ_DIR)/scenario_test.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Scenario Test: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(DEEP_TEST_TARGET): $(OBJ_DIR)/prompt_deep_test.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Prompt Deep Test: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(SINGLE_CORE_TARGET): $(OBJ_DIR)/single_core_test.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Single Core Test: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(QUANT_BENCH_TARGET): $(OBJ_DIR)/quantization_benchmark.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Quantization Benchmark: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(BENCH_LONG_TARGET): $(OBJ_DIR)/bench_long_1core.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Bench Long 1-Core: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(SONIC_TOOL_TARGET): $(OBJ_DIR)/sonic_tool.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Google Sonic Tool: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(BENCH_RATES_TARGET): $(OBJ_DIR)/bench_speed_rates.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Bench Speed Rates: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(BENCH_PROMPT_TARGET): $(OBJ_DIR)/bench_prompt_speed.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Bench Prompt Speed: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(BENCH_RAM_TARGET): $(OBJ_DIR)/bench_ram_1core.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Bench RAM 1-Core: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(BIN_DIR)/bench_continuous: $(OBJ_DIR)/bench_continuous.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân Continuous Benchmark: $@"
	$(CXX) $^ $(LDFLAGS) -o $@
	@echo "✅ [Build] Biên dịch thành công: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "🔨 [Compile C++] $< -> $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "🔨 [Compile C] $< -> $@"
	gcc -O3 $(INCLUDES) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(CLI_TARGET) $(TEST_TARGET) $(SONIC_TOOL_TARGET)
