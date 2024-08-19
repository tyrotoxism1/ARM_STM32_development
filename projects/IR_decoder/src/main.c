#include "stm32f4xx.h"
#include "UART_console.h"


typedef enum {
    IR_STATE_IDLE,
    //During IR trans, the first setup pulse is expected to be 9ms
    IR_STATE_SETUP_1,
    //The second setup pulse is expected to be either 4.5ms(start of command) or 2.25ms(repeat prev command)
    IR_STATE_SETUP_2,
    IR_STATE_REPEAT,
    IR_STATE_PROCESS_DATA,
    IR_STATE_ERROR,
    IR_STATE_COMPLETE
} IR_state;



static const uint32_t PRESCALER = 15; 
//Timer overflows after around 105ms, generating interrupt to reset FSM
static const uint32_t AUTORELOAD = 10500; 
//Constants for timing ranges
static const uint32_t LOW_9MS_RANGE = 8500;
static const uint32_t HIGH_9MS_RANGE = 9500;
static const uint32_t LOW_4_5MS_RANGE = 4000;
static const uint32_t HIGH_4_5MS_RANGE = 5000;
// Bit timing for 1 and the repeat setup timing is the same
static const uint32_t LOW_bit_1_RANGE = 2000;
static const uint32_t HIGH_bit_1_RANGE = 2500;
static const uint32_t LOW_bit_0_RANGE = 875;
static const uint32_t HIGH_bit_0_RANGE = 1750;

volatile uint32_t interrupt_count=0;
volatile uint32_t timer_arr_index=0;
volatile IR_state curr_ir_state;

