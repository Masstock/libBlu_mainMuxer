/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 * Copyright (C) 2009 Grigori Goronzy <greg@geekmind.org>
 *
 * This file is part of libass.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

#include <inttypes.h>
#include <math.h>

#include <ass/ass.h>

#include "pgs_generator.h"

#define BPP  4

#if 0
typedef struct image_s {
  int width, height, stride;
  uint8_t * buffer;  // ABGR32
} image_t;
#endif

ASS_Library * ass_library;
ASS_Renderer * ass_renderer;

void msg_callback(
  int level,
  const char * fmt,
  va_list va,
  void * data
)
{
  (void) data;

  if (level > 1)
    return;

  printf("libass: ");
  vprintf(fmt, va);
  printf("\n");
}

#if 0
#include <png.h>

static void _write_png(
  char * fname, HdmvBitmap * bitmap)
{
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info_ptr = png_create_info_struct(png_ptr);

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return;
  }

  FILE * fp;
  if (NULL == (fp = fopen(fname, "wb"))) {
    printf("PNG Error opening %s for writing!\n", fname);
    return;
  }

  png_init_io(png_ptr, fp);
  png_set_compression_level(png_ptr, 3);
  png_set_IHDR(
    png_ptr, info_ptr, bitmap->width, bitmap->height,
    8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
  );

  png_write_info(png_ptr, info_ptr);
  if (!IS_BIG_ENDIAN) {
    // Ensure uint32_t -> uint8_t correct conversion
    png_set_swap_alpha(png_ptr);
    png_set_bgr(png_ptr);
  }

  png_byte ** row_pointers;
  if (NULL == (row_pointers = malloc(bitmap->height * sizeof(png_byte *)))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return;
  }

  for (uint16_t k = 0; k < bitmap->height; k++)
    row_pointers[k] = (uint8_t *) &bitmap->rgba[k * bitmap->width];

  png_write_image(png_ptr, row_pointers);
  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  free(row_pointers);

  fclose(fp);
}
#endif

static void init(int frame_w, int frame_h)
{
  ass_library = ass_library_init();
  if (NULL == ass_library) {
    printf("ass_library_init failed!\n");
    exit(1);
  }

  ass_set_message_cb(ass_library, msg_callback, NULL);
  ass_set_extract_fonts(ass_library, 1);

  ass_renderer = ass_renderer_init(ass_library);
  if (!ass_renderer) {
    printf("ass_renderer_init failed!\n");
    exit(1);
  }

  ass_set_storage_size(ass_renderer, frame_w, frame_h);
  ass_set_frame_size(ass_renderer, frame_w, frame_h);
  ass_set_fonts(
    ass_renderer, NULL, "sans-serif",
    ASS_FONTPROVIDER_AUTODETECT, NULL, 1
  );
}

// static image_t *_gen_image(int width, int height)
// {
//   image_t * img;
//   if (NULL == (img = malloc(sizeof(image_t))))
//     return NULL;

//   img->width = width;
//   img->height = height;
//   img->stride = width * BPP;

//   if (NULL == (img->buffer = calloc(1, height * width * BPP))) {
//     free(img);
//     return NULL;
//   }

//   return img;
// }

static void _blendASSImageToHdmvBitmap(
  HdmvBitmap * bitmap,
  ASS_Image * img
)
{
  double src_r = getR_RGBA(img->color);
  double src_g = getG_RGBA(img->color);
  double src_b = getB_RGBA(img->color);
  double src_a = getA_RGBA(img->color);
  double opacity = 1. - src_a;

  uint8_t  * src = img->bitmap;
  uint32_t * dst = &bitmap->rgba[img->dst_y * bitmap->width + img->dst_x];

  for (int y = 0; y < img->h; y++) {
    for (int x = 0; x < img->w; x++) {
      double a_a = src[x] / 255. * opacity;
      double r_a = src_r * a_a;
      double g_a = src_g * a_a;
      double b_a = src_b * a_a;

      double a_b = getA_RGBA(dst[x]);
      double b_b = getB_RGBA(dst[x]) * a_b;
      double g_b = getG_RGBA(dst[x]) * a_b;
      double r_b = getR_RGBA(dst[x]) * a_b;

      double a_0 =  a_a + a_b * (1. - a_a);
      double b_0 = (b_a + b_b * (1. - a_a)) / a_0;
      double g_0 = (g_a + g_b * (1. - a_a)) / a_0;
      double r_0 = (r_a + r_b * (1. - a_a)) / a_0;

      dst[x] = getRGBA(r_0, g_0, b_0, a_0);
    }

    src += img->stride;
    dst += bitmap->width;
  }
}

// static void _draw_box_pixel(uint8_t * dst, uint32_t rgba)
// {
//   dst[0] = (rgba      ) & 0xFF;
//   dst[1] = (rgba >>  8) & 0xFF;
//   dst[2] = (rgba >> 16) & 0xFF;
//   dst[3] = (rgba >> 24) & 0xFF;
// }

char * font_provider_labels[] = {
  [ASS_FONTPROVIDER_NONE]       = "None",
  [ASS_FONTPROVIDER_AUTODETECT] = "Autodetect",
  [ASS_FONTPROVIDER_CORETEXT]   = "CoreText",
  [ASS_FONTPROVIDER_FONTCONFIG] = "Fontconfig",
  [ASS_FONTPROVIDER_DIRECTWRITE]= "DirectWrite",
};

static void _print_font_providers(ASS_Library * ass_library)
{
  ASS_DefaultFontProvider * providers;
  size_t providers_size = 0;
  ass_get_available_font_providers(ass_library, &providers, &providers_size);
  printf("test.c: Available font providers (%zu): ", providers_size);
  for (size_t i = 0; i < providers_size; i++) {
    const char * separator = i > 0 ? ", ": "";
    printf("%s'%s'", separator,  font_provider_labels[providers[i]]);
  }
  printf(".\n");
  free(providers);
}

// static const char * _change_str(
//   int change
// )
// {
//   switch (change) {
//     case 0:  return "Identical";
//     case 1:  return "Positions change";
//     case 2:  return "Content change";
//     default: return "Unknown";
//   }
// }

