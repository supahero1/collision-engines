#!/bin/bash

cc hshg.c hshg_test.c -o hshg -Ofast -march=native -lshnet -lm && ./hshg
