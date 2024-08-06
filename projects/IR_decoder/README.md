# button\_toggle Overview
- This project transitions from internally creating interrupts, like the [[../blink_led]] project, to now having external interrupts
- This project uses the user button on the STM32F4 development board that when pressed, triggers an interrupt that toggles an LED.
- This project shows how to:
    - Setup GPIO for input
    - Setup the external interrupts for ARM controllers
    - Connect/map the external interrupt to the desired port
    - trigger an interrupt using rising edge


# Program Flow
1. Enable GPIO, EXTI and timer
2. Wait for 
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
7. Configure the trigger for the external interrupt 13 to be from the rising edge
8. Enable the interrupt handler
9. Loop infinitely until an interrupt occurs

# Design Notes
## Flow of triggers from IR protocol
- We want to store the timings of bursts in the IR transmissions into an array.
    - To do this, we can have seperate variables for storing the timings of the start transmission and the rest of the bits
- The protocol starts with a 9ms LOW then 4.5ms HIGH signal to indicate beggining of transmission
    - So we'd store those timings then start using an array to store the 48 bits of info received. 
- The EXTI is triggered on both rising and falling edges, so the first 4 times the interrupt is triggered is during the setup, then the next 96 triggers are the rise and falls of the bits.
    - During the setup the timer is enabled immedietly, then the counter value is reset on the rising edge of the 4.5ms setup, then on the falling edge the timer is reset and disabled until the next rising edge
    - On each successive rising edge the timer is enabled and each falling edge stores the value,disables the counter, and resets the value.
- Using the counter value we can use the equation: (counter\_value)/(Periph\_clock) = total\_time
    - This time would be in seconds

 
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
        - So we can start to see the structure of the address, since if we group the binary value into groups of 2 bits, we can see that the expansion is setting the 5th set of bits.
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
1. Change the trigger from a rising edge to a falling edge
2. Create a push-button circuit that uses PA1 as the input
3. Implement button debouncing
