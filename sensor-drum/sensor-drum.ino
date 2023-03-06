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
#define INPUT_THRESHOLD 250 //Amount of signal to increase above idle to register as a hit
#define IGNORE_PERIOD 10 //Number of samples to ignore after a hit detection
#define INTERVAL_LIMIT 64 //Number of interval values to store for analysis
#define INTERVAL_MS_MIN 200 // Limit to 300 BPM and lower to minimize false positive beats
#define INTERVAL_MS_TERMINATE 5000// Maximum interval before resetting counter (typically the end of a song)
#define UDP_READ_TIMEOUT 2 //Number of ms until timing out on UDP read
#define MAX_US 4294967294U //Highest possible uS value

// #define SERIAL_PRINT
// #define DEBUG_PRINT
#define EVENT_PRINT
#define TEMPO_PRINT


Adafruit_ADS1115 ads;

int16_t snare, cymbal, bass, tom; //Use for storing sample inputs
int16_t snare_th, cymbal_th, bass_th, tom_th;//Input thresholds
byte snare_ignore, cymbal_ignore, bass_ignore, tom_ignore; //Ignore flags
byte snare_hit, cymbal_hit, bass_hit, tom_hit, any_hit; //Hit state 
byte any_sample_intvl_stored = 0;
unsigned long sample_time;
unsigned long snare_hit_ms, cymbal_hit_ms, bass_hit_ms, tom_hit_ms, any_hit_ms;
unsigned long snare_intvl, cymbal_intvl, bass_intvl, tom_intvl, any_sample_intvl;
uint16_t any_sample_intervals[INTERVAL_LIMIT] = {0};
uint16_t asi_ix = 0; // rolling index, reset at INTERVAL_LIMIT
unsigned long calculated_tempo;
uint32_t tempo_confidence = 100;

const int ledPin = LED_BUILTIN; 
const int blueLedPin = 0;
int led_state = LOW;
int blueLed_state = LOW;
int has_wifi = 1;
int wifi_status;
int udp_in_last_generation = 0;
int udp_in_gcur_eneration = 0; //number of times udp_in is reset
int i;

const int AGG_PACKET_SIZE = 255;

uint32_t tempo_msg_buffer[5];
char incoming_udp_buffer[AGG_PACKET_SIZE]; //big enough to handle time or event messages
uint32_t registration_message_buffer[2];
uint32_t event_msg_buffer[16];

struct time_message current_time;
unsigned long last_time_message_ms, next_beat_ms;
int16_t cur_measure, cur_beat, cur_beat_interval, last_beat, last_measure;
int16_t cur_signature = 4;



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
  pinMode(blueLedPin, INPUT);
  digitalWrite(blueLedPin, LOW);

  int16_t max_s = 0;
  int16_t max_b = 0;
  int16_t max_c = 0;
  int16_t max_tom = 0;
  //document baseline signal - no events
  //user should not hit drums during initialization period
  Serial.print("Learning sensor thresholds..\n");
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
  cymbal_th = max_c + INPUT_THRESHOLD;
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
  any_hit = 0;

  snare_hit_ms = 0;
  cymbal_hit_ms = 0;
  bass_hit_ms = 0;
  tom_hit_ms = 0;
  any_hit_ms = 0;
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

unsigned long sendHitEventMessage(
  IPAddress& address,
  uint32_t snare_hit, uint32_t snare_velocity,
  uint32_t bass_hit, uint32_t bass_velocity,
  uint32_t cymbal_hit, uint32_t cymbal_velocity,
  uint32_t tom_hit, uint32_t tom_velocity,
  uint32_t beat, uint32_t measure
) {
  #ifdef DEBUG_PRINT
  Serial.print("Sending Hit Event Message to Agg Server");
  #endif

  memset(event_msg_buffer, 0, sizeof(struct event_message));

  event_msg_buffer[0] = htonl(EVENT);
  event_msg_buffer[1] = htonl(DEVICE_ID);
  event_msg_buffer[2] = htonl(DRUM_HIT);
  event_msg_buffer[3] = htonl(measure);
  event_msg_buffer[4] = htonl(beat);
  event_msg_buffer[5] = htonl((uint32_t)8); //used params
  event_msg_buffer[6] = htonl(snare_hit);
  event_msg_buffer[7] = htonl(snare_velocity);
  event_msg_buffer[8] = htonl(bass_hit);
  event_msg_buffer[9] = htonl(bass_velocity);
  event_msg_buffer[10] = htonl(cymbal_hit);
  event_msg_buffer[11] = htonl(cymbal_velocity);
  event_msg_buffer[12] = htonl(tom_hit);
  event_msg_buffer[13] = htonl(tom_velocity);

  udp_out.beginPacket(address, SEND_PORT);
  udp_out.write((uint8_t*)event_msg_buffer, sizeof(struct event_message));
  udp_out.endPacket();
  udp_out.flush();
}

