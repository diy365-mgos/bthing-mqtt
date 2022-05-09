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

/*****************************************************************************************************************************
  TOPIC PATH                                                            P/S   DESCRIPTION

  COMMON TOPICS
  - ${topic_prefix}/cmd                                                    SUB   Receive command for all eddies (broadcas)
  - ${topic_prefix}/${device_id}/cmd                                       SUB   Receive command for this eddy
  - ${topic_prefix}/${device_id}/LWT                                       PUB   Publish LWT messages
  
  SHADOW-MODE TOPICS
  - ${topic_prefix}/${device_id}/state/+
      ${topic_prefix}/${device_id}/state/get                               SUB   Recieve get-state command for the shadow
      ${topic_prefix}/${device_id}/state/set                               SUB   Rrecieve set-state command for the shadow
  - ${topic_prefix}/${device_id}/state/updated                             PUB   Publish the shadow state
  
  STANDARD-MODE TOPICS
  - ${topic_prefix}/${device_id}/state/get                                 SUB   Recieve get-state command for all bThings
  - ${topic_prefix}/${device_id}/+/+/state/+
      ${topic_prefix}/${device_id}/${bthing_domain}/${bthing_id}/state/get    SUB   Recieve get-state command for a domain.bThing
      ${topic_prefix}/${device_id}/${bthing_domain}/${bthing_id}/state/set    SUB   Recieve set-state command for a domain.bThing
  - ${topic_prefix}/${device_id}/+/state/+
      ${topic_prefix}/${device_id}/${bthing_id}/state/get                  SUB   Recieve get-state command for a bThing
      ${topic_prefix}/${device_id}/${bthing_domain}/state/get                 SUB   Recieve get-state command for all domain.bThings
      ${topic_prefix}/${device_id}/${bthing_id}/state/set                  SUB   Recieve set-state command for a bThing
      ${topic_prefix}/${device_id}/${bthing_domain}/state/set                 SUB   Recieve set-state command for all domain.bThings
  - ${topic_prefix}/${device_id}/${bthing_domain}/${bthing_id}/state/updated  PUB   Publish the state of a domain.bThing
  - ${topic_prefix}/${device_id}/${bthing_id}/state/updated                PUB   Publish the state of a bThing

*/

#ifndef MGOS_BTHING_MQTT_H_
#define MGOS_BTHING_MQTT_H_

#include <stdbool.h>
#include "mgos_bthing.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MGOS_BTHING_MQTT_CMD_PING       "ping"
#define MGOS_BTHING_MQTT_VERB_GET       "get"
#define MGOS_BTHING_MQTT_VERB_SET       "set"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MGOS_BTHING_MQTT_H_ */