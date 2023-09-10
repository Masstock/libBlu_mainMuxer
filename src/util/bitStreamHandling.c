#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "bitStreamHandling.h"

#if defined(ARCH_WIN32)
#  include <pdh.h>
#  include <winnt.h>
#  include <fileapi.h>
#else
#  include <sys/stat.h>
#endif

void applyNoTableShiftedCrcContext(
  CrcContext * ctx,
  uint8_t byte
)
{
  unsigned length = ctx->param.length + 7;
  uint32_t poly = ctx->param.poly << 8;
  uint64_t buf  = ctx->buf << 8;

  buf ^= byte;
  for (unsigned i = 0; i < 8; i++) {
    if (buf & (1 << length))
      buf = (buf << 1) ^ poly;
    else
      buf <<= 1;
  }

  ctx->buf = buf >> 8;
}

void applyNoTableCrcContext(
  CrcContext * ctx,
  uint8_t byte
)
{
  unsigned length = ctx->param.length;
  uint32_t poly = ctx->param.poly;
  uint64_t buf  = ctx->buf;

  buf ^= byte << (length - 8);
  for (unsigned i = 0; i < 8; i++) {
    if (buf & (1 << length))
      buf = (buf << 1) ^ poly;
    else
      buf <<= 1;
  }

  ctx->buf = buf;
}

#if 0
void applyNoTableCrcContext(
  CrcContext * ctx,
  uint8_t byte
)
{
  if (!ctx->in_use)
    return;

  uint32_t modulo, mask = (modulo = (1ull << ctx->param.crcLength)) - 1;

  if (ctx->param.crcLookupTable != NO_CRC_TABLE) {
    /* Applying CRC look-up table */
    ctx->buf = ((ctx->buf << 8) & mask) ^ getCrcTableValue(
      (byte ^ (uint8_t) ((ctx->buf & mask) >> 8)),
      ctx->param.crcLookupTable
    );
    return;
  }

  /* Manual CRC Operation : */
  for (unsigned i = 0; i < 8; i++) {
    ctx->buf <<= 1;
    if (ctx->param.crcBigByteOrder)
      ctx->buf ^= (((uint32_t) byte >> (7 - i)) & 1);
    else
      ctx->buf ^= ((((uint32_t) byte >> (7 - i)) & 1) << ctx->param.crcLength);

    if (modulo <= ctx->buf)
      ctx->buf ^= ctx->param.crcPoly;
  }
}
#endif

/** \~english
 * \brief Generate a unique identifier for #BitstreamHandler structures.
 *
 * \return uint64_t Generated identifier.
 *
 * Uses a static incremented variable. Generated identifier can be used to
 * distinguish two different bitstream handlers.
 */
static uint64_t _generatedBistreamIdentifier(
  void
)
{
  static uint64_t id = 0;

  return id++;
}

static int _fillBitstreamReader(
  BitstreamReaderPtr bs
)
{
  size_t readedSize;

  if (bs->byteArrayOff < bs->byteArraySize)
    return 0;

  readedSize = fread(bs->byteArray, 1, bs->byteArraySize, bs->file);

  if (bs->byteArraySize != readedSize) {
    if (ferror(bs->file))
      LIBBLU_ERROR_RETURN(
        "Error happen during input file reading, %s (errno: %d).\n",
        strerror(errno),
        errno
      );

    if (!readedSize) {
      LIBBLU_ERROR_RETURN(
        "Error happen during input file reading, EOF reached.\n"
      );
    }

    LIBBLU_DEBUG_COM(
      "Unable to completly fill input buffer, reducing its size "
      "(old: %zu, new: %zu, EOF: %s).\n",
      bs->byteArraySize,
      readedSize,
      BOOL_INFO(feof(bs->file))
    );

    /* Not enouth data to fill entire read buffer,
    updating buffer virtual length. */
    bs->byteArraySize = readedSize;
  }
  bs->fileOffset = bs->fileOffset + readedSize;
  bs->byteArrayOff = 0;

  return 0;
}

