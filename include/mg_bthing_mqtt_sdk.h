/*
 * Copyright (c) 2021 DIY356
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MGOS_BTHING_MQTT_SDK_H_
#define MGOS_BTHING_MQTT_SDK_H_ 

#include <stdbool.h>
#include <stdarg.h>
#include "mgos_mqtt.h"
#include "mgos_bthing.h"
#include "mg_bthing_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mg_bthing_mqtt_item {
  bool enabled;
  mgos_bthing_t thing;
  char *pub_topic;
  char *sub_topic;
  bool shadow_publish;
  struct mg_bthing_mqtt_item *next;
};

int mg_bthing_mqtt_pub(const char *topic, const char *msg, bool retain);
int mg_bthing_mqtt_pubf(const char *topic, bool retain, const char *json_fmt, ...);
int mg_bthing_mqtt_pubv(const char *topic, bool retain, const char *json_fmt, va_list ap);

char *mg_bthing_mqtt_build_device_topic(const char *topic);

void mg_bthing_mqtt_add_item(struct mg_bthing_mqtt_item *item);
struct mg_bthing_mqtt_item *mg_bthing_mqtt_get_item(mgos_bthing_t thing);
struct mg_bthing_mqtt_item *mg_bthing_mqtt_get_items();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MGOS_BTHING_MQTT_SDK_H_  */