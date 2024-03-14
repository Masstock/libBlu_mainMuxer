#include <stdio.h>
#include <stdlib.h>

#include "hdmv_object.h"

static uint32_t _maxRleSize(
  uint16_t width,
  uint16_t height
)
{
  // Double width
  return ((width + 1u) << 1) * height;
}

static int _allocRle(
  HdmvObject *obj
)
{
  assert(0 < obj->pal_bitmap.width);
  assert(0 < obj->pal_bitmap.height);

  uint32_t max_rle_size = _maxRleSize(
    obj->pal_bitmap.width,
    obj->pal_bitmap.height
  );

  uint8_t *rle = malloc(max_rle_size);
  if (NULL == rle)
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
  obj->rle = rle;

  return 0;
}

int initFromPalletizedHdmvObject(
  HdmvObject *dst,
  HdmvPalletizedBitmap pal_bitmap
)
{
  assert(0 < pal_bitmap.width);
  assert(0 < pal_bitmap.height);

  HdmvObject obj = {
    .pal_bitmap = pal_bitmap
  };

  if (_allocRle(&obj) < 0)
    return -1;

  *dst = obj;
  return 0;
}

static int _copyRle(
  HdmvObject *dst,
  const uint8_t *src_rle,
  uint32_t src_rle_size
)
{
  size_t size = src_rle_size + 1u; // Extra room for error-free optimized decompression
  uint8_t *rle = calloc(1u, size);
  if (NULL == rle)
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
  memcpy(rle, src_rle, src_rle_size);

  dst->rle      = rle;
  dst->rle_size = src_rle_size;
  return 0;
}

int initHdmvObject(
  HdmvObject *dst,
  const uint8_t *rle,
  uint32_t rle_size,
  uint16_t width,
  uint16_t height
)
{
  assert(NULL != dst);
  assert(NULL != rle);
  assert(0 < rle_size);
  assert(0 < width);
  assert(0 < height);

  HdmvObject obj = {
    .pal_bitmap = {
      .width  = width,
      .height = height
    }
  };

  if (_copyRle(&obj, rle, rle_size) < 0)
    return -1;

  *dst = obj;
  return 0;
}

int compressRleHdmvObject(
  HdmvObject *obj,
  unsigned *longuest_compressed_line_size_ret,
  uint16_t *longuest_compressed_line_ret
)
{
  uint16_t height = obj->pal_bitmap.height;
  uint16_t stride = obj->pal_bitmap.width;
  uint8_t *src   = obj->pal_bitmap.data;
  uint8_t *dst   = obj->rle;

  unsigned max_compr_line_size = 0u;
  uint16_t max_compr_line = 0x0;

  for (uint16_t line_i = 0; line_i < height; line_i++) {
    uint8_t *eol = &src[stride];
    uint16_t run_len = 0u;
    uint8_t run_px = 0x00;
    unsigned off = 0u;

    for (;;) {
      if (!run_len && src != eol) {
        /* Initialize a new run */
        run_px = *(src++);
        run_len++;
      }

      if (
        src == eol // End of line reached
        || *src != run_px // End of current run
        || run_len == 16383 // Run is longer than max L
      ) {
        /* End of current run */

        if (run_px == 0x00) {
          /* 0b00000000 : Color 0 */
          WB_ARRAY(dst, off, 0x00);

          if (run_len <= 63) {
            /* 0b00LLLLLL */
            WB_ARRAY(dst, off, run_len & 0x3F);
          }
          else {
            /* 0b01LLLLLL 0bLLLLLLLL */
            WB_ARRAY(dst, off, ((run_len >> 8) & 0x3F) | 0x40);
            WB_ARRAY(dst, off,   run_len);
          }
        }
        else {
          if (run_len <= 3) {
            /* 0bCCCCCCCC */
            for (unsigned i = 0; i < run_len; i++)
              WB_ARRAY(dst, off, run_px);
          }
          else if (run_len <= 63) {
            /* 0b00000000 0b10LLLLLL 0bCCCCCCCC */
            WB_ARRAY(dst, off, 0x00);
            WB_ARRAY(dst, off, (run_len & 0x3F) | 0x80);
            WB_ARRAY(dst, off,  run_px);
          }
          else {
            /* 0b00000000 0b11LLLLLL 0bLLLLLLLL 0bCCCCCCCC */
            WB_ARRAY(dst, off, 0x00);
            WB_ARRAY(dst, off, ((run_len >> 8) & 0x3F) | 0xC0);
            WB_ARRAY(dst, off,   run_len);
            WB_ARRAY(dst, off,   run_px);
          }
        }

        run_len = 0u;
        if (src == eol)
          break; // End of line
        continue;
      }
      else {
        /* Continue current run */
        src++;
        run_len++;
      }
    }

    unsigned line_size = off;
    if (max_compr_line_size < line_size) {
      max_compr_line_size = line_size;
      max_compr_line      = line_i;
    }

    dst += off;

    /* End of line */
    /* 0b00000000 0b00000000 */
    *(dst++) = 0x00;
    *(dst++) = 0x00;
  }

  if (NULL != longuest_compressed_line_size_ret)
    *longuest_compressed_line_size_ret = max_compr_line_size;
  if (NULL != longuest_compressed_line_ret)
    *longuest_compressed_line_ret = max_compr_line;

  obj->rle_size = dst - obj->rle;
  return 0;
}

