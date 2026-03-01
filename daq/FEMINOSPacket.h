#pragma once

#include <deque>
#include <netinet/in.h>
#include <stdint.h>
#include <vector>

#include "SignalEvent.h"

//
// Prefix Codes for 14-bit data content
//
#define PFX_14_BIT_CONTENT_MASK 0xC000   // Mask to select 2 MSB's of prefix
#define PFX_CARD_CHIP_CHAN_HIT_IX 0xC000 // Index of Card, Chip and Channel Hit
#define PFX_CARD_CHIP_CHAN_HIT_CNT                                             \
  0x8000 // Nb of Channel hit for given Card and Chip
#define PFX_CARD_CHIP_CHAN_HISTO                                               \
  0x4000 // Pedestal Histogram for given Card and Chip

#define PUT_CARD_CHIP_CHAN_HISTO(ca, as, ch)                                   \
  (PFX_CARD_CHIP_CHAN_HISTO | (((ca)&0x1F) << 9) | (((as)&0x3) << 7) |         \
   (((ch)&0x7F) << 0))
//
// Prefix Codes for 12-bit data content
//
#define PFX_12_BIT_CONTENT_MASK 0xF000 // Mask to select 4 MSB's of prefix
#define PFX_ADC_SAMPLE 0x3000          // ADC sample
#define PFX_LAT_HISTO_BIN 0x2000 // latency or inter event time histogram bin
#define PFX_CHIP_LAST_CELL_READ 0x1000 // Chip index and last cell read

//
// Prefix Codes for 9-bit data content
//
#define PFX_9_BIT_CONTENT_MASK 0xFE00 // Mask to select 7 MSB's of prefix
#define PFX_TIME_BIN_IX 0x0E00        // Time-bin Index
#define PFX_HISTO_BIN_IX 0x0C00       // Histogram bin Index
#define PFX_PEDTHR_LIST 0x0A00        // List of pedestal or thresholds
#define PFX_START_OF_DFRAME                                                    \
  0x0800 // Start of Data Frame + 5 bit source + 4 bit Version
#define PFX_START_OF_MFRAME                                                    \
  0x0600 // Start of Monitoring Frame + 4 bit Version + 5 bit source
#define PFX_START_OF_CFRAME                                                    \
  0x0400 // Start of Configuration Frame + 4 bit Version + 5 bit source
// "0000001" : available for future use

#define PUT_HISTO_BIN_IX(bi) (PFX_HISTO_BIN_IX | ((bi)&0x1FF))
#define PUT_PEDTHR_LIST(f, a, m, t)                                            \
  (PFX_PEDTHR_LIST | (((f)&0x1F) << 4) | (((a)&0x3) << 2) | (((m)&0x1) << 1) | \
   (((t)&0x1) << 0))

//
// Prefix Codes for 8-bit data content
//
#define PFX_8_BIT_CONTENT_MASK 0xFF00 // Mask to select 8 MSB's of prefix
#define PFX_ASCII_MSG_LEN 0x0100      // ASCII message length

//
// Prefix Codes for 4-bit data content
//
#define PFX_4_BIT_CONTENT_MASK 0xFFF0 // Mask to select 12 MSB's of prefix
#define PFX_START_OF_EVENT                                                     \
  0x00F0 // Start of Event + 1 bit free + Event Trigger Type
#define PFX_END_OF_EVENT 0x00E0 // End of Event + 4 MSB of size
// #define 0x00D0 // available for future use
// #define 0x00C0 // available for future use
// #define 0x00B0 // available for future use
//  "000000001010" : available for future use
//  "000000001001" : available for future use
//  "000000001000" : available for future use

//
// Prefix Codes for 2-bit data content
//
#define PFX_2_BIT_CONTENT_MASK 0xFFFC // Mask to select 14 MSB's of prefix
#define PFX_CH_HIT_CNT_HISTO 0x007C   // Channel Hit Count Histogram
// "00000000011110" : available for future use
// "00000000011100" : available for future use
// "00000000011011" : available for future use
// "00000000011010" : available for future use
// "00000000011001" : available for future use
// "00000000011000" : available for future use

//
// Prefix Codes for 1-bit data content
//
#define PFX_1_BIT_CONTENT_MASK 0xFFFE // Mask to select 15 MSB's of prefix
// "000000000011111" : available for future use
// "000000000011110" : available for future use
// "000000000011101" : available for future use
// "000000000011100" : available for future use
// "000000000001111" : available for future use
// "000000000001110" : available for future use
// "000000000001101" : available for future use
// "000000000001100" : available for future use
// "000000000001011" : available for future use
// "000000000001010" : available for future use
// "000000000001001" : available for future use
// "000000000001000" : available for future use

