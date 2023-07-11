#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "dts_dtshd_file.h"

static const char * _dtshdChunkIdStr(
  DtsHdChunkMagic id
)
{
  switch (id) {
    case DTS_HD_DTSHDHDR:
      return "DTS-HD File Header Chunk";
    case DTS_HD_FILEINFO:
      return "DTS-HD File Info Data Chunk";
    case DTS_HD_CORESSMD:
      return "DTS-HD Core Substream Metadata Chunk";
    case DTS_HD_EXTSS_MD:
      return "DTS-HD Extension Substream Metadata Chunk";
    case DTS_HD_AUPR_HDR:
      return "DTS-HD Audio Presentation Header Metadata Chunk";
    case DTS_HD_AUPRINFO:
      return "DTS-HD Audio Presentation Information Text Chunk";
    case DTS_HD_NAVI_TBL:
      return "DTS-HD Navigation Metadata Chunk";
    case DTS_HD_BITSHVTB:
      return "DTS-HD Bit Sheving Metadata Chunk";
    case DTS_HD_STRMDATA:
      return "DTS-HD Encoded Stream Data Chunk";
    case DTS_HD_TIMECODE:
      return "DTS-HD Timecode Data Chunk";
    case DTS_HD_BUILDVER:
      return "DTS-HD BuildVer Data Chunk";
    case DTS_HD_BLACKOUT:
      return "DTS-HD Encoded Blackout Data Chunk";
    case DTS_HD_BRANCHPT:
      return "DTS-HD Branch Point Metadata Chunk";
  }

  return "DTS-HD Unknown Chunk";
}

#define READ_BYTES_BE(d, bs, s, e)                                            \
  do {                                                                        \
    uint64_t _val;                                                            \
    if (readValue64BigEndian(bs, s, &_val) < 0)                               \
      e;                                                                      \
    *d = _val;                                                                \
  } while (0)

#define READ_BYTES(d, bs, s, e)                                               \
  do {                                                                        \
    uint64_t _val;                                                            \
    if (readValue64BigEndian(bs, s, &_val) < 0)                               \
      e;                                                                      \
    *d = _val;                                                                \
  } while (0)

#define READ_BITS(d, bs, s, e)                                                \
  do {                                                                        \
    uint64_t _val;                                                            \
    if (readBits64(bs, &_val, s) < 0)                                         \
      e;                                                                      \
    *d = _val;                                                                \
  } while (0)

#define SKIP_BYTES(bs, s, e)                                                  \
  do {                                                                        \
    if (skipBytes(bs, s) < 0)                                                 \
      e;                                                                      \
  } while (0)

#define SKIP_BITS(bs, s, e)                                                   \
  do {                                                                        \
    if (skipBits(bs, s) < 0)                                                  \
      e;                                                                      \
  } while (0)



static int _getRefClock(
  unsigned * RefClock,
  uint8_t RefClockCode
)
{
  static const unsigned values[] = {
    32000u,
    44100u,
    48000u
  };

  if (ARRAY_SIZE(values) <= RefClockCode)
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use ('refClockCode' == %u).\n",
      RefClockCode
    );

  *RefClock = values[RefClockCode];
  return 0;
}

static int _getFrameRateCode(
  float * frame_rate,
  bool * frame_rate_drop_frame,
  uint8_t TC_Frame_Rate
)
{
  static const float fr_values[8] = {
    -1.f,
    24000.f / 1001.f,
    24.f,
    25.f,
    30000.f / 1001.f,
    30000.f / 1001.f,
    30.f,
    30.f
  };
  static const bool fr_drop_frame[8] = {
    false,
    false,
    false,
    false,
    true,
    false,
    true,
    false
  };

  uint8_t suffix = TC_Frame_Rate & 0xF;
  if (ARRAY_SIZE(fr_values) <= suffix)
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use ('TC_Frame_Rate' == 0x%02" PRIX8 ").\n",
      TC_Frame_Rate
    );

  *frame_rate            = fr_values[suffix];
  *frame_rate_drop_frame = fr_drop_frame[suffix];
  return 0;
}

