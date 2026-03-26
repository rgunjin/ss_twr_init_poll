#ifndef DW1000_REGS_H
#define DW1000_REGS_H

// =============================================================================
// DW1000 Register File Addresses
// DW1000 datasheet section 7 — Register Map
// =============================================================================

#define DW_REG_DEV_ID       0x00    // Device ID — read 0xDECA0130 to verify chip
#define DW_REG_EUI          0x01    // Extended Unique Identifier (64-bit)
#define DW_REG_PANADR       0x03    // PAN ID and short address
#define DW_REG_SYS_CFG      0x04    // System configuration
#define DW_REG_SYS_TIME     0x06    // System time counter (40-bit, read-only)
#define DW_REG_TX_FCTRL     0x08    // TX frame control
#define DW_REG_TX_BUFFER    0x09    // TX data buffer (up to 1023 bytes)
#define DW_REG_DX_TIME      0x0A    // Delayed send/receive time
#define DW_REG_RX_FWTO      0x0C    // RX frame wait timeout
#define DW_REG_SYS_CTRL     0x0D    // System control register
#define DW_REG_SYS_MASK     0x0E    // System event mask
#define DW_REG_SYS_STATUS   0x0F    // System event status register
#define DW_REG_RX_FINFO     0x10    // RX frame info (length, quality)
#define DW_REG_RX_BUFFER    0x11    // RX data buffer
#define DW_REG_RX_FQUAL     0x12    // RX frame quality info
#define DW_REG_RX_TTCKI     0x13    // RX time tracking interval
#define DW_REG_RX_TTCKO     0x14    // RX time tracking offset
#define DW_REG_RX_TIME      0x15    // RX timestamp (40-bit)
#define DW_REG_TX_TIME      0x17    // TX timestamp (40-bit)
#define DW_REG_TX_ANTD      0x18    // TX antenna delay
#define DW_REG_SYS_STATE    0x19    // System state info
#define DW_REG_ACK_RESP     0x1A    // ACK/response time
#define DW_REG_RX_SNIFF     0x1D    // Pulsed preamble RX config
#define DW_REG_TX_POWER     0x1E    // TX power control
#define DW_REG_CHAN_CTRL    0x1F    // Channel control
#define DW_REG_USR_SFD      0x21    // User-specified SFD sequence
#define DW_REG_AGC_CTRL     0x23    // AGC parameters
#define DW_REG_EXT_SYNC     0x24    // External clock sync
#define DW_REG_ACC_MEM      0x25    // Accumulator CIR memory
#define DW_REG_GPIO_CTRL    0x26    // GPIO control
#define DW_REG_DRX_CONF     0x27    // Digital RX config
#define DW_REG_RF_CONF      0x28    // RF configuration
#define DW_REG_TX_CAL       0x2A    // TX calibration
#define DW_REG_FS_CTRL      0x2B    // Frequency synthesis control
#define DW_REG_AON          0x2C    // Always-on register set
#define DW_REG_OTP_IF       0x2D    // OTP memory interface
#define DW_REG_LDE_CTRL     0x2E    // LDE microcode control
#define DW_REG_DIG_DIAG     0x2F    // Digital diagnostics
#define DW_REG_PMSC         0x36    // Power management and system control

// =============================================================================
// Sub-register offsets
// =============================================================================

// PMSC (0x36)
#define DW_SUBREG_PMSC_CTRL0    0x00    // Clock control (FORCE_SYS_XTI etc.)
#define DW_SUBREG_PMSC_CTRL1    0x04    // LDE run enable, sleep control
#define DW_SUBREG_PMSC_SNOZT    0x0C    // Snooze time
#define DW_SUBREG_PMSC_TXFSEQ   0x26    // TX fine grain sequencing

// EXT_SYNC (0x24)
#define DW_SUBREG_EC_CTRL       0x00    // External clock control

// AGC_CTRL (0x23)
#define DW_SUBREG_AGC_CTRL1     0x02    // AGC control 1
#define DW_SUBREG_AGC_TUNE1     0x04    // AGC tune 1
#define DW_SUBREG_AGC_TUNE2     0x0C    // AGC tune 2
#define DW_SUBREG_AGC_TUNE3     0x12    // AGC tune 3

// DRX_CONF (0x27)
#define DW_SUBREG_DRX_TUNE0B    0x02    // SFD timeout at 110k
#define DW_SUBREG_DRX_TUNE1A    0x04    // PRF dependent
#define DW_SUBREG_DRX_TUNE1B    0x06    // DRX tune 1b
#define DW_SUBREG_DRX_TUNE2     0x08    // PAC dependent
#define DW_SUBREG_DRX_SFDTOC    0x20    // SFD detection timeout (symbols) 
#define DW_SUBREG_DRX_TUNE4H    0x26    // DRX tune 4H

