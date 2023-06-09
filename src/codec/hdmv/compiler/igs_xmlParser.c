#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "igs_xmlParser.h"
#include "../../../util/errorCodesVa.h"

#define NB_OBJ(path, ...)                                                     \
  getNbObjectsFromExprIgsXmlFile(XML_CTX(ctx), path, ##__VA_ARGS__)

#define EXISTS(path, ...)                                                     \
  (0 < NB_OBJ(path, ##__VA_ARGS__))

#define GET_OBJ(path, ...)                                                    \
  getPathObjectFromExprIgsXmlFile(XML_CTX(ctx), path, ##__VA_ARGS__)

#define GET_STRING(dst, def, err_instr, path, ...)                            \
  do {                                                                        \
    int _ret;                                                                 \
                                                                              \
    _ret = getIfExistsStringFromExprIgsXmlFile(                               \
      XML_CTX(ctx), dst, (xmlChar *) def, path, ##__VA_ARGS__                 \
    );                                                                        \
    if (_ret < 0)                                                             \
      err_instr;                                                              \
  } while (0)

#define GET_BOOL(dst, def, err_instr, path, ...)                              \
  do {                                                                        \
    int _ret;                                                                 \
    bool _val;                                                                \
                                                                              \
    _ret = getIfExistsBooleanFromExprIgsXmlFile(                              \
      XML_CTX(ctx), &_val, def, path, ##__VA_ARGS__                           \
    );                                                                        \
    if (_ret < 0)                                                             \
      err_instr;                                                              \
                                                                              \
    *(dst) = _val;                                                            \
  } while (0)

#define GET_FLOAT(dst, def, err_instr, path, ...)                             \
  do {                                                                        \
    int _ret;                                                                 \
    float _val;                                                               \
                                                                              \
    _ret = getIfExistsFloatFromExprIgsXmlFile(                                \
      XML_CTX(ctx), &_val, def, path, ##__VA_ARGS__                           \
    );                                                                        \
    if (_ret < 0)                                                             \
      err_instr;                                                              \
                                                                              \
    *(dst) = _val;                                                            \
  } while (0)

#define GET_UINT(dst, def, err_msg, err_instr, path, ...)                     \
  do {                                                                        \
    int _ret;                                                                 \
    uint64_t _val;                                                            \
                                                                              \
    _ret = getIfExistsUint64FromExprIgsXmlFile(                               \
      XML_CTX(ctx), &_val, def, path, ##__VA_ARGS__                           \
    );                                                                        \
    if (_ret < 0)                                                             \
      err_instr;                                                              \
                                                                              \
    if (UINT_MAX < _val) {                                                    \
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR(err_msg);                               \
      err_instr;                                                              \
    }                                                                         \
                                                                              \
    *(dst) = _val;                                                            \
  } while (0)

#define GET_UINTN(n, dst, def, err_msg, err_instr, path, ...)                 \
  do {                                                                        \
    int _ret;                                                                 \
    uint64_t _val;                                                            \
                                                                              \
    _ret = getIfExistsUint64FromExprIgsXmlFile(                               \
      XML_CTX(ctx), &_val, def, path, ##__VA_ARGS__                           \
    );                                                                        \
    if (_ret < 0)                                                             \
      err_instr;                                                              \
                                                                              \
    if (UINT##n##_MAX < _val) {                                               \
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR(err_msg);                               \
      err_instr;                                                              \
    }                                                                         \
                                                                              \
    *(dst) = _val;                                                            \
  } while (0)

#define GET_UINT8(dst, def, err_msg, err_instr, path, ...)                    \
  GET_UINTN(8, dst, def, err_msg, err_instr, path, ##__VA_ARGS__)
#define GET_UINT16(dst, def, err_msg, err_instr, path, ...)                   \
  GET_UINTN(16, dst, def, err_msg, err_instr, path, ##__VA_ARGS__)
#define GET_UINT32(dst, def, err_msg, err_instr, path, ...)                   \
  GET_UINTN(32, dst, def, err_msg, err_instr, path, ##__VA_ARGS__)
#define GET_UINT64(dst, def, err_msg, err_instr, path, ...)                   \
  GET_UINTN(64, dst, def, err_msg, err_instr, path, ##__VA_ARGS__)

/* ========================== */

void echoErrorIgsXmlFile(
  const lbc * format,
  ...
)
{
  va_list args;

  va_start(args, format);
  LIBBLU_ERROR(LIBBLU_HDMV_IGS_COMPL_XML_PREFIX);
  LIBBLU_ERROR_VA(format, args);
  va_end(args);
}

void echoDebugIgsXmlFile(
  const lbc * format,
  ...
)
{
  va_list args;

  va_start(args, format);
  LIBBLU_DEBUG(LIBBLU_DEBUG_IGS_COMPL_XML_OPERATIONS, LIBBLU_HDMV_IGS_COMPL_XML_NAME, "");
  LIBBLU_DEBUG_VA(LIBBLU_DEBUG_IGS_COMPL_XML_OPERATIONS, format, args);
  va_end(args);
}

static unsigned lastParsedNodeLineIgsCompilerContext(
  const IgsCompilerContextPtr ctx
)
{
  return ctx->xmlCtx->lastParsedNodeLine;
}

#if 0
#define IGS_COMPILER_DEFAULT_OBJ_ALLOC 16

int addIgsCompilerInputPictureToList(
  IgsCompilerInputPicturePtr ** list,
  unsigned * listAllocationLen,
  unsigned * listUsageLen,
  IgsCompilerInputPicturePtr pic,
  uint16_t * id
)
{
  /** Return:
   * -1 Error
   *  0 Added
   *  1 Already present
   */

  IgsCompilerInputPicturePtr * newObjPicsList;

  /* References */
  IgsCompilerInputPicturePtr * inputList;
  uint16_t i;
  uint16_t inputListAllocationLen;
  uint16_t inputListUsageLen;

  assert(NULL != list);
  assert(NULL != listAllocationLen);
  assert(NULL != listUsageLen);

  inputList = *list;
  inputListAllocationLen = *listAllocationLen;
  inputListUsageLen = *listUsageLen;

  if (inputListUsageLen == UINT16_MAX)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Too many objects present (exceed 0xFFFE).\n"
    );

  for (i = 0; i < inputListUsageLen; i++) {
    if (inputList[i] == pic) {
      /* Picture already in list, return idx */
      if (NULL != id)
        *id = i;
      return 1;
    }
  }

  if (inputListAllocationLen <= inputListUsageLen) {
    /* Need realloc */
    inputListAllocationLen = GROW_ALLOCATION(
      inputListAllocationLen,
      IGS_COMPILER_DEFAULT_OBJ_ALLOC
    );

    newObjPicsList = (IgsCompilerInputPicturePtr *) realloc(
      inputList,
      inputListAllocationLen * sizeof(IgsCompilerInputPicturePtr)
    );
    if (NULL == newObjPicsList)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN("Memory allocation error.\n");

    inputList = newObjPicsList; /* Update */
  }

  inputList[inputListUsageLen++] = pic;

  /* Update pointers */
  *list = inputList;
  *listAllocationLen = inputListAllocationLen;
  *listUsageLen = inputListUsageLen;

  if (NULL != id)
    *id = inputListUsageLen - 1;
  return 0;
}

uint16_t addObjPicIgsCompilerComposition(
  IgsCompilerCompositionPtr compo,
  IgsCompilerInputPicturePtr pic
)
{
  int ret;
  uint16_t id;

  assert(NULL != compo);
  assert(NULL != pic);

  ret = addIgsCompilerInputPictureToList(
    &compo->objPics,
    &compo->nbAllocatedObjPics,
    &compo->nbObjPics,
    pic,
    &id
  );
  if (ret < 0)
    return UINT16_MAX;
  return id;
}
#endif

int parseParametersUopIgsXmlFile(
  IgsCompilerContextPtr ctx,
  const char * path,
  uint64_t * maskDest
)
{
  uint64_t mask;

  assert(NULL != maskDest);

  /* uop/mask */ {
    GET_UINT64(
      &mask, *maskDest,
      "Invalid UOP mask field, expect a valid unsigned value.\n",
      return -1,
      "%s/mask",
      path
    );
  }

  /* uop/<User Operation name> */
#define GET_UOP(m, n, f)                                                      \
  {                                                                           \
    bool _bl;                                                                 \
                                                                              \
    GET_BOOL(&_bl, !((*(m) >> (n)) & 0x1), goto free_return, "%s/"#f, path);  \
    *(m) = (*(m) & ~(1llu << (n))) | ((uint64_t) !_bl) << (n);                \
  }

  GET_UOP(&mask, 61, chapter_search);
  GET_UOP(&mask, 60, time_search);
  GET_UOP(&mask, 59, skip_to_next_point);
  GET_UOP(&mask, 58, skip_back_to_previous_point);
  GET_UOP(&mask, 56, stop);
  GET_UOP(&mask, 55, pause_on);
  GET_UOP(&mask, 53, still_off);
  GET_UOP(&mask, 52, forward_play);
  GET_UOP(&mask, 51, backward_play);
  GET_UOP(&mask, 50, resume);
  GET_UOP(&mask, 49, move_up_selected_button);
  GET_UOP(&mask, 48, move_down_selected_button);
  GET_UOP(&mask, 47, move_left_selected_button);
  GET_UOP(&mask, 46, move_right_selected_button);
  GET_UOP(&mask, 45, select_button);
  GET_UOP(&mask, 44, activate_button);
  GET_UOP(&mask, 43, select_button_and_activate);
  GET_UOP(&mask, 42, primary_audio_stream_number_change);
  GET_UOP(&mask, 40, angle_number_change);
  GET_UOP(&mask, 39, popup_on);
  GET_UOP(&mask, 38, popup_off);
  GET_UOP(&mask, 37, PG_textST_enable_disable);
  GET_UOP(&mask, 36, PG_textST_stream_number_change);
  GET_UOP(&mask, 35, secondary_video_enable_disable);
  GET_UOP(&mask, 34, secondary_video_stream_number_change);
  GET_UOP(&mask, 33, secondary_audio_enable_disable);
  GET_UOP(&mask, 32, secondary_audio_stream_number_change);
  GET_UOP(&mask, 31, PiP_PG_textST_stream_number_change);
#undef GET_UOP

  *maskDest = mask;

  return 0;

free_return:
  return -1;
}

int parseCommonSettingsIgsXmlFile(
  IgsCompilerContextPtr ctx
)
{
  int ret;

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("parameters:\n");

  /* video/@width : */ {
    unsigned value;

    GET_UINT(
      &value, 1920,
      "Invalid 'width' argument in parameters/video node in input "
      "XML file.\n",
      return -1,
      "/igs/parameters/video/@width"
    );

    if (
      value != 1920
      && value != 1440
      && value != 1280
      && value != 720
    ) {
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        "Invalid width argument, "
        "value shall be an integer value in string format "
        "(\"1920\", \"1440\", \"1280\" or \"720\", not %u) (line %u).\n",
        value,
        lastParsedNodeLineIgsCompilerContext(ctx)
      );
    }

    ctx->data.commonVideoDescriptor.video_width = value;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      " width: %u px.\n",
      ctx->data.commonVideoDescriptor.video_width
    );
  }

  /* video/@height : */ {
    unsigned value;

    GET_UINT(
      &value, 1080,
      "Invalid 'height' argument in parameters/video node in input "
      "XML file.\n",
      return -1,
      "/igs/parameters/video/@height"
    );

    if (
      value != 1080
      && value != 720
      && value != 480
      && value != 576
    ) {
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        "Height argument value shall be an integer value in string format "
        "(\"1080\", \"720\", \"480\" or \"576\", not %u) (line %u).\n",
        value,
        lastParsedNodeLineIgsCompilerContext(ctx)
      );
    }

    ctx->data.commonVideoDescriptor.video_height = value;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      " height: %u px.\n",
      ctx->data.commonVideoDescriptor.video_height
    );
  }

  /* video/@framerate : */ {
    float frameRate;
    HdmvFrameRateCode frameRateCode;

    GET_FLOAT(
      &frameRate, 23.976,
      return -1,
      "/igs/parameters/video/@framerate"
    );

    if (0x00 == (frameRateCode = getHdmvFrameRateCode(frameRate))) {
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        "Frame argument value shall be in string format "
        "(\"23.976\", \"24\", \"25\", \"29.970\", \"50\", \"59.94\") "
        "(line %u, %f).\n",
        lastParsedNodeLineIgsCompilerContext(ctx),
        frameRate
      );
    }

    ctx->data.commonVideoDescriptor.frame_rate = frameRateCode;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      " frame-rate id: 0x%u (%.3f).\n",
      ctx->data.commonVideoDescriptor.frame_rate,
      frameRate
    );
  }

  ctx->data.commonUopMask = 0x0;
  if (0 < NB_OBJ("/igs/parameters/uop")) {
    ret = parseParametersUopIgsXmlFile(
      ctx, "/igs/parameters/uop",
      &ctx->data.commonUopMask
    );
    if (ret < 0)
      return -1;
  }

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    " global common UO_mask: 0x%016" PRIX64 ".\n",
    ctx->data.commonUopMask
  );

  return 0;
}

int parseVideoDescriptorIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvVDParameters * param
)
{
  HdmvVDParameters * commonParam;

  assert(NULL != ctx);
  assert(NULL != param);

  /* Copy global settings */
  commonParam = &ctx->data.commonVideoDescriptor;
  param->video_width = commonParam->video_width;
  param->video_height = commonParam->video_height;
  param->frame_rate = commonParam->frame_rate;

  /* TODO ? Add composition specific video settings ? */

  return 0;
}

int parseCompositionDescriptorIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvCDParameters * param
)
{
  xmlChar * string = NULL;

  size_t i;

  static const xmlChar * compStates[] = {
    (xmlChar *) IGS_COMPILER_XML_STATE_NORMAL_CASE_STR,
    (xmlChar *) IGS_COMPILER_XML_STATE_ACQ_PNT_STR,
    (xmlChar *) IGS_COMPILER_XML_STATE_EPOCH_START_STR,
    (xmlChar *) IGS_COMPILER_XML_STATE_EPOCH_CONT_STR
  };

  assert(NULL != ctx);
  assert(NULL != param);

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    " composition_descriptor:\n"
  );

  GET_STRING(
    &string,
    IGS_COMPILER_XML_STATE_EPOCH_START_STR,
    return -1,
    "//composition_descriptor/@state"
  );
  if (NULL == string)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Missing required composition 'state' argument in "
      "'composition_descriptor' node in input XML file.\n"
    );

  param->composition_state = 0xFF;
  for (i = 0; i < ARRAY_SIZE(compStates); i++) {
    if (xmlStrEqual(compStates[i], string)) {
      param->composition_state = i;
      break;
    }
  }

  if (0xFF == param->composition_state)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Invalid composition 'state' argument in "
      "'composition_descriptor' node in input XML file.\n"
    );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "  state: %s (%u).\n",
    string,
    param->composition_state
  );

  /* Compute composition number */

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "  number: %u.\n",
      param->composition_number
    );

  freeXmlCharPtr(&string);
  return 0;
}

int parseCompositionParametersIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvICParameters * param
)
{
  xmlChar * string = NULL;
  uint32_t value;

  size_t i;

  static const xmlChar * streamModels[] = {
    (xmlChar *) IGS_COMPILER_XML_STREAM_MODEL_MULTIPLEXED,
    (xmlChar *) IGS_COMPILER_XML_STREAM_MODEL_OOM
  };
  static const xmlChar * uiModels[] = {
    (xmlChar *) IGS_COMPILER_XML_UI_MODEL_POP_UP,
    (xmlChar *) IGS_COMPILER_XML_UI_MODEL_NORMAL
  };

  param->interactive_composition_length = 0x0; /* Set default */

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    " parameters:\n"
  );

  GET_STRING(
    &string, streamModels[0],
    return -1,
    "//parameters/model/@stream"
  );

  param->stream_model = 0xFF;
  for (i = 0; i < ARRAY_SIZE(streamModels); i++) {
    if (xmlStrEqual(streamModels[i], string)) {
      param->stream_model = i;
      break;
    }
  }

  if (0xFF == param->stream_model)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Unknown stream model 'stream' argument in composition parameters's "
      "model node in input XML file (line %u).\n",
      lastParsedNodeLineIgsCompilerContext(ctx)
    );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "  stream_model: %s (%u).\n",
    string,
    param->stream_model
  );
  freeXmlCharPtr(&string);

  GET_STRING(
    &string, uiModels[1],
    return -1,
    "//parameters/model/@user_interface"
  );

  param->user_interface_model = 0xFF;
  for (i = 0; i < ARRAY_SIZE(uiModels); i++) {
    if (xmlStrEqual(uiModels[i], string)) {
      param->user_interface_model = i;
      break;
    }
  }

  if (0xFF == param->user_interface_model)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Unknown user interface model 'user_interface' argument in composition "
      "parameters's model node in input XML file (line %u).\n",
      lastParsedNodeLineIgsCompilerContext(ctx)
    );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "  user_interface: %s (%u).\n",
    string,
    param->user_interface_model
  );
  freeXmlCharPtr(&string);

  if (param->stream_model == HDMV_STREAM_MODEL_MULTIPLEXED) {
    uint64_t value;

    /* In Mux specific parameters */
    GET_UINT64(
      &value, 0,
      "Expect a positive or zero value for 'composition' option of section "
      "'time_out_pts'",
      return -1,
      "//parameters/time_out_pts/@composition"
    );
    if (0x1FFFFFFFF < value)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        "'composition' option of section 'time_out_pts' value exceed maximum "
        "value 0x1FFFFFFFF or 8,589,934,591 ticks with %u (line %u).\n",
        value,
        lastParsedNodeLineIgsCompilerContext(ctx)
      );

    param->oomRelatedParam.composition_time_out_pts = value & 0x1FFFFFFFF;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "  composition_time_out_pts: %" PRIu64 ".\n",
      param->oomRelatedParam.composition_time_out_pts
    );

    GET_UINT64(
      &value, 0,
      "Expect a positive or zero value for 'selection' option of section "
      "'time_out_pts'",
      return -1,
      "//parameters/time_out_pts/@selection"
    );
    if (0x1FFFFFFFF < value)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        "'selection' option of section 'time_out_pts' value exceed maximum "
        "value of 0x1FFFFFFFF or 8,589,934,591 ticks with %u (line %u).\n",
        value,
        lastParsedNodeLineIgsCompilerContext(ctx)
      );

    param->oomRelatedParam.selection_time_out_pts = value & 0x1FFFFFFFF;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "  selection_time_out_pts: %" PRIu64 ".\n",
      param->oomRelatedParam.selection_time_out_pts
    );
  }

  GET_UINT32(
    &value, 0,
    "Expect a positive or zero value for 'user' option of section "
    "'time_out_pts'",
    return -1,
    "//parameters/time_out_pts/@user"
  );
  if (0xFFFFFF < value)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "'user' option of section 'time_out_pts' exceed maximum value of "
      "0xFFFFFF or 16,777,215 ticks (0x%" PRIX32 ").\n",
      value
    );
  param->user_time_out_duration = value & 0xFFFFFF;

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "  user_time_out_duration: %" PRIu64 ".\n",
    param->user_time_out_duration
  );

  return 0;
}

HdmvPicturePtr parseImgIgsXmlFile(
  IgsCompilerContextPtr ctx,
  lbc ** imgFilename
)
{
  HdmvPicturePtr pic;
  xmlChar * string = NULL;

  lbc * imgPath = NULL;
  uint32_t cuttingX;
  uint32_t cuttingY;
  uint32_t width;
  uint32_t height;

  /* Parse all img fields: */
  /* img/@name : */
  GET_STRING(&string, NULL, return NULL, "//@name");
  if (NULL != imgFilename) {
    /* If string == NULL, passthrough */
    if (NULL == string)
      *imgFilename = NULL;
    else {
      if (NULL == (*imgFilename = lbc_utf8_convto(string)))
        LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN("Memory allocation error.\n");
    }
  }
  freeXmlCharPtr(&string);

  /* img/@path : */
  GET_STRING(&string, NULL, return NULL, "//@path");
  if (NULL == string) {
    printNbObjectsFromExprErrorIgsXmlFile(0, "//@path");
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Missing required 'path' argument on img in input XML file (line %u).\n",
      lastParsedNodeLineIgsCompilerContext(ctx)
    );
  }

  if (NULL == (imgPath = lbc_utf8_convto(string)))
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN("Memory allocation error.\n");
  freeXmlCharPtr(&string);

  /* img/@cutting_x : */
  GET_UINT32(
    &cuttingX, 0,
    "Img's 'cutting_x' optionnal parameter must be positive",
    goto free_return,
    "//@cutting_x"
  );

  /* img/@cutting_y : */
  GET_UINT32(
    &cuttingY, 0,
    "Img's 'cutting_y' optionnal parameter must be positive",
    goto free_return,
    "//@cutting_y"
  );

  /* img/@width : */
  GET_UINT32(
    &width, 0,
    "Img's 'width' optionnal parameter must be positive",
    goto free_return,
    "//@width"
  );

  /* img/@height : */
  GET_UINT32(
    &height, 0,
    "Img's 'height' optionnal parameter must be positive",
    goto free_return,
    "//@height"
  );

  if ((0 < width && width < 8) || (0 < height && height < 8))
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_NRETURN(
      "Image object dimensions cannot be smaller than 8x8 pixels (line %u).\n",
      lastParsedNodeLineIgsCompilerContext(ctx)
    );

  LIBBLU_HDMV_IGS_COMPL_XML_INFO(
    "Loading picture '%" PRI_LBCS "' (line %u).\n",
    imgPath, lastParsedNodeLineIgsCompilerContext(ctx)
  );

  if (NULL == (pic = openHdmvPicture(&ctx->imgLibs, imgPath, ctx->conf)))
    goto free_return;

  if (cropHdmvPicture(pic, cuttingX, cuttingY, width, height) < 0)
    goto free_return;

  free(imgPath);
  freeXmlCharPtr(&string);
  return pic;

