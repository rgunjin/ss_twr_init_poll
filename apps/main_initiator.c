#include <string.h>
#include <stdint.h>
#include "nrf52_addresses.h"
#include "nrf52_peripherals.h"
#include "nrf52_pins.h"
#include "spi.h"
#include "dw1000.h"
#include "dw1000_regs.h"
#include "dw1000_config.h"
#include "SEGGER_RTT.h"

// =============================================================================
// CONSTANTS
// =============================================================================
#define SPEED_OF_LIGHT      299702547.0         // м/с
#define DWT_TIME_UNITS      (1.0/499.2e6/128.0) // ~15.65 пс
#define ANT_DLY             16456               // дефолт для DWM1001

// Index in Texts Buffer
#define ALL_MSG_SN_IDX              2           // sequence number
#define ALL_MSG_COMMON_LEN          10          // name lange
#define RESP_MSG_POLL_RX_TS_IDX     10          // T2 in response
#define RESP_MSG_RESP_TX_TS_IDX     14          // T3 in response
#define RESP_MSG_TS_LEN             4           // bytes per embedded timestamp

// Poll frame: initator -> responder
static uint8_t tx_poll_msg[] = {
    0x41, 0x88,             // frame control
    0,                      // sequence number (filled before TX)
    0xCA, 0xDE,             // PAN ID
    'W', 'A', 'V', 'E',     // destination address
    0xE0,                   // function code: poll
    0, 0                    // CRC (appended automatically by DW1000)
};

// Expected response frame: responder -> initator
static uint8_t rx_resp_msg[] = {
    0x41, 0x88,             // frame control
    0,                      // sequence number (ignored by validation)
    0xCA, 0xDE,             // PAN ID
    'V', 'E', 'W', 'A',     // Destination address
    0xE1,                   // function code: response
    0, 0, 0, 0,             // T2: poll RX timestamp
    0, 0, 0, 0,             // T3: response TX timestamp
    0, 0                    // CRC
};

#define RX_BUF_LEN          20
static uint8_t rx_buffer[RX_BUF_LEN];

static uint8_t frame_seq_nb = 0;

// =============================================================================
// Helpers
// =============================================================================

static void delay(volatile uint32_t count) {
    while (count--) {}
}

static void led_init(void) {
    NRF_GPIO->DIRSET = (1 << 30) | (1 << 22) | (1 << 14) | (1 << 31);
    NRF_GPIO->OUTSET = (1 << 30) | (1 << 22) | (1 << 14) | (1 << 31);
}

static void led_on(uint8_t pin)  { NRF_GPIO->OUTCLR = (1 << pin); }
static void led_off(uint8_t pin) { NRF_GPIO->OUTSET = (1 << pin); }

// Extract 4-byte little-endian timestamp from message buffer
static uint32_t msg_get_ts(uint8_t *buf) {
    return (uint32_t)buf[0]          |
           ((uint32_t)buf[1] << 8)   |
           ((uint32_t)buf[2] << 16)  |
           ((uint32_t)buf[3] << 24);
}

// =============================================================================
// main
// =============================================================================

int main(void) {
    led_init();
    led_on(30);

    // Hardware reset DW1000 via RST pin (P0.24)
    NRF_GPIO->DIRSET = (1UL << 24);
    NRF_GPIO->OUTCLR = (1UL << 24);
    delay(10000);                       // ~1ms low
    NRF_GPIO->DIRCLR = (1UL << 24);     // release RST (hi-z, open drain)
    delay(5000000);                     // ~80ms - let XTAL stabilize

    spi_init(SPIM_FREQ_2M);

    if (dw1000_init() != DW_SUCCESS) {
        SEGGER_RTT_printf(0, "[INIT] FAILED\n");
        led_on(14);
        while (1) {}
    }

    spi_init(SPIM_FREQ_8M);

    dw1000_config_t cfg = DW1000_DEFAULT_CONFIG;
    dw1000_configure(&cfg);
    dw1000_set_antenna_delay(ANT_DLY, ANT_DLY);

    SEGGER_RTT_printf(0, "[INIT] OK - starting ranging\n");
    led_off(30);

    // =========================================================================
    // Ranging loop
    // =========================================================================
    while (1) {
        // 1. Prepair and send Poll
        tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
        dw1000_clear_sys_status(SYS_STATUS_TXFRS);
        dw1000_write_tx_data(tx_poll_msg, sizeof(tx_poll_msg), 0);
        dw1000_write_tx_fctrl(sizeof(tx_poll_msg), 0, 1);
        dw1000_start_tx(1);     // wait4resp = 1; auto-enable RX after TX
        SEGGER_RTT_printf(0, "[TX] poll sent #%d\n", frame_seq_nb);

        // 2. Poll SYS_STATUS until RX done or error
        uint32_t status;
        while (!((status = dw1000_read_sys_status()) &
                        (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR))) {}

        SEGGER_RTT_printf(0, "[RX] status=0x%08X\n", status);
        frame_seq_nb++;

        if (status & SYS_STATUS_RXFCG) {
            // 3. Read received frame
            dw1000_clear_sys_status(SYS_STATUS_RXFCG);
            uint32_t frame_len = dw1000_read_rx_finfo() & 0x7F;
            if (frame_len <= RX_BUF_LEN) {
                dw1000_read_rx_data(rx_buffer, frame_len);
            }

            // 4. Validate frame - clear sequence  number before compare
            rx_buffer[ALL_MSG_SN_IDX] = 0;
            if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) == 0) {
                    // 5. Read loacal timestamps
                    uint32_t poll_tx_ts = dw1000_read_tx_timestamp(); // T1
                    uint32_t resp_rx_ts = dw1000_read_rx_timestamp(); // T4

                    // 6. Extract remote timestamps for response payload
                    uint32_t poll_rx_ts = msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX]); // T2
                    uint32_t resp_tx_ts = msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX]); // T3

                    // 7. Compute time of flight and distance
                    uint32_t rtd_init = (uint32_t)(resp_rx_ts - poll_tx_ts);
                    uint32_t rtd_resp = (uint32_t)(resp_tx_ts - poll_rx_ts);
                    double tof = ((rtd_init - rtd_resp) / 2.0) * DWT_TIME_UNITS;
                    double distance = tof * SPEED_OF_LIGHT;

                    // SEGGER_RTT_printf does not support %f - print as cm integer
                    SEGGER_RTT_printf(0, "dist: %d cm\n", (int)(distance * 100));
                    led_on(30);
                    delay(10000);
                    led_off(30);
            }
        } else {
            // RX error - clear flags and reset receiver
            dw1000_clear_sys_status(SYS_STATUS_ALL_RX_ERR);
            dw1000_rx_reset();
        }

        delay(1000000);     // pause between ranging  exchanges
    }
}
