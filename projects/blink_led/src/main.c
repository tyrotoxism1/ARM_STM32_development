#include "stm32f4xx.h"

extern "C" {
    void TIM6_DAC_IRQHandler(void) {
        if (TIM6->SR & TIM_SR_UIF) {
            TIM6->SR &= ~(TIM_SR_UIF);
            // Toggle LD2 LED
            GPIOA->ODR ^= GPIO_ODR_OD5;
        }
    }
}


int main(void) {
     // Enable GPIOA clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // Set PA5 as output
    GPIOA->MODER |= GPIO_MODER_MODE5_0;

    // Enable TIM6 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    // Set TIM6 prescaler and auto-reload value
    TIM6->PSC = 65535;
    TIM6->ARR = 100;

    // Update event to apply settings
    TIM6->EGR = TIM_EGR_UG;

    // Enable update interrupt
    TIM6->DIER |= TIM_DIER_UIE;

    // Enable TIM6 interrupt in NVIC
    NVIC_EnableIRQ(TIM6_DAC_IRQn);

    // Start TIM6
    TIM6->CR1 |= TIM_CR1_CEN;
    while (1){
    }
}
