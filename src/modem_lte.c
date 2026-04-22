#include "modem_lte.h"

LOG_MODULE_DECLARE(app, LOG_LEVEL_INF);

static K_SEM_DEFINE(lte_connected, 0, 1);

int modem_configure(void) {
    int err;

    LOG_INF("Init modem library");
    err = nrf_modem_lib_init();
    if(err) {
            LOG_ERR("FAILED to init modem lib, error: %d", err);
            return err;
    }

    err = modem_key_mgmt_write(SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_IDENTITY, CONFIG_COAP_DEVICE_NAME, 
        strlen(CONFIG_COAP_DEVICE_NAME));
    if (err) {
            LOG_ERR("Failed to write identity: %d\n", err);
            return err;
    }

    err = modem_key_mgmt_write(SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PSK, CONFIG_COAP_SERVER_PSK, 
        strlen(CONFIG_COAP_SERVER_PSK));
    if (err) {
            LOG_ERR("Failed to write identity: %d\n", err);
            return err;
    }


    err = lte_lc_psm_req(true);
    if (err) {
            LOG_ERR("lte_lc_psm_req, error: %d", err);
    } 
    err = lte_lc_edrx_req(true);
    if (err) {
            LOG_ERR("lte_lc_edrx_req, error: %d", err);
    }


    LOG_INF("Connecting to LTE network");
    err = lte_lc_connect_async(lte_handler);
    if(err) {
            LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
            return err;
    }

    k_sem_take(&lte_connected, K_FOREVER);
    LOG_INF("Connected to LTE network");
    
    app_state = APP_STATE_LTE_READY;
    return 0;
}

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
                }
                else if(evt->rrc_mode == LTE_LC_RRC_MODE_IDLE) {
                        LOG_INF("RRC Connection state: Idle");
                }
                break;

        case LTE_LC_EVT_PSM_UPDATE:
                LOG_INF("PSM parameter update: TAU: %d, Active time: %d",
                evt->psm_cfg.tau, evt->psm_cfg.active_time);
                if (evt->psm_cfg.active_time == -1){
                LOG_ERR("Network rejected PSM parameters. Failed to enable PSM");
            }
                break;
        case LTE_LC_EVT_EDRX_UPDATE:
                LOG_INF("eDRX parameter update: eDRX: %f, PTW: %f",
                        (double)evt->edrx_cfg.edrx, (double)evt->edrx_cfg.ptw);
                break;
        default:
                break;
    }
}