#include "mgos.h"
#include "mgos_bthing_mqtt.h"
#include "mg_bthing_mqtt_sdk.h"
#include "mgos_bvar_json.h"

#ifdef MGOS_BTHING_MQTT_AGGREGATE_MODE
#include "mgos_bvar_dic.h"
#endif

#ifdef MGOS_HAVE_MJS
#include "mjs.h"
#endif

enum mg_bthing_mqtt_mode {
  MG_BTHING_MQTT_MODE_SINGLE,
  MG_BTHING_MQTT_MODE_AGGREGATE
};

struct mgos_bthing_mqtt_ctx {
  enum mg_bthing_mqtt_mode pub_mode;
  enum mg_bthing_mqtt_mode sub_mode;
  bool publishing;
};

static struct mgos_bthing_mqtt_ctx s_context;

static void mg_bthing_mqtt_on_set_state(struct mg_connection *, const char *, int, const char *, int, void *);

bool mgos_bthing_mqtt_enable(mgos_bthing_t thing, bool enable) {
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
  struct mg_bthing_mqtt_item *item = (struct mg_bthing_mqtt_item *)ud; 
  if (!msg || (item && !item->enabled)) return;

  mgos_bvar_t state = NULL;
  mgos_bvar_json_try_bscanf(msg, msg_len, &state);

  if (s_context.sub_mode == MG_BTHING_MQTT_MODE_SINGLE && item) {
    if (!state) state = mgos_bvar_new_nstr(msg, msg_len);
    mgos_bthing_set_state(item->thing, state);
  } else if ((s_context.sub_mode == MG_BTHING_MQTT_MODE_AGGREGATE) && state) {
    #ifdef MGOS_BTHING_MQTT_AGGREGATE_MODE
    const char *key_name;
    mgos_bvarc_t key_val;
    mgos_bvarc_enum_t keys = mgos_bvarc_get_keys(state);
    while (mgos_bvarc_get_next_key(&keys, &key_val, &key_name)) {
      mgos_bthing_set_state(mgos_bthing_get(key_name), key_val);
    }
    #endif
  }

  mgos_bvar_free(state);

  (void) nc;
  (void) topic;
  (void) topic_len;
}

#endif // #if MGOS_BTHING_HAVE_ACTUATORS

static void mg_bthing_mqtt_on_created(int ev, void *ev_data, void *userdata) {
  if (ev != MGOS_EV_BTHING_CREATED) return;
  mgos_bthing_t thing = (mgos_bthing_t)ev_data;

  // create new item
  struct mg_bthing_mqtt_item *item = calloc(1, sizeof(struct mg_bthing_mqtt_item));
  item->thing = thing;
  item->enabled = false;
  // add new item to the global item list
  mg_bthing_mqtt_add_item(item);

  if (s_context.pub_mode == MG_BTHING_MQTT_MODE_SINGLE) {
    if (mg_bthing_sreplace(mgos_sys_config_get_bthing_mqtt_pub_topic(), MGOS_BTHING_ENV_THINGID, mgos_bthing_get_id(thing), &(item->pub_topic))) {
      LOG(LL_DEBUG, ("bThing '%s' is going to publish state updates here: %s", mgos_bthing_get_id(thing), item->pub_topic));
    } else {
      LOG(LL_ERROR, ("Error: '%s' won't publish state updates becuase an invalid [bthing.mqtt.pub.topic] cfg.", mgos_bthing_get_id(thing)));
    }
  }

  #if MGOS_BTHING_HAVE_ACTUATORS
  if (s_context.sub_mode == MG_BTHING_MQTT_MODE_SINGLE) {
    if (mgos_bthing_is_typeof(thing, MGOS_BTHING_TYPE_ACTUATOR)) {
      if (mg_bthing_sreplace(mgos_sys_config_get_bthing_mqtt_sub_topic(), MGOS_BTHING_ENV_THINGID, mgos_bthing_get_id(thing), &(item->sub_topic))) {
        LOG(LL_DEBUG, ("bThing '%s' is going to listen to set-state messages here: %s", mgos_bthing_get_id(thing), item->sub_topic));
      } else {
        LOG(LL_ERROR, ("Error: '%s' won't receive set-state messages becuase an invalid [bthing.mqtt.sub.topic] cfg.", mgos_bthing_get_id(thing)));
      }
    }
  }
  #endif // MGOS_BTHING_HAVE_ACTUATORS

  mgos_bthing_mqtt_enable(thing, true);
}

