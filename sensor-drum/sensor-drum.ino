#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>
#include <RTCZero.h>

#include "wifi-info.h"
#include "inconcert-communication.h"

#define SNARE 0
#define BASS 1
#define CYMBAL 2
#define TOM 3

#define DEVICE_ID 1001

#define SAMPLE_INTERVAL 2  //MS between sensor samples (x4 samples)
#define INIT_CYCLES 100    //Number of sample cycles to perform to evaluate baseline
#define INPUT_THRESHOLD 50 //Amount of signal to increase above idle to register as a hit
#define IGNORE_PERIOD 30 //Number of samples to ignore after a hit detection
#define UDP_READ_TIMEOUT 1000 //Number of ms until timing out on UDP read

// #define SERIAL_PRINT

Adafruit_ADS1115 ads;

int16_t snare, cymbal, bass, tom; //Use for storing sample inputs
int16_t snare_th, cymbal_th, bass_th, tom_th;//Input thresholds
const int ledPin = LED_BUILTIN; 
int led_state = LOW;
int has_wifi = 1;
int wifi_status;
int i;
int16_t cur_measure, cur_beat;

uint32_t tempo_msg_buffer[5];
uint32_t incoming_udp_buffer[24]; //big enough to handle time or event messages
uint32_t registration_message_buffer[2];

struct time_message current_time;

IPAddress AGGServer(10, 42, 0, 1); // Aggregator wifi hotspot address
WiFiUDP udp_out;
IPAddress broadcast_ip(10, 42, 0, 255); 
WiFiUDP udp_in;
const int AGG_PACKET_SIZE = 255;

void setup() {
  Serial.begin(9600);
  Serial.print("Initializing drum-sensor runtime\n"); 

  if(!ads.begin()) {
    Serial.print("Unable to start ADS. Check the connections and try again.");
  }

  if(WiFi.status() == WL_NO_SHIELD) {
    Serial.print("WiFi shield not detected, continuing without wifi connectivity.\n");
    has_wifi = 0;
  }

  if(has_wifi == 1){
    Serial.print("Connecting to aggregator.\n");
    while(wifi_status != WL_CONNECTED) {
      wifi_status = WiFi.begin(SSID, PW);
      for(i = 0; i < 10; i++){ //Flash LED asymmetrically for ten seconds while we wait to connect
        digitalWrite(ledPin, HIGH);
        delay(500);
        digitalWrite(ledPin, LOW);
        delay(300);
        digitalWrite(ledPin, HIGH);
        delay(100);
        digitalWrite(ledPin, LOW);
        delay(100);
      }
    }
    Serial.println("\nStarting connection to server...");
    if(udp_out.begin(SEND_PORT)==0){
      Serial.print("Unable to bind to send socket.\n");
    } 
    if(udp_in.begin(BROADCAST_PORT)==0){
      Serial.print("Unable to bind receive port.\n");
    };
  }

  pinMode(ledPin, OUTPUT);

  int16_t max_s = 0;
  int16_t max_b = 0;
  int16_t max_c = 0;
  int16_t max_tom = 0;
  //document baseline signal - no events
  //user should not hit drums during initialization period
  for(int i = 0; i < INIT_CYCLES; i++) {
    if(led_state == LOW){
      led_state = HIGH;
    } else {
      led_state = LOW;
    }
    digitalWrite(ledPin, led_state);
    snare = ads.readADC_SingleEnded(SNARE);
    if (snare > max_s){
      max_s = snare;
    }
    delay(SAMPLE_INTERVAL);
    bass = ads.readADC_SingleEnded(BASS);
    if(bass > max_b){
      max_b = bass;
    }
    delay(SAMPLE_INTERVAL);
    digitalWrite(ledPin, HIGH);
    cymbal = ads.readADC_SingleEnded(CYMBAL);
    if(cymbal > max_c){
      max_c = cymbal;
    }
    delay(SAMPLE_INTERVAL);
    tom = ads.readADC_SingleEnded(TOM);
    if(tom > max_tom){
      max_tom = tom;
    }
    delay(SAMPLE_INTERVAL);    
  }
  snare_th = max_s + INPUT_THRESHOLD;
  cymbal_th = max_c + 10;
  bass_th = max_b + INPUT_THRESHOLD;
  tom_th = max_tom + INPUT_THRESHOLD;
}

unsigned long loop_start_time;
unsigned long next_sample;