static int _decodeDtshdHeaderChunk(
  BitstreamReaderPtr file,
  DtshdFileHeaderChunk * param
)
{
  assert(NULL != param);

  /* [v64 'DTSHDHDR' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(chunk_id == DTS_HD_DTSHDHDR);

  LIBBLU_DTS_DEBUG_DTSHD(
    " DTSHDHDR chunk identifier: 'DTSHDHDR' (0x4454534844484452).\n"
  );

  /* [u64 Hdr_Byte_Size] */
  READ_BYTES(&param->Hdr_Byte_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Header chunk size (Hdr_Byte_Size): %" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Hdr_Byte_Size,
    param->Hdr_Byte_Size
  );

  if (param->Hdr_Byte_Size < DTSHD_DTSHDHDR_SIZE || param->Hdr_Byte_Size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file header chunk size "
      "('Hdr_Byte_Size' == 0x%016" PRIX64 ").\n",
      param->Hdr_Byte_Size
    );

  /* [u32 Hdr_Version] */
  READ_BYTES(&param->Hdr_Version, file, 4, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Header version (Hdr_Version): %u.\n",
    param->Hdr_Version
  );

  if (DTS_HD_HEADER_SUPPORTED_VER != param->Hdr_Version)
    LIBBLU_DTS_ERROR_RETURN(
      "Unsupported DTS-HD file header version (%u).\n",
      param->Hdr_Version
    );

  /** [v40 Time_Code]
   * -> u2  - RefClockCode
   * -> v6  - reserved
   * -> u32 - TimeStamp
   */
  uint64_t Time_Code;
  READ_BYTES(&Time_Code, file, 5, return -1);
  param->RefClockCode = Time_Code >> 38;
  param->TimeStamp    = Time_Code & 0xFFFFFFFF;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Time code informations (Time_Code): 0x%010" PRIX64 ".\n",
    Time_Code
  );

  unsigned RefClock;
  if (_getRefClock(&RefClock, param->RefClockCode) < 0)
    return -1;

  LIBBLU_DTS_DEBUG_DTSHD(
    "  Reference clock (RefClockCode): %u Hz (0x%" PRIX8 ").\n",
    RefClock,
    param->RefClockCode
  );

  LIBBLU_DTS_DEBUG_DTSHD(
    "  Timestamp (TimeStamp): %u samples (0x%x).\n",
    param->TimeStamp,
    param->TimeStamp
  );

  /* [v8 TC_Frame_Rate] */
  READ_BYTES(&param->TC_Frame_Rate, file, 1, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Timecode frame-rate code (TC_Frame_Rate): 0x%02" PRIX8 ".\n",
    param->TC_Frame_Rate
  );

  float frame_rate;
  bool fr_drop_frame;
  if (_getFrameRateCode(&frame_rate, &fr_drop_frame, param->TC_Frame_Rate) < 0)
    return -1;

  if (frame_rate < 0)
    LIBBLU_DTS_DEBUG_DTSHD(" => Timecode rate: NOT_INDICATED (0x0).\n");
  else
    LIBBLU_DTS_DEBUG_DTSHD(
      " => Timecode rate: %.3f FPS %s(0x%02" PRIX8 ").\n",
      frame_rate,
      fr_drop_frame ? "Drop " : "",
      param->TC_Frame_Rate
    );

  /** [v16 Bitw_Stream_Metadata]
   * -> v11: reserved
   * -> b1 : Presence of an extension substream(s).
   * -> b1 : Presence of a core substream.
   * -> b1 : Navigation table presence.
   * -> b1 : Peak Bit-Rate Smoothing (PBRS) indicator.
   * -> b1 : Constant Bit-Rate or Variable Bit-Rate descriptor.
   */
  READ_BYTES(&param->Bitw_Stream_Metadata, file, 2, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Bitstream Metadata (Bitw_Stream_Metadata): 0x%04" PRIX16 ".\n",
    param->Bitw_Stream_Metadata
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Bit-rate mode descriptor: %s (0b%x).\n",
    (param->Bitw_Stream_Metadata & DTSHD_BSM__IS_VBR) ? "VBR" : "CBR",
    param->Bitw_Stream_Metadata & DTSHD_BSM__IS_VBR
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Peak Bit-rate Smoothing (PBRS) performed: %s (0b%x);\n",
    BOOL_STR(param->Bitw_Stream_Metadata & DTSHD_BSM__PBRS_PERFORMED),
    param->Bitw_Stream_Metadata & DTSHD_BSM__PBRS_PERFORMED
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Navigation Table Indicator: %s (0b%x);\n",
    BOOL_STR(param->Bitw_Stream_Metadata & DTSHD_BSM__NAVI_EMBEDDED),
    param->Bitw_Stream_Metadata & DTSHD_BSM__NAVI_EMBEDDED
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Core Substream: %s (0b%x);\n",
    BOOL_PRESENCE(param->Bitw_Stream_Metadata & DTSHD_BSM__CORE_PRESENT),
    param->Bitw_Stream_Metadata & DTSHD_BSM__CORE_PRESENT
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Extension Substream: %s (0b%x);\n",
    BOOL_PRESENCE(param->Bitw_Stream_Metadata & DTSHD_BSM__EXTSS_PRESENT),
    param->Bitw_Stream_Metadata & DTSHD_BSM__EXTSS_PRESENT
  );

  /* [u8 Num_Audio_Presentations] */
  READ_BYTES(&param->Num_Audio_Presentations, file, 1, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Number of Audio Presentations (Num_Audio_Presentations): "
    "%u presentation(s) (0x%02" PRIX8 ").\n",
    param->Num_Audio_Presentations,
    param->Num_Audio_Presentations
  );

  /* [u8 Number_Of_Ext_Sub_Streams] */
  READ_BYTES(&param->Number_Of_Ext_Sub_Streams, file, 1, return -1);

  if (param->Bitw_Stream_Metadata & DTSHD_BSM__EXTSS_PRESENT)
    param->Num_ExSS = param->Number_Of_Ext_Sub_Streams + 1;
  else
    param->Num_ExSS = 0;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Number of Extension Substreams (Number_Of_Ext_Sub_Streams): "
    "%u (0x%02" PRIX8 ").\n",
    param->Number_Of_Ext_Sub_Streams,
    param->Number_Of_Ext_Sub_Streams
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Actual Number of Extension Substreams (Num_ExSS): %u substream(s).\n",
    param->Num_ExSS
  );

  /* [vn Reserved/Dword_Align] */
  SKIP_BYTES(file, param->Hdr_Byte_Size - DTSHD_DTSHDHDR_SIZE, return -1);

  return 0;
}

static int _decodeDtshdString(
  BitstreamReaderPtr file,
  uint64_t max_size,
  char ** dst_ptr,
  uint64_t * res_size
)
{
  uint64_t buffer_size = 0, current_size = 0;
  uint8_t * buffer = NULL;

  uint8_t byte;
  do {
    if (max_size <= current_size)
      LIBBLU_DTS_ERROR_RETURN("DTS-HD file header string is not terminated.\n");

    READ_BYTES(&byte, file, 1, return -1);

    if (buffer_size <= current_size) {
      /* Realloc buffer */
      if (0 == (buffer_size = GROW_ALLOCATION(buffer_size, 16u)))
        LIBBLU_DTS_ERROR_RETURN("DTS-HD file header string size overflow.\n");
      uint8_t * new_buffer = realloc(buffer, buffer_size);
      if (NULL == new_buffer)
        LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");
      buffer = new_buffer;
    }

    buffer[current_size++] = byte;
  } while (byte);

  *dst_ptr  = (char *) buffer;
  *res_size = current_size;
  return 0;
}

