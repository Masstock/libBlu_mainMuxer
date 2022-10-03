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

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "crcLookupTables.h"
#include "common.h"
#include "macros.h"
#include "messages.h"
#include "errorCodes.h"

#define IO_VBUF_SIZE 1048576

typedef struct {
  unsigned crcLength;
  uint32_t crcPoly;
  int crcLookupTable;
  bool crcBigByteOrder;
} CrcParam;

typedef struct {
  bool crcInUse;       /**< Set current usage of CRC computation.            */
  uint32_t crcBuffer;  /**< CRC computation buffer.                          */
  CrcParam crcParam;   /**< Attached CRC generation parameters.              */
} CrcContext;

#define DEF_CRC_CTX()                                                         \
  (                                                                           \
    (CrcContext) {                                                            \
      .crcInUse = false                                                       \
    }                                                                         \
  )

#define IN_USE_BITSTREAM_CRC(bitStreamPtr)                                    \
  ((bitStreamPtr)->crcCtx.crcInUse)

#define BITSTREAM_CRC_CTX(bitStreamPtr)                                       \
  (&(bitStreamPtr)->crcCtx)

/** \~english
 * \brief Bitstreams handling structure used for reading and writing.
 *
 * High-level structure used to handle binary files, providing a secondary
 * buffer to increase performances, allow bit-per-bit operations and compute
 * CRC.
 */
typedef struct {
  uint8_t * byteArray;      /**< Reading byte-array high-level buffer.       */
  size_t byteArrayLength;   /**< Current length of byte-array buffer.        */
  size_t byteArrayOff;      /**< Reading offset in byte-array buffer.        */
  unsigned bitCount;        /**< Remaining unreaded bits in current pointed
    byte-array offset.                                                       */

  CrcContext crcCtx;

  FILE * file;    /**< Linked FILE * pointer.                                */
  char * buffer;  /**< Low level IO buffer.                                  */

  int64_t fileSize;   /**< File length in bytes.                           */
  int64_t fileOffset;   /**< Current file position offset in bytes.          */
  size_t bufferLength;  /**< Initial byteArray buffer length in bytes.       */
  uint64_t identifier;  /**< Bytestream randomized identifier.               */
} BitstreamHandler, *BitstreamWriterPtr, *BitstreamReaderPtr;

/** \~english
 * \brief Generate a unique identifier for #BitstreamHandler structures.
 *
 * \return uint64_t Generated identifier.
 *
 * Uses a static incremented variable. Generated identifier can be used to
 * distinguish two different bitstream handlers.
 */
uint64_t generatedBistreamIdentifier(
  void
);

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

static inline int initCrc(
  CrcContext * crcCtx,
  CrcParam param,
  unsigned initialValue
)
{
  assert(!crcCtx->crcInUse);

  if (initialValue > 0xFFFF || 32 < param.crcLength) {
    LIBBLU_ERROR(
      "CRC computation overflows "
      "(inital value: %u / 65355, crc length: %u).\n",
      initialValue, param.crcLength
    );

    LIBBLU_ERROR_RETURN(
      "CRC computation buffer overflows.\n"
    );
  }

  crcCtx->crcInUse = true;
  crcCtx->crcBuffer = initialValue;
  crcCtx->crcParam = param;

  return 0;
}

static inline int applyCrc(
  CrcContext * crcCtx,
  uint8_t byte
)
{
  uint32_t crcModulo, crcMask;
  uint8_t i;

  assert(crcCtx->crcInUse);

  crcMask = (crcModulo = (uint32_t) 1 << crcCtx->crcParam.crcLength) - 1;

  if (crcCtx->crcParam.crcLookupTable != NO_CRC_TABLE) {
    /* Applying CRC look-up table */
    crcCtx->crcBuffer =
      ((crcCtx->crcBuffer << 8) & crcMask) ^
      getCrcTableValue(
        byte ^ (uint8_t) ((crcCtx->crcBuffer & crcMask) >> 8),
        crcCtx->crcParam.crcLookupTable
      )
    ;

    return 0;
  }

  /* Manual CRC Operation : */
  for (i = 0; i < 8; i++) {
    if (crcCtx->crcParam.crcBigByteOrder)
      crcCtx->crcBuffer = (crcCtx->crcBuffer << 1) ^ (
        ((uint32_t) byte >> (7 - i)) & 1
      );
    else
      crcCtx->crcBuffer = (crcCtx->crcBuffer << 1) ^ (
        (((uint32_t) byte >> (7 - i)) & 1) << crcCtx->crcParam.crcLength
      );

    if (crcModulo <= crcCtx->crcBuffer)
      crcCtx->crcBuffer ^= crcCtx->crcParam.crcPoly;
  }

  return 0;
}

