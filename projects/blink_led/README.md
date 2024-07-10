# Blink\_led Challenge 3 Overview
- For this project we'll use a timer to control the delay and toggle the LED
    - We'll use Timer6 as the basic timer and utilize the timer interrupt to control toggling the LED
    - This allows us to execute other code in between toggling the LED as we aren't forcing the processesor to 
do nothing for an extended delay 
- This file aims to provide insight and explanation for what the specific ports, variables, etc. used in the project are doing and where they come from/mean

# Program Flow
1. We include the STM32F4 series definitions that include register definitions, bit positions, and peripheral base addresses
2. Enable GPIOA clock
    - This keeps the internal logic and operations of the peripheral devices, such as GPIO or UART, synchronized
3. Configure Port A pin 5 as output
    - This pin is connected to LD2 as described in [Nucleo Peripheral Pin definitions](https://github.com/ARMmbed/mbed-os/blob/master/targets/TARGET_STM/TARGET_STM32F4/TARGET_STM32F446xE/TARGET_NUCLEO_F446RE/PeripheralPins.c)
    - Each pin has 2 bits in the MODER for setting the type
        - `00` = Input
        - `01` = Output
        - `10` = Alternate function
        - `11` = Analog Mode   
4. Next we setup the basic timer, Timer6
    - First we must enable the bus that connects to Timer6, which is AHB/APB1.
        - This info can be found from the [stm32f446re datasheet](https://www.st.com/resource/en/datasheet/stm32f446re.pdf) found in Figure 3 on page 16. 
    - Next we Set the Prescaler (PSC) and Auto-Reload Register (ARR) values
        - These values determine the delay between events or when the counter resets (More on these values below)
    - Then we set the Update Generation (UG) bit in the Event Generation Register (EGR)
        - This triggers an event to re-initialize the timer count to 0 and apply the set PSC and ARR
        - So this is done after we've set the PSC and ARR registers
5. Then we setup the interrupt for the timer
    - First we enable the DIER (Data Memory Access (DMA) Interuupt Enable Register) Update Interrupt Enable bit
        - This defines which event triggers the timer 6 interrupt. In our case we want the interrupt to fire on an update which is when the counter reaches an overflow or the value set in ARR. 
    - Next we enable the Interrupt we want to use. In this case the timer6 interrupt shares an interrupt with the DAC interrupt to save hardware space
    - This is enabled in the Nested Vectored Interrupt Controller (NVIC), where we set it with the IRQn (Interrupt Request number) and then override the handler by redefining `TIM6__DAC_IRQHandler`
6. Finally we enable the timer to start counting
7. Create an infinite loop
