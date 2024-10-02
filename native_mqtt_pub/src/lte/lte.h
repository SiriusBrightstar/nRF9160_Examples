#ifndef __LTE_H__
#define __LTE_H__

#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <zephyr/logging/log.h>

int modem_configure(void);
void lte_handler(const struct lte_lc_evt *const evt);
int network_info_log(void);

#endif
