#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "errorCodes.h"

static const struct {
  LibbluStatus opt;
  char * name;
  char * desc;
} debuggingOptions[] = {
#define DECLARE_OPTION(o, n, d)  {.opt = o, .name = n, .desc = d}

  DECLARE_OPTION(
    LIBBLU_DEBUG_GLB,
    "all",
    "Enable all available categories, as '-d' option"
  ),

  DECLARE_OPTION(
    LIBBLU_DEBUG_SCRIPTS,
    "esms_scripts",
    "Muxer scripts management"
  ),

  DECLARE_OPTION(
    LIBBLU_DEBUG_MUXER_DECISION,
    "muxer_decisions",
    "Muxer packets decision"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_PES_BUILDING,
    "pes_building",
    "PES packets building information"
  ),

  /* Muxer T-STD Buffer Verifier : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_TEST,
    "t_std_test",
    "T-STD Buffer Verifier tests"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_DECLARATIONS,
    "t_std_decl",
    "T-STD Buffer Verifier packets data declaration"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_OPERATIONS,
    "t_std_operations",
    "T-STD Buffer Verifier operations"
  ),

  /* LCPM Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_LPCM_PARSING,
    "lcpm_parsing",
    "LPCM audio content"
  ),

  /* AC-3 Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_AC3_PARSING,
    "ac3_parsing",
    "AC-3 audio content"
  ),

  /* DTS Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_DTSHD,
    "dts_parsing_dtshd",
    "DTS audio DTSHD file headers content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_PBRFILE,
    "dts_parsing_pbrfile",
    "DTS audio dtspbr statistics file content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_CORE,
    "dts_parsing_core",
    "DTS audio Core Substream content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_EXTSS,
    "dts_parsing_extSS",
    "DTS audio Extension Substreams content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_XLL,
    "dts_parsing_xll",
    "DTS audio XLL extension content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PBR,
    "dts_pbr",
    "DTS audio PBR smoothing process informations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_OPERATIONS,
    "dts_operations",
    "DTS audio parser operations"
  ),

  /* H.262 Video : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_H262_PARSING,
    "h262_parsing",
    "H.262 video content"
  ),

  /* H.264 Video : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_NAL,
    "h264_parsing_nalu",
    "H.264 video NAL Units headers content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_AUD,
    "h264_parsing_aud",
    "H.264 video Access Unit Delimiter and other structural NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SPS,
    "h264_parsing_sps",
    "H.264 video Sequence Parameter Set NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_VUI,
    "h264_parsing_vui",
    "H.264 video SPS Video Usability Information content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_PPS,
    "h264_parsing_pps",
    "H.264 video Picture Parameter Set NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SEI,
    "h264_parsing_sei",
    "H.264 video Supplemental Enhancement Information messages NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SLICE,
    "h264_parsing_slice",
    "H.264 video Slice headers NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_OPERATIONS,
    "h264_operations",
    "H.264 video parser operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_AU_PROCESSING,
    "h264_access_units_processing",
    "H.264 video Access Units detection and stitching"
  ),

  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_HRD_CPB,
    "h264_hrd_cpb",
    "H.264 HRD Coded Picture Buffer status"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_HRD_DPB,
    "h264_hrd_dpb",
    "H.264 HRD Decoded Picture Buffer status"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_HRD_OPERATIONS,
    "h264_hrd_operations",
    "H.264 HRD verification operations"
  ),

  /* HDMV Common : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_COMMON,
    "hdmv_common",
    "HDMV bitstreams common operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_PARSER,
    "hdmv_parser",
    "HDMV bitstreams common parsing informations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_CHECKS,
    "hdmv_checks",
    "HDMV bitstreams compliance checks"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_SEG_BUILDER,
    "hdmv_seg_builder",
    "HDMV bitstreams segments builder"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_QUANTIZER,
    "hdmv_quantizer",
    "HDMV pictures color quantization using Octree (in fact. an Hexatree)"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_PAL,
    "hdmv_pal",
    "HDMV palettes contruction"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_PICTURES,
    "hdmv_pic",
    "HDMV pictures handling"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_LIBPNG,
    "hdmv_libpng",
    "HDMV pictures LibPng IO operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_TC,
    "hdmv_timecode",
    "HDMV streams generation timecodes management"
  ),

  /* HDMV IGS Parser : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_PARSER,
    "pgs_parser",
    "HDMV PGS (subtitles) content"
  ),

  /* HDMV IGS Parser : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_PARSER,
    "igs_parser",
    "HDMV IGS (menus) content"
  ),

  /* HDMV IGS Compiler : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_XML_OPERATIONS,
    "igs_compl_xml_operations",
    "HDMV IGS (menus) compiler XML file operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_XML_PARSING,
    "igs_compl_xml_parsing",
    "HDMV IGS (menus) compiler XML file content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_OPERATIONS,
    "igs_compl_operations",
    "HDMV IGS (menus) compiler operations"
  )
#undef DECLARE_OPTION
};

static char enabledStatus[LIBBLU_NB_STATUS] = {
  0
};

int enableDebugStatusString(
  const char * string
)
{
  bool arrayMode = false;

  if (*string == '\"') {
    arrayMode = true;
    string++;
  }

  do {
    LibbluStatus status;
    char modeName[50];
    size_t i;

    sscanf(string, " %49[A-Za-z0-9_]s", modeName);
    string += strlen(modeName);

    status = LIBBLU_FATAL_ERROR;
    for (i = 0; i < ARRAY_SIZE(debuggingOptions); i++) {
      if (0 == strcmp(modeName, debuggingOptions[i].name)) {
        status = debuggingOptions[i].opt;
        break;
      }
    }

    if (status <= LIBBLU_FATAL_ERROR)
      LIBBLU_ERROR_RETURN(
        "Unknown debugging mode '%s'.\n",
        modeName
      );

    enabledStatus[status] = 1;
    if (status == LIBBLU_DEBUG_GLB) {
      memset(enabledStatus, 1, ARRAY_SIZE(enabledStatus));
    }

    while ((arrayMode && isspace(*string)) || (ispunct(*string)))
      string++;
  } while ((!arrayMode || *string != '\"') && *string != '\0');

  return 0;
}

bool isEnabledLibbbluStatus(
  LibbluStatus status
)
{
  return enabledStatus[status];
}

void echoMessageFdVa(
  FILE * fd,
  LibbluStatus status,
  const lbc * format,
  va_list args
)
{
  if (status < LIBBLU_DEBUG_GLB || enabledStatus[status])
    lbc_vfprintf(fd, format, args);
}

void echoMessageVa(
  LibbluStatus status,
  const lbc * format,
  va_list args
)
{
  if (LIBBLU_FATAL_ERROR <= status)
    echoMessageFdVa(stderr, status, format, args);
  else
    echoMessageFdVa(stdout, status, format, args);
}

void echoMessageFd(
  FILE * fd,
  LibbluStatus status,
  const lbc * format,
  ...
)
{
  va_list args;

  va_start(args, format);
  echoMessageFdVa(fd, status, format, args);
  va_end(args);
}

void echoMessage(
  LibbluStatus status,
  const lbc * format,
  ...
)
{
  va_list args;

  va_start(args, format);
  echoMessageVa(status, format, args);
  va_end(args);
}

void printListLibbbluStatus(
  unsigned indent
)
{
  size_t i;

  for (i = 0; i < ARRAY_SIZE(debuggingOptions); i++) {
    lbc_printf(
      "%-*c- %s;\n",
      indent, ' ',
      debuggingOptions[i].name
    );
  }
}

void printListWithDescLibbbluStatus(
  unsigned indent
)
{
  size_t i;

  for (i = 0; i < ARRAY_SIZE(debuggingOptions); i++) {
    lbc_printf(
      "%-*c- %s;\n",
      indent, ' ',
      debuggingOptions[i].name
    );
    lbc_printf(
      "%-*c   %s.\n",
      indent, ' ',
      debuggingOptions[i].desc
    );
  }
}