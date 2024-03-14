

#ifndef __LIBBLU_MUXER_CODECS__LCPM__DATA_H__
#define __LIBBLU_MUXER_CODECS__LCPM__DATA_H__

#include "lpcm_error.h"

typedef enum {
  WAVE_RIFF = 0x46464952,
  WAVE_FMT  = 0x20746D66,
  WAVE_JUNK = 0x4B4E554A,
  WAVE_DATA = 0x61746164
} WaveRIFFChunkId;

static inline const char *WaveRIFFChunkIdStr(
  WaveRIFFChunkId ckID
)
{
  switch (ckID) {
  case WAVE_RIFF:
    return "RIFF";
  case WAVE_FMT:
    return "fmt";
  case WAVE_JUNK:
    return "JUNK";
  case WAVE_DATA:
    return "DATA";
  }
  return "Unknown";
}

typedef struct {
  WaveRIFFChunkId ckID;
  uint32_t ckSize;
} RiffChunkHeader;

typedef enum {
  RIFF_WAVE_FORM = 0x45564157
} RiffFormType;

static inline const char *RiffFormTypeStr(
  RiffFormType formType
)
{
  switch (formType) {
  case RIFF_WAVE_FORM:
    return "Waveform Audio Format";
  }
  return "Unknown";
}

typedef struct {
  RiffChunkHeader header;
  RiffFormType type;
} RiffFormHeader;

#define WAVE_SUB_FORMAT_PCM                        \
  {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, \
   0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}

#endif