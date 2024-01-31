#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "igs_compiler.h"

#define ECHO_DEBUG  LIBBLU_HDMV_IGS_COMPL_DEBUG

static int _getFileWorkingDirectory(
  lbc ** dirPath,
  lbc ** xml_filename,
  const lbc * xmlFilepath
)
{
  enum lbc_cwk_path_style savedStyle;
  const lbc * pathFilenamePart;
  size_t pathLen;

  /* Set path style to the guessed one */
  savedStyle = lbc_cwk_path_get_style();
  lbc_cwk_path_set_style(lbc_cwk_path_guess_style(xmlFilepath));

  /* Fetch directory: */
  lbc_cwk_path_get_dirname(xmlFilepath, &pathLen);
  if (!pathLen)
    LIBBLU_HDMV_IGS_COMPL_ERROR_FRETURN(
      "Unable to fetch file directory path needed "
      "for relative path access.\n"
    );

  if (NULL == (*dirPath = lbc_strndup(xmlFilepath, pathLen)))
    LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN("Memory allocation error.\n");

  /* Fetch filename: */
  lbc_cwk_path_get_basename(xmlFilepath, &pathFilenamePart, &pathLen);
  if (!pathLen)
    LIBBLU_HDMV_IGS_COMPL_ERROR_FRETURN(
      "Unable to fetch filename needed "
      "for relative path access.\n"
    );

  if (NULL == (*xml_filename = lbc_strdup(pathFilenamePart)))
    LIBBLU_HDMV_IGS_COMPL_ERROR_FRETURN("Memory allocation error.\n");

  /* Restore saved path style */
  lbc_cwk_path_set_style(savedStyle);
  return 0;

free_return:
  lbc_cwk_path_set_style(savedStyle);
  return -1;
}

