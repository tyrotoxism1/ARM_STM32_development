#include "stm32f4xx.h"
#include "UART.h"


typedef enum{
    //Waiting for state to change
    WAITING = 0,
    BUTTON_DOWN,
    BUTTON_UP
} BUTTON_STATE;

void GPIO_init(void){
    // Enable GPIOA and B clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;  
    // Set PA5 to alternate function 
    GPIOA->MODER |= GPIO_MODER_MODE5_1;   
    // Set PA6 to output mode
    GPIOA->MODER |= GPIO_MODER_MODE6_0;
    // Set the alternate function for PA5 to TIM2_CH1 0x01
    GPIOA->AFR[0] |= GPIO_AFRL_AFRL5_0;

    //Set PB4 & 5 to input for pushbuttons
    GPIOB->MODER &= ~(GPIO_MODER_MODE4); 
    GPIOB->MODER &= ~(GPIO_MODER_MODE5);
    //Configure pull up for pin 4 and 5
    //GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD4_0 | GPIO_PUPDR_PUPD5_0); 
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
    uint8_t button_down = 0;
    BUTTON_STATE button_state = WAITING;
    // Initialization code
    UART2_Init();
    GPIO_init();
    timer2_pwm_init();
    GPIOA->ODR &= ~GPIO_ODR_OD6;
    UART2_Printf("MODER: %x\r\n",GPIOB->MODER);
    while (1) {
        button_down = (!(GPIOB->IDR & GPIO_IDR_ID4))?1:0;
        //If in waiting state and the button is pressed, we turn the LED on and enter the BUTTON_DOWN state
        if(((button_state==WAITING) || button_state==BUTTON_UP) && button_down){
            GPIOA->ODR |= GPIO_ODR_OD6;
            button_state = BUTTON_DOWN;
        }
        //If the button state is BUTTON_DOWN but the button is actually up,
        //turn the LED off and enter state BUTTON_UP
        else if(button_state==BUTTON_DOWN && !button_down){
            GPIOA->ODR &= ~GPIO_ODR_OD6;
            button_state = BUTTON_UP;
        }
        //Otherwise default to WAIITNG state 
        else
            button_state = WAITING; 
    }
}
