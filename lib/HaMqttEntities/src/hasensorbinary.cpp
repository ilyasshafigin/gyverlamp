#include <Arduino.h>
#include <PubSubClient.h>

#include <hasensorbinary.h>

const char *const HASensorBinary::component PROGMEM = "binary_sensor";

HASensorBinary::HASensorBinary(const char *unique_id, const char *name,
    HADevice& device) : HASensor(unique_id,name,component)
{
    this->device = &device;
    this->state = false;
}

HASensorBinary::HASensorBinary(const char *unique_id, const char *name)
    : HASensor(unique_id,name, component)
{
    this->state = false;
}

bool HASensorBinary::sendState(PubSubClient * client)
{
    char topic[HA_MAX_TOPIC_LENGTH];
    getStateTopic(topic);
    const bool published = client->publish(topic,this->state ? "ON" : "OFF");
    if (published) dirty = false;
    return published;
}

void HASensorBinary::setState(bool on_off)
{
    if(this->state == on_off)
        return;
    dirty = true;
    this->state = on_off;
    this->onStateChange();
}