static inline int endCrc(
  CrcContext * crcCtx,
  uint32_t * returnedCrcValue
)
{
  assert(crcCtx->crcInUse);

  crcCtx->crcInUse = false;
  *returnedCrcValue = crcCtx->crcBuffer;

  return 0;
}

int fillBitstreamReader(
  BitstreamReaderPtr bitStream
);

int flushBitstreamWriter(
  BitstreamWriterPtr bitStream
);

static inline int isEof(
  const BitstreamReaderPtr bitStream
)
{
  assert(NULL != bitStream);

  return
    bitStream->byteArrayOff == bitStream->byteArrayLength
    && bitStream->fileOffset == bitStream->fileSize
  ;
}

static inline int64_t tellPos(
  const BitstreamReaderPtr bitStream
)
{
  assert(NULL != bitStream);

  return
    bitStream->fileOffset
    - bitStream->byteArrayLength
    + bitStream->byteArrayOff
  ;
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
  bitStream->byteArrayLength = bitStream->bufferLength;

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
  bitStream->byteArrayLength = bitStream->bufferLength;

  return 0;
}

static inline int nextBytes(
  BitstreamReaderPtr bitStream,
  uint8_t * data,
  const size_t dataLen
)
{
  size_t shiftingSteps;
  size_t readedDataLen;

  if (isEof(bitStream)) /* End of file */
    return -1;

  if (bitStream->byteArrayLength < bitStream->byteArrayOff + dataLen) {
    /* If asked bytes are out of the current reading buffer,
    perform shifting in buffer. */

#if 1
    shiftingSteps = MIN(
      bitStream->byteArrayLength - bitStream->byteArrayOff,
      bitStream->byteArrayOff
    );

    memmove(
      bitStream->byteArray,
      bitStream->byteArray + bitStream->byteArrayOff,
      shiftingSteps
    );
    bitStream->byteArrayOff = 0;

    readedDataLen = fread(
      bitStream->byteArray + shiftingSteps,
      sizeof(uint8_t),
      bitStream->byteArrayLength - shiftingSteps,
      bitStream->file
    );

    if (readedDataLen != bitStream->byteArrayLength - shiftingSteps) {
      bitStream->byteArrayLength = shiftingSteps + readedDataLen;

      if (ferror(bitStream->file)) {
        perror("Reading error");
        LIBBLU_ERROR_RETURN(
          "Input file reading error.\n"
        );
      }
      else if (readedDataLen + shiftingSteps < dataLen) {
        LIBBLU_ERROR_RETURN(
          "Unable to read next bytes, prematurate end of file reached.\n"
        );
      }
    }
    bitStream->fileOffset += readedDataLen;

#else
    size_t i;

    shiftingSteps = (
      bitStream->byteArrayOff + dataLen
    ) - bitStream->byteArrayLength;

    if (shiftingSteps > bitStream->byteArrayOff) {
      LIBBLU_ERROR_RETURN(
        "Not enougth data in reading buffer to perform shifting.\n"
      );
    }

    bitStream->byteArrayOff -= shiftingSteps;
    for (
      i = bitStream->byteArrayOff;
      i < bitStream->byteArrayLength - shiftingSteps;
      i++
    ) {
      bitStream->byteArray[i] = bitStream->byteArray[i + shiftingSteps];
    }

    if (shiftingSteps != 0) {
      readedDataLen = fread(
        bitStream->byteArray + i,
        sizeof(uint8_t),
        shiftingSteps,
        bitStream->file
      );

      if (readedDataLen != shiftingSteps) {
        bitStream->byteArrayOff = bitStream->byteArrayLength;

        if (ferror(bitStream->file)) {
          perror("Reading error");
          LIBBLU_ERROR_RETURN(
            "Input file reading error.\n"
          );
        }

        LIBBLU_ERROR_RETURN(
          "Unable to read next bytes, prematurate end of file reached.\n"
        );
      }

      bitStream->fileOffset += readedDataLen;
    }
#endif
  }

  if (NULL != data)
    memcpy(data, bitStream->byteArray + bitStream->byteArrayOff, dataLen);

  return 0;
}

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

