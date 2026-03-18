#ifndef NRF52_PERIPHERALS_H
#define NRF52_PERIPHERALS_H

#include <stdint.h>

// =============================================================================
// CLOCK
// =============================================================================
typedef struct {                                        /*!< CLOCK Structure                                                       */
  volatile uint32_t  TASKS_HFCLKSTART;                  /*!< Start HFCLK crystal oscillator                                        */
  volatile uint32_t  TASKS_HFCLKSTOP;                   /*!< Stop HFCLK crystal oscillator                                         */
  volatile uint32_t  TASKS_LFCLKSTART;                  /*!< Start LFCLK source                                                    */
  volatile uint32_t  TASKS_LFCLKSTOP;                   /*!< Stop LFCLK source                                                     */
  volatile uint32_t  TASKS_CAL;                         /*!< Start calibration of LFRC oscillator                                  */
  volatile uint32_t  TASKS_CTSTART;                     /*!< Start calibration timer                                               */
  volatile uint32_t  TASKS_CTSTOP;                      /*!< Stop calibration timer                                                */
  volatile uint32_t  RESERVED0[57];
  volatile uint32_t  EVENTS_HFCLKSTARTED;               /*!< HFCLK oscillator started                                              */
  volatile uint32_t  EVENTS_LFCLKSTARTED;               /*!< LFCLK started                                                         */
  volatile uint32_t  RESERVED1;
  volatile uint32_t  EVENTS_DONE;                       /*!< Calibration of LFCLK RC oscillator complete event                     */
  volatile uint32_t  EVENTS_CTTO;                       /*!< Calibration timer timeout                                             */
  volatile uint32_t  RESERVED2[124];
  volatile uint32_t  INTENSET;                          /*!< Enable interrupt                                                      */
  volatile uint32_t  INTENCLR;                          /*!< Disable interrupt                                                     */
  volatile uint32_t  RESERVED3[63];
  volatile uint32_t  HFCLKRUN;                          /*!< Status indicating that HFCLKSTART task has been triggered             */
  volatile uint32_t  HFCLKSTAT;                         /*!< HFCLK status                                                          */
  volatile uint32_t  RESERVED4;
  volatile uint32_t  LFCLKRUN;                          /*!< Status indicating that LFCLKSTART task has been triggered             */
  volatile uint32_t  LFCLKSTAT;                         /*!< LFCLK status                                                          */
  volatile uint32_t  LFCLKSRCCOPY;                      /*!< Copy of LFCLKSRC register, set when LFCLKSTART task was triggered     */
  volatile uint32_t  RESERVED5[62];
  volatile uint32_t  LFCLKSRC;                          /*!< Clock source for the LFCLK                                            */
  volatile uint32_t  RESERVED6[7];
  volatile uint32_t  CTIV;                              /*!< Calibration timer interval                                            */
  volatile uint32_t  RESERVED7[8];
  volatile uint32_t  TRACECONFIG;                       /*!< Clocking options for the Trace Port debug interface                   */
} NRF_CLOCK_Type;

// =============================================================================
// SPIM
// =============================================================================
typedef struct {
  volatile uint32_t  SCK;                               /*!< Pin select for SCK                                                    */
  volatile uint32_t  MOSI;                              /*!< Pin select for MOSI signal                                            */
  volatile uint32_t  MISO;                              /*!< Pin select for MISO signal                                            */
} SPIM_PSEL_Type;

typedef struct {
  volatile uint32_t  PTR;                               /*!< Data pointer                                                          */
  volatile uint32_t  MAXCNT;                            /*!< Maximum number of bytes in receive buffer                             */
  volatile uint32_t  AMOUNT;                            /*!< Number of bytes transferred in the last transaction                   */
  volatile uint32_t  LIST;                              /*!< EasyDMA list type                                                     */
} SPIM_RXD_Type;

typedef struct {
  volatile uint32_t  PTR;                               /*!< Data pointer                                                          */
  volatile uint32_t  MAXCNT;                            /*!< Maximum number of bytes in transmit buffer                            */
  volatile uint32_t  AMOUNT;                            /*!< Number of bytes transferred in the last transaction                   */
  volatile uint32_t  LIST;                              /*!< EasyDMA list type                                                     */
} SPIM_TXD_Type;

