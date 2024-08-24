#include "stm32f4xx.h"
#include "UART_console.h"

typedef enum {
    IR_STATE_IDLE = 0,          
    //During IR trans, the first setup pulse is expected to be 9ms
    IR_STATE_SETUP_1,
    //The second setup pulse is expected to be either 4.5ms(start of command) or 2.25ms(repeat prev command)
    IR_STATE_SETUP_2,
    IR_STATE_REPEAT,
    IR_STATE_PROCESS_DATA,
    IR_STATE_PROCESS_RISE,
    IR_STATE_PROCESS_FALL,
    IR_STATE_ERROR,      
    IR_STATE_COMPLETE   
} IR_state;

typedef enum { 
    ERROR_NONE = 0,
    ERROR_SETUP_1,
    ERROR_SETUP_2
} IR_ERROR;




static const uint32_t PRESCALER = 15; 
//Timer overflows after around 105ms, generating interrupt to reset FSM
static const int AUTORELOAD = 104999; 
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

volatile uint32_t timer_arr_index=0;
volatile uint32_t process_data_timer_capture=0; 
volatile IR_state curr_ir_state;
volatile IR_ERROR curr_ir_error;

static uint32_t setup_9ms=0;
static uint32_t setup_4_5ms=0;
//array to hold the differoence in times between rise and fall of signal creating 1s and 0s of IR transmissions
//32 data bits are being sent
uint32_t IR_burst_times[49] = {0};






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


    //NVIC_SetPriority(EXTI9_5_IRQn, 3);
    //Enable external interrupt routine
    //NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void timer6_init(void){
    // Enable TIM6 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    // Setup counter values
    TIM6->CR1 &= ~TIM_CR1_CEN;
    TIM6->PSC = PRESCALER;
    TIM6->ARR = 104999;
    TIM6->EGR = TIM_EGR_UG;
    UART2_Printf("ARR: %i,   PRE: %i\r\n", TIM6->ARR, TIM6->PSC);
    UART2_Printf("CR1: %b \r\n", TIM6->CR1);
    //Enable the update/overflow interrupt
    //TIM6->DIER |= TIM_DIER_UIE;
    // Update and apply settings
    //NVIC_SetPriority(TIM6_DAC_IRQn, 1);
}

