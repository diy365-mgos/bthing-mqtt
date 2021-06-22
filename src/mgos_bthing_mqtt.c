#include "mgos.h"
#include "mgos_bthing_mqtt.h"
#include "mg_bthing_mqtt_sdk.h"
#include "mgos_bvar_json.h"

#ifdef MGOS_BTHING_HAVE_SHADOW
#include "mgos_bthing_shadow.h"
#endif

#ifdef MGOS_HAVE_MJS
#include "mjs.h"
#endif

static void mg_bthing_mqtt_on_set_state(struct mg_connection *, const char *, int, const char *, int, void *);

bool mgos_bthing_mqtt_enable(mgos_bthing_t thing, bool enable) {
  #ifndef MGOS_BTHING_HAVE_SHADOW
  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
  if (!item) return false;
  if (item->enabled != enable) {
    if (item->sub_topic) {
      if (enable) {
        mgos_mqtt_sub(item->sub_topic, mg_bthing_mqtt_on_set_state, item);
      } else {
        if (!mgos_mqtt_unsub(item->sub_topic)) return false;
      }
    }
    item->enabled = enable;
  }
  return true;
  #endif //MGOS_BTHING_HAVE_SHADOW
  return false;
}

bool mg_bthing_mqtt_birth_message_pub() {  
  int msg_id = false;
  const char *will_topic = mgos_sys_config_get_mqtt_will_topic();
  const char *birth_message = mgos_sys_config_get_bthing_mqtt_birth_message();
  if (will_topic != NULL && birth_message != NULL) {
    msg_id = mg_bthing_mqtt_pub(will_topic, birth_message, false);
    if (msg_id <= 0) {
      LOG(LL_ERROR, ("Error publishing MQTT birth message to %s", will_topic));
    }
  }
  return (msg_id > 0);
}

static void mg_bthing_mqtt_on_event(struct mg_connection *nc,
                                     int ev,
                                     void *ev_data,
                                     void *user_data) {  
  if (ev == MG_EV_MQTT_CONNACK) {
    // Publish the 'birth' message 
    mg_bthing_mqtt_birth_message_pub();
    mgos_event_trigger(MGOS_EV_BTHING_UPDATE_STATE, NULL);
  } else if (ev == MG_EV_MQTT_DISCONNECT) {
  }
  (void) ev_data;
  (void) nc;
  (void) user_data;
}

void mg_bthing_mqtt_on_discovery(struct mg_connection *nc, const char *topic,
                                 int topic_len, const char *msg, int msg_len,
                                 void *ud) {
  mgos_event_trigger(MGOS_EV_BTHING_UPDATE_STATE, NULL);
  (void) nc; (void) topic; (void) topic_len; (void) msg; (void) msg_len; (void) ud;
}

#if MGOS_BTHING_HAVE_ACTUATORS
void mg_bthing_mqtt_on_set_state(struct mg_connection *nc, const char *topic,
                              int topic_len, const char *msg, int msg_len,
                              void *ud) {
  if (!msg || msg_len == 0) return;

  #ifdef MGOS_BTHING_HAVE_SHADOW
  mgos_bthing_shadow_json_set(msg, msg_len);

  #else
  struct mg_bthing_mqtt_item *item = (struct mg_bthing_mqtt_item *)ud; 
  if (item && item->enabled) {
    mgos_bvar_t state = NULL;
    if (!mgos_bvar_json_try_bscanf(msg, msg_len, &state)) {
      state = mgos_bvar_new_nstr(msg, msg_len);
    }
    mgos_bthing_set_state(item->thing, state);
    mgos_bvar_free(state);
  }

  #endif //MGOS_BTHING_HAVE_SHADOW

  (void) nc;
  (void) topic;
  (void) topic_len;
}
#endif //MGOS_BTHING_HAVE_ACTUATORS

