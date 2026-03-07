
#include <DCCPacket.h>

#include <cstdio>

void DCCPacket::DataPacket_Print(DataPacket *pck, FILE *fp) {

  // DCC data packet has a different structure
  if (GET_FRAME_TY_V2(ntohs(pck->dcchdr)) & FRAME_TYPE_DCC_DATA) {
    DCC_Data_Print((EndOfEventPacket *)pck, fp);
    return;
  } // FEM data - Pedestal Histogram Mathematics
  else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_HISTOSTAT) {
    Pedestal_PrintHistoMathPacket((PedestalHistoMathPacket *)pck, fp);
    return;
  } // FEM data - Pedestal Histogram Bins
  else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_HISTOGRAM) {
    Pedestal_PrintHistoBinPacket((PedestalHistoBinPacket *)pck, fp);
    return;
  } // FEM data - Pedestal Histogram Summary
  else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_HISTOSUMMARY) {
    Pedestal_PrintHistoSummaryPacket((PedestalHistoSummaryPacket *)pck, fp);
    return;
  } // FEM data - ADC samples in zero-suppressed or non zero-suppressed mode
  else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_ADC_DATA) {
    FemAdcDataPrint(pck, fp);
    return;
  }
}

void DCCPacket::DCC_Data_Print(EndOfEventPacket *pck, FILE *fp) {
  // DCC end of event packet
  if (GET_FRAME_TY_V2(ntohs(pck->dcchdr)) & FRAME_FLAG_EOEV) {
    EndOfEvent_PrintPacket(pck);
    return;
  }
  // DCC histogram packet
  else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_HISTOGRAM) {
    DCC_Histogram_Print((HistogramPacket *)pck);
    return;
  }

  else {
    fprintf(fp, "DCC_Data_Print: unknown frame type 0x%x\n",
            GET_FRAME_TY_V2(ntohs(pck->dcchdr)));
  }
}

void DCCPacket::DCC_Histogram_Print(HistogramPacket *pck, FILE *fp) {
  unsigned short i;
  unsigned short min_val;
  unsigned short max_val;
  unsigned short min_bin;
  unsigned short max_bin;
  unsigned short bin_cnt;
  unsigned short bin_wid;
  unsigned short samp;

  min_val = ntohs(pck->min_val);
  max_val = ntohs(pck->max_val);
  min_bin = ntohs(pck->min_bin);
  max_bin = ntohs(pck->max_bin);
  bin_wid = ntohs(pck->bin_wid);
  bin_cnt = ntohs(pck->bin_cnt);

  fprintf(fp, "------------------------------------\n");
  fprintf(fp, "DCC Header: Type:0x%x DCC_index:0x%x\n",
          GET_FRAME_TY_V2(ntohs(pck->dcchdr)),
          GET_DCC_INDEX(ntohs(pck->dcchdr)));
  fprintf(fp, "Bin min   : %d ms\n", min_bin);
  fprintf(fp, "Bin max   : %d ms\n", max_bin);
  fprintf(fp, "Bin width : %d ms\n", bin_wid);
  fprintf(fp, "Bin count : %d\n", bin_cnt);
  fprintf(fp, "Min val   : %d ms\n", min_val);
  fprintf(fp, "Max val   : %d ms\n", max_val);
  fprintf(fp, "Mean      : %.2f ms\n", ntohs(pck->mean) / 100.0);
  fprintf(fp, "StdDev    : %.2f ms\n", ntohs(pck->stddev) / 100.0);
  fprintf(fp, "Entries   : %d\n", ntohl(pck->entries));
  for (i = 0; i < bin_cnt; i++) {
    samp = ntohs(pck->samp[i]);
    fprintf(fp, "Bin(%3d) = %4d\n", i, samp);
  }
  fprintf(fp, "------------------------------------\n");
}

