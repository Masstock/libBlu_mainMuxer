/** \~english
 * \file packetIdentifier.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief MPEG-TS Packet IDentifier (PID) values handling module.
 */

#ifndef __LIBBLU_MUXER__PACKET_IDENTIFIER_H__
#define __LIBBLU_MUXER__PACKET_IDENTIFIER_H__

#include "util/errorCodes.h"

typedef struct {
  unsigned primaryVideo;
  unsigned secondaryVideo;

  unsigned primaryAudio;
  unsigned secondaryAudio;

  unsigned pg;
  unsigned ig;
  unsigned txtSubtitles;
} LibbluNumberOfESTypes;

static inline void cleanLibbluNumberOfESTypes(
  LibbluNumberOfESTypes * dst
)
{
  *dst = (LibbluNumberOfESTypes) {
    0
  };
}

void setBDAVStdLibbluNumberOfESTypes(
  LibbluNumberOfESTypes * dst
);

typedef enum {
  /* H.222 System/Reserved PID values : */
  PAT_PID         = 0x0000,  /**< Program Association Table (PAT)
    reserved PID.                                                            */
  CAT_PID         = 0x0001,  /**< Conditional Access Table (CAT)
    reserved PID.                                                            */
  TSDT_PID        = 0x0002,  /**< Transport Stream Description Table (TSDT)
    reserved PID.                                                            */
  IPMP_CI_PID     = 0x0003,  /**< IPMP Control Information Table
    reserved PID.                                                            */

  /* BDAV Standard PID values : */
  PMT_PID         = 0x0100,  /**< BDAV Program Map Table (PMT)
    reserved PID.                                                            */
  SIT_PID         = 0x001F,  /**< BDAV Selection Information Table (SIT)
    reserved PID.                                                            */
  PCR_PID         = 0x1001,  /**< BDAV Program Clock Reference (PCR)
    carrying reserved PID.                                                   */

  PRIM_VIDEO_PID  = 0x1011,  /**< First BDAV Primary Video PID.              */
  PRIM_AUDIO_PID  = 0x1100,  /**< First BDAV Primary Audio PID.              */
  PG_PID          = 0x1200,  /**< First BDAV Presentation Graphics PID.      */
  IG_PID          = 0x1400,  /**< First BDAV Interactive Graphics PID.       */
  TXT_SUB_PID     = 0x1800,  /**< First BDAV Text Subtitles PID.             */
  SEC_AUDIO_PID   = 0x1A00,  /**< First BDAV Secondary Audio PID.            */
  SEC_VIDEO_PID   = 0x1B00,  /**< First BDAV Secondary Video PID.            */

  /* H.222 NULL packets reserved PID value : */
  NULL_PID     = 0x1FFF   /**< Null packets reserved PID.                    */
} LibbluStdPIDValue;

static inline bool isReservedPIDValue(
  uint16_t pid
)
{
  return
    pid < 0x0010
    || 0x1FFE < pid
  ;
}

static inline bool isPCRCarryingAllowedPIDValue(
  uint16_t pid
)
{
  return
    pid <= 0x0001
    || 0x0010 <= pid
    || pid <= 0x1FFE
  ;
}

typedef struct LibbluRegisteredPIDValuesEntry {
  struct LibbluRegisteredPIDValuesEntry * nextEntry;
  uint16_t pid;
} LibbluRegisteredPIDValuesEntry, *LibbluRegisteredPIDValuesEntryPtr;

typedef struct {
  LibbluRegisteredPIDValuesEntryPtr entries;
} LibbluRegisteredPIDValues;

static inline void initLibbluRegisteredPIDValues(
  LibbluRegisteredPIDValues * dst
)
{
  *dst = (LibbluRegisteredPIDValues) {0};
}

static inline void cleanLibbluRegisteredPIDValues(
  LibbluRegisteredPIDValues set
)
{
  LibbluRegisteredPIDValuesEntryPtr cur, next;

  for (cur = set.entries; NULL != cur; cur = next) {
    next = cur->nextEntry;
    free(cur);
  }
}

int insertLibbluRegisteredPIDValues(
  LibbluRegisteredPIDValues * set,
  uint16_t pid
);

typedef struct {
  LibbluNumberOfESTypes limits;
  LibbluNumberOfESTypes nbStreams;
  LibbluRegisteredPIDValues registeredValues;

  bool errorOnInvalidPIDRequest;
} LibbluPIDValues;

static inline void initLibbluPIDValues(
  LibbluPIDValues * dst,
  void (*limitsInitializationFun) (LibbluNumberOfESTypes *)
)
{
  limitsInitializationFun(&dst->limits);
  cleanLibbluNumberOfESTypes(&dst->nbStreams);
  initLibbluRegisteredPIDValues(&dst->registeredValues);

  dst->errorOnInvalidPIDRequest = false;
}

static inline void cleanLibbluPIDValues(
  LibbluPIDValues values
)
{
  cleanLibbluRegisteredPIDValues(values.registeredValues);
}

#endif