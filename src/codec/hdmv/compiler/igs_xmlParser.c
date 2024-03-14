#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

#include "igs_xmlParser.h"
#include "../../../util/errorCodesVa.h"

#define PDEBUG_PARSE(il, fmt, ...)                                            \
  LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG("%-*c"fmt, il, ' ', ##__VA_ARGS__)
#define PDEBUG(il, fmt, ...)                                                  \
  LIBBLU_HDMV_IGS_COMPL_XML_DEBUG("%-*c"fmt, il, ' ', ##__VA_ARGS__)

#define NB_OBJ(path, ...)                                                     \
  getNbObjectsFromExprXmlCtx(ctx->xml_ctx, lbc_str(path), ##__VA_ARGS__)

#define GET_OBJ(path, ...)                                                    \
  getPathObjectFromExprXmlCtx(ctx->xml_ctx, lbc_str(path), ##__VA_ARGS__)

#define EXISTS(path, ...)                                                     \
  existsPathObjectFromExprXmlCtx(ctx->xml_ctx, lbc_str(path), ##__VA_ARGS__)

#define GET_STRING(dst, def, err_instr, path, ...)                            \
  do {                                                                        \
    int _ret;                                                                 \
                                                                              \
    _ret = getIfExistsStringFromExprXmlCtx(                               \
      ctx->xml_ctx, dst, def, lbc_str(path), ##__VA_ARGS__                    \
    );                                                                        \
    if (_ret < 0)                                                             \
      err_instr;                                                              \
  } while (0)

#define GET_BOOL(dst, def, err_instr, path, ...)                              \
  do {                                                                        \
    int _ret;                                                                 \
    bool _val;                                                                \
                                                                              \
    _ret = getIfExistsBooleanFromExprXmlCtx(                              \
      ctx->xml_ctx, &_val, def, lbc_str(path), ##__VA_ARGS__                  \
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
    _ret = getIfExistsFloatFromExprXmlCtx(                                \
      ctx->xml_ctx, &_val, def, lbc_str(path), ##__VA_ARGS__                  \
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
    _ret = getIfExistsUint64FromExprXmlCtx(                               \
      ctx->xml_ctx, &_val, def, lbc_str(path), ##__VA_ARGS__                  \
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
    _ret = getIfExistsUint64FromExprXmlCtx(                               \
      ctx->xml_ctx, &_val, def, lbc_str(path), ##__VA_ARGS__                  \
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

static int _parseParametersUop(
  IgsCompilerContext *ctx,
  uint64_t *mask_ret_ref
)
{
  uint64_t mask;
  /* uop/mask */ {
    GET_UINT64(
      &mask, *mask_ret_ref,
      "Invalid UOP mask field, expect a valid unsigned value.\n",
      return -1,
      "//mask"
    );
  }

  /* uop/<User Operation name> */
#define GET_UOP(m, n, f)                                                      \
  do {                                                                        \
    bool _bl;                                                                 \
    GET_BOOL(&_bl, !((*(m) >> (n)) & 0x1), goto free_return, "//"#f);         \
    *(m) = (*(m) & ~(1llu << (n))) | ((uint64_t) !_bl) << (n);                \
  } while (0)

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

  *mask_ret_ref = mask;

  return 0;

free_return:
  return -1;
}

static int _parseCommonSettings(
  IgsCompilerContext *ctx
)
{
  PDEBUG_PARSE(0, "common settings:\n");

  HdmvVDParameters *cm_vd = &ctx->data.common_video_desc;

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
        "(\"1920\", \"1440\", \"1280\" or \"720\", not %u).\n",
        value
      );
    }

    cm_vd->video_width = value;
    PDEBUG_PARSE(1, "width: %" PRIu16 " px.\n", cm_vd->video_width);
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
        "(\"1080\", \"720\", \"480\" or \"576\", not %u).\n",
        value
      );
    }

    cm_vd->video_height = value;
    PDEBUG_PARSE(1, "height: %" PRIu16 " px.\n", cm_vd->video_height);
  }

  /* video/@framerate : */ {
    float frame_rate;
    GET_FLOAT(
      &frame_rate, 23.976f,
      return -1,
      "/igs/parameters/video/@framerate"
    );

    HdmvFrameRateCode frame_rate_code = getHdmvFrameRateCode(frame_rate);
    if (0x00 == frame_rate_code)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        "Frame argument value shall be in string format "
        "(\"23.976\", \"24\", \"25\", \"29.970\", \"50\", \"59.94\", "
        "not %f).\n",
        frame_rate
      );

    cm_vd->frame_rate = frame_rate_code;
    PDEBUG_PARSE(1, "frame-rate id: 0x%u (%.3f).\n", cm_vd->frame_rate, frame_rate);
  }

  uint64_t uop_mask = 0x0;
  XmlXPathObjectPtr uop_path_obj = GET_OBJ("/igs/parameters/uop");

  if (existsNodeXmlXPathObject(uop_path_obj, 0)) {
    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, uop_path_obj, 0) < 0) {
      destroyXmlXPathObject(uop_path_obj);
      return -1;
    }

    if (_parseParametersUop(ctx, &uop_mask) < 0) {
      destroyXmlXPathObject(uop_path_obj);
      return -1;
    }

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0) {
      destroyXmlXPathObject(uop_path_obj);
      return -1;
    }
  }

  destroyXmlXPathObject(uop_path_obj);

  ctx->data.common_uop_mask = uop_mask;
  PDEBUG_PARSE(1, "global common UO_mask: 0x%016" PRIX64 ".\n", uop_mask);

  return 0;
}

static int _parseVideoDescriptor(
  IgsCompilerContext *ctx,
  HdmvVDParameters *param
)
{
  /* Copy global settings */
  HdmvVDParameters *vd = &ctx->data.common_video_desc;
  param->video_width = vd->video_width;
  param->video_height = vd->video_height;
  param->frame_rate = vd->frame_rate;

  // TODO: Add composition specific video settings ?
  return 0;
}

static int _parseCompositionState(
  IgsCompilerContext *ctx,
  HdmvCompositionState *ret
)
{
  lbc *string = NULL;

  static const lbc *strings[] = {
    lbc_str("NormalCase"),
    lbc_str("AcquisitionPoint"),
    lbc_str("EpochStart"),
    lbc_str("EpochContinue")
  };

  GET_STRING(
    &string,
    strings[2], // Epoch Start by default
    return -1,
    "//composition_descriptor/@state"
  );
  if (NULL == string)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Missing required composition 'state' argument in "
      "'composition_descriptor' node in input XML file.\n"
    );

  uint8_t composition_state = 0xFF;
  for (size_t i = 0; i < ARRAY_SIZE(strings); i++) {
    if (lbc_equal(strings[i], string)) {
      composition_state = i;
      break;
    }
  }

  if (0xFF == composition_state)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Invalid composition 'state' argument in "
      "'composition_descriptor' node in input XML file.\n"
    );

  PDEBUG_PARSE(2, "composition_state: %s (%u).\n", string, composition_state);

  *ret = composition_state;
  free(string);
  return 0;
}

