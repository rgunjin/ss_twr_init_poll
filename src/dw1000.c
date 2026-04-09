#include "dw1000.h"
#include "SEGGER_RTT.h"
#include "dw1000_regs.h"
#include "spi.h"
#include <stdint.h>

// =============================================================================
// Internal: build SPI header bytes for DW1000 transaction
//
// DW1000 datasheet section 2.2.1 — SPI transaction header format:
//
// Without sub-address (1 byte):
//   [RW | addr(6) | 0]
//
// With sub-address, short (2 bytes):
//   [RW | addr(6) | 1]  [0 | sub(7)]
//
// With sub-address, extended (3 bytes, sub > 127):
//   [RW | addr(6) | 1]  [1 | sub_low(7)]  [sub_high(8)]
//
// RW bit: 0 = read, 1 = write
// Bit 6: sub-address present flag
// Bit 7 of second byte: extended sub-address flag
//
// Returns header length (1, 2, or 3).
// =============================================================================
static uint8_t build_header(uint8_t *hdr, uint8_t addr, uint16_t sub,
                            uint8_t has_sub, uint8_t write) {
    uint8_t len = 0;

    if (!has_sub) {
        // Simple case: no sub-address
        hdr[len++] = (write ? 0x80 : 0x00) | addr;
    } else if (sub <= 127) {
        // Short sub-address
        hdr[len++] = (write ? 0x80 : 0x00) | addr | 0x40;
        hdr[len++] = (uint8_t)sub;
    } else {
        // Extended sub-address (sub > 127)
        hdr[len++] = (write ? 0x80 : 0x00) | addr | 0x40;
        hdr[len++] = 0x80 | (uint8_t)(sub & 0x7F);      // EXT=1, low 7 bits
        hdr[len++] = (uint8_t)(sub >> 7);               // high 8 bits
    }
    return len;
}

// =============================================================================
// Low-level register access
// =============================================================================
void dw1000_read_reg(uint8_t addr, uint8_t *buf, uint8_t len) {
    uint8_t hdr[1];
    build_header(hdr, addr, 0, 0, 0);       // Read, no sub-address

    spi_cs_low();
    spi_transfer_split(hdr, 1, buf, len, 1);
    spi_cs_high();
}

void dw1000_write_reg(uint8_t addr, uint8_t *buf, uint8_t len) {
    uint8_t hdr[1];
    build_header(hdr, addr, 0, 0, 1);       // Write, no sub-address

    spi_cs_low();
    spi_transfer_split(hdr, 1, buf, len, 0);
    spi_cs_high();
}

void dw1000_read_subreg(uint8_t addr, uint16_t sub, uint8_t *buf, uint8_t len) {
    uint8_t hdr[3];
    uint8_t hdr_len = build_header(hdr, addr, sub, 1, 0);   // Read, with sub-address

    spi_cs_low();
    spi_transfer_split(hdr, hdr_len, buf, len, 1);
    spi_cs_high();
}

void dw1000_write_subreg(uint8_t addr, uint16_t sub, uint8_t *buf, uint8_t len) {
    uint8_t hdr[3];
    uint8_t hdr_len = build_header(hdr, addr, sub, 1, 1);   // Write, with sub-address

    spi_cs_low();
    spi_transfer_split(hdr, hdr_len, buf, len, 0);
    spi_cs_high();
}

// Convenience: read 32-bit register (little-endian, no sub-address)
uint32_t dw1000_read32(uint8_t addr) {
    uint8_t buf[4];
    dw1000_read_reg(addr, buf, 4);
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}
    
// Convenience: write 32-bit register (little-endian, no sub-address)
void dw1000_write32(uint8_t addr, uint32_t value) {
    uint8_t buf[4];
    buf[0] = (uint8_t)value;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value >> 16);
    buf[3] = (uint8_t)(value >> 24);
    dw1000_write_reg(addr, buf, 4);
} 

// =============================================================================
// Static helpers — mirrors of Decawave internal functions
// =============================================================================
// Simply busy-wait delay (calibrated roughly for nRF52832 @ 64MHz)
static void dw_delay(volatile uint32_t count) {
    while (count--) {}
}

// enableclocks_xti - switch system clock to XTAL oscillator (FORCE_SYS_XTI)
// must be called before reading OTP - PLL clock makes reads unreliable
// NOTE: low byte must be written before high byte (datasheet requirement)
static void enableclocks_xti(void) {
    uint8_t reg[2];
    dw1000_read_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL0, reg, 2);
    reg[0] = (reg[0] & 0xFC) | 0x01;
    dw1000_write_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL0,     &reg[0], 1);
    // reg[1] don't touch, decawave make that
}

