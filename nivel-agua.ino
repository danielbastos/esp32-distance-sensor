#include "DistanceReader.h"
#include "AccessPoint.h"

const uint8_t TRIG_PIN = D9;
const uint8_t ECHO_PIN = D10;

const unsigned long ECHO_TIMEOUT_US = 30000;   // ~5 metros máx teórico
const uint8_t NUM_SAMPLES = 5;                 // quantidade de amostras por leitura
const unsigned long INTERVALO_SAMPLE = 500UL;

DistanceReader leitor(TRIG_PIN,
                      ECHO_PIN,
                      ECHO_TIMEOUT_US,
                      NUM_SAMPLES,
                      INTERVALO_SAMPLE);

AccessPoint accessPoint(leitor);

void setup() {
  Serial.begin(115200);
  leitor.begin();
  Serial.println("Leitura de distancia - AJ-SR04M");

  if (!accessPoint.begin()) {
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  accessPoint.handleClient();
}