static int _parseCompositionDesc(
  IgsCompilerContext *ctx,
  HdmvCDParameters *param
)
{
  PDEBUG_PARSE(1, "composition_descriptor:\n");

  if (_parseCompositionState(ctx, &param->composition_state) < 0)
    return -1;

  /* Compute composition number */
  // TODO: Support multiple compositions
  PDEBUG_PARSE(2, "composition_number: %u.\n", param->composition_number);

  return 0;
}

static int _parseStreamModel(
  IgsCompilerContext *ctx,
  HdmvStreamModel *ret
)
{
  lbc *string = NULL;

  static const lbc *strings[] = {
    lbc_str("Multiplexed"),
    lbc_str("OoM")
  };

  GET_STRING(
    &string,
    strings[0], // Multiplexed by default
    return -1,
    "//parameters/model/@stream"
  );

  uint8_t stream_model = 0xFF;
  for (size_t i = 0; i < ARRAY_SIZE(strings); i++) {
    if (lbc_equal(strings[i], string)) {
      stream_model = i;
      break;
    }
  }

  if (0xFF == stream_model)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Unknown stream model 'stream' argument in composition parameters's "
      "model node in input XML file.\n"
    );

  PDEBUG_PARSE(2, "stream_model: %s (%u).\n", string, stream_model);
  *ret = stream_model;
  free(string);
  return 0;
}

static int _parseUserInterfaceModel(
  IgsCompilerContext *ctx,
  HdmvUserInterfaceModel *ret
)
{
  lbc *string = NULL;

  static const lbc *strings[] = {
    lbc_str("Normal"),
    lbc_str("Pop-Up")
  };

  GET_STRING(
    &string,
    strings[0], // Normal by default
    return -1,
    "//parameters/model/@user_interface"
  );

  uint8_t user_interface_model = 0xFF;
  for (size_t i = 0; i < ARRAY_SIZE(strings); i++) {
    if (lbc_equal(strings[i], string)) {
      user_interface_model = i;
      break;
    }
  }

  if (0xFF == user_interface_model)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Unknown user interface model 'user_interface' argument.\n"
    );

  PDEBUG_PARSE(2, "user_interface: %s (%u).\n", string, user_interface_model);
  *ret = user_interface_model;
  free(string);
  return 0;
}

static int _parseCompositionTimeOutPts(
  IgsCompilerContext *ctx,
  int64_t *ret
)
{
  uint64_t composition_time_out_pts;
  GET_UINT64(
    &composition_time_out_pts, 0ull,
    "Expect a positive or zero value for 'composition' option of section "
    "'time_out_pts'",
    return -1,
    "//parameters/time_out_pts/@composition"
  );

  if (composition_time_out_pts >> 33)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "'composition' option of section 'time_out_pts' value exceed maximum "
      "value 0x1FFFFFFFF or 8,589,934,591 ticks with %u.\n",
      composition_time_out_pts
    );

  PDEBUG_PARSE(2, "composition_time_out_pts: %" PRIu64 ".\n", composition_time_out_pts);
  *ret = (int64_t) composition_time_out_pts;
  return 0;
}

static int _parseSelectionTimeOutPts(
  IgsCompilerContext *ctx,
  int64_t *ret
)
{
  uint64_t selection_time_out_pts;
  GET_UINT64(
    &selection_time_out_pts, 0ull,
    "Expect a positive or zero value for 'selection' option of section "
    "'time_out_pts'",
    return -1,
    "//parameters/time_out_pts/@selection"
  );

  if (selection_time_out_pts >> 33)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "'selection' option of section 'time_out_pts' value exceed maximum "
      "value of 0x1FFFFFFFF or 8,589,934,591 ticks with %u.\n",
      selection_time_out_pts
    );

  PDEBUG_PARSE(2, "selection_time_out_pts: %" PRIu64 ".\n", selection_time_out_pts);
  *ret = (int64_t) selection_time_out_pts;
  return 0;
}

static int _parseUserTimeOutDuration(
  IgsCompilerContext *ctx,
  uint32_t *ret
)
{
  uint32_t user_time_out_duration;
  GET_UINT32(
    &user_time_out_duration, 0,
    "Expect a positive or zero value for 'user' option of section "
    "'time_out_pts'",
    return -1,
    "//parameters/time_out_pts/@user"
  );

  if (user_time_out_duration >> 24)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "'user' option of section 'time_out_pts' exceed maximum value of "
      "0xFFFFFF or 16,777,215 ticks (0x%" PRIX32 ").\n",
      user_time_out_duration
    );

  PDEBUG_PARSE(2, "user_time_out_duration: %" PRIu64 ".\n", user_time_out_duration);
  *ret = user_time_out_duration;
  return 0;
}

