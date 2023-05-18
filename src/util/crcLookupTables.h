/** \~english
 * \file crcLookupTables.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief CRC computation look-up tables.
 *
 * \todo Fix TRUE_HD_MAJOR_SYNC_CRC_TABLE.
 */

#ifndef __LIBBLU_MUXER__UTIL__CRC_LOOKUP_TABLES_H__
#define __LIBBLU_MUXER__UTIL__CRC_LOOKUP_TABLES_H__

/* AC3 CRC-16 Look-up table */
static const uint32_t ac3CrcTable[256] = {
  0x0000, 0x0580, 0x0F80, 0x0A00, 0x1B80, 0x1E00, 0x1400, 0x1180,
  0x3380, 0x3600, 0x3C00, 0x3980, 0x2800, 0x2D80, 0x2780, 0x2200,
  0x6380, 0x6600, 0x6C00, 0x6980, 0x7800, 0x7D80, 0x7780, 0x7200,
  0x5000, 0x5580, 0x5F80, 0x5A00, 0x4B80, 0x4E00, 0x4400, 0x4180,
  0xC380, 0xC600, 0xCC00, 0xC980, 0xD800, 0xDD80, 0xD780, 0xD200,
  0xF000, 0xF580, 0xFF80, 0xFA00, 0xEB80, 0xEE00, 0xE400, 0xE180,
  0xA000, 0xA580, 0xAF80, 0xAA00, 0xBB80, 0xBE00, 0xB400, 0xB180,
  0x9380, 0x9600, 0x9C00, 0x9980, 0x8800, 0x8D80, 0x8780, 0x8200,
  0x8381, 0x8601, 0x8C01, 0x8981, 0x9801, 0x9D81, 0x9781, 0x9201,
  0xB001, 0xB581, 0xBF81, 0xBA01, 0xAB81, 0xAE01, 0xA401, 0xA181,
  0xE001, 0xE581, 0xEF81, 0xEA01, 0xFB81, 0xFE01, 0xF401, 0xF181,
  0xD381, 0xD601, 0xDC01, 0xD981, 0xC801, 0xCD81, 0xC781, 0xC201,
  0x4001, 0x4581, 0x4F81, 0x4A01, 0x5B81, 0x5E01, 0x5401, 0x5181,
  0x7381, 0x7601, 0x7C01, 0x7981, 0x6801, 0x6D81, 0x6781, 0x6201,
  0x2381, 0x2601, 0x2C01, 0x2981, 0x3801, 0x3D81, 0x3781, 0x3201,
  0x1001, 0x1581, 0x1F81, 0x1A01, 0x0B81, 0x0E01, 0x0401, 0x0181,
  0x0383, 0x0603, 0x0C03, 0x0983, 0x1803, 0x1D83, 0x1783, 0x1203,
  0x3003, 0x3583, 0x3F83, 0x3A03, 0x2B83, 0x2E03, 0x2403, 0x2183,
  0x6003, 0x6583, 0x6F83, 0x6A03, 0x7B83, 0x7E03, 0x7403, 0x7183,
  0x5383, 0x5603, 0x5C03, 0x5983, 0x4803, 0x4D83, 0x4783, 0x4203,
  0xC003, 0xC583, 0xCF83, 0xCA03, 0xDB83, 0xDE03, 0xD403, 0xD183,
  0xF383, 0xF603, 0xFC03, 0xF983, 0xE803, 0xED83, 0xE783, 0xE203,
  0xA383, 0xA603, 0xAC03, 0xA983, 0xB803, 0xBD83, 0xB783, 0xB203,
  0x9003, 0x9583, 0x9F83, 0x9A03, 0x8B83, 0x8E03, 0x8403, 0x8183,
  0x8002, 0x8582, 0x8F82, 0x8A02, 0x9B82, 0x9E02, 0x9402, 0x9182,
  0xB382, 0xB602, 0xBC02, 0xB982, 0xA802, 0xAD82, 0xA782, 0xA202,
  0xE382, 0xE602, 0xEC02, 0xE982, 0xF802, 0xFD82, 0xF782, 0xF202,
  0xD002, 0xD582, 0xDF82, 0xDA02, 0xCB82, 0xCE02, 0xC402, 0xC182,
  0x4382, 0x4602, 0x4C02, 0x4982, 0x5802, 0x5D82, 0x5782, 0x5202,
  0x7002, 0x7582, 0x7F82, 0x7A02, 0x6B82, 0x6E02, 0x6402, 0x6182,
  0x2002, 0x2582, 0x2F82, 0x2A02, 0x3B82, 0x3E02, 0x3402, 0x3182,
  0x1382, 0x1602, 0x1C02, 0x1982, 0x0802, 0x0D82, 0x0782, 0x0202
};

