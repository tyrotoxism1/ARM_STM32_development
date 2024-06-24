#include "stm32f4xx.h"

int main(void) {
    // Initialization code
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  // Enable GPIOA clock
    GPIOA->MODER |= GPIO_MODER_MODE6_0;   // Set PA5 as output

    while (1) {
        // Toggle LD2 LED
        GPIOA->ODR ^= GPIO_ODR_OD6;
        // Delay
        for (volatile int i = 0; i < 1000000; i++);
    }
}
