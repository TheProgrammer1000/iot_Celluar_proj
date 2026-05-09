#pragma once

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <zephyr/logging/log.h>
#include "agnss.h"
#include "app_state.h"

#define SEC_TAG 12

int modem_configure(void);
void lte_handler(const struct lte_lc_evt *const evt);