static HdmvRectangle _assImageRectangle(
  const ASS_Image * img,
  unsigned frame_w,
  unsigned frame_h
)
{
  uint16_t off_x = 0;
  uint16_t off_y = 0;
  uint16_t w = img->w;
  uint16_t h = img->h;
  uint16_t i, j;

  if (!w || !h)
    return emptyRectangle();

  /* Trim left */
  for (i = 0; i < w; i++) {
    uint8_t column = 0x00;
    for (j = 0; j < h; j++)
      column |= img->bitmap[j* img->stride+i];
    if (0x00 != column)
      break;
  }
  off_x += i; w -= i;

  /* Trim right */
  for (i = w-1; 0 < w && off_x < i; i--) {
    uint8_t column = 0x00;
    for (j = 0; j < h; j++)
      column |= img->bitmap[j* img->stride+off_x+i];
    if (0x00 != column)
      break;
  }
  w = i+1;

  /* Trim top */
  for (j = 0; j < h; j++) {
    uint8_t line = 0x00;
    for (i = 0; i < w; i++)
      line |= img->bitmap[j*img->stride+off_x+i];
    if (0x00 != line)
      break;
  }
  off_y += j; h -= j;

  /* Trim bottom */
  for (j = h-1; 0 < h && off_y < j; j--) {
    uint8_t line = 0x00;
    for (i = 0; i < w; i++)
      line |= img->bitmap[(off_y+j)*img->stride+off_x+i];
    if (0x00 != line)
      break;
  }
  h = j+1;

  uint16_t x = img->dst_x + off_x;
  uint16_t y = img->dst_y + off_y;
  if (frame_w <= x || frame_h <= y || 0 == h || 0 == w)
    return emptyRectangle();

  return (HdmvRectangle) {
    .x = x,
    .y = y,
    .w = MIN(x + w, frame_w-1) - x,
    .h = MIN(y + h, frame_h-1) - y
  };
}

static void _drawSurroundingBoxDimensions(
  HdmvBitmap * bitmap,
  HdmvRectangle dim,
  uint32_t rgba
)
{
  for (int j = 0; j <= dim.w; j++) {
    setPixelHdmvBitmap(bitmap, rgba, dim.x + j, dim.y);
    setPixelHdmvBitmap(bitmap, rgba, dim.x + j, dim.y + dim.h);
    // _draw_box_pixel(
    //   &bitmap->rgba[dim.y * bitmap->width + (dim.x + j)], rgba
    // );
    // _draw_box_pixel(
    //   &bitmap->rgba[
    //     (dim.y + dim.h) * bitmap->stride + (dim.x + j)
    //   ], rgba
    // );
  }
  for (int i = 0; i <= dim.h; i++) {
    setPixelHdmvBitmap(bitmap, rgba, dim.x, dim.y + i);
    setPixelHdmvBitmap(bitmap, rgba, dim.x + dim.w, dim.y + i);
    // _draw_box_pixel(
    //   &bitmap->rgba[
    //     (dim.y + i) * bitmap->stride + dim.x
    //   ], rgba
    // );
    // _draw_box_pixel(
    //   &bitmap->rgba[
    //     (dim.y + i) * bitmap->stride + (dim.x + dim.w)
    //   ], rgba
    // );
  }
}

#if 0

#define NB_WINDOWS  2

typedef struct {
  HdmvRectangle windows[NB_WINDOWS];
} RenderingWindows;

void resetRenderingWindows(
  RenderingWindows * rw
)
{
  memset(rw, 0, sizeof(*rw));
}

void addRenderingWindows(
  RenderingWindows * rw,
  HdmvRectangle obj_dims
)
{
  if (isEmptyRectangle(obj_dims))
    return;

  /* First window not used */
  if (isEmptyRectangle(rw->windows[0])) {
    rw->windows[0] = obj_dims;
    return;
  }

  /* Second window not used */
  if (isEmptyRectangle(rw->windows[1])) {
    if (areCollidingRectangle(rw->windows[0], obj_dims))
      rw->windows[0] = mergeRectangle(rw->windows[0], obj_dims);
    else
      rw->windows[1] = obj_dims;
    return;
  }

  /* Merge with one of the both windows */
  HdmvRectangle win_0_merged = mergeRectangle(rw->windows[0], obj_dims);
  HdmvRectangle win_1_merged = mergeRectangle(rw->windows[1], obj_dims);
  if (areaRectangle(win_0_merged) < areaRectangle(win_1_merged))
    rw->windows[0] = win_0_merged;
  else
    rw->windows[1] = win_1_merged;

  /* Test for collision between the two windows */
  if (areCollidingRectangle(rw->windows[0], rw->windows[1])) {
    /* Merge in first window, empty second one */
    rw->windows[0] = mergeRectangle(rw->windows[0], rw->windows[1]);
    rw->windows[1] = emptyRectangle();
  }
}

#endif

// static void _drawNodesMergingTreeNode(
//   HdmvBitmap * rframe,
//   const MergingTreeNode * node,
//   unsigned level
// )
// {
//   if (NULL == node)
//     return;

//   uint32_t rgba = (MIN(0xFF, level * 15) << 24) | 0xFF;
//   _drawSurroundingBoxDimensions(rframe, node->box, rgba);
//   _drawNodesMergingTreeNode(rframe, node->left, level+1);
//   _drawNodesMergingTreeNode(rframe, node->right, level+1);
// }

static uint32_t _drawSurroundingBoxesObjectsPgsFrame(
  HdmvBitmap * rframe,
  const PgsFrame * frame,
  uint32_t rgba
)
{
  uint32_t size = 0;

  for (unsigned i = 0; i < frame->nb_bitmaps; i++) {
    const HdmvRectangle dim = frame->bitmaps_pos[i];
    printf("  - %u: x=%u y=%u w=%u h=%u;\n", i, dim.x, dim.y, dim.w, dim.h);
    _drawSurroundingBoxDimensions(rframe, dim, rgba);
    size += dim.w * dim.h;
  }

  return size;
}