// enableclocks_seq — return clocks to normal sequenced mode (ENABLE_ALL_SEQ)
// Called after OTP read and LDE load are complete
static void enableclocks_seq(void) {
    uint8_t reg[2];
    dw1000_read_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL0, reg, 2);
    reg[0] = 0x00;
    reg[1] = reg[1] & 0xFE;
    dw1000_write_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL0,     &reg[0], 1);
    dw1000_write_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL0 + 1, &reg[1], 1);
}

// enableclocks_lde - force clocks for LDE microcode load (FORCE_LDE)
// Sets CTRL0[0] = 1 and CTRL1[1:3] = 3 to enable the LDE clock domain
static void enableclocks_lde(void) {
    uint8_t reg[2];
    dw1000_read_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL0, reg, 2);
    reg[0] = 0x01;
    reg[1] = 0x03;
    dw1000_write_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL0,     &reg[0], 1);
    dw1000_write_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL0 + 1, &reg[1], 1);
}

// otp_read - read one 32-bit word form OTP memory at the given address
// Mirrors _dwt_otpread() from deca_device.c
// Clock must be set to XTI before calling (call enableclocks_xti first)
static uint32_t otp_read(uint16_t otp_addr) {
    uint16_t addr = otp_addr;
    // Write address
    dw1000_write_subreg(DW_REG_OTP_IF, DW_SUBREG_OTP_ADDR, (uint8_t *)&addr, 2);
    // Trigger read: OTPREAD is self-clearing, OTPRDEN is not
    uint8_t ctrl = DW_OTP_CTRL_OTPREAD | DW_OTP_CTRL_OTPRDEN;
    dw1000_write_subreg(DW_REG_OTP_IF, DW_SUBREG_OTP_CTRL, &ctrl, 1);
    // Clear OTPRDEN manually
    ctrl = 0x00;
    dw1000_write_subreg(DW_REG_OTP_IF, DW_SUBREG_OTP_CTRL, &ctrl, 1);
    // Read result from output register
    uint32_t val = 0;
    dw1000_read_subreg(DW_REG_OTP_IF, DW_SUBREG_OTP_RDAT, (uint8_t *)&val, 4);
    return val;
}

// load_lde_microcode - load LDE algorithm from ROM into RAM
// Mirrors _dwt_loaducodefromrom() from deca_device.c
// Without this, RX timestamps will be inaccurate
static void load_lde_microcode(void) {
    enableclocks_lde();
    // Kick LDE load - takes up to 120us, we wait ~1ms to be safe
    uint16_t ctrl = DW_OTP_CTRL_LDELOAD;
    dw1000_write_subreg(DW_REG_OTP_IF, DW_SUBREG_OTP_CTRL, (uint8_t *)&ctrl, 2);
    dw_delay(100000);
    enableclocks_seq();
}


// =============================================================================
// High-level API
// =============================================================================
uint32_t dw1000_read_dev_id(void) {
    return dw1000_read32(DW_REG_DEV_ID);
}

// Soft reset via PMSC_CTRL0 register (sub-address 0x03)
// Mirrors dwt_softreset() from deca_device.c
static void dw1000_softreset(void) {
    // 1. Переключаем системный клок на XTAL осциллятор
    //    Это обязательно перед любым изменением PMSC -
    //    PLL клок не стабилен во время reset
    enableclocks_xti();

    // 2. Отключаем RF sequencing в PMSC_CTRL1
    //    0x0300 = PMSC_CTRL1_PKTSEQ_DISABLE (биты [9:8] = 11)
    //    Без этого RF блоки могут остаться в неопределенном состоянии
    uint16_t ctrl1 = 0x0300;
    dw1000_write_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL1, (uint8_t *)&ctrl1, 2);

    // 3. Минимальный сброс AON array
    //    Сначала сбрасываем, командой 0x00, затем сохраняем 0x02
    //    Это записывает текущие (нулевые) регистры в AON array
    //    чтобы при следующем wake-up не восстановился старый конфиг
    uint8_t aon_ctrl = 0x00;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CTRL, &aon_ctrl, 1);
    aon_ctrl = 0x02;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CTRL, &aon_ctrl, 1);
    
    // 4. Сброс всех блоков: HIF, TX, RX, PMSC
    //    Пишим 0x00 в байт 3 регистра PMSC_CTRL0 (sub-offset 0x03)
    uint8_t reset = 0x00;
    dw1000_write_subreg(DW_REG_PMSC, 0x03, &reset, 1);

    // 5. Ждем ~1ms - даем PLL залочиться после reset
    dw_delay(100000);

    // 6. Снимаем reset (0xF0 = все биты reset в 1 = нормальная работа)
    uint8_t clear = 0xF0;
    dw1000_write_subreg(DW_REG_PMSC, 0x03, &clear, 1);
}