// RF_CONF (0x28)
#define DW_SUBREG_RF_RXCTRLH    0x0B    // RX control H
#define DW_SUBREG_RF_TXCTRL     0x0C    // TX control

// TX_CAL (0x2A)
#define DW_SUBREG_TC_PGDELAY    0x0B    // Pulse generator delay

// FS_CTRL (0x2B)
#define DW_SUBREG_FS_PLLCFG     0x07    // PLL configuration
#define DW_SUBREG_FS_PLLTUNE    0x0B    // PLL tune
#define DW_SUBREG_FS_XTALT      0x0E    // Crystal trim

// AON (0x2C)
#define DW_SUBREG_AON_WCFG      0x00    // AON wake config
#define DW_SUBREG_AON_CTRL      0x02    // AON control
#define DW_SUBREG_AON_CFG0      0x06    // AON config 0
#define DW_SUBREG_AON_CFG1      0x0A    // AON config 1

// OTP_IF (0x2D)
#define DW_SUBREG_OTP_WDAT      0x00    // OTP write data
#define DW_SUBREG_OTP_ADDR      0x04    // OTP address to read
#define DW_SUBREG_OTP_CTRL      0x06    // OTP control
#define DW_SUBREG_OTP_RDAT      0x0A    // OTP read data output
#define DW_SUBREG_OTP_SF        0x08    // OTP special function

// LDE_CTRL (0x2E)
#define DW_SUBREG_LDE_CFG1      0x0806  // LDE config 1 (NTM, PMULT)
#define DW_SUBREG_LDE_CFG2      0x1806  // LDE config 2
#define DW_SUBREG_LDE_REPC      0x2804  // LDE replica coefficient (per preamble code)

// RX_TIME (0x15)
#define DW_SUBREG_RX_STAMP      0x00    // Adjusted RX timestamp (5 bytes)
#define DW_SUBREG_RX_FP_INDEX   0x05    // First path index (2 bytes)

// TX_TIME (0x17)
#define DW_SUBREG_TX_STAMP      0x00    // TX timestamp (5 bytes)

// =============================================================================
// OTP memory addresses (factory calibration data)
// =============================================================================

#define DW_OTP_ADDR_LDOTUNE     0x04    // LDO tune value
#define DW_OTP_ADDR_PARTID      0x06    // Part ID
#define DW_OTP_ADDR_LOTID       0x07    // Lot ID
#define DW_OTP_ADDR_XTRIM       0x1E    // XTAL trim

// =============================================================================
// OTP_IF control bits
// =============================================================================

#define DW_OTP_CTRL_OTPRDEN     0x01    // OTP read enable (manual mode, not self-clearing)
#define DW_OTP_CTRL_OTPREAD     0x02    // OTP read trigger (self-clearing)
#define DW_OTP_CTRL_LDELOAD     0x8000  // Load LDE microcode from ROM into RAM

#define DW_OTP_SF_LDO_KICK      0x02    // Kick LDO tune value from OTP

// =============================================================================
// EXT_SYNC control bits
// =============================================================================

#define DW_EC_CTRL_PLLLCK       0x04    // Enable CPLL lock detect

// =============================================================================
// FS_CTRL — crystal trim
// =============================================================================

#define DW_FS_XTALT_MASK        0x1F    // Crystal trim bits [4:0]
#define DW_FS_XTALT_RESERVED    0x60    // Bits [6:5] must always be 1 when writing
#define DW_FS_XTALT_MIDRANGE    0x10    // Mid-range trim (used if OTP not programmed)

// =============================================================================
// PMSC_CTRL0 clock control values (written to DW_SUBREG_PMSC_CTRL0)
// These are applied as read-modify-write on the low byte
// =============================================================================

#define DW_PMSC_CTRL0_SYS_XTI   0x01   // Force system clock to XTAL (safe for OTP read)
#define DW_PMSC_CTRL0_SYS_PLL   0x02   // Force system clock to PLL
#define DW_PMSC_CTRL0_LDE       0x01   // } Used together for FORCE_LDE:
#define DW_PMSC_CTRL1_LDE       0x03   // }   CTRL0[0]=1, CTRL1[1:0]=3

// PMSC_CTRL1 bits
#define DW_PMSC_CTRL1_LDERUN    (1UL << 9)  // Enable LDE algorithm — must be set
                                             // for accurate RX timestamps
// PMSC_CTRL0 soft reset values (written to byte 3, sub-address 0x03)
#define DW_PMSC_CTRL0_RESET_ALL     0x00    // Reset all
#define DW_PMSC_CTRL0_RESET_RX      0xE0    // Reset RX only
#define DW_PMSC_CTRL0_RESET_CLEAR   0xF0    // Clear reset