static uint32_t setup_9ms=0;
static uint32_t setup_4_5ms=0;
//array to hold the differoence in times between rise and fall of signal creating 1s and 0s of IR transmissions
//32 data bits are being sent
uint32_t IR_burst_times[32] = {0};




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
    // Configure EXTI9 line for PA9
    // First clear EXTICR2 bits
    SYSCFG->EXTICR[2] &= ~(SYSCFG_EXTICR3_EXTI9);
    SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI9_PA;

    // Enable Rising Edge Trigger for PA9 
    EXTI->RTSR |= EXTI_RTSR_TR9;
    // Enable Falling Edge Trigger for PA9
    EXTI->FTSR |= EXTI_FTSR_TR9;
    // Enable EXTI line 9 
    EXTI->IMR |= EXTI_IMR_IM9;


    NVIC_SetPriority(EXTI9_5_IRQn, 3);
    //Enable external interrupt routine
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void timer6_init(void){
    // Enable TIM6 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    // Setup counter values
    TIM6->PSC = PRESCALER;
    TIM6->ARR = AUTORELOAD;
    TIM6->EGR = TIM_EGR_UG;
    //Enable the update/overflow interrupt
    TIM6->DIER |= TIM_DIER_UIE;
    // Update and apply settings
    NVIC_SetPriority(TIM6_DAC_IRQn, 1);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/*
Calling a function could be too slow of a transition due to the context switch and could be better
to do inline function or just processing data within the main while loop  
*/
void process_data(void){
    UART2_Printf("Processing Data\r\n");
    curr_ir_state = IR_STATE_COMPLETE;
    return;
}

void TIM6_DAC_IRQ(void){
    GPIOA->ODR &= GPIO_ODR_OD5;
    //UART2_Printf("Timer Window Completed\r\n");

}

void EXTI9_5_IRQHandler(void){
    // Check if the interrupt was triggered by EXTI line 9 
    if (EXTI->PR & EXTI_PR_PR9) {
        // Clear the interrupt pending bit, this might need to be done when interrupt is done and not immedietly
        EXTI->PR |= EXTI_PR_PR9;
        GPIOA->ODR ^= GPIO_ODR_OD5;
        switch (curr_ir_state){
            case IR_STATE_IDLE:
                //Set FSM to setup1
                curr_ir_state = IR_STATE_SETUP_1;
                //reset and start counter 
                TIM6->CNT = 0; 
                TIM6->CR1 |= TIM_CR1_CEN;
                break;
            case IR_STATE_SETUP_1:
                //capture counter value
                setup_9ms = TIM6->CNT;
                //if cnt is 9ms enter second setup
                if(setup_9ms>LOW_9MS_RANGE && setup_9ms<HIGH_9MS_RANGE)
                    curr_ir_state = IR_STATE_SETUP_2;
                else //else if cnt was any other value, set FSM to ERROR
                    curr_ir_state = IR_STATE_ERROR;
                break;
            case IR_STATE_SETUP_2:
                //Capture difference between previous counter value from IR_STATE_SETUP_1 and new capture
                setup_4_5ms = (TIM6->CNT) - setup_9ms;
                //If value is around 4500 
                if(setup_4_5ms>LOW_4_5MS_RANGE && setup_4_5ms<HIGH_4_5MS_RANGE){
                    //set FSM to PROCESS_DATA
                    curr_ir_state = IR_STATE_PROCESS_DATA; 
                    //Disable EXTI9 interrupt. We want to leave the heavy processing to the main function during this time and essentially block all funcitonality during the data process
                    //NVIC_DisableIRQ(EXTI9_5_IRQn);
                }
                //If value is around 2250 set FSM to REPEAT
                else if(setup_4_5ms>LOW_bit_1_RANGE && setup_4_5ms<HIGH_bit_1_RANGE)
                    curr_ir_state = IR_STATE_REPEAT;
                else
                    curr_ir_state = IR_STATE_ERROR;
                break;
            case IR_STATE_ERROR:
                //If timer is running(meaning in middle of transmision):
                    //Disable EXTI9 to ignore rest of transmission until timer resets FSM and transmission tracking 
                //clear any captured values used for setup timing or other timing vars
                //reset FSM to IDLE
                //If timer isn't running(for whatever reason), we don't need to do anything extra 
                break;
            default:
                curr_ir_state = IR_STATE_IDLE;
                break;
        }
    }
}

int main(void) {
    interrupt_count=0;
    curr_ir_state = IR_STATE_IDLE;
    UART2_Init();
    uint32_t count = 0;

    GPIO_init();
    EXTI_init();
    timer6_init();
    //TIM6->CR1 |= TIM_CR1_CEN;


    while (1){
        //UART2_Printf("IR complete: %s\r\n",IR_burst_complete?"True":"False");
        //UART2_Printf("Interrupt Count: %i\r\n", interrupt_count);
        UART2_Printf("TIM6 val: %i\r\n", TIM6->CNT);
        GPIOA->ODR |= GPIO_ODR_OD5;
        switch(curr_ir_state){
            //While processing data
            case IR_STATE_PROCESS_DATA:
                process_data();
                break;
            //In Complete state, reset interrupt_count, set command buffer...
            case IR_STATE_COMPLETE:
                UART2_Printf("IR burst complete\r\n");
                UART2_Printf("9ms val: %i\t4.5ms val: %i\r\n", setup_9ms, setup_4_5ms);
                //Reset the IR burst variables
                curr_ir_state = IR_STATE_IDLE;
                timer_arr_index=0;
                interrupt_count=0;
            default:
                curr_ir_state = IR_STATE_IDLE;
                break;
        }


        if(curr_ir_state == IR_STATE_COMPLETE){
            UART2_Printf("IR burst complete\r\n");
            UART2_Printf("9ms val: %i\t4.5ms val: %i\r\n", setup_9ms, setup_4_5ms);
            //Reset the IR burst variables
            curr_ir_state = IR_STATE_IDLE;
            timer_arr_index=0;
        }
        else if(curr_ir_state == IR_STATE_REPEAT){
            UART2_Printf("REPEATING!\r\n");
            curr_ir_state = IR_STATE_IDLE;
        }
/*else if (IR_burst_complete == -1){
            UART2_Printf("REPEAT\r\n");
            //Reset the IR burst variables
            IR_burst_complete = 0;
            timer_arr_index=0;
 
        }
*/        
        //UART2_Printf("GPIO Value: %b\r\n", GPIOA->IDR);
    }
}




/* Old interrupt routine
// One burst/command from remote has 99 edges, so after 99 interrupts, reset variables
        ++interrupt_count;
        //Start timer at introduction signal which is expected to be 9ms 
        if(interrupt_count==1){
            TIM6->CR1 |= TIM_CR1_CEN;
            EXTI->PR |= EXTI_PR_PR9;
        }
        //Second interrupt is end of 9ms transmission, capture time and reset value to caputre timer val for 4.5 intro trans
        else if(interrupt_count==2){
            //Stop timer
            TIM6->CR1 &= ~(TIM_CR1_CEN);
            //Caputre timer val
            setup_9ms = TIM6->CNT;
            //Reset value and renable timer
            TIM6->CNT = 0;
            TIM6->CR1 |= TIM_CR1_CEN;
        }
        // End of 4.5 setup trans, we don't restart the timer since next interrupt is the start of the data bit
        else if(interrupt_count==3){
            //Stop timer
            TIM6->CR1 &= ~(TIM_CR1_CEN);
            //Caputre val
            setup_4_5ms = TIM6->CNT;
            // If the setup had the 9ms and then the 4.5ms then enter process data state
            curr_ir_state = (setup_4_5ms>4100 && setup_4_5ms<4900)?IR_STATE_PROCESS_DATA:IR_STATE_IDLE;
            //Reset timer count 
            TIM6->CNT = 0; 
        }

        // If the second pulse is around 2.5ms, then this is a repeat code instead of a new command
        // So reset everything
        else if(interrupt_count==4 && setup_4_5ms<4000){
            interrupt_count=0;
            curr_ir_state = IR_STATE_REPEAT;
            return;
        }
        //Now we have the actual command bits
        //If the interrupt count is even, it's the start of a data bit, otherwise its the end
        //On an even bit we start the timer
        /*
        else if(interrupt_count%2==0)
            TIM6->CR1 |= TIM_CR1_CEN;
        //If it's odd, stop the timer, capture the timer cnt,  
        else if(interrupt_count%2==1){
            TIM6->CR1 &= ~(TIM_CR1_CEN);
            IR_burst_times[timer_arr_index++] = TIM6->CNT; 
        }
        if(interrupt_count>=100){
            interrupt_count = 0;
            timer_arr_index = 0;
            curr_ir_state = IR_STATE_COMPLETE;
            GPIOA->ODR ^= GPIO_ODR_OD5;
        }
    }

*/