static int _parseCompositionParameters(
  IgsCompilerContext *ctx,
  HdmvICParameters *param
)
{
  param->interactive_composition_length = 0x0; /* Set default */

  PDEBUG_PARSE(1, "parameters:\n");

  if (_parseStreamModel(ctx, &param->stream_model) < 0)
    return -1;
  if (_parseUserInterfaceModel(ctx, &param->user_interface_model) < 0)
    return -1;

  if (param->stream_model == HDMV_STREAM_MODEL_MULTIPLEXED) {
    /* In Mux specific parameters */
    if (_parseCompositionTimeOutPts(ctx, &param->composition_time_out_pts) < 0)
      return -1;
    if (_parseSelectionTimeOutPts(ctx, &param->selection_time_out_pts) < 0)
      return -1;
  }

  if (_parseUserTimeOutDuration(ctx, &param->user_time_out_duration) < 0)
    return -1;

  return 0;
}

/** \~english
 * \brief Parse and return a picture from IGS XML description file.
 *
 * Use relative XPath format, root must be defined before call.
 */
static int _parseBitmapXmlCtx(
  HdmvBitmap *dst,
  IgsCompilerContext *ctx,
  lbc ** img_fp
)
{
  lbc *img_path = NULL;

  /* Parse all img fields: */
  /* img/@name : */
  GET_STRING(img_fp, NULL, goto free_return, "//@name");

  /* img/@path : */
  GET_STRING(&img_path, NULL, goto free_return, "//@path");
  if (NULL == img_path)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Missing required 'path' argument on 'img'.\n"
    );

  HdmvRectangle cutting_rect = emptyRectangle();

  /* img/@cutting_x : */
  GET_UINT32(
    &cutting_rect.x, 0,
    "Img's 'cutting_x' optionnal parameter must be positive",
    goto free_return,
    "//@cutting_x"
  );

  /* img/@cutting_y : */
  GET_UINT32(
    &cutting_rect.y, 0,
    "Img's 'cutting_y' optionnal parameter must be positive",
    goto free_return,
    "//@cutting_y"
  );

  /* img/@width : */
  GET_UINT32(
    &cutting_rect.w, 0,
    "Img's 'width' optionnal parameter must be positive",
    goto free_return,
    "//@width"
  );

  /* img/@height : */
  GET_UINT32(
    &cutting_rect.h, 0,
    "Img's 'height' optionnal parameter must be positive",
    goto free_return,
    "//@height"
  );

  if (
    (0 < cutting_rect.w && cutting_rect.w < 8)
    || (0 < cutting_rect.h && cutting_rect.h < 8)
  ) {
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Image object dimensions cannot be smaller than 8x8 pixels.\n"
    );
  }

  LIBBLU_HDMV_IGS_COMPL_XML_INFO(
    "Loading picture '%" PRI_LBCS "' (line %u).\n",
    img_path, lastParsedNodeLineXmlCtx(ctx->xml_ctx)
  );

  if (openHdmvBitmap(dst, &ctx->img_io_libs, img_path, ctx->conf_hdl) < 0)
    goto free_return;

  if (!isEmptyRectangle(cutting_rect)) {
    if (cropHdmvBitmap(dst, cutting_rect) < 0)
    goto free_return;
  }

  free(img_path);
  return 0;

free_return:
  free(img_path);
  return -1;
}

/** \~english
 * \brief Parse and return a reference picture from IGS XML description file.
 *
 * \param ctx Used context.
 *
 * Fetch a previously parsed reference picture (by #_parseReferencePictures()).
 * Use relative XPath format, root must be defined before call.
 */
static int _parseRefImg(
  HdmvBitmap *dst,
  IgsCompilerContext *ctx
)
{
  /* Parse all img fields: */
  /* ref_pic/@name : */
  lbc *name = getStringFromExprXmlCtx(ctx->xml_ctx, lbc_str("//@name"));
  if (NULL == name)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Missing required 'name' argument on 'img'.\n"
    );

  const HdmvBitmap *ref_bitmap_ptr = getRefPictureIgsCompilerComposition(
    CUR_COMP(ctx), name
  );
  if (NULL == ref_bitmap_ptr)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR(
      "No reference picture called '%" PRI_LBCS "' exits.\n",
      name
    );
  free(name);
  if (NULL == ref_bitmap_ptr)
    goto free_return;

  HdmvRectangle cutting_rect = emptyRectangle();

  /* ref_pic/@cutting_x : */
  GET_UINT32(
    &cutting_rect.x, 0,
    "Img's 'cutting_x' optionnal parameter must be positive",
    goto free_return,
    "//@cutting_x"
  );

  /* ref_pic/@cutting_y : */
  GET_UINT32(
    &cutting_rect.y, 0,
    "Img's 'cutting_y' optionnal parameter must be positive",
    goto free_return,
    "//@cutting_y"
  );

  /* ref_pic/@width : */
  GET_UINT32(
    &cutting_rect.w, 0,
    "Img's 'width' optionnal parameter must be positive",
    goto free_return,
    "//@width"
  );

  /* ref_pic/@height : */
  GET_UINT32(
    &cutting_rect.h, 0,
    "Img's 'height' optionnal parameter must be positive",
    goto free_return,
    "//@height"
  );

  HdmvBitmap bitmap;
  if (dupHdmvBitmap(&bitmap, ref_bitmap_ptr) < 0)
    goto free_return;

  if (!isEmptyRectangle(cutting_rect)) {
    if (cropHdmvBitmap(&bitmap, cutting_rect) < 0)
    goto free_return;
  }

  *dst = bitmap;
  return 0;

free_return:
  return -1;
}

static int _parseReferencePicture(
  IgsCompilerContext *ctx
)
{
  PDEBUG(1, "Parsing image...\n");
  HdmvBitmap bitmap;
  lbc *name;
  if (_parseBitmapXmlCtx(&bitmap, ctx, &name) < 0)
    return -1;

  if (NULL == name)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Missing reference image 'name' field.\n"
    );

  PDEBUG(1, "Adding image to references indexer...\n");
  if (addRefPictureIgsCompilerComposition(CUR_COMP(ctx), bitmap, name) < 0)
    goto free_return;

  free(name);
  return 0;

free_return:
  cleanHdmvBitmap(bitmap);
  free(name);
  return -1;
}

