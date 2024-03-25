# Batched Lookup Table

This is an end-to-end implementation of batched LUT. 

To compile, 

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=true -DLUT_OUTPUT_SIZE=$OUTPUT_SIZE
cmake --build ./build --target batchlut --parallel
```

`$OUTPUT_SIZE` can be set as 16 or 20. 

Then launch 

```bash
./build/bin/batchlut 127.0.0.1 r=1 par=0/1
./build/bin/batchlut 127.0.0.1 r=2 par=0/1
```

in 2 terminals. `par` is parallelization flag (1 for parallel computing, 0 for single thread). 

### Generate .lut files for FLUTE

The executable is `./build/bin/flutegen`. Run with `-h` for usage. The output would be saved in `./flute_luts/`
