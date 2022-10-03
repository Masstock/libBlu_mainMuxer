#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "igs_compiler.h"

#define ECHO_DEBUG  LIBBLU_HDMV_IGS_COMPL_DEBUG

IgsCompilerContextPtr createIgsCompilerContext(
  const lbc * xmlFilename,
  IniFileContextPtr conf
)
{
  IgsCompilerContextPtr ctx;

  ECHO_DEBUG("Creating IGS Compiler context.\n");

  ctx = (IgsCompilerContextPtr) malloc(sizeof(IgsCompilerContext));
  if (NULL == ctx)
    LIBBLU_HDMV_IGS_COMPL_ERROR_NRETURN("Memory allocation error.\n");

  *ctx = (IgsCompilerContext) {
    .conf = conf
  };

  ECHO_DEBUG(" Initialization of libraries and ressources.\n");

  /* Init Picture libraries */
  initHdmvPictureLibraries(&ctx->imgLibs);

  /* Init libxml context */
  ctx->xmlCtx = createXmlContext(&echoErrorIgsXmlFile, &echoDebugIgsXmlFile);
  if (NULL == ctx->xmlCtx)
    goto free_return;

  /* Init HDMV elements allocation inventory */
  if (NULL == (ctx->inv = createHdmvSegmentsInventory()))
    goto free_return;

  /* Set working directory to allow file-relative files handling: */
  ECHO_DEBUG(" Redefinition of working directory.\n");
  if (updateIgsCompilerWorkingDirectory(ctx, xmlFilename) < 0)
    goto free_return;

  ECHO_DEBUG("Done.\n");
  return ctx;

free_return:
  ECHO_DEBUG("Failed.\n");

  destroyIgsCompilerContext(ctx);
  return NULL;
}

void resetOriginalDirIgsCompilerContext(
  IgsCompilerContextPtr ctx
)
{
  if (NULL == ctx)
    return;

  if (NULL != ctx->workingDir) {
    if (lbc_chdir(ctx->initialWorkingDir) < 0)
      LIBBLU_WARNING("Unable to retrieve original working path.\n");
    else
      ECHO_DEBUG(
        " Original working path restored ('%" PRI_LBCS "').\n",
        ctx->initialWorkingDir
      );

    free(ctx->workingDir);
    ctx->workingDir = NULL;
  }
}

void destroyIgsCompilerContext(
  IgsCompilerContextPtr ctx
)
{
  unsigned i;

  if (NULL == ctx)
    return;

  ECHO_DEBUG("Free IGS Compiler context memory.\n");

  cleanHdmvPictureLibraries(ctx->imgLibs);
  destroyXmlContext(ctx->xmlCtx);
  destroyHdmvSegmentsInventory(ctx->inv);

  for (i = 0; i < ctx->data.nbCompo; i++)
    destroyIgsCompilerComposition(ctx->data.compositions[i]);

#if 0
  if (NULL != ctx->workingDir) {
    if (lbc_chdir(ctx->initialWorkingDir) < 0)
      LIBBLU_WARNING("Unable to retrieve original working path.\n");
    else
      LIBBLU_DEBUG_COM(
        " Original working path restored ('%" PRI_LBCS "').\n",
        ctx->initialWorkingDir
      );

    free(ctx->workingDir);
    ctx->workingDir = NULL;
  }
#else
  resetOriginalDirIgsCompilerContext(ctx);
#endif

  free(ctx->xmlFilename);
  free(ctx);

  ECHO_DEBUG("Done.\n");
}

int getFileWorkingDirectory(
  lbc ** dirPath,
  lbc ** xmlFilename,
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

  if (NULL == (*xmlFilename = lbc_strdup(pathFilenamePart)))
    LIBBLU_HDMV_IGS_COMPL_ERROR_FRETURN("Memory allocation error.\n");

  /* Restore saved path style */
  lbc_cwk_path_set_style(savedStyle);
  return 0;

free_return:
  lbc_cwk_path_set_style(savedStyle);
  return -1;
}

