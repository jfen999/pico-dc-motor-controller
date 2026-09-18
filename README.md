# Raspberry Pi Pico DC Motor Controller

I'm building a DC motor controller around a Raspberry Pi Pico and a 12V gearmotor with a Hall-effect encoder. A potentiometer sets the requested motor speed, and the Pico controls the motor through a BTS7960 motor driver using PWM.

The end goal is a closed-loop controller that measures the motor's actual speed with the encoder and automatically adjusts the PWM output to hold the requested RPM. I'm building the system in stages so I can test each part before combining everything into the final controller.

## Hardware

- Raspberry Pi Pico
- BTS7960 motor driver
- 12V DC gearmotor with Hall-effect encoder
- 10k potentiometer
- 12V motor power supply
- I2C OLED display
- Physical direction switch
- Custom carrier PCB being developed in KiCad

## Current Build

The potentiometer is connected to the Pico's ADC and produces a 12-bit reading from 0 to 4095. The firmware converts that reading into a motor speed command.

The Pico then generates PWM signals for the BTS7960 motor driver, which handles the higher-current 12V motor power. The Pico and motor driver share a common ground while the motor itself is powered from a separate 12V supply.

PWM speed control is working, and I am currently finishing and testing bidirectional motor control. The next hardware step is adding the physical direction switch.

The motor's Hall encoder provides two feedback channels that will be used to measure RPM for closed-loop speed control.

## Control System

The current signal path is:

`Potentiometer -> ADC -> Raspberry Pi Pico -> PWM -> BTS7960 -> DC Motor`

The completed system will add the encoder feedback path:

`Motor -> Hall Encoder -> Raspberry Pi Pico -> Speed Measurement`

The measured speed can then be compared with the requested speed so the controller can adjust the PWM output as needed.

## Software

The firmware is written in C using the Raspberry Pi Pico SDK.

So far, the project has involved:

- ADC input for the potentiometer
- PWM generation for motor speed control
- BTS7960 motor driver control
- Bidirectional motor control development
- Serial output for testing and debugging
- Hall encoder integration
- I2C setup for the OLED display

Serial output has been especially useful during development for checking raw ADC values, potentiometer percentage, speed commands, and motor behavior before adding the display.

## PCB

I'm also developing a custom carrier PCB in KiCad to clean up the breadboard wiring and organize the connections between the Pico, motor driver, potentiometer, encoder, direction switch, and OLED.

The high-current motor connections stay on the BTS7960 screw terminals rather than being routed through the carrier board.

## Next Steps

- Finish testing forward and reverse motor control
- Add the physical direction switch
- Integrate the OLED display
- Calculate motor RPM from the Hall encoder
- Implement closed-loop speed control
- Complete and test the carrier PCB