/* TRUE-HD Major Sync CRC-16 Look-up table */
#if 0
static const uint32_t trueHdMajorSyncCrcTable[256] = {
  0x0000, 0x002D, 0x005A, 0x0077, 0x00B4, 0x0099, 0x00EE, 0x00C3,
  0x0168, 0x0145, 0x0132, 0x011F, 0x01DC, 0x01F1, 0x0186, 0x01AB,
  0x02D0, 0x02FD, 0x028A, 0x02A7, 0x0264, 0x0249, 0x023E, 0x0213,
  0x03B8, 0x0395, 0x03E2, 0x03CF, 0x030C, 0x0321, 0x0356, 0x037B,
  0x05A0, 0x058D, 0x05FA, 0x05D7, 0x0514, 0x0539, 0x054E, 0x0563,
  0x04C8, 0x04E5, 0x0492, 0x04BF, 0x047C, 0x0451, 0x0426, 0x040B,
  0x0770, 0x075D, 0x072A, 0x0707, 0x07C4, 0x07E9, 0x079E, 0x07B3,
  0x0618, 0x0635, 0x0642, 0x066F, 0x06AC, 0x0681, 0x06F6, 0x06DB,
  0x0B40, 0x0B6D, 0x0B1A, 0x0B37, 0x0BF4, 0x0BD9, 0x0BAE, 0x0B83,
  0x0A28, 0x0A05, 0x0A72, 0x0A5F, 0x0A9C, 0x0AB1, 0x0AC6, 0x0AEB,
  0x0990, 0x09BD, 0x09CA, 0x09E7, 0x0924, 0x0909, 0x097E, 0x0953,
  0x08F8, 0x08D5, 0x08A2, 0x088F, 0x084C, 0x0861, 0x0816, 0x083B,
  0x0EE0, 0x0ECD, 0x0EBA, 0x0E97, 0x0E54, 0x0E79, 0x0E0E, 0x0E23,
  0x0F88, 0x0FA5, 0x0FD2, 0x0FFF, 0x0F3C, 0x0F11, 0x0F66, 0x0F4B,
  0x0C30, 0x0C1D, 0x0C6A, 0x0C47, 0x0C84, 0x0CA9, 0x0CDE, 0x0CF3,
  0x0D58, 0x0D75, 0x0D02, 0x0D2F, 0x0DEC, 0x0DC1, 0x0DB6, 0x0D9B,
  0x1680, 0x16AD, 0x16DA, 0x16F7, 0x1634, 0x1619, 0x166E, 0x1643,
  0x17E8, 0x17C5, 0x17B2, 0x179F, 0x175C, 0x1771, 0x1706, 0x172B,
  0x1450, 0x147D, 0x140A, 0x1427, 0x14E4, 0x14C9, 0x14BE, 0x1493,
  0x1538, 0x1515, 0x1562, 0x154F, 0x158C, 0x15A1, 0x15D6, 0x15FB,
  0x1320, 0x130D, 0x137A, 0x1357, 0x1394, 0x13B9, 0x13CE, 0x13E3,
  0x1248, 0x1265, 0x1212, 0x123F, 0x12FC, 0x12D1, 0x12A6, 0x128B,
  0x11F0, 0x11DD, 0x11AA, 0x1187, 0x1144, 0x1169, 0x111E, 0x1133,
  0x1098, 0x10B5, 0x10C2, 0x10EF, 0x102C, 0x1001, 0x1076, 0x105B,
  0x1DC0, 0x1DED, 0x1D9A, 0x1DB7, 0x1D74, 0x1D59, 0x1D2E, 0x1D03,
  0x1CA8, 0x1C85, 0x1CF2, 0x1CDF, 0x1C1C, 0x1C31, 0x1C46, 0x1C6B,
  0x1F10, 0x1F3D, 0x1F4A, 0x1F67, 0x1FA4, 0x1F89, 0x1FFE, 0x1FD3,
  0x1E78, 0x1E55, 0x1E22, 0x1E0F, 0x1ECC, 0x1EE1, 0x1E96, 0x1EBB,
  0x1860, 0x184D, 0x183A, 0x1817, 0x18D4, 0x18F9, 0x188E, 0x18A3,
  0x1908, 0x1925, 0x1952, 0x197F, 0x19BC, 0x1991, 0x19E6, 0x19CB,
  0x1AB0, 0x1A9D, 0x1AEA, 0x1AC7, 0x1A04, 0x1A29, 0x1A5E, 0x1A73,
  0x1BD8, 0x1BF5, 0x1B82, 0x1BAF, 0x1B6C, 0x1B41, 0x1B36, 0x1B1B
};
#endif

