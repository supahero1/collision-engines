#!/bin/bash

cc test.c qt.c -o qt -Ofast -march=native -lshnet -lm && ./qt