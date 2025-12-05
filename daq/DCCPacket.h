#ifndef DCC_PACKET_H
#define DCC_PACKET_H

#include <stdint.h>
#include <netinet/in.h>

#define MAX_ETH_PACKET_DATA_SIZE 4096

// Response Packet Header Decode Macros
#define GET_REQ_INDEX(word) ((word)&0x000F)
#define GET_WR_ACK(word) ((word)&0x0030)
#define GET_RD_ACK(word) ((word)&0x0040)
#define GET_TR_OK(word) ((word)&0x0080)
#define GET_TYPE(word) ((word)&0xF000)
#define PUT_TYPE(word, ty) (((word)&0x0FFF) | ((ty)&0xF000))

// Response Packet Types from FEM (see t2k_pkg.vhd for coherence)
#define RESP_TYPE_ADC_DATA 0x0000
#define RESP_TYPE_SLOW_CTRL 0x1000
// These types are not among those returned by the FEM but by the DCC
#define RESP_TYPE_HISTOGRAM 0x2000
#define RESP_TYPE_HISTOSTAT 0x3000
#define RESP_TYPE_HISTOSUMMARY 0x4000
#define GET_RESP_TYPE(word) (((word)&0xF000) >> 12)
#define GET_RESP_INDEX(word) ((word)&0x000F)

// Macros to interpret DataPacket header
#define GET_FEC_ERROR(word) (((word)&0x03F0) >> 4)
#define GET_LOS_FLAG(word) (((word)&0x0400) >> 10)
#define GET_SYNCH_FAIL(word) (((word)&0x0800) >> 11)

// Macros to interpret DataPacket read back arguments
#define GET_RB_MODE(word) (((word)&0x4000) >> 14)
#define GET_RB_COMPRESS(word) (((word)&0x2000) >> 13)
#define GET_RB_ARG2(word) (((word)&0x1E00) >> 9)
#define GET_RB_ARG1(word) (((word)&0x01FF))

// Macros to interpret DataPacket event type / count
#define GET_EVENT_TYPE(word) (((word)&0xC000) >> 14)
#define GET_EVENT_COUNT(word) (((word)&0x3FFF))

// Macros to interpret DataPacket Samples
#define CELL_INDEX_FLAG 0x1000
#define GET_CELL_INDEX(word) (((word)&0x0FFF))
#define ARGUMENT_FLAG 0x2000
#define GET_ARGUMENTS(word) (((word)&0xDFFF))
#define SAMPLE_COUNT_FLAG 0x4000
#define GET_SAMPLE_COUNT(word) (((word)&0x0FFF))

// Macros to manipulate DataPacket DCC header
#define FRAME_TYPE_FEM_DATA   0x0000
#define FRAME_TYPE_DCC_DATA   0x0001
#define FRAME_FLAG_EORQ       0x0004
#define FRAME_FLAG_EOEV       0x0008
#define GET_FEM_INDEX(word)      (((word) & 0x000F)>>0)
#define GET_DCC_INDEX(word)      (((word) & 0x03F0)>>4)
#define GET_FRAME_TY_V2(word)    (((word) & 0x3C00)>>10)
#define PUT_FEM_INDEX(word, ix)  (((word) & 0xFFF0) | (((ix) & 0x000F) << 0))
#define PUT_DCC_INDEX(word, ix)  (((word) & 0xFC0F) | (((ix) & 0x003F) << 4))
#define PUT_FRAME_TY_V2(word, ty) (((word) & 0x03FF) | FRAME_HDR_V2 | (((ty) & 0x000F) <<10))

// We now support the old and new version of the data packet structure
#define FRAME_HDR_V2_FIELD 0xC000
#define FRAME_HDR_V2       0x4000
#define IS_DATA_PACKET_V2(word)     (((word) & FRAME_HDR_V2_FIELD) == FRAME_HDR_V2)

//Miscellanea
#define MAX_NB_OF_FEM_PER_DCC 6
#define MAX_EVENT_SIZE 6 * 4 * 79 * 2 * (512 + 32)*12
#define BHISTO_SIZE 256
#define PHISTO_SIZE 512
#define ASIC_MAX_CHAN_INDEX 78

namespace DCCPacket {

  // New structure of DataPacket. Aliased to DataPacketV2
  typedef struct {
	unsigned short size;     // size in bytes including the size of the size field itself
	unsigned short dcchdr;   // specific header added by the DCC
	unsigned short hdr;      // specific header put by the FEM
	unsigned short args;     // read-back arguments echoed by FEM
	unsigned short ts_h;     // local FEM timestamp MSB
	unsigned short ts_l;     // local FEM timestamp LSB
	unsigned short ecnt;     // local FEM event type/count
	unsigned short scnt;     // sample count in first pulse over thr or total sample count in packet
	unsigned short samp[MAX_ETH_PACKET_DATA_SIZE/(sizeof(unsigned short))]; // data storage area
  } DataPacket;

