/** \~english
 * \file bitStreamHandling.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Bitstreams reading/writing handling module.
 */

#ifndef __LIBBLU_MUXER__UTIL__BIT_STREAM_HANDLING_H__
#define __LIBBLU_MUXER__UTIL__BIT_STREAM_HANDLING_H__

#include "common.h"
#include "crcLookupTables.h"
#include "errorCodes.h"
#include "macros.h"

#include <stdio.h>

// #define DISABLE_WRITE_BUFFER
// #define DISABLE_READ_BUFFER

#define IO_VBUF_SIZE 1048576

typedef struct {
  const uint32_t * table;
  bool shifted;
  unsigned length;
  uint32_t poly;
} CrcParam;

typedef struct {
  bool in_use;     /**< Set current usage of CRC computation.            */
  uint32_t buf;    /**< CRC computation buffer.                          */
  CrcParam param;  /**< Attached CRC generation parameters.              */
} CrcContext;

static inline void resetCrcContext(
  CrcContext * ctx
)
{
  *ctx = (CrcContext) {
    0
  };
}

static inline void initCrcContext(
  CrcContext * ctx,
  CrcParam param,
  uint32_t initial_value
)
{
  assert(NULL != param.table || 0x0 != param.poly);
  assert(0 < param.length);

  *ctx = (CrcContext) {
    .in_use = true,
    .buf = initial_value,
    .param = param
  };
}

static inline void setUseCrcContext(
  CrcContext * ctx,
  bool use
)
{
  ctx->in_use = use;
}

void applyNoTableShiftedCrcContext(
  CrcContext * ctx,
  uint8_t byte
);

void applyNoTableCrcContext(
  CrcContext * ctx,
  uint8_t byte
);

static inline void applySingleByteTableCrcContext(
  CrcContext * ctx,
  const uint32_t * table,
  uint8_t byte
)
{
  unsigned length = ctx->param.length;
  uint32_t buf    = ctx->buf;

  if (ctx->param.shifted)
    ctx->buf = ((buf << 8) ^ byte) ^ table[(buf >> (length - 8)) & 0xFF];
  else
    ctx->buf = (buf >> 8) ^ table[(buf & 0xFF) ^ byte];
}

static inline void applySingleByteCrcContext(
  CrcContext * ctx,
  uint8_t byte
)
{
  if (!ctx->in_use)
    return;

  if (NULL != ctx->param.table) {
    /* Applying CRC look-up table */
    applySingleByteTableCrcContext(ctx, ctx->param.table, byte);
  }
  else {
    if (ctx->param.shifted)
      applyNoTableShiftedCrcContext(ctx, byte);
    else
      applyNoTableCrcContext(ctx, byte);
  }
}

static inline void applyTableCrcContext(
  CrcContext * ctx,
  const uint8_t * array,
  unsigned size
)
{
  const uint32_t * table = ctx->param.table;

  assert(NULL != table);

  if (!ctx->in_use)
    return;

  for (unsigned i = 0; i < size; i++) {
    applySingleByteTableCrcContext(ctx, table, array[i]);
  }
}

static inline void applyCrcContext(
  CrcContext * ctx,
  const uint8_t * array,
  unsigned size
)
{
  if (!ctx->in_use)
    return;

  for (unsigned i = 0; i < size; i++) {
    applySingleByteCrcContext(ctx, array[i]);
  }
}

static inline uint32_t completeCrcContext(
  CrcContext * ctx
)
{
  assert(ctx->in_use);

  ctx->in_use = false;
  return (ctx->buf & ((1ull << ctx->param.length) - 1));
}

/** \~english
 * \brief Bitstreams handling structure used for reading and writing.
 *
 * High-level structure used to handle binary files, providing a secondary
 * buffer to increase performances, allow bit-per-bit operations and compute
 * CRC.
 */