int dw1000_init(void) {
    SEGGER_RTT_printf(0, "[INIT] step 0: status=0x%08X\n",
                      dw1000_read_sys_status());
    // 0, Force TRXOFF - вывести из любого активного состояния
    dw1000_write32(DW_REG_SYS_CTRL, SYS_CTRL_TRXOFF);
    dw_delay(10000);
    dw1000_clear_sys_status(0xFFFFFFFF);

    SEGGER_RTT_printf(0, "[INIT] step 1 after clear: status=0x%08X\n",
                      dw1000_read_sys_status());

    // 1. Verify DEV_ID before reset - fail fast is SPI broken
    if (dw1000_read_dev_id() != DW1000_DEV_ID) {
        return DW_ERROR;
    }

    // 2. Soft reset to put chip in know state
    dw1000_softreset();

    // 3. Small delay after reset - DW1000-datasheet recommends at least 10us.
    // At 64MHz, 1000 iterations ~ 16us
    dw_delay(100000);

    SEGGER_RTT_printf(0, "[INIT] step 2 after softreset: status=0x%08X\n",
                      dw1000_read_sys_status());

    // 4. Switch system clock to XTAL - required before reading OTP
    enableclocks_xti();

    // 5. Enable CPLL lock to detect on EXT_SYNC
    uint8_t ec_ctrl = DW_EC_CTRL_PLLLCK;
    dw1000_write_subreg(DW_REG_EXT_SYNC, DW_SUBREG_EC_CTRL, &ec_ctrl, 1);

    // 6. Read XTAL trim from OTP (address 0x1E)
    //    bits [4:0]  = trim value
    //    bits [15:8] = OTP revision number
    uint32_t otp_xtrim = otp_read(DW_OTP_ADDR_XTRIM);
    uint8_t xtrim = otp_xtrim & 0x1F;
    if (xtrim == 0) {
        // OTP not programmed — use mid-range as safe default
        xtrim = DW_FS_XTALT_MIDRANGE;
    }

    // 7. Read  LDO tune tune from OTP - kick it if a value is programmed
    //    LDO tune improves RF performance and measurement range
    uint32_t ldo_tune = otp_read(DW_OTP_ADDR_LDOTUNE);
    if ((ldo_tune & 0xFF) != 0) {
        uint8_t sf = DW_OTP_SF_LDO_KICK;
        dw1000_write_subreg(DW_REG_OTP_IF, DW_SUBREG_OTP_SF, &sf, 1);
    }

    // 8. Apply XTAL trim to FS_XTALT register
    //    Bits [6:5] are reserved and must always be 1 — OR with 0x60
    uint8_t xtal = DW_FS_XTALT_RESERVED | (xtrim & DW_FS_XTALT_MASK);
    dw1000_write_subreg(DW_REG_FS_CTRL, DW_SUBREG_FS_XTALT, &xtal, 1);

    SEGGER_RTT_printf(0, "[INIT] step 3 before LDE: status=0x%08X\n",
                      dw1000_read_sys_status());

    // 9. Load LDE microcode from ROM into chip RAM
    //    This enables accurate RX timestamps required for ranging
    load_lde_microcode();

    SEGGER_RTT_printf(0, "[INIT] step 4 after LDE: status=0x%08X\n",
                      dw1000_read_sys_status());

    // 10. Return clock to normal sequenced mode
    enableclocks_seq();

    SEGGER_RTT_printf(0, "[INIT] step 5 after enableclocks_seq: status=0x%08X\n",
                      dw1000_read_sys_status());

    // 10a. Enable LDE algorithm - must be set after loading microcode
    // Without this bit RX timestamp are garbage even if RX works
    // PMSC_CTRL1 is at sub-address 0x04, 2 bytes
    // LDERUN = bit 9 of the full 32-register = 9 bit of the 16-bit word
    uint16_t pmsc_ctrl1 = 0;
    dw1000_read_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL1, (uint8_t *)&pmsc_ctrl1, 2);
    pmsc_ctrl1 |= (1U << 9);        // LDERUN
    dw1000_write_subreg(DW_REG_PMSC, DW_SUBREG_PMSC_CTRL1, (uint8_t *)&pmsc_ctrl1, 2);

    SEGGER_RTT_printf(0, "[INIT] step 6 after LDERUN: status=0x%08X\n",
                      dw1000_read_sys_status());

    // 11. AON - запретить автоматический уход в sleep
    // В этом примере sleep не используем, поэтому все занулили
    // I) AON_WCFG = 0 - ничего не востанавливать при wake-up
    uint16_t aon_wcfg = 0x0000;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_WCFG, (uint8_t *)&aon_wcfg, 2);

    // II) AON_CFG0 = 0 - отключаем все триггеры засыпания
    //      бит 0: SLEEP_EN - разрешить sleep (0 = запрещено)
    //      бит 1: WAKE_PIN - wake по IRQ пину
    //      бит 2: WAKE_SPI - wake по SPI CS
    //      бит 3: WAKE_CNT - wake по таймеру
    //      все в 0 = чип не спит никогда
    uint8_t aon_cfg0 = 0x00;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CFG0, &aon_cfg0, 1);

    // III) AON_CFG1 = 0 - доп. настройки sleep, тоже зануляем
    uint8_t aon_cfg1 = 0x00;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CFG1, &aon_cfg1, 1);

    // IV) Сохранить эту конфигурацию в AON массив
    // Сначала пишем 0 (сброс команды), потом SAVE (0x02)
    uint8_t aon_ctrl = 0x00;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CTRL, &aon_ctrl, 1);
    aon_ctrl = 0x02;        
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CTRL, &aon_ctrl, 1);
    aon_ctrl = 0x00;
    dw1000_write_subreg(DW_REG_AON, DW_SUBREG_AON_CTRL, &aon_ctrl, 1);

    SEGGER_RTT_printf(0, "[INIT] step 7 after AON: status=0x%08X\n",
                      dw1000_read_sys_status());

    // 12. Final sanity check - verify chip still responds after init
    if (dw1000_read_dev_id() != DW1000_DEV_ID) {
        SEGGER_RTT_printf(0, "[INIT] DEV_ID check failed after init\n");
        return DW_ERROR;
    }
    return DW_SUCCESS;
}