static int _peekBitmapsPgsFrame(
  PgsFrame * frame,
  const HdmvBitmap * rframe,
  MergingTreeNode * img_merging_tree
)
{
  lb_static_assert(2 == HDMV_MAX_NB_PCS_COMPOS);

  if (NULL == img_merging_tree) {
    // Empty
    frame->nb_bitmaps = 0;
    return 0;
  }

  if (isSingleZoneMergingTreeNode(img_merging_tree)) {
    // Single
    frame->bitmaps_pos[0] = img_merging_tree->box;
    if (cropCopyHdmvBitmap(&frame->bitmaps[0], rframe, frame->bitmaps_pos[0]) < 0)
      return -1;
    frame->nb_bitmaps = 1;
    return 0;
  }

  // Multiple leaves
  frame->bitmaps_pos[0] = img_merging_tree->left->box;
  if (cropCopyHdmvBitmap(&frame->bitmaps[0], rframe, frame->bitmaps_pos[0]) < 0)
    return -1;
  frame->bitmaps_pos[1] = img_merging_tree->right->box;
  if (cropCopyHdmvBitmap(&frame->bitmaps[1], rframe, frame->bitmaps_pos[1]) < 0)
    return -1;
  frame->nb_bitmaps = 2;

  return 0;
}

static void _peekWindowsPgsFrameSequence(
  PgsFrameSequence * seq,
  MergingTreeNode * windows_merging_tree
)
{
  HdmvRectangle win0, win1;

  if (NULL == windows_merging_tree) {
    LIBBLU_HDMV_PGS_ASS_DEBUG(" Empty, no window.\n");
    seq->nb_windows = 0;
    return;
  }

  if (isSingleZoneMergingTreeNode(windows_merging_tree)) {
    // Single
    seq->windows[0] = win0 = windows_merging_tree->box;
    seq->nb_windows = 1;

    LIBBLU_HDMV_PGS_ASS_DEBUG(
      " - 0: x=%u y=%u w=%u h=%u;\n",
      win0.x, win0.y, win0.w, win0.h
    );
    return;
  }

  // Multiple leaves
  seq->windows[0] = win0 = windows_merging_tree->left->box;
  seq->windows[1] = win1 = windows_merging_tree->right->box;
  seq->nb_windows = 2;

  LIBBLU_HDMV_PGS_ASS_DEBUG(
    " - 0: x=%u y=%u w=%u h=%u;\n",
    win0.x, win0.y, win0.w, win0.h
  );
  LIBBLU_HDMV_PGS_ASS_DEBUG(
    " - 1: x=%u y=%u w=%u h=%u;\n",
    win1.x, win1.y, win1.w, win1.h
  );
}

static int64_t _computeWindowsDrawingDuration(
  PgsFrameSequence * seq
)
{
  int64_t size = 0;
  for (unsigned i = 0; i < seq->nb_windows; i++)
    size += areaRectangle(seq->windows[i]);
  return DIV_ROUND_UP(9LL * size, 3200);
}

static int _computeWindowsPgsFrameSequence(
  PgsFrameSequence * seq
)
{
  LIBBLU_HDMV_PGS_ASS_DEBUG("Building sequence window(s):\n");

  MergingTreeNode * windows_mt = NULL;

  for (const PgsFrame * frame = seq->first_frm; NULL != frame; frame = frame->next) {
    for (unsigned i = 0; i < frame->nb_bitmaps; i++) {
      if (!isEmptyRectangle(frame->bitmaps_pos[i])) {
        if (insertMergingTreeNode(&windows_mt, frame->bitmaps_pos[i]) < 0)
          return -1;
      }
    }
  }

  _peekWindowsPgsFrameSequence(seq, windows_mt);
  destroyMergingTreeNode(windows_mt);

  int64_t min_drawing_duration = _computeWindowsDrawingDuration(seq);

  LIBBLU_HDMV_PGS_ASS_DEBUG(
    " => Minimal windows drawing duration: %" PRId64 " ticks.\n",
    min_drawing_duration
  );

  seq->min_drawing_duration = min_drawing_duration;

  return 0;
}

static bool _checkDrawingDurationEpochStart(
  PgsFrameSequence * cur_seq
)
{
  const PgsFrameSequence * prev_seq = cur_seq->prev_seq;
  if (NULL == prev_seq)
    return true; // No previous frame sequence, we are fine

  assert(NULL != prev_seq->last_frm);
  if (cur_seq->first_frm->timestamp < prev_seq->last_frm->timestamp + cur_seq->min_drawing_duration)
    return false; // Overlap
  return true;
}

static void _initCompositionObject(
  PgsCompositionObject * compo_obj,
  HdmvRectangle bitmap_pos
)
{
  LIBBLU_HDMV_PGS_ASS_DEBUG(
    "   - Naive bitmap: x=%u y=%u w=%u h=%u.\n",
    bitmap_pos.x, bitmap_pos.y, bitmap_pos.w, bitmap_pos.h
  );

  *compo_obj = (PgsCompositionObject) {
    .x = bitmap_pos.x,
    .y = bitmap_pos.y,
    .cropping = {
      .width  = bitmap_pos.w,
      .height = bitmap_pos.h
    }
  };
}

static int _addRefPgsObjectVersionList(
  PgsObjectVersionList * ver_list,
  PgsFrame * frame
)
{
  PgsObjectVersion * ver = &ver_list->versions[ver_list->nb_used_versions-1];

  if (0 < ver->nb_used_references && ver->references[ver->nb_used_references-1] == frame)
    return 0; // Frame already registered as reference

  if (ver->nb_alloc_references <= ver->nb_used_references) {
    unsigned new_size = GROW_ALLOCATION(ver->nb_alloc_references, 1);
    PgsFrame ** new_array = (PgsFrame **) realloc(
      ver->references,
      new_size * sizeof(PgsFrame *)
    );
    if (NULL == new_array)
      LIBBLU_HDMV_PGS_ASS_ERROR_RETURN("Memory allocation error.\n");

    ver->references = new_array;
    ver->nb_alloc_references = new_size;
  }

  ver->references[ver->nb_used_references++] = frame;
  return 0;
}

