DEVICE = STM32F446xE
FLASH  = 0x08000000

USE_ST_CMSIS = true
# add path to libraries folder
CFLAGS += -S -g -0g
CPPFLAGS += -I../../libraries 
# Add the source files from the libraries folder (if there are any .c files)
SRC += $(wildcard ../../libraries/*.c)

# Include the main makefile
include ./STM32-base/make/common.mk

