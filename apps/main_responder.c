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
    delay(10000);                           // ~1ms low
    NRF_GPIO->DIRCLR = (1UL << 24);         // release RST (hi-z, open drain)
    delay(5000000);                         // ~80ms - let XTAL stabilize

    spi_init(SPIM_FREQ_2M);

    if (dw1000_init() != DW_SUCCESS) {
        SEGGER_RTT_printf(0, "[INIT] FAILED\n");
        led_on(14);
        while (1) {}
    }

    spi_init(SPIM_FREQ_8M);

    dw1000_config_t cfg = DW1000_DEFAULT_CONFIG;
    dw1000_configure(&cfg);

    uint8_t pllbuf[4];
    dw1000_read_subreg(DW_REG_FS_CTRL, DW_SUBREG_FS_PLLCFG, pllbuf, 4);
    uint32_t pllcfg = (uint32_t)pllbuf[0] | ((uint32_t)pllbuf[1]<<8) |
                  ((uint32_t)pllbuf[2]<<16) | ((uint32_t)pllbuf[3]<<24);
    uint8_t plltune = 0;
    dw1000_read_subreg(DW_REG_FS_CTRL, DW_SUBREG_FS_PLLTUNE, &plltune, 1);
    SEGGER_RTT_printf(0, "[CFG] FS_PLLCFG=0x%08X FS_PLLTUNE=0x%02X\n", pllcfg, plltune);

    SEGGER_RTT_printf(0, "[INIT] SYS_CFG=0x%08X\n", dw1000_read32(DW_REG_SYS_CFG));
    dw1000_set_antenna_delay(ANT_DLY, ANT_DLY);

    SEGGER_RTT_printf(0, "[INIT] OK - waiting for poll\n");
    led_off(31);

    dw1000_rx_enable();
    SEGGER_RTT_printf(0, "[RX] waiting...\n");

    // =========================================================================
    // Responder loop
    // =========================================================================
    while (1) {
        uint32_t status;
        

        // Ждем событие
        while (!(status = dw1000_read_sys_status() &
                (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR))) {}

        SEGGER_RTT_printf(0, "[RX] status=0x%08X\n", status);

        if (status & SYS_STATUS_RXFCG) {
            dw1000_clear_sys_status(SYS_STATUS_RXFCG);

            uint32_t frame_len = dw1000_read_rx_finfo() & 0x7F;
            if (frame_len <= RX_BUF_LEN) {
                dw1000_read_rx_data(rx_buffer, frame_len);
            }

            rx_buffer[ALL_MSG_SN_IDX] = 0;
            if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) == 0) {
                SEGGER_RTT_printf(0, "[RESP] got poll\n");

                // T2 - timestamp прием poll (40 бит, берем младшие 32)
                uint64_t poll_rx_ts = dw1000_read_rx_timestamp_u64();

                // Вычисляем время отправки ответа
                #define POLL_RX_TO_RESP_TX_DLY_UUS  1100
                #define UUS_TO_DWT_TIME             65536

                uint32_t resp_tx_time = (uint32_t)((poll_rx_ts +
                                        (uint64_t)(POLL_RX_TO_RESP_TX_DLY_UUS *
                                         UUS_TO_DWT_TIME)) >> 8);
                dw1000_set_delayed_tx_time(resp_tx_time);



                // T3 - заранее вычисленный TX timestamp
                uint32_t resp_tx_ts = ((uint32_t)(resp_tx_time & 0xFFFFFFFE) << 8) + ANT_DLY;

                // Вписываем timestamp в тело ответа
                msg_set_ts(&tx_resp_msg[RESP_MSG_POLL_RX_TS_IDX], poll_rx_ts);
                msg_set_ts(&tx_resp_msg[RESP_MSG_RESP_TX_TS_IDX], resp_tx_ts);

                // Готовим фрейм
                tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
                dw1000_write_tx_data(tx_resp_msg, sizeof(tx_resp_msg), 0);
                dw1000_write_tx_fctrl(sizeof(tx_resp_msg), 0, 1);

                // Запускаем отложенную передачу
                int ret = dw1000_start_tx_delayed();    // DWT_START_TX_DELAYED
                if (ret == DW_SUCCESS) {
                    while (!(dw1000_read_sys_status() & SYS_STATUS_TXFRS)) {}
                    dw1000_clear_sys_status(SYS_STATUS_TXFRS);
                    frame_seq_nb++;
                    SEGGER_RTT_printf(0, "[RESP] response sent\n");
                } else {
                    SEGGER_RTT_printf(0, "[ERR] delayed TX failed!\n");
                    dw1000_rx_reset();
                }
            }
        } else {
            dw1000_clear_sys_status(SYS_STATUS_ALL_RX_ERR);
            dw1000_rx_reset();
        }

        // Перезапускаем приемник для следующего цикла
        dw1000_rx_enable();
    }
}

// NOTE: In this simple implementation T3 is sent as 0 in the response.
// The initiator will compute an incorrect distance. To fix this properly,
// the responder needs to know T3 before sending — this requires either:
// a) delayed TX: schedule the response at a known future time, so T3 = TX_time + known_delay