static int _addPgsObjectVersionList(
  PgsObjectVersionList * ver_list,
  PgsFrame * frame,
  HdmvObject obj
)
{
  if (ver_list->nb_allocated_versions <= ver_list->nb_used_versions) {
    unsigned new_size = GROW_ALLOCATION(ver_list->nb_allocated_versions, 1);
    PgsObjectVersion * new_array = (PgsObjectVersion *) realloc(
      ver_list->versions,
      new_size * sizeof(PgsObjectVersion)
    );
    if (NULL == new_array)
      LIBBLU_HDMV_PGS_ASS_ERROR_RETURN("Memory allocation error.\n");

    ver_list->versions = new_array;
    ver_list->nb_allocated_versions = new_size;
  }

  assert(ver_list->width  == obj.pal_bitmap.width);
  assert(ver_list->height == obj.pal_bitmap.height);

  unsigned version = ver_list->nb_used_versions++;
  uint8_t obj_ver_nb = version & 0xFF;

  obj.desc.object_version_number = obj_ver_nb;
  ver_list->versions[version] = (PgsObjectVersion) {
    .object = obj
  };

  LIBBLU_HDMV_PGS_ASS_DEBUG(
    "    Adding object_id = %u, version = %" PRIu16 " (0x%02" PRIX8 ").\n",
    obj.desc.object_id,
    version,
    obj_ver_nb
  );

  if (_addRefPgsObjectVersionList(ver_list, frame) < 0)
    return -1;

  return 0;
}

static int _addNewObjectPgsFrameSequence(
  PgsFrameSequence * seq,
  PgsFrame * frame,
  HdmvObject obj,
  uint16_t * id_ret
)
{
  LIBBLU_HDMV_PGS_ASS_DEBUG("   Adding object:\n");

  if (HDMV_OD_PG_MAX_NB_OBJ <= seq->nb_used_objects)
    LIBBLU_HDMV_PGS_ASS_ERROR_RETURN(
      "Run out of ids, too many unique objects used between "
      "timestamps %" PRId64 " to %" PRId64 " 27MHz clock ticks.\n",
      seq->first_frm->timestamp,
      frame->timestamp
    );

  uint32_t obj_dec_size = decodedSizeHdmvObject(obj);
  LIBBLU_HDMV_PGS_ASS_DEBUG(
    "    Object buffer size: %" PRIu32 " bytes.\n",
    obj_dec_size
  );

  if (HDMV_PG_DB_SIZE < seq->dec_obj_buffer_usage + obj_dec_size)
    LIBBLU_HDMV_PGS_ASS_ERROR_RETURN(
      "Decoded Object Buffer overflow, too many unique objects used between "
      "timestamps %" PRId64 " to %" PRId64 " 27MHz clock ticks.\n",
      seq->first_frm->timestamp,
      frame->timestamp
    );

  if (seq->nb_allocated_objects <= seq->nb_used_objects) {
    unsigned new_size = GROW_ALLOCATION(seq->nb_allocated_objects, 8);
    PgsObjectVersionList * new_array = (PgsObjectVersionList *) realloc(
      seq->objects,
      new_size * sizeof(PgsObjectVersionList)
    );
    if (NULL == new_array)
      LIBBLU_HDMV_PGS_ASS_ERROR_RETURN("Memory allocation error.\n");

    seq->objects = new_array;
    seq->nb_allocated_objects = new_size;
  }

  uint16_t obj_id = seq->nb_used_objects++;
  obj.desc.object_id = obj_id;

  PgsObjectVersionList * ver_list = &seq->objects[obj_id];
  *ver_list = (PgsObjectVersionList) {
    .width  = obj.pal_bitmap.width,
    .height = obj.pal_bitmap.height,
    .decode_duration = computeObjectDataDecodeDuration(
      HDMV_STREAM_TYPE_PGS,
      obj.pal_bitmap.width,
      obj.pal_bitmap.height
    )
  };

  LIBBLU_HDMV_PGS_ASS_DEBUG(
    "    Creating object_id = %" PRIu16 ", width = %u, height = %u.\n",
    obj_id,
    ver_list->width,
    ver_list->height
  );

  if (_addPgsObjectVersionList(ver_list, frame, obj) < 0)
    return -1;

  seq->dec_obj_buffer_usage += obj_dec_size;
  LIBBLU_HDMV_PGS_ASS_DEBUG(
    "    Decoded Object Buffer usage: %" PRIu32 " / %" PRIu32 " bytes.\n",
    seq->dec_obj_buffer_usage,
    HDMV_PG_DB_SIZE
  );

  if (NULL != id_ret)
    *id_ret = obj_id;
  return 0;
}

static int64_t _computePlaneInitializationTime(
  const PgsFrameSequence * seq,
  const PgsFrame * frame,
  HdmvVDParameters video_desc
)
{
  // See #hdmv_context._computePlaneInitializationTime()

  if (NULL == frame->prev) {
    /* First frame in sequence, Epoch Start, clear whole graphical plane */
    return getHDMVPlaneClearTime(
      HDMV_STREAM_TYPE_PGS,
      video_desc.video_width,
      video_desc.video_height
    );
  }

  /* Not epoch start, clear only empty windows */
  int64_t init_duration = 0;

  for (unsigned window_id = 0; window_id < seq->nb_windows; window_id++) {
    bool empty_window = true;

    for (unsigned i = 0; i < frame->nb_compo_obj; i++) {
      if (frame->compo_obj[i].win_id_ref == window_id) {
        /* A composition object use the window, mark as not empty */
        empty_window = false;
        break;
      }
    }

    if (empty_window) {
      /* Empty window, clear it */
      init_duration += DIV_ROUND_UP(
        9LL * areaRectangle(seq->windows[window_id]),
        3200
      );
    }
  }

  // Add 1 tick delay, observed to avoid WDS DTS being equal to its PTS.
  return init_duration + 1;
}

static int64_t _getFrameObjectDecodingDuration(
  const PgsFrameSequence * seq,
  const PgsFrame * frame,
  uint16_t obj_id_ref
)
{
  assert(obj_id_ref < seq->nb_used_objects);

  for (unsigned i = 0; i < frame->nb_obj; i++) {
    if (frame->obj_ids[i] == obj_id_ref) // EXISTS(DS_n, object_id)
      return seq->objects[obj_id_ref].decode_duration;
  }

  return 0; // !EXISTS(DS_n, object_id)
}

