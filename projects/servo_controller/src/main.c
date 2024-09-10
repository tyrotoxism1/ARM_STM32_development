#include "stm32f4xx.h"
#include "UART.h"

typedef enum {
    IR_STATE_IDLE = 0,          
    //During IR trans, the first setup pulse is expected to be 9ms
    IR_STATE_SETUP_1,
    //The second setup pulse is expected to be either 4.5ms(start of command) or 2.25ms(repeat prev command)
    IR_STATE_SETUP_2,
    IR_STATE_REPEAT,
    IR_STATE_PROCESS_RISE,
    IR_STATE_PROCESS_FALL,
    IR_STATE_ERROR,      
    IR_STATE_COMPLETE   
} IR_state;

typedef enum { 
    ERROR_NONE = 0,
    ERROR_SETUP_1,
    ERROR_SETUP_2,
    ERROR_BIT_PROCESSING
} IR_ERROR;




static const uint32_t PRESCALER = 15; 
//Timer overflows after around 105ms, generating interrupt to reset FSM
static const int AUTORELOAD = 100000; 
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

static uint32_t setup_1_time=0;
static uint32_t setup_2_time=0;
//array to hold the differoence in times between rise and fall of signal creating 1s and 0s of IR transmissions
//32 data bits are being sent
uint32_t IR_burst_times[49] = {0};

extern "C" {
    void TIM2_IRQHandler(void){
        if(TIM2->SR & TIM_SR_UIF){
            GPIOA->ODR |= GPIO_ODR_OD8;
            TIM2->SR &= ~(TIM_SR_UIF);
            curr_ir_state = IR_STATE_IDLE;
            //GPIOA->ODR |= (GPIO_ODR_OD5);
            //UART2_Printf("In Timer 2 interrupt! %i \r\n", TIM2->CNT);
            if(curr_ir_error!= ERROR_NONE) 
                //UART2_Printf("ERROR: %i\r\n", curr_ir_error);
            curr_ir_error = ERROR_NONE;
            //GPIOA->ODR &= ~(GPIO_ODR_OD5);
            TIM2->CR1 &= ~(TIM_CR1_CEN);
            TIM2->CNT = 0;
            // Reinstate EXTI9 interrupt in case error occured during transmission 
            // Everything else(TIM2 count, FSM state, etc, should've beeen reset in ERROR state
            GPIOA->ODR &= ~(GPIO_ODR_OD8);
            NVIC_EnableIRQ(EXTI9_5_IRQn);
        }
    }

    void EXTI9_5_IRQHandler(void){
        // Check if the interrupt was triggered by EXTI line 9 
        if (EXTI->PR & EXTI_PR_PR9) {
            UART2_Printf("In EXTI\r\n");
            // Clear the interrupt pending bit 
            EXTI->PR |= EXTI_PR_PR9;
            switch (curr_ir_state){
                case IR_STATE_IDLE:
                    setup_2_time, setup_1_time = 0;
                    //Set FSM to setup1
                    curr_ir_state = IR_STATE_SETUP_1;
                    //reset and start counter 
                    TIM2->CNT = 0; 
                    TIM2->CR1 |= TIM_CR1_CEN;
                    //GPIOA->ODR |= GPIO_ODR_OD5;
                    //GPIOA->ODR |= GPIO_ODR_OD5;
                    break;
                case IR_STATE_SETUP_1:
                    //capture counter value
                    setup_1_time = TIM2->CNT;
                    //if cnt is 9ms enter second setup
                    if(setup_1_time>LOW_9MS_RANGE && setup_1_time<HIGH_9MS_RANGE)
                        curr_ir_state = IR_STATE_SETUP_2;
                    //else if cnt was any other value, set FSM to ERROR
                    else{
                        curr_ir_state = IR_STATE_ERROR;
                        curr_ir_error = ERROR_SETUP_1;
                    }
                    break;
                case IR_STATE_SETUP_2:
                    //Capture difference between previous counter value from IR_STATE_SETUP_1 and new capture
                    setup_2_time = (TIM2->CNT) - setup_1_time;
                    //If value is around 4500 
                    if(setup_2_time>LOW_4_5MS_RANGE && setup_2_time<HIGH_4_5MS_RANGE){
                        //set FSM to PROCESS_DATA
                        curr_ir_state = IR_STATE_PROCESS_RISE; 
                    }
                    //If value is around 2250 set FSM to REPEAT
                    else if(setup_2_time>LOW_bit_1_RANGE && setup_2_time<HIGH_bit_1_RANGE)
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
                    //Yes very long bad if statement :(
                    //But if the IR pulse is not within either a 1 or 0 timing, enter ERROR state
                    if( (IR_burst_times[timer_arr_index]>=1400 && IR_burst_times[timer_arr_index]<=1800) || (IR_burst_times[timer_arr_index]>=300 && IR_burst_times[timer_arr_index]<=800) ){
                        curr_ir_state = IR_STATE_PROCESS_RISE;
                    }
                    else{
                        curr_ir_state = IR_STATE_ERROR; 
                        curr_ir_error = ERROR_BIT_PROCESSING;
                    }
                    break;
                case IR_STATE_REPEAT:
                    //GPIOA->ODR &= ~(GPIO_ODR_OD5);
                    GPIOA->ODR |= (GPIO_ODR_OD5 | GPIO_ODR_OD6 | GPIO_ODR_OD7);
                    curr_ir_state = IR_STATE_IDLE;
                    //UART2_Printf("REPEATE\r\n");
                    break;
                case IR_STATE_ERROR:
                    GPIOA->ODR |= GPIO_ODR_OD8;
                    //Disable EXTI9 to ignore rest of transmission until timer resets FSM and transmission tracking 
                    NVIC_DisableIRQ(EXTI9_5_IRQn);
                    //clear any captured values used for setup timing or other timing vars
                    setup_2_time, setup_1_time= 0; //, TIM2->CNT 
                    curr_ir_state = IR_STATE_IDLE;
                    //reset FSM to IDLE
                    //If timer isn't running(for whatever reason), we don't need to do anything extra 
                    break;
                default:
                    curr_ir_state = IR_STATE_IDLE;
                    break;
            } //End of swtich statement
        } //End of if statement checking EXTI9 flag
    } 
} //End of `extern C`


