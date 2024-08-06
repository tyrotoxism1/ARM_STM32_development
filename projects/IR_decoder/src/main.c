#include "stm32f4xx.h"
#include <stdio.h>
//#include "UART.c"


static const uint32_t PRESCALER = 65535; 
static const uint32_t AUTORELOAD = 1; 

volatile uint32_t interrupt_count=0;
volatile uint32_t timer_arr_index=0;
static uint32_t setup_9ms;
static uint32_t setup_4_5ms;
//array to hold the difference in times between rise and fall of signal creating 1s and 0s of IR transmissions
uint32_t IR_burst_times[51] = {0};
// Controls when timer is counting to determine pulse width within IR transmission
int timer_running = 0;


void EXTI9_5_IRQHandler(void){
        // Check if the interrupt was triggered by EXTI line 9 
    if (EXTI->PR & EXTI_PR_PR9) {
        // Clear the interrupt pending bit
        EXTI->PR |= EXTI_PR_PR9;


        interrupt_count++;
        // At first interrupt enable timer
        if (interrupt_count==1)
            TIM6->CR1 |= TIM_CR1_CEN;
        // The 2nd time the interrupt is called is after 9ms delay and start of 4.5
        else if(interrupt_count==2){
            setup_9ms = TIM6->CNT;
            TIM6->CNT = 0;
        }
        // This is the end of the setup transmission, store the val, reset counter and stop counter until next interrupt
        else if(interrupt_count==3){
            setup_4_5ms = TIM6->CNT;
            TIM6->CR1 &= ~(TIM_CR1_CEN);
            TIM6->CNT = 0;
        }
        else if(interrupt_count<103){
            // Timer runs on even interrupts then stops at odd interrupts
            // So if timer is running, make sure it's enabled and reset 
            if(~(interrupt_count%2)){
                TIM6->CNT = 0;
                TIM6->CR1 |= TIM_CR1_CEN;
            }
            // If the interrupt counter is odd, it's the end of the bit, store the value and disable timer
            else{
                TIM6->CR1 &= ~(TIM_CR1_CEN);
                IR_burst_times[timer_arr_index++] = TIM6->CNT;
            }
        }
        else{ 
            interrupt_count=0;
            // Handle the interrupt (e.g., toggle an LED)
            GPIOA->ODR ^= GPIO_ODR_OD5;
        }

    }
} 


void GPIO_init(void){
    // Enable GPIO A and C clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    // Enable the system configuration controller
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // Set PA5 as output
    GPIOA->MODER |= GPIO_MODER_MODE5_0;
    // Set PA9 as input by clearing both mode bits 
    GPIOA->MODER &= ~(GPIO_MODER_MODE9);
    // Configure pull down for PA9 (pull down is set with `0b10` in the PUPDR)  
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD9); 
    GPIOA->PUPDR |= (GPIO_PUPDR_PUPD9_1);
}

void EXTI_init(void){
    // Configure EXTI9 line for PC9
    // First clear EXTICR2 bits
    SYSCFG->EXTICR[2] &= ~(SYSCFG_EXTICR3_EXTI9);
    SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI9_PA;

    // Enable Rising Edge Trigger for PA9 
    EXTI->RTSR |= EXTI_RTSR_TR9;
    // Enable Falling Edge Trigger for PA9
    EXTI->FTSR |= EXTI_FTSR_TR9;
    // Enable EXTI line 9 
    EXTI->IMR |= EXTI_IMR_IM9;

    //Enable external interrupt routine
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void timer6_init(void){
    // Enable TIM6 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    // Setup counter values
    TIM6->PSC = PRESCALER;
    TIM6->ARR = AUTORELOAD;
    // Update and apply settings
    TIM6->EGR = TIM_EGR_UG;
}

int main(void) {
    interrupt_count=0;
    int count = 0;

    //systick_init(16000000/1000);
    GPIO_init();
    EXTI_init();
    timer6_init();
    //uart_init(UART3, 115200);


    while (1){
    }
}