free_return:
  free(imgPath);
  freeXmlCharPtr(&string);

  return NULL;
}

HdmvPicturePtr parseRefImgIgsXmlFile(
  IgsCompilerContextPtr ctx
)
{
  HdmvPicturePtr pic;
  xmlChar * string = NULL;
  lbc * name;

  uint32_t cuttingX;
  uint32_t cuttingY;
  uint32_t width;
  uint32_t height;

  /* Parse all img fields: */
  /* ref_pic/@name : */
  string = getStringFromExprIgsXmlFile(XML_CTX(ctx), "//@name");
  if (NULL == string) {
    printNbObjectsFromExprErrorIgsXmlFile(0, "//@name");
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_NRETURN(
      "Missing required 'name' argument on img in input XML file.\n"
    );
  }

  if (NULL == (name = lbc_utf8_convto(string)))
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_NRETURN("Memory allocation error.\n");
  freeXmlCharPtr(&string);

  pic = getRefPictureIgsCompilerComposition(CUR_COMP(ctx), name);
  if (NULL == pic) {
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR(
      "No reference picture called '%" PRI_LBCS "' exits (line: %u).\n",
      name, lastParsedNodeLineIgsCompilerContext(ctx)
    );

    free(name);
    return NULL;
  }
  free(name);

  /* ref_pic/@cutting_x : */
  GET_UINT32(
    &cuttingX, 0,
    "Img's 'cutting_x' optionnal parameter must be positive",
    return NULL,
    "//@cutting_x"
  );

  /* ref_pic/@cutting_y : */
  GET_UINT32(
    &cuttingY, 0,
    "Img's 'cutting_y' optionnal parameter must be positive",
    return NULL,
    "//@cutting_y"
  );

  /* ref_pic/@width : */
  GET_UINT32(
    &width, 0,
    "Img's 'width' optionnal parameter must be positive",
    return NULL,
    "//@width"
  );

  /* ref_pic/@height : */
  GET_UINT32(
    &height, 0,
    "Img's 'height' optionnal parameter must be positive",
    return NULL,
    "//@height"
  );

  if (NULL == (pic = dupHdmvPicture(pic)))
    return NULL;

  if (cropHdmvPicture(pic, cuttingX, cuttingY, width, height) < 0) {
    destroyHdmvPicture(pic);
    return NULL;
  }

  return pic;
}

static int parseReferencePictureIndexerIgsXmlFile(
  IgsCompilerContextPtr ctx,
  xmlXPathObjectPtr obj,
  int idx
)
{
  HdmvPicturePtr pic;
  lbc * name;

  LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("   Setting root...\n");
  if (setRootPathFromPathObjectIgsXmlFile(XML_CTX(ctx), obj, idx) < 0)
    return -1;

  LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("   Parsing image...\n");
  if (NULL == (pic = parseImgIgsXmlFile(ctx, &name)))
    return -1;

  if (NULL == name)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Missing reference image 'name' field (line: %u).\n",
      lastParsedNodeLineIgsCompilerContext(ctx)
    );

  LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("   Adding image to references indexer...\n");
  if (addRefPictureIgsCompilerComposition(CUR_COMP(ctx), pic, name) < 0)
    goto free_return;
  free(name);

  LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("   Restoring path status...\n");
  if (restoreLastRootIgsXmlFile(XML_CTX(ctx)) < 0)
    return -1;
  return 0;

free_return:
  destroyHdmvPicture(pic);
  free(name);
  return -1;
}

int parseReferencePicturesIndexerIgsXmlFile(
  IgsCompilerContextPtr ctx
)
{
  xmlXPathObjectPtr imgPathObj;
  int i, nbImg;

  /* Picture references : */
  if (NULL == (imgPathObj = GET_OBJ("//reference_imgs/img")))
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Broken XML tree, 'reference_imgs' section is empty.\n"
    );
  nbImg = XML_PATH_NODE_NB(imgPathObj);

  if (0 < nbImg)
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(" reference_imgs:\n");

  for (i = 0; i < nbImg; i++) {
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("  reference_img[%d]:\n", i);
    if (parseReferencePictureIndexerIgsXmlFile(ctx, imgPathObj, i) < 0)
      goto free_return;
  }

  xmlXPathFreeObject(imgPathObj);
  return 0;

free_return:
  xmlXPathFreeObject(imgPathObj);
  return -1;
}

int parsePageBogIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvButtonOverlapGroupParameters * bog,
  int index,
  uint16_t * nextButtonId
)
{
  int nbButtons;
  int i, j;

  bool automatic_default_valid_button_id_ref;
  bool default_valid_button_id_refPresent;

  assert(NULL != ctx);
  assert(NULL != bog);
  assert(NULL != nextButtonId);

  /* bog/default_valid_button */
  GET_UINT16(
    &bog->default_valid_button_id_ref, 0xFFFF,
    "BOG's 'default_valid_button' optionnal parameter must be between "
    "0x0000 and 0xFFFF inclusive",
    return -1,
    "//bog[%d]/default_valid_button",
    index+1
  );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "     default_valid_button: 0x%" PRIX16 ".\n",
    bog->default_valid_button_id_ref
  );
  default_valid_button_id_refPresent =
    (0xFFFF == bog->default_valid_button_id_ref)
  ;
  automatic_default_valid_button_id_ref =
    !EXISTS("//bog[%d]/default_valid_button", index+1)
  ;

  nbButtons = NB_OBJ("//bog[%d]/button", index+1);
  if (HDMV_MAX_NB_ICS_BUTTONS < nbButtons)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Number of buttons in BOG %d at line %u in input XML file is invalid "
      "(%d, shall be between 0-255).\n",
      index,
      lastParsedNodeLineIgsCompilerContext(ctx),
      nbButtons
    );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "     buttons:\n"
  );

  if (!nbButtons) {
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "   None.\n"
    );
    LIBBLU_HDMV_IGS_COMPL_XML_WARNING(
      "BOG %d at line %u is empty.\n",
      index, lastParsedNodeLineIgsCompilerContext(ctx)
    );
  }

  bog->number_of_buttons = 0;
  for (i = 0; i < nbButtons; i++) {
    HdmvButtonParam * btn;
    int nbNavigationCommands;

    btn = getHdmvBtnParamHdmvSegmentsInventory(ctx->inv);
    if (NULL == btn)
      return -1;

    assert(NULL == btn->navigation_commands);

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "      button[%d]:\n",
      i
    );

    /* bog/button[i]/@id */
    GET_UINT16(
      &btn->button_id,
      *nextButtonId,
      "Button's 'id' optionnal parameter must be between "
      "0x0000 and 0x1FDF inclusive",
      return -1,
      "//bog[%d]/button[%d]/@id",
      index+1, i+1
    );
    *nextButtonId = btn->button_id + 1;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "       button_id: 0x%" PRIX16 ".\n",
      btn->button_id
    );

    if (0x1FDF < btn->button_id)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR(
        "Unexpected 'button_id' value, values shall be between 0x0000 "
        "and 0x1FDF inclusive.\n"
      );

    if (0 < i && btn->button_id <= bog->buttons[i-1]->button_id) {
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR(
        "BOG's button ids order broken (duplicated values or broken order) "
        "(line: %u).\n",
        lastParsedNodeLineIgsCompilerContext(ctx)
      );
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        " -> Buttons' ids shall strictly grows "
        "(and not share same values).\n"
      );
    }

    if (btn->button_id == bog->default_valid_button_id_ref)
      default_valid_button_id_refPresent = true;

    /* bog/button[i]/button_numeric_select_value */
    GET_UINT16(
      &btn->button_numeric_select_value,
      0xFFFF,
      "Button's 'button_numeric_select_value' optionnal parameter must be "
      "between 0x0000 and 0xFFFF inclusive",
      return -1,
      "//bog[%d]/button[%d]/button_numeric_select_value",
      index+1, i+1
    );

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "       button_numeric_select_value: 0x%" PRIX16 ".\n",
      btn->button_numeric_select_value
    );

    /* bog/button[i]/auto_action_flag */
    GET_BOOL(
      &btn->auto_action,
      false,
      return -1,
      "//bog[%d]/button[%d]/auto_action_flag",
      index+1, i+1
    );

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "       auto_action_flag: %s (0b%x).\n",
      BOOL_STR(btn->auto_action),
      btn->auto_action
    );

    /* bog/button[i]/horizontal_position */
    GET_UINT16(
      &btn->button_horizontal_position,
      -1,
      "Missing or invalid button's 'horizontal_position' parameter, "
      "must be between 0x0000 and 0xFFFF inclusive",
      return -1,
      "//bog[%d]/button[%d]/horizontal_position",
      index+1, i+1
    );

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "       horizontal_position: %" PRIu16 " px.\n",
      btn->button_horizontal_position
    );

    /* bog/button[i]/vertical_position */
    GET_UINT16(
      &btn->button_vertical_position,
      -1,
      "Missing or invalid button's 'vertical_position' parameter, "
      "must be between 0x0000 and 0xFFFF inclusive",
      return -1,
      "//bog[%d]/button[%d]/vertical_position",
      index+1, i+1
    );

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "       vertical_position: %" PRIu16 " px.\n",
      btn->button_vertical_position
    );

    /* neighbor_info */
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("       neighbor_info:\n");

    if (0 < NB_OBJ("//bog[%d]/button[%d]/neighbor_info", index+1, i+1)) {
      GET_UINT16(
        &btn->neighbor_info.upper_button_id_ref,
        btn->button_id,
        "Missing or invalid button's neighbor 'upper_button' parameter, "
        "must be between 0x0000 and 0xFFFF inclusive (line %u).\n",
        return -1,
        "//bog[%d]/button[%d]/neighbor_info/upper_button",
        index+1, i+1
      );

      GET_UINT16(
        &btn->neighbor_info.lower_button_id_ref,
        btn->button_id,
        "Missing or invalid button's neighbor 'lower_button' parameter, "
        "must be between 0x0000 and 0xFFFF inclusive (line %u).\n",
        return -1,
        "//bog[%d]/button[%d]/neighbor_info/lower_button",
        index+1, i+1
      );

      GET_UINT16(
        &btn->neighbor_info.left_button_id_ref,
        btn->button_id,
        "Missing or invalid button's neighbor 'left_button' parameter, "
        "must be between 0x0000 and 0xFFFF inclusive (line %u).\n",
        return -1,
        "//bog[%d]/button[%d]/neighbor_info/left_button",
        index+1, i+1
      );

      GET_UINT16(
        &btn->neighbor_info.right_button_id_ref,
        btn->button_id,
        "Missing or invalid button's neighbor 'right_button' parameter, "
        "must be between 0x0000 and 0xFFFF inclusive (line %u).\n",
        return -1,
        "//bog[%d]/button[%d]/neighbor_info/right_button",
        index+1, i+1
      );
    }
    else
      btn->neighbor_info.upper_button_id_ref = btn->button_id,
      btn->neighbor_info.lower_button_id_ref = btn->button_id,
      btn->neighbor_info.left_button_id_ref  = btn->button_id,
      btn->neighbor_info.right_button_id_ref = btn->button_id
    ;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        upper_button: 0x%" PRIX16 ".\n",
      btn->neighbor_info.upper_button_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        lower_button: 0x%" PRIX16 ".\n",
      btn->neighbor_info.lower_button_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        left_button:  0x%" PRIX16 ".\n",
      btn->neighbor_info.left_button_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        right_button: 0x%" PRIX16 ".\n",
      btn->neighbor_info.right_button_id_ref
    );

    /* Normal state */
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "       normal_state_info:\n"
    );

    if (EXISTS("//bog[%d]/button[%d]/normal_state_info", index+1, i+1)) {
      xmlXPathObjectPtr imgPathObj;
      int nbStatesImgs;

      LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("        objects:\n");

      imgPathObj = GET_OBJ(
        "//bog[%d]/button[%d]/normal_state_info/graphic/*",
        index+1, i+1
      );
      nbStatesImgs = XML_PATH_NODE_NB(imgPathObj);

      if (!nbStatesImgs)
        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("        None.\n");

      for (j = 0; j < nbStatesImgs; j++) {
        HdmvPicturePtr pic;
        unsigned id;
        bool refPic;

        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("         object[%d]:\n", j);

        if (!strcmp((char *) XML_PATH_NODE(imgPathObj, j)->name, "img"))
          refPic = false;
        else if (!strcmp((char *) XML_PATH_NODE(imgPathObj, j)->name, "ref_pic"))
          refPic = true;
        else
          continue; /* Unknown/unsupported node */

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Setting root...\n");
        if (setRootPathFromPathObjectIgsXmlFile(XML_CTX(ctx), imgPathObj, j) < 0)
          return -1;

        if (!refPic) {
          LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Parsing image...\n");
          if (NULL == (pic = parseImgIgsXmlFile(ctx, NULL)))
            return -1;
        }
        else {
          LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Parsing reference image...\n");
          if (NULL == (pic = parseRefImgIgsXmlFile(ctx)))
            return -1;
        }

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Adding image to objects...\n");

        if (addObjectIgsCompilerComposition(CUR_COMP(ctx), pic, &id) < 0)
          return -1;
        if (UINT16_MAX <= id)
          LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN("Too many objects.\n");

        if (j == 0)
          btn->normal_state_info.start_object_id_ref = id;
        if (j == nbStatesImgs-1)
          btn->normal_state_info.end_object_id_ref = id;

        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
          "          object_id: 0x%04X.\n",
          id
        );

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("   Restoring path status...\n");
        if (restoreLastRootIgsXmlFile(XML_CTX(ctx)) < 0)
          return -1;
      }

      xmlXPathFreeObject(imgPathObj);

      GET_BOOL(
        &btn->normal_state_info.repeat_flag,
        false,
        return -1,
        "//bog[%d]/button[%d]/normal_state_info/repeat_flag",
        index+1, i+1
      );

      GET_BOOL(
        &btn->normal_state_info.complete_flag,
        false,
        return -1,
        "//bog[%d]/button[%d]/normal_state_info/complete_flag",
        index+1, i+1
      );
    }

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        start_object_id_ref: 0x%04" PRIX16 ".\n",
      btn->normal_state_info.start_object_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        end_object_id_ref:   0x%04" PRIX16 ".\n",
      btn->normal_state_info.end_object_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        repeat_flag:         %s (0b%x).\n",
      BOOL_STR(btn->normal_state_info.repeat_flag),
      btn->normal_state_info.repeat_flag
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        complete_flag:       %s (0b%x).\n",
      BOOL_STR(btn->normal_state_info.complete_flag),
      btn->normal_state_info.complete_flag
    );

    /* Selected state */
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("       selected_state_info:\n");

    if (EXISTS("//bog[%d]/button[%d]/selected_state_info", index+1, i+1)) {
      xmlXPathObjectPtr imgPathObj;
      int nbStatesImgs;

      GET_UINT8(
        &btn->selected_state_info.state_sound_id_ref, 0xFF,
        "Invalid button's selected state 'sound_id' parameter, "
        "must be between 0x00 and 0xFF inclusive",
        return -1,
        "//bog[%d]/button[%d]/selected_state_info/sound_id",
        index+1, i+1
      );

      LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("        objects:\n");

      imgPathObj = GET_OBJ(
        "//bog[%d]/button[%d]/selected_state_info/graphic/*",
        index+1, i+1
      );
      nbStatesImgs = XML_PATH_NODE_NB(imgPathObj);

      if (!nbStatesImgs)
        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("        None.\n");

      for (j = 0; j < nbStatesImgs; j++) {
        HdmvPicturePtr pic;
        unsigned id;
        bool refPic;

        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("         object[%d]:\n", j);

        if (!strcmp((char *) XML_PATH_NODE(imgPathObj, j)->name, "img"))
          refPic = false;
        else if (!strcmp((char *) XML_PATH_NODE(imgPathObj, j)->name, "ref_pic"))
          refPic = true;
        else
          continue; /* Unknown/unsupported node */

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Setting root...\n");
        if (setRootPathFromPathObjectIgsXmlFile(XML_CTX(ctx), imgPathObj, j) < 0)
          return -1;

        if (!refPic) {
          LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Parsing image...\n");
          if (NULL == (pic = parseImgIgsXmlFile(ctx, NULL)))
            return -1;
        }
        else {
          LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Parsing reference image...\n");
          if (NULL == (pic = parseRefImgIgsXmlFile(ctx)))
            return -1;
        }

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Adding image to objects...\n");

        if (addObjectIgsCompilerComposition(CUR_COMP(ctx), pic, &id) < 0)
          return -1;
        if (UINT16_MAX <= id)
          LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN("Too many objects.\n");

        if (j == 0)
          btn->selected_state_info.start_object_id_ref = id;
        if (j == nbStatesImgs-1)
          btn->selected_state_info.end_object_id_ref = id;

        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
          "          object_id: 0x%04X.\n",
          id
        );

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("   Restoring path status...\n");
        if (restoreLastRootIgsXmlFile(XML_CTX(ctx)) < 0)
          return -1;
      }

      xmlXPathFreeObject(imgPathObj);

      GET_BOOL(
        &btn->selected_state_info.repeat_flag,
        false,
        return -1,
        "//bog[%d]/button[%d]/selected_state_info/repeat_flag",
        index+1, i+1
      );

      GET_BOOL(
        &btn->selected_state_info.complete_flag,
        false,
        return -1,
        "//bog[%d]/button[%d]/selected_state_info/complete_flag",
        index+1, i+1
      );
    }

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        start_object_id_ref: 0x%02" PRIX8 ".\n",
      btn->selected_state_info.state_sound_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        start_object_id_ref: 0x%04" PRIX16 ".\n",
      btn->selected_state_info.start_object_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        end_object_id_ref:   0x%04" PRIX16 ".\n",
      btn->selected_state_info.end_object_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        repeat_flag:         %s (0b%x).\n",
      BOOL_STR(btn->selected_state_info.repeat_flag),
      btn->selected_state_info.repeat_flag
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        complete_flag:       %s (0b%x).\n",
      BOOL_STR(btn->selected_state_info.complete_flag),
      btn->selected_state_info.complete_flag
    );

    /* Activated state */
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("       activated_state_info:\n");

    if (EXISTS("//bog[%d]/button[%d]/activated_state_info", index+1, i+1)) {
      xmlXPathObjectPtr imgPathObj;
      int nbStatesImgs;

      GET_UINT8(
        &btn->activated_state_info.state_sound_id_ref, 0xFF,
        "Invalid button's activated state 'sound_id' parameter, "
        "must be between 0x00 and 0xFF inclusive",
        return -1,
        "//bog[%d]/button[%d]/activated_state_info/sound_id",
        index+1, i+1
      );

      LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("        objects:\n");

      imgPathObj = GET_OBJ(
        "//bog[%d]/button[%d]/activated_state_info/graphic/*",
        index+1, i+1
      );
      nbStatesImgs = XML_PATH_NODE_NB(imgPathObj);

      if (!nbStatesImgs)
        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("        None.\n");

      for (j = 0; j < nbStatesImgs; j++) {
        HdmvPicturePtr pic;
        unsigned id;
        bool refPic;

        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("         object[%d]:\n", j);

        if (!strcmp((char *) XML_PATH_NODE(imgPathObj, j)->name, "img"))
          refPic = false;
        else if (!strcmp((char *) XML_PATH_NODE(imgPathObj, j)->name, "ref_pic"))
          refPic = true;
        else
          continue; /* Unknown/unsupported node */

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Setting root...\n");
        if (setRootPathFromPathObjectIgsXmlFile(XML_CTX(ctx), imgPathObj, j) < 0)
          return -1;

        if (!refPic) {
          LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Parsing image...\n");
          if (NULL == (pic = parseImgIgsXmlFile(ctx, NULL)))
            return -1;
        }
        else {
          LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Parsing reference image...\n");
          if (NULL == (pic = parseRefImgIgsXmlFile(ctx)))
            return -1;
        }

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("          Adding image to objects...\n");

        if (addObjectIgsCompilerComposition(CUR_COMP(ctx), pic, &id) < 0)
          return -1;
        if (UINT16_MAX <= id)
          LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN("Too many objects.\n");

        if (j == 0)
          btn->activated_state_info.start_object_id_ref = id;
        if (j == nbStatesImgs-1)
          btn->activated_state_info.end_object_id_ref = id;

        LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
          "          object_id: 0x%04X.\n",
          id
        );

        LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("   Restoring path status...\n");
        if (restoreLastRootIgsXmlFile(XML_CTX(ctx)) < 0)
          return -1;
      }

      xmlXPathFreeObject(imgPathObj);
    }

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        start_object_id_ref: 0x%02" PRIX8 ".\n",
      btn->activated_state_info.state_sound_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        start_object_id_ref: 0x%04" PRIX16 ".\n",
      btn->activated_state_info.start_object_id_ref
    );
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "        end_object_id_ref:   0x%04" PRIX16 ".\n",
      btn->activated_state_info.end_object_id_ref
    );

    /* Commands */
    nbNavigationCommands = NB_OBJ(
      "//bog[%d]/button[%d]/commands/command",
      index+1, i+1
    );

    if (HDMV_MAX_NB_ICS_BUTTONS < nbButtons)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        "Number of buttons in BOG %d at line %u in input XML file is invalid "
        "(%d, shall be between 0-255).\n",
        index,
        lastParsedNodeLineIgsCompilerContext(ctx),
        nbButtons
      );

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("      navigation_commands:\n");
    if (!nbNavigationCommands)
      LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("       *no command*\n");

    {
      HdmvNavigationCommand * last = NULL;

      for (j = 0; j < nbNavigationCommands; j++) {
        HdmvNavigationCommand * com;

        if (NULL == (com = getHdmvNaviComHdmvSegmentsInventory(ctx->inv)))
          return -1;

        if (NULL != last)
          setNextHdmvNavigationCommand(last, com);
        else
          btn->navigation_commands = com;

        /* command/@code */
        GET_UINT32(
          &com->opcode, -1,
          "Missing or invalid navigation command 'code' field, "
          "must be between 0x00000000 and 0xFFFFFFFF inclusive",
          return -1,
          "//bog[%d]/button[%d]/commands/command[%d]/@code",
          index+1, i+1, j+1
        );

        /* command/@destination */
        GET_UINT32(
          &com->destination, -1,
          "Missing or invalid navigation command 'destination' field, "
          "must be between 0x00000000 and 0xFFFFFFFF inclusive",
          return -1,
          "//bog[%d]/button[%d]/commands/command[%d]/@destination",
          index+1, i+1, j+1
        );

        /* command/@source */
        GET_UINT32(
          &com->source, -1,
          "Missing or invalid navigation command 'source' field, "
          "must be between 0x00000000 and 0xFFFFFFFF inclusive",
          return -1,
          "//bog[%d]/button[%d]/commands/command[%d]/@source",
          index+1, i+1, j+1
        );

        last = com;
      }

      btn->number_of_navigation_commands = nbNavigationCommands;
    }

    bog->buttons[bog->number_of_buttons++] = btn;
  }

  if (!default_valid_button_id_refPresent)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "BOG %d 'default_valid_button_id' does not match with any button.\n",
      index
    );

  if (automatic_default_valid_button_id_ref && 0 < bog->number_of_buttons)
    bog->default_valid_button_id_ref = bog->buttons[0]->button_id;

  return 0;
}

int parsePageIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvPageParameters * page,
  int index,
  uint8_t * nextPageId
)
{
  unsigned i, nbBogs;
  uint16_t button_id;

  xmlXPathObjectPtr bogPathObj;

  bool automaticDefaultSelectedButtonId;
  bool defaultSelectedButtonIdPresent;
  bool defaultActivatedButtonIdPresent;

  assert(NULL != ctx);
  assert(NULL != page);
  assert(0 <= index && index < UINT8_MAX);

  GET_UINT8(
    &page->page_id,
    *nextPageId,
    "Page's 'page_id' optionnal parameter must be between "
    "0x00 and 0xFE inclusive",
    return -1,
    "//page[%d]/@id",
    index+1
  );
  *nextPageId = page->page_id + 1;

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "   page_id: %" PRIu8 ".\n",
    page->page_id
  );

  if (0xFE < page->page_id)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR(
      "Unexpected 'page_id' value, values shall be between 0x00 "
      "and 0xFE inclusive.\n"
    );

#if 0
  /* page/@version */
  GET_UINT8(
    &page->page_version_number, 0x00,
    "Page's 'version' optionnal parameter must be between 0 and 255 inclusive",
    return -1,
    "//page[%d]/@version",
    index+1
  );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "   @version: %" PRIu8 ".\n",
    page->page_version_number
  );