static int _decodeDtshdFileInfoChunk(
  BitstreamReaderPtr file,
  DtshdFileInfo * param
)
{

  /* [v64 'FILEINFO' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(chunk_id == DTS_HD_FILEINFO);

  LIBBLU_DTS_DEBUG_DTSHD(
    " FILEINFO chunk identifier: 'FILEINFO' (0x46494C45494E464F).\n"
  );

  /* [u64 FILEINFO_Text_Byte_Size] */
  READ_BYTES(&param->FILEINFO_Text_Byte_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " FILEINFO chunk size (FILEINFO_Text_Byte_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->FILEINFO_Text_Byte_Size,
    param->FILEINFO_Text_Byte_Size
  );

  if (param->FILEINFO_Text_Byte_Size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file FILEINFO chunk size "
      "('FILEINFO_Text_Byte_Size' == 0x%016" PRIX64 ").\n",
      param->FILEINFO_Text_Byte_Size
    );

  /* [vn string] */
  uint64_t string_size;
  uint64_t max_string_size = param->FILEINFO_Text_Byte_Size;
  if (_decodeDtshdString(file, max_string_size, &param->text, &string_size) < 0)
    LIBBLU_DTS_ERROR_RETURN("Invalid DTS-HD file FILEINFO string.\n");

  LIBBLU_DTS_DEBUG_DTSHD(
    " Information Text string: '%s'.\n",
    param->text
  );

  /* [vn Reserved/Dword_Align] */
  SKIP_BYTES(file, param->FILEINFO_Text_Byte_Size - string_size, return -1);

  return 0;
}

static int _decodeDtshdStreamDataChunk(
  BitstreamReaderPtr file,
  DtshdStreamData * param,
  bool in_STRMDATA,
  uint64_t off_STRMDATA
)
{

  if (in_STRMDATA) {
    if (param->Stream_Data_Byte_Size <= off_STRMDATA) {
      if (param->Stream_Data_Byte_Size < off_STRMDATA)
        LIBBLU_DTS_ERROR_RETURN(
          "DTS-HD file chunk length error (longer than expected).\n"
        );

      /* Align to DWORD (32 bits) boundary */
      if (paddingBoundary(file, 4) < 0)
        return -1;
      return 0; // End of STRMDATA chunk, stop reading DTS audio frames.
    }

    return 1; // Middle of STRMDATA chunk, read DTS audio frames.
  }

  /* [v64 'STRMDATA' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_STRMDATA == chunk_id);

  LIBBLU_DTS_DEBUG_DTSHD(
    " STRMDATA chunk identifier: 'STRMDATA' (0x5354524D44415441).\n"
  );

  /* [u64 Stream_Data_Byte_Size] */
  READ_BYTES(&param->Stream_Data_Byte_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " STRMDATA chunk size (Stream_Data_Byte_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Stream_Data_Byte_Size,
    param->Stream_Data_Byte_Size
  );

  if (0x00 == param->Stream_Data_Byte_Size)
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (unexpected empty stream data chunk).\n"
    );

  /* [vn DTS-HD Encoded Bitstream] */
  /* Parsed outside of function. */

  return 1;
}

static int _decodeDtshdCoreSubStreamMetaChunk(
  BitstreamReaderPtr file,
  DtshdCoreSubStrmMeta * param
)
{

  /* [v64 'CORESSMD' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_CORESSMD == chunk_id);

  LIBBLU_DTS_DEBUG_DTSHD(
    " CORESSMD chunk identifier: 'CORESSMD' (0x434F524553534D44).\n"
  );

  /* [u64 Core_Ss_Md_Bytes_Size] */
  READ_BYTES(&param->Core_Ss_Md_Bytes_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " CORESSMD chunk size (Core_Ss_Md_Bytes_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Core_Ss_Md_Bytes_Size,
    param->Core_Ss_Md_Bytes_Size
  );

  unsigned chunk_size = param->Core_Ss_Md_Bytes_Size;
  if (chunk_size < DTSHD_CORESSMD_SIZE || chunk_size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file Core Substream Metadata chunk size "
      "('Core_Ss_Md_Bytes_Size' == 0x%016" PRIX64 ").\n",
      param->Core_Ss_Md_Bytes_Size
    );

  /* [u24 Core_Ss_Max_Sample_Rate_Hz] */
  READ_BYTES(&param->Core_Ss_Max_Sample_Rate_Hz, file, 3, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Core Sub-Stream Maximum Sampling Rate (Core_Ss_Max_Sample_Rate_Hz): "
    "%" PRIu32 " Hz (0x%06" PRIX32 ").\n",
    param->Core_Ss_Max_Sample_Rate_Hz,
    param->Core_Ss_Max_Sample_Rate_Hz
  );

  /* [u16 Core_Ss_Bit_Rate_Kbps] */
  READ_BYTES(&param->Core_Ss_Bit_Rate_Kbps, file, 2, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Core Sub-Stream Bit-Rate (Core_Ss_Bit_Rate_Kbps): "
    "%" PRIu16 " Kbps (0x%04" PRIX16 ").\n",
    param->Core_Ss_Bit_Rate_Kbps,
    param->Core_Ss_Bit_Rate_Kbps
  );

  /* [v16 Core_Ss_Channel_Mask] */
  READ_BYTES(&param->Core_Ss_Channel_Mask, file, 2, return -1);

  char channel_mask_str[DCA_CHMASK_STR_BUFSIZE];
  unsigned channel_mask_nb_ch = buildDcaExtChMaskString(
    channel_mask_str,
    param->Core_Ss_Channel_Mask
  );

  LIBBLU_DTS_DEBUG_DTSHD(
    " Core Sub-Stream Channels Mask (Core_Ss_Channel_Mask): "
    "%s (%u channels, 0x%04" PRIX16 ").\n",
    channel_mask_str,
    channel_mask_nb_ch,
    param->Core_Ss_Channel_Mask
  );

  if (0 == channel_mask_nb_ch)
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file empty Core Sub-Stream channel mask.\n"
    );

  /* [u32 Core_Ss_Frame_Payload_In_Bytes] */
  READ_BYTES(&param->Core_Ss_Frame_Payload_In_Bytes, file, 4, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Core Sub-Stream Frame Payload size (Core_Ss_Frame_Payload_In_Bytes): "
    "%" PRIu32 " bytes (0x%08" PRIX32 ").\n",
    param->Core_Ss_Frame_Payload_In_Bytes,
    param->Core_Ss_Frame_Payload_In_Bytes
  );

  /* [vn reserved/padding] */
  SKIP_BYTES(file, param->Core_Ss_Md_Bytes_Size - DTSHD_CORESSMD_SIZE, return -1);

  return 0;
}

