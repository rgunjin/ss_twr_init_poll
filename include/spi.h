#ifndef SPI_H
#define SPI_H

#include "nrf52_peripherals.h"
#include <stdint.h>

// Initialize SPIM1 peripheral and CS pin
void spi_init(SPIM_Frequency_t freq);

// Transfer len bytes: send from tx, recieve into rx
// Both tx and rx must point to RAM buffers (EasyDMA requirement)
// Caller is responsible for CS
void spi_transfer(uint8_t *tx, uint8_t *rx, uint8_t len);


// Split-buffer transfer — mirrors Decawave's readfromspi/writetospi approach.
//
// Internally combines header and data into one flat TX buffer and performs
// a single SPI transaction. This is the only reliable way to do it on nRF52
// EasyDMA — two back-to-back TASKS_START causes a one-byte shift in RX data.
//
// For READ:  sends [header | zeros],  returns data from rx[header_len..]
// For WRITE: sends [header | data],   rx bytes are discarded
//
// Limitation: header_len + data_len must be <= 255.
// All buffers must be in RAM (EasyDMA requirement).
// Caller is responsible for CS (spi_cs_low / spi_cs_high)
void spi_transfer_split(
    uint8_t *header_buf, uint8_t header_len,
    uint8_t *data_buf,   uint8_t data_len,
    uint8_t is_read
);

// Chip Select control - called by dw1000.c around transactions
void spi_cs_low();
void spi_cs_high();

#endif // !SPI_H
