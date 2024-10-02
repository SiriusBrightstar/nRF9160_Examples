#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "nrf_modem_at.h"

#include "lte/lte.h"
#include "mqtt/mqtt.h"

LOG_MODULE_REGISTER(MAIN, LOG_LEVEL_INF);

#define CGSN_RESPONSE_LENGTH 32
#define CLIENT_ID_LEN 23
#define IMEI_LEN 16

int get_nordic_device_id(char *client_id, size_t client_id_len);

int main(void)
{
    int err = nrf_modem_lib_init();
    if (err)
    {
        LOG_ERR("Modem library initialization failed, error: %d", err);
        return err;
    }

    err = modem_configure();
    if (err)
    {
        LOG_ERR("Modem Configure: %d", err);
        return err;
    }

    err = network_info_log();
    if (err)
    {
        LOG_ERR("Failed to get network info: %d", err);
        return err;
    }

    char client_id[CLIENT_ID_LEN];
    int id_err = get_nordic_device_id(client_id, sizeof(client_id));

    int ret = mqtt_init("test.mosquitto.org", "nrf-9160", "rw", "readwrite", 1884);
    if (ret != 0)
    {
        printk("MQTT initialization failed: %d\n", ret);
        // Handle the error
    }
    else
    {
        mqtt_pub_payload("ETHER/NORDIC/TEST", client_id);
        mqtt_sub_topic("my/subscription/topic");
    }

    while (1)
    {
        mqtt_process();
        k_sleep(K_SECONDS(10));
    }
}

int get_nordic_device_id(char *client_id, size_t client_id_len)
{
    char imei_buf[CGSN_RESPONSE_LENGTH + 1];
    int err;

    if (client_id == NULL || client_id_len < CLIENT_ID_LEN)
    {
        LOG_ERR("Invalid arguments");
        return -EINVAL;
    }

    err = nrf_modem_at_cmd(imei_buf, sizeof(imei_buf), "AT+CGSN");
    if (err)
    {
        LOG_ERR("Failed to obtain IMEI, error: %d", err);
        return err;
    }

    imei_buf[IMEI_LEN] = '\0';

    for (int i = IMEI_LEN - 1; i >= 0 && (imei_buf[i] == '\r' || imei_buf[i] == '\n'); i--)
    {
        imei_buf[i] = '\0';
    }

    snprintf(client_id, client_id_len, "nrf-%.*s", IMEI_LEN, imei_buf);

    LOG_DBG("client_id = %s", client_id);

    return 0;
}
