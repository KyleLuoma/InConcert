#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

#include "wifi-info.h"
#include "inconcert-communication.h"

#define NUM_LEDS 7
#define UDP_READ_TIMEOUT 4
#define BUTTON_IGNORE_MS 500

//int led_pin_array[NUM_LEDS] = {3,4,5,6,7};
int led_pin_array[NUM_LEDS] = {2,3,4,5,6,7,8};
int led_status_array[NUM_LEDS] = {LOW};

int buzzer_pin = 9;
int button_pin = 10;
int button_state = LOW;
unsigned long ignore_button_until = 0;
int alert_led_pin = 11;

char ssid[] = SSID;
char pw[] = PW;
int wifi_status = WL_IDLE_STATUS;

IPAddress AGGServer(10, 42, 0, 1);
WiFiUDP udp_in;

char incoming_udp_buffer[255];
struct tempo_message latest_tempo_message;
struct time_message latest_time_message;


void init_tempo_message(struct tempo_message * message) {
    message->message_type = TEMPO;
    message->device_id = 0;
    message->bpm = 120;
    message->confidence = 100;
    message->measure = 0;
    message->beat = 0;
}

void init_time_message(struct time_message * message) {
    message->message_type = TIME;
    message->device_id = 0;
    message->beat_signature_L = 4;
    message->beat_signature_R = 4;
    message->measure = 0;
    message->beat = 0;
    message->beat_interval = (60000 / 120);
}

