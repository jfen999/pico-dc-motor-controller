#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include <stdint.h>
#include <inttypes.h>


//defines are also helpful for kicad mapping
#define MAX_RPM 150
#define R_EN_PIN 14
#define L_EN_PIN 15
#define RPWM_PIN 16
#define LPWM_PIN 17
#define HALL_A_PIN 18
#define HALL_B_PIN 19
#define LOOP_TIME_MS 500
// test1 - 426, test2 - 400, test3 - 409, test4 - 406, test5 - 411 | Average - 410 pulses per revolution.
#define PULSES_PER_REV 410 
#define DIR_SW_PIN 20
#define OLED_SDA_PIN 4
#define OLED_SCL_PIN 5
#define OLED_ADDR 0x3C


volatile uint32_t hall_pulses = 0; //variable that counts the motor pulses when it spins. NOTE: 32 bits costs basically nothing on the pico and pulses can rack up.

void hall_callback(uint gpio, uint32_t events){
    if(gpio == HALL_A_PIN){
        hall_pulses++;
    }
}

// --OLED FUNCTIONS--

typedef struct {
    char character;
    uint8_t pixels[5];
} oled_glyph_t;

// Small 5x7 font containing the characters used on the display
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

void oled_command(uint8_t command) {
    uint8_t packet[2] = {0x00, command};

    i2c_write_blocking(
        i2c0,
        OLED_ADDR,
        packet,
        2,
        false
    );
}

void oled_set_cursor(uint8_t page, uint8_t column) {
    oled_command(0xB0 | page);
    oled_command(0x00 | (column & 0x0F));
    oled_command(0x10 | ((column >> 4) & 0x0F));
}

const uint8_t *oled_get_glyph(char character) {
    int glyph_count = sizeof(oled_font) / sizeof(oled_font[0]);

    for (int i = 0; i < glyph_count; i++) {
        if (oled_font[i].character == character) {
            return oled_font[i].pixels;
        }
    }

    return oled_font[0].pixels;
}

