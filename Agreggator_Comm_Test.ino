#include "aggregator.h" 

#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <util.h>

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )

int status = WL_IDLE_STATUS;
char ssid[] = "milehighsalsero";        // your network SSID (name)
char pass[] = "BabyH3li098";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)

unsigned int localPort = 54555;      // local port to listen for UDP packets

IPAddress AGGServer(35, 92, 239, 128); // AWS Server

const int AGG_PACKET_SIZE = 255;

//byte packetBuffer[AGG_PACKET_SIZE];
//struct tempo_message dance_tx_message;
//struct tempo_message dance_rx_message;
char dance_tx_message[255];
uint32_t dance_int_tx[5];
unsigned char dance_int2_tx[5];
byte dance_tx_bytes[74];
char dance_rx_message[255];

WiFiUDP Udp;

void setup() {
  Serial.begin(9600);

  while (!Serial) {
    ; 
  }

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
}

void loop() {

  sendAGGpacket(AGGServer); 
  delay(1000);

  if (Udp.parsePacket()) {
    Serial.print("packet received size ");
    Serial.println(Udp.parsePacket());
    Udp.read(dance_rx_message, AGG_PACKET_SIZE); 
    Serial.print("Contents:");
    Serial.println(dance_rx_message);      
  }
  delay(1000);
}

  unsigned long sendAGGpacket(IPAddress& address) {
  Serial.print("Sending Packet to Agg Server ");
  Serial.println(AGGServer);
  //memset(packetBuffer, 0, AGG_PACKET_SIZE);
  memset(dance_int_tx, 0, AGG_PACKET_SIZE);

/*  packetBuffer[0] = 123;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
*/
  dance_tx_message[0] = 0;
  dance_tx_message[1] = 1000;
  dance_tx_message[2] = 75;
  dance_tx_message[3] = 100;
  dance_tx_message[4] = 0;

  dance_int_tx[0]=htonl(0);
  dance_int_tx[1]=htonl(1000);
  dance_int_tx[2]=htonl(75);
  dance_int_tx[3]=htonl(100);
  dance_int_tx[4]=htonl(0);

  dance_int2_tx[0]=0;
  dance_int2_tx[1]=1000;
  dance_int2_tx[2]=75;
  dance_int2_tx[3]=100;
  dance_int2_tx[4]=0;

  for (int i=0; i<74; ++i) {
      dance_tx_bytes[i] = 1;
  }  
  Udp.beginPacket(address, 54523); //AGG requests are to port 54534
  Udp.write(dance_tx_bytes, AGG_PACKET_SIZE);
  Udp.endPacket();

  Udp.beginPacket(address, 54523); //AGG requests are to port 54534
  Udp.write((uint8_t*)dance_int_tx, AGG_PACKET_SIZE);
  Udp.endPacket();

  Udp.beginPacket(address, 54523); //AGG requests are to port 54534
  Udp.write(dance_int2_tx, AGG_PACKET_SIZE);
  Udp.endPacket();

  Udp.beginPacket(address, 54523); //AGG requests are to port 54534
  Udp.write(dance_tx_message, AGG_PACKET_SIZE);
  Udp.endPacket();

  }

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