static int64_t _computeWindowTransferDuration(
  const PgsFrameSequence * seq,
  uint16_t window_id
)
{
  assert(window_id < seq->nb_windows);
  return DIV_ROUND_UP(
    9LL * areaRectangle(seq->windows[window_id]),
    3200
  );
}

static int64_t _computeMinimalObjectsDecodingDuration(
  const PgsFrameSequence * seq,
  const PgsFrame * frame
)
{
  int64_t decode_duration = 0;
  for (unsigned i = 0; i < frame->nb_obj; i++)
    decode_duration += seq->objects[frame->obj_ids[i]].decode_duration;
  return decode_duration;
}

static void _computeAndSetCompositionDecodingDurations(
  const PgsFrameSequence * seq,
  PgsFrame * frame,
  HdmvVDParameters video_desc
)
{
  // See #hdmv_context._computePGDisplaySetDecodeDuration()

  LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
    "Compute DECODE_DURATION of PG Display Set:\n"
  );

  int64_t decode_duration = _computePlaneInitializationTime(
    seq, frame, video_desc
  ); // Time required to clear Graphical Plane (or empty Windows).
  frame->init_duration = decode_duration;

  LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
    " => PLANE_INITIALIZATION_TIME(DS_n): %" PRId64 " ticks.\n",
    decode_duration
  );

  int64_t min_drawing_duration = 0;

  if (2 == frame->nb_compo_obj) {
    PgsCompositionObject obj_0 = frame->compo_obj[0];
    PgsCompositionObject obj_1 = frame->compo_obj[1];

    int64_t obj_0_decode_duration = _getFrameObjectDecodingDuration(seq, frame, obj_0.obj_id_ref);

    LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
      "  => ODS_DECODE_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_decode_duration
    );

    int64_t obj_1_decode_duration = _getFrameObjectDecodingDuration(seq, frame, obj_1.obj_id_ref);

    LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
      "  => ODS_DECODE_DURATION(DS_n.PCS.OBJ[1]): %" PRId64 " ticks.\n",
      obj_1_decode_duration
    );

    decode_duration = MAX(decode_duration, obj_0_decode_duration);

    if (obj_0.win_id_ref == obj_1.win_id_ref) {
      /* Both composition objects share the same window */
      decode_duration = MAX(decode_duration, obj_0_decode_duration + obj_1_decode_duration);

      int64_t obj_0_transfer_duration = _computeWindowTransferDuration(seq, obj_0.win_id_ref);

      LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
        "  => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

      decode_duration += obj_0_transfer_duration;
      min_drawing_duration += obj_0_transfer_duration;
    }
    else {
      int64_t obj_0_transfer_duration = _computeWindowTransferDuration(seq, obj_0.win_id_ref);

      LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
        "  => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

      decode_duration       += obj_0_transfer_duration;
      min_drawing_duration += obj_0_transfer_duration;
      decode_duration = MAX(decode_duration, obj_0_decode_duration + obj_1_decode_duration);

      int64_t obj_1_transfer_duration = _computeWindowTransferDuration(seq, obj_1.win_id_ref);

      LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
        "  => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[1]): %" PRId64 " ticks.\n",
        obj_1_transfer_duration
      );

      decode_duration       += obj_1_transfer_duration;
      min_drawing_duration += obj_1_transfer_duration;
    }
  }
  else if (1 == frame->nb_compo_obj) {
    PgsCompositionObject obj_0 = frame->compo_obj[0];
    int64_t obj_0_decode_duration = _getFrameObjectDecodingDuration(seq, frame, obj_0.obj_id_ref);

    LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
      "  => ODS_DECODE_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_decode_duration
    );

    decode_duration = MAX(decode_duration, obj_0_decode_duration);

    int64_t obj_0_transfer_duration = _computeWindowTransferDuration(seq, obj_0.win_id_ref);

      LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
        "  => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

    decode_duration       += obj_0_transfer_duration;
    min_drawing_duration += obj_0_transfer_duration;
  }

  frame->min_drawing_duration = min_drawing_duration;

  int64_t obj_min_decode_duration = _computeMinimalObjectsDecodingDuration(
    seq, frame
  );
  frame->min_obj_decode_duration = obj_min_decode_duration;

  LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
    "  => MIN_DECODE_DURATION(DS_n.ODS): %" PRId64 " ticks.\n",
    obj_min_decode_duration
  );

  decode_duration = MAX(decode_duration, obj_min_decode_duration);

  LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(
    " => DECODE_DURATION(DS_n): %" PRId64 " ticks.\n",
    decode_duration
  );

  frame->decode_duration = decode_duration;
}

static void _abortWholeSequence(
  PgsFrameSequence ** seq_ptr
)
{
  PgsFrameSequence * seq = *seq_ptr;

  if (NULL != seq->prev_seq)
    seq->prev_seq->next_seq = seq->next_seq;
  *seq_ptr = seq->prev_seq;

  seq->prev_seq = NULL;
  seq->next_seq = NULL;
  destroyPgsFrameSequence(seq);
}

