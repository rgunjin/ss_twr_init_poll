#ifndef DW1000_H
#define DW1000_H

#include <stdint.h>
#include "dw1000_regs.h"

// =============================================================================
// Low-level register access
//
// These are the building blocks. dw1000.c uses them internally, but you can
// call them from main.c if you need to poke a register directly
// =============================================================================

// Read len bytes from register at addr (no sub-address)
// Example: dw1000_read_reg(DW_REG_DEV_ID, buf, 4)
void dw1000_read_reg(uint8_t addr, uint8_t *buf, uint8_t len);

// Write len bytes to register at addr (no sub-address)
void dw1000_write_reg(uint8_t addr, uint8_t *buf, uint8_t len);

// Read len bytes from register addr at sub-address offset
// Example: dw1000_read_subreg(DW_REG_LDE_CTRL, DW_SUBREG_LDE_CFG1, buf, 2)
void dw1000_read_subreg(uint8_t addr, uint16_t sub, uint8_t *buf, uint8_t len);

// Write len bytes to register addr at sub-address offset
void dw1000_write_subreg(uint8_t addr, uint16_t sub, uint8_t *buf, uint8_t len);

// Convenience wrappers for 32-bit registers (little-endian, no sub-address)
uint32_t dw1000_read32(uint8_t addr);
void     dw1000_write32(uint8_t addr, uint32_t value);

// =============================================================================
// High-level API
// =============================================================================

// Initialize DW1000: soft reset, verify DEV_ID.
// Returns DW_SUCCESS or DW_ERROR
int dw1000_init(void);

// Read and return DEV_ID. Expected: DW1000_DEV_ID (0xDECA0130)
uint32_t dw1000_read_dev_id(void);

// Write TX frame payload into TX_BUFFER at byte offset.
// offset is normally 0.
// len does NOT include the 2-byte CRC — DW1000 appends it automatically
void dw1000_write_tx_data(uint8_t *data, uint8_t len, uint8_t offset);

// Set TX frame control register (read-modify-write to preserve rate/PRF bits)
// len     — payload length in bytes (CRC will be added by hardware, +2)
// offset  — TX buffer offset, normally 0
// ranging — 1 to set the ranging bit (TR), 0 otherwise
void dw1000_write_tx_fctrl(uint8_t len, uint8_t offset, uint8_t ranging);

// Start transmission
// wait4resp — 1: automatically enable RX after TX (initiator role)
//             0: TX only, no auto-RX (responder sending final reply)
void dw1000_start_tx(uint8_t wait4resp);

// Enable receiver manually (use when not relying on wait4resp)
void dw1000_rx_enable(void);

// Read received frame payload into buf (len bytes from RX_BUFFER offset 0)
void dw1000_read_rx_data(uint8_t *buf, uint8_t len);

// Read lower 32 bits of TX timestamp
// Sufficient for SS-TWR — 32-bit range covers ~17 seconds, far longer than
// any ranging exchange
uint32_t dw1000_read_tx_timestamp(void);

// Read lower 32 bits of RX timestamp
uint32_t dw1000_read_rx_timestamp(void);

// Read full SYS_STATUS register
uint32_t dw1000_read_sys_status(void);

// Write to SYS_STATUS to clear flags (write-1-to-clear).
// Example: dw1000_clear_sys_status(SYS_STATUS_TXFRS | SYS_STATUS_RXFCG)
void dw1000_clear_sys_status(uint32_t mask);

// Read RX_FINFO register — bits [6:0] contain received frame length
uint32_t dw1000_read_rx_finfo(void);

// Force transceiver off. Call before re-enabling RX after an error
void dw1000_trxoff(void);

#endif // DW1000_H
