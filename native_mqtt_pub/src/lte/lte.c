#include "lte.h"
#include "modem/modem_info.h"

LOG_MODULE_REGISTER(LTE, LOG_LEVEL_INF);
K_SEM_DEFINE(lte_connected, 0, 1);

void lte_handler(const struct lte_lc_evt *const evt)
{
    switch (evt->type)
    {
    case LTE_LC_EVT_NW_REG_STATUS:
        if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
            (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
        {
            break;
        }
        LOG_INF("Network registration status: %s",
                evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");
        k_sem_give(&lte_connected);
        break;
    default:
        break;
    }
}

int modem_configure(void)
{
    int err;

    LOG_INF("Connecting to LTE network");
    err = lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_NBIOT, LTE_LC_SYSTEM_MODE_PREFER_NBIOT_PLMN_PRIO);
    LOG_INF("lte_lc_system_mode_set: %d", err);
    if (err)
    {
        LOG_ERR("Failed to activate LTE, error: %d", err);
        return 0;
    }
    err = lte_lc_connect_async(lte_handler);
    LOG_INF("lte_lc_connect_async: %d", err);
    if (err)
    {
        LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
        return err;
    }

    k_sem_take(&lte_connected, K_FOREVER);

    LOG_INF("Connected to LTE network");

    return err;
}

int network_info_log(void)
{
    int err = modem_info_init();
    if (err)
    {
        LOG_ERR("Failed to initialize modem info: %d", err);
        return err;
    }
    LOG_INF("====== Cell Network Info ======");
    char sbuf[128];
    modem_info_string_get(MODEM_INFO_RSRP, sbuf, sizeof(sbuf));
    LOG_INF("Signal strength: %s", sbuf);

    modem_info_string_get(MODEM_INFO_CUR_BAND, sbuf, sizeof(sbuf));
    LOG_INF("Current LTE band: %s", sbuf);

    modem_info_string_get(MODEM_INFO_SUP_BAND, sbuf, sizeof(sbuf));
    LOG_INF("Supported LTE bands: %s", sbuf);

    modem_info_string_get(MODEM_INFO_AREA_CODE, sbuf, sizeof(sbuf));
    LOG_INF("Tracking area code: %s", sbuf);

    modem_info_string_get(MODEM_INFO_UE_MODE, sbuf, sizeof(sbuf));
    LOG_INF("Current mode: %s", sbuf);

    modem_info_string_get(MODEM_INFO_OPERATOR, sbuf, sizeof(sbuf));
    LOG_INF("Current operator name: %s", sbuf);

    modem_info_string_get(MODEM_INFO_CELLID, sbuf, sizeof(sbuf));
    LOG_INF("Cell ID of the device: %s", sbuf);

    modem_info_string_get(MODEM_INFO_IP_ADDRESS, sbuf, sizeof(sbuf));
    LOG_INF("IP address of the device: %s", sbuf);

    modem_info_string_get(MODEM_INFO_FW_VERSION, sbuf, sizeof(sbuf));
    LOG_INF("Modem firmware version: %s", sbuf);

    modem_info_string_get(MODEM_INFO_LTE_MODE, sbuf, sizeof(sbuf));
    LOG_INF("LTE-M support mode: %s", sbuf);

    modem_info_string_get(MODEM_INFO_NBIOT_MODE, sbuf, sizeof(sbuf));
    LOG_INF("NB-IoT support mode: %s", sbuf);

    modem_info_string_get(MODEM_INFO_GPS_MODE, sbuf, sizeof(sbuf));
    LOG_INF("GPS support mode: %s", sbuf);

    modem_info_string_get(MODEM_INFO_DATE_TIME, sbuf, sizeof(sbuf));
    LOG_INF("Mobile network time and date: %s", sbuf);

    LOG_INF("===============================");

    return 0;
}
