#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static K_SEM_DEFINE(lte_connected, 0, 1);


void lte_handler(const struct lte_lc_evt *const evt) {
        switch(evt->type) {
                case LTE_LC_EVT_NW_REG_STATUS:
                        if((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) && (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
                                break;
                        }

                        LOG_INF("Network registration status: %s",    
                                evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
                                "Connected home network" : "Connected - Roaming");

                        k_sem_give(&lte_connected);
                        break;
                case LTE_LC_EVT_RRC_UPDATE:
                        if(evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED) {
                                LOG_INF("RRC Connection state: Connected");
                                k_sem_give(&lte_connected);
                        }
                        else if(evt->rrc_mode == LTE_LC_RRC_MODE_IDLE) {
                                LOG_INF("RRC Connection state: Idle");
                        }
                        break;
                default:
                        break;
        }

}

static int modem_configure(void) {
        int err;

        LOG_INF("Init modem library");
        err = nrf_modem_lib_init();
        if(err) {
                LOG_ERR("FAILED to init modem lib, error: %d", err);
                return err;
        }

        LOG_INF("Connecting to LTE network");
        err = lte_lc_connect_async(lte_handler);
        if(err) {
                LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
                return err;
        }

        return 0;
}

int main(void)
{
        int err;
        if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LEDs Library");
	}

        err = modem_configure();
        if(err) {
                LOG_ERR("Failed to configure modem");
                return 0;
        }

        k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");

	dk_set_led_on(DK_LED2);

        return 0;
}
