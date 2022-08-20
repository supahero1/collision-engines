#!/bin/bash

cc hshg.c hshg_test.c -o hshg -Og -g3 -fno-omit-frame-pointer -lshnet -lm && valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./hshg
