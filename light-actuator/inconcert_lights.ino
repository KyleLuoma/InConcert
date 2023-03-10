#include "./Adafruit_NeoPixel.h"     //Library file
#include "inconcert-communication.h"
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

#define PIN 6   
#define PINS2 5                     
#define MAX_LED 14                   //14 RGB light

#define LEDG 10
#define LEDR 11
#define LEDB 12

#define DEVICE_ID 1009

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )

Adafruit_NeoPixel strip = Adafruit_NeoPixel( MAX_LED, PIN, NEO_RGB + NEO_KHZ800 );
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel( MAX_LED, PINS2, NEO_RGB + NEO_KHZ800 );
  
char ssid[] = "inconcert";        
char pass[] = "itgoesto11";    
int keyIndex = 0;                 

unsigned int localPort = 54555;      // local port to listen for UDP packets
unsigned int sourcePort = 54523;     //port to initiate contact with AGGserver

IPAddress AGGServer(10, 42, 0, 1); // Raspberry Pi

const int AGG_PACKET_SIZE = 255;
byte packetBuffer[AGG_PACKET_SIZE];
uint32_t registration_message_buffer[2];

int status = WL_IDLE_STATUS;

WiFiUDP Udp;

unsigned long snare_time;
unsigned long bass_time;
unsigned long cymbal_time;
unsigned long tom_time; 
uint32_t snare;
uint32_t bass;
uint32_t cymbal;
uint32_t tom;
uint32_t dance_status;
uint32_t timing_count;
uint32_t timing_color;
uint32_t timing_flag;

void setup()
{
  timing_count=0;
  timing_color=1;
  //initialize RGB light
  pinMode(LEDG, OUTPUT);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDB, OUTPUT); 
  
  //initialize light strips
  strip.begin();  
  strip.show();            
  strip2.begin(); 
  strip2.show(); 

  Serial.begin(9600);      // initialize serial communication
  // check for the WiFi module:
  digitalWrite(LEDR, HIGH);
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }
  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);                   
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
  digitalWrite(LEDR, LOW); 
  digitalWrite(LEDG, HIGH);
  printWifiStatus();     

  //initialize Udp stream
  Udp.begin(localPort);

  //One time registration
  registerWithHost(AGGServer);
}

void RGB_Light(int R, int G, int B, int j)
1{ 
  uint32_t color2 = strip.Color(G,R,B); 
  strip.setPixelColor(j, color2);
  strip.show();
}

void RGB_Light2(int R, int G, int B, int j)
{ 
  uint32_t color2 = strip2.Color(G,R,B); 
  strip2.setPixelColor(j, color2);
  strip2.show();
}

void RGB_OFF()
{ 
  uint8_t i = 0;
  uint32_t color2 = strip2.Color(0, 0, 0);
  for(i=0;i<MAX_LED;i++)   
  {
    strip2.setPixelColor(i, color2);  
  }
    strip2.show();
}

void loop() {   
  snare=0;
  bass=0;
  cymbal=0;
  tom=0;
  dance_status=0;

  if (Udp.parsePacket()) {
    struct event_message * new_event; 

    Udp.read(packetBuffer,AGG_PACKET_SIZE);
    int offset = 0;

    int message_type = packetBuffer[0];

    //Udp message extraction for events
    if(message_type == 1) { 
      offset = 4;
      new_event->message_type = message_type;
      unsigned long highWord = word(packetBuffer[offset], packetBuffer[offset+1]);
      unsigned long lowWord = word(packetBuffer[offset+2], packetBuffer[offset+3]);
      unsigned long temp = highWord << 16 | lowWord;
      new_event->device_id    = htonl(temp);
      offset = 8;
      highWord = word(packetBuffer[offset], packetBuffer[offset+1]);
      lowWord = word(packetBuffer[offset+2], packetBuffer[offset+3]);
      temp = highWord << 16 | lowWord;
      new_event->event_type = htonl(temp);
      dance_status = new_event->event_type;
      snare = packetBuffer[24];
      bass = packetBuffer[32];
      cymbal = packetBuffer[40];
      tom = packetBuffer[48];
    }
  }
  changeLights(snare,bass,cymbal,tom);
  changeDance(dance_status);
}

//changes lights according to drum 
void changeLights(uint32_t snare,uint32_t bass,uint32_t cymbal,uint32_t tom) {
  unsigned long currentTime = millis();
  if (snare) {
	  RGB_Light2(0,255,0,1); 
	  RGB_Light2(0,255,0,2); 
	  RGB_Light2(0,255,0,3); 
    snare_time = millis(); //used instead of delay
	}
  else {
    if ((currentTime-snare_time)>200) {
	    RGB_Light2(0,0,0,1); 
	    RGB_Light2(0,0,0,2); 
	    RGB_Light2(0,0,0,3);       
    }
  }
  if (bass) {
	  RGB_Light2(0,0,255,5); 
	  RGB_Light2(0,0,255,6); 
	  RGB_Light2(0,0,255,7);
    bass_time = millis();
  }
  else {
    if ((currentTime-bass_time)>200) {
	    RGB_Light2(0,0,0,5); 
	    RGB_Light2(0,0,0,6); 
	    RGB_Light2(0,0,0,7);       
    }
  }
  if (cymbal) {
	  RGB_Light2(255,0,0,8); 
	  RGB_Light2(255,0,0,9); 
	  RGB_Light2(255,0,0,10);
    cymbal_time = millis(); 
  }
  else {
    if ((currentTime-cymbal_time)>200) {
	    RGB_Light2(0,0,0,8); 
	    RGB_Light2(0,0,0,9); 
	    RGB_Light2(0,0,0,10);       
    }
  }
  if (tom) {
	  RGB_Light2(0,255,255,12); 
	  RGB_Light2(0,255,255,13); 
	  RGB_Light2(0,255,255,14);
    tom_time = millis(); 
  }  
    else {
    if ((currentTime-tom_time)>200) {
	    RGB_Light2(0,0,0,12); 
	    RGB_Light2(0,0,0,13); 
	    RGB_Light2(0,0,0,14);       
    }
  }   
}

//Changes SMD LED according to dance detected
void changeDance(uint32_t dance) {
  if (dance != 0) {
    Serial.println(dance);
  }
  if (dance == 4) {
    digitalWrite(LEDR, LOW);
    digitalWrite(LEDG, HIGH);
    digitalWrite(LEDB, LOW);
    Serial.println("");
    Serial.println("salsa");
    Serial.println("");
  } 
  if (dance == 8) {
    digitalWrite(LEDR, LOW);
    digitalWrite(LEDG, LOW);
    digitalWrite(LEDB, HIGH);
    Serial.println("");
    Serial.println("bachata");
    Serial.println("");
  }
  if (dance == 1) {
    digitalWrite(LEDR, HIGH);
    digitalWrite(LEDG, LOW);
    digitalWrite(LEDB, LOW);
    Serial.println("");
    Serial.println("idle");  
    Serial.println("");  
  } 
}

//Registers with aggregator and starts broadcast packet flow
unsigned long registerWithHost(IPAddress& address) {
  #ifdef DEBUG_PRINT
  Serial.print("Sending registration message to Agg Server ");
  Serial.println(AGGServer);
  #endif
  Serial.print("Sending registration message to Agg Server ");
  registration_message_buffer[0] = htonl((uint32_t)REGISTER_CLIENT);
  registration_message_buffer[1] = htonl((uint32_t)DEVICE_ID);
  Udp.beginPacket(address,54523);
  Udp.write((uint8_t*)registration_message_buffer, sizeof(struct register_client_message));
  Udp.endPacket();
}

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}