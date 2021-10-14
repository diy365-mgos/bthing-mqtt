# bThings MQTT Library
## Overview
Mongoose-OS library that allows you allows you to easily enable MQTT over [bThings](https://github.com/diy365-mgos/bthing).
## Features
- **Zero-Conf** - Just including the library all registered bThings will be automatically exposed via MQTT protocol.
- **Shadow state support** - Just add the [bThings Shadow library](https://github.com/diy365-mgos/bthing-shadow) to enable the Shadow mode.
## Supported modes
The library supports two modes:
- Standard mode
- Shadow mode
### Standard mode
This is the default mode. When in standard mode, the state of a bThing is published to a dedicated MQTT topic, and it can be set singularly and independently from the others.
### Shadow mode
To enable Shadow mode just include the [bThings Shadow library](https://github.com/diy365-mgos/bthing-shadow) in your `mos.yaml` file. In Shadow mode only one signle shadow state containing the state of all registered and enabled bThings is published, and many bThing states can be set simultaneously, in one shot.
## Common MQTT topics
A bThing uses common topics regardless of the enabled mode.
### /cmd
Publish a command to this topic to send it to the `{device_id}` device.
```
{topic_dom}/{device_id}/cmd
```
Publish a command to this topic to boradcast it to all devices in the local network.
```
{topic_dom}/cmd
```
**Remarks**

These are the commands can be sent to a device. Mind that not all commands support broadcast.
|Command|Broadcastable||
|--|--|--|
|**ping**|Y|Ping the device. The device responds by publishing a birth message to [/LWT](#lwt) topic and by publishing its state to **/state/updated** topic, in either [standard mode](#stateupdated) or shadow mode.|
### /LWT
A device publishes a birth message to this topic (see `birth_message` [configration](#configuration)).
```
{topic_dom}/{device_id}/LWT
```
## Standard mode MQTT topics
In standard mode, each device uses dedicated MQTT topics.
### /state/updated
A bThing publishes its state to this topic.
```
{topic_dom}/{device_id}/{bthing_dom}/{bthing_id}/state/updated
{topic_dom}/{device_id}/{bthing_id}/state/updated
```
### /state/set
Publish a state payload to this topic to set the bThing's state.
```
{topic_dom}/{device_id}/{bthing_dom}/{bthing_id}/state/set
{topic_dom}/{device_id}/{bthing_id}/state/set
```
Publish a state payload to this topic to set, in one shot, the state of all bThings in the `{bthing_dom}` domain.
```
{topic_dom}/{device_id}/{bthing_dom}/state/set
```
**Remarks**

In case the requested state is not euqal to the current one, a bThings responds by publishing its new state to [/state/updated](#stateupdated).
### /state/get
Publish an empty payload to this topic to get the bThing's state.
```
{topic_dom}/{device_id}/{bthing_dom}/{bthing_id}/state/get
{topic_dom}/{device_id}/{bthing_id}/state/get
```
Publish an empty payload to this topic to get the state of all bThings in the `{bthing_dom}` domain.
```
{topic_dom}/{device_id}/{bthing_dom}/state/get
```
Publish an empty payload to this topic to get the state of all device's bThings.
```
{topic_dom}/{device_id}/state/get
```
**Remarks**

A bThing responds by publishing its state to [/state/updated](#stateupdated).
## Shadow mode MQTT topics
## Configuration
The library adds the `bthing.mqtt` section to the device configuration:
```javascript
{
  "birth_message": "online",  // Default MQTT birth message
  "qos": 0,                   // Default MQTT QOS value for publishing messages
  "retain": false,            // Default MQTT retain value for publishing messages
  "topic_dom": "$bthings"     // Default domain name to use as prefix in topic's path"
}
```
In addition, following settings are available in `bthing.mqtt` section when the [bThings Shadow library](https://github.com/diy365-mgos/bthing-shadow) is included:
```javascript
{
  "pub_delta_shadow": false,    //Enable publishing delta shadow instead of the full one
}
```
The library sets these `mqtt` section settings as well:
```javascript
{
  "enable": false,
  "server": "",
  "ssl_ca_cert": "ca.pem",
  "will_topic": "{topic_domain}/{device_id}/LWT",
  "will_message": "offline"
}
```
## C/C++ API Reference
### mgos_bthing_mqtt_disable
```c
bool mgos_bthing_mqtt_disable(mgos_bthing_t thing);
```
Disables MQTT messages for a bThing. Returns `true` on success, or `false` otherwise. If Shadow mode is enabled, the function [mgos_bthing_shadow_disable()](https://github.com/diy365-mgos/bthing-shadow#mgos_bthing_shadow_disable) is automatically invoked behind the scenes.

|Parameter||
|--|--| 
|thing|A bThing.|
## To Do
- Implement javascript APIs for [Mongoose OS MJS](https://github.com/mongoose-os-libs/mjs).
