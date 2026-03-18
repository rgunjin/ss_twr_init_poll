#ifndef NRF52_ADDRESSES_H
#define NRF52_ADDRESSES_H

#include "nrf52_peripherals.h"

#define NRF_CLOCK_BASE   0x40000000UL
#define NRF_GPIO_BASE    0x50000000UL
#define NRF_SPIM1_BASE   0x40004000UL

#define NRF_CLOCK   ((NRF_CLOCK_Type*) NRF_CLOCK_BASE)
#define NRF_GPIO    ((NRF_GPIO_Type*)  NRF_GPIO_BASE)
#define NRF_SPIM1   ((NRF_SPIM_Type*)  NRF_SPIM1_BASE)

#endif
