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
// LED helpers (active LOW)
// =============================================================================

static void led_init(void) {
    NRF_GPIO->DIRSET = (1 << 30) | (1 << 31) | (1 << 22) | (1 << 14);
    NRF_GPIO->OUTSET = (1 << 30) | (1 << 31) | (1 << 22) | (1 << 14);
}

static void led_on(uint8_t pin)  { NRF_GPIO->OUTCLR = (1 << pin); }
static void led_off(uint8_t pin) { NRF_GPIO->OUTSET = (1 << pin); }

// =============================================================================
// Simple delay
// =============================================================================

static void delay(volatile uint32_t count) {
    while (count--) {}
}

// =============================================================================
// Test helpers
// =============================================================================

static uint32_t pass_count = 0;
static uint32_t fail_count = 0;

static void test_u32(const char *name, uint32_t got, uint32_t expected) {
    if (got == expected) {
        SEGGER_RTT_printf(0, "[PASS] %s = 0x%08X\n", name, got);
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] %s: got 0x%08X, expected 0x%08X\n",
                          name, got, expected);
        fail_count++;
    }
}

static void test_nonzero(const char *name, uint32_t got) {
    if (got != 0) {
        SEGGER_RTT_printf(0, "[PASS] %s = 0x%08X (nonzero)\n", name, got);
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] %s = 0x00000000 (expected nonzero)\n", name);
        fail_count++;
    }
}

static void test_result(const char *name, int got, int expected) {
    if (got == expected) {
        SEGGER_RTT_printf(0, "[PASS] %s = %d\n", name, got);
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] %s: got %d, expected %d\n",
                          name, got, expected);
        fail_count++;
    }
}

// =============================================================================
// main
// =============================================================================