void dw1000_write_tx_data(uint8_t *data, uint8_t len, uint8_t offset) {
    // TX_BUFFER is a simple register with no sub-address structure —
    // the offset is passed as the sub-address into the 1024-byte buffer
    dw1000_write_subreg(DW_REG_TX_BUFFER, offset, data, len);
}

void dw1000_write_tx_fctrl(uint8_t len, uint8_t offset, uint8_t ranging) {
    // TX_FCTRL (0x08) is a 5-byte register. We only need to set bits [9:0]:
    //   bits [9:0]  — TFLEN: frame length (payload + 2 CRC bytes)
    //   bits [12:10] — TFLE: frame length extension (usually 0)
    //   bits [14:13] — TXBR: bitrate (we keep existing value)
    //   bit  [15]   — TR: ranging bit
    //
    // Read-modify-write to preserve TXBR and other config bits
    uint8_t buf[5];
    dw1000_read_reg(DW_REG_TX_FCTRL, buf, 5);

    // Reconstruct lower 32 bits
    uint32_t fctrl = (uint32_t)buf[0] |
                     ((uint32_t)buf[1] << 8) |
                     ((uint32_t)buf[2] << 16) |
                     ((uint32_t)buf[3] << 24); 

    // Clear and set frame length bits [9:0]
    fctrl &= ~0x3FFUL;
    fctrl |= (uint32_t)(len + 2) & 0x3FF;   // +2 for CRC

    // Clear and set buffer offset bits [22:16]
    fctrl &= ~(0x3FUL << 16);
    fctrl |= ((uint32_t)offset & 0x3F) << 16;

    // Set or clear ranging bit [15]
    if (ranging) {
        fctrl |= TX_FCTRL_TR;
    } else {
        fctrl &= ~TX_FCTRL_TR;
    }

    buf[0] = (uint8_t)(fctrl);
    buf[1] = (uint8_t)(fctrl >> 8);
    buf[2] = (uint8_t)(fctrl >> 16);
    buf[3] = (uint8_t)(fctrl >> 24);
    // buf[4] (byte 4 of TX_FCTRL) — leave unchanged

    dw1000_write_reg(DW_REG_TX_FCTRL, buf, 5);
}

void dw1000_start_tx(uint8_t mode) {
    uint8_t ctrl = 0;
    if (mode & DW_TX_DELAYED) {
        ctrl |= SYS_CTRL_TXDLYS;
    }
    if (mode & DW_TX_WAIT4RESP) {
        ctrl |= SYS_CTRL_WAIT4RESP;
    }
    ctrl |= SYS_CTRL_TXSTRT;
    dw1000_write_subreg(DW_REG_SYS_CTRL, 0x00, &ctrl, 1);
}

