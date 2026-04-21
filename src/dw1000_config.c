#include "dw1000_config.h"
#include "dw1000.h"
#include "dw1000_regs.h"
#include "SEGGER_RTT.h"
#include <stdint.h>

// =============================================================================
// Register values for channel 5, PRF 64MHz, 6.8Mbps
// All values taken directly from deca_regs.h / deca_params_init.c
// =============================================================================

// FS_CTRL - frequency synthesiser
#define CFG_FS_PLLCFG       0x0800041DUL    // CH5
#define CFG_FS_PLLTUNE      0xBE            // CH5

// RF blocks
#define CFG_RF_RXCTRLH      0xD8            // narrow bandwidth (CH5)
#define CFG_RF_TXCTRL       0x001E3FE0UL    // CH5
#define CFG_TC_PGDELAY      0xC0            // CH5 pulse generator delay

// DRX - digital recieve tuning
// PRF 64MHz, 6.8 Mbps, standard SFD, preamble 128, PAC8
#define CFG_DRX_TUNE0b      0x0016          // 110kbps, non-standard SFD
#define CFG_DRX_TUNE1a      0x008D          // PRF 64MHz
#define CFG_DRX_TUNE1b      0x0064          // preamble > 64 symbols, 6.8Mbps
#define CFG_DRX_TUNE2       0x372A011BUL    // PRF 64MHz, PAC8
#define CFG_DRX_TUNE4H      0x0010          // preamble >= 128 symbols

// AGC — automatic gain control
#define CFG_AGC_TUNE1       0x889B          // PRF 64MHz
#define CFG_AGC_TUNE2       0x2502A907UL    // fixed value, same for all configs

// LDE — leading edge detection algorithm
#define CFG_LDE_CFG1        0x6D            // NTM=13, PMULT=3 (same for all PRF)
#define CFG_LDE_CFG2        0x0607          // PRF 64MHz
#define CFG_LDE_REPC        0x051E          // 0x28F4 >> 3, for 110kbps

// SFD timeout: preamble(128) + SFD(8) + 1 = 137
#define CFG_SFD_TO          0xFFFF

// CHAN_CTRL fields
#define CFG_CHAN            5
#define CFG_PRF_VAL         2               // DW_PRF_64M = 2, goes into RXPRF bits
#define CFG_TX_CODE         9
#define CFG_RX_CODE         9

// =============================================================================
// dw1000_configure — write all radio config registers
// Mirrors dwt_configure() from deca_device.c
// =============================================================================

