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

#define RX_BUF_LEN 20
static uint8_t rx_buffer[RX_BUF_LEN];

static uint8_t frame_seq_nb = 0;

// =============================================================================
// Helpers
// =============================================================================

static void delay(volatile uint32_t count) {
    while (count--) {}
}

static void led_init(void) {
    NRF_GPIO->DIRSET = (1 << 30) | (1 << 31) | (1 << 22) | (1 << 14);
    NRF_GPIO->OUTSET = (1 << 30) | (1 << 31) | (1 << 22) | (1 << 14);
}

static void led_on(uint8_t pin)  { NRF_GPIO->OUTCLR = (1 << pin); }
static void led_off(uint8_t pin) { NRF_GPIO->OUTSET = (1 << pin); }

// Write 4-byte little-endian timestamp into message buffer
static void msg_set_ts(uint8_t *buf, uint32_t ts) {
    buf[0] = (uint8_t)(ts);
    buf[1] = (uint8_t)(ts >> 8);
    buf[2] = (uint8_t)(ts >> 16);
    buf[3] = (uint8_t)(ts >> 24);
}

// =============================================================================
// main
// =============================================================================

int main(void) {
    led_init();
    led_on(31);

    // Hardware reset DW1000 via RST pin (P0.24)
    NRF_GPIO->DIRSET = (1UL << 24);
    NRF_GPIO->OUTCLR = (1UL << 24);
    delay(10000);
    NRF_GPIO->DIRCLR = (1UL << 24);         // release RST (hi-z, open drain)
    delay(500000);

    spi_init(SPIM_FREQ_2M);

    if (dw1000_init() != DW_SUCCESS) {
        SEGGER_RTT_printf(0, "[INIT] FAILED\n");
        led_on(14);
        while (1) {}
    }

    dw1000_config_t cfg = DW1000_DEFAULT_CONFIG;
    dw1000_configure(&cfg);
    dw1000_set_antenna_delay(ANT_DLY, ANT_DLY);

    SEGGER_RTT_printf(0, "[INIT] OK - waiting for poll\n");
    led_off(31);

    // =========================================================================
    // Responder loop
    // =========================================================================
    while (1) {
        // 1. Enable RX and wait for poll
        dw1000_rx_enable();
        SEGGER_RTT_printf(0, "[RX] listening...\n");

        uint32_t status;
        while (!((status = dw1000_read_sys_status()) &
                    (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR))) {}

        
        SEGGER_RTT_printf(0, "[RX] status=0x%08X\n", status);

        if (status & SYS_STATUS_RXFCG) {
            // 2. Read recieved frame
            dw1000_clear_sys_status(SYS_STATUS_RXFCG);
            uint32_t frame_len = dw1000_read_rx_finfo() & 0x7F;
            if (frame_len <= RX_BUF_LEN) {
                dw1000_read_rx_data(rx_buffer, frame_len);
            }

            // 3. Validate frame - clear sequence number before compare
            rx_buffer[ALL_MSG_SN_IDX] = 0;
            if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) == 0) {
                // 4. Record T2: poll RX timestamp
                uint32_t poll_rx_ts = dw1000_read_rx_timestamp();   // T2

                // 5. Fill responce: embed T2 and placeholder for T3
                tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
                msg_set_ts(&tx_resp_msg[RESP_MSG_POLL_RX_TS_IDX], poll_rx_ts);
                msg_set_ts(&tx_resp_msg[RESP_MSG_RESP_TX_TS_IDX], 0);   // T3 unknown

                // 6. Send responde
                dw1000_clear_sys_status(SYS_STATUS_TXFRS);
                dw1000_write_tx_data(tx_resp_msg, sizeof(tx_resp_msg), 0);
                dw1000_write_tx_fctrl(sizeof(tx_resp_msg), 0, 1);
                dw1000_start_tx(0);     // NO wait4resp - TX only

                // 7. Wait for TX done
                while (!(dw1000_read_sys_status() & SYS_STATUS_TXFRS)) {}

                // 8. Record T3: response TX timestamp
                // NOTE: T3 is sent as 0 in this simple version.
                // For accurate ranging, T3 must be sent in follow-up frame
                // or computed from a know TX delay. See NOTE below
                uint32_t resp_tx_ts = dw1000_read_tx_timestamp();   // T3
                (void)resp_tx_ts;

                frame_seq_nb++;
                SEGGER_RTT_printf(0, "[RESP] sent response #%d\n", frame_seq_nb);
                led_on(31);
                delay(10000);
                led_off(31);
            }
        } else {
            // RX error - clear flags and reset receiver
            dw1000_clear_sys_status(SYS_STATUS_ALL_RX_ERR);
            dw1000_rx_reset();
        }
    }
}

// NOTE: In this simple implementation T3 is sent as 0 in the response.
// The initiator will compute an incorrect distance. To fix this properly,
// the responder needs to know T3 before sending — this requires either:
// a) delayed TX: schedule the response at a known future time, so T3 = TX_time + known_delay
