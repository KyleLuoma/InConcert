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
// #define REGISTER_WITH_HOST //Use if unable to receive broadcasts messages

#define SAMPLE_INTERVAL 2  //MS between sensor samples (x4 samples)
#define SAMPLE_INTERVAL_US 2000 //uS between sensor samples
#define INIT_CYCLES 100    //Number of sample cycles to perform to evaluate baseline
#define INPUT_THRESHOLD 50 //Amount of signal to increase above idle to register as a hit
#define IGNORE_PERIOD 30 //Number of samples to ignore after a hit detection
#define INTERVAL_LIMIT 32 //Number of interval values to store for analysis
#define UDP_READ_TIMEOUT 2 //Number of ms until timing out on UDP read
#define MAX_US 4294967295U //Highest possible uS value

// #define SERIAL_PRINT
// #define DEBUG_PRINT


Adafruit_ADS1115 ads;

int16_t snare, cymbal, bass, tom; //Use for storing sample inputs
int16_t snare_th, cymbal_th, bass_th, tom_th;//Input thresholds
byte snare_ignore, cymbal_ignore, bass_ignore, tom_ignore; //Ignore flags
byte snare_hit, cymbal_hit, bass_hit, tom_hit; //Hit state 
unsigned long sample_time;
unsigned long snare_hit_ms, cymbal_hit_ms, bass_hit_ms, tom_hit_ms;
unsigned long snare_intvl, cymbal_intvl, bass_intvl, tom_intvl;

const int ledPin = LED_BUILTIN; 
int led_state = LOW;
int has_wifi = 1;
int wifi_status;
int udp_in_last_generation = 0;
int udp_in_gcur_eneration = 0; //number of times udp_in is reset
int i;

const int AGG_PACKET_SIZE = 255;

uint32_t tempo_msg_buffer[5];
char incoming_udp_buffer[AGG_PACKET_SIZE]; //big enough to handle time or event messages
uint32_t registration_message_buffer[2];

struct time_message current_time;
unsigned long last_time_message_ms, next_beat_ms;
int16_t cur_measure, cur_beat, cur_beat_interval, last_beat, last_measure;



IPAddress AGGServer(10, 42, 0, 1); // Aggregator wifi hotspot address
WiFiUDP udp_out;
IPAddress broadcast_ip(10, 42, 0, 255); 
WiFiUDP udp_in;


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
#ifdef REGISTER_WITH_HOST
    registerWithHost(AGGServer);
#endif
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
  // --- Initialize status variables ---
  snare_th = max_s + INPUT_THRESHOLD;
  cymbal_th = max_c + 10;
  bass_th = max_b + INPUT_THRESHOLD;
  tom_th = max_tom + INPUT_THRESHOLD;
  cur_beat_interval = 500;

  snare_ignore = 0;
  cymbal_ignore = 0;
  bass_ignore = 0;
  tom_ignore = 0;

  snare_hit = 0;
  cymbal_hit = 0;
  bass_hit = 0;
  tom_hit = 0;

  snare_hit_ms = 0;
  cymbal_hit_ms = 0;
  bass_hit_ms = 0;
  tom_hit_ms = 0;
}

unsigned long loop_start_time;
unsigned long next_sample;

unsigned long registerWithHost(IPAddress& address) {
  #ifdef DEBUG_PRINT
  Serial.print("Sending registration message to Agg Server ");
  Serial.println(AGGServer);
  #endif
  memset(registration_message_buffer, 0, sizeof(struct register_client_message));
  registration_message_buffer[0] = htonl(REGISTER_CLIENT);
  registration_message_buffer[1] = htonl(DEVICE_ID);
  udp_out.beginPacket(address, SEND_PORT);
  udp_out.write((uint8_t*)registration_message_buffer, sizeof(struct register_client_message));
  udp_out.endPacket();
}

