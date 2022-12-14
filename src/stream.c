#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "stream.h"

static LibbluStreamPtr createLibbluStream(
  StreamType type,
  uint16_t pid
)
{
  LibbluStreamPtr stream;

  if (NULL == (stream = (LibbluStreamPtr) malloc(sizeof(LibbluStream))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  stream->type = type;
  stream->pid = pid;
  stream->packetNb = 0;

  return stream;
}

LibbluStreamPtr createElementaryLibbluStream(
  LibbluESSettings * settings
)
{
  LibbluStreamPtr stream;

  if (NULL == (stream = createLibbluStream(TYPE_ES, 0x0000)))
    return NULL;
  initLibbluES(&stream->es, settings);

  return stream;
}

#define DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN(sysName, paramName)               \
LibbluStreamPtr create##sysName##SystemLibbluStream(                          \
  paramName param,                                                            \
  uint16_t pid                                                                \
)                                                                             \
{                                                                             \
  LibbluStreamPtr stream;                                                     \
                                                                              \
  if (NULL == (stream = createLibbluStream(TYPE_##sysName, pid)))             \
    return NULL;                                                              \
                                                                              \
  if (prepare##sysName##SystemStream(&stream->sys, param) < 0)                \
    goto free_return;                                                         \
                                                                              \
  return stream;                                                              \
                                                                              \
free_return:                                                                  \
  destroyLibbluStream(stream);                                                \
  return NULL;                                                                \
}

#define DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN_NP(sysName)                       \
LibbluStreamPtr create##sysName##SystemLibbluStream(                          \
  uint16_t pid                                                                \
)                                                                             \
{                                                                             \
  LibbluStreamPtr stream;                                                     \
                                                                              \
  if (NULL == (stream = createLibbluStream(TYPE_##sysName, pid)))             \
    return NULL;                                                              \
                                                                              \
  if (prepare##sysName##SystemStream(&stream->sys) < 0)                       \
    goto free_return;                                                         \
                                                                              \
  return stream;                                                              \
                                                                              \
free_return:                                                                  \
  destroyLibbluStream(stream);                                                \
  return NULL;                                                                \
}

DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN(PAT, PatParameters)
DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN(PMT, PmtParameters)
DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN_NP(PCR)
DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN(SIT, SitParameters)
DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN_NP(NULL)

#undef DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN
#undef DEF_CREATE_SYSTEM_LIBBLU_STREAM_FUN_NP

void destroyLibbluStream(
  LibbluStreamPtr stream
)
{
  if (NULL == stream)
    return;

  if (isESLibbluStream(stream))
    cleanLibbluES(stream->es);
  else
    cleanLibbluSystemStream(stream->sys);

  free(stream);
}

int requestESPIDLibbluStream(
  LibbluPIDValues * values,
  uint16_t * pid,
  LibbluStreamPtr stream
)
{
  int ret;

  LibbluESPtr es;
  LibbluESSettings * streamSettings;
  LibbluESProperties streamProperties;

  uint16_t basePid;
  unsigned * curEsTypeNb;
  unsigned limitEsTypeNb;
  const char * selectedTypeStr;

  uint16_t selectedPid;
  bool validPid;

  assert(NULL != values);
  assert(NULL != pid);
  assert(NULL != stream);
  assert(isESLibbluStream(stream));

  es = &stream->es;
  streamSettings = es->settings;
  streamProperties = es->prop;

  /* Get the base PID and the current number of ES of stream type */
  if (streamProperties.type == ES_VIDEO) {
    if (!streamSettings->options.secondaryStream) {
      basePid = PRIM_VIDEO_PID;
      curEsTypeNb = &values->nbStreams.primaryVideo;
      limitEsTypeNb = values->limits.primaryVideo;

      selectedTypeStr = "Primary Video";
    }
    else {
      basePid = SEC_VIDEO_PID;
      curEsTypeNb = &values->nbStreams.secondaryVideo;
      limitEsTypeNb = values->limits.secondaryVideo;

      selectedTypeStr = "Secondary Video";
    }
  }
  else if (streamProperties.type == ES_AUDIO) {
    if (!streamSettings->options.secondaryStream) {
      basePid = PRIM_AUDIO_PID;
      curEsTypeNb = &values->nbStreams.primaryAudio;
      limitEsTypeNb = values->limits.primaryAudio;

      selectedTypeStr = "Primary Audio";
    }
    else {
      basePid = SEC_AUDIO_PID;
      curEsTypeNb = &values->nbStreams.secondaryAudio;
      limitEsTypeNb = values->limits.secondaryAudio;

      selectedTypeStr = "Secondary Audio";
    }
  }
  else if (streamProperties.codingType == STREAM_CODING_TYPE_PG) {
    basePid = PG_PID;
    curEsTypeNb = &values->nbStreams.pg;
    limitEsTypeNb = values->limits.pg;

    selectedTypeStr = "Presentation Graphics (PG) subtitles";
  }
  else if (streamProperties.codingType == STREAM_CODING_TYPE_IG) {
    basePid = IG_PID;
    curEsTypeNb = &values->nbStreams.ig;
    limitEsTypeNb = values->limits.ig;

    selectedTypeStr = "Interactive Graphics (IG) menus";
  }
  else if (streamProperties.codingType == STREAM_CODING_TYPE_TEXT) {
    basePid = TXT_SUB_PID;
    curEsTypeNb = &values->nbStreams.txtSubtitles;
    limitEsTypeNb = values->limits.txtSubtitles;

    selectedTypeStr = "Text Subtitles (TextST)";
  }
  else
    LIBBLU_ERROR_RETURN(
      "Unknown stream coding type, unable to select a PID value.\n"
    );

  /* Check number of already registered ES */
  if (limitEsTypeNb <= *curEsTypeNb)
    LIBBLU_ERROR_RETURN(
      "Too many %s streams.\n",
      selectedTypeStr
    );

  if (streamSettings->pid != 0x0000) {
    /* Custom requested PID */
    bool valid;

    selectedPid = streamSettings->pid;

    valid = true;
    if (isReservedPIDValue(selectedPid)) {
      if (values->errorOnInvalidPIDRequest)
        LIBBLU_ERROR_RETURN(
          "Unable to use PID value 0x%04" PRIX16 ", this value is reserved.\n",
          streamSettings->pid
        );
      selectedPid = 0x0000;
    }

    if (selectedPid < basePid || basePid + limitEsTypeNb <= selectedPid) {
      /* Out of range PID request */
      if (values->errorOnInvalidPIDRequest)
        LIBBLU_ERROR_RETURN(
          "Unable to use PID value 0x%04" PRIX16 ", it is not in the allowed "
          "range for a %s stream.\n",
          streamSettings->pid,
          selectedTypeStr
        );

      valid = false;
      selectedPid = basePid;
    }

    if (!valid)
      LIBBLU_INFO(
        "Unable to use PID value 0x%04" PRIX16 ", "
        ", selection of a replacement available value.\n",
        streamSettings->pid,
        selectedPid
      );
  }
  else
    selectedPid = basePid;

  validPid = false;

  while (!validPid) {
    /* Check PID value range */
    if (basePid + limitEsTypeNb <= selectedPid)
      LIBBLU_ERROR_RETURN(
        "Unable to select a valid PID value for a %s stream.\n",
        selectedTypeStr
      );

    /* Try to insert value */
    ret = insertLibbluRegisteredPIDValues(&values->registeredValues, basePid);
    switch (ret) {
      case 0: /* Unable to inser value */
        selectedPid++;
        break;

      case 1: /* Insertion successfull */
        validPid = true;
        break;

      default: /* Error */
        return -1;
    }
  }

  if (NULL != pid)
    *pid = selectedPid;
  (*curEsTypeNb)++;

  return 0;
}