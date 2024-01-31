#include <stdio.h>
#include <stdlib.h>

#include "hdmv_object.h"

// #define DISABLE_RLE_MAX_LINE_SIZE

static uint32_t _maxRleSize(
  uint16_t width,
  uint16_t height
)
{
#if defined(DISABLE_RLE_MAX_LINE_SIZE)
  return ((width + 1u) << 1) * height;
#else
  // Uncompressed line size + 2 bytes (coding overhead)
  // + safety max possible RLE line size
  return (width + 2u) * height + ((width + 1u) << 1);
#endif
}

static int _reallocRleHdmvObject(
  HdmvObject * obj
)
{
  assert(0 < obj->pal_bitmap.width);
  assert(0 < obj->pal_bitmap.height);

  uint32_t max_rle_size = _maxRleSize(
    obj->pal_bitmap.width,
    obj->pal_bitmap.height
  );

  uint8_t * rle = realloc(obj->rle, max_rle_size);
  if (NULL == rle)
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
  obj->rle = rle;

  return 0;
}

int initHdmvObject(
  HdmvObject * dst,
  HdmvPalletizedBitmap pal_bitmap
)
{
  assert(0 < pal_bitmap.width);
  assert(0 < pal_bitmap.height);

  HdmvObject obj = {
    .pal_bitmap = pal_bitmap
  };

  if (_reallocRleHdmvObject(&obj) < 0)
    return -1;

  *dst = obj;
  return 0;
}

bool performRleHdmvObject(
  HdmvObject * obj,
  uint16_t * problematic_line_ret
)
{
  uint16_t height = obj->pal_bitmap.height;
  uint16_t stride = obj->pal_bitmap.width;
  uint8_t * src   = obj->pal_bitmap.data;
  uint8_t * dst   = obj->rle;

  for (uint16_t line_i = 0; line_i < height; line_i++) {
    uint8_t * eol = &src[stride];
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

    if (stride < off + 2u) {
      /* Line len exceed uncompressed size + 2 coding overhead bytes */
      if (NULL != problematic_line_ret)
        *problematic_line_ret = line_i;
      return false;
    }
    dst += off;

    /* End of line */
    /* 0b00000000 0b00000000 */
    *(dst++) = 0x00;
    *(dst++) = 0x00;
  }

  obj->rle_size = dst - obj->rle;
  return true;
}