static int _parseReferencePictures(
  IgsCompilerContext *ctx
)
{
  /* Picture references : */
  XmlXPathObjectPtr refimg_path_obj = GET_OBJ("//reference_imgs/img");
  int nb_refimg = getNbNodesXmlXPathObject(refimg_path_obj);
  if (0 < nb_refimg)
    PDEBUG_PARSE(1, "reference_imgs:\n");

  for (int i = 0; i < nb_refimg; i++) {
    PDEBUG_PARSE(2, "reference_img[%d]:\n", i);

    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, refimg_path_obj, i) < 0)
      return -1;

    if (_parseReferencePicture(ctx) < 0)
      goto free_return;

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0)
      return -1;
  }

  destroyXmlXPathObject(refimg_path_obj);
  return 0;

free_return:
  destroyXmlXPathObject(refimg_path_obj);
  return -1;
}

static int _parseButtonStateImg(
  IgsCompilerContext *ctx,
  uint16_t *start_object_id_ref,
  uint16_t *end_object_id_ref
)
{
  XmlXPathObjectPtr img_path_obj = GET_OBJ("//graphic/*");

  unsigned nb_nodes  = getNbNodesXmlXPathObject(img_path_obj);
  unsigned nb_states = 0u;

  for (unsigned node_i = 0; node_i < nb_nodes; node_i++) {
    bool is_ref_pic = false;

    XmlNodePtr xml_node = getNodeXmlXPathObject(img_path_obj, node_i);
    if (NULL == xml_node)
      return -1;

    const lbc *node_name = getNameXmlNode(xml_node);
    if (NULL == node_name)
      return -1;

    if (lbc_equal(node_name, lbc_str("ref_pic")))
      is_ref_pic = false;
    if (!lbc_equal(node_name, lbc_str("img")))
      continue; // Unknown/unsupported node

    PDEBUG_PARSE(10, "object[%d] (%s):\n", node_i, is_ref_pic ? "ref" : "img");

    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, img_path_obj, node_i) < 0)
      return -1;

    int ret;
    HdmvBitmap bitmap;
    if (!is_ref_pic)
      ret = _parseBitmapXmlCtx(&bitmap, ctx, NULL);
    else
      ret = _parseRefImg(&bitmap, ctx);
    if (ret < 0)
      return ret;

    unsigned id;
    if (addObjectIgsCompilerComposition(CUR_COMP(ctx), bitmap, &id) < 0)
      return -1;
    if (UINT16_MAX <= id)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN("Too many objects.\n");

    if (0u == nb_states)
      *start_object_id_ref = id;
    *end_object_id_ref     = id;
    nb_states++;

    PDEBUG_PARSE(11, "object_id: 0x%04X.\n", id);

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0)
      return -1;
  }

  if (0u == nb_states) {
    PDEBUG_PARSE(10, "None.\n");
    *start_object_id_ref = 0xFFFF;
    *end_object_id_ref   = 0xFFFF;
  }

  destroyXmlXPathObject(img_path_obj);
  return 0;
}

