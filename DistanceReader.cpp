#include "DistanceReader.h"

namespace {
const float SOUND_SPEED_CM_US = 0.0343f;
const float MIN_DISTANCE_CM = 2.0f;
const float MAX_DISTANCE_CM = 400.0f;
}

DistanceReader::DistanceReader(uint8_t trigPin,
                               uint8_t echoPin,
                               unsigned long echoTimeoutUs,
                               uint8_t numSamples,
                               unsigned long sampleIntervalMs)
    : _trigPin(trigPin),
      _echoPin(echoPin),
      _echoTimeoutUs(echoTimeoutUs),
      _numSamples(numSamples),
      _sampleIntervalMs(sampleIntervalMs) {}

void DistanceReader::begin() {
  pinMode(_trigPin, OUTPUT);
  pinMode(_echoPin, INPUT);
  digitalWrite(_trigPin, LOW);
  delay(500);
}

void DistanceReader::sendPulse() {
  digitalWrite(_trigPin, LOW);
  delayMicroseconds(3);

  digitalWrite(_trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(_trigPin, LOW);
}

float DistanceReader::readDistance() {
  sendPulse();

  const unsigned long duration = pulseIn(_echoPin, HIGH, _echoTimeoutUs);

  if (duration == 0) {
    return -1.0f;
  }

  const float distance = (duration * SOUND_SPEED_CM_US) / 2.0f;

  if (distance < MIN_DISTANCE_CM || distance > MAX_DISTANCE_CM) {
    return -1.0f;
  }

  return distance;
}

float DistanceReader::readAvgDistance() {
  Serial.println("readAvgDistance:start");
  float sum = 0.0f;
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < _numSamples; i++) {
    const float distance = readDistance();

    if (distance >= 0.0f) {
      sum += distance;
      validCount++;
    }

    delay(_sampleIntervalMs);
  }

  if (validCount == 0) {
    return -1.0f;
  }
  Serial.println("readAvgDistance:end");

  return sum / validCount;
}
