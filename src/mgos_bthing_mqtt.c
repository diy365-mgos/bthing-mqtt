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

static struct mg_bthing_mqtt_topics s_mqtt_topics;

bool mg_bthing_mqtt_use_shadow() {
  #ifdef MGOS_BTHING_HAVE_SHADOW
  return mgos_sys_config_get_bthing_shadow_enable();
  #else
  return false;
  #endif
}

static void mg_bthing_mqtt_on_set_state(struct mg_connection *, const char *, int, const char *, int, void *);

static void mg_bthing_mqtt_on_get_state(struct mg_connection *nc, const char *topic,
                                        int topic_len, const char *msg, int msg_len,
                                        void *ud) {
  if (ud) {
    // force to update a specific bThing state
    mgos_bthing_update_state(((struct mg_bthing_mqtt_item *)ud)->thing);
  } else {
    // force to update all bThing states
    mgos_bthing_update_states(MGOS_BTHING_TYPE_ANY);
  }

  (void) nc;
  (void) topic;
  (void) topic_len;
  (void) msg;
  (void) msg_len;
}

bool mgos_bthing_mqtt_disable(mgos_bthing_t thing) {
  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    return mgos_bthing_shadow_disable(thing);
  }
  #endif //MGOS_BTHING_HAVE_SHADOW

  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
  if (item && item->enabled) {
    if (!mgos_mqtt_unsub(item->topics.get_state)) return false;
    #if MGOS_BTHING_HAVE_ACTUATORS
    if (mgos_bthing_is_typeof(thing, MGOS_BTHING_TYPE_ACTUATOR)) {
      if (!mgos_mqtt_unsub(item->topics.set_state)) return false;
    }
    #endif //MGOS_BTHING_HAVE_ACTUATORS
    item->enabled = false;
  }
  return true;
}

void mg_bthing_mqtt_enable(mgos_bthing_t thing) {
  if (!mg_bthing_mqtt_use_shadow()) {
    struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
    if (item && !item->enabled) {
      #if MGOS_BTHING_HAVE_ACTUATORS
      if (mgos_bthing_is_typeof(thing, MGOS_BTHING_TYPE_ACTUATOR)) {
        mgos_mqtt_sub(item->topics.set_state, mg_bthing_mqtt_on_set_state, item);
      }
      #endif //MGOS_BTHING_HAVE_ACTUATORS
      mgos_mqtt_sub(item->topics.get_state, mg_bthing_mqtt_on_get_state, item);
      item->enabled = true;
    }
  }
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

static void mg_bthing_mqtt_pub_ping() {
  // publish availability (will message)
  mg_bthing_mqtt_birth_message_pub();
  // force to update all bThing states
  mgos_bthing_update_states(MGOS_BTHING_TYPE_ANY);
}

static void mg_bthing_mqtt_on_cmd(struct mg_connection *nc, const char *topic,
                                  int topic_len, const char *msg, int msg_len,
                                  void *ud) {
  if (!msg || (msg_len == 0)) return;
  if (strncmp(msg, MGOS_BTHING_MQTT_CMD_PING, msg_len) == 0) {
    mg_bthing_mqtt_pub_ping();
  }
  
  (void) nc;
  (void) topic;
  (void) topic_len;
  (void) msg;
  (void) msg_len;
  (void) ud;
}

static void mg_bthing_mqtt_on_event(struct mg_connection *nc,
                                     int ev,
                                     void *ev_data,
                                     void *user_data) {  
  if (ev == MG_EV_MQTT_CONNACK) {
    mg_bthing_mqtt_pub_ping();
  } else if (ev == MG_EV_MQTT_DISCONNECT) {
    // todo
  }
  (void) ev_data;
  (void) nc;
  (void) user_data;
}

#if MGOS_BTHING_HAVE_ACTUATORS
void mg_bthing_mqtt_on_set_state(struct mg_connection *nc, const char *topic,
                                 int topic_len, const char *msg, int msg_len,
                                 void *ud) {
  if (!msg || msg_len == 0) return;

  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    mgos_bthing_shadow_json_set(msg, msg_len);
  }
  #endif //MGOS_BTHING_HAVE_SHADOW

  if (!mg_bthing_mqtt_use_shadow()) {
    struct mg_bthing_mqtt_item *item = (struct mg_bthing_mqtt_item *)ud; 
    if (item && item->enabled) {
      mgos_bvar_t state = NULL;
      if (!mgos_bvar_json_try_bscanf(msg, msg_len, &state)) {
        state = mgos_bvar_new_nstr(msg, msg_len);
      }
      mgos_bthing_set_state(item->thing, state);
      mgos_bvar_free(state);
    }
  }

  (void) nc;
  (void) topic;
  (void) topic_len;
}
#endif //MGOS_BTHING_HAVE_ACTUATORS

