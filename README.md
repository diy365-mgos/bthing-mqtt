# bThings MQTT Library
## Overview
Mongoose-OS library that allows you allows you to easily enable MQTT over [bThings](https://github.com/diy365-mgos/bthing).
## C/C++ API Reference
### mgos_bthing_mqtt_enable
```c
bool mgos_bthing_mqtt_enable(mgos_bthing_t thing, bool enable);
```
Enables or disables MQTT messages for a bThing. Returns `true` on success, or `false` otherwise.

|Parameter||
|--|--| 
|thing|A bThing.|
|enable|Enable or disable MQTT.|
## To Do
- Implement javascript APIs for [Mongoose OS MJS](https://github.com/mongoose-os-libs/mjs).
