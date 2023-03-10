#include "WiFiNINA.h"
#include "LSM6DSOXSensor.h"
#include "lsm6dsox_motion_intensity.h"
#include "aggregator.h" 

#include <SPI.h>
#include <WiFiUdp.h>
#include <util.h>

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )

//Interrupts.
#define INT_1 INT_IMU
volatile int mems_event = 0;
int status = WL_IDLE_STATUS;
char ssid[] = "SECRET_SSID";        
char pass[] = "SECRET_Password";   
int keyIndex = 0;  
unsigned int localPort = 54555;    // local port to listen for UDP packets
IPAddress AGGServer(10, 42, 0, 1); // Raspberry Pi Router
const int AGG_PACKET_SIZE = 255;
uint32_t dance_int_tx[3];
WiFiUDP Udp;


// IMU
LSM6DSOXSensor AccGyr(&Wire, LSM6DSOX_I2C_ADD_L);
// MLC
ucf_line_t *ProgramPointer;
int32_t LineCounter;
int32_t TotalNumberOfLine;

void INT1Event_cb();
void printMLCStatus(uint8_t status);

void setup()
{
  // LED on IMU
  pinMode(LEDB, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDR, OUTPUT);
  digitalWrite(LEDB, LOW);
  digitalWrite(LEDG, LOW);
  digitalWrite(LEDR, HIGH);

  // Initialize serial for output.
  Serial.begin(9600);

  //Wifi connection code
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
  digitalWrite(LEDR, LOW);
  Serial.println("Connected to wifi");
  printWifiStatus();
  Serial.println("\nStarting connection to server...");
  Udp.begin(localPort);
  // Initialize I2C bus.
  Wire.begin();
  // Initialize IMU
  AccGyr.begin();

  //MLC  
  ProgramPointer = (ucf_line_t *)lsm6dsox_motion_intensity;
  TotalNumberOfLine = sizeof(lsm6dsox_motion_intensity) / sizeof(ucf_line_t);
  Serial.println("Motion Intensity for LSM6DSOX MLC");
  Serial.print("UCF Number Line=");
  Serial.println(TotalNumberOfLine);

  for (LineCounter = 0; LineCounter < TotalNumberOfLine; LineCounter++) {
    if (AccGyr.Write_Reg(ProgramPointer[LineCounter].address, ProgramPointer[LineCounter].data)) {
      Serial.print("Error loading the Program to LSM6DSOX at line: ");
      Serial.println(LineCounter);
      while (1) {
        // Led blinking.
        digitalWrite(LED_BUILTIN, HIGH);
        delay(250);
        digitalWrite(LED_BUILTIN, LOW);
        delay(250);
      }
    }
  }

  Serial.println("Program loaded inside the LSM6DSOX MLC");

  //Initialize the accelerometer
  AccGyr.Enable_X();
  AccGyr.Set_X_ODR(104.0f);
  AccGyr.Set_X_FS(2);

  //Initialize the gyro
  AccGyr.Enable_G();
  AccGyr.Set_G_ODR(104.0f);
  AccGyr.Set_G_FS(2000);

  //Interrupts.
  pinMode(INT_1, INPUT);
  attachInterrupt(INT_1, INT1Event_cb, RISING);
}

void loop()
{
  if (mems_event) {
    mems_event = 0;
    //MLC code
    LSM6DSOX_MLC_Status_t status;
    AccGyr.Get_MLC_Status(&status);
    if (status.is_mlc1) {
      uint8_t mlc_out[8];
      AccGyr.Get_MLC_Output(mlc_out);
      printMLCStatus(mlc_out[0]);
    }
  }
}

void INT1Event_cb()
{
  mems_event = 1;
}

unsigned long sendAGGpacket(IPAddress& address, uint32_t event) {
  Serial.print("Sending Packet to Agg Server ");
  Serial.println(AGGServer);
  memset(dance_int_tx, 0, AGG_PACKET_SIZE);
  dance_int_tx[0]=htonl(1);  //1 is an event
  dance_int_tx[1]=htonl(1000);  //1000 is the device_id
  dance_int_tx[2]=htonl(event);  //event depends on dance type
  Udp.beginPacket(address, 54523); //AGG requests are to port 54534
  Udp.write((uint8_t*)dance_int_tx, AGG_PACKET_SIZE);
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

void printMLCStatus(uint8_t status)
{
  switch (status) {
    case 1:
      // Reset leds status
      digitalWrite(LEDB, LOW);
      digitalWrite(LEDG, LOW);
      digitalWrite(LEDR, LOW);
      // LEDB On
      digitalWrite(LEDR, HIGH);
      Serial.println("idle");
      sendAGGpacket(AGGServer,1);
      break;
    case 4:
      // Reset leds status
      digitalWrite(LEDB, LOW);
      digitalWrite(LEDG, LOW);
      digitalWrite(LEDR, LOW);
      // LEDG On
      digitalWrite(LEDG, HIGH);
      Serial.println("salsa");
      sendAGGpacket(AGGServer,4);
      break;     
    case 8:
      // Reset leds status
      digitalWrite(LEDB, LOW);
      digitalWrite(LEDG, LOW);
      digitalWrite(LEDR, LOW);
      // LEDG On
      digitalWrite(LEDB, HIGH);
      Serial.println("bachata");
      sendAGGpacket(AGGServer,8);
      break;    
    default:
      // Reset leds status
      digitalWrite(LEDB, LOW);
      digitalWrite(LEDG, LOW);
      digitalWrite(LEDR, LOW);
      //LEDR On
      digitalWrite(LEDR, HIGH);
      Serial.println("Idle");
      sendAGGpacket(AGGServer,1);
      break;
  }
}