#ifndef MGOS_BTHING_HAVE_SHADOW
static void mg_bthing_mqtt_on_created(int ev, void *ev_data, void *userdata) {
  if (ev != MGOS_EV_BTHING_CREATED) return;
  mgos_bthing_t thing = (mgos_bthing_t)ev_data;
  const char *id = mgos_bthing_get_id(thing);

  mg_bthing_mqtt_add_item(thing);

  if (mg_bthing_sreplace(mgos_sys_config_get_bthing_mqtt_pub_topic(), MGOS_BTHING_ENV_THINGID, id, &(item->pub_topic))) {
    LOG(LL_DEBUG, ("bThing '%s' is going to publish state updates here: %s", id, item->pub_topic));
  } else {
    LOG(LL_ERROR, ("Error: '%s' won't publish state updates becuase an invalid [bthing.mqtt.pub.topic] cfg.", id));
  }

  #if MGOS_BTHING_HAVE_ACTUATORS
  if (mgos_bthing_is_typeof(thing, MGOS_BTHING_TYPE_ACTUATOR)) {
    if (mg_bthing_sreplace(mgos_sys_config_get_bthing_mqtt_sub_topic(), MGOS_BTHING_ENV_THINGID, id, &(item->sub_topic))) {
      LOG(LL_DEBUG, ("bThing '%s' is going to listen to set-state messages here: %s", id, item->sub_topic));
    } else {
      LOG(LL_ERROR, ("Error: '%s' won't receive set-state messages becuase an invalid [bthing.mqtt.sub.topic] cfg.", id));
    }
  }
  #endif //MGOS_BTHING_HAVE_ACTUATORS

  mgos_bthing_mqtt_enable(thing, true);
}
#endif //MGOS_BTHING_HAVE_SHADOW

#if MGOS_BTHING_HAVE_SENSORS
static bool mg_bthing_mqtt_pub_state(const char *topic, mgos_bvarc_t state) {
  if (topic) {
    char *payload = NULL;
    enum mgos_bvar_type state_type = mgos_bvar_get_type(state);
    if (state_type == MGOS_BVAR_TYPE_STR) {
      payload = (char *)mgos_bvar_get_str(state);
    } else {
      payload = json_asprintf("%M", json_printf_bvar, state);
    }
    if (payload) {
      int ret = mg_bthing_mqtt_pub(topic, payload, mgos_sys_config_get_bthing_mqtt_retain());
      if (state_type != MGOS_BVAR_TYPE_STR) free(payload);
      return (ret > 0);
    }
  }
  return false;
}

static void mg_bthing_mqtt_on_state_changed(int ev, void *ev_data, void *userdata) {
  if (!mgos_mqtt_global_is_connected()) return;
  
  #ifdef MGOS_BTHING_HAVE_SHADOW
  LOG(LL_INFO, ("Publishig the shadow..."));
  struct mgos_bthing_shadow_state *state = (struct mgos_bthing_shadow_state *)ev_data;
  if (!mg_bthing_mqtt_pub_state(mgos_sys_config_get_bthing_mqtt_pub_topic(), state->full_shadow)) {
    LOG(LL_ERROR, ("Error publishing '%s' shadow.", mgos_sys_config_get_device_id()));
  }

  #else
  mgos_bthing_t thing = (mgos_bthing_t)ev_data;
  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
  if (item && item->enabled) {
    if (!mg_bthing_mqtt_pub_state(item->pub_topic, mg_bthing_get_raw_state(item->thing))) {
      LOG(LL_ERROR, ("Error publishing '%s' state.", mgos_bthing_get_id(item->thing)));
    }
  }

  #endif //MGOS_BTHING_HAVE_SHADOW
  (void) userdata;
  (void) ev;
}
#endif //MGOS_BTHING_HAVE_SENSORS

