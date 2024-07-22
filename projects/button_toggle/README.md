# button\_toggle Overview
- This project transitions from internally creating interrupts, like the [[../blink_led]] project, to now having external interrupts
- This project uses the user button on the STM32F4 development board that when pressed, triggers an interrupt that toggles an LED.
- This projects shows how to:
    - Setup GPIO for input
    - Setup the external interrupts for ARM controllers
    - Connect/map the external interrupt to the desired port
    - trigger an interrupt using rising edge


# Program Flow
1. We include the STM32F4 series definitions that include register definitions, bit positions, and peripheral base addresses
2. Enable GPIOA and GPIOC clock
    - This keeps the internal logic and operations of the peripheral devices, such as GPIO or UART, synchronized
3. Configure Port A pin 5 as output
    - Each pin has 2 bits in the MODER for setting the type
        - `00` = Input
        - `01` = Output
        - `10` = Alternate function
        - `11` = Analog Mode   
    - This pin is connected to LD2 as described in [Nucleo STM32 User Manual](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo144-boards-mb1137-stmicroelectronics.pdf) in section 6.4 *LEDs*(page 23). 
4. Configure Port C pin 13 as input
    - This pin is connected to the user push button as described in [Nucleo STM32 User Manual](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo144-boards-mb1137-stmicroelectronics.pdf) in section 6.5 *Push-buttons*(page 23).
5. Set the push-button to use a pull-up resistor
    - The pull-up resistor sets the default state of the push-button to HIGH when not pressed
        - More on this in **Rise/Fall Trigger** section below
6. Connect the external interrupt 13 line to port C pin 13
7. Configure the trigger for the external interrupt 13 to be from rising edge
8. Enable the interrupt handler
9. Loop infinitely until interrupt occurs

# Additional Information
## External Interrupt Lines
### Connecting External Interrupt Lines
- In the project source code, there are two lines to setup the external interrupt for PC13:
```C
    SYSCFG->EXTICR[3] &= ~(SYSCFG_EXTICR4_EXTI13);
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC;
``` 
    - These lines are dealing with the external interrupt configuration register EXTICR, where the STM32F446 has 4 total registers for connecting the 15 avaliable configurable external interrupts to the desired port. 
    - One important note is that each external interrupt can only be connected to the corresponding pin number (Found in section 10.2.5 of the [STM32f446 Datasheet](https://www.st.com/resource/en/reference_manual/dm00135183-stm32f446xx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)).
        - So in our case, we want to use pin 13 or Port C since it connects to the user button. This means we have to use external interrupt 13. 
    - To access the 13th external interrupt, we use index 3 (4th register) to configure the bits 
        - The first line in the example code above clears all the bits of EXTI13 (so bits [7:4] of SYSCFG\_EXTICR4)
        - The next line sets the bits for connecting the interrupt to Port C
### Handling External Interrupts 
- To handle interrupt requests, the 15 configurable EXTI do not each get their own interrupt handler. 
    - Rather, interrupts 5-9 and 10-15 are grouped together, but interrupts 0,1,2,3,4 have a unique interrupt handler. 
- This is why the interrupt hanlder function is named `EXTI15_10_IRQHandler` since the project is using EXTI13.
- This info can be found from Table 38 of the [STM32f446 Datasheet](https://www.st.com/resource/en/reference_manual/dm00135183-stm32f446xx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)
## Rise/Fall Triggers
- A rising edge trigger occurs when the external input goes from a digital value 0 to digital value 1
- A falling edge trigger occurs when the external input goes from a digital value 1 to digital value 0
    - These triggers are important as they provide key functionality for detecting input. 
        - For example, if a project is detecting a protocol, a rising edge provides the exact time to begin decoding the protocol from that point. 
        - Or if a project wants to track the amount of time from when a button is released then a falling edge would be best. 
- In the case of a push button, such as this project, a pull up/down resistor should be used
    - These resistors are often included in microcontroller pins and provide a default state to the pin isntead of left floating (More info about [Pull Up/Down Resistors](https://www.electronics-tutorials.ws/logic/pull-up-resistor.html)
    - This project uses pull up resistor, so the default value is 5V. 
        - This can be seen when the button is pressed, the value drops to 0V, then the external interrupt (configed for rising edge trigger) is triggered only once the button is released



# Abbreviation Breakdown (incomplete)
- RCC => Reset and Clock Control 
    - This module is used to enable/disable clock signals sent to different peripherals
- AHB1ENR => Advanced High-Performance Bus 1 Enable Register
    - The [STM32F446](https://www.st.com/resource/en/datasheet/stm32f446re.pdf) interconnects all Majors (CPUS, DMAs, USB HS) and subordinates (flash mem, RAM, QuadSPI, etc) using a bus matrix
    - Thus we need to let the CPU know which bus to connect to. 
    - So we set AHB1ENR to enable the GPIO A clock 
- GPIO => General Purpose Input/Output
    - This refers to peripheral pins for a board that deal with input and output data
- RCC\_AHB1ENR\_GPIOAEN => GPIO A Enable
    - This refers to the address of the GPIOA clock which is `0x00000001` 
- MODER => Mode Register
- GPIO\_MODER\_MODER5\_0 => This is a bit mask that sets the mode for pin 5  to output
    - Going to the definition of GPIO\_MODER\_MODER5\_0, it expands to the value `0x00000400` which is `0b0100 0000 0000`. 
        - So we can start to see that the structure of the address, since if we group the binary value into groups of 2 bits, we can see that the expansion is setting the 5th set of bits.
        ```
        0b |01| |00| |00| |00| |00| |00|
            P5   P4   P3   P2   P1   P0 
        ```
        - If we set this value to `0x00000C00` (`0b1100 0000 0000`) we would be setting Pin 5 to Analog mode
- ODR => Output Data Register
    - Using `GPIO_ODR` we can set the value of the ODR. 
    - Each GPIO has its own ODR, where each ODR is 32 bits and can be used to set the state of a pin
        - For example, we set `GPIOA->ODR` equal to `GPIO_MODER_MODER5_0`, which we showed to expand to be a 1 at the 5th group of 2 bits. 
 
# Extra challenges
1. Roughly double the amount of delay between toggling the led
2. Change the output pin and create a circuit with your own LED (and resistor) to toggle that LED
3. Implement an actual delay with the use of the timers. 
