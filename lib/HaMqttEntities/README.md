# HaMqttEntities for GyverLamp

This directory vendors a forked snapshot of
[paulino/ha-mqtt-entities](https://github.com/paulino/ha-mqtt-entities),
baseline version 1.0.12.

Original library copyright (c) Paulino Ruiz de Clavijo Vázquez
(`<pruiz@us.es>`). This vendored snapshot is licensed under the Apache License,
Version 2.0; see [LICENSE](LICENSE).

## Changes from upstream

- Added `HALight`, a Home Assistant MQTT light entity with brightness, effect,
  and RGB command/state support.
- Added `HATime`, a Home Assistant MQTT time entity with `HH:MM` state
  normalization.
- Replaced global `HAMQTT` with a caller-owned `HAMQTTController` and bounded
  entity registry.
- Added contextual command callback: entity parsing precedes application state
  reconciliation and corrected state publishing.
- Made `HADevice` passive; controller owns availability and MQTT lifecycle.
- Added cooperative discovery and state synchronization: controller sends at
  most one scheduled MQTT operation per `tick()`, preserves dirty state on
  publish failure, and retries with bounded backoff.
- Hardened command parsing and entity state handling: only fully valid payloads
  reach application callbacks; state-less entities have no phantom state step.

The upstream source layout, `library.properties`, license, and examples are
kept so PlatformIO can build this library from `src/`.

## Upstream overview

HaMqttEntities provides small Arduino/ESP classes for Home Assistant MQTT
discovery. It supports entity grouping in an `HADevice` and uses PubSubClient3
or PubSubClient for MQTT transport.

For usage of the upstream entities, see the retained examples in `examples/`.
