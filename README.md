# bThings MQTT Library
## Overview
Mongoose-OS library that allows you allows you to easily enable MQTT over [bThings](https://github.com/diy365-mgos/bthing).
## Features
- **Zero-Conf** - Just including the library all registered bThings will be automatically exposed via MQTT protocol. NOTE: private bThings (see [mgos_bthing_make_private()](https://github.com/diy365-mgos/bthing#mgos_bthing_make_private) function) won't be exposed.
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
$bthings/{device_id}/cmd
```
Publish a command to this topic to boradcast it to all devices in the local network.
```
$bthings/cmd
```
**Remarks**

These are the commands can be sent to a device. Mind that not all commands support broadcast.
|Command|Broadcastable||
|--|--|--|
|**ping**|Y|Ping the device. The device responds by publishing a birth message to [/LWT](#lwt) topic and by publishing its state to **/state/updated** topic, in either [standard mode](#standard_state_updated) or [shadow mode](#shadow_state_updated).|
### /LWT
A device publishes a birth message to this topic (see `birth_message` [configration](#configuration)).
```
$bthings/{device_id}/LWT
```
## Standard mode MQTT topics
In standard mode, each bThing of a device has its own dedicated MQTT topics.
### <a name="standard_state_updated"></a>/state/updated
A bThing publishes its state to this topic.
```
$bthings/{device_id}/{bthing_domain}/{bthing_id}/state/updated
$bthings/{device_id}/{bthing_id}/state/updated
```
### <a name="standard_state_set"></a>/state/set
Publish a state payload to this topic to set the bThing's state.
```
$bthings/{device_id}/{bthing_domain}/{bthing_id}/state/set
$bthings/{device_id}/{bthing_id}/state/set
```
Publish a state payload to this topic to set, in one shot, the state of all bThings in the `{bthing_domain}` domain.
```
$bthings/{device_id}/{bthing_domain}/state/set
```
**Remarks**

In case the requested state is not euqal to the current one, a bThings responds by publishing its new state to [/state/updated](#standard_state_updated) topic.
### <a name="standard_state_get"></a>/state/get
Publish an empty payload to this topic to get the bThing's state.
```
$bthings/{device_id}/{bthing_domain}/{bthing_id}/state/get
$bthings/{device_id}/{bthing_id}/state/get
```
Publish an empty payload to this topic to get the state of all bThings in the `{bthing_domain}` domain.
```
$bthings/{device_id}/{bthing_domain}/state/get
```
Publish an empty payload to this topic to get the state of all device's bThings.
```
$bthings/{device_id}/state/get
```
**Remarks**

A bThing responds by publishing its state to [/state/updated](#standard_state_updated).
## Shadow mode MQTT topics
In shadow mode, one single [shadow state document](#shadow-state-document) is used to set/get the state of all device's bThings.
#### Shadow state document
The Shadow state document is a JSON document used for setting and getting device's bThings states.

The first-level node can be a bThing ID or a bThing domain name as per following schema. The `<state_value>` can be any of the [bVariant data types](https://github.com/diy365-mgos/bvar#supported-data-types).
```
{
  "bthing_id": <state_value>,
  ...,
  "bthing_domain": {         
    "bthing_id": <state_value>,
    ...,
  },
}
```
The example below shows the sahdow state document of a device that hosts 4 bThings: a [bSwitch](https://github.com/diy365-mgos/bswitch) (ID `'switch_01'`), a [bFlowSensor](https://github.com/diy365-mgos/bflowsensor) (ID `'flow_01'`) and two [bSwitches](https://github.com/diy365-mgos/bswitch) (ID `'switch_02'` and `'switch_03'`) in the `'lights'` domain.
```json
{
  "switch_01": "ON",
  "flow_01": {
    "flowRate": 0.56,
    "totalFlow": 1.66
  },
  "lights": {         
    "switch_02": "ON",
    "switch_03": "ON",
  },
}
```
### <a name="shadow_state_updated"></a>/state/updated
A device publishes its [shadow state document](#shadow-state-document) to this topic.
```
$bthings/{device_id}/state/updated
```
### <a name="shadow_state_set"></a>/state/set
Publish a [shadow state document](#shadow-state-document) to this topic to set the state of one or more bThings.
```
$bthings/{device_id}/state/set
```
**Remarks**

In case the requested [shadow state document](#shadow-state-document) is not euqal to the current one, a device responds by publishing its new [shadow state document](#shadow-state-document) to [/state/updated](#shadow_state_updated) topic.
### <a name="shadow_state_get"></a>/state/get
Publish an empty payload to this topic to get the [shadow state document](#shadow-state-document) of the device.
```
$bthings/{device_id}/state/get
```
**Remarks**

A device responds by publishing its [shadow state document](#shadow-state-document) to [/state/updated](#shadow_state_updated) topic.
## Configuration
The library adds the `bthing.mqtt` section to the device configuration:
```javascript
{
  "birth_message": "online",  // Default MQTT birth message
  "qos": 0,                   // Default MQTT QOS value for publishing messages
  "retain": false,            // Default MQTT retain value for publishing messages
  "topic_prefix": "$bthings"  // Default MQTT topic prefix (e.g.: {topic_prefix}/{device_id}/...)
}
```
In addition, following settings are available in `bthing.mqtt` section when the [bThings Shadow library](https://github.com/diy365-mgos/bthing-shadow) is included:
```javascript
{
  "pub_delta_shadow": false    //Enable publishing delta shadow instead of the full one
}
```
The library sets these `mqtt` section settings as well:
```javascript
{
  "enable": true,
  "will_topic": "{topic_prefix}/{device_id}/LWT",
  "will_message": "offline"
}
```
<!-- ## C/C++ API Reference
### mgos_bthing_mqtt_disable
```c
bool mgos_bthing_mqtt_disable(mgos_bthing_t thing);
```
Disables MQTT messages for a bThing. Returns `true` on success, or `false` otherwise. If Shadow mode is enabled, the function [mgos_bthing_shadow_disable()](https://github.com/diy365-mgos/bthing-shadow#mgos_bthing_shadow_disable) is automatically invoked behind the scenes.

|Parameter||
|--|--| 
|thing|A bThing.| -->
## To Do
- Implement javascript APIs for [Mongoose OS MJS](https://github.com/mongoose-os-libs/mjs).