static void mg_bthing_mqtt_on_created(int ev, void *ev_data, void *userdata) {
  if (ev != MGOS_EV_BTHING_CREATED) return;
  mgos_bthing_t thing = (mgos_bthing_t)ev_data;
  const char *id = mgos_bthing_get_id(thing);

  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_add_item(thing);

  mg_bthing_sreplace(mgos_sys_config_get_bthing_mqtt_state_updated_topic(),
    MGOS_BTHING_ENV_THINGID, id, &(item->topics.state_updated));

  mg_bthing_sreplace(mgos_sys_config_get_bthing_mqtt_get_state_topic(),
    MGOS_BTHING_ENV_THINGID, id, &(item->topics.get_state));

  #if MGOS_BTHING_HAVE_ACTUATORS
  if (mgos_bthing_is_typeof(thing, MGOS_BTHING_TYPE_ACTUATOR)) {
    mg_bthing_sreplace(mgos_sys_config_get_bthing_mqtt_set_state_topic(), 
      MGOS_BTHING_ENV_THINGID, id, &(item->topics.set_state));
  }
  #endif //MGOS_BTHING_HAVE_ACTUATORS

  mg_bthing_mqtt_enable(thing);
}

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
      LOG(LL_INFO, ("PUBLISHING on '%s'", topic)); //CANCEL
      int ret = mg_bthing_mqtt_pub(topic, payload, mgos_sys_config_get_bthing_mqtt_retain());
      if (state_type != MGOS_BVAR_TYPE_STR) free(payload);
      return (ret > 0);
    }
  }
  LOG(LL_INFO, ("FAILED TO PUBLISH")); //CANCEL
  return false;
}

static bool mg_bthing_mqtt_try_pub_state(void *state_data) {
  LOG(LL_INFO, ("mg_bthing_mqtt_try_pub_state()...")); //CANCEL
  if (!mgos_mqtt_global_is_connected()) return false;
  
  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    LOG(LL_INFO, ("mg_bthing_mqtt_try_pub_state() ON SHADOW...")); //CANCEL
    mgos_bvarc_t state = (mgos_sys_config_get_bthing_mqtt_pub_delta_shadow() ?
      ((struct mgos_bthing_shadow_state *)state_data)->delta_shadow : ((struct mgos_bthing_shadow_state *)state_data)->full_shadow);
    if (!mg_bthing_mqtt_pub_state(s_mqtt_topics.state_updated, state)) {
      LOG(LL_ERROR, ("Error publishing '%s' shadow.", mgos_sys_config_get_device_id()));
      return false;
    }
  }
  #endif //MGOS_BTHING_HAVE_SHADOW

  if (!mg_bthing_mqtt_use_shadow()) {
    struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(((struct mgos_bthing_state *)state_data)->thing);
    LOG(LL_INFO, ("mg_bthing_mqtt_try_pub_state() NOT ON SHADOW...")); //CANCEL
    if (item && item->enabled) {
      LOG(LL_INFO, ("mg_bthing_mqtt_try_pub_state() NOT ON SHADOW #2...")); //CANCEL
      if (!mg_bthing_mqtt_pub_state(item->topics.state_updated, ((struct mgos_bthing_state *)state_data)->state)) {
        LOG(LL_ERROR, ("Error publishing '%s' state.", mgos_bthing_get_uid(((struct mgos_bthing_state *)state_data)->thing)));
        return false;
      }
    }
  }
  return true;
}

static void mg_bthing_mqtt_on_state_changed(int ev, void *ev_data, void *userdata) {
  mg_bthing_mqtt_try_pub_state(ev_data);
  (void) userdata;
  (void) ev;
}

static void mg_bthing_mqtt_on_state_updated(int ev, void *ev_data, void *userdata) {
  enum mgos_bthing_state_flag state_flags = MGOS_BTHING_STATE_FLAG_UNCHANGED ;
  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    state_flags = ((struct mgos_bthing_shadow_state *)ev_data)->state_flags;
  }
  #endif //MGOS_BTHING_HAVE_SHADOW

  if (!mg_bthing_mqtt_use_shadow()) {
    state_flags = ((struct mgos_bthing_state *)ev_data)->state_flags;
  }

  if ((state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) != MGOS_BTHING_STATE_FLAG_CHANGED) {
    // The state/get topic has been invoked (most probably),
    // or, in any case, the state has been updated, so I must try to publish it.
    mg_bthing_mqtt_try_pub_state(ev_data);
  }

  (void) userdata;
  (void) ev;
}