int updateIgsCompilerWorkingDirectory(
  IgsCompilerContextPtr ctx,
  const lbc * xmlFilename
)
{
  /* Save current working directory */
  if (lbc_getwd(ctx->initialWorkingDir, PATH_BUFSIZE) < 0)
    return -1;
  ECHO_DEBUG("  Saved path: '%" PRI_LBCS "'.\n", ctx->initialWorkingDir);

  /* Getting working directory based on input XML filename */
  if (getFileWorkingDirectory(&ctx->workingDir, &ctx->xmlFilename, xmlFilename) < 0)
    return -1;
  ECHO_DEBUG("  New working path: '%" PRI_LBCS "'.\n", ctx->workingDir);
  ECHO_DEBUG("  XML filename: '%" PRI_LBCS "'.\n", ctx->xmlFilename);
  /* BUG: ctx->xmlFilename as 'invalid' read errors, make a proper copy */

  /* Redefinition of working path */
  if (lbc_chdir(ctx->workingDir) < 0)
    LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN(
      "Unable to redefine working directory to opened file location, "
      "%s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return 0;
}

static HdmvPaletteDefinitionPtr createAndBuildPalette(
  const HdmvPicturesListPtr list,
  HdmvPictureColorDitheringMethod ditherMeth,
  HdmvPaletteColorMatrix colorMatrix
)
{
  HdmvPaletteDefinitionPtr pal;

  /* static const HdmvPictureColorDitheringMethod ditherMeth =
    HDMV_PIC_CDM_FLOYD_STEINBERG
  ;
  static const HdmvPaletteColorMatrix palColorMatrix =
    HDMV_PAL_CM_BT_601
  ; */

  /* Create a new palette */
  if (NULL == (pal = createHdmvPaletteDefinition(0x0)))
    return NULL;

  /* Fill the palette according to pictures */
  if (buildPaletteListHdmvPalette(pal, list, 255, NULL) < 0)
    goto free_return;

  /* Sort palette */
  sortEntriesHdmvPaletteDefinition(pal);

  /* Apply the palette on pictures */
  if (setPaletteHdmvPicturesList(list, pal, ditherMeth) < 0)
    goto free_return;

  /* Set palette conversion method */
  setColorMatrixHdmvPaletteDefinition(pal, colorMatrix);

  return pal;

free_return:
  destroyHdmvPaletteDefinition(pal);
  return NULL;
}

int buildIgsCompilerComposition(
  IgsCompilerCompositionPtr compo,
  HdmvPictureColorDitheringMethod ditherMeth,
  HdmvPaletteColorMatrix colorMatrix
)
{
  unsigned nbPages, page_id;
  HdmvPicturesListPtr pictures;

  assert(NULL != compo);

  LIBBLU_HDMV_IGS_COMPL_INFO("Building interactive compositions...\n");

  if (NULL == (pictures = createHdmvPicturesList()))
    goto free_return;

  LIBBLU_HDMV_IGS_COMPL_DEBUG("Collecting content of each page:\n");

  /* Collect all composition pictures */
  nbPages = compo->interactiveComposition.number_of_pages;
  for (page_id = 0; page_id < nbPages; page_id++) {
    /* Create a palette for each page */
    HdmvPageParameters * page = compo->interactiveComposition.pages[page_id];

    HdmvPaletteDefinitionPtr pal;
    unsigned nbPics, palId, bogId;

    LIBBLU_HDMV_IGS_COMPL_DEBUG(" Page %u\n", page_id);
    LIBBLU_HDMV_IGS_COMPL_DEBUG("  Collecting every object from each BOG:\n");

    for (bogId = 0; bogId < page->number_of_BOGs; bogId++) {
      HdmvButtonOverlapGroupParameters * bog = page->bogs[bogId];

      unsigned button_id;

      for (button_id = 0; button_id < bog->number_of_buttons; button_id++) {
        /* Collect all pictures */
        HdmvButtonParam * btn = bog->buttons[button_id];

#define COLLECT_STATE(name)                                                   \
        if (0xFFFF != btn->name.start_object_ref) {                           \
          uint16_t objId;                                                     \
                                                                              \
          for (                                                               \
            objId = btn->name.start_object_ref;                               \
            objId != btn->name.end_object_ref + 1;                            \
            objId++                                                           \
          ) {                                                                 \
            assert(objId < compo->nbUsedObjPics);                             \
                                                                              \
            if (addHdmvPicturesList(pictures, compo->objPics[objId]) < 0)     \
              goto free_return;                                               \
          }                                                                   \
        }

        /* Normal state */
        COLLECT_STATE(normal_state_info);

        /* Selected state */
        COLLECT_STATE(selected_state_info);

        /* Activated state */
        COLLECT_STATE(activated_state_info);

#undef COLLECT_STATE
      }
    }

    /* Getting the number of collected objects. */
    nbPics = nbPicsHdmvPicturesList(pictures);

    if (!nbPics) {
      LIBBLU_HDMV_IGS_COMPL_DEBUG("  Empty page, skipping to next one.\n");
      continue;
    }

    LIBBLU_HDMV_IGS_COMPL_DEBUG("  Creating palette for %u pictures.\n", nbPics);
    pal = createAndBuildPalette(pictures, ditherMeth, colorMatrix);
    if (NULL == pal)
      goto free_return;

    /* Add finished palette to composition and reset list for next page */
    LIBBLU_HDMV_IGS_COMPL_DEBUG("  Saving generated palette.\n");
    if (addPaletteIgsCompilerComposition(compo, pal, &palId) < 0) {
      destroyHdmvPaletteDefinition(pal);
      goto free_return;
    }

    /* Save generated ID */
    if (UINT8_MAX < palId)
      LIBBLU_HDMV_IGS_COMPL_ERROR_FRETURN("Too many palettes present (id exceed 0xFF).\n");
    page->palette_id_ref = palId; /* Set page palette id */

    LIBBLU_HDMV_IGS_COMPL_DEBUG("  Flushing list.\n");
    flushHdmvPicturesList(pictures);
  }

  LIBBLU_HDMV_IGS_COMPL_DEBUG(" Completed.\n");

  destroyHdmvPicturesList(pictures);
  return 0;

free_return:
  destroyHdmvPicturesList(pictures);
  return -1;
}

static HdmvPictureColorDitheringMethod getDitherMethodFromIniIgsCompiler(
  IgsCompilerContextPtr ctx
)
{
  lbc * str;

  if (NULL == (str = lookupIniFile(ctx->conf, "HDMV.DITHERMETHOD")))
    return HDMV_PIC_CDM_DISABLED;

  if (lbc_equal(str, lbc_str("floydSteinberg")))
    return HDMV_PIC_CDM_FLOYD_STEINBERG;

  return HDMV_PIC_CDM_DISABLED;
}

static HdmvPaletteColorMatrix getColorMatrixFromIniIgsCompiler(
  IgsCompilerContextPtr ctx
)
{
  lbc * str;

  if (NULL == (str = lookupIniFile(ctx->conf, "HDMV.COLORMATRIX")))
    return HDMV_PAL_CM_BT_601;

  if (lbc_equal(str, lbc_str("BT.709")))
    return HDMV_PAL_CM_BT_709;
  if (lbc_equal(str, lbc_str("BT.2020")))
    return HDMV_PAL_CM_BT_2020;

  return HDMV_PAL_CM_BT_601;
}

int processIgsCompiler(
  const lbc * xmlPath,
  IniFileContextPtr conf
)
{
  int ret;

  IgsCompilerContextPtr ctx;
  lbc * igsOutputFilename;
  unsigned compId;

  HdmvPictureColorDitheringMethod ditherMeth;
  HdmvPaletteColorMatrix colorMatrix;

  LIBBLU_HDMV_IGS_COMPL_INFO("Compiling IGS...\n");

  if (NULL == (ctx = createIgsCompilerContext(xmlPath, conf)))
    return -1;

  if (parseIgsXmlFile(ctx) < 0)
    goto free_return;

  ditherMeth = getDitherMethodFromIniIgsCompiler(ctx);
  colorMatrix = getColorMatrixFromIniIgsCompiler(ctx);

  for (compId = 0; compId < ctx->data.nbCompo; compId++) {
    if (buildIgsCompilerComposition(GET_COMP(ctx, compId), ditherMeth, colorMatrix) < 0)
      goto free_return;
  }

  resetOriginalDirIgsCompilerContext(ctx);

  /* Create XML output filename */
  ret = lbc_asprintf(&igsOutputFilename, HDMV_IGS_COMPL_OUTPUT_FMT, xmlPath);
  if (ret < 0)
    LIBBLU_HDMV_IGS_COMPL_ERROR_GRETURN(free_return2, "Memory allocation error.\n");

  if (buildIgsCompiler(&ctx->data, igsOutputFilename) < 0)
    goto free_return2;

  LIBBLU_HDMV_IGS_COMPL_INFO("Compilation finished, parsing output file.\n");

  free(igsOutputFilename);
  destroyIgsCompilerContext(ctx);
  return 0;

free_return2:
  free(igsOutputFilename);
free_return:
  destroyIgsCompilerContext(ctx);
  return -1;
}
