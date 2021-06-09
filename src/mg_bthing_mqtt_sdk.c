#include "mgos_mqtt.h"
#include "mg_bthing_sdk.h"
#include "mg_bthing_mqtt_sdk.h"

static struct mg_bthing_mqtt_item *s_mqtt_items = NULL;

int mg_bthing_mqtt_pub(const char *topic, const char *msg, bool retain) {
  return mgos_mqtt_pub(topic, msg, (msg == NULL ? 0 : strlen(msg)),
      mgos_sys_config_get_bthing_mqtt_qos(), retain);
}

int mg_bthing_mqtt_pubf(const char *topic, bool retain, const char *json_fmt, ...) {
  va_list ap;
  va_start(ap, json_fmt);
  int r = mg_bthing_mqtt_pubv(topic, retain, json_fmt, ap);
  va_end(ap);
  return r;
}

int mg_bthing_mqtt_pubv(const char *topic, bool retain, const char *json_fmt, va_list ap) {
  return mgos_mqtt_pubv(topic, mgos_sys_config_get_bthing_mqtt_qos(), retain, json_fmt, ap);
}

char *mg_bthing_mqtt_build_topic(const char *topic, mgos_bthing_t thing) {
  if (topic) {
    char *new_topic = NULL;
    if (mgos_bthing_sreplaces(topic, &new_topic, (thing ? 2 : 1),
          MGOS_BTHING_ENV_DEVICEID, mgos_sys_config_get_device_id(),
          MGOS_BTHING_ENV_THINGID, mgos_bthing_get_id(thing))) {
      return new_topic;
    }
    free(new_topic);
  }
  return NULL;
}

char *mg_bthing_mqtt_build_x_topic(const char *topic, const char *topic_suf, mgos_bthing_t thing) {
  if (!topic || !topic_suf) return NULL;
  char *tmp_topic = mg_bthing_mqtt_build_topic(topic, thing);
  const char *base_topic = (tmp_topic != NULL ? tmp_topic : topic);
  char *ret_topic = NULL;

  int ret = asprintf(&ret_topic, "%s/%s", base_topic, topic_suf);
  if (ret == -1) {
    free(ret_topic);
    ret_topic = NULL;
  }

  free(tmp_topic);
  return ret_topic;
}

char *mg_bthing_mqtt_build_sub_topic(mgos_bthing_t thing) {
  return mg_bthing_mqtt_build_x_topic(mgos_sys_config_get_bthing_mqtt_base_topic(),
    mgos_sys_config_get_bthing_mqtt_sub_topic(), thing);
}

char *mg_bthing_mqtt_build_pub_topic(mgos_bthing_t thing) {
  return mg_bthing_mqtt_build_x_topic(mgos_sys_config_get_bthing_mqtt_base_topic(),
    mgos_sys_config_get_bthing_mqtt_pub_topic(), thing);
}

void mg_bthing_mqtt_add_item(struct mg_bthing_mqtt_item *item) {
  if (item) {
    if (!s_mqtt_items) {
        s_mqtt_items = item;
    } else {
        item->next = s_mqtt_items->next;
        s_mqtt_items->next = item;
    }
  }
}
struct mg_bthing_mqtt_item *mg_bthing_mqtt_get_item(mgos_bthing_t thing) {
  struct mg_bthing_mqtt_item *ret = s_mqtt_items;
  while(ret) {
    if (ret->thing == thing) return ret;
    ret = ret->next;
  };
  return NULL;
}