#if MGOS_BTHING_HAVE_SENSORS

static bool mg_bthing_mqtt_pub_state(const char *topic, mgos_bthing_t thing) {
  if (topic) {
    char *payload = NULL;
    mgos_bvarc_t state = mgos_bthing_get_state(thing);
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
  LOG(LL_ERROR, ("Error publishing '%s' state on topic %s.", mgos_bthing_get_id(thing), (topic ? topic : "")));
  return false;
}

static void mg_bthing_mqtt_on_state_changed(int ev, void *ev_data, void *userdata) {
  if (!mgos_mqtt_global_is_connected()) return;
  
  mgos_bthing_t thing = (mgos_bthing_t)ev_data;

  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
  if (!item || !item->enabled) return;  

  const char* topic = NULL;
  if (s_context.pub_mode == MG_BTHING_MQTT_MODE_SINGLE) {
    topic = item->pub_topic;
  } else {
    // TODO: assing global topic
    topic = NULL;
  }

  mg_bthing_mqtt_pub_state(topic, thing);

  (void) userdata;
  (void) ev;
}

#endif //MGOS_BTHING_HAVE_SENSORS

void mgos_bthing_mqtt_init_topics() {
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
}

bool mgos_bthing_mqtt_init_context();
  const char *err1 = "The [%s] is configured for using the AGGREGATE mode, but it is not enbled.";
  const char *err2 = "Add 'cdefs: MGOS_BTHING_MQTT_AGGREGATE_MODE: 1' to the mos.yml file for enabling the AGGREGATE mode.";
  s_context.publishing = false;
  if (mg_bthing_scount(mgos_sys_config_get_bthing_mqtt_sub_topic(), MGOS_BTHING_ENV_THINGID) == 0) {
    #ifdef MGOS_BTHING_MQTT_AGGREGATE_MODE
    s_context.sub_mode = MG_BTHING_MQTT_MODE_AGGREGATE;
    #else
    LOG(LL_ERROR, (err1, "bthing.mqtt.sub.topic"));
    LOG(LL_ERROR, (err2));
    return false;
    #endif
  } else {
    s_context.sub_mode = MG_BTHING_MQTT_MODE_SINGLE;
  }
  if (mg_bthing_scount(mgos_sys_config_get_bthing_mqtt_pub_topic(), MGOS_BTHING_ENV_THINGID) == 0) {
    #ifdef MGOS_BTHING_MQTT_AGGREGATE_MODE
    s_context.pub_mode = MG_BTHING_MQTT_MODE_AGGREGATE;
    #else
    LOG(LL_ERROR, (err1, "bthing.mqtt.pub.topic"));
    LOG(LL_ERROR, (err2));
    return false;
    #endif
  } else {
    s_context.pub_mode = MG_BTHING_MQTT_MODE_SINGLE;
  }

  return true;
}

bool mgos_bthing_mqtt_init() {
  // initialize defualt topic values
  mgos_bthing_mqtt_init_topics();
  // initialize the context
  if (!mgos_bthing_mqtt_init_context()) {
    LOG(LL_ERROR, ("Error initializing the context. See above message/s for more details."));
    return false;
  }

  if (!mgos_event_add_handler(MGOS_EV_BTHING_CREATED, mg_bthing_mqtt_on_created, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_CREATED handler."));
    return false;
  }

  #if MGOS_BTHING_HAVE_SENSORS
  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGED, mg_bthing_mqtt_on_state_changed, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGED handler."));
    return false;
  }
  #endif

  if (s_context.sub_mode == MG_BTHING_MQTT_MODE_AGGREGATE) {
    // subscribe for receiving set-state messages in AGGREGATE mode
    mgos_mqtt_sub(mgos_sys_config_get_bthing_mqtt_sub_topic(), mg_bthing_mqtt_on_set_state, NULL);
    return false;
  }

  mgos_mqtt_add_global_handler(mg_bthing_mqtt_on_event, NULL);

  // subscribe to the device discovery topic
  mgos_mqtt_sub(mgos_sys_config_get_bthing_mqtt_disco_topic(), mg_bthing_mqtt_on_discovery, NULL);
  
  return true;
}