static int _updateIgsCompilerWorkingDirectory(
  IgsCompilerContext * ctx,
  const lbc * xml_filename
)
{
  /* Save current working directory */
  if (lbc_getcwd(ctx->initial_working_dir, PATH_BUFSIZE) < 0)
    return -1;
  ECHO_DEBUG("  Saved path: '%" PRI_LBCS "'.\n", ctx->initial_working_dir);

  /* Getting working directory based on input XML filename */
  if (_getFileWorkingDirectory(&ctx->cur_working_dir, &ctx->xml_filename, xml_filename) < 0)
    return -1;
  ECHO_DEBUG("  New working path: '%" PRI_LBCS "'.\n", ctx->cur_working_dir);
  ECHO_DEBUG("  XML filename: '%" PRI_LBCS "'.\n", ctx->xml_filename);
  /* BUG: ctx->xml_filename as 'invalid' read errors, make a proper copy */

  /* Redefinition of working path */
  if (lbc_chdir(ctx->cur_working_dir) < 0)
    LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN(
      "Unable to redefine working directory to opened file location, "
      "%s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return 0;
}

static void _resetOriginalDirIgsCompilerContext(
  IgsCompilerContext * ctx
)
{
  if (NULL != ctx->cur_working_dir) {
    if (lbc_chdir(ctx->initial_working_dir) < 0)
      LIBBLU_WARNING("Unable to retrieve original working path.\n");
    else
      ECHO_DEBUG(
        " Original working path restored ('%" PRI_LBCS "').\n",
        ctx->initial_working_dir
      );

    free(ctx->cur_working_dir);
    ctx->cur_working_dir = NULL;
  }
}

static void _cleanIgsCompilerContext(
  IgsCompilerContext ctx
)
{
  ECHO_DEBUG("Free IGS Compiler context memory.\n");

  cleanHdmvPictureLibraries(ctx.img_io_libs);
  cleanXmlContext(ctx.xml_ctx);

  for (unsigned i = 0; i < ctx.data.nb_compositions; i++)
    cleanIgsCompilerComposition(ctx.data.compositions[i]);

  _resetOriginalDirIgsCompilerContext(&ctx);
  free(ctx.xml_filename);

  ECHO_DEBUG("Done.\n");
}

static int _initIgsCompilerContext(
  IgsCompilerContext * dst,
  const lbc * xml_filename,
  HdmvTimecodes * timecodes,
  IniFileContext conf_hdl
)
{
  ECHO_DEBUG("IGS Compiler context initialization.\n");
  IgsCompilerContext ctx = {
    .conf_hdl  = conf_hdl,
    .timecodes = timecodes
  };

  ECHO_DEBUG(" Initialization of libraries and ressources.\n");

  /* Init Picture libraries */
  initHdmvPictureLibraries(&ctx.img_io_libs);

  /* Set working directory to allow file-relative files handling: */
  ECHO_DEBUG(" Redefinition of working directory.\n");
  if (_updateIgsCompilerWorkingDirectory(&ctx, xml_filename) < 0)
    goto free_return;

  ECHO_DEBUG("Done.\n");
  *dst = ctx;
  return 0;

free_return:
  ECHO_DEBUG("Failed.\n");
  _cleanIgsCompilerContext(ctx);
  return -1;
}

static int _buildPalette(
  HdmvPalette * dst,
  HdmvBitmapList * list,
  HdmvPaletteColorMatrix color_matrix
)
{
  /* Create a new palette */
  HdmvPalette pal;
  initHdmvPalette(&pal, 0x0, color_matrix);

  /* Fill the palette according to pictures */
  if (buildPaletteListHdmvPalette(&pal, list, 255u, NULL) < 0)
    return -1;

  /* Sort palette */
  sortEntriesHdmvPalette(&pal);

  *dst = pal;
  return 0;
}

static int _buildObjects(
  IgsCompilerComposition * compo,
  const HdmvBitmapList * list,
  const HdmvPalette * palette,
  HdmvColorDitheringMethod dither_method
)
{
  HdmvObject * objects_arr = malloc(list->nb_bitmaps * sizeof(HdmvObject));
  if (NULL == objects_arr)
    LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN("Memory allocation error.\n");

  unsigned nb_objects = 0u;
  for (unsigned i = 0; i < list->nb_bitmaps; i++) {
    HdmvBitmap bitmap = list->bitmaps[i];

    HdmvPalletizedBitmap pal_bm;
    if (getPalletizedHdmvBitmap(&pal_bm, bitmap, palette, dither_method) < 0)
      goto free_return;

    HdmvObject * obj = &objects_arr[nb_objects++];
    if (initHdmvObject(obj, pal_bm) < 0) {
      cleanHdmvPaletizedBitmap(pal_bm);
      goto free_return;
    }

    obj->desc.object_id = bitmap.object_id; // Set object_id as requested

    uint16_t line;
    if (!performRleHdmvObject(obj, &line)) {
      // TODO: Provide other solutions
      cleanHdmvPaletizedBitmap(pal_bm);
      LIBBLU_HDMV_IGS_COMPL_ERROR_FRETURN(
        "Unable to generate RLE for object %u, "
        "compressed line %u exceed %u + 2 bytes.\n",
        i, line, obj->pal_bitmap.width
      );
    }
  }

  compo->objects    = objects_arr;
  compo->nb_objects = nb_objects;
  return 0;

free_return:
  while (nb_objects--)
    cleanHdmvObject(objects_arr[nb_objects]);
  free(objects_arr);
  return -1;
}

static int _collectObjectRange(
  HdmvBitmapList * list,
  HdmvBitmap * bm_array,
  uint16_t start_object_id_ref,
  uint16_t end_object_id_ref,
  HdmvODSUsage usage
)
{
  if (0xFFFF == start_object_id_ref) {
    LIBBLU_HDMV_IGS_COMPL_DEBUG("     No object\n");
    return 0;
  }

  uint16_t start, end;
  if (start_object_id_ref <= end_object_id_ref)
    start = start_object_id_ref, end = end_object_id_ref;
  else
    start = end_object_id_ref, end = start_object_id_ref;

  for (uint16_t object_id_ref = start; object_id_ref <= end; object_id_ref++) {
    LIBBLU_HDMV_IGS_COMPL_DEBUG(
      "      Object with 'object_id' == 0x%04" PRIX16 "\n",
      object_id_ref
    );

    HdmvBitmap bitmap = bm_array[object_id_ref];

    bitmap.usage     = MAX(bitmap.usage, usage); // Set usage of the ODS
    bitmap.object_id = object_id_ref; // Set object_id

    if (addHdmvBitmapList(list, bitmap) < 0)
      return -1;
  }

  return 0;
}

int buildIgsCompilerComposition(
  IgsCompilerComposition * compo,
  HdmvColorDitheringMethod dither_method,
  HdmvPaletteColorMatrix color_matrix
)
{
  assert(NULL != compo);

  LIBBLU_HDMV_IGS_COMPL_INFO("Building interactive compositions...\n");

  HdmvBitmapList obj_bms = {0};

  LIBBLU_HDMV_IGS_COMPL_DEBUG("Preparing assets of each page:\n");

  /* Collect all composition pictures */
  uint8_t nb_pages = compo->interactive_composition.number_of_pages;
  for (uint8_t page_id = 0; page_id < nb_pages; page_id++) {
    /* Create a palette for each page */
    HdmvPageParameters * page = &compo->interactive_composition.pages[page_id];

    LIBBLU_HDMV_IGS_COMPL_DEBUG(" Page %" PRIu8 "\n", page_id);

    /* Collect objects */
    for (uint8_t bog_id = 0; bog_id < page->number_of_BOGs; bog_id++) {
      const HdmvButtonOverlapGroupParameters bog = page->bogs[bog_id];

      LIBBLU_HDMV_IGS_COMPL_DEBUG("  Bog %" PRIu8 "\n", bog_id);

      for (uint8_t btn_id = 0; btn_id < bog.number_of_buttons; btn_id++) {
        HdmvButtonParam btn = bog.buttons[btn_id];
        bool is_def_sel_btn = (btn.button_id == page->default_selected_button_id_ref);
        bool is_def_val_btn = (btn.button_id == bog.default_valid_button_id_ref);

        LIBBLU_HDMV_IGS_COMPL_DEBUG("   Button %" PRIu8 "\n", btn_id);

        uint16_t start_obj_id_ref, end_obj_id_ref;
        HdmvODSUsage usage = ODS_UNUSED_PAGE0;

        /* Collect ODS used for Normal State */
        LIBBLU_HDMV_IGS_COMPL_DEBUG("    Normal state\n");
        start_obj_id_ref = btn.normal_state_info.start_object_id_ref;
        end_obj_id_ref   = btn.normal_state_info.end_object_id_ref;

        if (0x00 == page_id)
          usage = (is_def_val_btn) ? ODS_DEFAULT_NORMAL_PAGE0 : ODS_USED_OTHER_PAGE0;

        if (_collectObjectRange(&obj_bms, compo->object_bitmaps, start_obj_id_ref, end_obj_id_ref, usage) < 0)
          goto free_return;

        /* Collect ODS used for Selected State */
        LIBBLU_HDMV_IGS_COMPL_DEBUG("    Selected state\n");
        start_obj_id_ref = btn.selected_state_info.start_object_id_ref;
        end_obj_id_ref   = btn.selected_state_info.end_object_id_ref;

        if (0x00 == page_id)
          usage = (is_def_val_btn) ? ODS_DEFAULT_SELECTED_PAGE0 : ODS_USED_OTHER_PAGE0;

        unsigned idx_first_sel_ods = nbPicsHdmvBitmapList(obj_bms);
        if (_collectObjectRange(&obj_bms, compo->object_bitmaps, start_obj_id_ref, end_obj_id_ref, usage) < 0)
          goto free_return;

        if (
          0x00 == page_id
          && idx_first_sel_ods != nbPicsHdmvBitmapList(obj_bms)
          && is_def_sel_btn
        ) {
          /* Mark first ODS used for default selected button of Page 0 (if exists) */
          HdmvBitmap * bm;
          if (getRefHdmvBitmapList(&obj_bms, &bm, idx_first_sel_ods) < 0)
            LIBBLU_HDMV_IGS_COMPL_ERROR_FRETURN("Unable to get default selected button.\n");
          bm->usage = MAX(bm->usage, ODS_DEFAULT_FIRST_SELECTED_PAGE0);
        }

        /* Collect ODS used for Activated State */
        LIBBLU_HDMV_IGS_COMPL_DEBUG("    Activated state\n");
        start_obj_id_ref = btn.activated_state_info.start_object_id_ref;
        end_obj_id_ref   = btn.activated_state_info.end_object_id_ref;

        if (0x00 == page_id)
          usage = (is_def_val_btn) ? ODS_DEFAULT_ACTIVATED_PAGE0 : ODS_USED_OTHER_PAGE0;

        if (_collectObjectRange(&obj_bms, compo->object_bitmaps, start_obj_id_ref, end_obj_id_ref, usage) < 0)
          goto free_return;
      }
    }

    /* Getting the number of collected objects. */
    unsigned nb_obj = obj_bms.nb_bitmaps;
    if (0 == nb_obj) {
      LIBBLU_HDMV_IGS_COMPL_DEBUG("  Empty page, skipping to next one.\n");
      continue;
    }

    /* Reorder objects for Page 0 */
    if (0x00 == page_id)
      sortByUsageHdmvBitmapList(&obj_bms);

    LIBBLU_HDMV_IGS_COMPL_DEBUG("  Creating palette for %u objects.\n", nb_obj);
    HdmvPalette pal;
    if (_buildPalette(&pal, &obj_bms, color_matrix) < 0)
      goto free_return;

    LIBBLU_HDMV_IGS_COMPL_DEBUG("  Apply palette to create objects.\n");
    if (_buildObjects(compo, &obj_bms, &pal, dither_method) < 0)
      goto free_return;

    /* Add finished palette to composition and reset list for next page */
    LIBBLU_HDMV_IGS_COMPL_DEBUG("  Saving generated palette.\n");
    uint8_t palette_id;
    if (addPaletteIgsCompilerComposition(compo, &pal, &palette_id) < 0)
      goto free_return;
    page->palette_id_ref = palette_id; /* Set page palette id */

    LIBBLU_HDMV_IGS_COMPL_DEBUG("  Flushing list.\n");
    flushHdmvBitmapList(&obj_bms);
  }

  LIBBLU_HDMV_IGS_COMPL_DEBUG(" Completed.\n");

  cleanHdmvBitmapList(obj_bms);
  return 0;

free_return:
  cleanHdmvBitmapList(obj_bms);
  return -1;
}

#undef COLLECT_STATE

static HdmvColorDitheringMethod _getDitherMethodFromConf(
  const IgsCompilerContext * ctx
)
{
  lbc * str = lookupIniFile(ctx->conf_hdl, "HDMV.DITHERMETHOD");
  if (NULL != str && lbc_equal(str, lbc_str("floydSteinberg")))
    return HDMV_PIC_CDM_FLOYD_STEINBERG;
  return HDMV_PIC_CDM_DISABLED;
}

static HdmvPaletteColorMatrix _getColorMatrixFromConf(
  const IgsCompilerContext * ctx
)
{
  lbc * str = lookupIniFile(ctx->conf_hdl, "HDMV.COLORMATRIX");
  if (NULL != str && lbc_equal(str, lbc_str("BT.709")))
    return HDMV_PAL_CM_BT_709;
  if (NULL != str && lbc_equal(str, lbc_str("BT.2020")))
    return HDMV_PAL_CM_BT_2020;
  return HDMV_PAL_CM_BT_601;
}

int processIgsCompiler(
  const lbc * xml_filepath,
  HdmvTimecodes * timecodes,
  IniFileContext conf_hdl
)
{
  IgsCompilerContext ctx = {0};
  if (_initIgsCompilerContext(&ctx, xml_filepath, timecodes, conf_hdl) < 0)
    return -1;

  LIBBLU_HDMV_IGS_COMPL_INFO("Compiling IGS...\n");
  if (parseIgsXmlFile(&ctx) < 0)
    goto free_return;

  HdmvColorDitheringMethod dm = _getDitherMethodFromConf(&ctx);
  HdmvPaletteColorMatrix cm = _getColorMatrixFromConf(&ctx);

  for (unsigned i = 0; i < ctx.data.nb_compositions; i++) {
    if (buildIgsCompilerComposition(&ctx.data.compositions[i], dm, cm) < 0)
      goto free_return;
  }

  _resetOriginalDirIgsCompilerContext(&ctx);

  /* Create XML output filename */
  lbc * igs_out_filename;
  int ret = lbc_asprintf(&igs_out_filename, HDMV_IGS_COMPL_OUTPUT_FMT, xml_filepath);
  if (ret < 0)
    LIBBLU_HDMV_IGS_COMPL_ERROR_GRETURN(free_return2, "Memory allocation error.\n");

  if (buildIgsCompiler(&ctx.data, igs_out_filename) < 0)
    goto free_return2;

  LIBBLU_HDMV_IGS_COMPL_INFO("Compilation finished, parsing output file.\n");

  free(igs_out_filename);
  _cleanIgsCompilerContext(ctx);
  return 0;

free_return2:
  free(igs_out_filename);
free_return:
  _cleanIgsCompilerContext(ctx);
  return -1;
}