static int _checkDecodeTimePgsFrame(
  const PgsFrameSequence * seq,
  const PgsFrame * frame
)
{
  /* Check decode delay against previous frame in same sequence */
  PgsFrame * prev_frame = frame->prev;
  if (NULL == prev_frame) {
    /* No previous frame in same sequence */
    const PgsFrameSequence * prev_seq = seq->prev_seq;
    if (NULL != prev_seq) {
      /* Check delay with previous sequence */
      const PgsFrame * prev_frame = prev_seq->last_frm;
      if (prev_frame->timestamp + seq->min_drawing_duration > frame->timestamp) {
        /**
         * Unable to draw windows fast enough, previous event is too
         * close or windows are too big.
         * Nothing can be done => Discard event.
         */
        // TODO: Discard event
        LIBBLU_HDMV_PGS_ASS_ERROR_RETURN(
          "Frame at %" PRId64 " is too complex and cannot be drawn fast enough, "
          "interval with previous frame is insufficient.\n",
          frame->timestamp
        );
      }

      if (prev_frame->timestamp + frame->decode_duration > frame->timestamp) {
        /**
         * Sequences are too close, not enough time to clear Graphical Plane,
         * decode objects and draw current (first) frame.
         * Shall attempt to merge sequences.
         */
        // TODO: Check sequences merging, and do it if possible
        LIBBLU_HDMV_PGS_ASS_ERROR_RETURN(
          "Frame at %" PRId64 ", first of a new sequence, is too complex and "
          "cannot be drawn fast enough, interval with previous sequence is "
          "insufficient.\n",
          frame->timestamp
        );
      }
    }
  }
  else {
    /* Check minimal drawing time */
    if (prev_frame->timestamp + seq->min_drawing_duration > frame->timestamp) {
      /**
       * Unable to draw windows fast enough, previous event is too
       * close or windows are too big.
       * Nothing can be done => Discard event.
       */
      // TODO: Discard event
      LIBBLU_HDMV_PGS_ASS_ERROR_RETURN(
        "Frame at %" PRId64 " is too complex and cannot be drawn fast enough, "
        "interval with previous frame is insufficient.\n",
        frame->timestamp
      );
    }

    if (prev_frame->timestamp + frame->decode_duration > frame->timestamp) {
      /**
       * Unable to decode objects and draw fast enough, shall attempt to
       * pre-decode objects in previous frames.
       */
      // TODO: Optimize objects pre-decoding to avoid
      int64_t time_debt = prev_frame->timestamp + frame->decode_duration - frame->timestamp;
      LIBBLU_HDMV_PGS_ASS_ERROR_RETURN(
        "Frame at %" PRId64 " is too complex and cannot be drawn fast enough, "
        "interval with previous frame is insufficient (%" PRId64 " ticks).\n",
        frame->timestamp,
        time_debt
      );
    }
  }

  return 0;
}

static int _processCompletePgsFrameSequence(
  PgsFrameSequence ** seq_ptr,
  HdmvVDParameters video_desc
)
{
  PgsFrameSequence * cur_seq = *seq_ptr;

  LIBBLU_HDMV_PGS_ASS_DEBUG("Processing frame sequence.\n");

  if (NULL == cur_seq) {
    LIBBLU_HDMV_PGS_ASS_DEBUG(" Empty sequence.\n");
    return 0; // No sequence currently builded
  }
  assert(NULL != cur_seq->first_frm && NULL != cur_seq->last_frm);

  /* Compute windows for the sequence */
  if (_computeWindowsPgsFrameSequence(cur_seq) < 0)
    return -1;

  /* Check windows drawing duration against previous Epoch final time */
  if (!_checkDrawingDurationEpochStart(cur_seq)) {
    LIBBLU_HDMV_PGS_ASS_ERROR(
      "Frame sequence at %" PRId64 " windows minimal drawing duration overlap "
      "with previous sequence last event at %" PRId64 " 27MHz clock ticks, "
      "interval is too short.\n",
      cur_seq->first_frm->timestamp,
      (NULL != cur_seq->prev_seq) ? cur_seq->prev_seq->last_frm->timestamp : -1
    );

    // TODO: Try merging with previous sequence
    _abortWholeSequence(seq_ptr);
    return 0;
  }

  HdmvQuantHexTreeNodesInventory qht_inv = {0};
  HdmvBitmapList bm_list = {0};

  LIBBLU_HDMV_PGS_ASS_DEBUG("Building sequence graphical objects:\n");

  cur_seq->dec_obj_buffer_usage = 0u;
  for (
    PgsFrame * frame = cur_seq->first_frm;
    NULL != frame;
    frame = frame->next
  ) {
    LIBBLU_HDMV_PGS_ASS_DEBUG(
      " Processing frame (%" PRId64 "):\n",
      frame->timestamp
    );

    /* Init composition objects and collect bitmaps */
    if (frame->nb_bitmaps)
      LIBBLU_HDMV_PGS_ASS_DEBUG("  Initialize naive composition objects:\n");
    for (unsigned i = 0; i < frame->nb_bitmaps; i++) {
      _initCompositionObject(&frame->compo_obj[i], frame->bitmaps_pos[i]);
      if (addHdmvBitmapList(&bm_list, frame->bitmaps[i]) < 0)
        return -1;
    }
    frame->nb_compo_obj = frame->nb_bitmaps;

    /* Build palette */
    LIBBLU_HDMV_PGS_ASS_DEBUG("  Build naive palette:\n");
    HdmvPalette pal;
    const HdmvPaletteColorMatrix color_matrix = HDMV_PAL_CM_BT_601;
    initHdmvPalette(&pal, 0x00, color_matrix);

    if (0 < frame->nb_bitmaps) {
      if (buildPaletteListHdmvPalette(&pal, &bm_list, 255u, &qht_inv) < 0)
        return -1;
    }

    LIBBLU_HDMV_PGS_ASS_DEBUG(
      "   Generated palette: %u entrie(s).\n",
      pal.nb_entries
    );
    flushHdmvBitmapList(&bm_list);

    /* Build objects */
    if (frame->nb_bitmaps)
      LIBBLU_HDMV_PGS_ASS_DEBUG("  Build object(s):\n");
    const HdmvColorDitheringMethod color_dm = HDMV_PIC_CDM_DISABLED;
    for (unsigned i = 0; i < frame->nb_bitmaps; i++) {
      HdmvBitmap bitmap = frame->bitmaps[i];

      HdmvPalletizedBitmap pal_bm;
      if (getPalletizedHdmvBitmap(&pal_bm, bitmap, &pal, color_dm) < 0)
        return -1;

      /* Create a new object */
      HdmvObject obj;
      if (initHdmvObject(&obj, pal_bm) < 0)
        return -1;
      uint16_t obj_id_ref;
      if (_addNewObjectPgsFrameSequence(cur_seq, frame, obj, &obj_id_ref) < 0)
        return -1;
      frame->compo_obj[i].obj_id_ref  = obj_id_ref; // Set obj in co
      frame->obj_ids[frame->nb_obj++] = obj_id_ref; // Set obj as present in future DS
    }

    LIBBLU_HDMV_PGS_ASS_DEBUG("  Compute and check timings:\n");

    /* Compute decoding time */
    _computeAndSetCompositionDecodingDurations(cur_seq, frame, video_desc);

    /* Check decoding time against previous frames / sequences */
    /* Update previous frames decoding time if required */
    if (_checkDecodeTimePgsFrame(cur_seq, frame) < 0)
      return -1;
  }

  cleanHdmvBitmapList(bm_list);
  return 0;
}