static int _parseButton(
  IgsCompilerContext *ctx,
  HdmvButtonParam *btn,
  uint16_t *next_button_id_ref,
  bool used_button_id_ref[static HDMV_MAX_NB_BUTTONS]
)
{
  GET_UINT16(
    &btn->button_id,
    *next_button_id_ref,
    "Button's 'id' optionnal parameter must be between "
    "0x0000 and 0x1FDF inclusive",
    return -1,
    "//@id"
  );
  PDEBUG_PARSE(
    8, "button_id: 0x%" PRIX16 ".\n",
    btn->button_id
  );

  if (HDMV_MAX_NB_BUTTONS <= btn->button_id)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR(
      "Unexpected 'button_id' value, values shall be between 0x0000 "
      "and 0x%04X inclusive (got 0x%04" PRIX16 ").\n",
      HDMV_MAX_NB_BUTTONS-1,
      btn->button_id
    );

  if (used_button_id_ref[btn->button_id])
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR(
      "Non-unique button ID 0x%04" PRIX16 ", values must be unique on the page.",
      btn->button_id
    );
  used_button_id_ref[btn->button_id] = true;
  *next_button_id_ref = btn->button_id + 1;

  GET_UINT16(
    &btn->button_numeric_select_value,
    0xFFFF,
    "Button's 'button_numeric_select_value' optionnal parameter must be "
    "between 0x0000 and 0xFFFF inclusive",
    return -1,
    "//button_numeric_select_value"
  );
  PDEBUG_PARSE(
    8, "button_numeric_select_value: 0x%" PRIX16 ".\n",
    btn->button_numeric_select_value
  );



  GET_BOOL(&btn->auto_action, false, return -1, "//auto_action_flag");
  PDEBUG_PARSE(
    8, "auto_action_flag: %s (0b%x).\n",
    BOOL_STR(btn->auto_action),
    btn->auto_action
  );

  GET_UINT16(
    &btn->button_horizontal_position,
    -1,
    "Missing or invalid button's 'horizontal_position' parameter, "
    "must be between 0x0000 and 0xFFFF inclusive",
    return -1,
    "//horizontal_position"
  );
  PDEBUG_PARSE(
    8, "horizontal_position: %" PRIu16 " px.\n",
    btn->button_horizontal_position
  );

  GET_UINT16(
    &btn->button_vertical_position,
    -1,
    "Missing or invalid button's 'vertical_position' parameter, "
    "must be between 0x0000 and 0xFFFF inclusive",
    return -1,
    "//vertical_position"
  );
  PDEBUG_PARSE(
    8, "vertical_position: %" PRIu16 " px.\n",
    btn->button_vertical_position
  );

  PDEBUG_PARSE(8, "neighbor_info:\n");
  if (0 < NB_OBJ("//neighbor_info")) {
    GET_UINT16(
      &btn->neighbor_info.upper_button_id_ref,
      btn->button_id,
      "Missing or invalid button's neighbor 'upper_button' parameter, "
      "must be between 0x0000 and 0xFFFF inclusive (line %u).\n",
      return -1,
      "//neighbor_info/upper_button"
    );

    GET_UINT16(
      &btn->neighbor_info.lower_button_id_ref,
      btn->button_id,
      "Missing or invalid button's neighbor 'lower_button' parameter, "
      "must be between 0x0000 and 0xFFFF inclusive (line %u).\n",
      return -1,
      "//neighbor_info/lower_button"
    );

    GET_UINT16(
      &btn->neighbor_info.left_button_id_ref,
      btn->button_id,
      "Missing or invalid button's neighbor 'left_button' parameter, "
      "must be between 0x0000 and 0xFFFF inclusive (line %u).\n",
      return -1,
      "//neighbor_info/left_button"
    );

    GET_UINT16(
      &btn->neighbor_info.right_button_id_ref,
      btn->button_id,
      "Missing or invalid button's neighbor 'right_button' parameter, "
      "must be between 0x0000 and 0xFFFF inclusive (line %u).\n",
      return -1,
      "//neighbor_info/right_button"
    );
  }
  else
    btn->neighbor_info.upper_button_id_ref = btn->button_id,
    btn->neighbor_info.lower_button_id_ref = btn->button_id,
    btn->neighbor_info.left_button_id_ref  = btn->button_id,
    btn->neighbor_info.right_button_id_ref = btn->button_id
  ;

  PDEBUG_PARSE(
    9, "upper_button: 0x%" PRIX16 ".\n",
    btn->neighbor_info.upper_button_id_ref
  );
  PDEBUG_PARSE(
    9, "lower_button: 0x%" PRIX16 ".\n",
    btn->neighbor_info.lower_button_id_ref
  );
  PDEBUG_PARSE(
    9, "left_button:  0x%" PRIX16 ".\n",
    btn->neighbor_info.left_button_id_ref
  );
  PDEBUG_PARSE(
    9, "right_button: 0x%" PRIX16 ".\n",
    btn->neighbor_info.right_button_id_ref
  );

  /* Normal state */
  PDEBUG_PARSE(8, "normal_state_info:\n");
  XmlXPathObjectPtr nsi_path_obj = GET_OBJ("//normal_state_info");
  HdmvButtonNormalStateInfoParam *nsi = &btn->normal_state_info;

  if (0 < getNbNodesXmlXPathObject(nsi_path_obj)) {
    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, nsi_path_obj, 0) < 0) {
      destroyXmlXPathObject(nsi_path_obj);
      return -1;
    }

    PDEBUG_PARSE(9, "objects:\n");
    uint16_t *start_obj_id_ref = &nsi->start_object_id_ref;
    uint16_t *end_obj_id_ref   = &nsi->end_object_id_ref;
    if (_parseButtonStateImg(ctx, start_obj_id_ref, end_obj_id_ref) < 0) {
      destroyXmlXPathObject(nsi_path_obj);
      return -1;
    }

    GET_BOOL(&nsi->repeat_flag, false, return -1, "//repeat_flag");
    GET_BOOL(&nsi->complete_flag, false, return -1, "//complete_flag");

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0) {
      destroyXmlXPathObject(nsi_path_obj);
      return -1;
    }
  }
  else {
    nsi->start_object_id_ref = 0xFFFF;
    nsi->end_object_id_ref   = 0xFFFF;
    nsi->repeat_flag         = false;
    nsi->complete_flag       = false;
  }

  destroyXmlXPathObject(nsi_path_obj);

  PDEBUG_PARSE(9, "start_object_id_ref: 0x%04" PRIX16 ".\n", nsi->start_object_id_ref);
  PDEBUG_PARSE(9, "end_object_id_ref:   0x%04" PRIX16 ".\n", nsi->end_object_id_ref);
  PDEBUG_PARSE(9, "repeat_flag:         %s.\n", BOOL_STR(nsi->repeat_flag));
  PDEBUG_PARSE(9, "complete_flag:       %s.\n", BOOL_STR(nsi->complete_flag));

  /* Selected state */
  PDEBUG_PARSE(8, "selected_state_info:\n");
  XmlXPathObjectPtr ssi_path_obj = GET_OBJ("//selected_state_info");
  HdmvButtonSelectedStateInfoParam *ssi = &btn->selected_state_info;

  if (0 < getNbNodesXmlXPathObject(ssi_path_obj)) {
    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, ssi_path_obj, 0) < 0) {
      destroyXmlXPathObject(ssi_path_obj);
      return -1;
    }

    GET_UINT8(
      &ssi->state_sound_id_ref, 0xFF,
      "Invalid button's selected state 'sound_id' parameter, "
      "must be between 0x00 and 0xFF inclusive",
      return -1,
      "//sound_id"
    );

    PDEBUG_PARSE(9, "objects:\n");
    uint16_t *start_obj_id_ref = &ssi->start_object_id_ref;
    uint16_t *end_obj_id_ref   = &ssi->end_object_id_ref;
    if (_parseButtonStateImg(ctx, start_obj_id_ref, end_obj_id_ref) < 0) {
      destroyXmlXPathObject(ssi_path_obj);
      return -1;
    }

    GET_BOOL(&ssi->repeat_flag, false, return -1, "//repeat_flag");
    GET_BOOL(&ssi->complete_flag, false, return -1, "//complete_flag");

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0) {
      destroyXmlXPathObject(ssi_path_obj);
      return -1;
    }
  }

  destroyXmlXPathObject(ssi_path_obj);

  PDEBUG_PARSE(9, "state_sound_id_ref:  0x%02" PRIX8 ".\n", ssi->state_sound_id_ref);
  PDEBUG_PARSE(9, "start_object_id_ref: 0x%04" PRIX16 ".\n", ssi->start_object_id_ref);
  PDEBUG_PARSE(9, "end_object_id_ref:   0x%04" PRIX16 ".\n", ssi->end_object_id_ref);
  PDEBUG_PARSE(9, "repeat_flag:         %s.\n", BOOL_STR(ssi->repeat_flag));
  PDEBUG_PARSE(9, "complete_flag:       %s.\n", BOOL_STR(ssi->complete_flag));

  /* Activated state */
  PDEBUG_PARSE(8, "activated_state_info:\n");
  XmlXPathObjectPtr asi_path_obj = GET_OBJ("//activated_state_info");
  HdmvButtonActivatedStateInfoParam *asi = &btn->activated_state_info;

  if (0 < getNbNodesXmlXPathObject(asi_path_obj)) {
    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, asi_path_obj, 0) < 0) {
      destroyXmlXPathObject(asi_path_obj);
      return -1;
    }

    GET_UINT8(
      &asi->state_sound_id_ref, 0xFF,
      "Invalid button's activated state 'sound_id' parameter, "
      "must be between 0x00 and 0xFF inclusive",
      return -1,
      "//sound_id"
    );

    PDEBUG_PARSE(9, "objects:\n");
    uint16_t *start_obj_id_ref = &asi->start_object_id_ref;
    uint16_t *end_obj_id_ref   = &asi->end_object_id_ref;
    if (_parseButtonStateImg(ctx, start_obj_id_ref, end_obj_id_ref) < 0) {
      destroyXmlXPathObject(asi_path_obj);
      return -1;
    }

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0) {
      destroyXmlXPathObject(asi_path_obj);
      return -1;
    }
  }

  destroyXmlXPathObject(asi_path_obj);

  PDEBUG_PARSE(9, "state_sound_id_ref:  0x%02" PRIX8 ".\n", asi->state_sound_id_ref);
  PDEBUG_PARSE(9, "start_object_id_ref: 0x%04" PRIX16 ".\n", asi->start_object_id_ref);
  PDEBUG_PARSE(9, "end_object_id_ref:   0x%04" PRIX16 ".\n", asi->end_object_id_ref);

  /* Commands */
  PDEBUG_PARSE(8, "navigation_commands:\n");
  XmlXPathObjectPtr com_path_obj = GET_OBJ("//commands/command");
  int nb_nav_com = getNbNodesXmlXPathObject(com_path_obj);

  if (HDMV_MAX_NB_BUTTON_NAV_COM < nb_nav_com)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Invalid number of navigation commands in button, "
      "%d exceeds maximum of " STR(HDMV_MAX_NB_BUTTON_NAV_COM) ".\n",
      nb_nav_com
    );
  btn->number_of_navigation_commands = nb_nav_com;

  if (!nb_nav_com)
    PDEBUG_PARSE(9, "*no command*\n");

  if (allocateCommandsHdmvButtonParam(btn) < 0)
    return -1;

  for (int i = 0; i < nb_nav_com; i++) {
    HdmvNavigationCommand *com = &btn->navigation_commands[i];
    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, com_path_obj, i) < 0) {
      destroyXmlXPathObject(com_path_obj);
      return -1;
    }

    /* command/@code */
    GET_UINT32(
      &com->opcode, -1,
      "Missing or invalid navigation command 'code' field, "
      "must be between 0x00000000 and 0xFFFFFFFF inclusive",
      return -1,
      "//@code"
    );

    /* command/@destination */
    GET_UINT32(
      &com->destination, -1,
      "Missing or invalid navigation command 'destination' field, "
      "must be between 0x00000000 and 0xFFFFFFFF inclusive",
      return -1,
      "//@destination"
    );

    /* command/@source */
    GET_UINT32(
      &com->source, -1,
      "Missing or invalid navigation command 'source' field, "
      "must be between 0x00000000 and 0xFFFFFFFF inclusive",
      return -1,
      "//@source"
    );

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0) {
      destroyXmlXPathObject(com_path_obj);
      return -1;
    }
  }

  destroyXmlXPathObject(com_path_obj);
  return 0;
}