static int _flushBitstreamWriter(
  BitstreamWriterPtr bs
)
{
  size_t readedSize;

  if (bs->byteArrayOff == 0)
    return 0; /* Empty writing buffer */

  readedSize = fwrite(bs->byteArray, 1, bs->byteArrayOff, bs->file);

  if (bs->byteArrayOff != readedSize)
    LIBBLU_ERROR_RETURN(
      "Error happen during output file writing, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  bs->fileOffset += bs->byteArrayOff;
  bs->byteArrayOff = 0;

  return 0;
}

BitstreamReaderPtr createBitstreamReader(
  const lbc * inputFilename,
  const size_t bufferSize
)
{
  BitstreamReaderPtr bitStream;

  assert(NULL != inputFilename);
  assert(32 < bufferSize);

  if (READ_BUFFER_LEN < bufferSize)
    LIBBLU_ERROR_NRETURN("Specified buffering size is too high.\n");

  if (NULL == (bitStream = (BitstreamReaderPtr) malloc(sizeof(BitstreamHandler))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  *bitStream = (BitstreamHandler) {0};

  if (NULL == (bitStream->file = lbc_fopen(inputFilename, "rb")))
    LIBBLU_ERROR_FRETURN(
      "Error happen during input file '%" PRI_LBCS "' opening, "
      "%s (errno: %d).\n",
      inputFilename,
      strerror(errno),
      errno
    );

#if !defined(DISABLE_READ_BUFFER)
  {
    char * buffer;

    if (NULL == (buffer = (char *) malloc(IO_VBUF_SIZE)))
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

    if (setvbuf(bitStream->file, buffer, _IOFBF, IO_VBUF_SIZE) < 0)
      LIBBLU_ERROR_FRETURN(
        "Reading buffer definition error, %s (errno: %d).\n",
        strerror(errno),
        errno
      );
    bitStream->buffer = buffer;
  }
#endif

  if (NULL == (bitStream->byteArray = (uint8_t *) malloc(bufferSize)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  bitStream->bufferLength = bitStream->byteArraySize = bufferSize;
  bitStream->byteArrayOff = bufferSize;
  bitStream->bitCount = 8; /* By default, a complete byte can be readed at bit level. */
  resetCrcContext(&bitStream->crcCtx);

  bitStream->identifier = _generatedBistreamIdentifier();

  if (getFileSize(inputFilename, &bitStream->fileSize) < 0)
    LIBBLU_ERROR_FRETURN("Unable to mesure input file length.\n");

  return bitStream;

free_return:
  closeBitstreamReader(bitStream);
  return NULL;
}

void closeBitstreamReader(
  BitstreamReaderPtr bitStream
)
{
  if (NULL == bitStream)
    return;

  free(bitStream->byteArray);
  if (NULL != bitStream->file)
    fclose(bitStream->file);
  free(bitStream->buffer);
  free(bitStream);
}

BitstreamWriterPtr createBitstreamWriter(
  const lbc * outputFilename,
  const size_t bufferSize
)
{
  BitstreamWriterPtr bitStream;

  assert(NULL != outputFilename);
  assert(32 < bufferSize);

  if (NULL == (bitStream = (BitstreamWriterPtr) malloc(sizeof(BitstreamHandler))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  *bitStream = (BitstreamHandler) {0};

  if (NULL == (bitStream->file = lbc_fopen(outputFilename, "wb")))
    LIBBLU_ERROR_NRETURN(
      "Unable to open output file '%" PRI_LBCS "', %s (errno: %d).\n",
      outputFilename,
      strerror(errno),
      errno
    );

#if !defined(DISABLE_WRITE_BUFFER)
  {
    char * buffer;

    if (NULL == (buffer = (char *) malloc(IO_VBUF_SIZE)))
      LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

    if (setvbuf(bitStream->file, buffer, _IOFBF, IO_VBUF_SIZE) < 0) {
      perror("Unable to set writing buffer");
      LIBBLU_ERROR_NRETURN("Error happen during writing buffer creation.\n");
    }

    bitStream->buffer = buffer;
  }
#endif

  if (NULL == (bitStream->byteArray = (uint8_t *) malloc(bufferSize)))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  bitStream->byteArraySize = bufferSize;
  resetCrcContext(&bitStream->crcCtx);

  bitStream->identifier = _generatedBistreamIdentifier();

  return bitStream;
}

void closeBitstreamWriter(
  BitstreamWriterPtr bitStream
)
{
  if (NULL == bitStream)
    return;

  _flushBitstreamWriter(bitStream);
  free(bitStream->byteArray);
  if (NULL != bitStream->file)
    fclose(bitStream->file);
  free(bitStream->buffer);
  free(bitStream);
}

int nextBytes(
  BitstreamReaderPtr bitStream,
  uint8_t * data,
  size_t dataLen
)
{
  size_t shiftingSteps;
  size_t readedDataLen;

  if (unlikely(isEof(bitStream))) /* End of file */
    return -1;

  if (bitStream->byteArraySize < bitStream->byteArrayOff + dataLen) {
    /* If asked bytes are out of the current reading buffer,
    perform shifting in buffer. */

#if 1
    shiftingSteps = MIN(
      bitStream->byteArraySize - bitStream->byteArrayOff,
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
      bitStream->byteArraySize - shiftingSteps,
      bitStream->file
    );

    if (readedDataLen != bitStream->byteArraySize - shiftingSteps) {
      bitStream->byteArraySize = shiftingSteps + readedDataLen;

      if (ferror(bitStream->file)) {
        LIBBLU_ERROR_RETURN(
          "Input file reading error, %s (errno: %d).\n",
          strerror(errno),
          errno
        );
      }

      if (readedDataLen + shiftingSteps < dataLen) {
        /* LIBBLU_ERROR_RETURN(
          "Unable to read next bytes, prematurate end of file reached.\n"
        ); */
        return -1;
      }
    }
    bitStream->fileOffset += readedDataLen;

#else
    size_t i;

    shiftingSteps = (
      bitStream->byteArrayOff + dataLen
    ) - bitStream->byteArraySize;

    if (shiftingSteps > bitStream->byteArrayOff) {
      LIBBLU_ERROR_RETURN(
        "Not enougth data in reading buffer to perform shifting.\n"
      );
    }

    bitStream->byteArrayOff -= shiftingSteps;
    for (
      i = bitStream->byteArrayOff;
      i < bitStream->byteArraySize - shiftingSteps;
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
        bitStream->byteArrayOff = bitStream->byteArraySize;

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

int readBit(
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
    applySingleByteCrcContext(&bitStream->crcCtx, byte);

    /* A complete byte as been readed, jumping to next one. */
    bitStream->byteArrayOff++; /* Change reading offset to the next byte's one. */
    bitStream->bitCount = 8; /* Reset the number of bits remaining in the current byte. */

    /* If reading offset goes outside of the reading buffer,
    buffering a new section of readed file. */
    if (bitStream->byteArraySize <= bitStream->byteArrayOff) {
      if (_fillBitstreamReader(bitStream) < 0)
        return -1; /* Error happen during low level file reading for buffering. */
    }
  }

  return 0;
}

#if USE_ALTER_BIT_READING

int readBits(
  BitstreamReaderPtr bitStream,
  uint32_t * value,
  size_t size
)
{
  uint8_t byte;
  unsigned readedBitsNb;

  static const unsigned bitmask[9] = {
    0x00, 0x01, 0x03, 0x07,
    0x0F, 0x1F, 0x3F, 0x7F,
    0xFF
  };

  assert(size <= 32); /* Can't read more than 32 bits using readBits() */

  if (unlikely(size == 0)) {
    if (NULL != value)
      *value = 0;
    return 0;
  }

  readedBitsNb = MIN(size, bitStream->bitCount);
  bitStream->bitCount -= readedBitsNb;

  if (unlikely(nextBytes(bitStream, &byte, 1) < 0))
    LIBBLU_ERROR_RETURN("Prematurate end of file.\n");

  if (NULL != value)
    *value = (byte >> bitStream->bitCount) & bitmask[readedBitsNb];

  if (bitStream->bitCount == 0) {
    /* Applying CRC calculation if in use : */
    applySingleByteCrcContext(&bitStream->crcCtx, byte);

    /* A complete byte as been readed, jumping to next one. */
    bitStream->byteArrayOff++; /* Change reading offset to the next byte's one. */
    bitStream->bitCount = 8; /* Reset the number of bits remaining in the current byte. */

    /* If reading offset goes outside of the reading buffer, buffering a new section of readed file. */
    if (unlikely(!isEof(bitStream) && bitStream->byteArraySize <= bitStream->byteArrayOff)) {
      if (_fillBitstreamReader(bitStream) < 0)
        return -1;
    }

    size -= readedBitsNb;
    if (0 < size) {
      /* Reading recursively extra bits in following byte if needed : */
      uint32_t recursiveValue;

      if (readBits(bitStream, &recursiveValue, size) < 0)
        return -1;
      *value = (*value << size) | recursiveValue;
    }
  }

  return 0;
}

#else

int readBits(
  BitstreamReaderPtr bitStream,
  uint32_t * value,
  size_t size
)
{
  bool bit;

  assert(0 < size);
  assert(size <= 32); /* Can't read more than 32 bits using readBits() */

  if (NULL != value)
    *value = 0;

  while (length--) {
    if (readBit(bitStream, &bit) < 0)
      return -1;

    if (NULL != value)
      *value = (*value << 1) + ((uint8_t) bit);
  }

  return 0;
}

#endif

int readByte(
  BitstreamReaderPtr bitStream,
  uint8_t * data
)
{
  uint8_t byte;

  assert(NULL != bitStream);
  assert(NULL != data);

  if (bitStream->bitCount != 8) {
    uint32_t value;

    if (readBits(bitStream, &value, 8) < 0)
      return -1;

    *data = value;
    return 0;
  }

  if (bitStream->byteArraySize <= bitStream->byteArrayOff) {
    /* If the end of the reading buffer has been reached,
    buffering a new file section. */
    if (_fillBitstreamReader(bitStream) < 0)
      return -1;
  }

  byte = bitStream->byteArray[bitStream->byteArrayOff++];
  *data = byte;

  /* Applying CRC calculation if in use : */
  applySingleByteCrcContext(&bitStream->crcCtx, byte);
  return 0;
}

int readBytes(
  BitstreamReaderPtr bs,
  uint8_t * data,
  size_t size
)
{

  while (0 < size) {
    if (bs->byteArraySize <= bs->byteArrayOff) {
      if (_fillBitstreamReader(bs) < 0)
        return -1;
    }

    size_t read_size = MIN(bs->byteArraySize - bs->byteArrayOff, size);

    if (NULL != data) {
      memcpy(data, bs->byteArray + bs->byteArrayOff, read_size);
      data += read_size;
    }

    applyCrcContext(&bs->crcCtx, bs->byteArray + bs->byteArrayOff, read_size);
    bs->byteArrayOff += read_size;
    size -= read_size;
  }

  return 0;
}

int readValueBigEndian(
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

int readValue64BigEndian(
  BitstreamReaderPtr bs,
  size_t size,
  uint64_t * value
)
{
  assert(0 < size && size <= 8);

  uint64_t v = 0;
  for (size_t idx = 0; idx < size; idx++) {
    uint8_t byte;
    if (readByte(bs, &byte) < 0)
      return -1;
    v |= ((uint64_t) byte) << ((size - idx - 1) << 3ull);
  }

  if (NULL != value)
    *value = v;

  return 0;
}

int readValue64LittleEndian(
  BitstreamReaderPtr bitStream,
  size_t length,
  uint64_t * value
)
{
  assert(0 < length && length <= 8);

  uint64_t v = 0;
  for (size_t idx = 0; idx < length; idx++) {
    uint8_t byte;
    if (readByte(bitStream, &byte) < 0)
      return -1;
    v |= byte << (8ull * idx);
  }

  if (NULL != value)
    *value = v;

  return 0;
}

int writeByte(
  BitstreamWriterPtr bitStream,
  uint8_t data
)
{
  if (bitStream->byteArraySize <= bitStream->byteArrayOff) {
    if (_flushBitstreamWriter(bitStream) < 0)
      return -1;
  }

  bitStream->byteArray[bitStream->byteArrayOff++] = data;
  bitStream->fileSize++;

  return 0;
}

int writeBytes(
  BitstreamWriterPtr bs,
  const uint8_t * data,
  size_t size
)
{
#if 0
  size_t i;

  assert(NULL != bs);
  assert(NULL != data);

  for (i = 0; i < size; i++) {
    if (bs->byteArraySize <= bs->byteArrayOff) {
      if (_flushBitstreamWriter(bs) < 0)
        return -1;
    }

    bs->byteArray[bs->byteArrayOff++] = data[i];
    bs->fileSize++;
  }
#else
  size_t off = 0;
  size_t totalSize = size;

  assert(NULL != bs);
  assert(NULL != data);

  while (0 < size) {
    size_t availableSize = bs->byteArraySize - bs->byteArrayOff;

    if (availableSize < size) {
      memcpy(bs->byteArray + bs->byteArrayOff, data + off, availableSize);

      off += availableSize;
      bs->byteArrayOff += availableSize;
      size -= availableSize;

      if (_flushBitstreamWriter(bs) < 0)
        return -1;
    }
    else {
      memcpy(bs->byteArray + bs->byteArrayOff, data + off, size);
      bs->byteArrayOff += size;
      break;
    }
  }

  bs->fileSize += totalSize;
#endif

  return 0;
}

void crcTableGenerator(
  CrcParam param
)
{
  /* Usage: crcTableGenerator(param); */
  unsigned length = param.length;
  uint32_t poly   = param.poly;
  const lbc * eol = lbc_str("");

  lbc_printf("{\n");
  for (unsigned byte = 0; byte < 256; byte++) {
    if (!(byte & 0x7))
      lbc_printf("%s  ", eol);

    uint64_t crc = byte;
    for (unsigned bit = 0; bit < length; bit++) {
      // if (crc & 0x1)
      //   crc = (crc >> 1) ^ poly;
      // else
      //   crc >>= 1;
      if (crc & (1ull << (length - 1)))
        crc = (crc << 1) ^ poly;
      else
        crc <<= 1;
    }

    lbc_printf("0x%0*" PRIX64 ", ", length >> 2, crc);
    eol = lbc_str("\n");
  }

  lbc_printf("\n}\n");
}

#if defined(ARCH_WIN32)

int getFileSize(
  const lbc * filename,
  int64_t * size
)
{
  HANDLE file;
  LARGE_INTEGER st;

  assert(NULL != filename);
  assert(NULL != size);

  /* lbc is wchar_t on WIN32 */
  file = CreateFileW(
    filename,
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL
  );
  if (file == INVALID_HANDLE_VALUE)
    return -1;

  if (!GetFileSizeEx(file, &st))
    goto free_return;

  *size = (int64_t) st.QuadPart;

  CloseHandle(file);

  return 0;

free_return:
  CloseHandle(file);
  return -1;
}

#else

int getFileSize(
  const lbc * filename,
  int64_t * size
)
{
  struct stat st;

  assert(NULL != filename);
  assert(NULL != size);

  /* lbc is char on Unix */
  if (stat(filename, &st) == -1)
    LIBBLU_ERROR_RETURN(
      "Unable to get '%" PRI_LBCS "' file stats, %s (errno: %d).\n",
      filename,
      strerror(errno),
      errno
    );

  *size = st.st_size;

  return 0;
}

#endif
