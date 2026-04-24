#ifndef DW1000_CONFIG_H
#define DW1000_CONFIG_H

#include <stdint.h>

// =============================================================================
// DW1000 radio configuration
// Fixed for DWM1001: channel 5, PRF 64MHz, preamble 128, 6.8Mbps, code 9
// =============================================================================

// Channel
#define DW_CHANNEL          5

// Pulse Repetition Frequency
#define DW_PRF_16M          1
#define DW_PRF_64M          2

// Data rate
#define DW_BR_110K          0
#define DW_BR_850K          1
#define DW_BR_6M8           2

// Preamble
#define DW_PLEN_64          0x04
#define DW_PLEN_128         0x14
#define DW_PLEN_256         0x24
#define DW_PLEN_512         0x34
#define DW_PLEN_1024        0x08
#define DW_PLEN_2048        0x18
#define DW_PLEN_4096        0x0C

// PAC size (Preamble Acquisition Chunk)
// Rule of thumb: PAC = preamble_length / 8, max 64
#define DW_PAC8             0
#define DW_PAC16            1
#define DW_PAC32            2
#define DW_PAC64            3

// PHR mode
#define DW_PHRMODE_STD      0x0     // Standard frame (max 127 bytes)
#define DW_PHRMODE_EXT      0x3     // Extended frame (max 1023 bytes)

// =============================================================================
// Configuration structure
// =============================================================================
typedef struct {
    uint8_t  chan;           // Channel: 1, 2, 3, 4, 5, 7
    uint8_t  prf;            // PRF: DW_PRF_16M or DW_PRF_64M
    uint8_t  txPreambLength; // Preamble length: DW_PLEN_*
    uint8_t  rxPAC;          // PAC size: DW_PAC*
    uint8_t  txCode;         // TX preamble code (1-8 for 16M, 9-24 for 64M)
    uint8_t  rxCode;         // RX preamble code (same as txCode normally)
    uint8_t  nsSFD;          // 0 = standard SFD, 1 = Decawave non-standard
    uint8_t  dataRate;       // DW_BR_110K / DW_BR_850K / DW_BR_6M8
    uint8_t  phrMode;        // DW_PHRMODE_STD or DW_PHRMODE_EXT
    uint16_t sfdTO;          // SFD timeout in symbols (0 = use default)
} dw1000_config_t;

// =============================================================================
// Default config for DWM1001
// Channel 5, PRF 64MHz, preamble 128, PAC8, code 9, 6.8Mbps
// This matches Decawave SS-TWR example defaults
// =============================================================================

#define DW1000_DEFAULT_CONFIG  {  \
    .chan          = 5,           \
    .prf           = DW_PRF_64M, \
    .txPreambLength= DW_PLEN_128,\
    .rxPAC         = DW_PAC8,    \
    .txCode        = 10,          \
    .rxCode        = 10,          \
    .nsSFD         = 0,          \
    .dataRate      = DW_BR_6M8,  \
    .phrMode       = DW_PHRMODE_STD, \
    .sfdTO         = 137 \
}

// sfdTO = preamble_length + SFD_length + 1
// for preamble 128, standard SFD (8 symbols): 128 + 8 + 1 = 137

// =============================================================================
// API
// =============================================================================

void dw1000_configure(const dw1000_config_t *cfg);

#endif // DW1000_CONFIG_H