#endif
  page->page_version_number = 0x00;

  /* page/parameters/uop */
  page->UO_mask_table = ctx->data.commonUopMask; /* Applying global flags. */

  if (EXISTS("//page[%d]/parameters/uop", index+1)) {
    char uopPath[50];
    snprintf(uopPath, 50, "//page[%d]/parameters/uop", index+1);

    if (parseParametersUopIgsXmlFile(ctx, uopPath, &page->UO_mask_table) < 0)
      return -1;
  }
  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "   uop: 0x%016" PRIX64 ".\n",
    page->UO_mask_table
  );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "   parameters:\n"
  );

  /* page/parameters/animation_frame_rate_code */
  GET_UINT8(
    &page->animation_frame_rate_code, 0x00,
    "Page's parameter 'animation_frame_rate_code' optionnal parameter must "
    "be between 0 and 255 inclusive",
    return -1,
    "//page[%d]/parameters/animation_frame_rate_code",
    index+1
  );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "    animation_frame_rate_code: %" PRIu8 ".\n",
    page->animation_frame_rate_code
  );

  /* page/parameters/default_selected_button */
  GET_UINT16(
    &page->default_selected_button_id_ref, 0xFFFF,
    "Page's parameter 'default_selected_button' optionnal parameter must be "
    "between 0x0000 and 0xFFFF inclusive",
    return -1,
    "//page[%d]/parameters/default_selected_button",
    index+1
  );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "    default_selected_button: 0x%04" PRIX16 ".\n",
    page->default_selected_button_id_ref
  );

  automaticDefaultSelectedButtonId =
    !EXISTS("//page[%d]/parameters/default_selected_button", index+1)
  ;
  defaultSelectedButtonIdPresent =
    (0xFFFF == page->default_selected_button_id_ref)
  ;

  /* page/parameters/default_activated_button */
  GET_UINT16(
    &page->default_activated_button_id_ref, 0xFFFF,
    "Page's parameter 'default_selected_button' optionnal parameter must be "
    "between 0x0000 and 0xFFFF inclusive",
    return -1,
    "//page[%d]/parameters/default_activated_button",
    index+1
  );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "    default_activated_button: 0x%04" PRIX16 ".\n",
    page->default_activated_button_id_ref
  );

  defaultActivatedButtonIdPresent =
    (0xFFFE <= page->default_activated_button_id_ref)
  ;

  /* page/in_effects */
  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "   in_effects:\n"
  );
  /* TODO */
  memset(&page->in_effects, 0x0, sizeof(HdmvEffectSequenceParameters));
  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "    TODO\n"
  );

  /* page/out_effects */
  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "   out_effects:\n"
  );
  /* TODO */
  memset(&page->out_effects, 0x0, sizeof(HdmvEffectSequenceParameters));
  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "    TODO\n"
  );

  /* page/bog */
  nbBogs = NB_OBJ("//page[%d]/bog", index+1);
  if (HDMV_MAX_NB_ICS_BOGS < nbBogs)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Number of BOGs in IGS page %d described in input XML file is invalid "
      "(%d, shall be between 1-255).\n",
      index, nbBogs
    );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    "   BOGs (Button Overlap Groups):\n"
  );

  if (!nbBogs) {
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "    None.\n"
    );
    LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN(
      "Page %d is empty, expect at least one BOG.\n",
      index
    );
  }

  /* Set path for relative path accessing: */
  if (NULL == (bogPathObj = GET_OBJ("//page[%d]", index+1)))
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Broken XML tree, missing 'page' section in 'composition'.\n"
    );

  if (setRootPathFromPathObjectIgsXmlFile(XML_CTX(ctx), bogPathObj, 0) < 0)
    goto free_return;

  button_id = 0x0000;
  page->number_of_BOGs = 0;

  for (i = 0; i < nbBogs; i++) {
    HdmvButtonOverlapGroupParameters * bog;
    unsigned j;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "    bog[%d]:\n",
      i
    );

    if (NULL == (bog = getHdmvBOGParamHdmvSegmentsInventory(ctx->inv)))
      goto free_return;

    /* page/bog[i] */
    if (parsePageBogIgsXmlFile(ctx, bog, i, &button_id) < 0)
      goto free_return;

    if (bog->default_valid_button_id_ref == page->default_selected_button_id_ref)
      defaultSelectedButtonIdPresent = true;
    for (j = 0; j < bog->number_of_buttons; j++) {
      if (bog->buttons[j]->button_id == page->default_activated_button_id_ref)
        defaultActivatedButtonIdPresent = true;
    }

    page->bogs[page->number_of_BOGs++] = bog;
  }

  if (!defaultSelectedButtonIdPresent)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Page %d (id: %u) default selected button id does not match any "
      "page's BOG's default valid button id.\n",
      index,
      page->page_id
    );

  if (!defaultActivatedButtonIdPresent)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Page %d (id: %u) default activated button id does not match any "
      "page's button id.\n",
      index,
      page->page_id
    );

  /* Restore original path */
  if (restoreLastRootIgsXmlFile(XML_CTX(ctx)) < 0)
    goto free_return;
  xmlXPathFreeObject(bogPathObj);

  if (automaticDefaultSelectedButtonId && 0 < page->number_of_BOGs)
    page->default_selected_button_id_ref = page->bogs[0]->buttons[0]->button_id;

  page->palette_id_ref = 0x00; /* Apply default palette ID */

  return 0;

