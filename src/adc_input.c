#include "adc_input.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, LOG_LEVEL_INF);

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

int init_adc(void) {
    if (!adc_is_ready_dt(&adc_channels[0])) {
        return -1;
    }
    return adc_channel_setup_dt(&adc_channels[0]);
}

int32_t read_voltage_mv(void) {
    uint16_t buf;
    int32_t val_mv;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    adc_sequence_init_dt(&adc_channels[0], &sequence);
    
    int err = adc_read_dt(&adc_channels[0], &sequence);
    if (err < 0) return err;

    val_mv = (int32_t)buf;
    err = adc_raw_to_millivolts_dt(&adc_channels[0], &val_mv);
    
    int32_t real_battery_mv = val_mv * 2;


    // ... efter real_battery_mv beräkningen ...
    LOG_INF("ADC rå-mv på pinne: %d mV | Beräknad batterispänning: %d mV", val_mv, real_battery_mv);
    return (err < 0) ? err : real_battery_mv;
}


uint8_t calculate_battery_percentage(int32_t mvolts) {
    // Definiera brytpunkterna för spänning (mV) och motsvarande procent (%)
    // Dessa matchar ett typiskt 4x AA-batteripack under belastning
    static const int32_t volt_pts[] = {6000, 5200, 4800, 4400, 4000, 3600};
    static const int32_t pct_pts[]  = {100,  80,   60,   40,   15,   0};
    static const int num_pts = sizeof(volt_pts) / sizeof(volt_pts[0]);

    // Om spänningen är högre än vår max-punkt
    if (mvolts >= volt_pts[0]) {
        return (uint8_t)pct_pts[0];
    }
    
    // Om spänningen är lägre än vår min-punkt (helt urladdat)
    if (mvolts <= volt_pts[num_pts - 1]) {
        return (uint8_t)pct_pts[num_pts - 1];
    }

    // Hitta vilket intervall spänningen ligger mellan
    for (int i = 0; i < num_pts - 1; i++) {
        if (mvolts >= volt_pts[i + 1]) {
            // Vi har hittat rätt intervall! 
            // volt_pts[i] är den högre spänningen, volt_pts[i+1] är den lägre.
            int32_t v_high = volt_pts[i];
            int32_t v_low  = volt_pts[i + 1];
            int32_t p_high = pct_pts[i];
            int32_t p_low  = pct_pts[i + 1];

            // Räkna ut exakt var inom detta specifika intervall vi befinner oss (linjärt)
            int32_t percentage = p_low + ((mvolts - v_low) * (p_high - p_low)) / (v_high - v_low);
            
            return (uint8_t)percentage;
        }
    }

    return 0; // Fallback
}