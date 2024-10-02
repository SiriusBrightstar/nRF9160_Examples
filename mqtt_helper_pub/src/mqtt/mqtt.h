#ifndef __MQTT_H__
#define __MQTT_H__

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include "zephyr/logging/log.h"
#include <nrf_modem_at.h>
#include <zephyr/random/random.h>

#define IMEI_LEN 15
#define CGSN_RESPONSE_LENGTH (IMEI_LEN + 6 + 1)
#define CLIENT_ID_LEN sizeof("nrf-") + IMEI_LEN

#define MQTT_BROKER_HOST "test.mosquitto.org"
#define MQTT_BROKER_PORT 1884
#define MQTT_BROKER_USER "rw"
#define MQTT_BROKER_PASS "readwrite"
#define MQTT_SUBSCRIBE_TOPIC "DEV/NORDIC/NRF9160"

int mqtt_init();
int publish_mqtt_msg(const char *message, const char *pub_topic);
bool mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *const event);
const uint8_t *client_id_get(void);

#endif
