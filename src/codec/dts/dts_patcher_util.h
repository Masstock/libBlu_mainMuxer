/** \~english
 * \file dts_patcher_util.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams patching utilities module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__PATCHER_UTIL_H__
#define __LIBBLU_MUXER__CODECS__DTS__PATCHER_UTIL_H__

#include "../../util.h"
#include "../../esms/scriptCreation.h"

#include "dts_error.h"

/* #include "dts_util.h" */

/** \~english
 * \brief Bitstream construction handling structure.
 *
 * Used to compile a byte-array to be inserted in output stream/
 */
typedef struct {
  uint8_t * data;              /**< Constructed bitstream byte array.        */
  size_t dataAllocatedLength;  /**< Current array allocation size.           */
  size_t dataUsedLength;       /**< Current array used size.                 */

  uint8_t currentByte;         /**< Bit-level temporary byte.                */
  char currentByteUsedBits;    /**< Currently used bits in temporary byte.   */

  CrcContext crc;              /**< CRC checksum generation context.         */
} DtsPatcherBitstreamHandle, *DtsPatcherBitstreamHandlePtr;

#define DTS_PATCHER_BITSTREAM_HANDLE_DEFAULT_SIZE 1024

/** \~english
 * \brief Create a #DtsPatcherBitstreamHandle object.
 *
 * \return DtsPatcherBitstreamHandlePtr Upon success, created object is
 * returned. Otherwise, a NULL pointer is returned.
 *
 * \note Created object must be passed to #destroyDtsPatcherBitstreamHandle()
 * after use.
 */
DtsPatcherBitstreamHandlePtr createDtsPatcherBitstreamHandle(
  void
);

/** \~english
 * \brief Destroy memory allocation created by
 * #createDtsPatcherBitstreamHandle() function.
 *
 * \param handle Bitstream handle to free.
 */
void destroyDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle
);

int getGeneratedArrayDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  const uint8_t ** array,
  size_t * arraySize
);

/** \~english
 * \brief Init CRC checksum computation on given bitstream handle.
 *
 * \param handle Bitstream handle.
 * \param param CRC computation parameters.
 * \param initialValue Initial CRC value.
 *
 * After initialization, added bits in bitstream will be proceed in CRC
 * computation.
 * Only one CRC computation process can be used at one time.
 */
void initCrcDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  CrcParam param,
  uint32_t initialValue
);

/** \~english
 * \brief Complete CRC checksum computation on given bitstream handle.
 *
 * \param handle Bitstream handle with enabled CRC process.
 * \return uint32_t Final CRC computed value.
 */
uint16_t finalizeCrcDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle
);

/** \~english
 * \brief Write a single byte on given bitstream handle.
 *
 * \param handle Destination bitstream handle.
 * \param byte Written byte.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * \note This function shall only be used if destination bitstream handle is
 * byte-boundary aligned. It can be ensured using
 * #isByteAlignedDtsPatcherBitstreamHandle().
 */
int writeByteDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  uint8_t byte
);

/** \~english
 * \brief Write a sequence of bytes on given bitstream handle.
 *
 * \param handle Destination bitstream handle.
 * \param bytes Bytes array to write.
 * \param size Number of bytes to be written from given array.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int writeBytesDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  const uint8_t * bytes,
  size_t size
);

/** \~english
 * \brief Return true if given bitstream handle is byte boundary aligned.
 *
 * \param handle Bitstream handle.
 * \return true Bitstream is byte boundary aligned.
 * \return false Bitstream writing pointer is not aligned and shall be aligned
 * using single bits prior to using bytes writing.
 *
 * Handle can be non-byte aligned if individual bits, not matching a complete
 * byte (so a multiple of 8 bits), have been written in bitstream.
 */
bool isByteAlignedDtsPatcherBitstreamHandle(
  const DtsPatcherBitstreamHandlePtr handle
);

/** \~english
 * \brief Align to next byte boundary bitstream handle.
 *
 * \param handle Bitstream handle.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Missing bits from current byte will be filled of 0b0.
 */
int byteAlignDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle
);

/** \~english
 * \brief Get vurrent bit distance relative to last byte boundary in bits.
 *
 * \param handle
 * \param blockStartBitOffset
 * \return int
 *
 * This may be used with #blockByteAlignDtsPatcherBitstreamHandle() to ensure
 * the size of a defined a block of binary fields in bits is a multiple of 8.
 *
 * \code{.c}
 * DtsPatcherBitstreamHandlePtr handle;
 * size_t off;
 *
 * // Initialize handle and write some bits
 *
 * // Aligned size-block start:
 * getBlockBinaryBoundaryOffDtsPatcherBitstreamHandle(handle, &off);
 *
 * // Write some bits
 *
 * // Aligned size block end:
 * if (blockByteAlignDtsPatcherBitstreamHandle(handle, off) < 0)
 *   exit(EXIT_FAILURE);
 * \endcode
 */
void getBlockBinaryBoundaryOffDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  size_t * blockStartBitOffset
);

/** \~english
 * \brief Align to next block start relative byte boundary.
 *
 * \param handle
 * \param blockStartBitOffset Block first bit position offset
 * \return int
 *
 * Append 0b0 bits to ensure the size of an arbitrary block is a multiple of
 * 8 bits. See #getBlockBinaryBoundaryOffDtsPatcherBitstreamHandle().
 */
int blockByteAlignDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  size_t blockStartBitOffset
);

/** \~english
 * \brief Write a single bit on given bitstream handle.
 *
 * \param handle Destination bitstream handle.
 * \param bit Bit to write.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Using this function may lead bitstream handle in a non-byte aligned state,
 * unallowing addition of bytes until alignement has been performed using
 * #byteAlignDtsPatcherBitstreamHandle() or manually using enough bits.
 */
int writeBitDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  bool bit
);

/** \~english
 * \brief Write a sequence of bits on given bitstream handle.
 *
 * \param handle Destination bitstream handle.
 * \param bits Binary value to write.
 * \param size Number of bits from given value to be written between 0 and
 * 32 inclusive.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Bits are taken from given value in a least-significant-bit-first order.
 */
int writeBitsDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  uint32_t bits,
  size_t size
);

/** \~english
 * \brief Write an integer value on given bitstream handle.
 *
 * \param handle Destination bitstream handle.
 * \param value Value to write.
 * \param size Field size in bits.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * If given value overflows specified field size, an error is returned.
 * Bits are taken from given value in a least-significant-bit-first order.
 */
int writeValueDtsPatcherBitstreamHandle(
  DtsPatcherBitstreamHandlePtr handle,
  uint32_t value,
  size_t size
);

#endif