#define PGS_ASS_MIN_NB_COMPLETED_FRAMES_THRESHOLD  8

/** \~english
 * \brief Return the oldest complete sequence.
 *
 * \param cur_seq Current (just processed) sequence.
 * \param nb_complete_seq_ret Pointer for the number of complete sequences
 * returned value.
 * \return PgsFrameSequence* Oldest complete sequence (or NULL if none).
 *
 * There are four possible types of sequences: Future sequences not yet
 * processed, pending sequences that might be updated by the next processed
 * sequence, completed sequences that could not be updated anymore and shall
 * be generated for output, and finally already generated sequences.
 *
 * This function counts the number of pending frames in previous sequences i.e.
 * frames with initial decoding time that might be brought forward by the next
 * sequence generation optimizations.
 *
 * If the frame count exceeds the threshold, it counts the number of sequences
 * before the sequence containing oldest pending frame and return the oldest
 * completed sequence (that could ne generated now). Return NULL if no sequence
 * is complete yet.
 */
static PgsFrameSequence * _getOldestCompleteSequence(
  PgsFrameSequence * cur_seq,
  unsigned * nb_complete_seq_ret
)
{
  lb_static_assert(0 < PGS_ASS_MIN_NB_COMPLETED_FRAMES_THRESHOLD);
  assert(NULL != nb_complete_seq_ret);

  PgsFrameSequence * oldest_complete_seq = NULL;
  unsigned nb_pending_frames = 0;
  unsigned nb_complete_sequences = 0;

  for (
    PgsFrameSequence * seq = cur_seq;
    NULL != seq;
    seq = seq->prev_seq
  ) {
    if (nb_pending_frames < PGS_ASS_MIN_NB_COMPLETED_FRAMES_THRESHOLD)
      nb_pending_frames += seq->nb_frames;
    else {
      oldest_complete_seq = seq;
      nb_complete_sequences++;
    }
  }

  *nb_complete_seq_ret = nb_complete_sequences;
  return oldest_complete_seq;
}

static int _generateOutputPgsFrameSequence(
  PgsFrameSequence ** seq_ptr
)
{
  PgsFrameSequence * cur_seq = *seq_ptr;

  LIBBLU_HDMV_PGS_ASS_DEBUG("Generating completed frame sequences.\n");

  unsigned nb_complete_seq;
  PgsFrameSequence * seq = _getOldestCompleteSequence(
    cur_seq,
    &nb_complete_seq
  );

  LIBBLU_HDMV_PGS_ASS_DEBUG(" Completed sequence(s): %u.\n", nb_complete_seq);

  for (unsigned i = 0; i < nb_complete_seq; i++, seq = seq->next_seq) {
    assert(cur_seq != seq); // Unexpect reaching last processed sequence.
    assert(NULL != seq);




    // TODO
  }

  if (0 < nb_complete_seq)
    LIBBLU_TODO("Done!\n");

  return 0;
}

