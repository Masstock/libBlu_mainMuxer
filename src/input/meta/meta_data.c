#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "meta_data.h"

static const LibbluMetaOption options[] = {
#define A_(...)  (LibbluStreamCodingType[]) { __VA_ARGS__ , -1}
#define D_(i, n, a, c)                                                      \
  (LibbluMetaOption) {i, lbc_str(n), a, A_ c}
#define HRD  STREAM_CODING_TYPE_NULL  /* Only in header */
#define ANY  STREAM_CODING_TYPE_ANY   /* On any track type */

  /*             Option enum value,       option string,        argument type, */
  /* (allowed location(s))                                                     */
  D_(  LBMETA_OPT__NO_EXTRA_HEADER,   "--no-extra-header",  LBMETA_OPTARG_NO_ARG,
    (HRD)),
  D_(          LBMETA_OPT__CBR_MUX,               "--cbr",  LBMETA_OPTARG_NO_ARG,
    (HRD)),
  D_(        LBMETA_OPT__FORCE_ESMS,        "--force-esms", LBMETA_OPTARG_NO_ARG,
    (ANY, HRD)),
  D_(    LBMETA_OPT__DISABLE_T_STD,      "--disable-tstd",  LBMETA_OPTARG_NO_ARG,
    (HRD)),

  D_(       LBMETA_OPT__START_TIME,        "--start-time",  LBMETA_OPTARG_UINT64,
    (HRD)),
  D_(         LBMETA_OPT__MUX_RATE,          "--mux-rate",  LBMETA_OPTARG_UINT64,
    (HRD)),

  D_(    LBMETA_OPT__DISABLE_FIXES,     "--disable-fixes",  LBMETA_OPTARG_NO_ARG,
    (STREAM_CODING_TYPE_AVC)),

  D_(        LBMETA_OPT__DVD_MEDIA,         "--dvd-media",  LBMETA_OPTARG_NO_ARG,
    (ANY, HRD)),

  D_(        LBMETA_OPT__SECONDARY,         "--secondary",  LBMETA_OPTARG_NO_ARG,
    (STREAM_CODING_TYPE_H262, STREAM_CODING_TYPE_AVC, STREAM_CODING_TYPE_VC1,
    STREAM_CODING_TYPE_AC3, STREAM_CODING_TYPE_DTS)),

  D_(     LBMETA_OPT__EXTRACT_CORE,              "--core",  LBMETA_OPTARG_NO_ARG,
    (STREAM_CODING_TYPE_AC3, STREAM_CODING_TYPE_DTS)),

  D_(         LBMETA_OPT__PBR_FILE,               "--pbr",  LBMETA_OPTARG_STRING,
    (STREAM_CODING_TYPE_DTS)),

  D_(          LBMETA_OPT__SET_FPS,               "--fps",  LBMETA_OPTARG_STRING,
    (STREAM_CODING_TYPE_AVC)),
  D_(           LBMETA_OPT__SET_AR,                "--ar",  LBMETA_OPTARG_STRING,
    (STREAM_CODING_TYPE_AVC)),
  D_(        LBMETA_OPT__SET_LEVEL,             "--level",  LBMETA_OPTARG_STRING,
    (STREAM_CODING_TYPE_AVC)),
  D_(       LBMETA_OPT__REMOVE_SEI,        "--remove-sei",  LBMETA_OPTARG_NO_ARG,
    (STREAM_CODING_TYPE_AVC)),
  D_(LBMETA_OPT__DISABLE_HRD_VERIF, "--disable-hrd-verif",  LBMETA_OPTARG_NO_ARG,
    (STREAM_CODING_TYPE_AVC)),
  D_(    LBMETA_OPT__HRD_CPB_STATS,     "--hrd-cpb-stats",  LBMETA_OPTARG_STRING,
    (STREAM_CODING_TYPE_AVC)),
  D_(    LBMETA_OPT__HRD_DPB_STATS,     "--hrd-dpb-stats",  LBMETA_OPTARG_STRING,
    (STREAM_CODING_TYPE_AVC)),

  D_(  LBMETA_OPT__HDMV_INITIAL_TS, "--hdmv-initial-timestamp", LBMETA_OPTARG_UINT64,
    (STREAM_CODING_TYPE_PG, STREAM_CODING_TYPE_IG)),
  D_(LBMETA_OPT__HDMV_FORCE_RETIMING, "--hdmv-force-retiming",  LBMETA_OPTARG_NO_ARG,
    (STREAM_CODING_TYPE_PG, STREAM_CODING_TYPE_IG)),

  D_(LBMETA_OPT__HDMV_ASS_INPUT,            "--ass-input",  LBMETA_OPTARG_NO_ARG,
    (STREAM_CODING_TYPE_PG)),

  D_(             LBMETA_OPT__ESMS,              "--esms",  LBMETA_OPTARG_STRING,
    (ANY))

#undef A_
#undef D_
#undef HDR
};

