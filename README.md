# bThings MQTT Library
## Overview
Mongoose-OS library that allows you allows you to easily enable MQTT over [bThings](https://github.com/diy365-mgos/bthing).
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
Disables MQTT messages for a bThing. Returns `true` on success, or `false` otherwise.

|Parameter||
|--|--| 
|thing|A bThing.|
## To Do
- Implement javascript APIs for [Mongoose OS MJS](https://github.com/mongoose-os-libs/mjs).
