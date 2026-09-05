#pragma once

#ifdef USE_MQTT

#include <haentity.h>

class PubSubClient;
class HADevice;

class HALight : public HAEntity {
protected:
  bool dirty;
  uint32_t revision;
  bool state;
  uint8_t brightness;

  uint8_t red;
  uint8_t green;
  uint8_t blue;

  char effect[48];
  String effectList;

  void (*commandCallback)(bool on, uint8_t brightness);
  void (*effectCommandCallback)(const char* effectName);
  void (*colorCommandCallback)(uint8_t r, uint8_t g, uint8_t b);

public:
  HALight(const char* unique_id, const char* name, HADevice& device);
  HALight(const char* unique_id, const char* name);

  inline bool getState() { return state; }
  inline uint8_t getBrightness() { return brightness; }
  inline const char* getEffect() { return effect; }
  inline uint8_t getRed() { return red; }
  inline uint8_t getGreen() { return green; }
  inline uint8_t getBlue() { return blue; }
  inline bool isDirty() override { return dirty; }

  void setState(bool state);
  void setBrightness(uint8_t brightness);
  void setEffectList(const char* commaSeparatedEffects);
  void setEffect(const char* effectName);
  void setColor(uint8_t r, uint8_t g, uint8_t b);

  void onCommand(void (*callback)(bool on, uint8_t brightness));
  void onEffectCommand(void (*callback)(const char* effectName));
  void onColorCommand(void (*callback)(uint8_t r, uint8_t g, uint8_t b));

  void onConnect(PubSubClient* client) override;
  bool handleCommand(PubSubClient* client, char* topic, byte* payload, size_t length) override;
  uint8_t discoveryStepCount() override;
  HAOperationResult discoveryStep(PubSubClient* client, uint8_t step) override;
  uint8_t stateStepCount() const override;
  uint32_t stateRevision() const override { return revision; }
  HAOperationResult stateStep(PubSubClient* client, uint8_t step, uint32_t expectedRevision) override;
  bool onReceivedTopic(PubSubClient* client, byte* payload, unsigned int length) override;
  bool onReceivedBrightnessTopic(PubSubClient* client, byte* payload, unsigned int length);
  bool onReceivedEffectTopic(PubSubClient* client, byte* payload, unsigned int length);
  bool onReceivedColorTopic(PubSubClient* client, byte* payload, unsigned int length);
  bool sendState(PubSubClient* client) override;

  char* getCommandTopic(char* buffer) override;
  char* getOnOffCommandTopic(char* buffer);
  char* getBrightnessCommandTopic(char* buffer);
  char* getBrightnessStateTopic(char* buffer);
  char* getEffectCommandTopic(char* buffer);
  char* getEffectStateTopic(char* buffer);
  char* getColorCommandTopic(char* buffer);
  char* getColorStateTopic(char* buffer);

  bool dispatchCommand(PubSubClient* client, const char* topic, byte* payload, unsigned int length);
  bool isBrightnessCommandTopic(const char* topic);
  bool isEffectCommandTopic(const char* topic);
  bool isColorCommandTopic(const char* topic);
};

#endif