typedef struct {                                        /*!< SPIM Structure                                                        */
  volatile uint32_t  RESERVED0[4];
  volatile uint32_t  TASKS_START;                       /*!< Start SPI transaction                                                 */
  volatile uint32_t  TASKS_STOP;                        /*!< Stop SPI transaction                                                  */
  volatile uint32_t  RESERVED1;
  volatile uint32_t  TASKS_SUSPEND;                     /*!< Suspend SPI transaction                                               */
  volatile uint32_t  TASKS_RESUME;                      /*!< Resume SPI transaction                                                */
  volatile uint32_t  RESERVED2[56];
  volatile uint32_t  EVENTS_STOPPED;                    /*!< SPI transaction has stopped                                           */
  volatile uint32_t  RESERVED3[2];
  volatile uint32_t  EVENTS_ENDRX;                      /*!< End of RXD buffer reached                                             */
  volatile uint32_t  RESERVED4;
  volatile uint32_t  EVENTS_END;                        /*!< End of RXD buffer and TXD buffer reached                              */
  volatile uint32_t  RESERVED5;
  volatile uint32_t  EVENTS_ENDTX;                      /*!< End of TXD buffer reached                                             */
  volatile uint32_t  RESERVED6[10];
  volatile uint32_t  EVENTS_STARTED;                    /*!< Transaction started                                                   */
  volatile uint32_t  RESERVED7[44];
  volatile uint32_t  SHORTS;                            /*!< Shortcut register                                                     */
  volatile uint32_t  RESERVED8[64];
  volatile uint32_t  INTENSET;                          /*!< Enable interrupt                                                      */
  volatile uint32_t  INTENCLR;                          /*!< Disable interrupt                                                     */
  volatile uint32_t  RESERVED9[125];
  volatile uint32_t  ENABLE;                            /*!< Enable SPIM                                                           */
  volatile uint32_t  RESERVED10;
  SPIM_PSEL_Type PSEL;                                  /*!< Unspecified                                                           */
  volatile uint32_t  RESERVED11[4];
  volatile uint32_t  FREQUENCY;                         /*!< SPI frequency. Accuracy depends on the HFCLK source selected.         */
  volatile  uint32_t  RESERVED12[3];
  SPIM_RXD_Type RXD;                                    /*!< RXD EasyDMA channel                                                   */
  SPIM_TXD_Type TXD;                                    /*!< TXD EasyDMA channel                                                   */
  volatile uint32_t  CONFIG;                            /*!< Configuration register                                                */
  volatile uint32_t  RESERVED13[26];
  volatile uint32_t  ORC;                               /*!< Over-read character. Character clocked out in case and over-read
                                                         of the TXD buffer.                                                    */
} NRF_SPIM_Type;

// SPIM frequency values (nRF52832 datasheet, section SPIM)
typedef enum {
    SPIM_FREQ_125K = 0x02000000UL,
    SPIM_FREQ_250K = 0x04000000UL,
    SPIM_FREQ_500K = 0x08000000UL,
    SPIM_FREQ_1M   = 0x10000000UL,
    SPIM_FREQ_2M   = 0x20000000UL,   // safe starting point for DW1000
    SPIM_FREQ_4M   = 0x40000000UL,
    SPIM_FREQ_8M   = 0x80000000UL,   // maximum for DW1000
} SPIM_Frequency_t;
 
// =============================================================================
// GPIO
// =============================================================================
typedef struct {                                        /*!< GPIO Structure                                                        */
  volatile uint32_t  RESERVED0[321];
  volatile uint32_t  OUT;                               /*!< Write GPIO port                                                       */
  volatile uint32_t  OUTSET;                            /*!< Set individual bits in GPIO port                                      */
  volatile uint32_t  OUTCLR;                            /*!< Clear individual bits in GPIO port                                    */
  volatile uint32_t  IN;                                /*!< Read GPIO port                                                        */
  volatile uint32_t  DIR;                               /*!< Direction of GPIO pins                                                */
  volatile uint32_t  DIRSET;                            /*!< DIR set register                                                      */
  volatile uint32_t  DIRCLR;                            /*!< DIR clear register                                                    */
  volatile uint32_t  LATCH;                             /*!< Latch register indicating what GPIO pins that have met the criteria
                                                         set in the PIN_CNF[n].SENSE registers                                 */
  volatile uint32_t  DETECTMODE;                        /*!< Select between default DETECT signal behaviour and LDETECT mode       */
  volatile uint32_t  RESERVED1[118];
  volatile uint32_t  PIN_CNF[32];                       /*!< Description collection[0]: Configuration of GPIO pins                 */
} NRF_GPIO_Type;

#endif
