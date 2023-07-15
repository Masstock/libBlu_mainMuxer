/** \~english
 * \file dts_patcher.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams patching and modification module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__PATCHER_H__
#define __LIBBLU_MUXER__CODECS__DTS__PATCHER_H__

#include "dts_patcher_util.h"
#include "dts_data.h"

#define COMPUTE_NUBITS4MIXOUTMASK(res, masks, nbMasks)                        \
  do {                                                                        \
    unsigned i, max;                                                          \
    for (max = 0, i = 0; i < (nbMasks); i++)                                  \
      max = MAX(max, (masks)[i]);                                             \
    for (                                                                     \
      (res) = 4;                                                              \
      (res) < max;                                                            \
      (res) += 4                                                              \
    )                                                                         \
      ;                                                                       \
  } while(0)

size_t computeDcaExtSSHeaderMixMetadataSize(
  const DcaExtSSHeaderMixMetadataParameters * param
);

size_t computeDcaExtSSHeaderStaticFieldsSize(
  const DcaExtSSHeaderStaticFieldsParameters * param,
  const uint8_t nExtSSIndex
);

int buildDcaExtSSHeaderStaticFields(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaExtSSHeaderStaticFieldsParameters * param,
  const uint8_t nExtSSIndex
);

#define COMPUTE_NUNUMBITS4SAMASK(res, mask)                                   \
  do {                                                                        \
    for (                                                                     \
      (res) = 4;                                                              \
      (res) < lb_fast_log2_32(mask);                                              \
      (res) += 4                                                              \
    )                                                                         \
      ;                                                                       \
  } while(0)

size_t computeDcaExtSSAssetDescriptorStaticFieldsSize(
  const DcaAudioAssetDescSFParameters * param
);

int buildDcaExtSSAssetDescriptorStaticFields(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescSFParameters * param
);

/** \~english
 * \brief Return the length in bits of the dynamic metadata section fields of
 * Extension Substream Asset Descriptor based on given parameters.
 *
 * \param param Ext SS Asset Descriptor dynamic metadata fields parameters.
 * \param assetStaticFieldsParam Ext SS Asset Descriptor static fields
 * parameters.
 * \param staticFieldsParam Ext SS Static Fields parameters. Can be NULL,
 * if so Static Fields are considered as absent.
 * \return size_t Computed length in bits.
 */
size_t computeDcaExtSSAssetDescriptorDynamicMetadataSize(
  const DcaAudioAssetDescDMParameters * param,
  const DcaAudioAssetDescSFParameters * assetStaticFieldsParam,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam
);

int buildDcaExtSSAssetDescriptorDynamicMetadata(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescDMParameters * param,
  const DcaAudioAssetDescSFParameters * assetStaticFieldsParam,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam
);

#define COMPUTE_NUBITSINITDECDLY(res, delayValue)                             \
  do {                                                                        \
    (res) = MAX(                                                              \
      1,                                                                      \
      lb_fast_log2_32(delayValue)                                                 \
    );                                                                        \
  } while(0)

/** \~english
 * \brief Return the length in bits of the decoder navigation data section
 * fields of Extension Substream Asset Descriptor based on given parameters.
 *
 * \param param Ext SS Asset Descriptor decoder navigation data fields
 * parameters.
 * \return size_t Computed length in bits.
 */
size_t computeDcaExtSSAssetDescriptorDecNavDataSize(
  const DcaAudioAssetDescDecNDParameters * param,
  const DcaAudioAssetDescSFParameters * assetStaticFieldsParam,
  const DcaAudioAssetDescDMParameters * dynMetadataParam,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  const size_t nuBits4ExSSFsize
);

int buildDcaExtSSAssetDescriptorDecNavData(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescDecNDParameters * param,
  const DcaAudioAssetDescSFParameters * assetStaticFieldsParam,
  const DcaAudioAssetDescDMParameters * dynMetadataParam,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  const size_t nuBits4ExSSFsize
);

/** \~english
 * \brief Return the length in bytes of the Extension Substream Asset
 * Descriptor based on given parameters.
 *
 * \param param Ext SS Asset Descriptor parameters.
 * \param staticFieldsParam Ext SS Static Fields parameters. Can be NULL, if
 * so Static Fields are considered as absent.
 * \param nuBits4ExSSFsize 'nuBits4ExSSFsize' value, the size of Ext SS frame
 * size fields. Defined by 'bHeaderSizeType' field in Ext SS Header.
 * \return size_t Computed length in bytes.
 */
size_t computeDcaExtSSAudioAssetDescriptorSize(
  const DcaAudioAssetDescParameters * param,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  const size_t nuBits4ExSSFsize
);

/** \~english
 * \brief Build an Extension Substream Audio Asset Descriptor based on given
 * parameters.
 *
 * \param handle Destination Bitstream builder handle.
 * \param param Ext SS Asset Descriptor parameters.
 * \param staticFieldsParam Ext SS Static Fields parameters. Can be NULL, if
 * so Static Fields are considered as absent.
 * \param nuBits4ExSSFsize 'nuBits4ExSSFsize' value, the size of Ext SS frame
 * size fields. Defined by 'bHeaderSizeType' field in Ext SS Header.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int buildDcaExtSSAudioAssetDescriptor(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescParameters * param,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  const size_t nuBits4ExSSFsize,
  const size_t descriptorSize
);

/** \~english
 * \brief Return the length in bytes of the Extension Substream header based
 * on given parameters.
 *
 * \param param Ext SS parameters.
 * \param assetDescsSizes Optionnal assets descriptors sizes. Avoid
 * re-computation if already done. Can be NULL, disabling this functionnality.
 * The size of the array must be equal to the specified number of assets in
 * given Ext SS parameters.
 * \return size_t Computed length in bytes.
 */
size_t computeDcaExtSSHeaderSize(
  const DcaExtSSHeaderParameters * param,
  const size_t * assetDescsSizes
);

/** \~english
 * \brief Append on given ESMS script handle a reconstruction of the Extension
 * Substream header based on given parameters.
 *
 * \param script Destination ESMS script handle.
 * \param insertingOffset Script frame insertion offset in bytes.
 * \param param Ext SS parameters.
 * \return size_t Number of bytes written.
 */
size_t appendDcaExtSSHeader(
  EsmsFileHeaderPtr script,
  const size_t insertingOffset,
  const DcaExtSSHeaderParameters * param
);

size_t appendDcaExtSSAsset(
  EsmsFileHeaderPtr script,
  const size_t insertingOffset,
  const DcaXllFrameSFPosition * param,
  const unsigned scriptSourceFileId
);

#endif