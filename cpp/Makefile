CXX := g++
CXXFLAGS := -std=c++17 -O3 -march=native -mtune=native -mavx2 -mfma -ffast-math -ftree-vectorize -Wall -Wextra -Wno-unused-parameter -pthread
ORT_LIB_DIR ?= $(shell python3 -c "import onnxruntime, os; print(os.path.dirname(onnxruntime.__file__) + '/capi')" 2>/dev/null || echo "/home/alida/.local/lib/python3.12/site-packages/onnxruntime/capi")
NV_LIB_DIRS ?= $(shell python3 -c "import glob; print(' '.join(['-L' + p for p in glob.glob('/home/alida/.local/lib/python3.12/site-packages/nvidia/*/lib')]))" 2>/dev/null || echo "")
NV_RPATH_DIRS ?= $(shell python3 -c "import glob; print(' '.join(['-Wl,-rpath,' + p for p in glob.glob('/home/alida/.local/lib/python3.12/site-packages/nvidia/*/lib')]))" 2>/dev/null || echo "")

INCLUDES := -Iinclude -Iinclude/onnxruntime
LDFLAGS := -L$(ORT_LIB_DIR) $(NV_LIB_DIRS) \
           -lonnxruntime \
           -Wl,-rpath,$(ORT_LIB_DIR) $(NV_RPATH_DIRS) \
           -lm

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := ../bin

COMMON_OBJS := $(OBJ_DIR)/text_processor.o $(OBJ_DIR)/text_normalizer.o $(OBJ_DIR)/tts_engine.o $(OBJ_DIR)/sonic.o
CLI_TARGET := $(BIN_DIR)/matcha_tts_cli

.PHONY: all clean

all: $(CLI_TARGET)

$(CLI_TARGET): $(OBJ_DIR)/main.o $(COMMON_OBJS) | $(BIN_DIR)
	@echo "🔗 [Link] Đang liên kết nhị phân CLI: $@"
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
	rm -rf $(OBJ_DIR) $(BIN_DIR)/*