void GPIO_init(void){
    // Enable GPIOA and B clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  
    // Enable the system configuration controller
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // Set PA6,7 and 8 as output(All LEDs)
    GPIOA->MODER |= GPIO_MODER_MODE6_0 | GPIO_MODER_MODE7_0 | GPIO_MODER_MODE8_0;
    // Set PA9 as input by clearing both mode bits, used for IR sesnor input
    GPIOA->MODER &= ~(GPIO_MODER_MODE9);
    // Set PA0 to alternate function 
    GPIOA->MODER |= GPIO_MODER_MODE0_1;   

    // Configure pull down for PA9 (pull down is set with `0b10` in the PUPDR)  
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD9); 
    GPIOA->PUPDR |= (GPIO_PUPDR_PUPD9_1);

    // Set the alternate function for PA0 to TIM5_CH1 by setting AF2 0x02
    GPIOA->AFR[0] |= GPIO_AFRL_AFRL0_1;

    return;
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
    return;
}

void timer2_init(void){
    // Enable TIM2 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    // Setup counter values
    TIM2->PSC = PRESCALER;
    TIM2->ARR = AUTORELOAD;
    TIM2->EGR = TIM_EGR_UG;
    //UART2_Printf("ARR: %i,   PRE: %i\r\n", TIM2->ARR, TIM2->PSC);
    //UART2_Printf("CR1: %b \r\n", TIM2->CR1);
    NVIC_EnableIRQ(TIM2_IRQn);
    //Enable the update/overflow interrupt
    TIM2->DIER |= TIM_DIER_UIE;
    TIM2->CR1 &= ~TIM_CR1_CEN;
    // Update and apply settings
}



void timer5_pwm_init(void){
    //Enable Timer 5 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    //Set prescalar to 15 so that each count is around a microsecond 
    TIM5->PSC = 15;
    //With PSC counting up every us, ARR of 20,000 gives us a 50Hz period 
    TIM5->ARR = 20000;
    //We want the duty cycle of the PWM to be around 1-2ms so we'll set the CCR1 to 2000
    TIM5->CCR1 = 2000;
    //Set output compare mode to PWM mode 1 (upcounting and channel 1 is active when CNT<CCR1) with value `0b110` and enable preload
    TIM5->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE;
    // Enable caputre/compare output on channel1 (CC1E)
    TIM5->CCER |= TIM_CCER_CC1E;
    //Enable Auto-reload preload 
    TIM5->CR1 |= TIM_CR1_ARPE;
    //Start the timer
    TIM5->CR1 |= TIM_CR1_CEN;
    return;
}

void parse_transmission(uint8_t* address_low, uint8_t* address_high, uint8_t* command){
    for(int i = 1; i<=48; i++){
        // First 8 bits are Address_LOW of transmission
        if(i<=8){
            //Shift bits over like a queue
            *address_low <<= 1;
            //If the pusle width is 1, then LSB is a 1, Otherwise it can be left as 0
            if(IR_burst_times[i]>1400 && IR_burst_times[i]<1800)
                *address_low |= 1;
        }
        // After Address_LOW inverse section, bits 17-24 are Address_HIGH 
        else if(i>=17 && i<=24){
            *address_high <<= 1;
            if(IR_burst_times[i]>1400 && IR_burst_times[i]<1800)
                *address_high |= 1; 
        }
        // Finally after Address_HIGH inverse section, bits 33-48 are the command bits
        else if(i>=33 && i<=40){
            *command <<= 1;
            if(IR_burst_times[i]>1400 && IR_burst_times[i]<1800)
                *command |= 1; 
        }
    }
}


