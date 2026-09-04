#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Tự động phát hiện thư viện ONNX Runtime và NVIDIA CUDA/cuDNN trên mọi máy
ORT_DIR=$(python3 -c "import onnxruntime, os; print(os.path.dirname(onnxruntime.__file__) + '/capi')" 2>/dev/null || echo "/home/alida/.local/lib/python3.12/site-packages/onnxruntime/capi")
NV_LIBS=$(python3 -c "import glob; print(':'.join(glob.glob('/home/alida/.local/lib/python3.12/site-packages/nvidia/*/lib')))" 2>/dev/null || echo "")

export LD_LIBRARY_PATH="$ORT_DIR:$NV_LIBS:$LD_LIBRARY_PATH"

# Tự động biên dịch nếu file binary chưa tồn tại
if [ ! -f "$DIR/bin/matcha_tts_cli" ]; then
    echo "⚙️  Chưa tìm thấy binary CLI, đang tự động biên dịch C++ Native..."
    make -C "$DIR/cpp"
fi

mkdir -p "$DIR/outputs"

exec "$DIR/bin/matcha_tts_cli" "$@"