void dw1000_rx_enable(void) {
    uint8_t ctrl = 0x01;  // RXENAB = бит 8 = второй байт = 0x01
    dw1000_write_subreg(DW_REG_SYS_CTRL, 0x01, &ctrl, 1);
}

void dw1000_read_rx_data(uint8_t *buf, uint8_t len) {
    // RX_BUFFER (0x11) — read from offset 0
    dw1000_read_subreg(DW_REG_RX_BUFFER, 0, buf, len);
}

uint32_t dw1000_read_tx_timestamp(void) {
    // TX_TIME (0x17) — 5-byte timestamp, we read the lower 4 bytes.
    // For SS-TWR this is sufficient (32-bit wraps ~17 seconds, much longer
    // than any ranging exchange)
    uint8_t buf[4];
    dw1000_read_subreg(DW_REG_TX_TIME, 0, buf, 4);
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

uint32_t dw1000_read_rx_timestamp(void) {
    // RX_TIME (0x15) — 5-byte timestamp, lower 4 bytes
    uint8_t buf[4];
    dw1000_read_subreg(DW_REG_RX_TIME, 0, buf, 4);
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

uint64_t dw1000_read_rx_timestamp_u64(void) {
    uint8_t buf[5];
    dw1000_read_subreg(DW_REG_RX_TIME, 0, buf, 5);
    return (uint64_t)buf[0]         |
           ((uint64_t)buf[1] << 8)  |
           ((uint64_t)buf[2] << 16) |
           ((uint64_t)buf[3] << 24) |
           ((uint64_t)buf[4] << 32);
}


uint32_t dw1000_read_sys_status(void) {
    return dw1000_read32(DW_REG_SYS_STATUS);
}

void dw1000_clear_sys_status(uint32_t mask) {
    // SYS_STATUS is write-1-to-clean: writing a 1 bit clear the flag
    dw1000_write32(DW_REG_SYS_STATUS, mask);
}

uint32_t dw1000_read_rx_finfo(void) {
    return dw1000_read32(DW_REG_RX_FINFO);
}

void dw1000_trxoff(void) {
    dw1000_write32(DW_REG_SYS_CTRL, SYS_CTRL_TRXOFF);
}

void dw1000_rx_reset(void) {
    uint8_t reset = DW_PMSC_CTRL0_RESET_RX;
    uint8_t clear = DW_PMSC_CTRL0_RESET_CLEAR;
    dw1000_write_subreg(DW_REG_PMSC, 0x03, &reset, 1);
    dw1000_write_subreg(DW_REG_PMSC, 0x03, &clear, 1);
}

void dw1000_set_antenna_delay(uint16_t tx_delay, uint16_t rx_delay) {
    // TX_ANTD (0x18) - 16-bit TX antenna delay
    dw1000_write_subreg(DW_REG_TX_ANTD, 0x00, (uint8_t *)&tx_delay, 2);
    // LDE_RXANTD - RX antenna delay lives in LDE_CTRL
    dw1000_write_subreg(DW_REG_LDE_CTRL, 0x1804, (uint8_t *)&rx_delay, 2);
}

void dw1000_set_delayed_tx_time(uint32_t tx_time) {
    // DX_TIME (0x0A) - 5 байт, но аппаратно значим только биты [40:9],
    // т.е. мы пишем уже сдвинутое значение в байты [1..4]
    // Проще всего заипсать как 4-байтовое слово начиная с байта 1
    uint8_t buf[4];
    buf[0] = (uint8_t)(tx_time);
    buf[1] = (uint8_t)(tx_time >> 8);
    buf[2] = (uint8_t)(tx_time >> 16);
    buf[3] = (uint8_t)(tx_time >> 24);
    dw1000_write_subreg(DW_REG_DX_TIME, 0x01, buf, 4);
}

int dw1000_start_tx_delayed(void) {
    // Выставляем TXDLYS + TXSTRT в SYS_CTRL
    uint8_t ctrl = SYS_CTRL_TXDLYS | SYS_CTRL_TXSTRT;
    dw1000_write_subreg(DW_REG_SYS_CTRL, 0x00, &ctrl, 1);

    // Проверяем HPDWARN - если выставлен, чип не успел подготовиться
    // к запланированному времени TX. Передача не произошла
    uint32_t status = dw1000_read_sys_status();
    if (status & SYS_STATUS_HPDWARN) {
        // Отменяем - принудительно выключаем трансивер
        dw1000_trxoff();
        dw1000_clear_sys_status(SYS_STATUS_HPDWARN);
        return DW_ERROR;
    }

    return DW_SUCCESS;
}
