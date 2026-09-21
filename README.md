# Raspberry Pi Pico Closed-Loop DC Motor Controller

A closed-loop speed controller for a 12 V brushed DC gearmotor built around a Raspberry Pi Pico, BTS7960 H-bridge, Hall-effect encoder, potentiometer, direction switch, and SSD1306 OLED.

I built this project to get hands-on experience with embedded motor control beyond basic open-loop PWM. The Pico reads a requested speed from the potentiometer, measures the motor's actual speed from the encoder, calculates the RPM error, and adjusts the PWM command to bring the motor toward the target speed.

The controller also supports forward/reverse operation, controlled deceleration before reversing, live OLED data, and USB serial telemetry for debugging.

## What it does

- Reads a potentiometer through the Pico ADC and maps it to a 0-150 RPM target
- Counts Hall encoder pulses using a GPIO interrupt
- Calculates motor RPM every 500 ms
- Adjusts PWM using closed-loop speed feedback
- Drives the motor in both directions through a BTS7960 H-bridge
- Uses a physical SPDT switch for forward/reverse control
- Ramps PWM down before reversing and waits for the motor to stop
- Displays target RPM, actual RPM, direction, and PWM on an SSD1306 OLED
- Sends detailed telemetry over USB serial for testing and debugging

## System overview

```text
Potentiometer -> Pico ADC -> Target RPM

Hall Encoder -> GPIO Interrupt -> Actual RPM

Target RPM - Actual RPM -> Speed Error -> PWM Correction -> BTS7960 -> DC Motor

Direction Switch -> Pico -> Direction / Reversal Logic

Pico -> I2C -> SSD1306 OLED
```

The potentiometer sets a target speed instead of directly setting PWM. The encoder closes the loop by giving the Pico an actual RPM measurement to compare against that target.

## Hardware

- Raspberry Pi Pico / RP2040
- 12 V brushed DC gearmotor with Hall encoder
- BTS7960 / IBT-2 H-bridge motor driver
- 10 kΩ potentiometer
- 0.96 inch 128x64 SSD1306 I2C OLED
- SPDT direction switch
- External 12 V motor supply
- Breadboard and jumper wiring for the current prototype

The Pico is powered over USB while the motor uses the external 12 V supply. The Pico and motor driver share a common ground.

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
| GP19 | Hall B | Second encoder channel |
| GP20 | Direction switch | Forward/reverse request |
| GP26 / ADC0 | Potentiometer wiper | Speed setpoint |

## Encoder feedback

Motor speed is measured from Hall A using rising-edge interrupts.

I calibrated the encoder at the output shaft by rotating the shaft one full revolution several times and recording the pulse count. The average came out to about 410 pulses per output-shaft revolution, so the firmware uses:

```c
#define PULSES_PER_REV 410
```

Every 500 ms, the program checks how many new pulses were counted, converts that value to pulses per second, and then calculates RPM.

Hall B is connected but is not currently needed for speed measurement.

## Closed-loop speed control

The potentiometer is mapped to a target speed from 0-150 RPM.

The controller compares that target with the measured motor speed:

```text
error = target RPM - actual RPM
```

The PWM command is then adjusted using:

```c
motor_pwm = motor_pwm + error * CONTROL_GAIN;
```

with:

```c
#define CONTROL_GAIN 5
```

A positive error increases PWM and a negative error decreases it. The PWM command is limited to the 12-bit range of 0-4095.

This is a simple incremental closed-loop controller, not a full PID controller. During testing, the motor was able to settle near the requested speed without needing to run at full PWM.

## Direction control

The BTS7960 uses separate PWM inputs for each direction:

```text
Forward:  RPWM = motor_pwm, LPWM = 0
Reverse:  RPWM = 0,         LPWM = motor_pwm
```

The SPDT switch on GP20 sets the requested direction.

Instead of switching direction immediately while the motor is still spinning, the firmware uses a small state machine. When the requested direction changes, PWM is ramped down first. The controller then waits for a full measurement window with no encoder pulses before changing the active direction and resuming closed-loop control.

The states are:

```text
READY -> DECELERATING -> WAITING_FOR_STOP -> READY
```

This keeps the reversal behavior more controlled and avoids commanding the H-bridge straight into the opposite direction while the shaft is still moving.

## OLED display

The 128x64 SSD1306 OLED is connected over I2C and shows:

```text
TGT: target RPM
ACT: actual RPM
DIR: FWD / REV
PWM: current PWM command
```

The display interface is written directly in C using the Pico SDK I2C functions. The firmware also includes a small 5x7 bitmap font for the characters used on the screen.

## Serial debugging

USB serial output was one of the main tools I used while building and debugging the controller.

The telemetry includes:

- active and requested direction
- controller state
- raw ADC value
- potentiometer percentage
- target RPM
- PWM command
- encoder pulse counts
- pulses per second
- actual RPM
- RPM error

This was especially useful for separating hardware problems from software problems. For example, an unstable potentiometer connection caused the ADC reading to jump unexpectedly, and the serial data made it clear that the controller was responding correctly to a bad input signal rather than failing internally.

## Current status

The main breadboard prototype is working.

- [x] Potentiometer speed setpoint
- [x] PWM motor control
- [x] Hall encoder feedback
- [x] RPM measurement
- [x] Closed-loop speed regulation
- [x] Forward and reverse control
- [x] Physical direction switch
- [x] Controlled deceleration before reversal
- [x] Encoder-based stop confirmation
- [x] SSD1306 OLED telemetry
- [x] USB serial debugging
- [ ] Final mechanical mounting / wire cleanup
- [ ] Carrier PCB assembly

## PCB

I am also designing a KiCad carrier/interface PCB to replace the breadboard signal wiring and make the system cleaner and more repeatable.

The carrier board connects the Pico to the BTS7960 control signals, encoder, potentiometer, OLED, and direction switch. The high-current 12 V motor path stays on the BTS7960 screw terminals instead of being routed through the carrier PCB.

## Skills used

- Embedded C
- Raspberry Pi Pico SDK
- RP2040
- ADC
- Hardware PWM
- GPIO interrupts
- Hall-effect encoder feedback
- Closed-loop control
- H-bridge motor control
- I2C
- SSD1306 display interfacing
- State-machine logic
- Serial debugging
- KiCad schematic and PCB design
- Hardware/software integration

## Next steps

The control system is complete. The remaining work is mainly physical: clean up the prototype, finish the carrier PCB, and do additional testing and controller tuning.
