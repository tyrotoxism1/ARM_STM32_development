#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include "stm32f4xx.h"
#include <stdio.h>
#include <stdarg.h>

void UART2_Init(void);
void UART2_Write(char ch);
void UART2_WriteString(const char* str);
void UART2_Printf(const char* fmt, ...);

#endif