int decompressRleHdmvObject(
  HdmvObject *obj,
  unsigned *longuest_compressed_line_size_ret,
  uint16_t *longuest_compressed_line_ret
)
{
  assert(0 < obj->rle_size);

  if (NULL != obj->pal_bitmap.data)
    LIBBLU_HDMV_COM_ERROR_RETURN("Palettized bitmap data is not empty!\n");

  unsigned nb_lines   = obj->pal_bitmap.height;
  unsigned line_width = obj->pal_bitmap.width;

  size_t bitmap_size = (nb_lines *line_width);
  uint8_t *dst = malloc(bitmap_size);
  if (NULL == dst)
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
  obj->pal_bitmap.data = dst;

  const uint8_t *src     = obj->rle;
  const uint8_t *src_end = &obj->rle[obj->rle_size];
  uint8_t       * dst_end = &dst[bitmap_size];

  unsigned max_compr_line_size = 0u;
  uint16_t max_compr_line = 0x0;

  const uint8_t *src_line_start = src;
  const uint8_t *dst_line_end   = &dst[line_width];
  unsigned cur_nb_lines = 0;

  while (src < src_end && dst < dst_end) {
    uint8_t color_code = *src++;

    if (0x00 != color_code) {
      /* Single pixel color code */
      *dst++ = color_code;
      continue;
    }

    /* Extended code */
    uint8_t flags = *src++; // This can be done without check as an extra
                            // 0-byte is appended to RLE source data

    if (0x00 == flags) { // EOL (or out of RLE data)
      if (dst != dst_line_end) // Line is shorter/longer than expected
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Bitmap RLE data is invalid, invalid line length (%u, got %u).\n",
          line_width,
          (dst > dst_line_end)
          ? line_width + (dst - dst_line_end)
          : line_width - (dst_line_end - dst)
        );

      unsigned line_size = src - src_line_start;
      if (max_compr_line_size < line_size) {
        max_compr_line_size = line_size;
        max_compr_line      = cur_nb_lines;
      }

      src_line_start = src;
      dst_line_end   = &dst[line_width]; // Update next end of line pointer
      cur_nb_lines++;
      continue;
    }

    unsigned run = flags & 0x3F;
    if (flags & 0x40) // long run switch (63 < run)
      run = (run << 8) | *src++;
    if (flags & 0x80) // Non-zero color switch
      color_code = *src++;

    if (run < (dst_end - dst))
      memset(dst, color_code, run);
    dst += run;
  }

  /* Source RLE final EOL */
  if (src+2u != src_end) // Out of RLE
    LIBBLU_HDMV_COM_ERROR_RETURN("Bitmap RLE data is invalid, unexpected end.\n");
  if (src[0] != 0x00 || src[1] != 0x00) // Final EOL missing
    LIBBLU_HDMV_COM_ERROR_RETURN("Bitmap RLE data is invalid, missing final EOL.\n");
  cur_nb_lines++;

  /* Destination bitmap */
  if (dst != &obj->pal_bitmap.data[bitmap_size])
    LIBBLU_HDMV_COM_ERROR_RETURN("Bitmap RLE data is invalid, wrong bitmap size.\n");
  if (cur_nb_lines != nb_lines)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Bitmap RLE data is invalid, wrong number of lines (got %u, expect %u).\n",
      cur_nb_lines, nb_lines
    );

  if (NULL != longuest_compressed_line_size_ret)
    *longuest_compressed_line_size_ret = max_compr_line_size;
  if (NULL != longuest_compressed_line_ret)
    *longuest_compressed_line_ret = max_compr_line;

  return 0;
}