unsigned long registerWithHost(IPAddress& address) {
  Serial.print("Sending registration message to Agg Server ");
  Serial.println(AGGServer);
  memset(registration_message_buffer, 0, sizeof(struct register_client_message));
  registration_message_buffer[0] = htonl(REGISTER_CLIENT);
  registration_message_buffer[1] = htonl(DEVICE_ID);
  udp_out.beginPacket(address, SEND_PORT);
  udp_out.write((uint8_t*)registration_message_buffer, sizeof(struct register_client_message));
  udp_out.endPacket();
}

unsigned long sendAGGpacket(IPAddress& address) {
  Serial.print("Sending Packet to Agg Server ");
  Serial.println(AGGServer);

  memset(tempo_msg_buffer, 0, sizeof(struct tempo_message));

  tempo_msg_buffer[0] = htonl(0);            //message type
  tempo_msg_buffer[1] = htonl(DEVICE_ID);    //device id
  tempo_msg_buffer[2] = htonl(120);          //tempo
  tempo_msg_buffer[3] = htonl(94);           //confidence
  tempo_msg_buffer[5] = htonl(10001);        //timestamp

  udp_out.beginPacket(address, SEND_PORT); //AGG requests are to port 54534
  udp_out.write((uint8_t*)tempo_msg_buffer, sizeof(struct tempo_message));
  udp_out.endPacket();
}

int getTimeMessage(struct time_message * new_time) {
  unsigned long timeout = millis() + UDP_READ_TIMEOUT;
  int got_message = 0;
  Serial.print("Waiting for time message\n");
  udp_in.flush();
  while(got_message == 0 && millis() < timeout){
  // while(got_message == 0) {
    if(udp_in.parsePacket() > 0){
      Serial.print("Received a UDP packet, checking type.\n");
      udp_in.read((unsigned char*)incoming_udp_buffer, sizeof(incoming_udp_buffer));
      if(((uint32_t*)incoming_udp_buffer)[0] == htonl(TIME)){
        new_time->message_type     = incoming_udp_buffer[0];
        new_time->device_id        = incoming_udp_buffer[1];
        new_time->beat_signature_L = incoming_udp_buffer[2];
        new_time->beat_signature_R = incoming_udp_buffer[3];
        new_time->measure          = incoming_udp_buffer[4];
        new_time->beat             = incoming_udp_buffer[5];
        new_time->beat_interval    = incoming_udp_buffer[6];
        got_message = 1;
        Serial.print("Received time message "); Serial.print(new_time->measure);
        Serial.print(new_time->beat); Serial.print("\n");
      }
    } else {
      Serial.print("Received UDP message that wasn't a TIME message\n");
      udp_in.flush();
    }
  }
  if(millis() > UDP_READ_TIMEOUT){
    Serial.print("Timed out while waiting for udp packets on broadcast.\n");
    registerWithHost(AGGServer);
  }
}

//Operates on a periodic schedule:
// ms   event
// 0    Sample Snare
// 1    Send previously calculated tempo
// 2    Sample Bass
// 3    Get network time synch
// 4    Sample HiHat
// 5    Send events in queue
// 6    Sample Tom
// 7    Recalculate tempo
// 8    Write to terminal

unsigned int loopcounter = 0;
void loop() {
  loop_start_time = micros();

  //0 Sample Snare
  snare = ads.readADC_SingleEnded(SNARE);
  next_sample = loop_start_time + 2000;

  //1 Send tempo packet
  if(has_wifi == 1 && loopcounter % 125 == 0){
    sendAGGpacket(AGGServer);
  }

  //2 Sample bass  
  while(micros() < next_sample){};
  bass = ads.readADC_SingleEnded(BASS);
  next_sample = loop_start_time + 4000;

  //3 Get time synch
  if(has_wifi == 1 && (loopcounter + 50) % 125 == 0){
    getTimeMessage(&current_time);    
  }

  //4 Sample hihat  
  while(micros() < next_sample){};
  cymbal = ads.readADC_SingleEnded(CYMBAL);
  next_sample = loop_start_time + 6000;

  //5 Send events in queue

  //6 Sample Tom
  while(micros() < next_sample){};
  tom = ads.readADC_SingleEnded(TOM);
  next_sample = loop_start_time + 8000;

  // 7 Recalculate tempo


  // 8 Flash LED and Write to terminal
  if(snare > snare_th || cymbal > cymbal_th || bass > bass_th || tom > tom_th){
    led_state = HIGH;
  } else {
    led_state = LOW;
  }
  digitalWrite(ledPin, led_state);

#ifdef SERIAL_PRINT  
  Serial.print(snare); Serial.print(", ");
  Serial.print(bass); Serial.print(", ");
  Serial.print(cymbal); Serial.print(", ");
  Serial.print(tom); Serial.print("\n");
#endif

  loopcounter++;
  while(micros() < next_sample){};
}