void timer2_init(void){
    // Enable TIM6 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    // Setup counter values
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->PSC = PRESCALER;
    TIM2->ARR = 104999;
    TIM2->EGR = TIM_EGR_UG;
    UART2_Printf("ARR: %i,   PRE: %i\r\n", TIM2->ARR, TIM2->PSC);
    UART2_Printf("CR1: %b \r\n", TIM2->CR1);
    NVIC_EnableIRQ(TIM2_IRQn);
    //Enable the update/overflow interrupt
    TIM2->DIER |= TIM_DIER_UIE;
    GPIOA->ODR &= ~(GPIO_ODR_OD5);
    // Update and apply settings
    //NVIC_SetPriority(TIM6_DAC_IRQn, 1);
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



extern "C" {
    void TIM6_DAC_IRQ(void){
        if(TIM6->SR & TIM_SR_UIF){
            GPIOA->ODR ^= GPIO_ODR_OD5;
            TIM6->SR &= ~(TIM_SR_UIF);
            //UART2_Printf("Timer Window Completed\r\n");
        }
    }

    void TIM2_IRQHandler(void){
        if(TIM2->SR & TIM_SR_UIF){
            GPIOA->ODR |= (GPIO_ODR_OD5);
            //GPIOA->ODR ^= GPIO_ODR_OD5;
            TIM2->SR &= ~(TIM_SR_UIF);
            UART2_Printf("In Timer 2 interrupt!\r\n");
            // Reinstate EXTI9 interrupt in case error occured during transmission 
            // Everything else(TIM2 count, FSM state, etc, should've beeen reset in ERROR state
            NVIC_EnableIRQ(EXTI9_5_IRQn);
        }
    }

    void EXTI9_5_IRQHandler(void){
        // Check if the interrupt was triggered by EXTI line 9 
        if (EXTI->PR & EXTI_PR_PR9) {
            // Clear the interrupt pending bit, this might need to be done when interrupt is done and not immedietly
            //UART2_Printf("IR STATE\r\n");
            //Temp clearing pending bit so interrupt can try to finish
            EXTI->PR |= EXTI_PR_PR9;
            GPIOA->ODR ^= GPIO_ODR_OD5;
            
            switch (curr_ir_state){
                case IR_STATE_IDLE:
                    setup_4_5ms, setup_9ms = 0;
                    //Set FSM to setup1
                    curr_ir_state = IR_STATE_SETUP_1;
                    //reset and start counter 
                    TIM2->CNT = 0; 
                    TIM2->CR1 |= TIM_CR1_CEN;
                    break;
                case IR_STATE_SETUP_1:
                    //capture counter value
                    setup_9ms = TIM2->CNT;
                    //if cnt is 9ms enter second setup
                    if(setup_9ms>LOW_9MS_RANGE && setup_9ms<HIGH_9MS_RANGE)
                        curr_ir_state = IR_STATE_SETUP_2;
                    else {//else if cnt was any other value, set FSM to ERROR
                        curr_ir_state = IR_STATE_ERROR;
                        curr_ir_error = ERROR_SETUP_1;
                    }
                    break;
                case IR_STATE_SETUP_2:
                    //Capture difference between previous counter value from IR_STATE_SETUP_1 and new capture
                    setup_4_5ms = (TIM2->CNT) - setup_9ms;
                    //If value is around 4500 
                    if(setup_4_5ms>LOW_4_5MS_RANGE && setup_4_5ms<HIGH_4_5MS_RANGE){
                        //set FSM to PROCESS_DATA
                        //curr_ir_state = IR_STATE_PROCESS_DATA; 
                        curr_ir_state = IR_STATE_PROCESS_RISE; 
                        //UART2_Printf("setup count: %i, %i\r\n", setup_9ms, setup_4_5ms);
                        //Disable EXTI9 interrupt. We want to leave the heavy processing to the main function during this time and essentially block all funcitonality during the data process
                        //NVIC_DisableIRQ(EXTI9_5_IRQn);
                    }
                    //If value is around 2250 set FSM to REPEAT
                    else if(setup_4_5ms>LOW_bit_1_RANGE && setup_4_5ms<HIGH_bit_1_RANGE)
                        curr_ir_state = IR_STATE_REPEAT;
                    else{
                        curr_ir_state = IR_STATE_ERROR;
                        curr_ir_error = ERROR_SETUP_2;
                    }
                    break;

                case IR_STATE_PROCESS_RISE:
                    /*
                     * After completing a command, the IR signal ends the last bit logic LOW then returns
                     * to logic HIGH while waiting. To avoid interrupt of the IR returning to the stable IDLE
                     * volatage value and  disturbing main loop from executing the correlated IR command, 
                     * the check for the last bit is done on a rising edge since the reciever 
                     * returns to stable logic HIGH after receviing command. The greater than or equal 
                     * is just to ensure if some pusle was missed, the FSM can return to a known state and 
                     * reset.
                     */
                    process_data_timer_capture = TIM2->CNT;
                    if(timer_arr_index >= 48){
                        curr_ir_state = IR_STATE_COMPLETE;
                        timer_arr_index = 0;
                        break;
                    }
                    curr_ir_state = IR_STATE_PROCESS_FALL;
                    break;
                case IR_STATE_PROCESS_FALL:
                    IR_burst_times[++timer_arr_index] = TIM2->CNT - process_data_timer_capture; 
                    //GPIOA->ODR ^= GPIO_ODR_OD5;
                    curr_ir_state = IR_STATE_PROCESS_RISE;
                    break;
                case IR_STATE_ERROR:
                    NVIC_DisableIRQ(EXTI9_5_IRQn);
                    setup_4_5ms, setup_9ms= 0; //, TIM2->CNT 
                    //TIM2->CR1 &= ~(TIM_CR1_CEN);
                    // Disable the EXTI9 interrupt to allow the system to recover and ignore 
                    // incoming tranmission, the EXTI9 is reinstated after TIM2 interrupt occurs
                    // which is 105ms from the start of the transmission. 
                    curr_ir_state = IR_STATE_IDLE;
                    UART2_Printf("ERROR: %i\r\n", curr_ir_error);
                    curr_ir_error = ERROR_NONE;
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
}

void execute_command(){
    for(int i = 1; i<=48; i++){
        // If around 1600 uS, then received a `1` value
        if(IR_burst_times[i]>1400 && IR_burst_times[i]<1800)
            UART2_Printf("1 ");
        else if(IR_burst_times[i]>300 && IR_burst_times[i]<800) 
            UART2_Printf("0 ");
        else
            UART2_Printf("%i ", IR_burst_times[i]);
        if(i%8==0)
            UART2_Printf("\r\n");
    }
    UART2_Printf("\r\n");
}

int main(void) {
    curr_ir_state = IR_STATE_IDLE;
    curr_ir_error = ERROR_NONE;
    UART2_Init();
    uint32_t count = 0;

    GPIO_init();
    EXTI_init();
    //timer6_init();
    timer2_init();
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    GPIOA->ODR &= ~(GPIO_ODR_OD5);
    //TIM6->CR1 |= TIM_CR1_CEN;


    while (1){
        //UART2_Printf("IR complete: %s\r\n",IR_burst_complete?"True":"False");
        //UART2_Printf("Interrupt Count: %i\r\n", interrupt_count);
        if(curr_ir_state!=0)
            UART2_Printf("IR State: %i, CNT: %i\r\n", curr_ir_state, TIM2->CNT );
        //GPIOA->ODR |= GPIO_ODR_OD5;
        switch(curr_ir_state){
            //While processing data
            case IR_STATE_PROCESS_DATA:
                process_data();
                break;
            //In Complete state, reset interrupt_count, set command buffer...
            case IR_STATE_COMPLETE:
                GPIOA->ODR &= ~(GPIO_ODR_OD5);
                // Disable interrupts until the desired command execution is complete 
                NVIC_DisableIRQ(EXTI9_5_IRQn);
                //Reset the IR burst variables
                curr_ir_state = IR_STATE_IDLE;
                timer_arr_index=0;
                TIM2->CR1 &= ~(TIM_CR1_CEN);
                //GPIOA->ODR ^= GPIO_ODR_OD5;
                //Re enable EXTI9 interrupt
                UART2_Printf("IR burst complete, CNT: %i\r\n", TIM2->CNT);
                UART2_Printf("9ms val: %i\t4.5ms val: %i\r\n", setup_9ms, setup_4_5ms);
                TIM2->CNT = 0;
                execute_command();
                // Clear any pending interrupt bits
                EXTI->PR |= EXTI_PR_PR9;
                NVIC_EnableIRQ(EXTI9_5_IRQn);
                break;
            case IR_STATE_ERROR:
                curr_ir_state = IR_STATE_ERROR;
                timer_arr_index=0;
                break;

        }

        /*
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
        */
    }
}
