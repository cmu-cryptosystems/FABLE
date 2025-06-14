FROM ubuntu:24.04

# Install system dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake make git \
        gcc g++ clang \
        python3 \
        libomp-dev \
        libgmp-dev \
        libmpfr-dev \
        libssl-dev \
        libboost-all-dev \
        libfmt-dev \
        libtool \
        pkg-config \
        iproute2 \
    && apt-get clean

# Install libOTe
WORKDIR /workspace
RUN git config --global http.sslVerify false 
RUN git clone --recursive https://github.com/osu-crypto/libOTe.git
WORKDIR /workspace/libOTe
RUN git checkout 90dbd858cb47f1381fd9186f8436b14edc0f0dbe
RUN python3 build.py --boost --sodium --all
RUN python3 build.py --install

COPY . /workspace/FABLE
WORKDIR /workspace/FABLE
RUN git submodule update --init --recursive
RUN cmake -S . -B build && cmake --build ./build --parallel
RUN git config --global http.sslVerify true