unsigned long sendTempoMessage(
  IPAddress& address, 
  uint32_t tempo, uint32_t confidence, uint32_t beat, uint32_t measure
  ){
  #ifdef DEBUG_PRINT
  Serial.print("Sending Tempo Message to Agg Server ");
  Serial.println(AGGServer);
  #endif

  memset(tempo_msg_buffer, 0, sizeof(struct tempo_message));

  tempo_msg_buffer[0] = htonl(TEMPO);
  tempo_msg_buffer[1] = htonl(DEVICE_ID);
  tempo_msg_buffer[2] = htonl(tempo);          
  tempo_msg_buffer[3] = htonl(confidence);          
  tempo_msg_buffer[5] = htonl(measure);    
  tempo_msg_buffer[6] = htonl(beat);

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

unsigned long absolute_value(int number){
  if(number < 0){
    return number * -1;
  }
  return number;
}

//Perform tempo calculation algorithm on full interval array
unsigned long calculate_tempo_full(uint16_t * intervals, uint16_t signature) {
  //Cycle through possible intervals from low to high (~170 BPM to 50 BPM)
  #ifdef TEMPO_PRINT
  Serial.print("Starting tempo calculation.\n");
  unsigned long start = millis();
  #endif
  unsigned long total_error, min_total_error, 
           min_intvl_error, intvl_error,
           most_likely_interval;
  unsigned long most_likely_tempo = 0;
  min_total_error = 1000000;
  for(unsigned long check_intvl = 350; check_intvl < 1200; check_intvl += 25){
    total_error = 0;
    //Cycle through each recorded interval
    for(unsigned long ix = 0; ix < INTERVAL_LIMIT; ix++) {
      //Find the minimum distance
      min_intvl_error = 1000000;
      for(unsigned long multiple = 1; multiple <= signature; multiple++){
        intvl_error = absolute_value((int)(intervals[ix] - (multiple * check_intvl)));
        if(intvl_error < min_intvl_error) {
          min_intvl_error = intvl_error;
        }
      }
      //add minimum error for the checked interval to total error
      total_error += min_intvl_error;
    }
    if(total_error < min_total_error){
      min_total_error = total_error;
      most_likely_interval = check_intvl;
      #ifdef TEMPO_PRINT
      Serial.print("Updated most likely interval to "); Serial.print(most_likely_interval); Serial.print("\n");
      #endif
    }
  }
  most_likely_tempo = 60000 / most_likely_interval;
  #ifdef TEMPO_PRINT
  Serial.print("Finished tempo calculation. Time: "); Serial.print(millis() - start); Serial.print(" MS \n");
  Serial.print("Most likely tempo: "); Serial.print(most_likely_tempo); Serial.print(" BPM\n");
  Serial.print("Most likely interval: "); Serial.print(most_likely_interval); Serial.print(" MS\n");
  #endif
  return most_likely_tempo;
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
// 7    Recalculate tempo (aperiodic, 7ms run time) and progress time 
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
    if(any_hit_ms > 0 && sample_time - any_hit_ms > INTERVAL_MS_MIN){
      any_sample_intvl = sample_time - any_hit_ms;
      any_sample_intvl_stored = 0;
    }
    snare_hit_ms = sample_time;
    any_hit_ms = sample_time;
    snare_hit = 1;
    snare_ignore = IGNORE_PERIOD;
    #ifdef EVENT_PRINT
    Serial.print("HIT: SNARE VELOCITY: "); Serial.print(snare); 
    Serial.print(" INTERVALs SNARE: "); Serial.print(snare_intvl);
    Serial.print(" ANY: "); Serial.print(any_sample_intvl); Serial.print("\n");
    #endif
  } else if (snare_ignore > 0) {
    snare_ignore--;
  } else {
    snare_hit = 0;
  }

  next_sample = getNextSampleUS(next_sample);
  
  //1 Send tempo packet
  if(has_wifi == 1 && loopcounter % 500 == 0){
    //int32_t tempo, uint32_t confidence, uint32_t beat, uint32_t measure
    sendTempoMessage(
      AGGServer, 
      (uint32_t)calculated_tempo, tempo_confidence, 
      (uint32_t)cur_beat, (uint32_t)cur_measure
    );
  }

  //2 Sample bass
  while(micros() < next_sample){};
  bass = ads.readADC_SingleEnded(BASS);
  sample_time = millis();
  if(bass_ignore == 0 && bass > bass_th) {
    if(bass_hit_ms > 0){
      bass_intvl = sample_time - bass_hit_ms;
    }
    if(any_hit_ms > 0 && sample_time - any_hit_ms > INTERVAL_MS_MIN) {
      any_sample_intvl = sample_time - any_hit_ms;
      any_sample_intvl_stored = 0;
    }
    bass_hit_ms = sample_time;
    any_hit_ms = sample_time;
    bass_hit = 1;
    bass_ignore = IGNORE_PERIOD;
    #ifdef EVENT_PRINT
    Serial.print("HIT: BASS VELOCITY: "); Serial.print(bass); 
    Serial.print(" INTERVALs BASS: "); Serial.print(bass_intvl);
    Serial.print(" ANY: "); Serial.print(any_sample_intvl); Serial.print("\n");
    #endif
  } else if (bass_ignore > 0) {
    bass_ignore--;
  } 
  
  next_sample = getNextSampleUS(next_sample);

  //3 Get time synch
  getTimeMessage(&current_time);    
  

  //4 Sample hihat  
  while(micros() < next_sample){};
  cymbal = ads.readADC_SingleEnded(CYMBAL);
  sample_time = millis();
  if(cymbal_ignore == 0 && cymbal > cymbal_th) {
    if(cymbal_hit_ms > 0){
      cymbal_intvl = sample_time - cymbal_hit_ms;
    }
    if(any_hit_ms > 0 && sample_time - any_hit_ms > INTERVAL_MS_MIN) {
      any_sample_intvl = sample_time - any_hit_ms;
      any_sample_intvl_stored = 0;
    }
    cymbal_hit_ms = sample_time;
    any_hit_ms = sample_time;
    cymbal_hit = 1;
    cymbal_ignore = IGNORE_PERIOD;
    #ifdef EVENT_PRINT
    Serial.print("HIT: CYMBAL VELOCITY: "); Serial.print(cymbal); 
    Serial.print(" INTERVALs CYMBAL: "); Serial.print(cymbal_intvl);
    Serial.print(" ANY: "); Serial.print(any_sample_intvl); Serial.print("\n");
    #endif
  } else if (cymbal_ignore > 0) {
    cymbal_ignore--;
  } 

  next_sample = getNextSampleUS(next_sample);

  //5 Send event message if any hits are active
  if((snare_hit + cymbal_hit + tom_hit + bass_hit) > 0) {
    sendHitEventMessage(
      AGGServer,
      snare_hit, (snare * snare_hit), //send velocity if snare_hit, else will be 0
      bass_hit, (bass * bass_hit),
      cymbal_hit, (cymbal * cymbal_hit),
      tom_hit, (tom * tom_hit),
      cur_beat, cur_measure
    );
    snare_hit = 0;
    cymbal_hit = 0;
    bass_hit = 0;
    tom_hit = 0;
  }

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
  sample_time = millis();
  if(tom_ignore == 0 && tom > tom_th) {
    if(tom_hit_ms > 0){
      tom_intvl = sample_time - tom_hit_ms;
    }
    if(any_hit_ms > 0 && sample_time - any_hit_ms > INTERVAL_MS_MIN) {
      any_sample_intvl = sample_time - any_hit_ms;
      any_sample_intvl_stored = 0;
    }
    tom_hit_ms = sample_time;
    any_hit_ms = sample_time;
    tom_hit = 1;
    tom_ignore = IGNORE_PERIOD;
    #ifdef EVENT_PRINT
    Serial.print("HIT: TOM VELOCITY: "); Serial.print(tom); 
    Serial.print(" INTERVALs TOM: "); Serial.print(tom_intvl);
    Serial.print(" ANY: "); Serial.print(any_sample_intvl); Serial.print("\n");
    #endif
  } else if (tom_ignore > 0){
    tom_ignore--;
  }

  next_sample = getNextSampleUS(next_sample);

  // 7 Recalculate tempo 
  //Register most recent interval and mark it as stored
  if(any_sample_intvl > 0 && any_sample_intvl_stored == 0) {
    any_sample_intervals[asi_ix] = (uint16_t)any_sample_intvl;
    asi_ix++;
    if(asi_ix == INTERVAL_LIMIT){
      asi_ix = 0;
      #ifdef EVENT_PRINT
      Serial.print("INTERVAL VALUE DUMP:\n");
      for(i = 0; i < INTERVAL_LIMIT; i++){
        Serial.print(any_sample_intervals[i]); Serial.print(" MS\n");
      }
      #endif
      calculated_tempo = calculate_tempo_full(any_sample_intervals, 4);
    }
    any_sample_intvl_stored = 1;
  }

  // Reset any_sample_intervals counter if interval is too high
  if(any_sample_intvl > INTERVAL_MS_TERMINATE){
    asi_ix = 0;
  }


  // and progress time

  if (last_beat != cur_beat || last_measure != cur_measure){ //First check if we got an update from UDP
    #ifdef DEBUG_PRINT
    Serial.print("From host: M: "); Serial.print(cur_measure);;
    Serial.print(" B: ");Serial.print(cur_beat); Serial.print("\n");
    #endif
    last_beat = cur_beat;
    last_measure = cur_measure;

    if(blueLed_state == HIGH) {
      blueLed_state = LOW;
    } else {
      blueLed_state = HIGH;
    }
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

    if(blueLed_state == HIGH) {
      blueLed_state = LOW;
    } else {
      blueLed_state = HIGH;
    }
  } 

  digitalWrite(blueLedPin, blueLed_state);

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