void oled_write_line(uint8_t page, const char *text) {
    uint8_t packet[129] = {0};

    packet[0] = 0x40;

    int position = 1;

    while (*text != '\0' && position <= 123) {
        const uint8_t *glyph = oled_get_glyph(*text);

        for (int i = 0; i < 5; i++) {
            packet[position++] = glyph[i];
        }

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

void oled_init(void) {
    sleep_ms(100);

    oled_command(0xAE);

    oled_command(0xD5);
    oled_command(0x80);

    oled_command(0xA8);
    oled_command(0x3F);

    oled_command(0xD3);
    oled_command(0x00);

    oled_command(0x40);

    oled_command(0x8D);
    oled_command(0x14);

    oled_command(0x20);
    oled_command(0x02);

    oled_command(0xA1);
    oled_command(0xC8);

    oled_command(0xDA);
    oled_command(0x12);

    oled_command(0x81);
    oled_command(0xCF);

    oled_command(0xD9);
    oled_command(0xF1);

    oled_command(0xDB);
    oled_command(0x40);

    oled_command(0xA4);
    oled_command(0xA6);

    oled_command(0xAF);

    oled_clear();
}

int main() {
    stdio_init_all();// Start USB serial so printf works
    sleep_ms(2000);// Wait 2 seconds so serial monitor has time to connect

    //--ADC SETUP--
    adc_init();// Start the ADC system
    adc_gpio_init(26);// Tell Pico that GP26 is being used as an ADC pin
    adc_select_input(0);// Select ADC input 0, which matches GP26_A0

    // --OLED I2C SETUP--
    i2c_init(i2c0, 400 * 1000);

    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);

    oled_init();

    //--DRIVER SETUP--
    gpio_init(R_EN_PIN); //prepare pin
    gpio_set_dir(R_EN_PIN, GPIO_OUT); //make the pin an output
    gpio_put(R_EN_PIN, 1); //set the pin HIGH or LOW

    gpio_init(L_EN_PIN);
    gpio_set_dir(L_EN_PIN, GPIO_OUT);   
    gpio_put(L_EN_PIN, 1);

    // --PWM SETUP--
    gpio_set_function(RPWM_PIN, GPIO_FUNC_PWM); //let PWM hardware control GP16, connected to BTS7960 RPWM
    gpio_set_function(LPWM_PIN, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(RPWM_PIN); //find the slice inside the PWM hardware connected to GP16

    pwm_set_wrap(slice_num, 4095); //set PWM controller max count to 4095 to parallel raw adc

    pwm_set_gpio_level(RPWM_PIN, 0);
    pwm_set_gpio_level(LPWM_PIN, 0);

    pwm_set_enabled(slice_num, true); //turn that PWM controller on

    // --HALL SENSOR SETUP--
    gpio_init(HALL_A_PIN);
    gpio_set_dir(HALL_A_PIN, GPIO_IN);
    gpio_pull_up(HALL_A_PIN); //need the pullup just in case the hall sensor is a ground release type and doesnt fully send a 3.3V signal to count a pulse.

    gpio_init(HALL_B_PIN);
    gpio_set_dir(HALL_B_PIN, GPIO_IN);
    gpio_pull_up(HALL_B_PIN);

    gpio_set_irq_enabled_with_callback(HALL_A_PIN, GPIO_IRQ_EDGE_RISE, true, &hall_callback); //line for interrupt "watch gp18 and if gp18 changes run callback"
    
    uint32_t last_pulses = 0;
    int motor_pwm = 0;
    

    // --DIRECTION SWITCH SETUP--
    gpio_init(DIR_SW_PIN);
    gpio_set_dir(DIR_SW_PIN, GPIO_IN);
    gpio_pull_down(DIR_SW_PIN);

    int active_forward = gpio_get(DIR_SW_PIN);

    //--MAIN LOOP--
    while (true) {
        //read potentiometer voltage as a raw ADC number
        uint16_t raw_adc = adc_read();

        //convert raw ADC into useful values we can use
        int knob_percent = raw_adc * 100 / 4095;
        int target_rpm = raw_adc * MAX_RPM / 4095;

        if (raw_adc < 100){
            target_rpm = 0;
        }
        
        int motor_forward = gpio_get(DIR_SW_PIN);

        uint32_t current_pulses = hall_pulses; //take a snap shot of the current pulse count right now.
        uint32_t pulses_this_loop = current_pulses - last_pulses; //new pulses since the previous loop.
        last_pulses = current_pulses; //save this snapshot for the next loop.

        uint32_t pulses_per_second = pulses_this_loop * 1000 / LOOP_TIME_MS; //adjusts for if we change sleep duration since its always per second.
        uint32_t actual_rpm = pulses_per_second * 60 / PULSES_PER_REV;

        int32_t error = (int32_t)target_rpm - (int32_t)actual_rpm;

        if (motor_forward != active_forward){
            //direction change requested
            motor_pwm = 0;

            if (pulses_this_loop == 0){
                active_forward = motor_forward;
            }
        } else if(target_rpm == 0){
                motor_pwm = 0;
        } else {
            motor_pwm = motor_pwm + error * 5;
        }

        if (motor_pwm < 0){
            motor_pwm = 0;
        }

        if (motor_pwm > 4095){
            motor_pwm = 4095;
        }

        if (active_forward){
            pwm_set_gpio_level(RPWM_PIN, motor_pwm);
            pwm_set_gpio_level(LPWM_PIN, 0);
        } else {
            pwm_set_gpio_level(RPWM_PIN, 0);
            pwm_set_gpio_level(LPWM_PIN, motor_pwm);
        }

        //direction text
        const char *direction_text;
        const char *direction_status;

        if(active_forward){
            direction_text = "FWD";
        } else {
            direction_text = "REV";
        }

        if (motor_forward != active_forward){
            direction_status = "CHANGING";
        } else {
            direction_status = "READY";
        }

        // --OLED DISPLAY--
        char oled_line1[22];
        char oled_line2[22];
        char oled_line3[22];
        char oled_line4[22];

        snprintf(oled_line1, sizeof(oled_line1),
             "TGT:%d RPM", target_rpm);

        snprintf(oled_line2, sizeof(oled_line2),
           "ACT:%" PRIu32 " RPM", actual_rpm);

        snprintf(oled_line3, sizeof(oled_line3),
            "DIR:%s", direction_text);

        snprintf(oled_line4, sizeof(oled_line4),
            "PWM:%d", motor_pwm);

        oled_write_line(0, oled_line1);
        oled_write_line(2, oled_line2);
        oled_write_line(4, oled_line3);
        oled_write_line(6, oled_line4);

        //Print debugging info - PRIu32 is a macro for correct printf format for uint32_t
        printf("Direction: %s | Status: %s | Raw ADC: %u | Knob: %d%% | Target RPM: %d | Motor PWM: %d | Pulses: %" PRIu32 " | Loop Pulses: %" PRIu32 " | PPS: %" PRIu32 " | Real RPM: %" PRIu32
       " | rpm_error: %" PRId32 "\n",      
       direction_text, direction_status, raw_adc, knob_percent, target_rpm, motor_pwm, hall_pulses, pulses_this_loop, pulses_per_second, actual_rpm, error);

        sleep_ms(LOOP_TIME_MS);
    }

    return 0;
}
