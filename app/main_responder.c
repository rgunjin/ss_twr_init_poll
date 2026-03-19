#include <stdint.h>
#include <string.h>
#include "nrf52_addresses.h"
#include "nrf52_peripherals.h"
#include "nrf52_pins.h"
#include "spi.h"
#include "dw1000.h"
#include "dw1000_regs.h"
#include "dw1000_config.h"
#include "SEGGER_RTT.h"

// =============================================================================
// Constants
// =============================================================================

#define ANT_DLY             16456               // Default antenna delay for DW1001

// Message field indexes
#define ALL_MSG_SN_IDX              2           // sequence number byte
#define ALL_MSG_COMMON_LEN          10          // common header lenght for frame validator
#define RESP_MSG_POLL_RX_TS_IDX     10          // T2: poll RX timestamp in response
#define RESP_MSG_RESP_TX_TS_IDX     14          // T3: response TX timestamp in response

// Expected poll frame: initator -> responder
static uint8_t rx_poll_msg[] = {
    0x41, 0x88,             // Frame control
    0,                      // sequence number (ignored during validation)
    0xCA, 0xDE,             // PAN ID
    'W', 'A', 'V', 'E',     // destination address
    0xE0,                   // function code: poll
    0, 0                    // CRC
};

// Response frame: responder -> initator
static uint8_t tx_resp_msg[] = {
    0x41, 0x88,             // Frame control
    0,                      // sequence number (filled before TX)
    0xCA, 0xDE,             // PAN ID
    'V', 'E', 'W', 'A',     // destination address
    0xE1,                   // function code: response
    0, 0, 0, 0,             // T2: poll RX timestamp (filled before TX)
    0, 0, 0, 0,             // T3: response  TX timestamp (filled before TX)
    0, 0                    // CRC (appended auto by DW1000)
};

