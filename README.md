# Raspberry Pi Pico Closed-Loop DC Motor Controller

A closed-loop speed controller for a 12 V brushed DC gearmotor built around a Raspberry Pi Pico, BTS7960 H-bridge, Hall encoder, potentiometer, direction switch, and SSD1306 OLED.

I built this project to go beyond basic open-loop PWM and work through the full embedded control path: read a user setpoint, measure the motor's actual speed, calculate error, adjust the drive signal, and feed the result back into the next control cycle.

## What it does

- Reads a potentiometer through the Pico ADC and maps it to a target speed from 0-150 RPM
- Counts Hall encoder pulses with a GPIO interrupt
- Calculates actual motor RPM every 500 ms
- Uses closed-loop error correction to adjust PWM until actual RPM approaches the target
- Drives a BTS7960 H-bridge for forward and reverse operation
- Uses a physical SPDT switch for direction control
- Stops the drive and waits for the motor to stop before changing direction
- Displays target RPM, actual RPM, direction, and PWM on a 128x64 SSD1306 OLED
- Streams detailed telemetry over USB serial for testing and debugging

## System overview

```text
Potentiometer
     |
     v
Pico ADC ---> Target RPM
                  |
                  v
             Speed Error <----------------------+
                  |                              |
                  v                              |
         Closed-Loop Correction                  |
                  |                              |
                  v                              |
                PWM                              |
                  |                              |
                  v                              |
              BTS7960                            |
                  |                              |
                  v                              |
              DC Motor ----> Hall Encoder -------+
                  |
                  +----> Output Shaft

SPDT Direction Switch ---> Direction / Reversal Logic

Pico I2C ---> SSD1306 OLED
```

## Hardware

- Raspberry Pi Pico / RP2040
- 12 V DC gearmotor with Hall encoder
- BTS7960 / IBT-2 H-bridge motor driver
- 10 kOhm potentiometer
- 0.96 inch 128x64 SSD1306 I2C OLED
- SPDT direction switch
- External 12 V motor supply
- Breadboard and jumper wiring for the current prototype

The Pico is powered separately over USB. The motor uses the external 12 V supply, with a common logic ground between the Pico and motor driver.

## Pin map

| Pico GPIO | Connection | Purpose |
|---|---|---|
| GP4 | OLED SDA | I2C data |
| GP5 | OLED SCL | I2C clock |
| GP14 | BTS7960 R_EN | Driver enable |
| GP15 | BTS7960 L_EN | Driver enable |
| GP16 | BTS7960 RPWM | Forward PWM |
| GP17 | BTS7960 LPWM | Reverse PWM |
| GP18 | Hall A | Encoder pulse input |
| GP19 | Hall B | Reserved encoder channel |
| GP20 | Direction switch | Forward/reverse request |
| GP26 / ADC0 | Potentiometer wiper | Speed setpoint |

## Encoder feedback

The Hall encoder is read using rising-edge interrupts on Hall A. I calibrated the encoder at the output shaft by manually rotating the shaft one revolution and recording the pulse count across several trials.

The firmware currently uses:

```c
#define PULSES_PER_REV 410
```

This represents approximately 410 Hall A rising edges per output-shaft revolution.

Every 500 ms, the firmware calculates the number of new pulses, converts that value to pulses per second, and then converts it to RPM.

## Closed-loop speed control

The potentiometer sets a target RPM rather than directly commanding PWM.

The controller calculates:

```text
error = target RPM - actual RPM
```

and applies an incremental correction:

```c
motor_pwm = motor_pwm + error * 5;
```

The PWM command is clamped between 0 and 4095.

This means PWM is only increased or decreased as much as needed to bring the measured speed toward the target. At the upper end of the current bench test, target and actual speed settle together in the mid-to-high 140 RPM range while PWM settles around 75% duty instead of automatically saturating at 100%.

The current controller is intentionally simple and is not presented as a full PID implementation.

## Bidirectional control

The BTS7960 is driven using two PWM inputs:

```text
Forward:
RPWM = motor_pwm
LPWM = 0

Reverse:
RPWM = 0
LPWM = motor_pwm
```

The physical direction switch is read on GP20.

To avoid immediately commanding the opposite direction while the shaft is still moving, the firmware keeps separate requested and active direction states. When a direction change is requested, PWM is set to zero. The firmware waits until the encoder reports no pulses during the measurement window, updates the active direction, and then allows closed-loop control to resume.

The next planned firmware improvement is a controlled deceleration ramp before the stop-and-reverse transition.

## OLED interface

A 128x64 SSD1306 display is connected over I2C using GP4 and GP5.

The display shows live values for:

```text
TGT: target RPM
ACT: actual RPM
DIR: FWD / REV
PWM: current PWM command
```

The display driver is implemented directly in C using the Pico SDK I2C hardware interface and a small 5x7 bitmap font.

## Debugging and testing

USB serial telemetry was used throughout development to inspect the complete control loop:

```text
ADC value
potentiometer percentage
target RPM
PWM command
total encoder pulses
pulses per control-loop interval
pulses per second
actual RPM
signed RPM error
direction
direction-change status
```

One useful debugging case was an unstable potentiometer connection that caused ADC values to jump unexpectedly. The serial output made it clear that the controller itself was responding correctly and that the fault was coming from the input wiring.

## Current status

The main prototype is functionally complete:

- [x] Potentiometer speed setpoint
- [x] PWM motor drive
- [x] Hall encoder feedback
- [x] RPM calculation
- [x] Closed-loop speed regulation
- [x] Forward and reverse operation
- [x] Physical direction switch
- [x] Stop-before-reverse interlock
- [x] USB serial telemetry
- [x] SSD1306 OLED telemetry
- [ ] Controlled deceleration before reversal
- [ ] Final mechanical cleanup / mounting
- [ ] Carrier PCB revision and final assembly

## PCB

A KiCad carrier/interface PCB is being developed to replace the breadboard signal wiring and make the controller more compact and repeatable.

The PCB is intended to consolidate the Pico connections for the motor-driver logic, encoder, potentiometer, OLED, and direction switch. The 12 V high-current motor path remains on the BTS7960 screw terminals rather than being routed through the carrier board.

## What I learned

This project gave me hands-on experience with:

- Embedded C and the Raspberry Pi Pico SDK
- ADC input and setpoint mapping
- Hardware PWM
- GPIO interrupts
- Encoder calibration and RPM measurement
- Closed-loop motor control
- H-bridge direction control
- I2C peripheral communication
- Basic state/interlock logic
- Serial debugging of mixed hardware/software problems
- KiCad schematic and PCB design

## Next steps

The core controller is working. The next revision will focus on controlled deceleration during direction changes, cleaner mechanical packaging, final PCB integration, and additional controller tuning/testing.
