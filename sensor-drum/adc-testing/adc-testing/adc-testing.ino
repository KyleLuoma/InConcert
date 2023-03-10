#include <Adafruit_ADS1X15.h>
#include <SPI.h>

long a0;
Adafruit_ADS1115 ads;
long rate_interval_us = 1163;
unsigned long next_sample = 0;

void setup() {
  // put your setup code here, to run once:
  ads.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  while(next_sample > micros()){}
  a0 = ads.readADC_SingleEnded(0);
  Serial.println(a0);
  next_sample = micros() + rate_interval_us;
}