void setup() {   

    pinMode(buzzer_pin, OUTPUT);
    digitalWrite(buzzer_pin, LOW);
    pinMode(button_pin, INPUT);
    pinMode(alert_led_pin, OUTPUT);
    digitalWrite(alert_led_pin, HIGH);

//    beep(int freq, unsigned long duration_ms)
    for(int i = 100; i < 700; i += 50){
      beep(i, 200);
    }
  
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
    if(udp_in.begin(BROADCAST_PORT) == 0) {
        Serial.print("Unable to bind receive socket\n");
        all_leds_off();
        led_status_array[0] = HIGH;
        led_status_array[6] = HIGH;
        refresh_leds();
    }
    init_tempo_message(&latest_tempo_message);
    init_time_message(&latest_time_message);
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

uint32_t get_uint32_from_charbuffer(char * charbuffer) {
  uint32_t return_val = ((unsigned int)charbuffer[0] << 24) +
                        ((unsigned int)charbuffer[1] << 16) +
                        ((unsigned int)charbuffer[2] << 8)  +
                        ((unsigned int)charbuffer[3]);
  return return_val;           
}

unsigned long get_unsigned_long_from_buffer(char * receive_buffer, int long_ix) {
  int offset = long_ix * 4;
  unsigned long high_word = word(receive_buffer[offset], receive_buffer[offset + 1]);
  unsigned long low_word = word(receive_buffer[offset + 2], receive_buffer[offset + 3]);
  unsigned long message_type = high_word << 16 | low_word;
  return htonl(message_type);
}


//Same as retrieve_udp_packet, but uses read approach found here:
//https://docs.arduino.cc/tutorials/communication/wifi-nina-examples?_gl=1*1216s9b*_ga*MTcyNTg5Nzc4LjE2NzQ1ODI4MjQ.*_ga_NEXN8H46L5*MTY3ODEyMTc5Ni40Mi4xLjE2NzgxMjIyNDIuMC4wLjA.#wifinina-udp-ntp-client
int retrieve_udp_packet_using_word(
    char * receive_buffer, 
    struct tempo_message * tempo, 
    struct time_message * time
) {
   int packet_size = 0;
   packet_size = udp_in.parsePacket();
   if(packet_size > 0){
      udp_in.read(receive_buffer, 255);
      unsigned long message_type = get_unsigned_long_from_buffer(receive_buffer, 0);
//      Serial.print("Using word the message type is: ");
//      Serial.print(message_type);
//      Serial.print("\n");
      if(message_type == TIME) {
        time->message_type = message_type;
        time->device_id = get_unsigned_long_from_buffer(receive_buffer, 1);
        time->beat_signature_L = get_unsigned_long_from_buffer(receive_buffer, 2);
        time->beat_signature_R = get_unsigned_long_from_buffer(receive_buffer, 3);
        time->measure = get_unsigned_long_from_buffer(receive_buffer, 4);
        time->beat = get_unsigned_long_from_buffer(receive_buffer, 5);
//        time->beat_interval = get_unsigned_long_from_buffer(receive_buffer, 6);
//        Serial.print("\ndevice_id: "); Serial.print(time->device_id);
//        Serial.print("\nb_sL: "); Serial.print(time->beat_signature_L);
//        Serial.print("\nb_sR: "); Serial.print(time->beat_signature_R);
//        Serial.print("\nMeasure: "); Serial.print(time->measure);
//        Serial.print("\nBeat: "); Serial.print(time->beat);
//        Serial.print("\nInterval: "); Serial.print(time->beat_interval);Serial.print("\n");
      } else if (message_type == TEMPO) {
        tempo->message_type = message_type;
        tempo->device_id = get_unsigned_long_from_buffer(receive_buffer, 1);
        tempo->bpm = get_unsigned_long_from_buffer(receive_buffer, 3);
        tempo->confidence = get_unsigned_long_from_buffer(receive_buffer, 4);
        tempo->measure = get_unsigned_long_from_buffer(receive_buffer, 5);
        tempo->beat = get_unsigned_long_from_buffer(receive_buffer, 6);
        Serial.println("Tempo Struct Fields and Values:");
        Serial.print("message_type: "); Serial.println(tempo->message_type);
        Serial.print("device_id: "); Serial.println(tempo->device_id);
        Serial.print("bpm: "); Serial.println(tempo->bpm);
        Serial.print("confidence: "); Serial.println(tempo->confidence);
        Serial.print("measure: "); Serial.println(tempo->measure);
        Serial.print("beat: "); Serial.println(tempo->beat);
      } else if (message_type == EVENT) {
        Serial.println("Event Struct Fields and Values:");
        Serial.print("message_type: "); Serial.println(get_unsigned_long_from_buffer(receive_buffer, 0));
        Serial.print("device_id: "); Serial.println(get_unsigned_long_from_buffer(receive_buffer, 1));
        Serial.print("event_type: "); Serial.println(get_unsigned_long_from_buffer(receive_buffer, 2));
        Serial.print("measure: "); Serial.println(get_unsigned_long_from_buffer(receive_buffer, 3));
        Serial.print("beat: "); Serial.println(get_unsigned_long_from_buffer(receive_buffer, 4));
        Serial.print("Num params: "); Serial.println(get_unsigned_long_from_buffer(receive_buffer, 5));
        unsigned long num_params = (get_unsigned_long_from_buffer(receive_buffer, 5));
        for(int i = 0; i < num_params; i++){
          Serial.print("  Param "); Serial.print(i); Serial.print(": "); Serial.println(get_unsigned_long_from_buffer(receive_buffer, 6 + i));
        }
      }
      if(message_type >= 0 && message_type < 4) {
        return message_type;
      }
   }
   return -1;
}


//Checks for incomingudp packet and updates appropriate message struct
//returns message type or -1 if no message received.
int retrieve_udp_packet(char * receive_buffer, struct tempo_message * tempo, struct time_message * time) {
    int packet_size = 0;
    memset(receive_buffer, 0, 255);
    unsigned long timeout = millis() + UDP_READ_TIMEOUT;
    packet_size = udp_in.parsePacket();
    if(packet_size > 0) {
        udp_in.read((unsigned char*)receive_buffer, 255);
        uint32_t message_type = htonl(get_uint32_from_charbuffer(receive_buffer));

        char* buffer_pointer = &receive_buffer[0];

        if(message_type == TIME) {
            time->message_type     = message_type;
            time->device_id        = htonl(get_uint32_from_charbuffer(buffer_pointer+4));
            time->beat_signature_L = htonl(get_uint32_from_charbuffer(buffer_pointer+8));
            time->beat_signature_R = htonl(get_uint32_from_charbuffer(buffer_pointer+12));
            time->measure          = htonl(get_uint32_from_charbuffer(buffer_pointer+16));
            time->beat             = htonl(get_uint32_from_charbuffer(buffer_pointer+20));
            time->beat_interval    = htonl(get_uint32_from_charbuffer(buffer_pointer+24));
        } else if (message_type == TEMPO) {
            tempo->message_type    = message_type;
            tempo->device_id       = htonl(get_uint32_from_charbuffer(buffer_pointer+4));
            tempo->bpm             = htonl(get_uint32_from_charbuffer(buffer_pointer+8));
            tempo->confidence      = htonl(get_uint32_from_charbuffer(buffer_pointer+12));
            tempo->measure         = htonl(get_uint32_from_charbuffer(buffer_pointer+16));
            tempo->beat            = htonl(get_uint32_from_charbuffer(buffer_pointer+20));
        }

        if(message_type >= 0 && message_type < 4){
            return message_type;
        }
    }
    return -1;
}

void beep(int freq, unsigned long duration_ms) {
  unsigned long cycle_ms = 1000 / freq;
  duration_ms += millis();
  while(millis() < duration_ms){
    digitalWrite(buzzer_pin, HIGH);
    delay(cycle_ms);
    digitalWrite(buzzer_pin, LOW);
    delay(cycle_ms);
  }
}

int direction = 1; //Direction of movement

int bpm = 120;
int r_signature = 4;
int interval = (60000/120) / r_signature; //Time between movements
int left_bound = 1;
int right_bound = 5;
int last_message_type = -1;
unsigned long next_movement = millis() + interval;
int active_led = left_bound; //Which led is active 
int tempo_updated = 0;
int output_mode = 0; //0: audio and light; 1: light only; 2: audio only; 3: off

void loop() {

    if(ignore_button_until < millis()){
      button_state = digitalRead(button_pin);
      if(button_state == LOW){
        if(output_mode == 3){
          output_mode = 0;
        } else {
          output_mode++;
        }
        Serial.print("Button press detected\n");
      }
      ignore_button_until = millis() + BUTTON_IGNORE_MS;
    }
    

    if(millis() > next_movement) {
        // move led back and forth
        led_status_array[active_led] = LOW;
        if(active_led <= right_bound && active_led >= left_bound) {
            active_led += direction;
        } 
        if(active_led <= left_bound) {
            direction = 1;
//            Serial.print("Tick.\n");
            if(r_signature == 4){
                led_status_array[0] = HIGH;
                led_status_array[6] = LOW;
            }
            if(output_mode == 0 || output_mode == 2){
                beep(600, 25);
            }
                
        } 
        if(active_led >= right_bound) {
            direction = -1;
//            Serial.print("Tock.\n");
            if(r_signature == 4){
                led_status_array[6] = HIGH;
                led_status_array[0] = LOW; 
            }
            if(output_mode == 0 || output_mode == 2){
                beep(500, 25);
            }
        }
        led_status_array[active_led] = HIGH;
        if(output_mode > 1){
          all_leds_off();
        }
        
        refresh_leds();
        
        next_movement += interval;
    }

    // retrieve_udp_packet_using_word(
    //   incoming_udp_buffer,
    //   &latest_tempo_message,
    //   &latest_time_message
    // );
    
    last_message_type = retrieve_udp_packet(
      incoming_udp_buffer, 
      &latest_tempo_message, 
      &latest_time_message
    );

    if(last_message_type == EVENT){
      Serial.print("Received event message\n");
    }

    if(last_message_type == TIME){
      if(tempo_updated == 1){
          next_movement = millis() + interval;
          tempo_updated = 0;
      }
      
      if(latest_time_message.beat_signature_R != r_signature){
        r_signature = latest_time_message.beat_signature_R;
        if(r_signature == 4 || r_signature == 8){
          left_bound = 1;
          right_bound = 5;
        } else if(r_signature == 3 || r_signature == 6) {
          left_bound = 0;
          right_bound = 6;
        }
      }
      
      // Serial.print("Received time message.\n");
    }
    if(last_message_type == TEMPO && latest_tempo_message.bpm > 0) {
      interval = (60000/latest_tempo_message.bpm) / r_signature;
      tempo_updated = 1;
      if(r_signature == 6 || r_signature == 8) {
        interval = interval / 2;
      }
      Serial.print("Received tempo message.\n");
      beep(200, 50);
      beep(300, 50);
    }
    last_message_type = -1;
}
