#!/bin/bash

cc hshg.c hshg_test.c -o hshg -O3 -march=native -lshnet -lm && ./hshg
