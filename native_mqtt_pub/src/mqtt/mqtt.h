#ifndef __MQTT_H__
#define __MQTT_H__

int mqtt_init(const char *broker_ip, const char *client_id_str,
              const char *username_str, const char *password_str, int port);
int mqtt_pub_payload(const char *topic, const char *payload);
int mqtt_sub_topic(const char *topic);
void mqtt_process(void);

#endif
