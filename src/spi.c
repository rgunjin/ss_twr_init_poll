#include "spi.h"
#include "nrf52_addresses.h"
#include "nrf52_peripherals.h"
#include "nrf52_pins.h"
#include <stdint.h>

// =============================================================================
// spi_init
// Configure SPIM1 peripheral for communication with DW1000
// =============================================================================
void spi_init(SPIM_Frequency_t freq) {
    // - CS pin: output, hold HIGH (DW1000 inactive)
    NRF_GPIO->DIRSET = (1UL << DW_CS);
    NRF_GPIO->OUTSET = (1UL << DW_CS);

    // - Disable SPIM before configuration
    // nRF52832 datasheet: PSEL must not be changed while SPIM is enabled
    NRF_SPIM1->ENABLE = 0;

    // - Assign physical pins
    // PSEL register: bits[4:0] = pin number, bit[31] = 1 → disconnected
    // Writing pin number only: bit31 = 0 → connected
    NRF_SPIM1->PSEL.SCK  = DW_SCK;
    NRF_SPIM1->PSEL.MOSI = DW_MOSI;
    NRF_SPIM1->PSEL.MISO = DW_MISO;

    // - Frequency
    NRF_SPIM1->FREQUENCY = (uint32_t)freq;

    // - SPI mode
    // CONFIG register:
    //   bit 0: CPHA  0=sample on leading edge (Mode 0)
    //   bit 1: CPOL  0=SCK idle LOW           (Mode 0)
    //   bit 2: ORDER 0=MSB first
    // DW1000 requires Mode 0, MSB first → CONFIG = 0
    NRF_SPIM1->CONFIG = 0;

    // - Enable SPIM
    NRF_SPIM1->ENABLE = 7;
}

// spi_transfer_split
// Single-transaction SPI transfer with separate header and data buffers.
//
// Combines header and data into one flat stack buffer, then calls
// spi_transfer once. This mirrors the Decawave approach (writetospi /
// readfromspi) and avoids the one-byte RX shift caused by two back-to-back
// TASKS_START — DW1000 starts clocking out data during the header phase,
// so splitting into two transfers loses the first byte.
//
// READ:  tx = [header | 0x00 x data_len], data returned from rx[header_len..]
// WRITE: tx = [header | data],            rx ignored
//
// Buffers are on the stack — guaranteed RAM, safe for EasyDMA.
// Caller is responsible for CS (spi_cs_low / spi_cs_high).
// Max total: header_len + data_len <= 258 bytes
void spi_transfer_split(uint8_t *header_buf, uint8_t header_len,
                        uint8_t *data_buf, uint8_t data_len,
                        uint8_t is_read) {
    uint8_t total = header_len + data_len;

    uint8_t tx[total];
    uint8_t rx[total];

    for (uint8_t i = 0; i < header_len; i++) tx[i] = header_buf[i];
    if (is_read) {
        for (uint8_t i = 0; i < data_len; i++) tx[header_len + i] = 0x00;
    } else {
        for (uint8_t i = 0; i < data_len; i++) tx[header_len + i] = data_buf[i];
    }

    spi_transfer(tx, rx, total);

    if (is_read) {
        for (uint8_t i = 0; i < data_len; i++) data_buf[i] = rx[header_len + i];
    }
}

// =============================================================================
// spi_cs_low / spi_cs_high
// CS is controlled manually by dw1000.c — not inside spi_transfer.
// This allows multi-transfer transactions without releasing CS in between.
// =============================================================================
void spi_cs_low(void) {
    NRF_GPIO->OUTCLR = (1UL << DW_CS);
}

void spi_cs_high(void) {
    NRF_GPIO->OUTSET = (1UL << DW_CS);
}

// =============================================================================
// spi_transfer
// Full-duplex SPI transaction: send len bytes from tx, receive len bytes to rx.
//
// IMPORTANT: both tx and rx must be in RAM — nRF52 EasyDMA cannot access Flash.
// Caller is responsible for CS (cs_low before, cs_high after).
// =============================================================================
void spi_transfer(uint8_t *tx, uint8_t *rx, uint8_t len) {
    // Configure EasyDMA buffers
    NRF_SPIM1->TXD.PTR      = (uint32_t)tx;
    NRF_SPIM1->TXD.MAXCNT   = len;
    NRF_SPIM1->RXD.PTR      = (uint32_t)rx;
    NRF_SPIM1->RXD.MAXCNT   = len;

    // Clear END event before starting
    NRF_SPIM1->EVENTS_END = 0;

    // Start transaction
    NRF_SPIM1->TASKS_START = 1;

    // Wait until both TX and RX are complete
    // EVENTS_END fires when both buffers are fully processed
    // (EVENTS_ENDTX fires earlier — RX may still be in progress)
    while (!NRF_SPIM1->EVENTS_END) {}

    // Clear event for next transaction
    NRF_SPIM1->EVENTS_END = 0;
}