void DCCPacket::EndOfEvent_PrintPacket(EndOfEventPacket *eop, FILE *fp) {
  int i;

  fprintf(fp, "------------------------------------\r\n");
  fprintf(fp, "Message size : %d bytes\r\n", ntohs(eop->size));
  fprintf(fp, "DCC Header: Type:0x%x DCC_index:0x%x FEM_index:0x%x\r\n",
          GET_FRAME_TY_V2(ntohs(eop->dcchdr)),
          GET_DCC_INDEX(ntohs(eop->dcchdr)), GET_FEM_INDEX(ntohs(eop->dcchdr)));
  fprintf(fp, "FEM Header: 0x%x\r\n", ntohs(eop->hdr));
  for (i = 0; i < MAX_NB_OF_FEM_PER_DCC; i++) {
    fprintf(fp, "Fem(%2d)   : recv: %d bytes sent: %d bytes\r\n", i,
            ntohl(eop->byte_rcv[i]), ntohl(eop->byte_snd[i]));
  }
  fprintf(fp, "Total     : recv: %d bytes sent: %d bytes\r\n",
          ntohl(eop->tot_byte_rcv), ntohl(eop->tot_byte_snd));
}

/*******************************************************************************
 Pedestal_PrintHistoMathPacket
*******************************************************************************/
void DCCPacket::Pedestal_PrintHistoMathPacket(PedestalHistoMathPacket *phm,
                                              FILE *fp) {
  fprintf(fp, "------------------------------------\n");
  fprintf(fp, "Message size : %d bytes\n", ntohs(phm->size));
  fprintf(fp, "DCC Header: Type:0x%x DCC_index:0x%x FEM_index:0x%x\n",
          GET_FRAME_TY_V2(ntohs(phm->dcchdr)),
          GET_DCC_INDEX(ntohs(phm->dcchdr)), GET_FEM_INDEX(ntohs(phm->dcchdr)));
  fprintf(fp, "FEM Header: 0x%x\n", ntohs(phm->hdr)),
      fprintf(fp, "Read-back : Mode:%d Compress:%d Arg1:0x%x Arg2:0x%x\n",
              GET_RB_MODE(ntohs(phm->args)), GET_RB_COMPRESS(ntohs(phm->args)),
              GET_RB_ARG1(ntohs(phm->args)), GET_RB_ARG2(ntohs(phm->args)));

  fprintf(fp, "Bin min   : %d\n", ntohs(phm->min_bin));
  fprintf(fp, "Bin max   : %d\n", ntohs(phm->max_bin));
  fprintf(fp, "Bin width : %d\n", ntohs(phm->bin_wid));
  fprintf(fp, "Bin count : %d\n", ntohs(phm->bin_cnt));
  fprintf(fp, "Min val   : %d\n", ntohs(phm->min_val));
  fprintf(fp, "Max val   : %d\n", ntohs(phm->max_val));
  fprintf(fp, "Mean      : %.2f\n", ((double)(ntohs(phm->mean))) / 100.0);
  fprintf(fp, "StdDev    : %.2f\n", ((double)(ntohs(phm->stddev))) / 100.0);
  fprintf(fp, "Entries   : %d\n", ntohl(phm->entries));
  fprintf(fp, "Bin satur.: %d\n", ntohs(phm->bin_sat));
  fprintf(fp, "Align     : %d\n", ntohs(phm->align));
  fprintf(fp, "------------------------------------\n");
}

/*******************************************************************************
 Pedestal_PrintHistoBinPacket
*******************************************************************************/
void DCCPacket::Pedestal_PrintHistoBinPacket(PedestalHistoBinPacket *pck,
                                             FILE *fp) {
  unsigned short i;
  unsigned short nbsw;
  unsigned short samp;

  fprintf(fp, "------------------------------------\n");
  fprintf(fp, "Message size : %d bytes\n", ntohs(pck->size));
  fprintf(fp, "DCC Header   : Type:0x%x DCC_index:0x%x FEM_index:0x%x\n",
          GET_FRAME_TY_V2(ntohs(pck->dcchdr)),
          GET_DCC_INDEX(ntohs(pck->dcchdr)), GET_FEM_INDEX(ntohs(pck->dcchdr)));

  fprintf(fp, "FEM Header   : Msg_type:0x%x\n", GET_RESP_TYPE(ntohs(pck->hdr)));
  fprintf(fp, "Read-back    : Arg1:0x%x Arg2:0x%x\n",
          GET_RB_ARG1(ntohs(pck->args)), GET_RB_ARG2(ntohs(pck->args)));
  fprintf(fp, "NbOfWords    : %d\n", ntohs(pck->scnt));

  // Compute the number of short words in the packet
  // Must subtract:
  // .  2 bytes for DCC header,
  // . 12 bytes for FEM header,
  // .  2 bytes used for storing the size field itself
  nbsw = (ntohs(pck->size) - 2 - 12 - 2) / sizeof(short);

  for (i = 0; i < nbsw; i++) {
    samp = ntohs(pck->samp[i]);
    fprintf(fp, "Bin(%3d)= %5d\n", i, samp);
  }
  fprintf(fp, "------------------------------------\n");
}