static int _decodeDtshdExtSubStreamMetaChunk(
  BitstreamReaderPtr file,
  DtshdExtSubStrmMeta * param,
  bool is_VBR
)
{

  /* [v64 'EXTSS_MD' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_EXTSS_MD == chunk_id);

  LIBBLU_DTS_DEBUG_DTSHD(
    " EXTSS_MD chunk identifier: 'EXTSS_MD' (0x45585453535F4D44).\n"
  );

  /* [u64 Ext_Ss_Md_Bytes_Size] */
  READ_BYTES(&param->Ext_Ss_Md_Bytes_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " EXTSS_MD chunk size (Ext_Ss_Md_Bytes_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Ext_Ss_Md_Bytes_Size,
    param->Ext_Ss_Md_Bytes_Size
  );

  unsigned chunk_size = param->Ext_Ss_Md_Bytes_Size;
  if (chunk_size < (7 + (unsigned) is_VBR) || chunk_size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file Extension Substream Metadata chunk size "
      "('Ext_Ss_Md_Bytes_Size' == 0x%016" PRIX64 ").\n",
      param->Ext_Ss_Md_Bytes_Size
    );
  unsigned remaining_size = chunk_size - (7 + (unsigned) is_VBR);

  /* [u24 Ext_Ss_Avg_Bit_Rate_Kbps] */
  READ_BYTES(&param->Ext_Ss_Avg_Bit_Rate_Kbps, file, 3, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Extension Substreams Average Bit-Rate (Ext_Ss_Avg_Bit_Rate_Kbps): "
    "%" PRIu32 " Kbps (0x%06" PRIX32 ").\n",
    param->Ext_Ss_Avg_Bit_Rate_Kbps,
    param->Ext_Ss_Avg_Bit_Rate_Kbps
  );

  if (is_VBR) {
    /* [u24 Ext_Ss_Peak_Bit_Rate_Kbps] */
    READ_BYTES(&param->vbr.Ext_Ss_Peak_Bit_Rate_Kbps, file, 3, return -1);

    LIBBLU_DTS_DEBUG_DTSHD(
      " Extension Substreams Peak Bit-Rate (Ext_Ss_Avg_Bit_Rate_Kbps): "
      "%" PRIu32 " Kbps (0x%06" PRIX32 ").\n",
      param->vbr.Ext_Ss_Peak_Bit_Rate_Kbps,
      param->vbr.Ext_Ss_Peak_Bit_Rate_Kbps
    );

    /* [u16 Pbr_Smooth_Buff_Size_Kb] */
    READ_BYTES(&param->vbr.Pbr_Smooth_Buff_Size_Kb, file, 2, return -1);

    LIBBLU_DTS_DEBUG_DTSHD(
      " Peak Bit-Rate Smoothing Buffer Size (Pbr_Smooth_Buff_Size_Kb): "
      "%" PRIu16 " KiB (0x%04" PRIX16 ").\n",
      param->vbr.Pbr_Smooth_Buff_Size_Kb,
      param->vbr.Pbr_Smooth_Buff_Size_Kb
    );
  }
  else {
    /* [u32 Ext_Ss_Frame_Payload_In_Bytes] */
    READ_BYTES(&param->cbr.Ext_Ss_Frame_Payload_In_Bytes, file, 4, return -1);

    LIBBLU_DTS_DEBUG_DTSHD(
      " Extension Substreams Frame Payload (Ext_Ss_Frame_Payload_In_Bytes): "
      "%" PRIu32 " bytes (0x%06" PRIX32 ").\n",
      param->cbr.Ext_Ss_Frame_Payload_In_Bytes,
      param->cbr.Ext_Ss_Frame_Payload_In_Bytes
    );
  }

  /* [vn reserved/padding] */
  SKIP_BYTES(file, remaining_size, return -1);

  return 0;
}

