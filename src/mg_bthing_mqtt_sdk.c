#include "mgos_mqtt.h"
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

char *mg_bthing_mqtt_build_device_topic(const char *topic) {
  char *new_topic = NULL;
  if (mg_bthing_sreplace(topic, MGOS_BTHING_ENV_DEVICEID, mgos_sys_config_get_device_id(), &new_topic)) {
    return new_topic;
  }
  return NULL;
}

#ifndef MGOS_BTHING_HAVE_SHADOW

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

struct mg_bthing_mqtt_item *mg_bthing_mqtt_get_items() {
  return s_mqtt_items;
}

#endif //MGOS_BTHING_HAVE_SHADOW