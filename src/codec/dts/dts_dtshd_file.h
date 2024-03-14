/** \~english
 * \file dts_dtshd_file.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio DTS-HD files (.dtshd) handling module.
 *
 * REFERENCE:
 *  - ATSC A/103:2014, "Annex E: DTS-HD File Structure".
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__DTSHD_FILE_H__
#define __LIBBLU_MUXER__CODECS__DTS__DTSHD_FILE_H__

#include "../../util.h"
#include "dts_util.h"
#include "dts_data.h"

/** \~english
 * \brief DTS-HD file format chunks identification magic numbers values.
 *
 * A DTS-HD file is composed of a succession of chunks composed of:
 *
 * \code{.unparsed}
 *  u64 - ASCII identifier, magic number.
 *  u64 - Size of chunk in bytes from next byte (payload, reserved field and
 *        padding inclusive; magic number and size field exclude).
 *  vn  - Payload. Content defined by identifier.
 *  vn  - Reserved field, for future use.
 *  vn  - Padding field, composed of 0x0, used to align chunk end to DWORD
 *        (32-bit) boundary. n size value of this field is comprise between
 *        0 and 31 bits inclusive.
 * \endcode
 */
typedef enum {
  DTS_HD_DTSHDHDR  = 0x4454534844484452,  /**< DTS-HD file header chunk.     */
  DTS_HD_FILEINFO  = 0x46494C45494E464F,  /**< Textual file description
    chunk.                                                                   */
  DTS_HD_CORESSMD  = 0x434F524553534D44,  /**< Metadata of Core Sub-stream.  */
  DTS_HD_EXTSS_MD  = 0x45585453535F4D44,  /**< Metadata of Ext. Sub-stream.  */
  DTS_HD_AUPR_HDR  = 0x415550522D484452,  /**< Metadata of specific Audio
    Presentation.                                                            */
  DTS_HD_AUPRINFO  = 0x41555052494E464F,  /**< Textual specific Audio
    Presentation description chunk.                                          */
  DTS_HD_NAVI_TBL  = 0x4E4156492D54424C,  /**< File Navigation Pointers for
    variable bit-rate streams chunk.                                         */
  DTS_HD_BITSHVTB  = 0x4249545348565442,  /**< Lossless Stream LSB bits
    shaving informations chunk.                                              */
  DTS_HD_STRMDATA  = 0x5354524D44415441,  /**< DTS-HD Stream audio data
    payload chunk.                                                           */
  DTS_HD_TIMECODE  = 0x54494D45434F4445,  /**< Stream time-codes metadata
    chunk.                                                                   */
  DTS_HD_BUILDVER  = 0x4255494C44564552,  /**< DTS-HD Encoder Building
    Version description chunk.                                               */
  DTS_HD_BLACKOUT  = 0x424C41434B4F5554,  /**< DTS-HD Silence audio frame
    chunk.                                                                   */
  DTS_HD_BRANCHPT  = 0x4252414E43485054   /**< Branching points markers
    chunk.                                                                   */
} DtsHdChunkMagic;

/** \~english
 * \brief Supported DTS-HD file header Hdr_Version value.
 */
#define DTS_HD_HEADER_SUPPORTED_VER  0x0

static inline bool isNextQWORDDtshdHdr(
  BitstreamReaderPtr bs
)
{
  return DTS_HD_DTSHDHDR == nextUint64(bs);
}

static inline void cleanDtshdFileHandler(
  DtshdFileHandler hdl
)
{
  free(hdl.FILEINFO.text);
  free(hdl.AUPRINFO.text);
  free(hdl.BUILDVER.text);
  free(hdl.BLACKOUT.Blackout_Frame);
}

/** \~english
 * \brief Decode a DTS-HD file (.dtshd) chunk.
 *
 * \param dtsInput Input file.
 * \param handle DTS-HD file handle.
 * \return int On success, a zero is returned if a complete chunk as been
 * parsed. A '1' if decoded chunk is a DTS-HD Encoded Stream Data chunk,
 * followed by DTS audio frames that may be decoded. Otherwise (if error
 * occurs), a negative value is returned.
 *
 * Usage exemple:
 * \code{.c}
 * BitstreamReaderPtr file; // Input DTS-HD file
 * DtshdFileHandler *handle; // DTS-HD file handle
 * int ret; // Return value
 *
 * while (!isEof(file)) {
 *   if ((ret = decodeDtshdFileChunk(file, handle, false)) < 0)
 *     exit(-1); // Error
 *   if (!ret)
 *     continue; // Parse next DTS-HD chunk
 *
 *   // Decode DTS audio frame
 * }
 * \endcode
 */
int decodeDtshdFileChunk(
  BitstreamReaderPtr bs,
  DtshdFileHandler *handle
);

#endif