int processPgsGenerator(
  char * ass_filepath
)
{
  HdmvVDParameters video_desc = {
    .video_width  = 1920,
    .video_height = 1080,
    .frame_rate   = FRAME_RATE_CODE_23976
  };
  int frame_w = video_desc.video_width;
  int frame_h = video_desc.video_height;

  _print_font_providers(ass_library);

  init(1920, 1080);
  ASS_Track * track = ass_read_file(ass_library, ass_filepath, NULL);
  if (!track) {
    printf("track init failed!\n");
    return 1;
  }

  int64_t timestamp = 0;
  int64_t frame_duration = frameDur27MHzHdmvFrameRateCode(
    video_desc.frame_rate
  );

  unsigned frame_nb = 0;
  // unsigned blank_period = 0;
  // unsigned epoch_size = 0;
  // unsigned errors = 0;

  // RenderingWindows rendering_windows = (RenderingWindows) {0};

  PgsFrameSequence * first_frame_seq = NULL;
  PgsFrameSequence * cur_frame_seq = NULL;

  bool empty_screen = true;

  /* Check all events to find last event end timestamp */
  long long last_ts = 0;
  for (int i = 0; i < track->n_events; i++) {
    long long end_ts = track->events[i].Start + track->events[i].Duration;
    last_ts = MAX(last_ts, end_ts);
  }
  int64_t end_timestamp = last_ts * 27000ll;
  printf("End timestamp: %" PRId64 ".\n", end_timestamp);

  HdmvBitmap rframe;
  if (initHdmvBitmap(&rframe, frame_w, frame_h) < 0)
    return -1;

  for (; timestamp < end_timestamp; timestamp += frame_duration, frame_nb++) {
    int change;
    long long now = timestamp / 27000ll;
    ASS_Image * img = ass_render_frame(ass_renderer, track, now, &change);

    if (NULL == img) {
      if (!empty_screen) {
        /* First empty frame */
        printf("%u/%" PRId64 ") Empty frame (NULL).\n", frame_nb, timestamp);

        if (NULL == newFramePgsFrameSequence(cur_frame_seq, timestamp))
          return -1;

#if 0
        clearHdmvBitmap(&rframe);
        char img_filename[1024];
        snprintf(
          img_filename, 1024, "%s/%" PRId64 "_frame_%u_empty.png",
          outfolder, now, frame_nb
        );

        _write_png(img_filename, &rframe);
#endif
      }

      empty_screen = true;
    }
    else {
      if (empty_screen) {
        /* Nothing on screen previously, start of a new frame sequence */
        if (_processCompletePgsFrameSequence(&cur_frame_seq, video_desc) < 0)
          return -1;

        if (NULL == first_frame_seq)
          first_frame_seq = cur_frame_seq;

        if (_generateOutputPgsFrameSequence(&cur_frame_seq) < 0)
          return -1;

        if (startPgsFrameSequence(&cur_frame_seq) < 0)
          return -1;

        assert(2 == change);
      }

      if (0 != change) {
        const char * state_str = (change == 1) ? "position_change" : "content_change";

        PgsFrame * frame = newFramePgsFrameSequence(cur_frame_seq, timestamp);
        if (NULL == frame)
          return -1;

        clearHdmvBitmap(&rframe);

        unsigned nb_obj = 0;
        MergingTreeNode * bitmaps_mt = NULL;
        for (; NULL != img; img = img->next, nb_obj++) {
          // clearHdmvBitmap(&rframe);
          _blendASSImageToHdmvBitmap(&rframe, img);

          HdmvRectangle box = _assImageRectangle(img, frame_w, frame_h);
          if (!isEmptyRectangle(box)) {
            if (insertMergingTreeNode(&bitmaps_mt, box) < 0)
              return -1;
          }
        }

        printf(
          "%u/%" PRId64 ") %s, %u objects (change == %d).\n",
          frame_nb, timestamp, state_str, nb_obj, change
        );

        if (_peekBitmapsPgsFrame(frame, &rframe, bitmaps_mt) < 0)
          return -1;
        _drawSurroundingBoxesObjectsPgsFrame(&rframe, frame, 0xFF0000FF);
        destroyMergingTreeNode(bitmaps_mt);

#if 0
        char img_filename[1024];
        snprintf(
          img_filename, 1024, "%s/%" PRId64 "_frame_%u_%s.png",
          outfolder, now, frame_nb, state_str
        );

        _write_png(img_filename, &rframe);
#endif
      }

      empty_screen = false;
    }

#if 0
    if (NULL == img) {
      blank_period++;
      continue;
    }

    if (2 <= blank_period && 0 < epoch_size) {
      printf("END of an epoch of %u events.\n", epoch_size);
      resetRenderingWindows(&rendering_windows);
      epoch_size = 0;
    }

    if (0 != change) {
      blank_period = 0;
      epoch_size++;

      printf(
        "Frame %u - Timestamp %" PRId64 " ms (%" PRId64 " ticks).\n",
        frame_nb, now, timestamp
      );

      printf(" Content change: %s (%d).\n", _change_str(change), change);

      if (NULL != img) {
        image_t * frame = _gen_image(frame_w, frame_h);

        int nb_objects = 0;
        RenderingWindows objects = (RenderingWindows) {0};
        for (; NULL != img; img = img->next) {
          _blend_single(frame, img);
          HdmvRectangle img_dim = _assImageRectangle(img, frame_w, frame_h);
          // printf("Adding x=%u y=%u w=%u h=%u %d %08X\n", img_dim.x, img_dim.y, img_dim.w, img_dim.h, img->type, img->color);
          _drawSurroundingBoxDimensions(frame, img_dim, 0x00FF00FF);
          addRenderingWindows(&rendering_windows, img_dim);
          addRenderingWindows(&objects, img_dim);
          nb_objects++;
        }
        printf(" Nb objects: %d.\n", nb_objects);

        printf(" Windows:\n");
        uint32_t size = _drawSurroundingBoxesRenderingWindows(frame, &rendering_windows, 0xFF0000FF);
        printf(" Windows size: %u pixels.\n", size);
        printf(" Objects:\n");
        size = _drawSurroundingBoxesRenderingWindows(frame, &objects, 0x0000FFFF);
        printf(" Objects size: %u pixels.\n", size);

        uint64_t time = size * 9u / 3200u;
        printf(" Drawing time: %u ticks.\n", time);
        if (3753 <= time) {
          printf(" ### TOO BIG Frame %u ###\n", frame_nb);
          errors++;
        }

#if 1
        char img_filename[1024];
        snprintf(
          img_filename, 1024, "%s/%" PRId64 "_frame_%u.png",
          outfolder, now, frame_nb
        );

        printf(" Output file: %s.\n", img_filename);

        _write_png(img_filename, frame);
#endif

        free(frame->buffer);
        free(frame);
      }
      else
        blank_period++;

#if 0
      int64_t next_event_shift = ass_step_sub(track, timestamp, 1);

      printf(" Next event in: %" PRId64 " ms.\n", next_event_shift);

      if (59425 < 90 * next_event_shift) {
        if (0 < epoch_size) {
          printf("END of an epoch of %zu event(s).\n", epoch_size);
        }
        epoch_size = 0;
      }
      epoch_size++;
#endif

    }
    else
      blank_period++;
#endif
  }

  // printf("%u error(s).\n", errors);

  cleanHdmvBitmap(rframe);

#if 0
  int64_t timeshift;
  size_t epoch_size, ds_size;
  int event_id = 0;

  epoch_size = ds_size = 0;
  while (0 < (timeshift = ass_step_sub(track, timestamp, 1))) {
    timestamp += timeshift;

    printf("Event %d:\n", event_id++);
    printf(
      " Timestamp: %lld ms (+%lld ms, %u ticks).\n",
      timestamp, timeshift, timeshift * 90
    );

    unsigned eventFrameNb = timestamp * 360 % frame_duration;
    if (eventFrameNb != frameNb) {
      if (0 < ds_size) {
        printf("END of a Display Set of %zu event(s).\n", ds_size);
      }
      frameNb = eventFrameNb;
      ds_size = 0;
    }
    ds_size++;

    if (59425 < timeshift * 90) {
      if (0 < epoch_size) {
        printf("END of an epoch of %zu event(s).\n", epoch_size);
      }
      epoch_size = 0;
    }
    epoch_size++;

    int change;
    ASS_Image * img = ass_render_frame(ass_renderer, track, timestamp, &change);

    printf(" Content change: %s (%d).\n", _change_str(change), change);

    image_t * frame = _gen_image(frame_w, frame_h);

    int nb_objects = 0;
    for (; NULL != img; img = img->next) {
      _blend_single(frame, img);
      _draw_surrounding_box(frame, img);
      nb_objects++;
    }
    printf(" Nb objects: %d.\n", nb_objects);

    char img_filename[1024];
    snprintf(
      img_filename, 1024, "%lld_frame_%d.png",
      timestamp, event_id-1
    );

    printf(" Output file: %s.\n", img_filename);

    // _write_png(img_filename, frame);
    free(frame->buffer);
    free(frame);
  }
#endif

  // image_t * frame = gen_image(frame_w, frame_h);
  // blend(frame, img);

  ass_free_track(track);
  ass_renderer_done(ass_renderer);
  ass_library_done(ass_library);

  // write_png(imgfile, frame);
  // free(frame->buffer);
  // free(frame);

  return -1;
  return 0;
}
