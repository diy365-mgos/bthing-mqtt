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

#define MG_TMPBUF_SIZE 50
static char s_tmpbuf1[MG_TMPBUF_SIZE];
static char s_tmpbuf2[MG_TMPBUF_SIZE];

static char *s_state_updated_topic = NULL;

bool mg_bthing_mqtt_use_shadow() {
  #ifdef MGOS_BTHING_HAVE_SHADOW
  return mgos_sys_config_get_bthing_shadow_enable();
  #else
  return false;
  #endif
}

bool mgos_bthing_mqtt_disable(mgos_bthing_t thing) {
  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    return mgos_bthing_shadow_disable(thing);
  }
  #endif //MGOS_BTHING_HAVE_SHADOW

  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
  if (item && item->enabled) {
    item->enabled = false;
  }
  return true;
}

void mg_bthing_mqtt_enable(mgos_bthing_t thing) {
  if (!mg_bthing_mqtt_use_shadow()) {
    struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
    if (item && !item->enabled) {
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

static void mg_bthing_mqtt_pub_ping_response() {
  // publish availability (will message)
  mg_bthing_mqtt_birth_message_pub();
  // force to update all bThing states
  mgos_bthing_update_states(MGOS_BTHING_FILTER_BY_NOTHING);
}

static bool mg_bthing_mqtt_update_item_state(const char* id_or_domain, const char *domain) {
  mgos_bthing_t thing = (domain ? mgos_bthing_get_by_id(id_or_domain, domain) : mgos_bthing_get_by_id(id_or_domain, NULL));
  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
  if (item && !item->enabled) return false;

  if (item) {
    // update one single thing
    mgos_bthing_update_state(item->thing);
  } else {
    // (try) update all things in the domain
    mgos_bthing_update_states(MGOS_BTHING_FILTER_BY_DOMAIN, s_tmpbuf1);
  }
  return false;
}

#if MGOS_BTHING_HAVE_ACTUATORS

static bool mg_bthing_mqtt_set_item_state(const char* id_or_domain, const char *domain, const char* state, int state_len) {
  if (state == NULL || state_len <= 0) return false; 

  mgos_bthing_t thing = (domain ? mgos_bthing_get_by_id(id_or_domain, domain) : mgos_bthing_get_by_id(id_or_domain, NULL));
  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(thing);
  if (item && !item->enabled) return false;

  mgos_bvar_t var_state = NULL;
  if (!mgos_bvar_json_try_bscanf(state, state_len, &var_state)) {
    var_state = mgos_bvar_new_nstr(state, state_len);
  }

  bool ret = true;
  if (item) {
    // ste the state of one single thing
    ret = mgos_bthing_set_state(item->thing, var_state);
  } else {
    // ste the state of all things ij a domain
    mgos_bthing_enum_t things = mgos_bthing_get_all();
    while(mgos_bthing_filter_get_next(&things, &thing, MGOS_BTHING_FILTER_BY_DOMAIN, id_or_domain) && ret) {
      ret = mgos_bthing_set_state(thing, var_state);
    }
  }
  mgos_bvar_free(var_state);
  return ret;
}

#endif // MGOS_BTHING_HAVE_ACTUATORS

static void mg_bthing_mqtt_on_event(struct mg_connection *nc,
                                     int ev,
                                     void *ev_data,
                                     void *user_data) {  
  if (ev == MG_EV_MQTT_CONNACK) {
    mg_bthing_mqtt_pub_ping_response();
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
  
  struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_add_item(thing);

  const char *domain = mgos_bthing_get_domain(thing);
  if (domain) {
    item->state_updated_topic = mgos_bthing_sjoin("/", 6,
    mgos_sys_config_get_bthing_mqtt_topic_dom(), mgos_sys_config_get_device_id(),
      domain, mgos_bthing_get_id(thing), "state", "updated");
  } else {
    item->state_updated_topic = mgos_bthing_sjoin("/", 5,
    mgos_sys_config_get_bthing_mqtt_topic_dom(), mgos_sys_config_get_device_id(),
      mgos_bthing_get_id(thing), "state", "updated");
  }

  LOG(LL_DEBUG, ("bThing '%s' is going to publish state here: %s",
    mgos_bthing_get_uid(thing), item->state_updated_topic));

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
      int ret = mg_bthing_mqtt_pub(topic, payload, mgos_sys_config_get_bthing_mqtt_retain());
      if (state_type != MGOS_BVAR_TYPE_STR) free(payload);
      return (ret > 0);
    }
  }
  return false;
}

static bool mg_bthing_mqtt_try_pub_state(void *state_data) {
  if (!mgos_mqtt_global_is_connected()) return false;
  
  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    mgos_bvarc_t state = (mgos_sys_config_get_bthing_mqtt_pub_delta_shadow() ?
      ((struct mgos_bthing_shadow_state *)state_data)->delta_shadow : ((struct mgos_bthing_shadow_state *)state_data)->full_shadow);
    if (!mg_bthing_mqtt_pub_state(s_state_updated_topic, state)) {
      LOG(LL_ERROR, ("Error publishing '%s' shadow.", mgos_sys_config_get_device_id()));
      return false;
    }
  }
  #endif //MGOS_BTHING_HAVE_SHADOW

  if (!mg_bthing_mqtt_use_shadow()) {
    struct mg_bthing_mqtt_item *item = mg_bthing_mqtt_get_item(((struct mgos_bthing_state *)state_data)->thing);
    if (item && item->enabled) {
      if (!mg_bthing_mqtt_pub_state(item->state_updated_topic, ((struct mgos_bthing_state *)state_data)->state)) {
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

/* 
  Topic's handle for: ${topic_dom}/cmd 
*/
static void mg_bthing_mqtt_on_broadcast_cmd(struct mg_connection *nc, const char *topic,
                                         int topic_len, const char *msg, int msg_len, void *ud) {
  if (!msg || (msg_len == 0)) return;

  /* COMMAND: 'ping' */
  if (strncmp(msg, MGOS_BTHING_MQTT_CMD_PING, msg_len) == 0) {
    mg_bthing_mqtt_pub_ping_response();
    return; // DONE
  }
  (void) nc; (void) topic; (void) topic_len; (void) ud;
}

/* 
  Topic's handle for: ${topic_dom}/${device_id}/cmd
*/
static void mg_bthing_mqtt_on_cmd(struct mg_connection *nc, const char *topic,
                                  int topic_len, const char *msg, int msg_len, void *ud) {
  if (!msg || (msg_len == 0)) return;

  /* COMMAND: 'ping' */
  if (strncmp(msg, MGOS_BTHING_MQTT_CMD_PING, msg_len) == 0) {
    mg_bthing_mqtt_pub_ping_response();
    return; // DONE
  }
  (void) nc; (void) topic; (void) topic_len; (void) ud;
}

#ifdef MGOS_BTHING_HAVE_SHADOW

/* 
  Topic's handle for: ${topic_dom}/${device_id}/state/+
    - ${topic_dom}/${device_id}/state/get
    - ${topic_dom}/${device_id}/state/set  
*/
static void mg_bthing_mqtt_on_shadow_state_cmd(struct mg_connection *nc, const char *topic,
                                               int topic_len, const char *msg, int msg_len, void *ud) {
  int seg_len;
  const char *seg_val;
  if (!mg_bthing_mqtt_use_shadow()) return;

  // seg_val = verb (get or set)
  if (!mg_bthing_path_get_segment(topic, topic_len, '/', 3, &seg_val, &seg_len))
    return; // missing topic segment #3

  /* MENAGE /state/get topic */
  if (strncmp(seg_val, MGOS_BTHING_MQTT_VERB_GET, seg_len) == 0) {
    // force to update all bThing states (and the shadow consequently)
    mgos_bthing_update_states(MGOS_BTHING_FILTER_BY_NOTHING);
    return; // DONE
  }
  
  #if MGOS_BTHING_HAVE_ACTUATORS
  /* MENAGE /state/set topic */
  if (strncmp(seg_val, MGOS_BTHING_MQTT_VERB_SET, seg_len) == 0 && msg_len > 0) {
    mgos_bthing_shadow_json_set(msg, msg_len);
    return; // DONE
  }
  #endif //MGOS_BTHING_HAVE_ACTUATORS

  (void) nc; (void) topic; (void) topic_len; (void) msg; (void) msg_len; (void) ud;
}

#endif // MGOS_BTHING_HAVE_SHADOW

/* 
  Topic's handle for: ${topic_dom}/${device_id}/+/state/+
    - ${topic_dom}/${device_id}/${bthing_id}/state/get
    - ${topic_dom}/${device_id}/${bthing_dom}/state/get
    - ${topic_dom}/${device_id}/${bthing_id}/state/set
    - ${topic_dom}/${device_id}/${bthing_dom}/state/set
*/
static void mg_bthing_mqtt_on_state_cmd1(struct mg_connection *nc, const char *topic,
                                         int topic_len, const char *msg, int msg_len, void *ud) {
  int seg_len;
  const char *seg_val;
  if (mg_bthing_mqtt_use_shadow()) return;

  // s_tmpbuf1 = ${bthing_id} or ${bthing_dom}
  if (!mg_bthing_path_get_segment(topic, topic_len, '/', 2, &seg_val, &seg_len))
    return; // missing topic segment #2
  strncpy(s_tmpbuf1, seg_val, seg_len);
  s_tmpbuf1[seg_len] = '\0';

  // seg_val = verb (get or set)
  if (!mg_bthing_path_get_segment(topic, topic_len, '/', 4, &seg_val, &seg_len))
    return; // missing topic segment #4

  /* MANAGE /state/get topics */
  if (strncmp(seg_val, MGOS_BTHING_MQTT_VERB_GET, seg_len) == 0) {
    mg_bthing_mqtt_update_item_state(s_tmpbuf1, NULL);
    return; // DONE
  }
  
  #if MGOS_BTHING_HAVE_ACTUATORS
  /* MANAGE /state/set topics */
  if (strncmp(seg_val, MGOS_BTHING_MQTT_VERB_SET, seg_len) == 0 && msg_len > 0) {
    mg_bthing_mqtt_set_item_state(s_tmpbuf1, NULL, msg, msg_len);
    return; // DONE
  }
  #endif //MGOS_BTHING_HAVE_ACTUATORS

  (void) nc; (void) msg; (void) msg_len; (void) ud;
}

/* 
  Topic's handle for: ${topic_dom}/${device_id}/+/+/state/+
    - ${topic_dom}/${device_id}/${bthing_dom}/${bthing_id}/state/get
    - ${topic_dom}/${device_id}/${bthing_dom}/${bthing_id}/state/set
*/
static void mg_bthing_mqtt_on_state_cmd2(struct mg_connection *nc, const char *topic,
                                         int topic_len, const char *msg, int msg_len, void *ud) {
  int seg_len;
  const char *seg_val;

  // s_tmpbuf1 = ${bthing_dom}
  if (!mg_bthing_path_get_segment(topic, topic_len, '/', 2, &seg_val, &seg_len))
    return; // missing topic segment #2
  strncpy(s_tmpbuf1, seg_val, seg_len);
  s_tmpbuf1[seg_len] = '\0';

  // s_tmpbuf2 = ${bthing_id}
  if (!mg_bthing_path_get_segment(topic, topic_len, '/', 3, &seg_val, &seg_len))
    return; // missing topic segment #3
  strncpy(s_tmpbuf2, seg_val, seg_len);
  s_tmpbuf2[seg_len] = '\0';

  // seg_val = verb (get or set)
  if (!mg_bthing_path_get_segment(topic, topic_len, '/', 5, &seg_val, &seg_len))
    return; // missing topic segment #5

  /* MANAGE /state/get topic */
  if (strncmp(seg_val, MGOS_BTHING_MQTT_VERB_GET, seg_len) == 0) {
    mg_bthing_mqtt_update_item_state(s_tmpbuf2, s_tmpbuf1);
    return; // DONE
  }
  
  #if MGOS_BTHING_HAVE_ACTUATORS
  /* MANAGE /state/set topic */
  if (strncmp(seg_val, MGOS_BTHING_MQTT_VERB_SET, seg_len) == 0 && msg_len > 0) {
    mg_bthing_mqtt_set_item_state(s_tmpbuf2, s_tmpbuf1, msg, msg_len);
    return; // DONE
  }
  #endif //MGOS_BTHING_HAVE_ACTUATORS

  (void) nc; (void) topic; (void) topic_len; (void) msg; (void) msg_len; (void) ud;
}

bool mg_bthing_mqtt_init_topics() {
  char *topic = NULL;
  const char *cfg_upd = "Setting [%s] updated to %s";
  const char* device_id = mgos_sys_config_get_device_id();
  const char *topic_dom = mgos_sys_config_get_bthing_mqtt_topic_dom();

  /* Set the will topic */
  // ${topic_dom}/${device_id}/LWT
  topic = mgos_bthing_sjoin("/", 3, topic_dom, device_id, "LWT");
  LOG(LL_DEBUG, (cfg_upd, "mqtt.will_topic", topic));
  mgos_sys_config_set_mqtt_will_topic(topic);
  free(topic);

  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    s_state_updated_topic = mgos_bthing_sjoin("/", 4, topic_dom, device_id, "state", "updated");
    LOG(LL_DEBUG, ("Shadow-state updates are going to be published here: %s", s_state_updated_topic));
  }
  #else
  s_state_updated_topic = NULL;
  #endif

  return true;
}

bool mg_bthing_mqtt_sub_topics() {
  char *topic = NULL;
  const char* device_id = mgos_sys_config_get_device_id();
  const char *topic_dom = mgos_sys_config_get_bthing_mqtt_topic_dom();
  if (!device_id || !topic_dom || strlen(topic_dom) == 0) return false;

  /* COMMON TOPICS */

  // ${topic_dom}/cmd
  topic = mgos_bthing_sjoin("/", 2, topic_dom, "cmd");
  mgos_mqtt_sub(topic, mg_bthing_mqtt_on_broadcast_cmd, NULL);
  LOG(LL_DEBUG, ("Looking for broadcast commands here: %s", topic));
  // free(topic);

  // ${topic_dom}/${device_id}/cmd
  topic = mgos_bthing_sjoin("/", 3, topic_dom, device_id, "cmd");
  mgos_mqtt_sub(topic, mg_bthing_mqtt_on_cmd, NULL);
  LOG(LL_DEBUG, ("Looking for commands here: %s", topic));
  // free(topic);

  #ifdef MGOS_BTHING_HAVE_SHADOW
  if (mg_bthing_mqtt_use_shadow()) {
    /* SHADOW-MODE TOPICS */

    // ${topic_dom}/${device_id}/state/+
    topic = mgos_bthing_sjoin("/", 4, topic_dom, device_id, "state", "+");
    mgos_mqtt_sub(topic, mg_bthing_mqtt_on_shadow_state_cmd, NULL);
    LOG(LL_DEBUG, ("Looking for shadow-state commands here: %s", topic));
    // free(topic);
  }
  #endif // MGOS_BTHING_HAVE_SHADOW

  if (!mg_bthing_mqtt_use_shadow()) {
    /* STANDARD-MODE TOPICS */

    // ${topic_dom}/${device_id}/+/+/state/+
    topic = mgos_bthing_sjoin("/", 6, topic_dom, device_id, "+", "+", "state", "+");
    mgos_mqtt_sub(topic, mg_bthing_mqtt_on_state_cmd2, NULL);
    LOG(LL_DEBUG, ("Looking for state commands here: %s", topic));
    // free(topic);

    // ${topic_dom}/${device_id}/+/state/+
    topic = mgos_bthing_sjoin("/", 5, topic_dom, device_id, "+", "state", "+");
    mgos_mqtt_sub(topic, mg_bthing_mqtt_on_state_cmd1, NULL);
    LOG(LL_DEBUG, ("Looking for state commands here: %s", topic));
    // free(topic);
  }
  return true;
}

bool mgos_bthing_mqtt_init() {

  // initialize pub/sub MQTT topics
  if (!mg_bthing_mqtt_init_topics()) {
    LOG(LL_ERROR, ("Error initializing pub/sub MQTT topics. See above message/s for more details."));
    return false;
  }

  // subscribe to MQTT topics
  if (!mg_bthing_mqtt_sub_topics()) {
    LOG(LL_ERROR, ("Error subscribing to MQTT topics. See above message/s for more details."));
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

  mgos_mqtt_add_global_handler(mg_bthing_mqtt_on_event, NULL);

  return true;
}