unsigned long sendAGGpacket(IPAddress& address) {
  #ifdef DEBUG_PRINT
  Serial.print("Sending Packet to Agg Server ");
  Serial.println(AGGServer);
  #endif

  memset(tempo_msg_buffer, 0, sizeof(struct tempo_message));

  tempo_msg_buffer[0] = htonl(0);            //message type
  tempo_msg_buffer[1] = htonl(DEVICE_ID);    //device id
  tempo_msg_buffer[2] = htonl(60);          //tempo
  tempo_msg_buffer[3] = htonl(94);           //confidence
  tempo_msg_buffer[5] = htonl(10001);        //timestamp

  udp_out.beginPacket(address, SEND_PORT); //AGG requests are to port 54534
  udp_out.write((uint8_t*)tempo_msg_buffer, sizeof(struct tempo_message));
  udp_out.endPacket();
  udp_out.flush();  
}

uint32_t get_uint32_from_charbuffer(char * charbuffer) {
  uint32_t return_val = ((unsigned int)charbuffer[0] << 24) +
                        ((unsigned int)charbuffer[1] << 16) +
                        ((unsigned int)charbuffer[2] << 8)  +
                        ((unsigned int)charbuffer[3]);
  return return_val;           
}

int getTimeMessage(struct time_message * new_time) {

  memset(incoming_udp_buffer, 0, AGG_PACKET_SIZE);
  unsigned long timeout = millis() + UDP_READ_TIMEOUT;
  int got_message = 0;
  int packet_size = 0;
  while(millis() < timeout && got_message == 0){
    packet_size = udp_in.parsePacket();
    if(packet_size > 0){
      udp_in.read((unsigned char*)incoming_udp_buffer, AGG_PACKET_SIZE);
      uint32_t message_type = htonl(get_uint32_from_charbuffer(incoming_udp_buffer));

      char* buffer_pointer = &incoming_udp_buffer[0];

      if(message_type == TIME){
        new_time->message_type     = message_type;
        new_time->device_id        = htonl(get_uint32_from_charbuffer(buffer_pointer+4));
        new_time->beat_signature_L = htonl(get_uint32_from_charbuffer(buffer_pointer+8));
        new_time->beat_signature_R = htonl(get_uint32_from_charbuffer(buffer_pointer+12));
        new_time->measure          = htonl(get_uint32_from_charbuffer(buffer_pointer+16));
        new_time->beat             = htonl(get_uint32_from_charbuffer(buffer_pointer+20));
        new_time->beat_interval    = htonl(get_uint32_from_charbuffer(buffer_pointer+24));
        last_time_message_ms = millis();
      #ifdef DEBUG_PRINT 
        Serial.print("Received time message m: "); Serial.print(new_time->measure);
        Serial.print(" b: ");Serial.print(new_time->beat); 
        Serial.print(" interval: "); Serial.print(new_time->beat_interval);
        Serial.print(" at MS: "); Serial.print(last_time_message_ms); Serial.print("\n");
      #endif
        got_message = 1;
      } else {
        #ifdef DEBUG_PRINT 
        Serial.print("Received message of type: "); Serial.print(message_type); Serial.print("\n");
        #endif
      }

    } else {
      udp_in.read();
    }
  }
  if(got_message == 1) {
    #ifdef DEBUG_PRINT 
    // Serial.print("Updating timing from:\n");
    // Serial.print(  "  - cur_measure:  "); Serial.print(cur_measure);
    // Serial.print("\n  - cur_beat:     "); Serial.print(cur_beat);
    // Serial.print("\n  - cur_interval: "); Serial.print(cur_beat_interval);
    // Serial.print("\n  - next_beat_ms: "); Serial.print(next_beat_ms);
    #endif
    cur_measure = new_time->measure; 
    cur_beat = new_time->beat;
    cur_beat_interval = new_time->beat_interval;
    next_beat_ms = last_time_message_ms + cur_beat_interval;
    #ifdef DEBUG_PRINT 
    // Serial.print("\nWith new values::\n");
    // Serial.print(  "  - cur_measure:  "); Serial.print(cur_measure);
    // Serial.print("\n  - cur_beat:     "); Serial.print(cur_beat);
    // Serial.print("\n  - cur_interval: "); Serial.print(cur_beat_interval);
    // Serial.print("\n  - next_beat_ms: "); Serial.print(next_beat_ms);
    // Serial.print("\n");
    #endif

  }

  if(millis() > timeout){
    #ifdef DEBUG_PRINT 
    Serial.print("Timed out while waiting for udp packets on broadcast.\n");
    #endif 
  }
}

