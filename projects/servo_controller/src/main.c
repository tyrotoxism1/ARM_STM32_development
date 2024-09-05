#include "stm32f4xx.h"

void GPIO_init(void){
    // Enable GPIOA clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  
    // Set PA5 to alternate function 
    GPIOA->MODER |= GPIO_MODER_MODE5_1;   
    // Set PA6 to output mode
    GPIOA->MODER |= GPIO_MODER_MODE6_0;
    // Set the alternate function for PA5 to TIM2_CH1 0x01
    GPIOA->AFR[0] |= GPIO_AFRL_AFRL5_0;

    //Set PB4 & 5 to input for pushbuttons
    GPIOB->MODER &= ~(GPIO_MODER_MODE4 | GPIO_MODER_MODE5);
    //Configure pull up for pin 4 and 5
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD4 |GPIO_PUPDR_PUPD5);
    
    return;
}

void timer2_pwm_init(void){
    //Enable Timer 2 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    //Set prescalar to 15 so that each count is around a microsecond 
    TIM2->PSC = 15;
    //With PSC counting up every us, ARR of 20,000 gives us a 50Hz period 
    TIM2->ARR = 20000;
    //We want the duty cycle of the PWM to be around 1-2ms so we'll set the CCR1 to 2
    TIM2->CCR1 = 2000;
    //Set output compare mode to PWM mode 1 (upcounting and channel 1 is active when CNT<CCR1) with value `0b110` and enable preload
    TIM2->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE;
    // Enable caputre/compare output on channel1 (CC1E)
    TIM2->CCER |= TIM_CCER_CC1E;
    //Enable Auto-reload preload 
    TIM2->CR1 |= TIM_CR1_ARPE;
    //Start the timer
    TIM2->CR1 |= TIM_CR1_CEN;
    return;
}

int main(void) {
    // Initialization code
    GPIO_init();
    timer2_pwm_init();
    while (1) {
        if(!(GPIOB->IDR & GPIO_IDR_ID4)){
            GPIOA->ODR |= GPIO_ODR_OD6;
        }
        else{
            GPIOA->ODR &= ~GPIO_ODR_OD6;
        }
    }
}