static int _parseBog(
  IgsCompilerContext *ctx,
  HdmvButtonOverlapGroupParameters *bog,
  uint16_t *next_button_id_ref,
  bool used_btn_id_ref[static HDMV_MAX_NB_BUTTONS],
  uint8_t used_btn_num_sel_val_ref[static HDMV_MAX_BUTTON_NUMERIC_SELECT_VALUE],
  uint8_t bog_idx
)
{
  /* bog/default_valid_button */
  GET_UINT16(
    &bog->default_valid_button_id_ref, 0xFFFF,
    "BOG's 'default_valid_button' optionnal parameter must be between "
    "0x0000 and 0xFFFF inclusive",
    return -1,
    "//default_valid_button"
  );

  PDEBUG_PARSE(
    6, "default_valid_button: 0x%" PRIX16 ".\n",
    bog->default_valid_button_id_ref
  );

  bool def_val_btn_id_ref_pres = (0xFFFF == bog->default_valid_button_id_ref);
  bool auto_def_val_btn_id     = !EXISTS("//default_valid_button");

  XmlXPathObjectPtr btn_path_obj = GET_OBJ("//button");
  {
    int nb_btn = getNbNodesXmlXPathObject(btn_path_obj);
    if (HDMV_MAX_NB_ICS_BUTTONS < nb_btn)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
        "Number of buttons is invalid (%d, shall be between 0-255).\n",
        nb_btn
      );
    bog->number_of_buttons = nb_btn;
  }

  PDEBUG_PARSE(6, "buttons:\n");

  if (!bog->number_of_buttons) {
    PDEBUG_PARSE(7, "None.\n");
    LIBBLU_HDMV_IGS_COMPL_XML_WARNING(
      "BOG at line %u is empty.\n",
      lastParsedNodeLineXmlCtx(ctx->xml_ctx)
    );
  }

  for (int i = 0; i < bog->number_of_buttons; i++) {
    HdmvButtonParam *btn = &bog->buttons[i];
    /* Set path for relative path accessing: */
    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, btn_path_obj, i) < 0)
      return -1;

    PDEBUG_PARSE(7, "button[%d]:\n", i);

    /* bog/button[i] */
    if (_parseButton(ctx, btn, next_button_id_ref, used_btn_id_ref) < 0)
      goto free_return;

    uint16_t btn_num_sel_val = btn->button_numeric_select_value;
    if (0xFFFF != btn_num_sel_val) {
      uint16_t bog_using_num_sel_val = used_btn_num_sel_val_ref[btn_num_sel_val];
      if (0xFFFF != bog_using_num_sel_val && bog_using_num_sel_val != bog_idx)
        LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
          "Button numeric selection value must be unique to those of other "
          "buttons on the page that are not part of the same BOG "
          "(problematic value: %u, found in BOG %u and already used in BOG %u).\n",
          btn_num_sel_val,
          bog_idx,
          used_btn_num_sel_val_ref[btn_num_sel_val]
        );
      used_btn_num_sel_val_ref[btn_num_sel_val] = bog_idx;
    }

    if (btn->button_id == bog->default_valid_button_id_ref)
      def_val_btn_id_ref_pres = true;

    /* Restore original path */
    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0)
      goto free_return;
  }

  if (!def_val_btn_id_ref_pres)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "BOG 'default_valid_button_id' does not match with any button.\n"
    );

  if (auto_def_val_btn_id && 0 < bog->number_of_buttons)
    bog->default_valid_button_id_ref = bog->buttons[0].button_id;

  destroyXmlXPathObject(btn_path_obj);
  return 0;