typedef struct {
  uint8_t * byteArray;      /**< Reading byte-array high-level buffer.       */
  size_t byteArraySize;     /**< Current size of byte-array buffer in bytes. */
  size_t byteArrayOff;      /**< Reading offset in byte-array buffer.        */
  unsigned bitCount;        /**< Remaining unreaded bits in current pointed
    byte-array offset.                                                       */

  CrcContext crcCtx;

  FILE * file;    /**< Linked FILE * pointer.                                */
  char * buffer;  /**< Low level IO buffer.                                  */

  int64_t fileSize;     /**< File length in bytes.                           */
  int64_t fileOffset;   /**< Current file position offset in bytes.          */
  size_t bufferLength;  /**< Initial byteArray buffer length in bytes.       */
  uint64_t identifier;  /**< Bytestream randomized identifier.               */
} BitstreamHandler, *BitstreamWriterPtr, *BitstreamReaderPtr;

static inline CrcContext * crcCtxBitstream(
  BitstreamHandler * bs
)
{
  return &bs->crcCtx;
}

/** \~english
 * \brief Creates a bitstream reading handling structure on supplied file.
 *
 * \param inputFilename Bitstream linked file name.
 * \param bufferSize Bitstream buffering size (at least 32) in bytes.
 * \return BitstreamReaderPtr On success, created object is returned.
 * Otherwise, a NULL pointer is returned.
 *
 * Created reader must be passed to #closeBitstreamReader() after use.
 */
BitstreamReaderPtr createBitstreamReader(
  const lbc * inputFilename,
  const size_t bufferSize
);

/** \~english
 * \brief Creates a bitstream reading handling structure on supplied file
 * with the default reading buffering size.
 *
 * \param inputFilename Bitstream linked file name.
 * \return BitstreamReaderPtr On success, created object is returned.
 * Otherwise, a NULL pointer is returned.
 *
 * Created reader must be passed to #closeBitstreamReader() after use.
 * To set a custom buffer size, use #createBitstreamReader() instead.
 */
static inline BitstreamReaderPtr createBitstreamReaderDefBuf(
  const lbc * inputFilename
)
{
  return createBitstreamReader(
    inputFilename,
    READ_BUFFER_LEN
  );
}

/** \~english
 * \brief Destroy object and close bitstream reader attached file.
 *
 * \param bitStream Reading bitstream object to free.
 */
void closeBitstreamReader(
  BitstreamReaderPtr bitStream
);

/** \~english
 * \brief Creates a bitstream writing handling structure on supplied file.
 *
 * \param outputFilename Bitstream output filename.
 * \param bufferSize Bitstream writing buffering size (at least 32) in bytes.
 * \return BitstreamWriterPtr On success, created object is returned.
 * Otherwise, a NULL pointer is returned.
 *
 * Created reader must be passed to #closeBitstreamWriter() after use.
 */
BitstreamWriterPtr createBitstreamWriter(
  const lbc * outputFilename,
  const size_t bufferSize
);

/** \~english
 * \brief Creates a bitstream writing handling structure on supplied file
 * with the default writing buffering size.
 *
 * \param outputFilename Bitstream output filename.
 * \return BitstreamReaderPtr On success, created object is returned.
 * Otherwise, a NULL pointer is returned.
 *
 * Created reader must be passed to #closeBitstreamWriter() after use.
 * To set a custom buffer size, use #createBitstreamWriter() instead.
 */
static inline BitstreamReaderPtr createBitstreamWriterDefBuf(
  const lbc * outputFilename
)
{
  return createBitstreamWriter(
    outputFilename,
    WRITE_BUFFER_LEN
  );
}

/** \~english
 * \brief Destroy object and close bitstream attached writted file.
 *
 * \param bitStream Writing bitstream object to free.
 */
void closeBitstreamWriter(
  BitstreamWriterPtr bitStream
);

static inline int64_t remainingSize(
  const BitstreamReaderPtr bitStream
)
{
  assert(NULL != bitStream);

  return bitStream->fileSize - bitStream->fileOffset;
}

static inline int isEof(
  const BitstreamReaderPtr bitStream
)
{
  assert(NULL != bitStream);

  return
    bitStream->byteArrayOff == bitStream->byteArraySize
    && bitStream->fileOffset == bitStream->fileSize
  ;
}

