#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "lte/lte.h"
#include "mqtt/mqtt.h"

LOG_MODULE_REGISTER(MAIN, LOG_LEVEL_INF);

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

    err = mqtt_init();
    if (err)
    {
        LOG_ERR("MQTT Connection Fail: %d", err);
        return err;
    }

    return 0;
}
