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

    // Hardware reset
    NRF_GPIO->DIRSET = (1UL << 24);
    NRF_GPIO->OUTCLR = (1UL << 24);
    delay(10000);
    NRF_GPIO->DIRCLR = (1UL << 24);
    delay(5000000);

    spi_init(SPIM_FREQ_2M);

    // AON cleanup
    dw1000_write32(DW_REG_SYS_CTRL, SYS_CTRL_TRXOFF);
    delay(5000);
    uint8_t zero = 0x00;
    uint16_t zero16 = 0x0000;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CTRL, &zero, 1);
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_WCFG, (uint8_t *)&zero16, 2);
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CFG0, &zero, 1);
    uint8_t save = 0x02;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CTRL, &save, 1);
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CTRL, &zero, 1);
    dw1000_clear_sys_status(0xFFFFFFFF);
    delay(10000);

    if (dw1000_init() != DW_SUCCESS) {
        SEGGER_RTT_printf(0, "[INIT] FAILED\n");
        led_on(14);
        while (1) {}
    }

    spi_init(SPIM_FREQ_8M);

    dw1000_config_t cfg = DW1000_DEFAULT_CONFIG;
    dw1000_configure(&cfg);
    dw1000_set_antenna_delay(ANT_DLY, ANT_DLY);

    SEGGER_RTT_printf(0, "[INIT] OK - waiting for poll\n");
    led_off(31);

    // =========================================================
    // МИНИМАЛЬНЫЙ RX ТЕСТ
    // =========================================================
    dw1000_clear_sys_status(0xFFFFFFFF);        // Чистим статус
    dw1000_write32(DW_REG_SYS_CTRL, SYS_CTRL_RXENAB);
    SEGGER_RTT_printf(0, "[RX] receiver enabled\n");
    SEGGER_RTT_printf(0, "[DBG] SYS_STATE=0x%08X\n",
                      dw1000_read32(DW_REG_SYS_STATE));

    while (1) {
        uint32_t status = dw1000_read_sys_status();

        if (status & SYS_STATUS_RXFCG) {
            SEGGER_RTT_printf(0, "[RX] got frame! status=0x%08X\n", status);
            dw1000_clear_sys_status(SYS_STATUS_RXFCG);
            dw1000_write32(DW_REG_SYS_CTRL, SYS_CTRL_RXENAB);

        } else if (status & SYS_STATUS_ALL_RX_ERR) {
            SEGGER_RTT_printf(0, "[RX] error status=0x%08X\n", status);
            if (status & SYS_STATUS_RXPHE)   SEGGER_RTT_printf(0, "  -> RXPHE\n");
            if (status & SYS_STATUS_RXFCE)   SEGGER_RTT_printf(0, "  -> RXFCE\n");
            if (status & SYS_STATUS_RXRFSL)  SEGGER_RTT_printf(0, "  -> RXRFSL\n");
            if (status & SYS_STATUS_RXRFTO)  SEGGER_RTT_printf(0, "  -> RXRFTO\n");
            if (status & SYS_STATUS_LDEERR)  SEGGER_RTT_printf(0, "  -> LDEERR\n");
            if (status & SYS_STATUS_RXSFDTO) SEGGER_RTT_printf(0, "  -> RXSFDTO\n");
            dw1000_clear_sys_status(SYS_STATUS_ALL_RX_ERR);
            dw1000_rx_reset();
            dw1000_write32(DW_REG_SYS_CTRL, SYS_CTRL_RXENAB);
        } 
    }
}

// NOTE: In this simple implementation T3 is sent as 0 in the response.
// The initiator will compute an incorrect distance. To fix this properly,
// the responder needs to know T3 before sending — this requires either:
// a) delayed TX: schedule the response at a known future time, so T3 = TX_time + known_delay
