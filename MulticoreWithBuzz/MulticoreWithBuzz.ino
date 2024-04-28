// MulticoreWithBuzz.ino
// Add a buzz using the ArdiPi buzzer

// Demonstrates a simple use of the setup1()/loop1() functions
// for a multiprocessor run.

// Will output something like, where C0 is running on core 0 and
// C1 is on core 1, in parallel.

// 11:23:07.507 -> C0: Blue leader standing by...
// 11:23:07.507 -> C1: Red leader standing by...
// 11:23:07.507 -> C1: Stay on target...
// 11:23:08.008 -> C1: Stay on target...
// 11:23:08.505 -> C0: Blue leader standing by...
// 11:23:08.505 -> C1: Stay on target...
// 11:23:09.007 -> C1: Stay on target...
// 11:23:09.511 -> C0: Blue leader standing by...
// 11:23:09.511 -> C1: Stay on target...
// 11:23:10.015 -> C1: Stay on target...

// Released to the public domain

#include <PWMAudio.h>
#define BUZZER_PIN 22

PWMAudio pwm(BUZZER_PIN);


void buzz()
{
  for (int i = 100; i < 10000; i += 1) {
    tone(BUZZER_PIN, i);
    delayMicroseconds(20);
  }
  tone(BUZZER_PIN, 0);
}

// The normal, core0 setup
void setup() {
  Serial.begin(115200);
  delay(5000);
  buzz();
}

void loop() {
  Serial.printf("C0: Blue leader standing by...\n");
  delay(1000);
  buzz();
}

// Running on core1
void setup1() {
  delay(5000);
  Serial.printf("C1: Red leader standing by...\n");
}

void loop1() {
  Serial.printf("C1: Stay on target...\n");
  delay(500);
}
