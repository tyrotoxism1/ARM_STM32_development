# Blink\_led Overview
- This is a simple start project that turns on and off the LD2 led on the STM32F446 development board from Nucleo
    - Project uses a very quick and dirty delay implementation and toggles the led value
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
4. Create an infinite loop
5. Toggle LD2 LED on and off
    - Using the XOR operation, we flip whatever value is stored in data, effectively toggling the value on and off
    - We can access the data in the OUTPUT DATA register(ODR) of pin 5 with `GPIO_ODR_OD5`
        - Then we assign the toggled value to the ODR itself with `GPIO->ODR`
6. Delay infinite loop
    - To create a super simple delay, we just execute a bunch of nothing instructions in a for-loop

7. That's It!

# Abbreviation Breakdown
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