free_return:
  destroyXmlXPathObject(btn_path_obj);
  return -1;
}

static int _parsePage(
  IgsCompilerContext *ctx,
  HdmvPageParameters *page,
  uint8_t page_idx
)
{
  page->page_id = page_idx;
  page->page_version_number = 0x00;

  PDEBUG_PARSE(3, "page_id (infered): %" PRIu8 ".\n", page->page_id);
  PDEBUG_PARSE(3, "page_version_number (infered): %" PRIu8 ".\n", page->page_version_number);

  /* page/parameters/uop */
  uint64_t uop_mask = ctx->data.common_uop_mask; // Applying global flags
  XmlXPathObjectPtr uop_path_obj = GET_OBJ("//parameters/uop");

  if (existsNodeXmlXPathObject(uop_path_obj, 0)) {
    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, uop_path_obj, 0) < 0) {
      destroyXmlXPathObject(uop_path_obj);
      return -1;
    }

    if (_parseParametersUop(ctx, &uop_mask) < 0) {
      destroyXmlXPathObject(uop_path_obj);
      return -1;
    }

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0) {
      destroyXmlXPathObject(uop_path_obj);
      return -1;
    }
  }

  destroyXmlXPathObject(uop_path_obj);

  page->UO_mask_table = uop_mask;
  PDEBUG_PARSE(3, "uop: 0x%016" PRIX64 ".\n", page->UO_mask_table);

  PDEBUG_PARSE(3, "parameters:\n");

  /* page/parameters/animation_frame_rate_code */
  GET_UINT8(
    &page->animation_frame_rate_code, 0x00,
    "Page's parameter 'animation_frame_rate_code' optionnal parameter must "
    "be between 0 and 255 inclusive",
    return -1,
    "//parameters/animation_frame_rate_code"
  );

  PDEBUG_PARSE(
    4, "animation_frame_rate_code: %" PRIu8 ".\n",
    page->animation_frame_rate_code
  );

  /* page/parameters/default_selected_button */
  GET_UINT16(
    &page->default_selected_button_id_ref, 0xFFFF,
    "Page's parameter 'default_selected_button' optionnal parameter must be "
    "between 0x0000 and 0xFFFF inclusive",
    return -1,
    "//parameters/default_selected_button"
  );

  PDEBUG_PARSE(
    4, "default_selected_button: 0x%04" PRIX16 ".\n",
    page->default_selected_button_id_ref
  );

  bool auto_def_sel_btn_id = !EXISTS("//parameters/default_selected_button");
  bool def_sel_btn_id_pres = (0xFFFF == page->default_selected_button_id_ref);

  /* page/parameters/default_activated_button */
  GET_UINT16(
    &page->default_activated_button_id_ref, 0xFFFF,
    "Page's parameter 'default_selected_button' optionnal parameter must be "
    "between 0x0000 and 0xFFFF inclusive",
    return -1,
    "//parameters/default_activated_button"
  );

  PDEBUG_PARSE(
    4, "default_activated_button: 0x%04" PRIX16 ".\n",
    page->default_activated_button_id_ref
  );

  bool def_act_btn_id_pres = (0xFFFE <= page->default_activated_button_id_ref);

  /* page/in_effects */
  PDEBUG_PARSE(3, "in_effects:\n");
  memset(&page->in_effects, 0x0, sizeof(HdmvEffectSequenceParameters));
  PDEBUG_PARSE(4, "TODO\n"); // TODO

  /* page/out_effects */
  PDEBUG_PARSE(3, "out_effects:\n");
  memset(&page->out_effects, 0x0, sizeof(HdmvEffectSequenceParameters));
  PDEBUG_PARSE(4, "TODO\n"); // TODO

  PDEBUG_PARSE(3, "BOGs (Button Overlap Groups):\n");

  /* page/bog */
  XmlXPathObjectPtr bog_path_obj = GET_OBJ("//bog");
  int nb_bog = getNbNodesXmlXPathObject(bog_path_obj);

  if (HDMV_MAX_NB_ICS_BOGS < nb_bog)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Number of BOGs described is invalid (shall be between 1-255).\n",
      nb_bog
    );
  page->number_of_BOGs = nb_bog;

  if (!nb_bog) {
    PDEBUG_PARSE(4, "None.\n");
    LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN(
      "Empty page, expect at least one BOG.\n"
    );
  }

  if (allocateBogsHdmvPageParameters(page) < 0)
    return -1;

  bool used_btn_id[HDMV_MAX_NB_BUTTONS];
    MEMSET_ARRAY(used_btn_id, false);
  uint8_t used_btn_num_sel_val[HDMV_MAX_BUTTON_NUMERIC_SELECT_VALUE];
    MEMSET_ARRAY(used_btn_num_sel_val, 0xFF);
  uint16_t button_id = 0x0000;

  for (int bog_i = 0; bog_i < nb_bog; bog_i++) {
    HdmvButtonOverlapGroupParameters *bog = &page->bogs[bog_i];

    /* Set path for relative path accessing: */
    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, bog_path_obj, bog_i) < 0)
      goto free_return;

    PDEBUG_PARSE(4, "bog[%d]:\n", bog_i);

    /* page/bog[i] */
    if (_parseBog(ctx, bog, &button_id, used_btn_id, used_btn_num_sel_val, (uint8_t) bog_i) < 0)
      goto free_return;

    if (bog->default_valid_button_id_ref == page->default_selected_button_id_ref)
      def_sel_btn_id_pres = true;
    for (uint8_t j = 0; j < bog->number_of_buttons; j++) {
      if (bog->buttons[j].button_id == page->default_activated_button_id_ref)
        def_act_btn_id_pres = true;
    }

    /* Restore original path */
    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0)
      goto free_return;
  }

  if (!def_sel_btn_id_pres)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Default selected button id does not match any "
      "page's BOG's default valid button id.\n"
    );

  if (!def_act_btn_id_pres)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Default activated button id does not match any "
      "page's button id.\n"
    );

  if (auto_def_sel_btn_id && 0 < page->number_of_BOGs)
    page->default_selected_button_id_ref = page->bogs[0].buttons[0].button_id;
  page->palette_id_ref = 0x00; // Apply default palette ID

  destroyXmlXPathObject(bog_path_obj);
  return 0;