/*******************************************************************************
 Pedestal_PrintHistoSummaryPacket
*******************************************************************************/
void DCCPacket::Pedestal_PrintHistoSummaryPacket(
    PedestalHistoSummaryPacket *pck, FILE *fp) {
  unsigned short i;
  unsigned short nbsw;

  fprintf(fp, "------------------------------------\n");
  fprintf(fp, "Message size : %d bytes\n", ntohs(pck->size));
  fprintf(fp, "DCC Header   : Type:0x%x DCC_index:0x%x FEM_index:0x%x\n",
          GET_FRAME_TY_V2(ntohs(pck->dcchdr)),
          GET_DCC_INDEX(ntohs(pck->dcchdr)), GET_FEM_INDEX(ntohs(pck->dcchdr)));

  fprintf(fp, "FEM Header   : Msg_type:0x%x\n", GET_RESP_TYPE(ntohs(pck->hdr)));
  fprintf(fp, "Read-back    : Arg1:0x%x Arg2:0x%x\n",
          GET_RB_ARG1(ntohs(pck->args)), GET_RB_ARG2(ntohs(pck->args)));
  fprintf(fp, "NbOfWords    : %d\n", ntohs(pck->scnt));

  // Compute the number of short words in the packet
  // Must subtract:
  // .  2 bytes for DCC header,
  // .  6 bytes for other header,
  // .  2 bytes used for storing the size field itself
  nbsw = (ntohs(pck->size) - 2 - 6 - 2) / sizeof(short);

  for (i = 0; i < (nbsw / 2); i++) {
    fprintf(fp, "Stat   (%3d) : mean = %.2f  stdev = %.2f\n", i,
            ((double)(ntohs(pck->stat[i].mean))) / 100.0,
            ((double)(ntohs(pck->stat[i].stdev))) / 100.0);
  }
  fprintf(fp, "------------------------------------\n");
}

/*******************************************************************************
 FemAdcDataPrint
*******************************************************************************/
void DCCPacket::FemAdcDataPrint(DataPacket *pck, FILE *fp) {
  unsigned short i;
  unsigned int ts;
  unsigned short samp;
  short nbsw;
  unsigned int tmp;
  unsigned short fec, asic, channel;

  fprintf(fp, "------------------------------------\n");
  fprintf(fp, "Packet sz : %d bytes\n", (ntohs(pck->size)));
  fprintf(fp, "DCC Header: Type:0x%x DCC_index:0x%x FEM_index:0x%x\n",
          GET_FRAME_TY_V2(ntohs(pck->dcchdr)),
          GET_DCC_INDEX(ntohs(pck->dcchdr)), GET_FEM_INDEX(ntohs(pck->dcchdr)));

  fprintf(fp, "FEM Header: Msg_type:0x%x Index:0x%x\n",
          GET_RESP_TYPE(ntohs(pck->hdr)), GET_RESP_INDEX(ntohs(pck->hdr)));
  fprintf(fp, "Errors    : FEC:0x%x Unable to SYNCH:%d Framing LOS:%d\n",
          GET_FEC_ERROR(ntohs(pck->hdr)), GET_SYNCH_FAIL(ntohs(pck->hdr)),
          GET_LOS_FLAG(ntohs(pck->hdr)));
  fprintf(fp, "Read-back : Mode:%d Compress:%d Arg1:0x%x Arg2:0x%x\n",
          GET_RB_MODE(ntohs(pck->args)), GET_RB_COMPRESS(ntohs(pck->args)),
          GET_RB_ARG1(ntohs(pck->args)), GET_RB_ARG2(ntohs(pck->args)));

  Arg12ToFecAsicChannel((unsigned short)GET_RB_ARG1(ntohs(pck->args)),
                        (unsigned short)GET_RB_ARG2(ntohs(pck->args)), fec,
                        asic, channel);

  fprintf(fp, "            Fec:%d Asic:%d Channel:%d\n", fec, asic, channel);

  ts = (((int)ntohs((pck->ts_h))) << 16) | (int)ntohs((pck->ts_l));
  fprintf(fp, "Event     : Type:%d Count:%d Time:0x%x NbOfWords:%d\n",
          GET_EVENT_TYPE(ntohs(pck->ecnt)), GET_EVENT_COUNT(ntohs(pck->ecnt)),
          ts, ntohs(pck->scnt));

  // Compute the number of short words in the packet
  // Must subtract:
  // .  2 bytes for DCC header,
  // . 12 bytes for FEM header,
  // .  2 bytes used for storing the size field itself,
  // .  4 bytes used in the trailer for CRC32 or debug info
  nbsw = (ntohs(pck->size) - 2 - 12 - 2 - 4) / sizeof(short);

  for (i = 0; i < nbsw; i++) {
    samp = ntohs(pck->samp[i]);
    if (samp & ARGUMENT_FLAG) {
      fprintf(fp, "ArgRb (%3d)=0x%3x", i, GET_ARGUMENTS(samp));
      Arg12ToFecAsicChannel((unsigned short)GET_RB_ARG1(samp),
                            (unsigned short)GET_RB_ARG2(samp), fec, asic,
                            channel);
      fprintf(fp, " (F:%d A:%d C:%d)\n", fec, asic, channel);
    } else if (samp & SAMPLE_COUNT_FLAG) {
      fprintf(fp, "0x%4x SamCnt(%3d)=0x%3x (%4d)\n", samp, i,
              GET_SAMPLE_COUNT(samp), GET_SAMPLE_COUNT(samp));
    } else if (samp & CELL_INDEX_FLAG) {
      fprintf(fp, "0x%4x Cell  (%3d)=0x%3x (%4d)\n", samp, i,
              GET_CELL_INDEX(samp), GET_CELL_INDEX(samp));
    } else {
      fprintf(fp, "Sample(%3d)=0x%4x (%4d)\n", i, samp, samp);
    }
  }
  tmp = (((unsigned int)ntohs(pck->samp[i])) << 16) |
        ((unsigned int)ntohs(pck->samp[i + 1]));
  fprintf(fp, "Trailer    =0x%08x (%d)\n", tmp, tmp);
  fprintf(fp, "------------------------------------\n");
}

