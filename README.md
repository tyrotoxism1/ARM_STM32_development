# Overview
- This repo contains different projects developed for the STM32F446 ARM development board. The goal is to learn and document ARM ISA and embedded system development
- This branch specifically accomplishes the simple challenge of using an external LED instead of LD2.

# Notes
## `STM32-base-STM32Cube` altered
- While not best practice, the file `STM32-base-STM32Cube/CMSIS/STM32F4xx/inc/stm32f4xx.h` is altered to include `#define STM32F446xx`
- This is done because defining the preprocessor didn't seem to be recognized when included in any project `main.c` file
- This change makes it less flexible. If a different board is used, this definition must be changed to the respective board 
## gdb binary not included
- Due to the large file size the file in `tools/bin/arm-none-eabi-gdb` is not included in this repo since the file size is too large.
- File can be found within [ARM toolchain](https://developer.arm.com/downloads/-/gnu-rm)