bool mgos_bthing_mqtt_init_topics() {
  // try to replace $device_id placehoder in will_topic 
  char *topic = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_mqtt_will_topic());
  if (topic) {
    LOG(LL_DEBUG, ("[mqtt.will_topic] cfg updated to %s", topic));
    mgos_sys_config_set_mqtt_will_topic(topic);
    free(topic);
  }

  // try to replace $device_id placehoder in disco_topic 
  topic = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_bthing_mqtt_disco_topic());
  if (topic) {
    LOG(LL_DEBUG, ("[bthing.mqtt.disco_topic] cfg updated to %s", topic));
    mgos_sys_config_set_bthing_mqtt_disco_topic(topic);
    free(topic);
  }

  // try to replace $device_id placehoder in pub_topic 
  topic = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_bthing_mqtt_pub_topic());
  if (topic) {
    LOG(LL_DEBUG, ("[bthing.mqtt.pub.topic] cfg updated to %s", topic));
    mgos_sys_config_set_bthing_mqtt_pub_topic(topic);
    free(topic);
  }

  // try to replace $device_id placehoder in pub_topic 
  topic = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_bthing_mqtt_sub_topic());
  if (topic) {
    LOG(LL_DEBUG, ("[bthing.mqtt.sub.topic] cfg updated to %s", topic));
    mgos_sys_config_set_bthing_mqtt_sub_topic(topic);
    free(topic);
  }

  if (mg_bthing_scount(mgos_sys_config_get_bthing_mqtt_sub_topic(), MGOS_BTHING_ENV_THINGID) == 0) {
    #ifndef MGOS_BTHING_HAVE_SHADOW
    LOG(LL_ERROR, ("The [%s] topic is configured for using shadow states, but the [mgos_bthing_shadow] library is missing.",
      "bthing.mqtt.sub.topic"));
    return false;
    #endif
  } else {
    #ifdef MGOS_BTHING_HAVE_SHADOW
    LOG(LL_ERROR, ("The [%s] topic is not configured for using shadow states.",
      "bthing.mqtt.sub.topic"));
    return false;
    #endif
  }

  if (mg_bthing_scount(mgos_sys_config_get_bthing_mqtt_pub_topic(), MGOS_BTHING_ENV_THINGID) == 0) {
    #ifndef MGOS_BTHING_HAVE_SHADOW
    LOG(LL_ERROR, ("The [%s] topic is configured for using shadow states, but the [mgos_bthing_shadow] library is missing.",
      "bthing.mqtt.pub.topic"));
    return false;
    #endif
  } else {
    #ifdef MGOS_BTHING_HAVE_SHADOW
    LOG(LL_ERROR, ("The [%s] topic is not configured for using shadow states.",
      "bthing.mqtt.pub.topic"));
    return false;
    #endif
  }

  return true;
}

bool mgos_bthing_mqtt_init() {
  // initialize pub/sup MQTT topics
  if (!mgos_bthing_mqtt_init_topics()) {
    LOG(LL_ERROR, ("Error initializing pub/sub MQTT topics. See above message/s for more details."));
    return false;
  }

  #ifndef MGOS_BTHING_HAVE_SHADOW
  if (!mgos_event_add_handler(MGOS_EV_BTHING_CREATED, mg_bthing_mqtt_on_created, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_CREATED handler."));
    return false;
  }
  #endif

  #if MGOS_BTHING_HAVE_SENSORS
  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (!mgos_event_add_handler(MGOS_EV_BTHING_SHADOW_CHANGED, mg_bthing_mqtt_on_state_changed, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_SHADOW_CHANGED handler."));
    return false;
  }
  LOG(LL_DEBUG, ("This device is going to publish shadow-state updates here: %s", mgos_sys_config_get_bthing_mqtt_pub_topic()));
  #else
  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGED, mg_bthing_mqtt_on_state_changed, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGED handler."));
    return false;
  }
  #endif //MGOS_BTHING_HAVE_SHADOW
  #endif //MGOS_BTHING_HAVE_SENSORS

  #if MGOS_BTHING_HAVE_ACTUATORS
  #ifdef MGOS_BTHING_HAVE_SHADOW
  // subscribe for receiving set-state shadow messages
  mgos_mqtt_sub(mgos_sys_config_get_bthing_mqtt_sub_topic(), mg_bthing_mqtt_on_set_state, NULL);
  LOG(LL_DEBUG, ("This device is going to listen to set-shadow-state messages here: %s", mgos_sys_config_get_bthing_mqtt_sub_topic()));
  #endif //MGOS_BTHING_HAVE_SHADOW
  #endif //MGOS_BTHING_HAVE_ACTUATORS

  mgos_mqtt_add_global_handler(mg_bthing_mqtt_on_event, NULL);

  // subscribe to the device discovery topic
  mgos_mqtt_sub(mgos_sys_config_get_bthing_mqtt_disco_topic(), mg_bthing_mqtt_on_discovery, NULL);
  
  return true;
}