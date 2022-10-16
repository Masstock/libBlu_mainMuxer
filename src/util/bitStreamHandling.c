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

uint64_t generatedBistreamIdentifier(void)
{
  static uint64_t id = 0;
  return id++;
}

BitstreamReaderPtr createBitstreamReader(
  const lbc * inputFilename,
  const size_t bufferSize
)
{
  BitstreamReaderPtr bitStream;
  char * buffer;

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

  if (NULL == (buffer = (char *) malloc(IO_VBUF_SIZE)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  if (setvbuf(bitStream->file, buffer, _IOFBF, IO_VBUF_SIZE) < 0)
    LIBBLU_ERROR_FRETURN(
      "Reading buffer definition error, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  bitStream->buffer = buffer;

  if (NULL == (bitStream->byteArray = (uint8_t *) malloc(bufferSize)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  bitStream->bufferLength = bitStream->byteArrayLength = bufferSize;
  bitStream->byteArrayOff = bufferSize;
  bitStream->bitCount = 8; /* By default, a complete byte can be readed at bit level. */
  bitStream->crcCtx = DEF_CRC_CTX();

  bitStream->identifier = generatedBistreamIdentifier();

  if (getFileSize(inputFilename, &bitStream->fileSize) < 0)
    LIBBLU_ERROR_FRETURN("Unable to mesure input file length.\n");

  return bitStream;

free_return:
  closeBitstreamReader(bitStream);
  return NULL;
}

BitstreamWriterPtr createBitstreamWriter(
  const lbc * outputFilename,
  const size_t bufferSize
)
{
  BitstreamWriterPtr bitStream;
  char * buffer;

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

  if (NULL == (buffer = (char *) malloc(IO_VBUF_SIZE)))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  if (setvbuf(bitStream->file, buffer, _IOFBF, IO_VBUF_SIZE) < 0) {
    perror("Unable to set writing buffer");
    LIBBLU_ERROR_NRETURN("Error happen during writing buffer creation.\n");
  }
  bitStream->buffer = buffer;

  if (NULL == (bitStream->byteArray = (uint8_t *) malloc(bufferSize)))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  bitStream->byteArrayLength = bufferSize;
  bitStream->crcCtx = DEF_CRC_CTX();

  bitStream->identifier = generatedBistreamIdentifier();

  return bitStream;
}

void closeBitstreamWriter(
  BitstreamWriterPtr bitStream
)
{
  if (NULL == bitStream)
    return;

  flushBitstreamWriter(bitStream);
  free(bitStream->byteArray);
  if (NULL != bitStream->file)
    fclose(bitStream->file);
  free(bitStream->buffer);
  free(bitStream);
}

int fillBitstreamReader(
  BitstreamReaderPtr bitStream
)
{
  size_t readedDataLen;

  if (bitStream->byteArrayOff < bitStream->byteArrayLength)
    return 0;

  readedDataLen = fread(
    bitStream->byteArray,
    sizeof(uint8_t),
    bitStream->byteArrayLength,
    bitStream->file
  );

  if (bitStream->byteArrayLength != readedDataLen) {
    if (ferror(bitStream->file))
      LIBBLU_ERROR_RETURN(
        "Error happen during input file reading, %s (errno: %d).\n",
        strerror(errno),
        errno
      );

    LIBBLU_DEBUG_COM(
      "Unable to completly fill input buffer, reducing its size "
      "(old: %zu, new: %zu, EOF: %s).\n",
      bitStream->byteArrayLength,
      readedDataLen,
      BOOL_INFO(feof(bitStream->file))
    );

    /* Not enouth data to fill entire read buffer,
    updating buffer virtual length. */
    bitStream->byteArrayLength = readedDataLen;
  }
  bitStream->fileOffset = bitStream->fileOffset + readedDataLen;
  bitStream->byteArrayOff = 0;

  return 0;
}

int flushBitstreamWriter(
  BitstreamWriterPtr bitStream
)
{
  size_t readedLen;

  if (bitStream->byteArrayOff == 0)
    return 0; /* Empty writing buffer */

  readedLen = fwrite(
    bitStream->byteArray,
    sizeof(uint8_t),
    bitStream->byteArrayOff,
    bitStream->file
  );

  if (bitStream->byteArrayOff != readedLen)
    LIBBLU_ERROR_RETURN(
      "Error happen during output file writing, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  bitStream->fileOffset += bitStream->byteArrayOff;
  bitStream->byteArrayOff = 0;

  return 0;
}

void crcTableGenerator(const CrcParam param)
{
  /* Usage: crcTableGenerator(crcParam); */
  unsigned bit, byte;
  uint32_t modulo, crc;

  assert(param.crcLength <= 32);

  modulo = (uint32_t) (1 << (param.crcLength - 1));

  lbc_printf("{\n");
  for (byte = 0; byte < 256; byte++) {
    if (0 == (byte & 0x7) && 0 < byte)
      lbc_printf("\n  ");
    else if (byte == 0)
      lbc_printf("  ");

    crc = byte;
    for (bit = 0; bit < param.crcLength; bit++) {
      if (crc & modulo)
        crc = (crc << 1) ^ param.crcPoly;
      else
        crc <<= 1;
    }

    lbc_printf("0x%" PRIX32, crc);
    if (byte != 255)
      lbc_printf(", ");
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
