#include <Adafruit_ADS1X15.h>
#include <WiFi101.h>
#include <RTCZero.h>

#include "wifi-info.h"
#include "inconcert-communication.h"

#define SNARE 0
#define BASS 1
#define CYMBAL 2
#define TOM 3

#define SAMPLE_INTERVAL 2  //MS between sensor samples (x4 samples)
#define INIT_CYCLES 100    //Number of sample cycles to perform to evaluate baseline
#define INPUT_THRESHOLD 50 //Amount of signal to increase above idle to register as a hit

Adafruit_ADS1115 ads;

int16_t snare, cymbal, bass, tom; //Use for storing sample inputs
int16_t snare_th, cymbal_th, bass_th, tom_th;//Input thresholds
const int ledPin = LED_BUILTIN; 
int led_state = LOW;
int has_wifi = 1;
int wifi_status;

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
    while(wifi_stats != WL_CONNECTED) {
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

unsigned long loop_start_time;
unsigned long next_sample;
void loop() {
  loop_start_time = micros();

  snare = ads.readADC_SingleEnded(SNARE);
  next_sample = loop_start_time + 2000;

  while(micros() < next_sample){};
  bass = ads.readADC_SingleEnded(BASS);
  next_sample = loop_start_time + 4000;

  while(micros() < next_sample){};
  cymbal = ads.readADC_SingleEnded(CYMBAL);
  next_sample = loop_start_time + 6000;

  while(micros() < next_sample){};
  tom = ads.readADC_SingleEnded(TOM);
  next_sample = loop_start_time + 8000;


  if(snare > snare_th || cymbal > cymbal_th || bass > bass_th || tom > tom_th){
    led_state = HIGH;
  } else {
    led_state = LOW;
  }
  digitalWrite(ledPin, led_state);

  Serial.print(snare); Serial.print(", ");
  Serial.print(bass); Serial.print(", ");
  Serial.print(cymbal); Serial.print(", ");
  Serial.print(tom); Serial.print("\n");

  while(micros() < next_sample){};
}
