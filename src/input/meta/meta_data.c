#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "meta_data.h"


#define STREAM_CODING_TYPE_ANY  0

#define HEADER_OPT(name, id, arg_type)                                        \
  (LibbluMetaOption) {lbc_str(name), id, arg_type, NULL}

#define ANY_CODING_TYPE  (LibbluStreamCodingType[]) { STREAM_CODING_TYPE_ANY }
#define CODING_TYPES(...)  (LibbluStreamCodingType[]) { __VA_ARGS__ , -1 }

#define TRACK_OPT(name, id, arg_type, coding_types)                           \
  (LibbluMetaOption) {lbc_str(name), id, arg_type, coding_types}


static const LibbluMetaOption options[] = {
  /*  */

  /* MUXOPT header options : */
  HEADER_OPT("--no-extra-header", LBMETA_OPT__NO_EXTRA_HEADER, LBMETA_OPTARG_NO_ARG),
  HEADER_OPT(            "--cbr",         LBMETA_OPT__CBR_MUX, LBMETA_OPTARG_NO_ARG),
  HEADER_OPT(     "--force-esms",      LBMETA_OPT__FORCE_ESMS, LBMETA_OPTARG_NO_ARG),
  HEADER_OPT(   "--disable-tstd",   LBMETA_OPT__DISABLE_T_STD, LBMETA_OPTARG_NO_ARG),

  HEADER_OPT(     "--start-time",      LBMETA_OPT__START_TIME, LBMETA_OPTARG_UINT64),
  HEADER_OPT(       "--mux-rate",        LBMETA_OPT__MUX_RATE, LBMETA_OPTARG_UINT64),
  HEADER_OPT(      "--dvd-media",       LBMETA_OPT__DVD_MEDIA, LBMETA_OPTARG_NO_ARG),

  /* MUXOPT track options : */
  TRACK_OPT(     "--force-esms",      LBMETA_OPT__FORCE_ESMS, LBMETA_OPTARG_NO_ARG,
    ANY_CODING_TYPE),
  TRACK_OPT(      "--dvd-media",       LBMETA_OPT__DVD_MEDIA, LBMETA_OPTARG_NO_ARG,
    ANY_CODING_TYPE),

  TRACK_OPT(      "--secondary",       LBMETA_OPT__SECONDARY, LBMETA_OPTARG_NO_ARG,
    CODING_TYPES(
      STREAM_CODING_TYPE_H262,
      STREAM_CODING_TYPE_AVC,
      STREAM_CODING_TYPE_VC1,
      STREAM_CODING_TYPE_AC3,
      STREAM_CODING_TYPE_DTS
    )),

  // H.264/AVC video tracks only :
#define CT  CODING_TYPES(STREAM_CODING_TYPE_AVC)
  TRACK_OPT(    "--disable-fixes",     LBMETA_OPT__DISABLE_FIXES, LBMETA_OPTARG_NO_ARG, CT),
  TRACK_OPT(              "--fps",           LBMETA_OPT__SET_FPS, LBMETA_OPTARG_STRING, CT),
  TRACK_OPT(               "--ar",            LBMETA_OPT__SET_AR, LBMETA_OPTARG_STRING, CT),
  TRACK_OPT(            "--level",         LBMETA_OPT__SET_LEVEL, LBMETA_OPTARG_STRING, CT),
  TRACK_OPT(       "--remove-sei",        LBMETA_OPT__REMOVE_SEI, LBMETA_OPTARG_NO_ARG, CT),
  TRACK_OPT("--disable-hrd-verif", LBMETA_OPT__DISABLE_HRD_VERIF, LBMETA_OPTARG_NO_ARG, CT),
  TRACK_OPT(    "--hrd-cpb-stats",     LBMETA_OPT__HRD_CPB_STATS, LBMETA_OPTARG_STRING, CT),
  TRACK_OPT(    "--hrd-dpb-stats",     LBMETA_OPT__HRD_DPB_STATS, LBMETA_OPTARG_STRING, CT),
#undef CT

  // AC3/DTS audio tracks only :
  TRACK_OPT(           "--core",    LBMETA_OPT__EXTRACT_CORE, LBMETA_OPTARG_NO_ARG,
    CODING_TYPES(STREAM_CODING_TYPE_AC3, STREAM_CODING_TYPE_DTS)),

  // DTS audio tracks only :
  TRACK_OPT(            "--pbr",        LBMETA_OPT__PBR_FILE, LBMETA_OPTARG_STRING,
    CODING_TYPES(STREAM_CODING_TYPE_DTS)),

  // HDMV tracks only :
#define CT  CODING_TYPES(STREAM_CODING_TYPE_PG, STREAM_CODING_TYPE_IG)
  TRACK_OPT("--hdmv-initial-timestamp",     LBMETA_OPT__HDMV_INITIAL_TS, LBMETA_OPTARG_UINT64, CT),
  TRACK_OPT(   "--hdmv-force-retiming", LBMETA_OPT__HDMV_FORCE_RETIMING, LBMETA_OPTARG_NO_ARG, CT),
#undef CT
};

