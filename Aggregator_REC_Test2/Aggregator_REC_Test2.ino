#include "inconcert-communication.h"

#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

//#include <util.h>

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )

#define DEVICE_ID 1009

int status = WL_IDLE_STATUS;
char ssid[] = "inconcert";        // your network SSID (name)
char pass[] = "itgoesto11";    // your network password (use for WPA, or use as key for WEP)
//char ssid[] = "milehighsalsero";        // your network SSID (name)
//char pass[] = "BabyH3li098";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)

unsigned int localPort = 54555;      // local port to listen for UDP packets

//IPAddress AGGServer(35, 92, 239, 128); // AWS Server
IPAddress AGGServer(10, 42, 0, 1); // Raspberry Pi

const int AGG_PACKET_SIZE = 255;

byte packetBuffer[AGG_PACKET_SIZE];
unsigned long last_time_message_ms;
uint32_t registration_message_buffer[2];

WiFiUDP Udp;
WiFiUDP Udp2;

void setup() {
  Serial.begin(9600);

  while (!Serial); 

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  String fv = WiFi.firmwareVersion();

  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }

  Serial.println("Connected to wifi");
  printWifiStatus();
  Serial.println("\nStarting connection to server...");
  Udp.begin(localPort);
  Udp2.begin(54523);
  registerWithHost(AGGServer);
}

void loop() {

  //sendAGGpacket(AGGServer); 
 // delay(1000);
/*
  if (Udp.parsePacket()) {
    Serial.print("packet received size ");
    Serial.println(Udp.parsePacket());
    Udp.read(dance_rx_message, AGG_PACKET_SIZE); 
    Serial.print("Contents:");
    Serial.println(dance_rx_message);      
  }*/


  /*unsigned long sendAGGpacket(IPAddress& address) {
  Serial.print("Sending Packet to Agg Server ");
  Serial.println(AGGServer);
  //memset(packetBuffer, 0, AGG_PACKET_SIZE);
  memset(dance_int_tx, 0, AGG_PACKET_SIZE);
  dance_int_tx[0]=htonl(0);
  dance_int_tx[1]=htonl(1000);
  dance_int_tx[2]=htonl(75);
  dance_int_tx[3]=htonl(100);
  dance_int_tx[4]=htonl(0);

  Udp.beginPacket(address, 54523); //AGG requests are to port 54534
  Udp.write((uint8_t*)dance_int_tx, AGG_PACKET_SIZE);
  Udp.endPacket();
  }*/

 if (Udp.parsePacket()) {
    int got_message = 0;
    int packet_size = 0;
    struct time_message * new_time;
    struct event_message * new_event;

    //memset(incoming_udp_buffer, 0, AGG_PACKET_SIZE);
    Serial.println("packet received");
    Udp.read(packetBuffer,AGG_PACKET_SIZE);
    //Udp.read((unsigned char*)incoming_udp_buffer, AGG_PACKET_SIZE); // read the packet into the buffer
    //uint32_t message_type = get_uint32_from_charbuffer(incoming_udp_buffer);
    /*unsigned long highWord = word(packetBuffer[0], packetBuffer[1]);
    unsigned long lowWord = word(packetBuffer[2], packetBuffer[3]);
    unsigned long message_type = highWord << 16 | lowWord;*/
    unsigned int message_type = word(packetBuffer[0]);

    /*char* buffer_pointer = &incoming_udp_buffer[0];
      new_time->message_type     = htonl(get_uint32_from_charbuffer(incoming_udp_buffer[0]));
      new_time->device_id        = htonl(get_uint32_from_charbuffer(incoming_udp_buffer[4]));
      new_time->beat_signature_L = htonl(get_uint32_from_charbuffer(buffer_pointer+8));
      new_time->beat_signature_R = htonl(get_uint32_from_charbuffer(buffer_pointer+12));
      new_time->measure          = htonl(get_uint32_from_charbuffer(buffer_pointer+16));
      new_time->beat             = htonl(get_uint32_from_charbuffer(buffer_pointer+20));
      new_time->beat_interval    = htonl(get_uint32_from_charbuffer(buffer_pointer+24));*/

    /*if(message_type == TIME){
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
    } 
    else {
        #ifdef DEBUG_PRINT 
        Serial.print("Received message of type: "); Serial.print(message_type); Serial.print("\n");
        #endif
    }*/
    Serial.println("?");
    Serial.println(message_type);
    /*Serial.println(new_time->message_type);
    Serial.println(new_time->device_id);
    Serial.println(new_time->beat_signature_L);
    Serial.println(new_time->beat_signature_R);
    Serial.println(new_time->measure);
    Serial.println(new_time->beat);
    Serial.println(new_time->beat_interval);*/
  }
}

  uint32_t get_uint32_from_charbuffer(char * charbuffer) {
  uint32_t return_val = ((unsigned int)charbuffer[0] << 24) +
                        ((unsigned int)charbuffer[1] << 16) +
                        ((unsigned int)charbuffer[2] << 8)  +
                        ((unsigned int)charbuffer[3]);
  return return_val;           
  }

unsigned long registerWithHost(IPAddress& address) {
  #ifdef DEBUG_PRINT
  Serial.print("Sending registration message to Agg Server ");
  Serial.println(AGGServer);
  #endif
  Serial.print("Sending registration message to Agg Server ");
  registration_message_buffer[0] = htonl((uint32_t)REGISTER_CLIENT);
  registration_message_buffer[1] = htonl((uint32_t)DEVICE_ID);
  Udp2.beginPacket(address,54523);
  Udp2.write((uint8_t*)registration_message_buffer, sizeof(struct register_client_message));
  Udp2.endPacket();
}

  /*unsigned long sendAGGpacket(IPAddress& address) {
  //struct register_client_message * new_reg;  

  Serial.print("Sending Packet to Agg Server ");
  Serial.println(AGGServer);
 //memset(packetBuffer, 0, AGG_PACKET_SIZE);
  //memset(new_reg, 0, AGG_PACKET_SIZE);
  new_reg->message_type = htonl(3);
  new_reg.device_id = htonl(2004);

  Udp.beginPacket(address, 54523); //AGG requests are to port 54534
  Udp.write((uint8_t*)new_reg, AGG_PACKET_SIZE);
  Udp.endPacket();
  }*/

void printWifiStatus() {

  // print the SSID of the network you're attached to:

  Serial.print("SSID: ");

  Serial.println(WiFi.SSID());

  // print your board's IP address:

  IPAddress ip = WiFi.localIP();

  Serial.print("IP Address: ");

  Serial.println(ip);

  // print the received signal strength:

  long rssi = WiFi.RSSI();

  Serial.print("signal strength (RSSI):");

  Serial.print(rssi);

  Serial.println(" dBm");
}