static inline int readBit(
  BitstreamReaderPtr bitStream,
  bool * bit
)
{
  uint8_t byte;
  bool b;

  if (nextBytes(bitStream, &byte, 1) < 0)
    return -1;

  b = (byte >> (--bitStream->bitCount)) & 0x1;
  if (NULL != bit)
    *bit = b;

  if (bitStream->bitCount == 0) {
    /* Applying CRC calculation if in use : */
    if (IN_USE_BITSTREAM_CRC(bitStream)) {
      if (applyCrc(&bitStream->crcCtx, byte) < 0)
        return -1;
    }

    /* A complete byte as been readed, jumping to next one. */
    bitStream->byteArrayOff++; /* Change reading offset to the next byte's one. */
    bitStream->bitCount = 8; /* Reset the number of bits remaining in the current byte. */

    /* If reading offset goes outside of the reading buffer,
    buffering a new section of readed file. */
    if (bitStream->byteArrayOff >= bitStream->byteArrayLength) {
      if (fillBitstreamReader(bitStream) < 0)
        return -1; /* Error happen during low level file reading for buffering. */
    }
  }

  return 0;
}

static inline int readBits(
  BitstreamReaderPtr bitStream,
  uint32_t * value,
  size_t length
)
{
#if USE_ALTER_BIT_READING
  const unsigned short andBits[9] = {
    0x0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF
  };

  uint8_t byte;
  unsigned readedBitsNb;

  uint32_t recursiveValue;
#else
  bool bit;
#endif

  assert(length <= 32); /* Can't read more than 32 bits using readBits() */

#if USE_ALTER_BIT_READING
  if (length == 0) {
    if (NULL != value)
      *value = 0;
    return 0;
  }

  readedBitsNb = MIN(length, bitStream->bitCount);
  bitStream->bitCount -= readedBitsNb;

  if (nextBytes(bitStream, &byte, 1) < 0)
    return -1;

  if (NULL != value) {
    *value = (uint32_t) (byte >> bitStream->bitCount) & andBits[readedBitsNb];
  }

  if (bitStream->bitCount <= 0) {
    /* Applying CRC calculation if in use : */
    if (
      IN_USE_BITSTREAM_CRC(bitStream)
      && applyCrc(&bitStream->crcCtx, byte) < 0
    )
      return -1; /* Error during CRC applying */

    /* A complete byte as been readed, jumping to next one. */
    bitStream->byteArrayOff++; /* Change reading offset to the next byte's one. */
    bitStream->bitCount = 8; /* Reset the number of bits remaining in the current byte. */

    /* If reading offset goes outside of the reading buffer, buffering a new section of readed file. */
    if (!isEof(bitStream) && bitStream->byteArrayOff >= bitStream->byteArrayLength) {
      if (fillBitstreamReader(bitStream) < 0)
        return -1;
    }

    /* Reading recursively extra bits in following byte if needed : */
    length -= readedBitsNb;
    if (length != 0) {
      if (readBits(bitStream, &recursiveValue, length) < 0)
        return -1;

      *value = (*value << length) | recursiveValue;
    }
  }
#else
  if (NULL != value)
    *value = 0;

  while (length--) {
    if (readBit(bitStream, &bit) < 0)
      return -1;

    if (NULL != value)
      *value = (*value << 1) + ((uint8_t) bit);
  }
#endif
  return 0;
}

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