free_return:
  destroyXmlXPathObject(bog_path_obj);
  return -1;
}

static int _parsePages(
  IgsCompilerContext *ctx,
  HdmvICParameters *compo
)
{
  XmlXPathObjectPtr page_path_obj = GET_OBJ("//page");
  int nb_pages = getNbNodesXmlXPathObject(page_path_obj);
  if (HDMV_MAX_NB_IC_PAGES < nb_pages)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Number of IGS pages described in input XML file is too large "
      "(shall be up to " STR(HDMV_MAX_NB_IC_PAGES) " inclusive, found %d pages).\n",
      nb_pages
    );
  compo->number_of_pages = nb_pages;

  PDEBUG_PARSE(1, "pages:\n");

  if (allocatePagesHdmvICParameters(compo) < 0)
    return -1;

  for (int page_i = 0; page_i < nb_pages; page_i++) {
    PDEBUG_PARSE(2, "page[%d]:\n", page_i);

    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, page_path_obj, page_i) < 0)
      goto free_return;

    if (_parsePage(ctx, &compo->pages[page_i], (uint8_t) page_i) < 0)
      LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN("Error happen in page %d.\n", page_i);

    /* Restore original path */
    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0)
      goto free_return;
  }

  destroyXmlXPathObject(page_path_obj);
  return 0;

free_return:
  destroyXmlXPathObject(page_path_obj);
  return -1;
}

static int _parseComposition(
  IgsCompilerContext *ctx,
  IgsCompilerComposition *compo
)
{
  /* Presentation timecode */
  if (addHdmvTimecodes(ctx->timecodes, 0) < 0)
    return -1;
  /* TODO: Enable custom */

  /* Video descriptor from common */
  if (_parseVideoDescriptor(ctx, &compo->video_descriptor) < 0)
    return -1;

  /* Composition Descriptor */
  if (_parseCompositionDesc(ctx, &compo->composition_descriptor) < 0)
    return -1;

  if (_parseCompositionParameters(ctx, &compo->interactive_composition) < 0)
    return -1;

  if (_parseReferencePictures(ctx) < 0)
    return -1;

  if (_parsePages(ctx, &compo->interactive_composition) < 0)
    return -1;
  return 0;
}

static int _parseCompositions(
  IgsCompilerContext *ctx
)
{
  XmlXPathObjectPtr compo_path_obj = GET_OBJ("/igs/composition");
  int nb_compo = getNbNodesXmlXPathObject(compo_path_obj);

  if (!nb_compo)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Expect at least one 'composition' node.\n"
    );
  if (HDMV_MAX_NB_ICS_COMPOS < nb_compo)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "The number of 'composition' nodes exceed maximum supported (one).\n"
    );

  for (int i = 0; i < nb_compo; i++) {
    PDEBUG_PARSE(0, "composition[%d]:\n", i);

    if (setRootPathFromPathObjectXmlCtx(ctx->xml_ctx, compo_path_obj, i) < 0)
      goto free_return;

    IgsCompilerComposition *compo = &ctx->data.compositions[ctx->data.nb_compositions++];
    memset(compo, 0, sizeof(IgsCompilerComposition));

    if (_parseComposition(ctx, compo) < 0)
      goto free_return;

    if (restoreLastRootXmlCtx(ctx->xml_ctx) < 0)
      goto free_return;
  }

  destroyXmlXPathObject(compo_path_obj);
  return 0;

free_return:
  destroyXmlXPathObject(compo_path_obj);
  return -1;
}

static int _parseFile(
  IgsCompilerContext *ctx
)
{
  if (_parseCommonSettings(ctx) < 0)
    return -1;
  if (_parseCompositions(ctx) < 0)
    return -1;
  return 0;
}

int parseIgsXmlFile(
  IgsCompilerContext *ctx
)
{
  PDEBUG(0, "Loading input XML file.\n");
  if (loadXmlCtx(ctx->xml_ctx, ctx->xml_filename) < 0)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN("Unable to load IGS XML file.\n");

  if (!EXISTS("/igs"))
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(
      "Input file is not a proper IGS Compiler XML file "
      "(missing 'igs' root node).\n"
    );

  PDEBUG(0, "Parsing input XML file.\n");
  if (_parseFile(ctx) < 0)
    LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(
      "Error occured line %u.\n",
      lastParsedNodeLineXmlCtx(ctx->xml_ctx)
    );

  PDEBUG(0, "Releasing input XML file.\n");
  releaseXmlCtx(ctx->xml_ctx);
  return 0;

free_return:
  releaseXmlCtx(ctx->xml_ctx);
  return -1;
}
