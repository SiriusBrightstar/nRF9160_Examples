#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/random/random.h>

#define MAX_TOPICS 10
#define MQTT_RECONNECT_DELAY_MS 5000
#define MQTT_CONNECT_TIMEOUT_MS 10000
#define MQTT_POLL_TIMEOUT_MS 500

struct mqtt_client client;
struct sockaddr_storage broker;
static uint8_t rx_buffer[1024];
static uint8_t tx_buffer[1024];

static struct mqtt_topic topics[MAX_TOPICS];
static int topic_count = 0;

static char client_id[32];
static struct mqtt_utf8 username_utf8;
static struct mqtt_utf8 password_utf8;
static int mqtt_port = 1883; // Default MQTT port

static char broker_ip[64];

static bool connected = false;
static struct pollfd fds[1];

K_SEM_DEFINE(mqtt_connected, 0, 1);

static void mqtt_event_handler(struct mqtt_client *client,
                               const struct mqtt_evt *evt)
{
    switch (evt->type)
    {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0)
        {
            printk("MQTT connect failed %d\n", evt->result);
            connected = false;
            break;
        }
        connected = true;
        k_sem_give(&mqtt_connected);
        printk("MQTT connected\n");
        break;

    case MQTT_EVT_DISCONNECT:
        printk("MQTT disconnected\n");
        connected = false;
        break;

    case MQTT_EVT_PUBLISH:
    {
        const struct mqtt_publish_param *p = &evt->param.publish;
        printk("MQTT publish received %d, %d\n", p->message.topic.topic.size,
               p->message.payload.len);
    }
    break;

    case MQTT_EVT_PUBACK:
        if (evt->result != 0)
        {
            printk("MQTT PUBACK error %d\n", evt->result);
            break;
        }
        printk("MQTT PUBACK received\n");
        break;

    default:
        printk("MQTT event %d\n", evt->type);
        break;
    }
}

static int mqtt_connect(void)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    int rc;

    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(mqtt_port);
    inet_pton(AF_INET, broker_ip, &broker4->sin_addr);

    mqtt_client_init(&client);

    client.broker = &broker;
    client.evt_cb = mqtt_event_handler;
    client.client_id.utf8 = client_id;
    client.client_id.size = strlen(client_id);
    client.user_name = &username_utf8;
    client.password = &password_utf8;
    client.protocol_version = MQTT_VERSION_3_1_0;
    client.rx_buf = rx_buffer;
    client.rx_buf_size = sizeof(rx_buffer);
    client.tx_buf = tx_buffer;
    client.tx_buf_size = sizeof(tx_buffer);

    rc = mqtt_connect(&client);
    if (rc != 0)
    {
        printk("MQTT connect failed %d\n", rc);
        return rc;
    }

    fds[0].fd = client.transport.tcp.sock;
    fds[0].events = POLLIN;

    rc = poll(fds, 1, MQTT_CONNECT_TIMEOUT_MS);
    if (rc < 0)
    {
        printk("MQTT connection poll error: %d\n", errno);
        return -errno;
    }
    else if (rc == 0)
    {
        printk("MQTT connection timeout\n");
        return -ETIMEDOUT;
    }

    mqtt_input(&client);

    if (!connected)
    {
        printk("MQTT connection failed\n");
        mqtt_abort(&client);
        return -ECONNABORTED;
    }

    return 0;
}

int mqtt_init(const char *broker_ip_addr, const char *client_id_str,
              const char *username_str, const char *password_str, int port)
{
    mqtt_port = port;
    strncpy(client_id, client_id_str, sizeof(client_id) - 1);
    strncpy(broker_ip, broker_ip_addr, sizeof(broker_ip) - 1);

    username_utf8.utf8 = (uint8_t *)username_str;
    username_utf8.size = strlen(username_str);

    password_utf8.utf8 = (uint8_t *)password_str;
    password_utf8.size = strlen(password_str);

    return mqtt_connect();
}

int mqtt_pub_payload(const char *topic, const char *payload)
{
    struct mqtt_publish_param param;
    int ret;

    param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    param.message.topic.topic.utf8 = topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *)payload;
    param.message.payload.len = strlen(payload);
    param.message_id = sys_rand32_get();
    param.dup_flag = 0U;
    param.retain_flag = 0U;

    ret = mqtt_publish(&client, &param);
    if (ret != 0)
    {
        printk("MQTT publish failed, attempting to reconnect\n");
        if (mqtt_connect() == 0)
        {
            ret = mqtt_publish(&client, &param);
        }
    }
    return ret;
}

int mqtt_sub_topic(const char *topic)
{
    if (topic_count >= MAX_TOPICS)
    {
        return -ENOMEM;
    }

    topics[topic_count].topic.utf8 = topic;
    topics[topic_count].topic.size = strlen(topic);
    topics[topic_count].qos = MQTT_QOS_1_AT_LEAST_ONCE;

    struct mqtt_subscription_list list = {
        .list = &topics[topic_count],
        .list_count = 1,
        .message_id = sys_rand32_get()};

    int ret = mqtt_subscribe(&client, &list);
    if (ret != 0)
    {
        printk("MQTT subscribe failed, attempting to reconnect\n");
        if (mqtt_connect() == 0)
        {
            ret = mqtt_subscribe(&client, &list);
        }
    }
    if (ret == 0)
    {
        topic_count++;
    }
    return ret;
}

void mqtt_process(void)
{
    int rc;

    if (!connected)
    {
        rc = mqtt_connect();
        if (rc != 0)
        {
            k_sleep(K_MSEC(MQTT_RECONNECT_DELAY_MS));
            return;
        }
    }

    rc = poll(fds, 1, MQTT_POLL_TIMEOUT_MS);
    if (rc < 0)
    {
        printk("MQTT poll error: %d\n", errno);
        connected = false;
        return;
    }

    if (rc > 0 && (fds[0].revents & POLLIN))
    {
        rc = mqtt_input(&client);
        if (rc != 0)
        {
            printk("MQTT input error: %d\n", rc);
            connected = false;
            return;
        }
    }

    rc = mqtt_live(&client);
    if (rc != 0 && rc != -EAGAIN)
    {
        printk("MQTT live error: %d\n", rc);
        connected = false;
        return;
    }
}
