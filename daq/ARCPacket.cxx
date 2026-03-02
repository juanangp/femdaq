
#include "ARCPacket.h"

#include <cstdio>
#include <inttypes.h>

namespace ARCPacket {

void DataPacket_Print(uint16_t *fr, const uint16_t &size) {

  int sz_rd = 0, si = 0;
  uint16_t r0, r1, r2, r3;

  do {
    if ((*fr & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_IX) {
      r0 = GET_CARD_IX(*fr);
      r1 = GET_CHIP_IX(*fr);
      r2 = GET_CHAN_IX(*fr);
      printf("Card %02d Chip %01d Channel %02d\n", r0, r1, r2);
      fr++;
      sz_rd++;
      si = 0;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_CHIP_CHAN_HIT_CNT) {
      r0 = GET_CHIP_IX(*fr);
      r1 = GET_CHAN_IX(*fr);
      printf("Chip %01d Channel_Hit_Count %02d\n", r0, r1);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_0_BIT_CONTENT_MASK) ==
               PFX_EXTD_CARD_CHIP_CHAN_HIT_IX) {
      fr++;
      sz_rd++;
      r0 = GET_CARD_IX(*fr);
      r1 = GET_CHIP_IX(*fr);
      r2 = GET_CHAN_IX(*fr);
      printf("Card %02d Chip %01d Channel %02d\n", r0, r1, r2);
      fr++;
      sz_rd++;
      si = 0;
    } else if ((*fr & PFX_0_BIT_CONTENT_MASK) ==
               PFX_EXTD_CARD_CHIP_CHAN_HISTO) {
      fr++;
      sz_rd++;
      r0 = GET_CARD_IX(*fr);
      r1 = GET_CHIP_IX(*fr);
      r2 = GET_CHAN_IX(*fr);
      printf("Card %02d Chip %01d Channel %02d\n", r0, r1, r2);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HISTO) {
      r0 = GET_CARD_IX(*fr);
      r1 = GET_CHIP_IX(*fr);
      r2 = GET_CHAN_IX(*fr);
      printf("Card %02d Chip %01d Channel_Histo_Index %02d\n", r0, r1, r2);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE) {
      r0 = GET_ADC_DATA(*fr);
      printf("%03d 0x%04x (%4d)\n", si, r0, r0);
      fr++;
      sz_rd++;
      si++;
    } else if ((*fr & PFX_12_BIT_CONTENT_MASK) == PFX_LAT_HISTO_BIN) {
      r0 = GET_LAT_HISTO_BIN(*fr);
      fr++;
      sz_rd++;
      uint32_t tmp = GetUInt32FromBufferInv(fr, sz_rd);
      fr += 2;
      printf("%03d %03d\n", r0, tmp);
    } else if ((*fr & PFX_11_BIT_CONTENT_MASK) == PFX_CHIP_LAST_CELL_READ_ARC) {
      r0 = GET_CHIP_IX_LCR(*fr);
      r1 = GET_LAST_CELL_READ(*fr);
      printf("#Last Cell Read chip %d bin %d\n", r0, r1);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {
      r0 = GET_TIME_BIN(*fr);
      printf("Time_Bin: %d\n", r0);
      fr++;
      sz_rd++;
      si = 0;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_HISTO_BIN_IX) {
      r0 = GET_HISTO_BIN(*fr);
      fr++;
      sz_rd++;
      printf("Bin %3d Val %5d\n", r0, *fr);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_PEDTHR_LIST) {
      r0 = GET_PEDTHR_LIST_FEM(*fr);
      r1 = GET_PEDTHR_LIST_ASIC(*fr);
      r2 = GET_PEDTHR_LIST_MODE(*fr);
      r3 = GET_PEDTHR_LIST_TYPE(*fr);
      fr++;
      sz_rd++;

      if (r3 == 0) { // pedestal entry
        printf("# Pedestal List for FEM %02d ASIC %01d\n", r0, r1);
      } else {
        printf("# Threshold List for FEM %02d ASIC %01d\n", r0, r1);
      }
      printf("fem %02d\n", r0);
      if (r2 == 0) { // AGET
        r2 = 71;
      } else {
        r2 = 78;
      }

      for (int j = 0; j <= r2; j++) {
        if (r3 == 0)
          printf("ped ");
        else
          printf("thr ");
        printf("%1d %2d 0x%04x (%4d)\n", r1, j, *fr, *fr);
        fr++;
        sz_rd++;
      }
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME) {
      r0 = GET_VERSION_FRAMING(*fr);
      r1 = GET_SOURCE_TYPE(*fr);
      r2 = GET_SOURCE_ID(*fr);
      fr++;
      sz_rd++;
      printf("--- Start of Data Frame (V.%01d) Source type %02d id %02d --\n",
             r0, r1, r2);
      printf("Filled with %d bytes\n", *fr);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME) {
      r0 = GET_VERSION_FRAMING(*fr);
      r1 = GET_SOURCE_TYPE(*fr);
      r2 = GET_SOURCE_ID(*fr);
      printf("--- Start of Moni Frame (V.%01d) Source type %02d id %02d --\n",
             r0, r1, r2);
      fr++;
      sz_rd++;
      printf("Filled with %d bytes\n", *fr);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_CFRAME) {
      r0 = GET_VERSION_FRAMING(*fr);
      r1 = GET_SOURCE_TYPE(*fr);
      r2 = GET_SOURCE_ID(*fr);
      fr++;
      sz_rd++;
      printf("--- Start of Config Frame (V.%01d) Source type %02d id %02d --\n",
             r0, r1, r2);
      printf("Error code: %d\n", *((short *)fr));
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_8_BIT_CONTENT_MASK) == PFX_ASCII_MSG_LEN) {
      r0 = GET_ASCII_LEN(*fr);
      fr++;
      sz_rd++;
      printf("ASCII Msg length: %d\n", r0);
      std::string asciiMsg;
      int j = 0;
      for (j = 0; j < r0 / 2; j++) {
        asciiMsg += (char)(*fr & 0xFF);
        asciiMsg += (char)(*fr & 0xFF00) >> 8;
        fr++;
        sz_rd++;
      }
      if (r0 & 0x1) {
        asciiMsg += (char)(*fr & 0xFF);
        fr++;
        sz_rd++;
      }
      asciiMsg += "\0";
      printf("%s\n", asciiMsg.c_str());
      fr++;
      sz_rd++;
    } else if (*fr == PFX_LONG_ASCII_MSG) {
      fr++;
      sz_rd++;
      r0 = (*fr);
      printf("Long ASCII Msg length: %d\n", r0);
      fr++;
      sz_rd++;
      std::string asciiMsg;
      int j = 0;
      for (j = 0; j < r0 / 2; j++) {
        asciiMsg += (char)(*fr & 0xFF);
        asciiMsg += (char)((*fr & 0xFF00) >> 8);
        fr++;
        sz_rd++;
      }
      if (r0 & 0x1) {
        asciiMsg += (char)(*fr & 0xFF);
      }
      std::cout << asciiMsg << std::endl;
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_8_BIT_CONTENT_MASK) == PFX_START_OF_EVENT_ARC) {
      r0 = GET_SOE_EV_TYPE(*fr);
      printf("-- Start of Event (Type %01d) --\n", r0);
      fr++;
      sz_rd++;
      uint64_t ts = *fr;
      fr++;
      sz_rd++;
      ts |= ((*fr) << 16);
      fr++;
      sz_rd++;
      ts |= ((*fr) << 24);
      fr++;
      sz_rd++;
      printf("Time %" PRId64 "\n", ts);

      uint32_t evC = GetUInt32FromBufferInv(fr, sz_rd);
      fr += 2;
      printf("Event_Count %d\n", evC);

    } else if ((*fr & PFX_6_BIT_CONTENT_MASK) == PFX_END_OF_EVENT_ARC) {
      r1 = GET_EOE_SOURCE_TYPE(*fr);
      r2 = GET_EOE_SOURCE_ID(*fr);
      // Skip one word (reserved)
      fr++;
      sz_rd++;
      fr++;
      sz_rd++;
      uint32_t size_ev = ((*fr) << 16);
      fr++;
      sz_rd++;
      size_ev |= *fr;
      fr++;
      sz_rd++;
      printf("----- End of Event ----- (size %d bytes)\n", size);
    } else if ((*fr & PFX_2_BIT_CONTENT_MASK) == PFX_CH_HIT_CNT_HISTO) {
      r0 = GET_CH_HIT_CNT_HISTO_CHIP_IX(*fr);
      printf("Channel Hit Count Histogram (ASIC %d)\n", r0);
      fr++;
      sz_rd++;
      // null word
      fr++;
      sz_rd++;

      fr += HistoStat_Print(fr, sz_rd, r0);

    } else if ((*fr & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME) {
      printf("----- End of Frame -----\n");
      fr++;
      sz_rd++;
    } else if (*fr == PFX_NULL_CONTENT) {
      printf("null word (2 bytes)\n");
      fr++;
      sz_rd++;
    } else if ((*fr == PFX_DEADTIME_HSTAT_BINS) ||
               (*fr == PFX_EVPERIOD_HSTAT_BINS)) {
      if (*fr == PFX_DEADTIME_HSTAT_BINS)
        printf("Dead-time Histogram\n");
      else
        printf("Inter Event Time Histogram\n");
      fr += 2;
      sz_rd += 2;
      // null word
      fr += 2;
      sz_rd += 2;

      fr += HistoStat_Print(fr, sz_rd, 0);

    } else if (*fr == PFX_PEDESTAL_HSTAT) {
      printf("\nPedestal Histogram Statistics\n");
      fr++;
      sz_rd++;

      fr += HistoStat_Print(fr, sz_rd, 0);

    } else if (*fr == PFX_PEDESTAL_H_MD) {
      fr++;
      sz_rd++;

      uint32_t mean = GetUInt32FromBufferInv(fr, sz_rd);
      fr += 2;
      uint32_t std_dev = GetUInt32FromBufferInv(fr, sz_rd);
      fr += 2;
      printf("Mean/Std_dev : %.2f  %.2f\n", (float)mean / 100.,
             (float)std_dev / 100.);

      fr++;
      sz_rd++;

    } else if ((*fr & PFX_0_BIT_CONTENT_MASK) == PFX_EXTD_CARD_CHIP_CHAN_H_MD) {
      fr++;
      sz_rd++;
      r0 = GET_EXTD_CARD_IX(*fr);
      r1 = GET_EXTD_CHIP_IX(*fr);
      r2 = GET_EXTD_CHAN_IX(*fr);
      fr++;
      sz_rd++;
      uint32_t mean = GetUInt32FromBufferInv(fr, sz_rd);
      fr += 2;
      uint32_t std_dev = GetUInt32FromBufferInv(fr, sz_rd);
      fr += 2;
      printf("Ped Card %02d Chip %01d Channel %02d Mean/Std_dev : %.2f  %.2f\n",
             r0, r1, r2, (float)mean / 100., (float)std_dev / 100.);
    } else if (*fr == PFX_SHISTO_BINS) {
      printf("Threshold Turn-on curve\n");
      fr++;
      sz_rd++;
      for (int j = 0; j < 16; j++) {
        printf("%d ", *fr);
        fr++;
        sz_rd++;
      }
      printf("\n\n");
    } else if (*fr == PFX_CMD_STATISTICS) {
      fr++;
      sz_rd++;

      uint32_t tmp_i[9];
      for (int j = 0; j < 9; j++) {
        tmp_i[j] = GetUInt32FromBuffer(fr, sz_rd);
        fr += 2;
      }

      printf("Server RX stat: cmd_count=%d daq_req=%d daq_timeout=%d "
             "daq_delayed=%d daq_missing=%d cmd_errors=%d\n",
             tmp_i[0], tmp_i[1], tmp_i[2], tmp_i[3], tmp_i[4], tmp_i[5]);
      printf("Server TX stat: cmd_replies=%d daq_replies=%d "
             "daq_replies_resent=%d\n",
             tmp_i[6], tmp_i[7], tmp_i[8]);
    } else { // No interpretable data
      printf("word(%04d) : 0x%x (%d) unknown data\n", sz_rd, *fr, *fr);
      fr++;
      sz_rd++;
    }

  } while (sz_rd < size);

  if (sz_rd > size)
    printf("Format error: read %d words but packet size is %d\n", sz_rd, size);
}

int HistoStat_Print(uint16_t *fr, int &sz_rd, const uint16_t &hitCount) {

  int length = sz_rd;

  printf("Min Bin    : %d\n", GetUInt32FromBuffer(fr, sz_rd));
  fr += 2;
  printf("Max Bin    : %d\n", GetUInt32FromBuffer(fr, sz_rd));
  fr += 2;
  printf("Bin Width  : %d\n", GetUInt32FromBuffer(fr, sz_rd));
  fr += 2;
  printf("Bin Count  : %d\n", GetUInt32FromBuffer(fr, sz_rd));
  fr += 2;
  printf("Min Value  : %d\n", GetUInt32FromBuffer(fr, sz_rd));
  fr += 2;
  printf("Max Value  : %d\n", GetUInt32FromBuffer(fr, sz_rd));
  fr += 2;
  printf("Mean       : %.2f\n",
         ((float)GetUInt32FromBuffer(fr, sz_rd)) / 100.0);
  fr += 2;
  printf("Std Dev    : %.2f\n",
         ((float)GetUInt32FromBuffer(fr, sz_rd)) / 100.0);
  fr += 2;
  printf("Entries    : %d\n", GetUInt32FromBuffer(fr, sz_rd));
  fr += 2;
  // Get all bins
  for (int j = 0; j < hitCount; j++) {
    printf("Bin(%2d) = %9d\n", j, GetUInt32FromBuffer(fr, sz_rd));
    fr += 2;
  }

  length -= sz_rd;
  return length;
}

uint32_t GetUInt32FromBuffer(uint16_t *fr, int &sz_rd) {

  uint32_t res = (*fr) << 16;
  fr++;
  sz_rd++;
  res |= *fr;
  fr++;
  sz_rd++;

  return res;
}

uint32_t GetUInt32FromBufferInv(uint16_t *fr, int &sz_rd) {

  uint32_t res = *fr;
  fr++;
  sz_rd++;
  res |= (*fr) << 16;
  fr++;
  sz_rd++;

  return res;
}

bool TryExtractNextEvent(std::deque<uint16_t> &buffer, size_t &idx,
                         std::deque<uint16_t> &out) {

  if (buffer.empty())
    return false;

  bool endOfEvent = false;
  const size_t buffSize = buffer.size();

  while (idx < buffSize) {
    uint16_t w = buffer[idx];
    if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME) {
      idx += 2;
    } else if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME) {
      idx += 2;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_EXTD_CARD_CHIP_CHAN_H_MD) {
      idx += 6;
    } else if ((w & PFX_8_BIT_CONTENT_MASK) == PFX_START_OF_EVENT_ARC) {
      idx += 6;
    } else if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_CHIP_CHAN_HIT_CNT) {
      idx++;
    } else if ((w & PFX_11_BIT_CONTENT_MASK) == PFX_CHIP_LAST_CELL_READ_ARC) {
      idx++;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_EXTD_CARD_CHIP_CHAN_HIT_IX) {
      idx += 2;
    } else if ((w & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE ||
               (w & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {
      idx++;
    } else if ((w & PFX_6_BIT_CONTENT_MASK) == PFX_END_OF_EVENT_ARC) {
      idx += 4;
      endOfEvent = true;
      break;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME) {
      idx++;
    } else {
      printf(
          "TryExtractNextEvent WARNING: word : 0x%x (%d) unknown data at %d \n",
          w, w, idx);
      idx++;
    }
  }

  if (!endOfEvent) {
    return false; // incomplete event
  }

  // std::cout<<"New event size "<<idx<<std::endl;

  out.assign(buffer.begin(), buffer.begin() + idx);
  buffer.erase(buffer.begin(), buffer.begin() + idx);

  idx = 0;

  return true;
}

void ParseEventFromWords(std::deque<uint16_t> &event, SignalEvent &sEvent,
                         uint64_t &tS, uint32_t &ev_count) {

  if (event.empty())
    return;
  size_t idx = 0;
  const size_t buffSize = event.size();

  while (idx < buffSize) {
    size_t w = event[idx];
    if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME) {
      idx++;
      // std::cout<<"Start of DFRAME Size "<<event[idx]<<" bytes"<<std::endl;
      idx++;
    } else if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME) {
      idx++;
      // std::cout<<"Start of MFRAME Size "<<event[idx]<<" bytes"<<std::endl;
      idx++;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_EXTD_CARD_CHIP_CHAN_H_MD) {
      idx += 6; // We skip in case, this is done on GetPedestalEvent
    } else if ((w & PFX_8_BIT_CONTENT_MASK) == PFX_START_OF_EVENT_ARC) {
      // std::cout<<"START OF EVENT "<<std::endl;
      idx++;
      tS = (uint64_t)event[idx];
      idx++;
      tS |= ((uint64_t)event[idx] << 16);
      idx++;
      tS |= ((uint64_t)event[idx] << 32);
      idx++;

      // Event count
      ev_count = (uint32_t)event[idx];
      idx++;
      ev_count |= ((uint32_t)event[idx] << 16);
      idx++;
      // if(ev_count%100==0)std::cout<<"EvCnt "<<ev_count<<" TS "<<tS
      // <<std::endl;
    } else if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_CHIP_CHAN_HIT_CNT) {
      // printf( "Card %02d Chip %01d Channel_Hit_Count %02d\n",
      // GET_CARD_IX(event[idx]), GET_CHIP_IX(event[idx]),
      // GET_CHAN_IX(event[idx]) );
      idx++;
    } else if ((w & PFX_11_BIT_CONTENT_MASK) == PFX_CHIP_LAST_CELL_READ_ARC) {
      idx++;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_EXTD_CARD_CHIP_CHAN_HIT_IX) {
      idx++;
      uint16_t cardID = GET_CARD_IX(event[idx]);
      uint16_t chipID = GET_CHIP_IX(event[idx]);
      uint16_t chID = GET_CHAN_IX(event[idx]);
      int physChannel = chID + chipID * 72 + cardID * 288;
      // std::cout<<" Card "<<cardID<<" Chip "<<chipID<<" Channel "<<chID<<"
      // PhysChann "<<physChannel<<std::endl;
      idx++;
      int timeBin = 0;
      std::vector<short> sData(512, 0);
      while ((event[idx] & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE ||
             (event[idx] & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {

        if ((event[idx] & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {
          timeBin = GET_TIME_BIN(event[idx]);
        } else if ((event[idx] & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE) {
          if (timeBin < 512)
            sData[timeBin] = GET_ADC_DATA(event[idx]);
          // std::cout<<"TimeBin "<<timeBin<<" "<<sData[timeBin]<<std::endl;
          timeBin++;
        } else {
          break;
        }
        idx++;
        if (idx >= buffSize)
          break;
      }

      sEvent.AddSignal(physChannel, sData);

    } else if ((w & PFX_6_BIT_CONTENT_MASK) == PFX_END_OF_EVENT_ARC) {
      idx += 2;
      // uint32_t event_size = event[idx] & 0xFFFF;
      idx++;
      // event_size |=  ( event[idx] << 16) & 0xFFFF0000;
      idx++;
      // std::cout<<"END OF EVENT"<<std::endl;
      return;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME) {
      // std::cout<<" END OF FRAME "<<std::endl;
      idx++;
    } else {
      printf("WARNING: event %d word : 0x%x (%d) unknown data\n", ev_count,
             event.front(), event.front());
      idx++;
    }
    // std::cout<<"Buffer size left "<<buffSize-idx<<" words "<<std::endl;
  }
}

void GetPedestalEvent(std::deque<uint16_t> &buffer, SignalEvent &sEvent) {

  size_t idx = 0;

  if (buffer.empty())
    return;

  while (idx < buffer.size()) {
    uint16_t w = buffer[idx];
    if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME) {
      idx += 2;
    } else if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME) {
      idx += 2;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_EXTD_CARD_CHIP_CHAN_H_MD) {
      std::vector<short> sData;
      idx++;
      w = buffer[idx];
      uint16_t cardID = GET_EXTD_CARD_IX(w);
      uint16_t chipID = GET_EXTD_CHIP_IX(w);
      uint16_t chID = GET_EXTD_CHAN_IX(w);
      int physChannel = chID + chipID * 72 + cardID * 288;
      idx++;
      w = buffer[idx];
      uint32_t mean = w;
      sData.push_back(w);
      idx++;
      w = buffer[idx];
      mean |= (w << 16);
      sData.push_back(w);
      idx++;
      w = buffer[idx];
      uint32_t std_dev = w;
      sData.push_back(w);
      idx++;
      w = buffer[idx];
      sData.push_back(w);
      std_dev |= (w << 16);
      idx++;
      // printf("Ped Card %02d Chip %01d Channel %02d PhysChannel %02d
      // Mean/Std_dev : %.2f  %.2f\n", cardID, chipID, chID, physChannel,
      // (float)mean/100., (float)std_dev/100.);
      sEvent.AddSignal(physChannel, sData);
    } else if ((w & PFX_8_BIT_CONTENT_MASK) == PFX_START_OF_EVENT_ARC) {
      idx += 6;
    } else if ((w & PFX_9_BIT_CONTENT_MASK) == PFX_CHIP_CHAN_HIT_CNT) {
      idx++;
    } else if ((w & PFX_11_BIT_CONTENT_MASK) == PFX_CHIP_LAST_CELL_READ_ARC) {
      idx++;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_EXTD_CARD_CHIP_CHAN_HIT_IX) {
      idx += 2;
    } else if ((w & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE ||
               (w & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {
      idx++;
    } else if ((w & PFX_6_BIT_CONTENT_MASK) == PFX_END_OF_EVENT_ARC) {
      idx += 4;
      break;
    } else if ((w & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME) {
      idx++;
    } else {
      idx++;
    }
  }
}

bool isDataFrame(uint16_t *fr) {

  return ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME);
}

bool isMFrame(uint16_t *fr) {

  return ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME);
}

} // namespace ARCPacket