static inline int64_t tellPos(
  const BitstreamReaderPtr bs
)
{
  assert(NULL != bs);
  return bs->fileOffset - bs->byteArraySize + bs->byteArrayOff;
}

static inline int64_t tellBinaryPos(
  const BitstreamReaderPtr bitStream
)
{
  assert(NULL != bitStream);

  return
    8 * tellPos(bitStream) +
    (int64_t) ABS(8 - bitStream->bitCount);
}

static inline int64_t tellWritingPos(
  const BitstreamWriterPtr bitStream
)
{
  assert(NULL != bitStream);

  return
    bitStream->fileOffset
    + bitStream->byteArrayOff
  ;
}

static inline int rewindFile(
  BitstreamReaderPtr bitStream
)
{
  assert(NULL != bitStream);

  errno = 0; /* Clear errno */
  rewind(bitStream->file);

  if (0 != errno)
    LIBBLU_ERROR_RETURN(
      "Error happen during file rewind, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  bitStream->fileOffset = 0x0;
  bitStream->byteArrayOff = bitStream->bufferLength;
  bitStream->byteArraySize = bitStream->bufferLength;

  return 0;
}

static inline int seekPos(
  BitstreamReaderPtr bitStream,
  int64_t offset,
  int whence
)
{
  assert(NULL != bitStream);

  if (offset == 0x0 && whence == SEEK_SET)
    return rewindFile(bitStream);

  if (fseek(bitStream->file, offset, whence) < 0) {
    if (ferror(bitStream->file))
      LIBBLU_ERROR_RETURN(
        "Error happen during file position seeking, %s (errno: %d).\n",
        strerror(errno),
        errno
      );

    LIBBLU_ERROR_RETURN(
      "Seeking supplied file position %lld is too large.\n",
      offset
    );
  }

  bitStream->fileOffset = ftell(bitStream->file);
  bitStream->byteArrayOff = bitStream->bufferLength;
  bitStream->byteArraySize = bitStream->bufferLength;

  return 0;
}

int nextBytes(
  BitstreamReaderPtr bitStream,
  uint8_t * data,
  size_t dataLen
);

static inline uint64_t nextUint64(
  BitstreamReaderPtr bitStream
)
{
  uint64_t returnedValue = 0;
  int i = 8, j = 0;
  uint8_t data[8];

  if (nextBytes(bitStream, data, 8) < 0)
    return 0;

  while (i--)
    returnedValue |= ((uint64_t) data[j++] << (i * 8));
  return returnedValue;
}

static inline uint32_t nextUint32(
  BitstreamReaderPtr bitStream
)
{
  uint8_t data[4];

  if (nextBytes(bitStream, data, 4) < 0)
    return 0;

  return
    (  (uint32_t) data[0] << 24)
    | ((uint32_t) data[1] << 16)
    | ((uint32_t) data[2] <<  8)
    | ((uint32_t) data[3]      )
  ;
}

static inline uint32_t nextUint24(
  BitstreamReaderPtr bitStream
)
{
  uint8_t data[3];

  if (nextBytes(bitStream, data, 3) < 0)
    return 0;

  return
    ((uint32_t) data[0] << 16) +
    ((uint32_t) data[1] <<  8) +
    ((uint32_t) data[2]      )
  ;
}

static inline uint32_t nextUint16(
  BitstreamReaderPtr bitStream
)
{
  uint8_t data[2];

  if (nextBytes(bitStream, data, 2) < 0)
    return 0;

  return
    ((uint32_t) data[0] <<  8) +
    ((uint32_t) data[1]      )
  ;
}

static inline uint8_t nextUint8(
  BitstreamReaderPtr bitStream
)
{
  uint8_t data;

  if (nextBytes(bitStream, &data, 1) < 0)
    return 0;

  return data;
}

static inline bool nextBit(
  BitstreamReaderPtr bitStream
)
{
  uint8_t data;

  if (nextBytes(bitStream, &data, 1) < 0)
    return 0;

  return (data >> bitStream->bitCount) & 0x1;
}

int readBit(
  BitstreamReaderPtr bitStream,
  bool * bit
);

int readBits(
  BitstreamReaderPtr bitStream,
  uint32_t * value,
  size_t size
);

static inline int readBits64(
  BitstreamReaderPtr bitStream,
  uint64_t * value,
  size_t length
)
{
  uint32_t smallValue;

  assert(length <= 64);

  smallValue = 0;
  if (32 < length) {
    if (readBits(bitStream, &smallValue, 32) < 0)
      return -1;
    length -= 32;
  }

  if (NULL != value)
    *value = smallValue << length;

  if (readBits(bitStream, &smallValue, length) < 0)
    return -1;

  if (NULL != value)
    *value |= smallValue;

  return 0;
}

static inline int skipBits(
  BitstreamReaderPtr bitStream,
  size_t length
)
{
  bool voidBit;

  while (0 < (length--))
    if (readBit(bitStream, &voidBit) < 0)
      return -1;

  return 0;
}

int readByte(
  BitstreamReaderPtr bitStream,
  uint8_t * data
);

int readBytes(
  BitstreamReaderPtr bs,
  uint8_t * data,
  size_t size
);

int readValueBigEndian(
  BitstreamReaderPtr bitStream,
  const size_t length,
  uint32_t * value
);

int readValue64BigEndian(
  BitstreamReaderPtr bitStream,
  const size_t length,
  uint64_t * value
);

int readValue64LittleEndian(
  BitstreamReaderPtr bitStream,
  size_t length,
  uint64_t * value
);

static inline int skipBytes(
  BitstreamReaderPtr bs,
  size_t size
)
{
  return readBytes(bs, NULL, size);
}

static inline int paddingByte(
  BitstreamReaderPtr bitStream
)
{
  while (bitStream->bitCount != 8)
    if (readBit(bitStream, NULL) < 0)
      return -1;

  return 0;
}

static inline int paddingBoundary(
  BitstreamReaderPtr br,
  unsigned size_in_bytes
)
{
  int64_t offset, alignment;
  int64_t size = size_in_bytes;

  if ((offset = tellPos(br)) < 0)
    LIBBLU_ERROR_RETURN("Unexpected negative file offset.\n");
  alignment = size - (offset % size);

  if (alignment == size)
    return 0; /* Already aligned */
  return skipBytes(br, alignment);
}

int writeByte(
  BitstreamWriterPtr bitStream,
  uint8_t data
);

int writeBytes(
  BitstreamWriterPtr bs,
  const uint8_t * data,
  size_t size
);

static inline int writeUint64(
  BitstreamWriterPtr bitStream,
  uint64_t value
)
{
  int i = 8;
  while (i--) {
    if (writeByte(bitStream, (uint8_t) (value >> (8 * i)) & 0xFF) < 0)
      return -1;
  }

  return 0;
}

static inline int writeUint48(
  BitstreamWriterPtr bitStream,
  uint64_t value
)
{
  int i = 6;
  while (i--) {
    if (writeByte(bitStream, (uint8_t) (value >> (8 * i)) & 0xFF) < 0)
      return -1;
  }

  return 0;
}

static inline int writeUint32(
  BitstreamWriterPtr bitStream,
  uint32_t value
)
{
  int i = 4;
  while (i--) {
    if (writeByte(bitStream, (uint8_t) (value >> (8 * i)) & 0xFF) < 0)
      return -1;
  }

  return 0;
}

static inline int writeUint16(
  BitstreamWriterPtr bitStream,
  uint16_t value
)
{
  int i = 2;
  while (i--) {
    if (writeByte(bitStream, (uint8_t) (value >> (8 * i)) & 0xFF) < 0)
      return -1;
  }

  return 0;
}

static inline int writeReservedBytes(
  BitstreamWriterPtr bitStream,
  const uint8_t reservedByte,
  const size_t dataLen
)
{
  size_t i;

  for (i = 0; i < dataLen; i++)
    if (writeByte(bitStream, reservedByte) < 0)
      return -1;

  return 0;
}

void crcTableGenerator(
  CrcParam param
);

int getFileSize(
  const lbc * filename,
  int64_t * size
);

#endif
