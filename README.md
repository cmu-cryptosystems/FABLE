# Batched Lookup Table

This is an end-to-end implementation of batched LUT. 

To compile, 

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=true
cmake --build ./build --target batchlut
```

Then launch 

```bash
./build/bin/batchlut r=1 par=0/1
./build/bin/batchlut r=2 par=0/1
```

in 2 terminals. `par` is parallelization flag (1 for parallel computing, 0 for single thread). 