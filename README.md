# Raspberry Pi Pico Closed-Loop DC Motor Controller

A closed-loop speed controller for a 12 V brushed DC gearmotor built with a Raspberry Pi Pico, BTS7960 H-bridge, Hall-effect encoder, potentiometer, direction switch, and SSD1306 OLED.

I built this project to get experience with embedded motor control beyond simply setting a PWM duty cycle. The Pico reads a requested speed from the potentiometer, measures the motor's actual RPM from the encoder, calculates the speed error, and adjusts PWM to keep the motor near the target speed.

The controller also supports bidirectional operation, controlled deceleration before reversing, live OLED telemetry, and USB serial debugging.

## Features

- Closed-loop DC motor speed control
- 0-150 RPM potentiometer setpoint
- Hall-effect encoder feedback
- GPIO interrupt-based pulse counting
- Real-time RPM calculation
- Hardware PWM through the RP2040
- BTS7960 H-bridge motor driver
- Forward/reverse control using a physical SPDT switch
- Controlled PWM ramp-down before reversing
- Stop confirmation using encoder feedback before changing direction
- SSD1306 OLED over I2C
- USB serial telemetry for debugging and testing

## System Overview

```text
Potentiometer
     |
     v
  Pico ADC
     |
     v
 Target RPM
     |
     v
 Speed Error <-----------------------------+
     |                                      |
     v                                      |
Closed-Loop                                 |
Controller                                  |
     |                                      |
     v                                      |
    PWM                                     |
     |                                      |
     v                                      |
  BTS7960                                   |
     |                                      |
     v                                      |
  DC Motor --------> Hall Encoder ----------+
     |
     v
Output Shaft

SPDT Switch ---> Direction / Reversal Logic

Pico I2C ---> SSD1306 OLED
```

The potentiometer determines the requested RPM rather than directly controlling PWM. This lets the controller increase or decrease motor drive based on the difference between requested and measured speed.

## Hardware

- Raspberry Pi Pico / RP2040
- 12 V brushed DC gearmotor with Hall encoder
- BTS7960 / IBT-2 H-bridge motor driver
- 10 kΩ potentiometer
- 0.96 inch 128x64 SSD1306 OLED
- SPDT direction switch
- External 12 V motor supply
- Breadboard and jumper wiring for the current prototype

The Pico is powered over USB while the motor uses a separate 12 V supply. The Pico and motor driver share a common logic ground.

## Pin Map

| Pico GPIO | Connection | Purpose |
|---|---|---|
| GP4 | OLED SDA | I2C data |
| GP5 | OLED SCL | I2C clock |
| GP14 | BTS7960 R_EN | Driver enable |
| GP15 | BTS7960 L_EN | Driver enable |
| GP16 | BTS7960 RPWM | Forward PWM |
| GP17 | BTS7960 LPWM | Reverse PWM |
| GP18 | Hall A | Encoder pulse input |
| GP19 | Hall B | Available encoder channel |
| GP20 | Direction switch | Forward/reverse request |
| GP26 / ADC0 | Potentiometer wiper | Speed setpoint |

## Encoder Feedback

Motor speed is measured using rising-edge interrupts from Hall A.

I calibrated the encoder by manually rotating the output shaft one full revolution several times and recording the pulse count.

```c
#define PULSES_PER_REV 410
```

The control loop runs every 500 ms. During each loop, the firmware calculates the number of new encoder pulses and converts that measurement into RPM.

Hall B is also connected and could be used later for quadrature direction or position sensing.

## Closed-Loop Speed Control

The potentiometer is mapped to a target speed from 0-150 RPM.

```text
error = target RPM - actual RPM
```

The controller then applies an incremental correction to the current PWM command:

```c
motor_pwm = motor_pwm + error * CONTROL_GAIN;
```

```c
#define CONTROL_GAIN 5
```

A positive error increases PWM, while a negative error reduces it.

PWM is limited to the 12-bit range used by the controller:

```text
0 - 4095
```

During testing, the motor can hold a target in the mid-to-high 140 RPM range while using roughly 75% PWM. Once actual RPM reaches the target, the controller no longer needs to increase the drive signal.

This is a simple incremental closed-loop controller rather than a full PID implementation.

## Direction Control

The BTS7960 uses separate PWM inputs for each motor direction.

```text
Forward:
RPWM = motor_pwm
LPWM = 0

Reverse:
RPWM = 0
LPWM = motor_pwm
```

The physical SPDT switch on GP20 selects the requested direction.

The firmware keeps the requested direction separate from the direction currently being driven. This prevents the H-bridge from immediately commanding the motor in the opposite direction.

```text
Direction switch changes
        ↓
DECELERATING
        ↓
PWM ramps toward 0
        ↓
WAITING_FOR_STOP
        ↓
Encoder reports no movement
        ↓
Direction changes
        ↓
READY
        ↓
Closed-loop control resumes
```

The current deceleration step is:

```c
#define DECEL_PWM_STEP 600
```

## OLED Display

The controller uses a 128x64 SSD1306 OLED connected over I2C.

```text
TGT: target RPM
ACT: actual RPM
DIR: FWD / REV
PWM: current PWM command
```

The OLED interface is written directly in C using the Raspberry Pi Pico SDK I2C hardware interface. A small 5x7 bitmap font is included in the firmware so the display can render the required letters and numbers without an external graphics library.

## Serial Debugging

USB serial telemetry was used throughout development and debugging.

The serial output includes active direction, requested direction, controller state, ADC input, target RPM, PWM command, encoder counts, actual RPM, and RPM error.

One useful debugging case was an unstable potentiometer connection that caused the ADC reading to occasionally drop from near full scale to almost zero. The serial data showed that the controller was reacting correctly to a bad input signal, which led to the wiring connection rather than the control code.

## Current Status

The main controller is working on the breadboard prototype.

- [x] Potentiometer speed setpoint
- [x] PWM motor control
- [x] Hall encoder feedback
- [x] RPM measurement
- [x] Closed-loop speed regulation
- [x] Forward and reverse motor control
- [x] Physical direction switch
- [x] Controlled deceleration before reversing
- [x] Encoder-based stop confirmation
- [x] SSD1306 OLED telemetry
- [x] USB serial debugging
- [ ] Final mechanical mounting / wire cleanup
- [ ] Carrier PCB assembly

## PCB

I am designing a KiCad carrier/interface PCB to replace the breadboard signal wiring and make the system cleaner and more repeatable.

The PCB connects the Pico to the BTS7960 control signals, Hall encoder, potentiometer, OLED, and direction switch. The high-current 12 V motor path remains on the BTS7960 screw terminals rather than being routed through the carrier PCB.

## Tools and Concepts Used

- Embedded C
- Raspberry Pi Pico SDK
- RP2040
- ADC
- PWM
- GPIO
- GPIO interrupts
- Hall-effect encoder feedback
- Closed-loop control
- H-bridge motor control
- I2C
- SSD1306 display interface
- State-machine logic
- Serial debugging
- KiCad
- Hardware/software integration

## Next Steps

The main control system is complete. The next phase is focused on the physical build: cleaning up the prototype, finishing the carrier PCB, and doing more testing and controller tuning.
