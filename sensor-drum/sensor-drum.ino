
#define DRUM_IN A0
#define SENSOR_DELAY 20
#define THRESHOLD_LOW 200
#define THRESHOLD_HIGH 360

#include <netinet/in.h>

int val_vib_0 = 0;

void setup() {
  Serial.begin(115200);
  Serial.print("a0, a1, a2, a3\n"); 
}

int start = millis();
int next_sample = start + SENSOR_DELAY;
int sample_time;

void loop() {
  sample_time = millis();
  if(sample_time > next_sample) {
    val_vib_0 = analogRead(DRUM_IN);
//    Serial.print(sample_time);
//    Serial.print(", ");
    if(val_vib_0 > THRESHOLD_LOW && val_vib_0 < THRESHOLD_HIGH) {
      val_vib_0 = 280;
    }
    Serial.print(val_vib_0);
    Serial.print("\n");
    next_sample += SENSOR_DELAY;
  } 
}
