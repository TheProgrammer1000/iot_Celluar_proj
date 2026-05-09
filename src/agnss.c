#include "agnss.h"
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <net/nrf_cloud_coap.h>
#include <net/nrf_cloud_agnss.h>
#include <net/nrf_cloud_rest.h>

static struct k_work agnss_request_work;
static struct nrf_modem_gnss_agnss_data_frame stored_agnss_req;

LOG_MODULE_DECLARE(app, LOG_LEVEL_INF);

static char agnss_buf[NRF_CLOUD_AGNSS_MAX_DATA_SIZE];
static void agnss_work(struct k_work *work) {
    int err;
    ARG_UNUSED(work);

    LOG_INF("A-GNSS work running");
    LOG_INF("A-GNSS data_flags: 0x%02x", stored_agnss_req.data_flags);
    LOG_INF("A-GNSS system_count: %d", stored_agnss_req.system_count);

    /*
        * Nästa steg senare:
        * 1. Skicka stored_agnss_req till nRF Cloud CoAP
        * 2. Få tillbaka A-GNSS data
        * 3. Injecta med nrf_cloud_agnss_process()
        */
    struct nrf_cloud_rest_agnss_request request = {
            .type = NRF_CLOUD_REST_AGNSS_REQ_CUSTOM,
            .agnss_req = &stored_agnss_req,
            .net_info = NULL,
            .filtered = false,
            .mask_angle = NRF_CLOUD_AGNSS_MASK_ANGLE_NONE,
    };

    struct nrf_cloud_rest_agnss_result result = {
            .buf = agnss_buf,
            .buf_sz = sizeof(agnss_buf),
            .agnss_sz = 0,
    };

    LOG_INF("Requesting A-GNSS data from nRF Cloud");

    err = nrf_cloud_coap_agnss_data_get(&request, &result);
    if (err) {
            LOG_ERR("nrf_cloud_coap_agnss_data_get failed: %d", err);
            return;
    }

    LOG_INF("A-GNSS data received: %d bytes", result.agnss_sz);

    err = nrf_cloud_agnss_process(result.buf, result.agnss_sz);
    if (err) {
            LOG_ERR("nrf_cloud_agnss_process failed: %d", err);
            return;
    }

    LOG_INF("A-GNSS data injected into modem");
}

int agnss_init(void)
{
        int err;

        k_work_init(&agnss_request_work, agnss_work);

        LOG_INF("Initializing nRF Cloud CoAP");

        err = nrf_cloud_coap_init();
        if (err) {
                LOG_ERR("nrf_cloud_coap_init failed: %d", err);
                return err;
        }

        LOG_INF("Connecting to nRF Cloud CoAP");

        err = nrf_cloud_coap_connect(NULL);
        if (err) {
                LOG_ERR("nrf_cloud_coap_connect failed: %d", err);
                return err;
        }

        LOG_INF("nRF Cloud CoAP connected");
        LOG_INF("A-GNSS work initialized");

        return 0;
}
 
void agnss_request_schedule(
        const struct nrf_modem_gnss_agnss_data_frame *req
)
{
        if (req == NULL) {
                LOG_ERR("A-GNSS request was NULL");
                return;
        }

        memcpy(&stored_agnss_req, req, sizeof(stored_agnss_req));

        k_work_submit(&agnss_request_work);

        LOG_INF("A-GNSS work scheduled");
}