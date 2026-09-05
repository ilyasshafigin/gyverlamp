#pragma once

#include "hasensor.h"

class PubSubClient;
class HADevice;

class HASensorNumeric : public HASensor {
protected:
  float state;
  int precision;
  const char* unitOfMeasurement;

public:
  HASensorNumeric(
    const char* unique_id, const char* name, HADevice& device, const char* unit_of_measurement = NULL, int precision = 0
  );
  HASensorNumeric(const char* unique_id, const char* name, const char* unit_of_measurement = NULL, int precision = 0);

  virtual void onConnect(PubSubClient* client);
  uint8_t discoveryStepCount() override { return 1; }
  HAOperationResult discoveryStep(PubSubClient* client, uint8_t step) override;
  float getState() { return this->state; };
  void setState(float state);

  bool sendState(PubSubClient* client) override;
};
