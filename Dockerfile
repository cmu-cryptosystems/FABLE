FROM ubuntu:24.04

# Install system dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake make git \
        gcc g++ clang \
        python3 python3-matplotlib python3-seaborn \
        libomp-dev \
        libgmp-dev \
        libmpfr-dev \
        libssl-dev \
        libboost-all-dev \
        libfmt-dev \
        libtool \
        pkg-config \
    && apt-get clean

# Install libOTe
WORKDIR /workspace
RUN git clone https://github.com/osu-crypto/libOTe.git --recursive
WORKDIR /workspace/libOTe
RUN python3 build.py --boost --sodium -DENABLE_SILENTOT=ON \
    && python3 build.py --install

COPY . /workspace/FABLE
WORKDIR /workspace/FABLE
RUN cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=true -DLUT_INPUT_SIZE=20 -DLUT_OUTPUT_SIZE=20 -DLUT_MAX_LOG_SIZE=20 && cmake --build ./build --target batchlut --parallel