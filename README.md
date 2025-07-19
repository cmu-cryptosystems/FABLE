# FABLE: Batched Evaluation on Confidential Lookup Tables in 2PC

[![Build](https://github.com/timzsu/FABLE/actions/workflows/build.yml/badge.svg)](https://github.com/timzsu/FABLE/actions/workflows/build.yml)
[![GitHub license](https://img.shields.io/github/license/timzsu/FABLE?color=blue)](https://github.com/timzsu/FABLE/blob/main/LICENSE)

This is the end-to-end implementation of the USENIX Security '25 paper "FABLE: Batched Evaluation on Confidential Lookup Tables in 2PC." 

The extended version of FABLE's paper can be found at https://eprint.iacr.org/2025/1081. You may also find the artifact evaluation at FABLE-AE (https://github.com/timzsu/FABLE-AE). 

## Dependency Installation

### Install with Docker

We recommend setting the environment with Docker. To do this, build a Docker image using the [Dockerfile](./Dockerfile), which will install all dependencies and build FABLE in `/workspace/FABLE`. 

### Install with Conda

It is also possible to install the dependencies with conda. 

```bash
mamba create -n fable gcc gxx cmake=3.31 make ninja fmt mpfr openmp openssl clang clangxx boost pkg-config -c conda-forge
```

## Compilation

We use CMake to build the project. 

First, please update the submodules if you have not already done so: 
```bash
git submodule update --init --recursive
```

To configure, use
```bash
cmake -S . -B build
```
Supported options include
- `LUT_INPUT_SIZE`: The input bits $\delta$ of the LUT. Default: 20. 
- `LUT_OUTPUT_SIZE`: The output bits $\sigma$ of the LUT. Default: 20. 
- `LUT_MAX_LOG_SIZE`: The binary logarithm of the LUT size, rounded up to the nearest integer. Default: same as `LUT_INPUT_SIZE`. 
For example, to build a FABLE protocol for a LUT with 24 input bits, 64 output bits, and $2^{20}$ rows, the project can be configured by
```bash
cmake -S . -B build -DLUT_INPUT_SIZE=24 -DLUT_OUTPUT_SIZE=64 -DLUT_MAX_LOG_SIZE=20 
```

To build the executable, use
```bash
cmake --build ./build --target fable --parallel
```
which will generate an executable `./build/bin/fable`. 

## Execution

To test `./build/bin/fable`, 
```bash
./build/bin/fable $NETWORK_HOST r=1 $optional_args
```
and
```bash
./build/bin/fable $NETWORK_HOST r=2 $optional_args
```
should be launched on two terminals. 

`$NETWORK_HOST` is the IP address of ALICE (the terminal that is running with `r=1`). If the terminals are on the same machine, then `$NETWORK_HOST` can be set as `127.0.0.1`. 

`$optional_args` contains the following options: 
- `p`: The port number for communication. Default: 8000. 
- `bs`: The batch size of the input. Default: 4096. 
- `db`: The LUT size. Default: `exp2(LUT_INPUT_SIZE)`. 
- `seed`: The random seed. Default: 12345.
- `par`: Whether enable parallelization. Default: 1.
- `thr`: Number of threads used in parallelization. Default: 16.
- `l`: Type of the LUT. Default: 0.
    - 0 = Random LUT. 
    - 1 = Gamma Function. 
    - 2 = Cauchy Distance. 
    - 3 = LUT filled with ones. 
- `h`: The OPRF type. Default: 0. 
    - 0 = LowMC
    - 1 = AES
- `f`: Whether to do operator fusion to save communication rounds. Default: 0.

## Citation

If you would like to use our implementation of FABLE, consider citing our paper:
```bibtex
@misc{cryptoeprint:2025/1081,
      author = {Zhengyuan Su and Qi Pang and Simon Beyzerov and Wenting Zheng},
      title = {{FABLE}: Batched Evaluation on Confidential Lookup Tables in {2PC}},
      howpublished = {Cryptology {ePrint} Archive, Paper 2025/1081},
      year = {2025},
      url = {https://eprint.iacr.org/2025/1081}
}
```
We will update the entry once the paper is published on USENIX Security '25. 
