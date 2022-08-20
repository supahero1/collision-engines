#!/bin/bash

cc grid.c test.c -o test -Ofast -march=native -lshnet -lm && ./test
