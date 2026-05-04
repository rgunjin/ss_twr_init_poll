#include <inttypes.h>
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

#define ANT_DLY                         16456   // Default antenna delay for DW1001
#define POLL_RX_TO_RESP_TX_DLY_UUS      5000    // 1100 микросекунд задержка между приёмом poll и отправкой response
#define UUS_TO_DWT_TIME                 65536   // 1 uus = 512/499.2 секунды, 1 секунда = 499.2*128 dtu, итого 65536


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

    // Debug
    uint32_t chan_ctrl = dw1000_read32(DW_REG_CHAN_CTRL);
    SEGGER_RTT_printf(0, "[DBG] CHAN_CTRL=0x%08X\n", chan_ctrl);
    // TX code = биты [26:22], RX code = биты [31:27]
    SEGGER_RTT_printf(0, "[DBG] TX_CODE=%d RX_CODE=%d\n",
                                    (chan_ctrl >> 22) & 0x1F,
                                    (chan_ctrl >> 27) & 0x1F);

    dw1000_set_antenna_delay(ANT_DLY, ANT_DLY);

    SEGGER_RTT_printf(0, "[INIT] OK - waiting for poll\n");
    SEGGER_RTT_printf(0, "[DBG] SYS_CFG=0x%08X\n", 
    dw1000_read32(DW_REG_SYS_CFG));
    led_off(31);

    // =========================================================
    // МИНИМАЛЬНЫЙ RX ТЕСТ
    // =========================================================
    dw1000_clear_sys_status(0xFFFFFFFF);        // Чистим статус

    // RX_FWTO (0x0C) — frame wait timeout в ~1.026 мкс единицах
    // 65000 = ~66ms, такое же значение как у Decawave
    uint16_t fwto = 65000;
    dw1000_write_subreg(DW_REG_RX_FWTO, 0x00, (uint8_t*)&fwto, 2);

    // Включить timeout в SYS_CFG
    uint32_t sys_cfg = dw1000_read32(DW_REG_SYS_CFG);
    sys_cfg |= SYS_CFG_RXWTOE;
    dw1000_write32(DW_REG_SYS_CFG, sys_cfg);

    //Debug
    SEGGER_RTT_printf(0, "[DBG] SYS_CFG after RXWTOE=0x%08X\n", 
    dw1000_read32(DW_REG_SYS_CFG));

    dw1000_rx_enable();

    SEGGER_RTT_printf(0, "[RX] receiver enabled\n");
    SEGGER_RTT_printf(0, "[DBG] SYS_STATE=0x%08X\n",
                      dw1000_read32(DW_REG_SYS_STATE));

    while (1) {
        static uint32_t loop_counter = 0;
        static uint32_t err_counter = 0;
        loop_counter++;
        
        uint32_t status = dw1000_read_sys_status();
        if (status & SYS_STATUS_RXFCG) {
            dw1000_clear_sys_status(SYS_STATUS_RXFCG);

            // Читаем длину и данные фрейма
            uint32_t frame_len = dw1000_read32(DW_REG_RX_FINFO) & 0x3FF;
            if (frame_len <= RX_BUF_LEN)
                dw1000_read_reg(DW_REG_RX_BUFFER, rx_buffer, frame_len);

            // Валидация: сбрасываем seq number и сравниваем с эталоном
            rx_buffer[ALL_MSG_SN_IDX] = 0;
            if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) != 0) {
                SEGGER_RTT_printf(0, "[RX] unknown frame, ignoring\n");
                dw1000_rx_reset();
                dw1000_rx_enable();
                // цикл дальше
            } else {
                SEGGER_RTT_printf(0, "[RX] poll received, sending response\n");

                // T2: читаем RX timestamp (40 бит) — момент когда пришёл poll
                uint8_t ts_buf[5];
                dw1000_read_reg(DW_REG_RX_TIME, ts_buf, 5);
                uint64_t poll_rx_ts = 0;
                for (int i = 4; i >= 0; i--) {
                    poll_rx_ts <<= 8;
                    poll_rx_ts |= ts_buf[i];
                }
                SEGGER_RTT_printf(0, "[TWR] T2 poll_rx_ts=0x%08X%08X\n",
                                  (uint32_t)(poll_rx_ts >> 32), (uint32_t)poll_rx_ts);

                // Debug: Check sys_time
                uint8_t sys_time_before[5];
                dw1000_read_reg(DW_REG_SYS_TIME, sys_time_before, 5);
                uint32_t t_before = sys_time_before[1] | (sys_time_before[2]<<8) | (sys_time_before[3]<<16) | (sys_time_before[4]<<24);

                // Вычисляем момент отправки ответа: T2 + 1100 uus
                // >> 8 потому что DX_TIME хранит время со сдвигом 8 бит
                uint64_t resp_tx_time_64 =
                    (poll_rx_ts + ((uint64_t)POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;

                // T3: восстанавливаем полный timestamp из resp_tx_time + antenna delay
                // Это значение которое initiator получит и использует для расчёта дистанции
                uint64_t resp_tx_ts = ((resp_tx_time_64 & ~1ULL) << 8) + ANT_DLY;
                SEGGER_RTT_printf(0, "[TWR] T3 resp_tx_ts=0x%08X%08X\n",
                                  (uint32_t)(resp_tx_ts >> 32), (uint32_t)resp_tx_ts);

                // Заполняем ответный фрейм
                tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb++;
                msg_set_ts(&tx_resp_msg[RESP_MSG_POLL_RX_TS_IDX], (uint32_t)poll_rx_ts);
                msg_set_ts(&tx_resp_msg[RESP_MSG_RESP_TX_TS_IDX], (uint32_t)resp_tx_ts);

                // Пишем delayed TX time в DX_TIME регистр
                uint8_t dx[5] = {0};
                dx[0] = (resp_tx_time_64)       & 0xFF;
                dx[1] = (resp_tx_time_64 >>  8) & 0xFF;
                dx[2] = (resp_tx_time_64 >> 16) & 0xFF;
                dx[3] = (resp_tx_time_64 >> 24) & 0xFF;
                dx[4] = (resp_tx_time_64 >> 32) & 0xFF;
                dw1000_write_reg(DW_REG_DX_TIME, dx, 5);

                // Пишем тело фрейма в TX буфер
                dw1000_write_reg(DW_REG_TX_BUFFER, tx_resp_msg, sizeof(tx_resp_msg));

                // TX_FCTRL: длина фрейма + ranging bit (TR)
                uint32_t fctrl = dw1000_read32(DW_REG_TX_FCTRL);
                fctrl &= ~0x3FF;                  // сбрасываем длину
                fctrl |= sizeof(tx_resp_msg);     // новая длина
                fctrl |= (1 << 15);              // TR bit = это ranging фрейм
                dw1000_write32(DW_REG_TX_FCTRL, fctrl);

                uint8_t sys_time_after[5];
                dw1000_read_reg(DW_REG_SYS_TIME, sys_time_after, 5);
                uint32_t t_after = sys_time_after[1] | (sys_time_after[2]<<8) | (sys_time_after[3]<<16) | (sys_time_after[4]<<24);

                uint32_t elapsed_dtu = t_after - t_before;
                uint32_t elapsed_uus = elapsed_dtu / 65536;
                SEGGER_RTT_printf(0, "[TIME] prep took ~%d uus\n", elapsed_uus);

                uint8_t cur_time[5];
                dw1000_read_reg(DW_REG_SYS_TIME, cur_time, 5);
                uint64_t now = 0;
                for (int i = 4; i >= 0; i--) { now <<= 8; now |= cur_time[i]; }
                SEGGER_RTT_printf(0, "[TIME] now_dtu=0x%08X%08X scheduled_dtu=0x%08X%08X\n",
                                    (uint32_t)(now >> 32), (uint32_t)now,
                                    (uint32_t)(resp_tx_time_64 >> 24), (uint32_t)(resp_tx_time_64 << 8));

                // Запускаем delayed TX
                uint8_t ctrl = SYS_CTRL_TXSTRT;
                dw1000_write_subreg(DW_REG_SYS_CTRL, 0x00, &ctrl, 1);

                // Ждём подтверждения отправки TXFRS
                int timeout = 100000;
                while (!(dw1000_read_sys_status() & SYS_STATUS_TXFRS) && --timeout) {}

                if (timeout == 0) {
                    SEGGER_RTT_printf(0, "[TX] delayed TX failed! status=0x%08X\n",
                    dw1000_read_sys_status());
                    dw1000_trxoff();
                } else {
                    SEGGER_RTT_printf(0, "[TX] response sent seq=%d\n", frame_seq_nb - 1);
                }

                dw1000_clear_sys_status(SYS_STATUS_TXFRS);
                dw1000_rx_reset();
                dw1000_rx_enable();
            }

        } else if (status & SYS_STATUS_ALL_RX_TO) {
            err_counter++;
            if (err_counter % 10 == 0)
                SEGGER_RTT_printf(0, "[RX] %d errors, %d loops\n", err_counter, loop_counter);
            dw1000_trxoff();
            dw1000_rx_reset();
            dw1000_clear_sys_status(SYS_STATUS_ALL_RX_TO);
            dw1000_rx_enable();

        } else if (status & SYS_STATUS_ALL_RX_ERR) {
            SEGGER_RTT_printf(0, "[RX] error status=0x%08X\n", status);
            if (status & SYS_STATUS_RXPHE)   SEGGER_RTT_printf(0, "  -> RXPHE\n");
            if (status & SYS_STATUS_RXFCE)   SEGGER_RTT_printf(0, "  -> RXFCE\n");
            if (status & SYS_STATUS_RXRFSL)  SEGGER_RTT_printf(0, "  -> RXRFSL\n");
            if (status & SYS_STATUS_RXSFDTO) SEGGER_RTT_printf(0, "  -> RXSFDTO\n");
            if (status & SYS_STATUS_LDEERR)  SEGGER_RTT_printf(0, "  -> LDEERR\n");
            dw1000_trxoff();
            dw1000_rx_reset();
            dw1000_clear_sys_status(SYS_STATUS_ALL_RX_ERR);
            dw1000_rx_enable();
        }
    }
}

// NOTE: In this simple implementation T3 is sent as 0 in the response.
// The initiator will compute an incorrect distance. To fix this properly,
// the responder needs to know T3 before sending — this requires either:
// a) delayed TX: schedule the response at a known future time, so T3 = TX_time + known_delay
