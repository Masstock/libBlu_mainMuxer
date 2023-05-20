/** \~english
 * \file crcLookupTables.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief CRC computation look-up tables.
 */

#ifndef __LIBBLU_MUXER__UTIL__CRC_LOOKUP_TABLES_H__
#define __LIBBLU_MUXER__UTIL__CRC_LOOKUP_TABLES_H__

extern const uint32_t ansi_crc_16_table[256];

/** \~english
 * \brief MLP/TrueHD Major Sync CRC lookup table.
 *
 * 16-bit CRC word, poly: 0x002D, initial value: 0x0000, shifted algorithm.
 */
extern const uint32_t mlp_ms_crc_16_table[256];

/** \~english
 * \brief MLP/TrueHD Restart Header CRC lookup table.
 *
 * 8-bit CRC word, poly: 0x1D, initial value: 0x00, shifted algorithm.
 */
extern const uint32_t mlp_rh_crc_8_table[256];

/** \~english
 * \brief CRC-CCITT lookup table.
 *
 * Used in DTS Extension Substream.
 * 16-bit CRC word, poly: 0x1021, initial value: 0xFFFF.
 */
extern const uint32_t ccitt_crc_16_table[256];

#endif