static int _decodeDtshdAudioPresHeaderMetaChunk(
  BitstreamReaderPtr file,
  DtshdAudioPresPropHeaderMeta * param
)
{

  /* [v64 'AUPR-HDR' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_AUPR_HDR == chunk_id);

  LIBBLU_DTS_DEBUG_DTSHD(
    " AUPR-HDR chunk identifier: 'AUPR-HDR' (0x415550522D484452).\n"
  );

  /* [u64 Audio_Pres_Hdr_Md_Bytes_Size] */
  READ_BYTES(&param->Audio_Pres_Hdr_Md_Bytes_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " AUPR-HDR chunk size (Audio_Pres_Hdr_Md_Bytes_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Audio_Pres_Hdr_Md_Bytes_Size,
    param->Audio_Pres_Hdr_Md_Bytes_Size
  );

  unsigned remaining_size = param->Audio_Pres_Hdr_Md_Bytes_Size;
  if (remaining_size < DTSHD_AUPR_HDR_MINSIZE || remaining_size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file Audio Presentation Header Metadata chunk size "
      "('Audio_Pres_Hdr_Md_Bytes_Size' == 0x%016" PRIX64 ").\n",
      param->Audio_Pres_Hdr_Md_Bytes_Size
    );
  remaining_size -= DTSHD_AUPR_HDR_MINSIZE;

  /* [u8 Audio_Pres_Index] */
  READ_BYTES(&param->Audio_Pres_Index, file, 1, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Audio Presentation Index (Audio_Pres_Index): 0x%02" PRIX8 ".\n",
    param->Audio_Pres_Index,
    param->Audio_Pres_Index
  );

 /** [v16 Bitw_Aupres_Metadata]
  * -> v12: reserved
  * -> b1 : Presence of Low Bit-Rate (LBR) coding component.
  * -> b1 : Presence of Lossless coding component.
  * -> b1 : Location of backward compatible Core audio coding component.
  * -> b1 : Presence of backward compatible Core audio coding component.
  */
  READ_BYTES(&param->Bitw_Aupres_Metadata, file, 2, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Audio Presentation Metadata (Bitw_Aupres_Metadata): 0x%04" PRIX16 ".\n",
    param->Bitw_Aupres_Metadata
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Presence of Low Bit-Rate (LBR) coding component: %s (0b%x).\n",
    BOOL_STR(param->Bitw_Aupres_Metadata & DTSHD_BAM__LBR_PRESENT),
    param->Bitw_Aupres_Metadata & DTSHD_BAM__LBR_PRESENT
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Presence of Lossless coding component: %s (0b%x);\n",
    BOOL_STR(param->Bitw_Aupres_Metadata & DTSHD_BAM__LOSSLESS_PRESENT),
    param->Bitw_Aupres_Metadata & DTSHD_BAM__LOSSLESS_PRESENT
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Location of backward compatible Core audio coding component: %s (0b%x);\n",
    (param->Bitw_Aupres_Metadata & DTSHD_BAM__CORE_IN_EXTSS)
      ? "Extension substream" : "Core substream",
    param->Bitw_Aupres_Metadata & DTSHD_BAM__CORE_IN_EXTSS
  );
  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Presence of backward compatible Core audio coding component: %s (0b%x);\n",
    BOOL_STR(param->Bitw_Aupres_Metadata & DTSHD_BAM__CORE_PRESENT),
    param->Bitw_Aupres_Metadata & DTSHD_BAM__CORE_PRESENT
  );

  /* [u24 Max_Sample_Rate_Hz] */
  READ_BYTES(&param->Max_Sample_Rate_Hz, file, 3, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Max audio Sampling Rate (Max_Sample_Rate_Hz): %u Hz (0x%06" PRIX32 ").\n",
    param->Max_Sample_Rate_Hz,
    param->Max_Sample_Rate_Hz
  );

  /* [u32 Num_Frames_Total] */
  READ_BYTES(&param->Num_Frames_Total, file, 4, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Total Number of audio Frames (Num_Frames_Total): "
    "%u frame(s) (0x%08" PRIX32 ").\n",
    param->Num_Frames_Total,
    param->Num_Frames_Total
  );

  /* [u16 Samples_Per_Frame_At_Max_Fs] */
  READ_BYTES(&param->Samples_Per_Frame_At_Max_Fs, file, 2, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Samples Per Frame at Maximum Sample Frequency "
    "(Samples_Per_Frame_At_Max_Fs): %u sample(s) (0x%04" PRIX16 ").\n",
    param->Samples_Per_Frame_At_Max_Fs,
    param->Samples_Per_Frame_At_Max_Fs
  );

  /* [u40 Num_Samples_Orig_Audio_At_Max_Fs] */
  READ_BYTES(&param->Num_Samples_Orig_Audio_At_Max_Fs, file, 5, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Number of Samples in Original Audio at Maximum Sample Frequency "
    "(Num_Samples_Orig_Audio_At_Max_Fs): "
    "%" PRIu64 " sample(s) (0x%010" PRIX64 ").\n",
    param->Num_Samples_Orig_Audio_At_Max_Fs,
    param->Num_Samples_Orig_Audio_At_Max_Fs
  );

  /* [v16 Channel_Mask] */
  READ_BYTES(&param->Channel_Mask, file, 2, return -1);

  char channel_mask_str[DCA_CHMASK_STR_BUFSIZE];
  unsigned channel_mask_nb_ch = buildDcaExtChMaskString(
    channel_mask_str,
    param->Channel_Mask
  );

  LIBBLU_DTS_DEBUG_DTSHD(
    " Audio Channels loudspeakers position Mask (Channel_Mask): "
    "%s (%u channels, 0x%04" PRIX16 ").\n",
    channel_mask_str,
    channel_mask_nb_ch,
    param->Channel_Mask
  );

  if (0 == channel_mask_nb_ch)
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file empty Audio Presentation Channel mask.\n"
    );

  /* [u16 Codec_Delay_At_Max_Fs] */
  READ_BYTES(&param->Codec_Delay_At_Max_Fs, file, 2, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Encoder delay at Maximum Sample Frequency (Codec_Delay_At_Max_Fs): "
    "%u sample(s) (0x%04" PRIX16 ").\n",
    param->Codec_Delay_At_Max_Fs,
    param->Codec_Delay_At_Max_Fs
  );

  if (
    param->Bitw_Aupres_Metadata & DTSHD_BAM__CORE_PRESENT
    && param->Bitw_Aupres_Metadata & DTSHD_BAM__CORE_IN_EXTSS
  ) {
    if (remaining_size < DTSHD_AUPR_HDR_CORE_SIZE)
      LIBBLU_DTS_ERROR_RETURN(
        "Invalid DTS-HD file Audio Presentation Header Metadata chunk size "
        "('Audio_Pres_Hdr_Md_Bytes_Size' == 0x%016" PRIX64 ").\n",
        param->Audio_Pres_Hdr_Md_Bytes_Size
      );
    remaining_size -= DTSHD_AUPR_HDR_CORE_SIZE;

    /* [u24 BC_Core_Max_Sample_Rate_Hz] */
    READ_BYTES(&param->BC_Core_Max_Sample_Rate_Hz, file, 3, return -1);

    LIBBLU_DTS_DEBUG_DTSHD(
      " Backward-Compatible Core Maximum Sample Rate "
      "(BC_Core_Max_Sample_Rate_Hz): %u Hz (0x%06" PRIX32 ").\n",
      param->BC_Core_Max_Sample_Rate_Hz,
      param->BC_Core_Max_Sample_Rate_Hz
    );

    /* [u16 BC_Core_Bit_Rate_Kbps] */
    READ_BYTES(&param->BC_Core_Bit_Rate_Kbps, file, 2, return -1);

    LIBBLU_DTS_DEBUG_DTSHD(
      " Backward-Compatible Core Bit-Rate "
      "(BC_Core_Bit_Rate_Kbps): %u Kbps (0x%04" PRIX16 ").\n",
      param->BC_Core_Bit_Rate_Kbps,
      param->BC_Core_Bit_Rate_Kbps
    );

    /* [u16 BC_Core_Channel_Mask] */
    READ_BYTES(&param->BC_Core_Channel_Mask, file, 2, return -1);

    char channel_mask_str[DCA_CHMASK_STR_BUFSIZE];
    unsigned channel_mask_nb_ch = buildDcaExtChMaskString(
      channel_mask_str,
      param->BC_Core_Channel_Mask
    );

    LIBBLU_DTS_DEBUG_DTSHD(
      " Backward-Compatible Core Audio Channels loudspeakers position Mask "
      "(BC_Core_Channel_Mask): %s (%u channels, 0x%04" PRIX16 ").\n",
      channel_mask_str,
      channel_mask_nb_ch,
      param->BC_Core_Channel_Mask
    );

    if (0 == channel_mask_nb_ch)
      LIBBLU_DTS_ERROR_RETURN(
        "DTS-HD file empty Audio Presentation "
        "Backward-Compatible Core Channel mask.\n"
      );
  }

  if (param->Bitw_Aupres_Metadata & DTSHD_BAM__LOSSLESS_PRESENT) {
    if (remaining_size < DTSHD_AUPR_HDR_XLL_SIZE)
      LIBBLU_DTS_ERROR_RETURN(
        "Invalid DTS-HD file Audio Presentation Header Metadata chunk size "
        "('Audio_Pres_Hdr_Md_Bytes_Size' == 0x%016" PRIX64 ").\n",
        param->Audio_Pres_Hdr_Md_Bytes_Size
      );
    remaining_size -= DTSHD_AUPR_HDR_XLL_SIZE;

    /* [u8 LSB_Trim_Percent] */
    READ_BYTES(&param->LSB_Trim_Percent, file, 1, return -1);

    LIBBLU_DTS_DEBUG_DTSHD(
      " Lossless Extension LSB trim (LSB_Trim_Percent): "
      "%u (0x%02" PRIX8 ").\n",
      param->LSB_Trim_Percent,
      param->LSB_Trim_Percent
    );
  }

  /* [vn reserved/padding] */
  SKIP_BYTES(file, remaining_size, return -1);

  return 0;
}

