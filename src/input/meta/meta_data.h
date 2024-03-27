

#ifndef __LIBBLU_MUXER__INPUT__META__DATA_H__
#define __LIBBLU_MUXER__INPUT__META__DATA_H__

#include "../../util.h"
#include "../../streamCodingType.h"
#include "meta_parser.h"

typedef enum {
  LBMETA_OPT__INVALID          = -1,

  LBMETA_OPT__NO_EXTRA_HEADER,
  LBMETA_OPT__CBR_MUX,
  LBMETA_OPT__FORCE_ESMS,
  LBMETA_OPT__DISABLE_T_STD,

  LBMETA_OPT__START_TIME,
  LBMETA_OPT__MUX_RATE,

  LBMETA_OPT__DVD_MEDIA,

  LBMETA_OPT__SECONDARY,
  LBMETA_OPT__DISABLE_FIXES,

  LBMETA_OPT__EXTRACT_CORE,

  LBMETA_OPT__PBR_FILE,

  LBMETA_OPT__SET_FPS,
  LBMETA_OPT__SET_AR,
  LBMETA_OPT__SET_LEVEL,
  LBMETA_OPT__REMOVE_SEI,
  LBMETA_OPT__DISABLE_HRD_VERIF,
  LBMETA_OPT__HRD_CPB_STATS,
  LBMETA_OPT__HRD_DPB_STATS,

  LBMETA_OPT__HDMV_INITIAL_TS,
  LBMETA_OPT__HDMV_FORCE_RETIMING,
  LBMETA_OPT__HDMV_ASS_INPUT,

  LBMETA_OPT__ESMS
} LibbluMetaOptionId;

typedef enum {
  LBMETA_OPTARG_NO_ARG,

  LBMETA_OPTARG_UINT64,
  LBMETA_OPTARG_STRING
} LibbluMetaOptionArgType;

typedef struct {
  const lbc *name;

  LibbluMetaOptionId id;
  LibbluMetaOptionArgType arg_type;

  const LibbluStreamCodingType *compat_coding_types;
} LibbluMetaOption;

typedef union {
  uint64_t u64;
  lbc *str;
} LibbluMetaOptionArgValue;

LibbluMetaOptionId parseLibbluMetaTrackOption(
  const LibbluMetaFileOption *node,
  LibbluMetaOption *option_ret,
  LibbluMetaOptionArgValue *argument_ret,
  LibbluStreamCodingType compat_coding_type
);

LibbluMetaOptionId parseLibbluMetaHeaderOption(
  const LibbluMetaFileOption *node,
  LibbluMetaOption *option_ret,
  LibbluMetaOptionArgValue *argument_ret
);

static inline void cleanLibbluMetaOptionArgValue(
  LibbluMetaOption opt,
  LibbluMetaOptionArgValue arg
)
{
  if (opt.arg_type == LBMETA_OPTARG_STRING)
    free(arg.str);
}

#endif