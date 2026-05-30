#include "adc_input.h"

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
    
    int32_t real_battery_mv = (val_mv * 32) / 10;

    return (err < 0) ? err : real_battery_mv;
}


uint8_t calculate_battery_percentage(int32_t mvolts) {
    // Sätt max- och mingränser för 4x AA-batterier
    const int32_t max_mv = 6400; // 100% (Helt nya batterier)
    const int32_t min_mv = 4000; // 0%   (Helt tomma batterier)

    // Om spänningen är högre än max (t.ex. vid helt sprillans nya litiumbatterier)
    if (mvolts >= max_mv) {
        return 100;
    }
    
    // Om spänningen är lägre än absolut minimum
    if (mvolts <= min_mv) {
        return 0;
    }

    // Räkna ut procenten linjärt däremellan
    int32_t percentage = ((mvolts - min_mv) * 100) / (max_mv - min_mv);
    
    return (uint8_t)percentage;
}