  // End of Event Packet
  typedef struct {
    unsigned short size;                           // size in bytes including the size of the size field itself
    unsigned short dcchdr;                         // specific header added by the DCC
    unsigned short hdr;                            // specific header similar to what is put by the FEM
    unsigned short pad;                            // padding for 32-bit alignement
    unsigned int byte_rcv[MAX_NB_OF_FEM_PER_DCC];  // data received per FEM
    unsigned int byte_snd[MAX_NB_OF_FEM_PER_DCC];  // data sent per FEM
    unsigned int tot_byte_rcv;
    unsigned int tot_byte_snd;
  } EndOfEventPacket;

  typedef struct {
    unsigned short size;               // size in bytes including the size of the size field itself
    unsigned short dcchdr;             // specific header added by the DCC
    unsigned short hdr;                // placeholder for header normally put by FEM
    unsigned short min_bin;            // range lower bound
    unsigned short max_bin;            // range higher bound
    unsigned short bin_wid;            // width of one bin in number of units
    unsigned short bin_cnt;            // number of bins
    unsigned short min_val;            // lowest bin with at least one entry
    unsigned short max_val;            // highest bin with at least one entry
    unsigned short mean;               // mean
    unsigned short stddev;             // standard deviation
    unsigned short pad;                // padding for 32-bit alignment
    unsigned int entries;              // number of entries
    unsigned short samp[BHISTO_SIZE];  // data storage area
  } HistogramPacket;

  typedef struct {
    unsigned short mean;   // mean of channel pedestal
    unsigned short stdev;  // standard deviation of channel pedestal
  } channel_stat;

  // Histogram data packet for pedestals
  typedef struct  {
    unsigned short size;                         // size in bytes including the size of the size field itself
    unsigned short dcchdr;                       // specific header added by the DCC
    unsigned short hdr;                          // specific header inherited from FEM
    unsigned short args;                         // read-back arguments as encoded by FEM
    unsigned short scnt;                         // number of unsigned short filled in following array
    channel_stat stat[ASIC_MAX_CHAN_INDEX + 1];  // summary of statistics for current ASIC
  } PedestalHistoSummaryPacket;

  // Histogram data packet for pedestals
  typedef struct {
    unsigned short size;     // size in bytes including the size of the size field itself
    unsigned short dcchdr;   // specific header added by the DCC
    unsigned short hdr;      // specific header put by the FEM
    unsigned short args;     // read-back arguments echoed by FEM
    unsigned short min_bin;  // range lower bound
    unsigned short max_bin;  // range higher bound
    unsigned short bin_wid;  // width of one bin in number of units
    unsigned short bin_cnt;  // number of bins
    unsigned short min_val;  // lowest bin with at least one entry
    unsigned short max_val;  // highest bin with at least one entry
    unsigned short mean;     // mean
    unsigned short stddev;   // standard deviation
    unsigned int entries;    // number of entries
    unsigned short bin_sat;  // number of bins saturated
    unsigned short align;    // alignment
  } PedestalHistoMathPacket;

  // New structure of DataPacket. Aliased to DataPacketV2
  typedef struct {
    unsigned short size;               // size in bytes including the size of the size field itself
    unsigned short dcchdr;             // specific header added by the DCC
    unsigned short hdr;                // specific header put by the FEM
    unsigned short args;               // read-back arguments echoed by FEM
    unsigned short ts_h;               // local FEM timestamp MSB
    unsigned short ts_l;               // local FEM timestamp LSB
    unsigned short ecnt;               // local FEM event type/count
    unsigned short scnt;               // sample count in first pulse over thr or total sample count in packet
    unsigned short samp[PHISTO_SIZE];  // data storage area
  } PedestalHistoBinPacket;

  enum class packetReply { ERROR = -1, RETRY = 0, OK = 1 };
  enum class packetType { ASCII = 0, BINARY = 1 };
  enum class packetDataType { NONE = 0, EVENT = 1, PEDESTAL = 2 };

  void DataPacket_Print( DataPacket *pck);
  void DCC_Data_Print(EndOfEventPacket* pck);
  void DCC_Histogram_Print(HistogramPacket* pck);
  void EndOfEvent_PrintPacket(EndOfEventPacket* eop);
  void Pedestal_PrintHistoMathPacket(PedestalHistoMathPacket* phm);
  void Pedestal_PrintHistoBinPacket(PedestalHistoBinPacket* pck);
  void Pedestal_PrintHistoSummaryPacket(PedestalHistoSummaryPacket* pck);
  void FemAdcDataPrint(DataPacket* pck);
  int Arg12ToFecAsicChannel(unsigned short arg1, unsigned short arg2, unsigned short &fec, unsigned short &asic, unsigned short  &channel);
  int Arg12ToFecAsic(unsigned short arg1, unsigned short arg2, unsigned short &fec, unsigned short &asic, unsigned short channel);
}

#endif