// =============================================================================
// SYS_STATUS register bits (0x0F)
// =============================================================================

#define SYS_STATUS_IRQS         (1UL << 0)  // Interrupt request status
#define SYS_STATUS_CPLOCK       (1UL << 1)  // Clock PLL lock
#define SYS_STATUS_ESYNCR       (1UL << 2)  // External sync clock reset
#define SYS_STATUS_AAT          (1UL << 3)  // Auto ACK trigger
#define SYS_STATUS_TXFRB        (1UL << 4)  // TX frame begins
#define SYS_STATUS_TXPRS        (1UL << 5)  // TX preamble sent
#define SYS_STATUS_TXPHS        (1UL << 6)  // TX PHR sent
#define SYS_STATUS_TXFRS        (1UL << 7)  // TX frame sent
#define SYS_STATUS_RXPRD        (1UL << 8)  // RX preamble detected
#define SYS_STATUS_RXSFDD       (1UL << 9)  // RX SFD detected
#define SYS_STATUS_LDEDONE      (1UL << 10) // LDE processing done
#define SYS_STATUS_RXPHD        (1UL << 11) // RX PHR detected
#define SYS_STATUS_RXPHE        (1UL << 12) // RX PHR error
#define SYS_STATUS_RXDFR        (1UL << 13) // RX data frame ready
#define SYS_STATUS_RXFCG        (1UL << 14) // RX FCS good
#define SYS_STATUS_RXFCE        (1UL << 15) // RX FCS error
#define SYS_STATUS_RXRFSL       (1UL << 16) // RX Reed Solomon sync loss
#define SYS_STATUS_RXRFTO       (1UL << 17) // RX frame wait timeout
#define SYS_STATUS_LDEERR       (1UL << 18) // LDE error
#define SYS_STATUS_RXOVRR       (1UL << 20) // RX buffer overrun
#define SYS_STATUS_RXPTO        (1UL << 21) // Preamble detect timeout
#define SYS_STATUS_GPIOIRQ      (1UL << 22) // GPIO interrupt
#define SYS_STATUS_SLP2INIT     (1UL << 23) // Sleep to init
#define SYS_STATUS_RFPLL_LL     (1UL << 24) // RF PLL lock loss
#define SYS_STATUS_CLKPLL_LL    (1UL << 25) // Clock PLL lock loss
#define SYS_STATUS_RXSFDTO      (1UL << 26) // RX SFD timeout
#define SYS_STATUS_HPDWARN      (1UL << 27) // Half period delay warning
#define SYS_STATUS_TXBERR       (1UL << 28) // TX buffer error
#define SYS_STATUS_AFFREJ       (1UL << 29) // Auto frame filter rejection

// Composite masks
#define SYS_STATUS_ALL_RX_GOOD  (SYS_STATUS_RXDFR | SYS_STATUS_RXFCG)
#define SYS_STATUS_ALL_RX_ERR   (SYS_STATUS_RXPHE | SYS_STATUS_RXFCE  | \
                                 SYS_STATUS_RXRFSL | SYS_STATUS_RXSFDTO | \
                                 SYS_STATUS_LDEERR | SYS_STATUS_RXRFTO | \
                                 SYS_STATUS_SLP2INIT)
#define SYS_STATUS_ALL_TX       (SYS_STATUS_TXFRS)

// =============================================================================
// SYS_CTRL register bits (0x0D)
// =============================================================================

#define SYS_CTRL_SFCST          (1UL << 0)  // Suppress auto FCS transmission
#define SYS_CTRL_TXSTRT         (1UL << 1)  // Start TX immediately
#define SYS_CTRL_TXDLYS         (1UL << 2)  // Start TX delayed
#define SYS_CTRL_CANSFCS        (1UL << 3)  // Cancel suppression of auto FCS
#define SYS_CTRL_TRXOFF         (1UL << 6)  // Force transceiver off
#define SYS_CTRL_WAIT4RESP      (1UL << 7)  // Enable RX automatically after TX
#define SYS_CTRL_RXENAB         (1UL << 8)  // Enable RX now
#define SYS_CTRL_RXDLYE         (1UL << 9)  // Enable delayed RX

// TX mode flags - passed to dw1000_start_tx()
#define DW_TX_IMMEDIATE         0x00        // send immediately
#define DW_TX_DELAYED           0x01        // send at time set by dw1000_set_delayed_tx_time()
#define DW_TX_WAIT4ESP          0x02        // auto-enable RX after TX (initiator role)