//Function just turns on LEDs to indicate which button is pressed.
// Function assums all bits in command are valid, if value doesn't match
// inidcate with ERROR LED 
void execute_command(uint16_t command){
    GPIOA->ODR &= ~(GPIO_ODR_OD6 | GPIO_ODR_OD7);
    switch(command){
        // Plus button,     0b10110000 
        case 176:
            //GPIOA->ODR |= GPIO_ODR_OD5;
            break;
        // Power Button,    0b11000000 
        case 192:
            GPIOA->ODR |= GPIO_ODR_OD6;
            break;
        // Minus Button,    0b10001000 
        case 136:
            //GPIOA->ODR |= (GPIO_ODR_OD5 | GPIO_ODR_OD6) ;
            break;
        // Timer Button,    0b11010000
        case 208:
            GPIOA->ODR |= GPIO_ODR_OD7;
            break;
        // Mode Button,     0b11110000
        case 240:
            //GPIOA->ODR |= (GPIO_ODR_OD5 | GPIO_ODR_OD7);
            break;
        // Swivel Button,   0b11001000
        case 200:
            GPIOA->ODR |= (GPIO_ODR_OD6 | GPIO_ODR_OD7);
            break;
        //Default turns on ERROR LED
        default:
            GPIOA->ODR |= GPIO_ODR_OD8;
    }
    return;
}



int main(void) {
    curr_ir_state = IR_STATE_IDLE;
    curr_ir_error = ERROR_NONE;
    uint8_t address_low, address_high, command;
 
   // Initialization code
    UART2_Init();
    GPIO_init();
    EXTI_init();
    timer2_init();
    timer5_pwm_init();
    GPIOA->ODR &= ~GPIO_ODR_OD6;

    NVIC_EnableIRQ(EXTI9_5_IRQn);
    TIM2->CNT = 0;
    GPIOA->ODR |= GPIO_ODR_OD6 | GPIO_ODR_OD7;
    GPIOA->ODR &= ~(GPIO_ODR_OD8);


    while (1) {
        switch(curr_ir_state){
            // If the counter is not running and the FSM is stuck in SETUP, reset back to IDLE
            // This often happens when spamming the buttons and holding long enough for the remote to send
            // a repeat transmission where the repeat signal gets messed up, leaving the FSM in setup
            //In Complete state, reset interrupt_count, set command buffer...
            case IR_STATE_SETUP_1:
                //GPIOA->ODR |= GPIO_ODR_OD8;
                if(TIM2->CR1 & (~TIM_CR1_CEN)){
                   GPIOA->ODR ^= GPIO_ODR_OD8;
                    curr_ir_state = IR_STATE_IDLE;
                    TIM2->CNT = 0;
                }
                //GPIOA->ODR &= GPIO_ODR_OD8;
                break;
            case IR_STATE_COMPLETE:
                // Disable interrupts until the desired command execution is complete 
                NVIC_DisableIRQ(EXTI9_5_IRQn);
                //Reset the IR burst variables
                curr_ir_state = IR_STATE_IDLE;
                timer_arr_index=0;
                TIM2->CR1 &= ~(TIM_CR1_CEN);
                //GPIOA->ODR ^= GPIO_ODR_OD5;
                //Re enable EXTI9 interrupt
                //UART2_Printf("IR burst complete, CNT: %i\r\n", TIM2->CNT);
                //UART2_Printf("9ms val: %i\t4.5ms val: %i\r\n", setup_1_time, setup_2_time);
                TIM2->CNT = 0;
                parse_transmission(&address_low, &address_high, &command);
                execute_command(command);
                // Clear any pending interrupt bits
                EXTI->PR |= EXTI_PR_PR9;
                //GPIOA->ODR &= ~(GPIO_ODR_OD5);
                NVIC_EnableIRQ(EXTI9_5_IRQn);
                break;
            case IR_STATE_ERROR:
                    //Disable EXTI9 to ignore rest of transmission until timer resets FSM and transmission tracking 
                    // But only if the timer is running, if we are just stuck in ERROR state, reset below
                    if(TIM2->CR1 & TIM_CR1_CEN)
                        NVIC_DisableIRQ(EXTI9_5_IRQn);
                    //clear any captured values used for setup timing or other timing vars
                    setup_2_time, setup_1_time= 0; //, TIM2->CNT 
                    curr_ir_state = IR_STATE_IDLE;
                    curr_ir_error = ERROR_NONE;
                    //reset FSM to IDLE
                    //If timer isn't running(for whatever reason), we don't need to do anything extra 
                    break;        
        }

    }
}