static int parseLibbluMetaOptionArg(
  const lbc *string,
  LibbluMetaOptionArgType expectedType,
  LibbluMetaOptionArgValue *value
)
{
  LibbluMetaOptionArgValue argVal;

  switch (expectedType) {
  case LBMETA_OPTARG_NO_ARG:
    LIBBLU_ERROR_RETURN("No argument expected.\n");

  case LBMETA_OPTARG_UINT64:
    if (!lbc_sscanf(string, lbc_str("%" SCNu64), &argVal.u64))
      return -1;
    break;

  case LBMETA_OPTARG_STRING:
    if (NULL == (argVal.str = lbc_strdup(string)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  }

  if (NULL != value)
    *value = argVal;

  return 0;
}

LibbluMetaOptionId parseLibbluMetaOption(
  const LibbluMetaFileOption *node,
  LibbluMetaOption *option_ret,
  LibbluMetaOptionArgValue *argument_ret,
  LibbluStreamCodingType track_coding_type
)
{
  const lbc *name = node->name;
  const lbc *arg  = node->arg;

  for (size_t opt_idx = 0; opt_idx < ARRAY_SIZE(options); opt_idx++) {
    if (lbc_equal(name, options[opt_idx].name)) {
      LibbluMetaOption selected_opt = options[opt_idx];
      size_t opt_compat_type_idx = 0;

      /* Check option location allowance */
      for (; 0 < selected_opt.compat_coding_types[opt_compat_type_idx]; opt_compat_type_idx++) {
        LibbluStreamCodingType opt_compat_types = selected_opt.compat_coding_types[opt_compat_type_idx];

        if (opt_compat_types == STREAM_CODING_TYPE_ANY || opt_compat_types == track_coding_type) {
          break;
        }
      }

      if (selected_opt.compat_coding_types[opt_compat_type_idx] < 0 && 0 <= track_coding_type) {
        if (selected_opt.compat_coding_types[0] < 0)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "Option '%s' can only be used in header.\n",
            name
          );

        LIBBLU_META_ANALYZER_ERROR_RETURN(
          "Option '%s' cannot be used on a %s track.\n",
          name,
          LibbluStreamCodingTypeStr(track_coding_type)
        );
      }

      /* Option argument */
      if (LBMETA_OPTARG_NO_ARG != selected_opt.arg_type) {
        /* Expect presence of argument. */
        if (NULL == arg)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "The option '%s' expect an argument.\n",
            name
          );

        if (parseLibbluMetaOptionArg(arg, selected_opt.arg_type, argument_ret) < 0)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "Invalid argument '%s' for option '%s'.\n",
            arg,
            name
          );
      }
      else {
        /* Unexpect presence of argument. */
        if (NULL != arg)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "The option '%s' does not expect an argument.\n",
            name
          );
      }

      if (NULL != option_ret)
        *option_ret = selected_opt;

      return selected_opt.id;
    }
  }

  LIBBLU_META_ANALYZER_ERROR_RETURN(
    "Unknown option '%s'.\n",
    name
  );
}
