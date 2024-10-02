#include "mqtt.h"
#include "net/mqtt_helper.h"

LOG_MODULE_REGISTER(MQTT, LOG_LEVEL_INF);

struct mqtt_client client;
struct sockaddr_storage broker;

static void on_connect_cb(enum mqtt_conn_return_code return_code, bool session_present)
{
    LOG_INF("MQTT Connect code: %d", return_code);

    int err = publish_mqtt_msg("Hello World!", "NORDIC/ETHER/TEST/CELLULAR");

    if (err != 0)
    {
        LOG_ERR("MQTT Publish failed: ");
    }
}

static void on_errors_cb(enum mqtt_helper_error error)
{
    LOG_INF("MQTT Error code: %d", error);
}

static void on_publish_cb(uint16_t message_id, int result)
{
    LOG_INF("MQTT Publish Result: %d, msg_id: %d", result, message_id);
}

int publish_mqtt_msg(const char *message, const char *pub_topic)
{
    int err;

    struct mqtt_publish_param param = {
        .message.payload.data = (uint8_t *)message,
        .message.payload.len = strlen(message),
        .message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE,
        .message_id = k_uptime_get_32(),
        .message.topic.topic.utf8 = (uint8_t *)pub_topic,
        .message.topic.topic.size = strlen(pub_topic),
    };

    err = mqtt_helper_publish(&param);
    if (err)
    {
        LOG_WRN("Failed to send payload, err: %d", err);
        return err;
    }
    LOG_INF("Published message: \"%.*s\" on topic: \"%.*s\"", param.message.payload.len,
            param.message.payload.data,
            param.message.topic.topic.size,
            param.message.topic.topic.utf8);
    return err;
}

int mqtt_init()
{
    const uint8_t *device_id = client_id_get();

    struct mqtt_helper_conn_params broker_conn_params = {
        .hostname.ptr = MQTT_BROKER_HOST,
        .hostname.size = strlen(MQTT_BROKER_HOST),
        .user_name.ptr = MQTT_BROKER_USER,
        .user_name.size = strlen(MQTT_BROKER_USER),
        .password.ptr = MQTT_BROKER_PASS,
        .password.size = strlen(MQTT_BROKER_PASS),
        .device_id.ptr = device_id,
        .device_id.size = strlen(device_id),
    };

    struct mqtt_helper_cfg broker_cb_cfg = {
        .cb = {
            .on_connack = on_connect_cb,
            .on_error = on_errors_cb,
            .on_puback = on_publish_cb,
            .on_all_events = mqtt_evt_handler,
        },
    };

    int err = mqtt_helper_init(&broker_cb_cfg);
    if (err)
    {
        LOG_ERR("Failed to initialize MQTT client: %d", err);
        return 0;
    }
    LOG_INF("Connection to broker using mqtt_init");
    err = mqtt_helper_connect(&broker_conn_params);
    while (err != 0)
    {
        LOG_ERR("Error in mqtt_init: %d", err);
        k_sleep(K_SECONDS(2));
    }
    return err;
}

bool mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *const event)
{
    switch (event->type)
    {
    case MQTT_EVT_CONNACK:
        if (event->result != 0)
        {
            LOG_ERR("MQTT connect failed: %d", event->result);
            break;
        }

        LOG_INF("MQTT client connected");
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT client disconnected: %d", event->result);
        break;

    case MQTT_EVT_PUBLISH:
        /* Listen to published messages received from the broker and extract the message */
        {
            const struct mqtt_publish_param *p = &event->param.publish;
            LOG_INF("MQTT PUBLISH result=%d len=%d",
                    event->result, p->message.payload.len);

            if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE)
            {
                const struct mqtt_puback_param ack = {
                    .message_id = p->message_id};
                mqtt_publish_qos1_ack(client, &ack);
            }
        }
        break;

    case MQTT_EVT_PUBACK:
        if (event->result != 0)
        {
            LOG_ERR("MQTT PUBACK error: %d", event->result);
            break;
        }

        LOG_INF("PUBACK packet id: %u", event->param.puback.message_id);
        break;

    case MQTT_EVT_SUBACK:
        if (event->result != 0)
        {
            LOG_ERR("MQTT SUBACK error: %d", event->result);
            break;
        }

        LOG_INF("SUBACK packet id: %u", event->param.suback.message_id);
        break;

    case MQTT_EVT_PINGRESP:
        if (event->result != 0)
        {
            LOG_ERR("MQTT PINGRESP error: %d", event->result);
        }
        break;

    default:
        LOG_INF("Unhandled MQTT event type: %d", event->type);
        break;
    }
}

const uint8_t *client_id_get(void)
{
    static uint8_t client_id[CLIENT_ID_LEN];

    char imei_buf[CGSN_RESPONSE_LENGTH + 1];
    int err;

    err = nrf_modem_at_cmd(imei_buf, sizeof(imei_buf), "AT+CGSN");
    if (err)
    {
        LOG_ERR("Failed to obtain IMEI, error: %d", err);
        goto exit;
    }

    imei_buf[IMEI_LEN] = '\0';

    snprintf(client_id, sizeof(client_id), "nrf-%.*s", IMEI_LEN, imei_buf);

exit:
    LOG_DBG("client_id = %s", (char *)(client_id));

    return client_id;
}