static inline int readByte(
  BitstreamReaderPtr bitStream,
  uint8_t * data
)
{
  uint8_t byte;

  if (bitStream->bitCount != 8) {
    uint32_t value;

    if (readBits(bitStream, &value, 8) < 0)
      return -1;

    if (NULL != data)
      *data = (uint8_t) value;
    return 0;
  }

  if (bitStream->byteArrayLength <= bitStream->byteArrayOff) {
    /* If the end of the reading buffer has been reached,
    buffering a new file section. */
    if (fillBitstreamReader(bitStream) < 0)
      return -1;
  }

  byte = bitStream->byteArray[bitStream->byteArrayOff++];
  if (NULL != data)
    *data = byte;

  /* Applying CRC calculation if in use : */
  if (IN_USE_BITSTREAM_CRC(bitStream))
    return applyCrc(&bitStream->crcCtx, byte);
  return 0;
}

static inline int readBytes(
  BitstreamReaderPtr bitStream,
  uint8_t * data,
  const size_t dataLen
)
{
  size_t i;

  for (i = 0; i < dataLen; i++) {
    if (readByte(bitStream, data++) < 0)
      return -1;
  }

  return 0;
}

static inline int readValueBigEndian(
  BitstreamReaderPtr bitStream,
  const size_t length,
  uint32_t * value
)
{
  size_t idx;
  uint32_t v;
  uint8_t byte;

  assert(0 < length && length <= 4);

  v = 0;
  byte = 0;
  for (idx = 0; idx < length; idx++) {
    if (readByte(bitStream, &byte) < 0)
      return -1;
    v |= (uint32_t) (byte << (8 * (length - idx - 1)));
  }

  if (NULL != value)
    *value = v;

  return 0;
}

static inline int readValue64BigEndian(
  BitstreamReaderPtr bitStream,
  const size_t length,
  uint64_t * value
)
{
  size_t idx;
  uint64_t v;
  uint8_t byte;

  assert(0 < length && length <= 8);

  v = 0;
  for (idx = 0; idx < length; idx++) {
    if (readByte(bitStream, &byte) < 0)
      return -1;
    v |= ((uint64_t) byte << (8 * (length - idx - 1)));
  }

  if (NULL != value)
    *value = v;

  return 0;
}

static inline int readValue64LittleEndian(
  BitstreamReaderPtr bitStream,
  const size_t length,
  uint64_t * value
)
{
  size_t idx;
  uint64_t v;
  uint8_t byte;

  assert(0 < length && length <= 4);

  v = 0;
  for (idx = 0; idx < length; idx++) {
    if (readByte(bitStream, &byte) < 0)
      return -1;
    v |= (uint64_t) (byte << (8 * idx));
  }

  if (NULL != value)
    *value = v;

  return 0;
}

static inline int skipBytes(
  BitstreamReaderPtr bitStream,
  size_t length
)
{
  while (0 < (length--))
    if (readByte(bitStream, NULL) < 0)
      return -1;

  return 0;
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
  BitstreamReaderPtr bitStream,
  unsigned size
)
{
  int64_t curOffset;
  size_t paddingSize;

  if ((curOffset = tellBinaryPos(bitStream)) < 0)
    LIBBLU_ERROR_RETURN("Unexpected negative file offset.\n");
  paddingSize = size - (curOffset % size);

  if (paddingSize == size)
    return 0; /* Already aligned */
  return skipBits(bitStream, paddingSize);
}

int initCrc(
  CrcContext * crcCtx,
  CrcParam param,
  unsigned initialValue
);

int applyCrc(
  CrcContext * crcCtx,
  uint8_t bit
);

int endCrc(
  CrcContext * crcCtx,
  uint32_t * returnedCrcValue
);

static inline int writeByte(
  BitstreamWriterPtr bitStream,
  uint8_t data
)
{
  if (bitStream->byteArrayOff >= bitStream->byteArrayLength) {
    if (flushBitstreamWriter(bitStream) < 0)
      return -1;
  }

  bitStream->byteArray[bitStream->byteArrayOff++] = data;
  bitStream->fileSize++;

  return 0;
}

static inline int writeBytes(
  BitstreamWriterPtr bitStream,
  const uint8_t * data,
  const size_t dataLen
)
{
  size_t i;

  assert(NULL != bitStream);
  assert(NULL != data);

  for (i = 0; i < dataLen; i++) {
    if (bitStream->byteArrayOff >= bitStream->byteArrayLength) {
      if (flushBitstreamWriter(bitStream) < 0)
        return -1;
    }

    bitStream->byteArray[bitStream->byteArrayOff++] = data[i];
  }

  return 0;
}

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
