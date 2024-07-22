#include "stm32f4xx.h"

extern "C" {
    void EXTI15_10_IRQHandler(void){
         // Check if the interrupt was triggered by EXTI line 13
        if (EXTI->PR & EXTI_PR_PR13) {
            // Clear the interrupt pending bit
            EXTI->PR |= EXTI_PR_PR13;
            // Handle the interrupt (e.g., toggle an LED)
            GPIOA->ODR ^= GPIO_ODR_OD5;
        }
    } 
}


int main(void) {
     // Enable GPIO A and C clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    // Enable the system configuration controller
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // Set PA5 as output
    GPIOA->MODER |= GPIO_MODER_MODE5_0;
    // Set PC13 as input (user button), this means clearing both mode bits 
    GPIOC->MODER &= ~(GPIO_MODER_MODE13);
    // Configure pull up for Pin13 (pull up is set with `0b01` in the PUPDR)  
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD13); // Clear the PUPDR bits for PC13
    GPIOC->PUPDR |= (GPIO_PUPDR_PUPD13_0);

    // Configure EXTI13 line for PC13
    // First clear EXTI13 bits
    SYSCFG->EXTICR[3] &= ~(SYSCFG_EXTICR4_EXTI13);
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC;

    // Enable Rising Edge Trigger for PA13
    EXTI->RTSR |= EXTI_RTSR_TR13;
    // Disable Falling Edge Trigger for PA13
    EXTI->FTSR &= ~(EXTI_FTSR_TR13);
    // Enable EXTI line 13
    EXTI->IMR |= EXTI_IMR_IM13;

    //Enable external interrupt routine
    NVIC_EnableIRQ(EXTI15_10_IRQn);


    while (1){
    }
}