#endif //MGOS_BTHING_HAVE_SENSORS

bool mgos_bthing_mqtt_init_topics() {
  if (!mg_bthing_mqtt_use_shadow()) {
    const char *thing_id_err = "Unable to init topics. The topic [%s] must contains the '%s' placeholder.";
    if (mg_bthing_scount(mgos_sys_config_get_bthing_mqtt_state_updated_topic(), MGOS_BTHING_ENV_THINGID) == 0) {
      LOG(LL_ERROR, (thing_id_err, "bthing.mqtt.state_updated_topic", MGOS_BTHING_ENV_THINGID));
      return false;
    }
    if (mg_bthing_scount(mgos_sys_config_get_bthing_mqtt_set_state_topic(), MGOS_BTHING_ENV_THINGID) == 0) {
      LOG(LL_ERROR, (thing_id_err, "bthing.mqtt.set_state_topic", MGOS_BTHING_ENV_THINGID));
      return false;
    }
    if (mg_bthing_scount(mgos_sys_config_get_bthing_mqtt_get_state_topic(), MGOS_BTHING_ENV_THINGID) == 0) {
      LOG(LL_ERROR, (thing_id_err, "bthing.mqtt.get_state_topic", MGOS_BTHING_ENV_THINGID));
      return false;
    }
  }

  const char *cfg_upd = "Setting [%s] updated to %s";
  // try to replace $device_id placehoder in 'will_topic' 
  char *topic = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_mqtt_will_topic());
  if (topic) {
    LOG(LL_DEBUG, (cfg_upd, "mqtt.will_topic", topic));
    mgos_sys_config_set_mqtt_will_topic(topic);
    free(topic);
  }

  // try to replace $device_id placehoder in 'get_state_topic '
  topic = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_bthing_mqtt_get_state_topic());
  if (topic) {
    LOG(LL_DEBUG, (cfg_upd, "bthing.mqtt.get_state_topic", topic));
    mgos_sys_config_set_bthing_mqtt_get_state_topic(topic);
    free(topic);
  }

  // try to replace $device_id placehoder in 'state_updated_topic' 
  topic = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_bthing_mqtt_state_updated_topic());
  if (topic) {
    LOG(LL_DEBUG, (cfg_upd, "bthing.mqtt.state_updated_topic", topic));
    mgos_sys_config_set_bthing_mqtt_state_updated_topic(topic);
    free(topic);
  }

  // try to replace $device_id placehoder in 'set_state_topic' 
  topic = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_bthing_mqtt_set_state_topic());
  if (topic) {
    LOG(LL_DEBUG, (cfg_upd, "bthing.mqtt.set_state_topic", topic));
    mgos_sys_config_set_bthing_mqtt_set_state_topic(topic);
    free(topic);
  }

  // initialize 'get_state' topic as <device> topic (discard any $bthing_id placeholders)
  s_mqtt_topics.get_state = NULL;
  mg_bthing_sreplaces(mgos_sys_config_get_bthing_mqtt_get_state_topic(), &s_mqtt_topics.get_state,
    2, MGOS_BTHING_ENV_THINGID, "", "//", "/");
  if (!s_mqtt_topics.get_state) s_mqtt_topics.get_state = (char *)mgos_sys_config_get_bthing_mqtt_get_state_topic();

  // initialize 'broadcast_cmd' topic (discard any $device_id placeholders)
  s_mqtt_topics.broadcast_cmd = NULL;
  mg_bthing_sreplaces(mgos_sys_config_get_bthing_mqtt_cmd_topic(), &s_mqtt_topics.broadcast_cmd,
    2, MGOS_BTHING_ENV_DEVICEID, "", "//", "/");
  if (!s_mqtt_topics.broadcast_cmd) s_mqtt_topics.broadcast_cmd = (char *)mgos_sys_config_get_bthing_mqtt_cmd_topic();

  // try to replace $device_id placehoder in 'state_updated_topic'
  s_mqtt_topics.cmd = mg_bthing_mqtt_build_device_topic(mgos_sys_config_get_bthing_mqtt_cmd_topic());

  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    s_mqtt_topics.set_state = NULL;
    mg_bthing_sreplaces(mgos_sys_config_get_bthing_mqtt_set_state_topic(), &s_mqtt_topics.set_state, 2, MGOS_BTHING_ENV_THINGID, "", "//", "/");
    if (!s_mqtt_topics.set_state) s_mqtt_topics.set_state = (char *)mgos_sys_config_get_bthing_mqtt_set_state_topic();

    s_mqtt_topics.state_updated = NULL;
    mg_bthing_sreplaces(mgos_sys_config_get_bthing_mqtt_state_updated_topic(), &s_mqtt_topics.state_updated, 2, MGOS_BTHING_ENV_THINGID, "", "//", "/");
    if (!s_mqtt_topics.state_updated) s_mqtt_topics.state_updated = (char *)mgos_sys_config_get_bthing_mqtt_state_updated_topic();
  }
  #else
    s_mqtt_topics.state_updated = NULL;
    s_mqtt_topics.set_state = NULL;
  #endif

  return true;
}

