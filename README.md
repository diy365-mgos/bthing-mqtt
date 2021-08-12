# bThings MQTT Library
## Overview
Mongoose-OS library that allows you allows you to easily enable MQTT over [bThings](https://github.com/diy365-mgos/bthing).
## Features
- **Zero-Conf** - Just including the library all registered bThings will be automatically exposed via MQTT protocol.
- **Shadow state support** - Just add the [bThings Shadow library](https://github.com/diy365-mgos/bthing-shadow) to enable the Shadow mode.
## Supported modes
Two different modes are supported: Standard and Shadow.
### Standard mode
This is the default mode. In Standard mode, the state of a bThing is published to a dedicated MQTT topic, and it can be set singularly and independently from the others. Following topics are available for every registered and enabled bThing:
|Verb|Default topic||
|--|--|--|
|**state/updated**|`bthings/${device_id}/${bthing_id}/state/updated`|Subscribe to this topic for receiving state updates of a bThing.|
|**state/set**|`bthings/${device_id}/${bthing_id}/state/set`|Send a payload to this topic for setting the state of a [bActuator](https://github.com/diy365-mgos/bactuator).| 
|**state/get**|`bthings/${device_id}/${bthing_id}/state/get`|Send an empty payload to this topic for getting a bThing state. As soon as this command is received, the bThing will publish a message to its *state/updated* topic.|

In addition, following topics are available for all registered and enabled bThings:
|Verb|Default topic||
|--|--|--|
|**state/get**|`bthings/${device_id}/state/get`|Send an empty payload to this topic for getting bThing states. As soon as this command is received, all bThings will publish singularly a message to their *state/updated* topics.|

### Shadow mode
To enable Shadow mode just include the [bThings Shadow library](https://github.com/diy365-mgos/bthing-shadow) in your `mos.yaml` file. In Shadow mode, only one signle shadow state containing the state of all registered and enabled bThings is published, and many bThing states can be set simultaneously, in one shot. Following topics are available for all registered and enabled bThings:
|Verb|Default topic||
|--|--|--|
|**state/updated**|`bthings/${device_id}/state/updated`|Subscribe to this topic for receiving shadow updates. If the configuration setting `"pub_delta_shadow":true` a partial (delta) shadow instead of a full shadow is published. 
|**state/set**|`bthings/${device_id}/state/set`|Send a JSON payload to this topic for setting one or more [bActuator](https://github.com/diy365-mgos/bactuator) states simultaneously.| 
|**state/get**|`bthings/${device_id}/state/get`|Send an empty payload to this topic for getting the shadow state. As soon as this command is received, a full shadow state is published to *state/updated* topic.|
## Configuration
The library adds the `bthing.mqtt` section to the device configuration:
```javascript
{
  "birth_message": "online",                                                // Default MQTT birth message
  "qos": 0,                                                                 // Default MQTT QOS value for publishing messages
  "retain": false,                                                          // Default MQTT retain value for publishing messages
  "state_updated_topic": "bthings/${device_id}/${bthing_id}/state/updated", //The the topic for publishing state updates
  "set_state_topic": "bthings/${device_id}/${bthing_id}/state/set",         // The the topic for receiving set-state messages
  "get_state_topic": "bthings/${device_id}/${bthing_id}/state/get",         // The the topic for getting the state
}
```
In addition, following settings are available when the [bThings Shadow library](https://github.com/diy365-mgos/bthing-shadow) is included:
```javascript
{
  "pub_delta_shadow": false,    //Enable publishing delta shadow instead of the full one
}
```
## C/C++ API Reference
### mgos_bthing_mqtt_disable
```c
bool mgos_bthing_mqtt_disable(mgos_bthing_t thing);
```
Disables MQTT messages for a bThing. Returns `true` on success, or `false` otherwise. If Shadow mode is enabled, the function [mgos_bthing_shadow_disable()](https://github.com/diy365-mgos/bthing-shadow#mgos_bthing_shadow_disable) is also invoked automatically.

|Parameter||
|--|--| 
|thing|A bThing.|
## To Do
- Implement javascript APIs for [Mongoose OS MJS](https://github.com/mongoose-os-libs/mjs).