static int _parseOptionArgument(
  LibbluMetaOptionArgValue *arg_value_ret,
  const lbc *string,
  LibbluMetaOptionArgType expected_arg_type
)
{
  LibbluMetaOptionArgValue arg_value;

  switch (expected_arg_type) {
  case LBMETA_OPTARG_NO_ARG:
    LIBBLU_ERROR_RETURN("No argument expected.\n");

  case LBMETA_OPTARG_UINT64:
    if (!lbc_sscanf(string, lbc_str("%" SCNu64), &arg_value.u64))
      return -1;
    break;

  case LBMETA_OPTARG_STRING:
    if (NULL == (arg_value.str = lbc_strdup(string)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  }

  if (NULL != arg_value_ret)
    *arg_value_ret = arg_value;

  return 0;
}

LibbluMetaOptionId parseLibbluMetaTrackOption(
  const LibbluMetaFileOption *option_node,
  LibbluMetaOption *option_ret,
  LibbluMetaOptionArgValue *argument_ret,
  LibbluStreamCodingType compat_coding_type
)
{
  assert(0 < compat_coding_type);

  // True if option name matches an header only option name
  bool exists_hdr_only_option_same_name = false;

  for (size_t opt_idx = 0; opt_idx < ARRAY_SIZE(options); opt_idx++) {
    if (lbc_equal(option_node->name, options[opt_idx].name)) {
      LibbluMetaOption selected_opt = options[opt_idx];

      if (NULL == selected_opt.compat_coding_types) {
        exists_hdr_only_option_same_name = true;
        continue;
      }

      unsigned opt_compat_type_idx = 0;
      for (; 0 < selected_opt.compat_coding_types[opt_compat_type_idx]; opt_compat_type_idx++) {
        if (compat_coding_type == selected_opt.compat_coding_types[opt_compat_type_idx])
          break;
      }

      if (selected_opt.compat_coding_types[opt_compat_type_idx] < 0)
        LIBBLU_META_ANALYZER_ERROR_RETURN(
          "Option '%s' cannot be used on a track with stream coding type '%s' (line %u).\n",
          option_node->name,
          LibbluStreamCodingTypeStr(compat_coding_type),
          option_node->line
        );

      /* Option argument */
      if (LBMETA_OPTARG_NO_ARG != selected_opt.arg_type) {
        // Argument expected
        if (NULL == option_node->arg)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "Option '%s' expect an argument (line %u).\n",
            option_node->name,
            option_node->line
          );

        if (_parseOptionArgument(argument_ret, option_node->arg, selected_opt.arg_type) < 0)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "Invalid argument '%s' for option '%s' (line %u).\n",
            option_node->arg,
            option_node->name,
            option_node->line
          );
      }
      else {
        // No argument expected
        if (NULL != option_node->arg)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "Option '%s' does not expect an argument (line %u).\n",
            option_node->name,
            option_node->line
          );
      }

      if (NULL != option_ret)
        *option_ret = selected_opt;

      return selected_opt.id;
    }
  }

  if (exists_hdr_only_option_same_name)
    LIBBLU_META_ANALYZER_ERROR_RETURN(
      "Option '%s' cannot be used as a track option (line %u).\n",
      option_node->name,
      option_node->line
    );

  LIBBLU_META_ANALYZER_ERROR_RETURN(
    "Unknown option '%s' (line %u).\n",
    option_node->name,
    option_node->line
  );
}

LibbluMetaOptionId parseLibbluMetaHeaderOption(
  const LibbluMetaFileOption *option_node,
  LibbluMetaOption *option_ret,
  LibbluMetaOptionArgValue *argument_ret
)
{
  for (size_t opt_idx = 0; opt_idx < ARRAY_SIZE(options); opt_idx++) {
    if (lbc_equal(option_node->name, options[opt_idx].name)) {
      LibbluMetaOption selected_opt = options[opt_idx];

      /* Option argument */
      if (LBMETA_OPTARG_NO_ARG != selected_opt.arg_type) {
        // Argument expected
        if (NULL == option_node->arg)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "Option '%s' expect an argument (line %u).\n",
            option_node->name,
            option_node->line
          );

        if (_parseOptionArgument(argument_ret, option_node->arg, selected_opt.arg_type) < 0)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "Invalid argument '%s' for option '%s' (line %u).\n",
            option_node->arg,
            option_node->name,
            option_node->line
          );
      }
      else {
        // No argument expected
        if (NULL != option_node->arg)
          LIBBLU_META_ANALYZER_ERROR_RETURN(
            "Option '%s' does not expect an argument (line %u).\n",
            option_node->name,
            option_node->line
          );
      }

      if (NULL != option_ret)
        *option_ret = selected_opt;

      return selected_opt.id;
    }
  }

  LIBBLU_META_ANALYZER_ERROR_RETURN(
    "Unknown option '%s' (line %u).\n",
    option_node->name,
    option_node->line
  );
}
