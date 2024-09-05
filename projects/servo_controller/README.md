# servo_controller Overview
- This is a simple project to learn about PWM and control a MG995 servo motor

# Program Flow


# Setting up PWM Mode
- Frequency of PWM is determined by ARR value
  - So if we want a 50Hz (once every 20ms) frequency with a 16MHz clock and a presalar of 15999 to have the counter count at a rate of 1ms we would need an ARR of 20
- Duty Cycle is determined by te value of the Capture Compare Register (CCR)
  - So we want a duty cycle of around 1/10 or 1/20 (around 1-2ms) so the CCR would be 1-2
- For the PWM mode, we want PWM mode 1 which sedts channel 1 active HGIH as long as the timer is less than the CCR, thus set OC1M bits to `0b110`
  - Keep in mind that the CNT starts at 0 so while the comparison between CNT and CCR isn't less than or equal, it will be the same since it starts at 0
  - We also need to enable the preload and auto-reload preload register by setting OCxPE in CCMR and ARPE in CR1 respectively 
- Once we start the timer we need to set the Update Generation (UG) bit in the EGR to transfer values from the shadow registers to the preload registers 
- The output channel is already configured as active high (which is what we want) since the reset value of CCER is 0x0000
- To pick which pin recieves the PWM it depends on the channel to pin mappings and setting the alternate function for the desired pin and port
  - For TIM2 channel 1 maps to PA0, PA5, PA15. We'll be using PA5
    - This info can be found in the [STM32F446RE Datasheet](https://www.st.com/resource/en/datasheet/stm32f446re.pdf) in table 10 where the Pin names the alternate functions
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
