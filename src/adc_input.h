#pragma once

#include <zephyr/drivers/adc.h>
#include <zephyr/devicetree.h>


#include <stdint.h>

/* Initierar ADC-hårdvaran baserat på devicetree */
int init_adc(void);

/* Läser av spänningen och returnerar värdet i millivolt (mV) */
int32_t read_voltage_mv(void);
uint8_t calculate_battery_percentage(int32_t mvolts);