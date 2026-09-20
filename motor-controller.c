/*
 * Raspberry Pi Pico Closed-Loop DC Motor Controller
 *
 * Uses Hall encoder feedback to control the speed of a 12 V DC gearmotor.
 * A potentiometer sets the target RPM, while a BTS7960 drives the motor.
 * The system also includes forward/reverse control and a small OLED for
 * live motor data.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"


// --SYSTEM SETTINGS--

#define MAX_RPM 150
#define LOOP_TIME_MS 500

#define ADC_MAX 4095
#define PWM_MAX 4095
#define ADC_DEADZONE 100

// PWM correction added for each RPM of error.
#define CONTROL_GAIN 5

// Amount PWM drops each loop while changing direction.
#define DECEL_PWM_STEP 600


// --PIN MAPPING--

#define POT_ADC_PIN 26
#define POT_ADC_INPUT 0

#define R_EN_PIN 14
#define L_EN_PIN 15
#define RPWM_PIN 16
#define LPWM_PIN 17

#define HALL_A_PIN 18
#define HALL_B_PIN 19

#define DIR_SW_PIN 20

#define OLED_SDA_PIN 4
#define OLED_SCL_PIN 5
#define OLED_ADDR 0x3C
#define OLED_I2C_BAUD 400000


// test1 - 426, test2 - 400, test3 - 409, test4 - 406, test5 - 411 | Average - 410 pulses per revolution.
#define PULSES_PER_REV 410


// --MOTOR STATE--

typedef enum {
    MOTOR_READY,
    MOTOR_DECELERATING,
    MOTOR_WAITING_FOR_STOP
} motor_state_t;


// --HALL ENCODER--

// Updated by the Hall sensor interrupt whenever a rising edge is detected.
volatile uint32_t hall_pulses = 0;

void hall_callback(uint gpio, uint32_t events) {

    // Required by the Pico SDK callback format.
    (void)events;

    if (gpio == HALL_A_PIN) {
        hall_pulses++;
    }
}


// --OLED FUNCTIONS--

//create new C type and every variable of this type stores 1 character and 5 bytes describing how the character should look on the OLED
typedef struct {
    char character;
    uint8_t pixels[5];
} oled_glyph_t;


// Small 5x7 font containing the characters used by the display.
static const oled_glyph_t oled_font[] = {

    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {':', {0x00, 0x36, 0x36, 0x00, 0x00}},

    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},

    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
    {'W', {0x7F, 0x20, 0x18, 0x20, 0x7F}}
};


// Send one command byte to the SSD1306.
void oled_command(uint8_t command) {

    // 0x00 tells the display that the next byte is a command.
    uint8_t packet[2] = {
        0x00,
        command
    };

    i2c_write_blocking(
        i2c0,
        OLED_ADDR,
        packet,
        2,
        false
    );
}


// Set the page and column where the next OLED data will be written.
void oled_set_cursor(uint8_t page, uint8_t column) {

    oled_command(0xB0 | page);

    // SSD1306 column address is sent in two parts.
    oled_command(0x00 | (column & 0x0F));
    oled_command(0x10 | ((column >> 4) & 0x0F));
}


// Find the pixel pattern for a character.
const uint8_t *oled_get_glyph(char character) {

    int glyph_count =
        sizeof(oled_font) / sizeof(oled_font[0]);

    for (int i = 0; i < glyph_count; i++) {

        if (oled_font[i].character == character) {
            return oled_font[i].pixels;
        }
    }

    // Unsupported characters are shown as spaces.
    return oled_font[0].pixels;
}


// Convert a C string into pixel data and write one OLED line.
void oled_write_line(uint8_t page, const char *text) {

    // 128 display columns plus one control byte.
    uint8_t packet[129] = {0};

    // 0x40 means the following bytes are display data.
    packet[0] = 0x40;

    int position = 1;

    while (*text != '\0' && position <= 123) {

        const uint8_t *glyph =
            oled_get_glyph(*text);

        for (int i = 0; i < 5; i++) {
            packet[position++] = glyph[i];
        }

        // Leave one blank column between characters.
        packet[position++] = 0x00;

        text++;
    }

    oled_set_cursor(page, 0);

    i2c_write_blocking(
        i2c0,
        OLED_ADDR,
        packet,
        sizeof(packet),
        false
    );
}


// Clear the full 128x64 display.
void oled_clear(void) {

    uint8_t blank_page[129] = {0};

    blank_page[0] = 0x40;

    for (int page = 0; page < 8; page++) {

        oled_set_cursor(page, 0);

        i2c_write_blocking(
            i2c0,
            OLED_ADDR,
            blank_page,
            sizeof(blank_page),
            false
        );
    }
}


// Configure the SSD1306 when the Pico starts.
void oled_init(void) {

    sleep_ms(100);

    oled_command(0xAE);       // Display off during setup

    oled_command(0xD5);       // Display clock
    oled_command(0x80);

    oled_command(0xA8);       // Multiplex ratio
    oled_command(0x3F);       // 64-pixel display

    oled_command(0xD3);       // Display offset
    oled_command(0x00);

    oled_command(0x40);       // Start line

    oled_command(0x8D);       // Charge pump
    oled_command(0x14);

    oled_command(0x20);       // Memory addressing mode
    oled_command(0x02);       // Page addressing

    oled_command(0xA1);       // Segment remap
    oled_command(0xC8);       // COM scan direction

    oled_command(0xDA);       // COM configuration
    oled_command(0x12);

    oled_command(0x81);       // Contrast
    oled_command(0xCF);

    oled_command(0xD9);       // Pre-charge period
    oled_command(0xF1);

    oled_command(0xDB);       // VCOMH level
    oled_command(0x40);

    oled_command(0xA4);
    oled_command(0xA6);

    oled_command(0xAF);       // Display on

    oled_clear();
}


// --MAIN PROGRAM--

int main() {

    stdio_init_all();
    sleep_ms(2000);


    // --MOTOR DRIVER STARTUP--

    // Keep the BTS7960 disabled while the rest of the system initializes.
    gpio_init(R_EN_PIN);
    gpio_set_dir(R_EN_PIN, GPIO_OUT);
    gpio_put(R_EN_PIN, 0);

    gpio_init(L_EN_PIN);
    gpio_set_dir(L_EN_PIN, GPIO_OUT);
    gpio_put(L_EN_PIN, 0);


    // --ADC SETUP--

    adc_init();
    adc_gpio_init(POT_ADC_PIN);
    adc_select_input(POT_ADC_INPUT);


    // --OLED I2C SETUP--

    i2c_init(i2c0, OLED_I2C_BAUD);

    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);

    // I2C lines normally sit HIGH when no device is pulling them LOW.
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);

    oled_init();


    // --PWM SETUP--

    gpio_set_function(RPWM_PIN, GPIO_FUNC_PWM);
    gpio_set_function(LPWM_PIN, GPIO_FUNC_PWM);

    // GP16 and GP17 are on the same RP2040 PWM slice.
    uint slice_num =
        pwm_gpio_to_slice_num(RPWM_PIN);

    // Use the same 0-4095 range as the ADC.
    pwm_set_wrap(slice_num, PWM_MAX);

    pwm_set_gpio_level(RPWM_PIN, 0);
    pwm_set_gpio_level(LPWM_PIN, 0);

    pwm_set_enabled(slice_num, true);


    // --HALL SENSOR SETUP--

    gpio_init(HALL_A_PIN);
    gpio_set_dir(HALL_A_PIN, GPIO_IN);
    gpio_pull_up(HALL_A_PIN);

    gpio_init(HALL_B_PIN);
    gpio_set_dir(HALL_B_PIN, GPIO_IN);
    gpio_pull_up(HALL_B_PIN);

    // Hall A rising edges are used for RPM measurement.
    // Hall B is wired but is not needed for the current speed calculation.
    gpio_set_irq_enabled_with_callback(
        HALL_A_PIN,
        GPIO_IRQ_EDGE_RISE,
        true,
        &hall_callback
    );


    // --DIRECTION SWITCH SETUP--

    gpio_init(DIR_SW_PIN);
    gpio_set_dir(DIR_SW_PIN, GPIO_IN);
    gpio_pull_down(DIR_SW_PIN);


    // --CONTROL VARIABLES--

    uint32_t last_pulses = 0;

    // Stored outside the loop so the controller keeps its previous PWM value.
    int motor_pwm = 0;

    // Direction the motor is currently allowed to run.
    int active_forward =
        gpio_get(DIR_SW_PIN);

    motor_state_t motor_state =
        MOTOR_READY;


    // Everything is initialized, so the motor driver can now be enabled.
    gpio_put(R_EN_PIN, 1);
    gpio_put(L_EN_PIN, 1);


    // --MAIN LOOP--

    while (true) {


        // --READ POTENTIOMETER--

        uint16_t raw_adc =
            adc_read();

        int knob_percent =
            raw_adc * 100 / ADC_MAX;

        int target_rpm =
            raw_adc * MAX_RPM / ADC_MAX;

        // Small dead zone gives the knob a reliable zero-speed position.
        if (raw_adc < ADC_DEADZONE) {
            target_rpm = 0;
        }


        // Live direction request from the SPDT switch.
        int motor_forward =
            gpio_get(DIR_SW_PIN);


        // --ENCODER / RPM CALCULATION--

        uint32_t current_pulses =
            hall_pulses; //take a snap shot of the current pulse count right now.

        uint32_t pulses_this_loop =
            current_pulses - last_pulses; //new pulses since the previous loop.

        last_pulses =
            current_pulses; //save this snapshot for the next loop.

        uint32_t pulses_per_second =
            pulses_this_loop * 1000 / LOOP_TIME_MS; //adjusts for if we change sleep duration since its always per second.

        uint32_t actual_rpm =
            pulses_per_second * 60 / PULSES_PER_REV;


        // Signed because the motor can be either above or below the target speed.
        int32_t error =
            (int32_t)target_rpm -
            (int32_t)actual_rpm;


        // --CLOSED-LOOP CONTROL / DIRECTION CHANGE--

        /*
         * motor_forward is the direction requested by the switch.
         * active_forward is the direction currently being driven.
         *
         * When they are different, PWM is ramped down first.
         * The direction is only changed after the encoder confirms the
         * shaft has stopped.
         */
        if (motor_forward != active_forward) {

            if (motor_state == MOTOR_READY) {
                motor_state = MOTOR_DECELERATING;
            }


            if (motor_state == MOTOR_DECELERATING) {

                if (motor_pwm > DECEL_PWM_STEP) {

                    motor_pwm -= DECEL_PWM_STEP;
                }

                else {

                    motor_pwm = 0;
                    motor_state = MOTOR_WAITING_FOR_STOP;
                }
            }


            else if (motor_state == MOTOR_WAITING_FOR_STOP) {

                motor_pwm = 0;

                // No encoder pulses for a full loop means the shaft has stopped.
                if (pulses_this_loop == 0) {

                    active_forward =
                        motor_forward;

                    motor_state =
                        MOTOR_READY;
                }
            }
        }


        else {

            motor_state = MOTOR_READY;


            // A zero RPM target shuts the motor off immediately.
            if (target_rpm == 0) {

                motor_pwm = 0;
            }


            else {

                /*
                 * Incremental closed-loop correction.
                 *
                 * Positive error -> increase PWM
                 * Negative error -> decrease PWM
                 */
                motor_pwm =
                    motor_pwm +
                    error * CONTROL_GAIN;
            }
        }


        // Keep PWM inside its valid range.
        if (motor_pwm < 0) {
            motor_pwm = 0;
        }

        if (motor_pwm > PWM_MAX) {
            motor_pwm = PWM_MAX;
        }


        // --APPLY PWM / DIRECTION--

        /*
         * Forward:
         * RPWM = motor_pwm
         * LPWM = 0
         *
         * Reverse:
         * RPWM = 0
         * LPWM = motor_pwm
         */
        if (active_forward) {

            pwm_set_gpio_level(
                RPWM_PIN,
                motor_pwm
            );

            pwm_set_gpio_level(
                LPWM_PIN,
                0
            );
        }

        else {

            pwm_set_gpio_level(
                RPWM_PIN,
                0
            );

            pwm_set_gpio_level(
                LPWM_PIN,
                motor_pwm
            );
        }


        // --DIRECTION / STATUS TEXT--

        //direction text
        const char *direction_text;
        const char *requested_direction_text;
        const char *direction_status;


        if (active_forward) {
            direction_text = "FWD";
        }
        else {
            direction_text = "REV";
        }


        if (motor_forward) {
            requested_direction_text = "FWD";
        }
        else {
            requested_direction_text = "REV";
        }


        if (motor_state == MOTOR_DECELERATING) {
            direction_status = "DECELERATING";
        }

        else if (motor_state == MOTOR_WAITING_FOR_STOP) {
            direction_status = "WAITING";
        }

        else {
            direction_status = "READY";
        }


        // --OLED DISPLAY--

        char oled_line1[22];
        char oled_line2[22];
        char oled_line3[22];
        char oled_line4[22];


        // Turn the current motor values into strings for the OLED.
        snprintf(
            oled_line1,
            sizeof(oled_line1),
            "TGT:%d RPM",
            target_rpm
        );

        snprintf(
            oled_line2,
            sizeof(oled_line2),
            "ACT:%" PRIu32 " RPM",
            actual_rpm
        );

        snprintf(
            oled_line3,
            sizeof(oled_line3),
            "DIR:%s",
            direction_text
        );

        snprintf(
            oled_line4,
            sizeof(oled_line4),
            "PWM:%d",
            motor_pwm
        );


        // Spread the four lines evenly across the display.
        oled_write_line(0, oled_line1);
        oled_write_line(2, oled_line2);
        oled_write_line(4, oled_line3);
        oled_write_line(6, oled_line4);


        // --USB SERIAL TELEMETRY--

        //Print debugging info - PRIu32 is a macro for correct printf format for uint32_t
        printf(
            "Direction: %s | Requested: %s | Status: %s "
            "| Raw ADC: %u | Knob: %d%% "
            "| Target RPM: %d | Motor PWM: %d "
            "| Pulses: %" PRIu32
            " | Loop Pulses: %" PRIu32
            " | PPS: %" PRIu32
            " | Real RPM: %" PRIu32
            " | rpm_error: %" PRId32 "\n",

            direction_text,
            requested_direction_text,
            direction_status,
            raw_adc,
            knob_percent,
            target_rpm,
            motor_pwm,
            hall_pulses,
            pulses_this_loop,
            pulses_per_second,
            actual_rpm,
            error
        );


        // RPM measurement and control update happen every 500 ms.
        sleep_ms(LOOP_TIME_MS);
    }


    return 0;
}