int main(void) {
    led_init();
    led_on(30);

    // Hardware reset DW1000 via RST pin (P0.24)
    // Drive RST low, then release (hi-z) — DW1000 boots in ~5ms
    NRF_GPIO->DIRSET = (1UL << 24);    // RST as output
    NRF_GPIO->OUTCLR = (1UL << 24);    // RST low
    delay(10000);                       // ~1ms
    NRF_GPIO->DIRCLR = (1UL << 24);    // RST as input (release, hi-z)
    delay(500000);                      // ~5ms boot time

    spi_init(SPIM_FREQ_2M);

    SEGGER_RTT_printf(0, "\n");
    SEGGER_RTT_printf(0, "========================================\n");
    SEGGER_RTT_printf(0, "  DW1000 init smoke tests\n");
    SEGGER_RTT_printf(0, "========================================\n\n");

    // ------------------------------------------------------------------
    // Test 1: DEV_ID before init
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "--- Test 1: DEV_ID (pre-init) ---\n");
    uint32_t dev_id = dw1000_read_dev_id();
    test_u32("DEV_ID", dev_id, DW1000_DEV_ID);

    // ------------------------------------------------------------------
    // Test 2: dw1000_init
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n--- Test 2: dw1000_init() ---\n");
    int init_result = dw1000_init();
    test_result("dw1000_init", init_result, DW_SUCCESS);

    if (init_result != DW_SUCCESS) {
        SEGGER_RTT_printf(0, "\nINIT FAILED — skipping remaining tests\n");
        led_on(14);
        while (1) {}
    }

    // ------------------------------------------------------------------
    // Test 3: DEV_ID after init
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n--- Test 3: DEV_ID (post-init) ---\n");
    dev_id = dw1000_read_dev_id();
    test_u32("DEV_ID", dev_id, DW1000_DEV_ID);

    // ------------------------------------------------------------------
    // Test 4: SYS_CFG
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n--- Test 4: SYS_CFG ---\n");
    uint32_t sys_cfg = dw1000_read32(DW_REG_SYS_CFG);
    SEGGER_RTT_printf(0, "[INFO] SYS_CFG = 0x%08X\n", sys_cfg);
    test_nonzero("SYS_CFG", sys_cfg);

    // ------------------------------------------------------------------
    // Test 5: FS_XTALT — XTAL trim written correctly
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n--- Test 5: FS_XTALT ---\n");
    uint8_t xtalt = 0;
    dw1000_read_subreg(DW_REG_FS_CTRL, DW_SUBREG_FS_XTALT, &xtalt, 1);
    SEGGER_RTT_printf(0, "[INFO] FS_XTALT = 0x%02X\n", xtalt);
    if ((xtalt & 0x60) == 0x60) {
        SEGGER_RTT_printf(0, "[PASS] FS_XTALT reserved bits [6:5] = 1 (correct)\n");
        SEGGER_RTT_printf(0, "[INFO] trim value = 0x%02X\n", xtalt & 0x1F);
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] FS_XTALT reserved bits [6:5] not set\n");
        fail_count++;
    }

    // ------------------------------------------------------------------
    // Test 6: PMSC_CTRL0 — clocks in sequenced mode
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n--- Test 6: PMSC_CTRL0 (clocks sequenced) ---\n");
    uint8_t pmsc[4];
    dw1000_read_reg(DW_REG_PMSC, pmsc, 4);
    SEGGER_RTT_printf(0, "[INFO] PMSC_CTRL0 = 0x%02X 0x%02X\n", pmsc[0], pmsc[1]);
    if ((pmsc[0] & 0x03) == 0x00) {
        SEGGER_RTT_printf(0, "[PASS] PMSC_CTRL0 bits [1:0] = 0 (sequenced mode)\n");
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] PMSC_CTRL0 bits [1:0] != 0 (clock not released)\n");
        fail_count++;
    }

    // ------------------------------------------------------------------
    // Test 7: AON_CFG1 — must be 0x00 after init
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n--- Test 7: AON_CFG1 ---\n");
    uint8_t aon_cfg1 = 0xFF;
    dw1000_read_subreg(DW_REG_AON, DW_SUBREG_AON_CFG1, &aon_cfg1, 1);
    SEGGER_RTT_printf(0, "[INFO] AON_CFG1 = 0x%02X\n", aon_cfg1);
    if (aon_cfg1 == 0x00) {
        SEGGER_RTT_printf(0, "[PASS] AON_CFG1 = 0x00\n");
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] AON_CFG1 != 0x00\n");
        fail_count++;
    }

    // ------------------------------------------------------------------
    // Test 8: TX_FCTRL round-trip
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n--- Test 8: TX_FCTRL round-trip ---\n");
    uint8_t tx_data[8] = {0x41, 0x88, 0x00, 0xCA, 0xDE, 0x57, 0x41, 0xE0};
    dw1000_write_tx_data(tx_data, 8, 0);
    dw1000_write_tx_fctrl(8, 0, 1);
    uint32_t fctrl = dw1000_read32(DW_REG_TX_FCTRL);
    SEGGER_RTT_printf(0, "[INFO] TX_FCTRL = 0x%08X\n", fctrl);
    uint32_t actual_len  = fctrl & 0x3FF;
    uint32_t ranging_bit = (fctrl >> 15) & 1;
    if (actual_len == 10) {
        SEGGER_RTT_printf(0, "[PASS] TX_FCTRL frame length = %lu\n", actual_len);
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] TX_FCTRL length: got %lu, expected 10\n", actual_len);
        fail_count++;
    }
    if (ranging_bit == 1) {
        SEGGER_RTT_printf(0, "[PASS] TX_FCTRL TR (ranging) bit = 1\n");
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] TX_FCTRL TR bit not set\n");
        fail_count++;
    }

    // ------------------------------------------------------------------
    // Test 9: dw1000_configure
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n--- Test 9: dw1000_configure() ---\n");
    dw1000_config_t cfg = DW1000_DEFAULT_CONFIG;
    dw1000_configure(&cfg);

    // chip still alive after configure?
    dev_id = dw1000_read_dev_id();
    test_u32("DEV_ID post-configure", dev_id, DW1000_DEV_ID);

    // CHAN_CTRL: CH5 TX+RX, PRF64, preamble code 9
    uint32_t chan_ctrl = dw1000_read32(DW_REG_CHAN_CTRL);
    SEGGER_RTT_printf(0, "[INFO] CHAN_CTRL = 0x%08X\n", chan_ctrl);
    uint8_t tx_chan = (chan_ctrl >> 0)  & 0xF;
    uint8_t rx_chan = (chan_ctrl >> 4)  & 0xF;
    uint8_t rx_prf  = (chan_ctrl >> 18) & 0x3;
    uint8_t tx_code = (chan_ctrl >> 22) & 0x1F;
    uint8_t rx_code = (chan_ctrl >> 27) & 0x1F;
    SEGGER_RTT_printf(0, "[INFO] tx_chan=%d rx_chan=%d prf=%d tx_code=%d rx_code=%d\n",
                      tx_chan, rx_chan, rx_prf, tx_code, rx_code);
    if (tx_chan == 5 && rx_chan == 5 && rx_prf == 2 &&
        tx_code == 9 && rx_code == 9) {
        SEGGER_RTT_printf(0, "[PASS] CHAN_CTRL fields correct\n");
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] CHAN_CTRL fields wrong\n");
        fail_count++;
    }

    // FS_PLLCFG: expect 0x0800041D for CH5
    uint8_t pllbuf[4];
    dw1000_read_subreg(DW_REG_FS_CTRL, DW_SUBREG_FS_PLLCFG, pllbuf, 4);
    uint32_t pllcfg = (uint32_t)pllbuf[0]        |
                      ((uint32_t)pllbuf[1] << 8)  |
                      ((uint32_t)pllbuf[2] << 16) |
                      ((uint32_t)pllbuf[3] << 24);
    test_u32("FS_PLLCFG", pllcfg, 0x0800041DUL);

    // DRX_TUNE1a: expect 0x008D for PRF 64MHz
    uint8_t tune1a_buf[2];
    dw1000_read_subreg(DW_REG_DRX_CONF, DW_SUBREG_DRX_TUNE1A, tune1a_buf, 2);
    uint16_t tune1a = (uint16_t)tune1a_buf[0] | ((uint16_t)tune1a_buf[1] << 8);
    SEGGER_RTT_printf(0, "[INFO] DRX_TUNE1a = 0x%04X\n", tune1a);
    if (tune1a == 0x008D) {
        SEGGER_RTT_printf(0, "[PASS] DRX_TUNE1a = 0x008D (PRF 64MHz correct)\n");
        pass_count++;
    } else {
        SEGGER_RTT_printf(0, "[FAIL] DRX_TUNE1a wrong: got 0x%04X\n", tune1a);
        fail_count++;
    }

    // AGC_TUNE1: expect 0x889B for PRF 64MHz
    uint8_t agc1_buf[2];
    dw1000_read_subreg(DW_REG_AGC_CTRL, DW_SUBREG_AGC_TUNE1, agc1_buf, 2);
    uint16_t agc1 = (uint16_t)agc1_buf[0] | ((uint16_t)agc1_buf[1] << 8);
    test_u32("AGC_TUNE1", (uint32_t)agc1, 0x889B);

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    SEGGER_RTT_printf(0, "\n========================================\n");
    SEGGER_RTT_printf(0, "  Results: %lu passed, %lu failed\n",
                      pass_count, fail_count);
    SEGGER_RTT_printf(0, "========================================\n");

    if (fail_count == 0) {
        SEGGER_RTT_printf(0, "  ALL TESTS PASSED\n\n");
        while (1) {
            led_on(30);
            delay(500000);
            led_off(30);
            delay(500000);
        }
    } else {
        SEGGER_RTT_printf(0, "  SOME TESTS FAILED\n\n");
        while (1) {
            led_on(14);
            delay(100000);
            led_off(14);
            delay(100000);
        }
    }
}
