#!/bin/bash

clang --target=wasm32 -O3 -flto -nostdlib -Wl,--export-all -Wl,--lto-O3 -Wl,-lm -lm -o hshg.wasm hshg.c hshg_wasm.c