/*******************************************************************************
 Arg12ToFecAsicChannel
*******************************************************************************/
int DCCPacket::Arg12ToFecAsicChannel(unsigned short arg1, unsigned short arg2,
                                     unsigned short &fec, unsigned short &asic,
                                     unsigned short &channel) {

  fec = (10 * (arg1 % 6) / 2 + arg2) / 4;
  asic = (10 * (arg1 % 6) / 2 + arg2) % 4;
  channel = arg1 / 6;
  if ((fec > 5) || (asic > 3)) {
    fec = arg2 - 4;
    asic = 4;
    channel = (arg1 - 4) / 6;
  }

  int physChannel = -10;
  if (channel > 2 && channel < 15) {
    physChannel = channel - 3;
  } else if (channel > 15 && channel < 28) {
    physChannel = channel - 4;
  } else if (channel > 28 && channel < 53) {
    physChannel = channel - 5;
  } else if (channel > 53 && channel < 66) {
    physChannel = channel - 6;
  } else if (channel > 66) {
    physChannel = channel - 7;
  }

  if (physChannel < 0)
    return physChannel;

  physChannel = fec * 72 * 4 + asic * 72 + physChannel;

  return physChannel;
}

/*******************************************************************************
 Arg12ToFecAsicChannel
*******************************************************************************/
int DCCPacket::Arg12ToFecAsic(unsigned short arg1, unsigned short arg2,
                              unsigned short &fec, unsigned short &asic,
                              unsigned short channel) {

  fec = (10 * (arg1 % 6) / 2 + arg2) / 4;
  asic = (10 * (arg1 % 6) / 2 + arg2) % 4;

  if ((fec > 5) || (asic > 3)) {
    fec = arg2 - 4;
    asic = 4;
  }

  int physChannel = -10;
  if (channel > 2 && channel < 15) {
    physChannel = channel - 3;
  } else if (channel > 15 && channel < 28) {
    physChannel = channel - 4;
  } else if (channel > 28 && channel < 53) {
    physChannel = channel - 5;
  } else if (channel > 53 && channel < 66) {
    physChannel = channel - 6;
  } else if (channel > 66) {
    physChannel = channel - 7;
  }

  if (physChannel < 0)
    return physChannel;

  physChannel = fec * 72 * 4 + asic * 72 + physChannel;

  return physChannel;
}