bool mgos_bthing_mqtt_init() {
  // initialize pub/sup MQTT topics
  if (!mgos_bthing_mqtt_init_topics()) {
    LOG(LL_ERROR, ("Error initializing pub/sub MQTT topics. See above message/s for more details."));
    return false;
  }

  if (!mg_bthing_mqtt_use_shadow()) {
    if (!mgos_event_add_handler(MGOS_EV_BTHING_CREATED, mg_bthing_mqtt_on_created, NULL)) {
      LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_CREATED handler."));
      return false;
    }
  }

  #if MGOS_BTHING_HAVE_SENSORS
  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    if (!mgos_event_add_handler(MGOS_EV_BTHING_SHADOW_CHANGED, mg_bthing_mqtt_on_state_changed, NULL)) {
      LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_SHADOW_CHANGED handler."));
      return false;
    }
    if (!mgos_event_add_handler(MGOS_EV_BTHING_SHADOW_UPDATED, mg_bthing_mqtt_on_state_updated, NULL)) {
      LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_SHADOW_UPDATED handler."));
      return false;
    }
    LOG(LL_DEBUG, ("This device is going to publish shadow-state updates here: %s",
      mgos_sys_config_get_bthing_mqtt_state_updated_topic()));
  }
  #endif //MGOS_BTHING_HAVE_SHADOW
  if (!mg_bthing_mqtt_use_shadow()) {
    if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGED, mg_bthing_mqtt_on_state_changed, NULL)) {
      LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGED handler."));
      return false;
    }
    if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_UPDATED, mg_bthing_mqtt_on_state_updated, NULL)) {
      LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_UPDATED handler."));
      return false;
    }
  }
  #endif //MGOS_BTHING_HAVE_SENSORS

  #if MGOS_BTHING_HAVE_ACTUATORS
  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    // subscribe for receiving set-state shadow messages
    if (s_mqtt_topics.set_state) {
      mgos_mqtt_sub(s_mqtt_topics.set_state, mg_bthing_mqtt_on_set_state, NULL);
      LOG(LL_DEBUG, ("This device is going to listen to set-shadow-state messages here: %s", s_mqtt_topics.set_state));
    } else {
      LOG(LL_ERROR, ("Invalid NULL set_state_topic setting. This device won't receive 'set-state' commands."));
    }
  }
  #endif //MGOS_BTHING_HAVE_SHADOW
  #endif //MGOS_BTHING_HAVE_ACTUATORS

  // subscribe to the device get-state topic
  if (s_mqtt_topics.get_state) {
    mgos_mqtt_sub(s_mqtt_topics.get_state, mg_bthing_mqtt_on_get_state, NULL);
    LOG(LL_DEBUG, ("This device listen to 'get-state' commands here: %s", s_mqtt_topics.get_state));
  }

  if (s_mqtt_topics.broadcast_cmd) {
    mgos_mqtt_sub(s_mqtt_topics.broadcast_cmd, mg_bthing_mqtt_on_cmd, NULL);
    LOG(LL_DEBUG, ("This device listen to broadcast commands here: %s", s_mqtt_topics.broadcast_cmd));
  }

  if (s_mqtt_topics.cmd) {
    mgos_mqtt_sub(s_mqtt_topics.cmd, mg_bthing_mqtt_on_cmd, NULL);
    LOG(LL_DEBUG, ("This device listen to commands here: %s", s_mqtt_topics.cmd));
  }
 
  mgos_mqtt_add_global_handler(mg_bthing_mqtt_on_event, NULL);

  return true;
}