//Calculate next uS sample time, use to protect from micros() rolloevers
unsigned long inline getNextSampleUS(unsigned long last_sample_time) {
  unsigned long next_sample_time = last_sample_time + SAMPLE_INTERVAL_US;
  if(MAX_US - SAMPLE_INTERVAL_US < next_sample_time) { //expect rollover
    next_sample_time = next_sample_time - (MAX_US - SAMPLE_INTERVAL_US);
  } 
  return next_sample_time;
}

//Operates on a periodic schedule:
// ms   event
// 0    Sample Snare
// 1    Send previously calculated tempo
// 2    Sample Bass
// 3    Get network time synch
// 4    Sample HiHat
// 5    Send events in queue, periodically reset udp sockets
// 6    Sample Tom
// 7    Recalculate tempo and progress time
// 8    Write to terminal

unsigned int loopcounter = 0;
void loop() {
  loop_start_time = micros();
  next_sample = loop_start_time;
  
  //0 Sample Snare
  snare = ads.readADC_SingleEnded(SNARE);
  sample_time = millis();
  if(snare_ignore == 0 && snare > snare_th){
    if(snare_hit_ms > 0){
      snare_intvl = sample_time - snare_hit_ms;
    }
    snare_hit_ms = sample_time;
    snare_hit = 1;
    snare_ignore = IGNORE_PERIOD;
  } else if (snare_ignore > 0) {
    snare_ignore--;
  }

  next_sample = getNextSampleUS(next_sample);
  
  //1 Send tempo packet
  if(has_wifi == 1 && loopcounter % 125 == 0){
    sendAGGpacket(AGGServer);
  }

  //2 Sample bass
  while(micros() < next_sample){};
  bass = ads.readADC_SingleEnded(BASS);

  next_sample = getNextSampleUS(next_sample);

  //3 Get time synch
  getTimeMessage(&current_time);    
  

  //4 Sample hihat  
  while(micros() < next_sample){};
  cymbal = ads.readADC_SingleEnded(CYMBAL);

  next_sample = getNextSampleUS(next_sample);

  //5 Send events in queue

  //periodically reset udp sockets
  if(loopcounter % 1000 == 0){
    #ifdef DEBUG_PRINT 
    Serial.println("Resetting UDP sockets");
    #endif
    udp_out.stop();
    udp_in.stop();
    udp_out.begin(SEND_PORT);
    udp_in.begin(BROADCAST_PORT);
  }

  //6 Sample Tom
  while(micros() < next_sample){};
  tom = ads.readADC_SingleEnded(TOM);

  next_sample = getNextSampleUS(next_sample);

  // 7 Recalculate tempo and progress time

  if (last_beat != cur_beat || last_measure != cur_measure){ //First check if we got an update from UDP
    #ifdef DEBUG_PRINT
    Serial.print("From host: M: "); Serial.print(cur_measure);;
    Serial.print(" B: ");Serial.print(cur_beat); Serial.print("\n");
    #endif
    last_beat = cur_beat;
    last_measure = cur_measure;
  } else if(millis() > next_beat_ms) { //Otherwise update with last known data
    next_beat_ms = millis() + cur_beat_interval;
    if(cur_beat >= current_time.beat_signature_R){
      #ifdef DEBUG_PRINT
      Serial.print("M: "); Serial.print(cur_measure);Serial.print("\n");
      #endif
      cur_beat = 1;
      cur_measure++;
    } else {
      cur_beat++;
      #ifdef DEBUG_PRINT
      Serial.print(" B: ");Serial.print(cur_beat);Serial.print("\n");
      #endif
    }   
    last_beat = cur_beat;
    last_measure = cur_measure;
  } 


  // 8 Flash LED and Write to terminal
  if(snare > snare_th || cymbal > cymbal_th || bass > bass_th || tom > tom_th){
    led_state = HIGH;
  } else {
    led_state = LOW;
  }
  digitalWrite(ledPin, led_state);

#ifdef SERIAL_PRINT
  //raw data:
  Serial.print(snare); Serial.print(", ");
  Serial.print(bass); Serial.print(", ");
  Serial.print(cymbal); Serial.print(", ");
  Serial.print(tom); Serial.print("\n");
  //Hit determination
#endif

  loopcounter++;
  while(micros() < next_sample){};
}