/* DTS Extension extSubstream CRC-16 Look-up table */
static const uint32_t dtsExtSSCrcTable[256] = {
  0x0000, 0x2110, 0x4220, 0x6330, 0x8440, 0xA550, 0xC660, 0xE770,
  0x0881, 0x2991, 0x4AA1, 0x6BB1, 0x8CC1, 0xADD1, 0xCEE1, 0xEFF1,
  0x3112, 0x1002, 0x7332, 0x5222, 0xB552, 0x9442, 0xF772, 0xD662,
  0x3993, 0x1883, 0x7BB3, 0x5AA3, 0xBDD3, 0x9CC3, 0xFFF3, 0xDEE3,
  0x6224, 0x4334, 0x2004, 0x0114, 0xE664, 0xC774, 0xA444, 0x8554,
  0x6AA5, 0x4BB5, 0x2885, 0x0995, 0xEEE5, 0xCFF5, 0xACC5, 0x8DD5,
  0x5336, 0x7226, 0x1116, 0x3006, 0xD776, 0xF666, 0x9556, 0xB446,
  0x5BB7, 0x7AA7, 0x1997, 0x3887, 0xDFF7, 0xFEE7, 0x9DD7, 0xBCC7,
  0xC448, 0xE558, 0x8668, 0xA778, 0x4008, 0x6118, 0x0228, 0x2338,
  0xCCC9, 0xEDD9, 0x8EE9, 0xAFF9, 0x4889, 0x6999, 0x0AA9, 0x2BB9,
  0xF55A, 0xD44A, 0xB77A, 0x966A, 0x711A, 0x500A, 0x333A, 0x122A,
  0xFDDB, 0xDCCB, 0xBFFB, 0x9EEB, 0x799B, 0x588B, 0x3BBB, 0x1AAB,
  0xA66C, 0x877C, 0xE44C, 0xC55C, 0x222C, 0x033C, 0x600C, 0x411C,
  0xAEED, 0x8FFD, 0xECCD, 0xCDDD, 0x2AAD, 0x0BBD, 0x688D, 0x499D,
  0x977E, 0xB66E, 0xD55E, 0xF44E, 0x133E, 0x322E, 0x511E, 0x700E,
  0x9FFF, 0xBEEF, 0xDDDF, 0xFCCF, 0x1BBF, 0x3AAF, 0x599F, 0x788F,
  0x8891, 0xA981, 0xCAB1, 0xEBA1, 0x0CD1, 0x2DC1, 0x4EF1, 0x6FE1,
  0x8010, 0xA100, 0xC230, 0xE320, 0x0450, 0x2540, 0x4670, 0x6760,
  0xB983, 0x9893, 0xFBA3, 0xDAB3, 0x3DC3, 0x1CD3, 0x7FE3, 0x5EF3,
  0xB102, 0x9012, 0xF322, 0xD232, 0x3542, 0x1452, 0x7762, 0x5672,
  0xEAB5, 0xCBA5, 0xA895, 0x8985, 0x6EF5, 0x4FE5, 0x2CD5, 0x0DC5,
  0xE234, 0xC324, 0xA014, 0x8104, 0x6674, 0x4764, 0x2454, 0x0544,
  0xDBA7, 0xFAB7, 0x9987, 0xB897, 0x5FE7, 0x7EF7, 0x1DC7, 0x3CD7,
  0xD326, 0xF236, 0x9106, 0xB016, 0x5766, 0x7676, 0x1546, 0x3456,
  0x4CD9, 0x6DC9, 0x0EF9, 0x2FE9, 0xC899, 0xE989, 0x8AB9, 0xABA9,
  0x4458, 0x6548, 0x0678, 0x2768, 0xC018, 0xE108, 0x8238, 0xA328,
  0x7DCB, 0x5CDB, 0x3FEB, 0x1EFB, 0xF98B, 0xD89B, 0xBBAB, 0x9ABB,
  0x754A, 0x545A, 0x376A, 0x167A, 0xF10A, 0xD01A, 0xB32A, 0x923A,
  0x2EFD, 0x0FED, 0x6CDD, 0x4DCD, 0xAABD, 0x8BAD, 0xE89D, 0xC98D,
  0x267C, 0x076C, 0x645C, 0x454C, 0xA23C, 0x832C, 0xE01C, 0xC10C,
  0x1FEF, 0x3EFF, 0x5DCF, 0x7CDF, 0x9BAF, 0xBABF, 0xD98F, 0xF89F,
  0x176E, 0x367E, 0x554E, 0x745E, 0x932E, 0xB23E, 0xD10E, 0xF01E
};

#endif