// =============================================================================
// TX_FCTRL register bits (0x08)
// =============================================================================

#define TX_FCTRL_TFLEN_MASK     0x3FFUL         // bits [9:0]: frame length
#define TX_FCTRL_TFLE_MASK      (0x7UL << 10)   // bits [12:10]: frame length ext
#define TX_FCTRL_TXBR_MASK      (0x3UL << 13)   // bits [14:13]: bit rate
#define TX_FCTRL_TR             (1UL << 15)      // Ranging bit
#define TX_FCTRL_TXPRF_MASK     (0x3UL << 16)   // bits [17:16]: PRF
#define TX_FCTRL_TXPSR_MASK     (0x3UL << 18)   // bits [19:18]: preamble symbol reps
#define TX_FCTRL_PE_MASK        (0x3UL << 20)   // bits [21:20]: preamble ext
#define TX_FCTRL_TXBOFFS_MASK   (0x3FFUL << 22) // bits [31:22]: TX buffer offset

#define TX_FCTRL_TXBR_110K      (0x0UL << 13)
#define TX_FCTRL_TXBR_850K      (0x1UL << 13)
#define TX_FCTRL_TXBR_6800K     (0x2UL << 13)

// =============================================================================
// SYS_CFG register bits (0x04)
// =============================================================================

#define SYS_CFG_FFE             (1UL << 0)  // Frame filter enable
#define SYS_CFG_FFBC            (1UL << 1)  // Accept beacon frames
#define SYS_CFG_FFBD            (1UL << 2)  // Accept data frames
#define SYS_CFG_FFAB            (1UL << 3)  // Accept ACK frames
#define SYS_CFG_FFAD            (1UL << 4)  // Accept MAC command frames
#define SYS_CFG_FFAA            (1UL << 5)  // Accept all address frames
#define SYS_CFG_FFAM            (1UL << 6)  // Accept multi-cast frames
#define SYS_CFG_FFAR            (1UL << 7)  // Accept reserved frames
#define SYS_CFG_FFA4            (1UL << 8)  // Accept type 4 frames
#define SYS_CFG_FFA5            (1UL << 9)  // Accept type 5 frames
#define SYS_CFG_HIRQ_POL        (1UL << 10) // IRQ polarity (1=active high)
#define SYS_CFG_SPI_EDGE        (1UL << 11) // SPI data edge
#define SYS_CFG_DIS_FCE         (1UL << 12) // Disable frame check error handling
#define SYS_CFG_DIS_DRXB        (1UL << 13) // Disable double RX buffer
#define SYS_CFG_DIS_PHE         (1UL << 14) // Disable PHR error handling
#define SYS_CFG_DIS_RSDE        (1UL << 15) // Disable Reed Solomon error handling
#define SYS_CFG_FCS_INIT2F      (1UL << 16) // FCS seed
#define SYS_CFG_PHR_MODE_MASK   (0x3UL << 17) // PHR mode bits
#define SYS_CFG_DIS_STXP        (1UL << 18) // Disable smart TX power
#define SYS_CFG_RXM110K         (1UL << 22) // RX at 110 kbps (long preamble)
#define SYS_CFG_RXWTOE          (1UL << 28) // RX wait timeout enable
#define SYS_CFG_RXAUTR          (1UL << 29) // RX auto re-enable after timeout
#define SYS_CFG_AUTOACK         (1UL << 30) // Auto ACK enable
#define SYS_CFG_AACKPEND        (1UL << 31) // Auto ACK pending bit

// =============================================================================
// CHAN_CTRL register bits (0x1F)
// =============================================================================

#define CHAN_CTRL_TX_CHAN_MASK  (0xFUL << 0)    // TX channel bits [3:0]
#define CHAN_CTRL_RX_CHAN_MASK  (0xFUL << 4)    // RX channel bits [7:4]
#define CHAN_CTRL_DWSFD         (1UL << 17)     // Use Decawave SFD
#define CHAN_CTRL_RXPRF_MASK    (0x3UL << 18)   // RX PRF bits [19:18]
#define CHAN_CTRL_TNSSFD        (1UL << 20)     // Non-standard SFD for TX
#define CHAN_CTRL_RNSSFD        (1UL << 21)     // Non-standard SFD for RX
#define CHAN_CTRL_TX_PCOD_MASK  (0x1FUL << 22)  // TX preamble code [26:22]
#define CHAN_CTRL_RX_PCOD_MASK  (0x1FUL << 27)  // RX preamble code [31:27]

// =============================================================================
// Expected device ID and return codes
// =============================================================================

#define DW1000_DEV_ID           0xDECA0130UL

#define DW_SUCCESS   0
#define DW_ERROR    -1

#endif // DW1000_REGS_H
