#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "dts_patcher_util.h"

DtsPatcherBitstreamHandlePtr createDtsPatcherBitstreamHandle(
  void
)
{
  DtsPatcherBitstreamHandlePtr handle;

  handle = (DtsPatcherBitstreamHandlePtr) calloc(
    1,
    sizeof(DtsPatcherBitstreamHandle)
  );
  if (NULL == handle)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  return handle;
}

void destroyDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle
)
{
  if (NULL == handle)
    return;

  free(handle->data);
  free(handle);
}

static int growDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle
)
{
  size_t newLength;
  uint8_t * newArray;

  newLength = GROW_ALLOCATION(
    handle->dataAllocatedLength,
    DTS_PATCHER_BITSTREAM_HANDLE_DEFAULT_SIZE
  );

  if (newLength <= handle->dataAllocatedLength)
    LIBBLU_DTS_ERROR_RETURN("Too many bytes in bitstream patcher.\n");

  newArray = (uint8_t *) realloc(handle->data, newLength);
  if (NULL == newArray)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  handle->data = newArray;
  handle->dataAllocatedLength = newLength;

  return 0;
}

int getGeneratedArrayDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  const uint8_t ** array,
  uint16_t * arr_size
)
{

  if (NULL != array)
    *array = handle->data;
  if (NULL != arr_size)
    *arr_size = handle->dataUsedLength;

  return 0;
}

void initCrcDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  CrcParam param,
  uint32_t initialValue
)
{
  initCrcContext(&handle->crc, param, initialValue);
}

uint16_t finalizeCrcDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle
)
{
  return completeCrcContext(&handle->crc);
}

int writeByteDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  uint8_t byte
)
{
  assert(!handle->currentByteUsedBits);

  if (handle->dataAllocatedLength <= handle->dataUsedLength) {
    if (growDtsPatcherBitstreamHandle(handle) < 0)
      return -1;
  }

  handle->data[handle->dataUsedLength++] = byte;

  if (handle->crc.in_use) {
    /* \warning Will include bits before activation */
    applySingleByteCrcContext(&handle->crc, byte);
  }

  return 0;
}

int writeBytesDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  const uint8_t * bytes,
  size_t size
)
{
  size_t i;

  for (i = 0; i < size; i++) {
    if (writeByteDtsPatcherBitstreamHandle(handle, bytes[i]) < 0)
      return -1;
  }

  return 0;
}

bool isByteAlignedDtsPatcherBitstreamHandle(
  const DtsPatcherBitstreamHandlePtr handle
)
{
  return !handle->currentByteUsedBits;
}

int byteAlignDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle
)
{
  return writeBitsDtsPatcherBitstreamHandle(
    handle, 0x0, (8 - handle->currentByteUsedBits) & 0x7
  );
}

void getBlockBinaryBoundaryOffDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  size_t * blockStartBitOffset
)
{
  *blockStartBitOffset = handle->currentByteUsedBits;
}

int blockByteAlignDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  size_t blockStartBitOffset
)
{
  return writeBitsDtsPatcherBitstreamHandle(
    handle,
    0x0,
    ((8 - handle->currentByteUsedBits) + blockStartBitOffset) & 0x7
  );
}

static int writeBitDtsPatcherBitstreamHandle_sub(
  DtsPatcherBitstreamHandlePtr handle,
  bool bit
)
{
  assert(handle->currentByteUsedBits < 8);

  handle->currentByte |= ((uint8_t) bit << (7 - handle->currentByteUsedBits));
  handle->currentByteUsedBits++;

  if (handle->currentByteUsedBits == 8) {
    handle->currentByteUsedBits = 0;
    if (writeByteDtsPatcherBitstreamHandle(handle, handle->currentByte) < 0)
      return -1;
    handle->currentByte = 0x00;
  }

  return 0;
}

int writeBitDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  bool bit
)
{
  /* lbc_printf("1 bit: 0b%x\n", bit); */

  return writeBitDtsPatcherBitstreamHandle_sub(
    handle, bit
  );
}

int writeBitsDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  uint32_t bits,
  size_t size
)
{
  size_t i;
  bool bit;

  assert(size <= 32);

  for (i = 0; i < size; i++) {
    bit = (bits >> (size - i - 1)) & 0x1;
    if (writeBitDtsPatcherBitstreamHandle_sub(handle, bit) < 0)
      return -1;
  }

  return 0;
}

int writeValueDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  uint32_t value,
  size_t size
)
{
  if (size < lb_fast_log2_32(value))
    LIBBLU_DTS_ERROR_RETURN(
      "Value 0x%" PRIX32 " exceed field size of %zu bits.\n"
    );

  return writeBitsDtsPatcherBitstreamHandle(handle, value, size);
}