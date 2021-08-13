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

#ifndef MGOS_BTHING_MQTT_H_
#define MGOS_BTHING_MQTT_H_

#include <stdbool.h>
#include "mgos_bthing.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MGOS_BTHING_MQTT_CMD_PING   "ping"

bool mgos_bthing_mqtt_disable(mgos_bthing_t thing);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MGOS_BTHING_MQTT_H_ */