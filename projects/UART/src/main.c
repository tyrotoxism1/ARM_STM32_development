#include "stm32f4xx.h"
#include <stdio.h>
#include <stdarg.h>

#define PERIPHERAL_CLOCK 16000000 
#define BAUD_RATE 9600 
#define DIV_MANTISSA (PERIPHERAL_CLOCK / (BAUD_RATE * 16))
#define DIV_FRACTION (((PERIPHERAL_CLOCK % (BAUD_RATE * 16)) * 16) / (BAUD_RATE * 16))

unsigned int BRR = (DIV_MANTISSA << 4) + DIV_FRACTION;


void UART2_Init(void) {
    // Enable clocks for GPIOA and USART2
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; 
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    //RCC->APB1ENR |= (1 << 17); // Enable USART2 clock

    // Configure PA2 as USART2_TX (AF7)
    GPIOA->MODER &= ~(GPIO_MODER_MODE2);  // Clear mode bits for PA2
    GPIOA->MODER |= GPIO_MODER_MODE2_1;   // Set PA2 to alternate function mode
    GPIOA->AFR[0] &= GPIO_AFRL_AFSEL2;    // Clear alternate function bits for PA3
    // Set AF7 for PA2 (USART2_TX) by assigning 0b0111 to AFRL2[11:8](7.4.9 in refman)
    GPIOA->AFR[0] |= GPIO_AFRL_AFSEL2_0 | GPIO_AFRL_AFSEL2_1 | GPIO_AFRL_AFSEL2_2;

    // Configure PA3 as USART2_RX (AF7)
    GPIOA->MODER &= ~(GPIO_MODER_MODE3); // Clear mode bits for PA3
    GPIOA->MODER |= GPIO_MODER_MODE3_1;  // Set PA3 to alternate function mode
    GPIOA->AFR[0] &= ~ GPIO_AFRL_AFSEL3; // Clear alternate function bits for PA3
    // Set AF7 for PA2 (USART2_TX) by assigning 0b0111 to AFRL2[11:8](7.4.9 in refman)
    GPIOA->AFR[0] |= GPIO_AFRL_AFSEL3_0 | GPIO_AFRL_AFSEL3_1 | GPIO_AFRL_AFSEL3_2;

    // Configure USART2
    USART2->BRR = BRR;  // Baud rate: 9600 @ 16MHz
    USART2->CR1 = 0x200C; // Enable USART2, TX, RX
}

void UART2_Write(char ch) {
    while (!(USART2->SR & (1 << 7))); // Wait until TXE (Transmit data register empty)
    USART2->DR = ch;                  // Write character to data register
}

void UART2_WriteString(const char* str) {
    while (*str) {
        UART2_Write(*str++);
    }
}

void UART2_Printf(const char* fmt, ...) {
    char buffer[128]; // Buffer to hold the formatted string
    char* ptr;
    va_list args;
    va_start(args, fmt);

    for (ptr = (char*)fmt; *ptr != '\0'; ptr++) {
        if (*ptr == '%') {
            ptr++; // Move to the next character
            switch (*ptr) {
                case 'c': { // Character
                    char ch = (char)va_arg(args, int);
                    UART2_Write(ch);
                    break;
                }
                case 's': { // String
                    char* str = va_arg(args, char*);
                    UART2_WriteString(str);
                    break;
                }
                case 'i':
                case 'd': { // Integer
                    int num = va_arg(args, int);
                    snprintf(buffer, sizeof(buffer), "%d", num);
                    UART2_WriteString(buffer);
                    break;
                }
                case 'u': { // Unsigned integer
                    unsigned int num = va_arg(args, unsigned int);
                    snprintf(buffer, sizeof(buffer), "%u", num);
                    UART2_WriteString(buffer);
                    break;
                }
                case 'x': { // Hexadecimal
                    unsigned int num = va_arg(args, unsigned int);
                    snprintf(buffer, sizeof(buffer), "%x", num);
                    UART2_WriteString(buffer);
                    break;
                }
                case 'f': { // Float
                    double num = va_arg(args, double);
                    snprintf(buffer, sizeof(buffer), "%.2f", num);
                    UART2_WriteString(buffer);
                    break;
                }
                default: { // Unknown specifier
                    UART2_Write('%');
                    UART2_Write(*ptr);
                    break;
                }
            }
        } else {
            UART2_Write(*ptr); // Print regular character
        }
    }
    va_end(args);
}

int main(void) {
    // Initialization code
    UART2_Init();
    GPIOA->MODER |= GPIO_MODER_MODE5_0;   // Set PA5 as output
    uint32_t count=0;
    while (1) {
        // Toggle LD2 LED
        //UART2_WriteString("Hello!\r\n");
        UART2_Printf("Hello %i\r\n", count++);
        GPIOA->ODR ^= GPIO_ODR_OD5;
        // Delay
        //for (volatile int i = 0; i < 1000000; i++);
    }
}
