#ifndef DISTANCE_READER_H
#define DISTANCE_READER_H

#include <Arduino.h>

class DistanceReader {
public:
  DistanceReader(uint8_t trigPin,
                 uint8_t echoPin,
                 unsigned long echoTimeoutUs = 30000UL,
                 uint8_t numSamples = 5,
                 unsigned long sampleIntervalMs = 500UL);

  void begin();
  float readAvgDistance();

private:
  const uint8_t _trigPin;
  const uint8_t _echoPin;
  const unsigned long _echoTimeoutUs;
  const uint8_t _numSamples;
  const unsigned long _sampleIntervalMs;

  void sendPulse();
  float readDistance();
};

#endif