static int _decodeDtshdAudioPresInfoChunk(
  BitstreamReaderPtr file,
  DtshdAudioPresText * param
)
{

  /* [v64 'AUPRINFO' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_AUPRINFO == chunk_id);

  LIBBLU_DTS_DEBUG_DTSHD(
    " AUPRINFO chunk identifier: 'AUPRINFO' (0x41555052494E464F).\n"
  );

  /* [u64 Audio_Pres_Info_Text_Byte_Size] */
  READ_BYTES(&param->Audio_Pres_Info_Text_Byte_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " AUPRINFO chunk size (Audio_Pres_Info_Text_Byte_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Audio_Pres_Info_Text_Byte_Size,
    param->Audio_Pres_Info_Text_Byte_Size
  );

  unsigned remaining_size = param->Audio_Pres_Info_Text_Byte_Size;
  if (remaining_size < DTSHD_AUPRINFO_MINSIZE || remaining_size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file Audio Presentation Info Text chunk size "
      "('Audio_Pres_Info_Text_Byte_Size' == 0x%016" PRIX64 ").\n",
      param->Audio_Pres_Info_Text_Byte_Size
    );
  remaining_size -= DTSHD_AUPRINFO_MINSIZE;

  /* [u8 Audio_Pres_Info_Text_Index] */
  READ_BYTES(&param->Audio_Pres_Info_Text_Index, file, 1, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Information Text associated Audio Presentation Index "
    "(Audio_Pres_Info_Text_Index): %u (0x%02" PRIX8 ").\n",
    param->Audio_Pres_Info_Text_Index,
    param->Audio_Pres_Info_Text_Index
  );

  /* [vn string] */
  uint64_t string_size;
  if (_decodeDtshdString(file, remaining_size, &param->text, &string_size) < 0)
    LIBBLU_DTS_ERROR_RETURN("Invalid DTS-HD file FILEINFO string.\n");

  LIBBLU_DTS_DEBUG_DTSHD(
    " Information Text string: '%s'.\n",
    param->text
  );

  /* [vn Reserved/Dword_Align] */
  SKIP_BYTES(file, remaining_size - string_size, return -1);

  return 0;
}

int decodeDtshdNavigationMetaChunk(
  BitstreamReaderPtr file,
  DtshdNavMeta * param
)
{

  /* [v64 'NAVI-TBL' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_NAVI_TBL == chunk_id);

  LIBBLU_DTS_DEBUG_DTSHD(
    " AUPRINFO chunk identifier: 'NAVI-TBL' (0x4E4156492D54424C).\n"
  );

  /* [u64 Navi_Tbl_Md_Bytes_Size] */
  READ_BYTES(&param->Navi_Tbl_Md_Bytes_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " NAVI-TBL chunk size (Navi_Tbl_Md_Bytes_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Navi_Tbl_Md_Bytes_Size,
    param->Navi_Tbl_Md_Bytes_Size
  );

  if (!param->Navi_Tbl_Md_Bytes_Size || param->Navi_Tbl_Md_Bytes_Size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file Navigation Table chunk size "
      "('Navi_Tbl_Md_Bytes_Size' == 0x%016" PRIX64 ").\n",
      param->Navi_Tbl_Md_Bytes_Size
    );

  /* [vn Reserved/Dword_Align] */
  SKIP_BYTES(file, param->Navi_Tbl_Md_Bytes_Size, return -1);

  return 0;
}