free_return:
  xmlXPathFreeObject(bogPathObj);
  return -1;
}

int parsePagesIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvICParameters * compo
)
{
  int i, nbPages;
  uint8_t page_id;

  nbPages = NB_OBJ("//page");
  if (nbPages < 1 || HDMV_MAX_NB_ICS_PAGES < nbPages)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Number of IGS pages described in input XML file is invalid "
      "(shall be between 1-255).\n"
    );

  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
    " pages:\n"
  );

  page_id = 0x00;
  compo->number_of_pages = 0;

  for (i = 0; i < nbPages; i++) {
    HdmvPageParameters * page;

    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "  page[%d]:\n",
      i
    );

    if (NULL == (page = getHdmvPageParamHdmvSegmentsInventory(ctx->inv)))
      return -1;

    if (parsePageIgsXmlFile(ctx, page, i, &page_id) < 0)
      return -1;

    compo->pages[compo->number_of_pages++] = page;
  }

  return 0;
}

int parseCompositionIgsXmlFile(
  IgsCompilerContextPtr ctx,
  IgsCompilerCompositionPtr compo
)
{
  /* Presentation timecode */
  if (addTimecodeIgsCompilerContext(ctx, 0) < 0)
    return -1;
  /* TODO: Enable custom */

  /* Video descriptor from common */
  if (parseVideoDescriptorIgsXmlFile(ctx, &compo->video_descriptor) < 0)
    return -1;

  /* Composition Descriptor */
  if (parseCompositionDescriptorIgsXmlFile(ctx, &compo->composition_descriptor) < 0)
    return -1;

  if (parseCompositionParametersIgsXmlFile(ctx, &compo->interactiveComposition) < 0)
    return -1;

  if (parseReferencePicturesIndexerIgsXmlFile(ctx) < 0)
    return -1;

  if (parsePagesIgsXmlFile(ctx, &compo->interactiveComposition) < 0)
    return -1;

  return 0;
}

int parseCompositionsIgsXmlFile(
  IgsCompilerContextPtr ctx
)
{
  xmlXPathObjectPtr compPathObj;
  IgsCompilerCompositionPtr compo;
  int nbCompos, i;

  if (NULL == (compPathObj = GET_OBJ("/igs/composition")))
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Broken XML tree, missing 'composition' section.\n"
    );
  nbCompos = XML_PATH_NODE_NB(compPathObj);

  if (HDMV_MAX_NB_ICS_COMPOS < nbCompos)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Number of IGS compositions described in input XML file is invalid "
      "(only one shall be present).\n"
    );

  ctx->data.nbCompo = 0;
  for (i = 0; i < nbCompos; i++) {
    LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(
      "composition[%d]:\n",
      i
    );

    LIBBLU_HDMV_IGS_COMPL_XML_DEBUG(
      " Initialization of IGS Compiler Composition data.\n"
    );

    if (NULL == (compo = createIgsCompilerComposition()))
      goto free_return;
    ctx->data.compositions[ctx->data.nbCompo++] = compo;

    if (setRootPathFromPathObjectIgsXmlFile(XML_CTX(ctx), compPathObj, i) < 0)
      goto free_return;

    if (parseCompositionIgsXmlFile(ctx, compo) < 0)
      goto free_return;

    if (restoreLastRootIgsXmlFile(XML_CTX(ctx)) < 0)
      goto free_return;
  }

  xmlXPathFreeObject(compPathObj);
  return 0;

free_return:
  xmlXPathFreeObject(compPathObj);
  return -1;
}

int parseIgsXmlFile(
  IgsCompilerContextPtr ctx
)
{
  int ret;

  if (loadIgsXmlFile(XML_CTX(ctx), ctx->xmlFilename) < 0)
    goto free_return;

  if (!(ret = NB_OBJ("/igs"))) {
    printNbObjectsFromExprErrorIgsXmlFile(ret, "/igs");
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Input file is not a proper IGS Compiler XML file "
      "(missing 'igs' root node).\n"
    );
  }

  if (parseCommonSettingsIgsXmlFile(ctx) < 0)
    goto free_return;

  if (parseCompositionsIgsXmlFile(ctx) < 0)
    goto free_return;

  releaseIgsXmlFile(XML_CTX(ctx));
  return 0;

free_return:
  releaseIgsXmlFile(XML_CTX(ctx));
  return -1;
}