void dw1000_configure(const dw1000_config_t *cfg) {
    (void)cfg;


    // Disable double RX buffer - siplifies buffer managment
    uint32_t sys_cfg = dw1000_read32(DW_REG_SYS_CFG);
    sys_cfg |= SYS_CFG_DIS_DRXB | SYS_CFG_RXM110K;
    dw1000_write32(DW_REG_SYS_CFG, sys_cfg);

    // --- Frequency synthesiser (PLL) ---
    uint32_t pllcfg  = CFG_FS_PLLCFG;
    uint8_t  plltune = CFG_FS_PLLTUNE;
    dw1000_write_subreg(DW_REG_FS_CTRL, DW_SUBREG_FS_PLLCFG,  (uint8_t *)&pllcfg,  4);
    dw1000_write_subreg(DW_REG_FS_CTRL, DW_SUBREG_FS_PLLTUNE, &plltune,             1);

    // --- RF blocks ---
    uint8_t  rxctrlh = CFG_RF_RXCTRLH;
    uint32_t txctrl  = CFG_RF_TXCTRL;
    uint8_t  pgdelay = CFG_TC_PGDELAY;
    dw1000_write_subreg(DW_REG_RF_CONF, DW_SUBREG_RF_RXCTRLH, &rxctrlh,            1);
    dw1000_write_subreg(DW_REG_RF_CONF, DW_SUBREG_RF_TXCTRL,  (uint8_t *)&txctrl,  4);
    dw1000_write_subreg(DW_REG_TX_CAL,  DW_SUBREG_TC_PGDELAY, &pgdelay,            1);

    // --- Digital RX tuning ---
    uint16_t tune0b = CFG_DRX_TUNE0b;
    uint16_t tune1a = CFG_DRX_TUNE1a;
    uint16_t tune1b = CFG_DRX_TUNE1b;
    uint32_t tune2  = CFG_DRX_TUNE2;
    uint16_t tune4h = CFG_DRX_TUNE4H;
    uint16_t sfdto  = CFG_SFD_TO;
    dw1000_write_subreg(DW_REG_DRX_CONF, DW_SUBREG_DRX_TUNE0B, (uint8_t *)&tune0b, 2);
    dw1000_write_subreg(DW_REG_DRX_CONF, DW_SUBREG_DRX_TUNE1A, (uint8_t *)&tune1a, 2);
    dw1000_write_subreg(DW_REG_DRX_CONF, DW_SUBREG_DRX_TUNE1B, (uint8_t *)&tune1b, 2);
    dw1000_write_subreg(DW_REG_DRX_CONF, DW_SUBREG_DRX_TUNE2,  (uint8_t *)&tune2,  4);
    dw1000_write_subreg(DW_REG_DRX_CONF, DW_SUBREG_DRX_TUNE4H, (uint8_t *)&tune4h, 2);
    dw1000_write_subreg(DW_REG_DRX_CONF, DW_SUBREG_DRX_SFDTOC, (uint8_t *)&sfdto,  2);

    // --- AGC ---
    uint16_t agc1 = CFG_AGC_TUNE1;
    uint32_t agc2 = CFG_AGC_TUNE2;
    dw1000_write_subreg(DW_REG_AGC_CTRL, DW_SUBREG_AGC_TUNE1, (uint8_t *)&agc1, 2);
    dw1000_write_subreg(DW_REG_AGC_CTRL, DW_SUBREG_AGC_TUNE2, (uint8_t *)&agc2, 4);

    // --- LDE algorithm ---
    uint8_t  lde1 = CFG_LDE_CFG1;
    uint16_t lde2 = CFG_LDE_CFG2;
    uint16_t repc = CFG_LDE_REPC;
    dw1000_write_subreg(DW_REG_LDE_CTRL, DW_SUBREG_LDE_CFG1, &lde1,             1);
    dw1000_write_subreg(DW_REG_LDE_CTRL, DW_SUBREG_LDE_CFG2, (uint8_t *)&lde2,  2);
    dw1000_write_subreg(DW_REG_LDE_CTRL, DW_SUBREG_LDE_REPC, (uint8_t *)&repc,  2);

    // --- CHAN_CTRL ---
    uint32_t chan_ctrl =
        ((uint32_t)CFG_CHAN    << 0)  |
        ((uint32_t)CFG_CHAN    << 4)  |
        CHAN_CTRL_DWSFD               |
        CHAN_CTRL_TNSSFD              |
        CHAN_CTRL_RNSSFD              |
        ((uint32_t)CFG_PRF_VAL << 18) |
        ((uint32_t)CFG_TX_CODE << 22) |
        ((uint32_t)CFG_RX_CODE << 27);
    SEGGER_RTT_printf(0, "[DBG] chan=%d prf=%d txcode=%d rxcode=%d\n",
                  CFG_CHAN, CFG_PRF_VAL, CFG_TX_CODE, CFG_RX_CODE);
    SEGGER_RTT_printf(0, "[DBG] chan_ctrl computed=0x%08X\n", chan_ctrl);
    dw1000_write32(DW_REG_CHAN_CTRL, chan_ctrl);

    // --- Non-standard SFD length for 110kbps ---
    uint8_t sfd_len = 64;
    dw1000_write_subreg(DW_REG_USR_SFD, DW_SUBREG_SFD_LENGTH, &sfd_len, 1);
    uint8_t sfd_verify = 0;
    dw1000_read_subreg(DW_REG_USR_SFD, DW_SUBREG_SFD_LENGTH, &sfd_verify, 1);
    SEGGER_RTT_printf(0, "[DBG] USR_SFD length=0x%02X\n", sfd_verify);

    // --- TX_FCTRL: preamble length + PRF + datarate ---
    uint8_t buf[5];
    dw1000_read_reg(DW_REG_TX_FCTRL, buf, 5);
    uint32_t fctrl = (uint32_t)buf[0]        |
                     ((uint32_t)buf[1] << 8)  |
                     ((uint32_t)buf[2] << 16) |
                     ((uint32_t)buf[3] << 24);

    fctrl &= ~(0x3UL  << 13);   // datarate bits [14:13]
    fctrl &= ~(0x3UL  << 16);   // PRF bits [17:16]
    fctrl &= ~(0x3FUL << 18);   // preamble bits [23:18]

    fctrl |= ((uint32_t)DW_BR_110K   << 13);
    fctrl |= ((uint32_t)DW_PRF_64M   << 16);
    fctrl |= ((uint32_t)DW_PLEN_1024 << 18);

    buf[0] = (uint8_t)(fctrl);
    buf[1] = (uint8_t)(fctrl >> 8);
    buf[2] = (uint8_t)(fctrl >> 16);
    buf[3] = (uint8_t)(fctrl >> 24);
    dw1000_write_reg(DW_REG_TX_FCTRL, buf, 5);

    uint8_t verify[5];
    dw1000_read_reg(DW_REG_TX_FCTRL, verify, 5);
    SEGGER_RTT_printf(0, "[DBG] TX_FCTRL=0x%02X%02X%02X%02X%02X\n",
                      verify[4], verify[3], verify[2], verify[1], verify[0]);

    // --- SFD workaround ---
    // Одна запись, две записи не работают
    uint8_t ctrl = (uint8_t)(SYS_CTRL_TXSTRT | SYS_CTRL_TRXOFF);
    dw1000_write_subreg(DW_REG_SYS_CTRL, 0x00, &ctrl, 1);
}