int decodeDtshdTimecodeChunk(
  BitstreamReaderPtr file,
  DtshdTimecode * param
)
{

  /* [v64 'TIMECODE' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_TIMECODE == chunk_id);

  LIBBLU_DTS_DEBUG_DTSHD(
    " TIMECODE chunk identifier: 'TIMECODE' (0x54494D45434F4445).\n"
  );

  /* [u64 Timecode_Data_Byte_Size] */
  READ_BYTES(&param->Timecode_Data_Byte_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " TIMECODE chunk size (Timecode_Data_Byte_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Timecode_Data_Byte_Size,
    param->Timecode_Data_Byte_Size
  );

  uint64_t chunk_size = param->Timecode_Data_Byte_Size;
  if (chunk_size < DTSHD_TIMECODE_SIZE || chunk_size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file Timecode Data chunk size "
      "('Timecode_Data_Byte_Size' == 0x%016" PRIX64 ").\n",
      param->Timecode_Data_Byte_Size
    );

  /* [u32 Timecode_Clock] */
  READ_BYTES(&param->Timecode_Clock, file, 4, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Timecode clock (Timecode_Clock): %u (0x%08" PRIX32 ").\n",
    param->Timecode_Clock,
    param->Timecode_Clock
  );

  /* [u8 Timecode_Frame_Rate] */
  READ_BYTES(&param->Timecode_Frame_Rate, file, 1, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Timecode frame-rate (Timecode_Frame_Rate): %u (0x%02" PRIX8 ").\n",
    param->Timecode_Frame_Rate,
    param->Timecode_Frame_Rate
  );

  /* [u64 Start_Samples_Since_Midnight] */
  READ_BYTES(&param->Start_Samples_Since_Midnight, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Start samples since midnight (Start_Samples_Since_Midnight): "
    "%" PRIu64 " (0x%016" PRIX64 ").\n",
    param->Start_Samples_Since_Midnight,
    param->Start_Samples_Since_Midnight
  );

  /* [u32 Start_Residual] */
  READ_BYTES(&param->Start_Residual, file, 4, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Start residual (Start_Residual): %" PRIu32 " (0x%08" PRIX32 ").\n",
    param->Start_Residual,
    param->Start_Residual
  );

  /* [u64 Reference_Samples_Since_Midnight] */
  READ_BYTES(&param->Reference_Samples_Since_Midnight, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Reference Samples Since Midnight (Reference_Samples_Since_Midnight): "
    "%" PRIu64 " (0x%08" PRIX64 ").\n",
    param->Reference_Samples_Since_Midnight,
    param->Reference_Samples_Since_Midnight
  );

  /* [u32 Reference_Residual] */
  READ_BYTES(&param->Reference_Residual, file, 4, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Reference Samples Since Midnight (Reference_Residual): "
    "%" PRIu32 " (0x%08" PRIX32 ").\n",
    param->Reference_Residual,
    param->Reference_Residual
  );

  /* [vn Reserved/Dword_Align] */
  SKIP_BYTES(file, param->Timecode_Data_Byte_Size - DTSHD_TIMECODE_SIZE, return -1);

  return 0;
}

int decodeDtshdBuildVerChunk(
  BitstreamReaderPtr file,
  DtshdBuildVer * param
)
{

  /* [v64 'BUILDVER' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_BUILDVER == chunk_id);

  /* [u64 BuildVer_Data_Byte_Size] */
  READ_BYTES(&param->BuildVer_Data_Byte_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " BUILDVER chunk size (BuildVer_Data_Byte_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->BuildVer_Data_Byte_Size,
    param->BuildVer_Data_Byte_Size
  );

  uint64_t chunk_size = param->BuildVer_Data_Byte_Size;
  if (chunk_size < DTSHD_TIMECODE_SIZE || chunk_size & 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS-HD file Build Version chunk size "
      "('BuildVer_Data_Byte_Size' == 0x%016" PRIX64 ").\n",
      param->BuildVer_Data_Byte_Size
    );

  /* [vn string] */
  uint64_t string_size;
  if (_decodeDtshdString(file, chunk_size, &param->text, &string_size) < 0)
    LIBBLU_DTS_ERROR_RETURN("Invalid DTS-HD file FILEINFO string.\n");

  LIBBLU_DTS_DEBUG_DTSHD(
    " Information Text string: '%s'.\n",
    param->text
  );

  /* [vn Reserved/Dword_Align] */
  SKIP_BYTES(file, chunk_size - string_size, return -1);

  return 0;
}

int decodeDtshdBlackoutChunk(
  BitstreamReaderPtr file,
  DtshdBlackout * param
)
{

  /* [v64 'BLACKOUT' ASCII identifier] */
  uint64_t chunk_id;
  READ_BYTES_BE(&chunk_id, file, 8, return -1);
  assert(DTS_HD_BLACKOUT == chunk_id);

  /* [u64 Blackout_Data_Byte_Size] */
  READ_BYTES(&param->Blackout_Data_Byte_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " BLACKOUT chunk size (Blackout_Data_Byte_Size): "
    "%" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    param->Blackout_Data_Byte_Size,
    param->Blackout_Data_Byte_Size
  );

  /* [vn Blackout_Frame] */
  uint8_t * frame = (uint8_t *) malloc(param->Blackout_Data_Byte_Size);
  if (NULL == frame)
    LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

  if (readBytes(file, frame, param->Blackout_Data_Byte_Size) < 0)
    return -1;
  param->Blackout_Frame = frame;

  /* [vn Reserved/Dword_Align] */
  // TODO: Fix Blackout_Frame size.

  return 0;
}

