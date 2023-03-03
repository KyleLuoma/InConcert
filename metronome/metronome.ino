#include <SPI.h>

#define NUM_LEDS 7

int led_pin_array[NUM_LEDS] = {2,3,4,5,6,7,8};
int led_status_array[NUM_LEDS] = {LOW};

void setup() {
    Serial.print("Initializing LED array.\n");
    for(int i = 0; i < NUM_LEDS; i++){
        pinMode(led_pin_array[i], OUTPUT);
        led_status_array[i] = HIGH;
        digitalWrite(led_pin_array[i], led_status_array[i]);
        delay(100);
        led_status_array[i] = LOW;
        digitalWrite(led_pin_array[i], led_status_array[i]);
        delay(100);
    }
}

void loop() {
    for(int i = 0; i < NUM_LEDS; i++){
        led_status_array[i] = HIGH;
        digitalWrite(led_pin_array[i], led_status_array[i]);
        delay(100);
        led_status_array[i] = LOW;
        digitalWrite(led_pin_array[i], led_status_array[i]);
        delay(100);
    }
}