#include "modem_lte.h"

LOG_MODULE_DECLARE(app, LOG_LEVEL_INF);

static K_SEM_DEFINE(lte_connected, 0, 1);

int64_t lte_network_connect_duration = 0;  

int modem_configure(void) {
    int err;
    int64_t start_time; // Variabel för att spara starttiden

    LOG_INF("Init modem library");
    err = nrf_modem_lib_init();
    if(err) {
            LOG_ERR("FAILED to init modem lib, error: %d", err);
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

    // 1. Spara tiden precis innan vi försöker ansluta och vänta på semaforen
    start_time = k_uptime_get();

    LOG_INF("Connecting to LTE network");
    err = lte_lc_connect_async(lte_handler);
    if(err) {
            LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
            return err;
    }

    k_sem_take(&lte_connected, K_FOREVER);
    LOG_INF("Connected to LTE network");
    // 2. Räkna ut hur lång tid det tog i millisekunder
        
    lte_network_connect_duration = k_uptime_delta(&start_time);

    // 3. Logga tiden (vi delar med 1000.0 för att få det snyggt i sekunder med decimaler)
    LOG_INF("Connected to LTE network! Connection took: %d.%03d sekunder", 
            (int)(lte_network_connect_duration / 1000), (int)(lte_network_connect_duration % 1000));

    app_state = APP_STATE_LTE_READY;
    return 0;
}

void tolka_mast_storning(int emm_cause) {
    switch (emm_cause) {
        case 0:
            // Ingen specifik felkod är satt, eller så pågår sökningen fortfarande
            break;
        case 14:
            LOG_WRN("MASTSTÖRNING: Operatören tillåter inte LTE på denna mast (EMM 14)!");
            break;
        case 15:
            LOG_WRN("MASTSTÖRNING: Ditt SIM-kort är blockerat i detta geografiska område (EMM 15)!");
            break;
        case 17:
            LOG_WRN("MASTSTÖRNING: Nätverksfel! Masten svarar inte på anrop (EMM 17)!");
            break;
        default:
            LOG_WRN("Nätverksfel: Modemet blev nekat av masten med EMM-kod: %d", emm_cause);
            break;
    }
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