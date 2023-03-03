#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

#include "wifi-info.h"

#define NUM_LEDS 7

int led_pin_array[NUM_LEDS] = {2,3,4,5,6,7,8};
int led_status_array[NUM_LEDS] = {LOW};

char ssid[] = SSID;
char pw[] = PW;
int wifi_status = WL_IDLE_STATUS;

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

    Serial.print("Setting up WIFI connection.\n");
    if(WiFi.status() == WL_NO_MODULE){
        Serial.print("Can't seem to find the wifi module :( \n");
        digitalWrite(led_pin_array[0], HIGH);
        digitalWrite(led_pin_array[6], HIGH);
        while(true);
    }

    int counter = 0;

    while(wifi_status != WL_CONNECTED) {
        Serial.print("Connecting to InConcert access point.\n");
        wifi_status = WiFi.begin(ssid, 0, pw);
        all_leds_off();
        
        while(counter < 10){
            led_status_array[3] = HIGH;
            refresh_leds();
            delay(500);
            led_status_array[3] = LOW;
            refresh_leds();
            delay(500);
            counter++;
        }
        counter = 0;
    }
    Serial.print("Connected!\n");
    all_leds_off();
    while(counter < 3){
        led_status_array[2] = HIGH;
        led_status_array[4] = HIGH;
        refresh_leds();
        delay(500);
        led_status_array[2] = LOW;
        led_status_array[4] = LOW;
        refresh_leds();
        delay(500);
        counter++;
    }
    counter = 0;
}

void all_leds_off() {
    for(int i = 0; i < NUM_LEDS; i++){
        led_status_array[i] = LOW;
        digitalWrite(led_pin_array[i], led_status_array[i]);
    }
}

void all_leds_on() {
    for(int i = 0; i < NUM_LEDS; i++){
        led_status_array[i] = HIGH;
        digitalWrite(led_pin_array[i], led_status_array[i]);
    }
}


void refresh_leds() {
    for(int i = 0; i < NUM_LEDS; i++){
        digitalWrite(led_pin_array[i], led_status_array[i]);
    }
}

int active_led = 0; //Which led is active 
int direction = 1; //Direction of movement
int interval = 50; //Time between movements
int bpm = 120;
unsigned long next_movement = millis() + interval;
void loop() {

    interval = (60000/bpm) / (NUM_LEDS - 1);
    
    if(millis() > next_movement) {
        // move led back and forth
        led_status_array[active_led] = LOW;
        if(active_led < NUM_LEDS && active_led >= 0) {
            active_led += direction;
        } 
        if(active_led == 0){
            direction = 1;
        } 
        if(active_led == (NUM_LEDS - 1)) {
            direction = -1;
        }
        led_status_array[active_led] = HIGH;
        refresh_leds();
        next_movement += interval;
    }
    
}