static int _decodeDtshdUnknownChunk(
  BitstreamReaderPtr file
)
{

  /* [v64 Unknown ASCII identifier] */
  uint64_t Unk_Chunk_Id;
  READ_BYTES(&Unk_Chunk_Id, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Unknown chunk identifier: 0x016" PRIX64 ".\n",
    Unk_Chunk_Id
  );

  /* [u64 Unknown chunk size] */
  uint64_t Unk_Byte_Size;
  READ_BYTES(&Unk_Byte_Size, file, 8, return -1);

  LIBBLU_DTS_DEBUG_DTSHD(
    " Unknown chunk size: %" PRIu64 " bytes (0x%016" PRIX64 ").\n",
    Unk_Byte_Size,
    Unk_Byte_Size
  );

  /* [vn reserved/padding] */
  SKIP_BYTES(file, Unk_Byte_Size, return -1);

  return 0;
}

bool isDtshdFile(
  BitstreamReaderPtr file
)
{
  return DTS_HD_DTSHDHDR == nextUint64(file);
}

DtshdFileHandler * createDtshdFileHandler(
  void
)
{
  return (DtshdFileHandler *) calloc(
    1, sizeof(DtshdFileHandler)
  );
}

void destroyDtshdFileHandler(
  DtshdFileHandler * handle
)
{
  if (NULL == handle)
    return;

  if (handle->FILEINFO_count)
    free(handle->FILEINFO.text);
  if (handle->AUPRINFO_count)
    free(handle->AUPRINFO.text);
  if (handle->BUILDVER_count)
    free(handle->BUILDVER.text);
  if (handle->BLACKOUT_count)
    free(handle->BLACKOUT.Blackout_Frame);
  free(handle);
}

int decodeDtshdFileChunk(
  BitstreamReaderPtr file,
  DtshdFileHandler * handle,
  bool skip_checks
)
{
  assert(NULL != file);
  assert(NULL != handle);

  if (handle->in_STRMDATA) {
    int ret = _decodeDtshdStreamDataChunk(
      file,
      &handle->STRMDATA,
      true,
      handle->off_STRMDATA
    );

    if (ret == 0)
      handle->in_STRMDATA = false;
    return ret;
  }

  uint64_t magic = nextUint64(file);

  LIBBLU_DTS_DEBUG_DTSHD(
    "0x%08" PRIX64 " === %s (0x%016" PRIX64 ") ===\n",
    tellPos(file), _dtshdChunkIdStr(magic), magic
  );

  unsigned * presence_counter = NULL;
  int ret = 0;

  switch (magic) {
    case DTS_HD_DTSHDHDR:
      presence_counter = &handle->DTSHDHDR_count;
      ret = _decodeDtshdHeaderChunk(file, &handle->DTSHDHDR);
      break;

    case DTS_HD_FILEINFO:
      presence_counter = &handle->FILEINFO_count;
      ret = _decodeDtshdFileInfoChunk(file, &handle->FILEINFO);
      break;

    case DTS_HD_CORESSMD:
      presence_counter = &handle->CORESSMD_count;
      ret = _decodeDtshdCoreSubStreamMetaChunk(file, &handle->CORESSMD);
      break;

    case DTS_HD_EXTSS_MD:
      if (!handle->DTSHDHDR_count)
        LIBBLU_DTS_ERROR_RETURN(
          "Expect presence of DTS-HD file header before EXTSS_MD chunk.\n"
        );
      presence_counter = &handle->EXTSS_MD_count;
      ret = _decodeDtshdExtSubStreamMetaChunk(
        file,
        &handle->EXTSS_MD,
        handle->DTSHDHDR.Bitw_Stream_Metadata & DTSHD_BSM__IS_VBR
      );
      break;

    case DTS_HD_AUPR_HDR:
      presence_counter = &handle->AUPR_HDR_count;
      ret = _decodeDtshdAudioPresHeaderMetaChunk(
        file, &handle->AUPR_HDR
      );
      break;

    case DTS_HD_AUPRINFO:
      presence_counter = &handle->AUPRINFO_count;
      ret = _decodeDtshdAudioPresInfoChunk(file, &handle->AUPRINFO);
      break;

    case DTS_HD_NAVI_TBL:
      presence_counter = &handle->NAVI_TBL_count;
      ret = decodeDtshdNavigationMetaChunk(
        file, &handle->NAVI_TBL
      );
      break;

    case DTS_HD_STRMDATA:
      presence_counter = &handle->STRMDATA_count;
      ret = _decodeDtshdStreamDataChunk(
        file, &handle->STRMDATA, false, 0
      );
      handle->off_STRMDATA = 0;
      handle->in_STRMDATA = true;
      break;

    case DTS_HD_TIMECODE:
      presence_counter = &handle->TIMECODE_count;
      ret = decodeDtshdTimecodeChunk(
        file, &handle->TIMECODE
      );
      break;

    case DTS_HD_BUILDVER:
      presence_counter = &handle->BUILDVER_count;
      ret = decodeDtshdBuildVerChunk(
        file, &handle->BUILDVER
      );
      break;

    case DTS_HD_BLACKOUT:
      presence_counter = &handle->BLACKOUT_count;
      ret = decodeDtshdBlackoutChunk(
        file, &handle->BLACKOUT
      );
      break;

    case DTS_HD_BITSHVTB:
    case DTS_HD_BRANCHPT:
    default:
      ret = _decodeDtshdUnknownChunk(file);
      break;
  }

  if (ret < 0)
    return ret;

  if (!skip_checks && NULL != presence_counter) {
    /* No error */
    if (0 < ((*presence_counter)++))
      LIBBLU_DTS_ERROR_RETURN(
        "Presence of duplicated %s.\n",
        _dtshdChunkIdStr(magic)
      );
  }

  return 0;
}