//
// Prefix Codes for 0-bit data content
//
#define PFX_0_BIT_CONTENT_MASK 0xFFFF   // Mask to select 16 MSB's of prefix
#define PFX_END_OF_FRAME 0x000F         // End of Frame (any type)
#define PFX_DEADTIME_HSTAT_BINS 0x000E  // Deadtime statistics and histogram
#define PFX_PEDESTAL_HSTAT 0x000D       // Pedestal histogram statistics
#define PFX_PEDESTAL_H_MD 0x000C        // Pedestal histogram Mean and Deviation
#define PFX_SHISTO_BINS 0x000B          // Hit S-curve histogram
#define PFX_CMD_STATISTICS 0x000A       // Command server statistics
#define PFX_START_OF_BUILT_EVENT 0x0009 // Start of built event
#define PFX_END_OF_BUILT_EVENT 0x0008   // End of built event
#define PFX_EVPERIOD_HSTAT_BINS                                                \
  0x0007                     // Inter Event Time statistics and histogram
#define PFX_SOBE_SIZE 0x0006 // Start of built event + Size
// "0000000000000101" : available for future use
// "0000000000000100" : available for future use
// "0000000000000011" : available for future use
// "0000000000000010" : available for future use
// "0000000000000001" : available for future use
#define PFX_NULL_CONTENT 0x0000 // Null content

//
// Macros to extract 14-bit data content
//
#define GET_CARD_IX(w) (((w)&0x3E00) >> 9)
#define GET_CHIP_IX(w) (((w)&0x0180) >> 7)
#define GET_CHAN_IX(w) (((w)&0x007F) >> 0)

//
// Macros to extract 12-bit data content
//
#define GET_ADC_DATA(w) (((w)&0x0FFF) >> 0)
#define GET_LAT_HISTO_BIN(w) (((w)&0x0FFF) >> 0)
#define PUT_LAT_HISTO_BIN(w) (PFX_LAT_HISTO_BIN | (((w)&0x0FFF) >> 0))
#define GET_LST_READ_CELL(w) (((w)&0x03FF) >> 0)
#define GET_LST_READ_CELL_CHIP_IX(w) (((w)&0x0C00) >> 10)

//
// Macros to extract 9-bit data content
//
#define GET_TIME_BIN(w) (((w)&0x01FF) >> 0)
#define GET_HISTO_BIN(w) (((w)&0x01FF) >> 0)
#define GET_PEDTHR_LIST_FEM(w) (((w)&0x01F0) >> 4)
#define GET_PEDTHR_LIST_ASIC(w) (((w)&0x000C) >> 2)
#define GET_PEDTHR_LIST_MODE(w) (((w)&0x0002) >> 1)
#define GET_PEDTHR_LIST_TYPE(w) (((w)&0x0001) >> 0)
#define PUT_FVERSION_FEMID(w, fv, id)                                          \
  (((w)&0xFE00) | (((fv)&0x0003) << 7) | (((id)&0x001F) << 0))
#define GET_FRAMING_VERSION(w) (((w)&0x0180) >> 7)
#define GET_FEMID(w) (((w)&0x001F) >> 0)

//
// Macros to act on 8-bit data content
//
#define GET_ASCII_LEN(w) (((w)&0x00FF) >> 0)
#define PUT_ASCII_LEN(w) (PFX_ASCII_MSG_LEN | ((w)&0x00FF))

//
// Macros to act on 4-bit data content
//
#define GET_EVENT_TYPE(w) (((w)&0x0007) >> 0)
#define GET_EOE_SIZE(w) (((w)&0x000F) >> 0)

//
// Macros to extract 2-bit data content
//
#define GET_CH_HIT_CNT_HISTO_CHIP_IX(w) (((w)&0x0003) >> 0)
#define PUT_CH_HIT_CNT_HISTO_CHIP_IX(w) (PFX_CH_HIT_CNT_HISTO | ((w)&0x0003))

#define CURRENT_FRAMING_VERSION 0

namespace FEMINOSPacket {

// enum class packetReply { ERROR = -1, RETRY = 0, OK = 1 };
// enum class packetType { ASCII = 0, BINARY = 1 };

void DataPacket_Print(uint16_t *fr, const uint16_t &size);
int HistoStat_Print(uint16_t *fr, int &sz_rd, const uint16_t &hitCount);
uint32_t GetUInt32FromBuffer(uint16_t *fr, int &sz_rd);
uint32_t GetUInt32FromBufferInv(uint16_t *fr, int &sz_rd);
bool TryExtractNextEvent(std::deque<uint16_t> &buffer, size_t &idx,
                         std::deque<uint16_t> &out);
void ParseEventFromWords(std::deque<uint16_t> &event, SignalEvent &sEvent,
                         uint64_t &tS, uint32_t &ev_count);
void GetPedestalEvent(std::deque<uint16_t> &buffer, SignalEvent &sEvent);
bool isDataFrame(uint16_t *fr);
bool isMFrame(uint16_t *fr);

} // namespace FEMINOSPacket
