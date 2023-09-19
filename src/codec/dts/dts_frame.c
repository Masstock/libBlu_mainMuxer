#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "dts_frame.h"

static void resetDtsAU(
  DtsAUFramePtr frm
)
{
  frm->nbUsedContentCells = 0;
  frm->initializedCell = false;
  frm->nbUsedReplacementParam = 0;
}

static int increaseContentCellsAllocationDtsAU(
  DtsAUFramePtr frm
)
{
  unsigned newLength;
  DtsAUCell * newArray;

  newLength = GROW_ALLOCATION(frm->nbAllocatedContentCells, 2);
  if (newLength <= frm->nbAllocatedContentCells)
    LIBBLU_DTS_ERROR_RETURN("Too many content in DTS frame.\n");

  newArray = (DtsAUCell *) realloc(
    frm->contentCells,
    newLength * sizeof(DtsAUCell)
  );
  if (NULL == newArray)
    LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

  frm->contentCells = newArray;
  frm->nbAllocatedContentCells = newLength;

  return 0;
}

static int increaseReplacementParamsAllocationDtsAU(
  DtsAUFramePtr frm
)
{
  unsigned newLength, i;
  DtsAUInnerReplacementParam * newArray;
  DtsAUCell * inner;

  newLength = GROW_ALLOCATION(frm->nbAllocatedReplacementParam, 2);
  if (newLength <= frm->nbAllocatedReplacementParam)
    LIBBLU_DTS_ERROR_RETURN("Too many content in DTS frame.\n");

  newArray = (DtsAUInnerReplacementParam *) realloc(
    frm->replacementParams,
    newLength * sizeof(DtsAUInnerReplacementParam)
  );
  if (NULL == newArray)
    LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

  /* Update links */
  for (i = 0; i < frm->nbUsedContentCells; i++) {
    inner = frm->contentCells + i;
    if (DTS_AU_REPLACE == inner->treatment)
      inner->param = newArray + (inner->param - frm->replacementParams);
  }

  frm->replacementParams = newArray;
  frm->nbAllocatedReplacementParam = newLength;

  return 0;
}

DtsAUCellPtr initDtsAUCell(
  DtsAUFramePtr frm,
  DtsAUInnerType type
)
{
  assert(NULL != frm);

  if (frm->initializedCell)
    LIBBLU_DTS_ERROR_NRETURN("Attempt to double initialize a new AU cell.\n");

  if (frm->nbAllocatedContentCells <= frm->nbUsedContentCells + 1) {
    if (increaseContentCellsAllocationDtsAU(frm) < 0)
      return NULL;
  }

  DtsAUCellPtr cell = &frm->contentCells[frm->nbUsedContentCells];
  cell->type = type;
  cell->treatment = DTS_AU_KEEP;

  frm->initializedCell = true;

  return cell;
}

uint32_t getSizeDtsAUFrame(
  const DtsAUFramePtr frm
)
{

  uint32_t size = 0;
  for (unsigned i = 0; i < frm->nbUsedContentCells; i++) {
    const DtsAUCell * cell = &frm->contentCells[i];
    assert(DTS_AU_REPLACE != cell->treatment); // Shall not be used after mods.
    if (DTS_AU_KEEP == cell->treatment)
      size += cell->size;
  }

  return size;
}

int replaceCurDtsAUCell(
  DtsAUFramePtr frm,
  DtsAUInnerReplacementParam param
)
{
  DtsAUCellPtr cell;

  if (NULL == (cell = recoverCurDtsAUCell(frm)))
    return -1;

  if (DTS_AU_KEEP != cell->treatment)
    LIBBLU_DTS_ERROR_RETURN(
      "Replacement parameters already defined for current AU cell "
      "or marked as skipped.\n"
    );

  if (frm->nbAllocatedReplacementParam <= frm->nbUsedReplacementParam) {
    if (increaseReplacementParamsAllocationDtsAU(frm) < 0)
      return -1;
  }

  frm->replacementParams[frm->nbUsedReplacementParam] = param;

  cell->treatment = DTS_AU_REPLACE;
  cell->param = &frm->replacementParams[frm->nbUsedReplacementParam++];

  return 0;
}

int discardCurDtsAUCell(
  DtsAUFramePtr frm
)
{
  DtsAUCellPtr cell;

  if (NULL == (cell = recoverCurDtsAUCell(frm)))
    return -1;

  if (DTS_AU_REPLACE == cell->treatment)
    frm->nbUsedReplacementParam--;
  frm->initializedCell = false;

  return 0;
}

int addCurCellToDtsAUFrame(
  DtsAUFramePtr frm
)
{
  if (!frm->initializedCell)
    LIBBLU_DTS_ERROR_RETURN("Nothing to add, AU cell never initialized.\n");

  frm->nbUsedContentCells++;
  frm->initializedCell = false;

  return 0;
}

/** \~english
 * \brief Merge contiguous frame cells from pending access unit.
 *
 * \param frm Frame builder.
 */
static void optimizeContiguousCellsDtsAUFrame(
  DtsAUFramePtr frm
)
{
  DtsAUCell * dst = NULL;
  for (unsigned i = 0; i < frm->nbUsedContentCells; i++) {
    DtsAUCell * src = frm->contentCells + i;

    if (DTS_AU_REPLACE == src->treatment) {
      /**
       * This cell is marked as replaced and so can't be used as destination
       * of a contiguous region and break current contiguous region.
       */
      dst = NULL;
    }
    else {
      if (NULL == dst || dst->offset + dst->size != src->offset) {
        /**
         * This cell is the first non-replaced one of potential contiguous
         * region or its start offset does not match the end offset of
         * the precedent one.
         * Setting it as a potential new contiguous region start.
         */
        dst = src;
      }
      else {
        /**
         * Start offset of this cell is contiguous with the end of
         * the precedent one.
         * Merging it with saved contiguous region start cell.
         */
        dst->size += src->size;
        src->treatment = DTS_AU_SKIP; // Mark newly useless cell as skipped.
      }
    }
  }
}

DtsAUContentType identifyContentTypeDtsAUFrame(
  DtsAUFramePtr frm
)
{

  bool core_ss_present = false;
  for (unsigned i = 0; i < frm->nbUsedContentCells; i++) {
    switch (frm->contentCells[i].type) {
      case DTS_FRM_INNER_CORE_SS:
        core_ss_present = true;
        break;

      case DTS_FRM_INNER_EXT_SS_HDR:
        return DTS_AU_EXT_SS;

      default:
        break;
    }
  }

  return core_ss_present ? DTS_AU_CORE_SS : DTS_AU_EMPTY;
}

static int _appendReplacementParameters(
  DtsAUCell * cell,
  EsmsHandlerPtr script,
  uint32_t cur_off,
  unsigned src_file_idx
)
{
  const DtsAUInnerReplacementParam * param = cell->param;

  switch (cell->type) {
    case DTS_FRM_INNER_EXT_SS_HDR:
      return appendDcaExtSSHeader(script, cur_off, &param->ext_ss_hdr);
    case DTS_FRM_INNER_EXT_SS_ASSET:
      return appendDcaExtSSAsset(script, cur_off, &param->ext_ss_asset, src_file_idx);
    default:
      LIBBLU_DTS_ERROR_RETURN(
        "Unknown replacement for AU cell type code: %u.\n",
        cell->type
      );
  }
}

int processCompleteFrameDtsAUFrame(
  BitstreamWriterPtr output,
  EsmsHandlerPtr script,
  DtsAUFramePtr frm,
  unsigned src_file_idx,
  uint64_t pts
)
{
  if (frm->initializedCell)
    LIBBLU_DTS_ERROR_RETURN("Incomplete AU cell in pipeline.\n");

  LIBBLU_DTS_DEBUG("Processing complete DTS Access Unit.\n");

  if (!frm->nbUsedContentCells) {
    LIBBLU_DTS_DEBUG(" Empty AU, reset.\n");
    resetDtsAU(frm);
    return 0;
  }

  optimizeContiguousCellsDtsAUFrame(frm); /* Merge contiguous cells */

  bool is_ext;
  switch (identifyContentTypeDtsAUFrame(frm)) {
    case DTS_AU_CORE_SS:
      LIBBLU_DTS_DEBUG(" AU type: Core substream.\n");
      is_ext = false;
      break;
    case DTS_AU_EXT_SS:
      LIBBLU_DTS_DEBUG(" AU type: Extension substream.\n");
      is_ext = true;
      break;
    default:
      LIBBLU_DTS_ERROR_RETURN("Unexpected AU content.\n");
  }

  if (initAudioPesPacketEsmsHandler(script, is_ext, false, pts, 0) < 0)
    return -1;

  LIBBLU_DTS_DEBUG(" Adding %u cells:\n", frm->nbUsedContentCells);

  uint32_t cur_off = 0;
  for (unsigned i = 0; i < frm->nbUsedContentCells; i++) {
    DtsAUCell * cell = &frm->contentCells[i];
    LIBBLU_DTS_DEBUG(
      "  Cell %u (%s, %s):\n",
      i,
      DtsAUInnerTypeStr(cell->type),
      DtsAUCellTreatmentStr(cell->treatment)
    );

    size_t added_size = 0;
    switch (cell->treatment) {
      case DTS_AU_KEEP: {
        int ret = appendAddPesPayloadCommandEsmsHandler(
          script, src_file_idx, cur_off, cell->offset, cell->size
        );
        if (0 <= ret)
          added_size = cell->size; // Only set if successfull
        break;
      }

      case DTS_AU_SKIP:
        continue; // Skip, do nothing.

      case DTS_AU_REPLACE: {
        LIBBLU_DTS_DEBUG("   Replacement parameters:\n");
        added_size = _appendReplacementParameters(
          cell, script, cur_off, src_file_idx
        );
      }
    }

    if (!added_size)
      return -1; // Unable to add, error.
    LIBBLU_DTS_DEBUG("   Cell size: %zu byte(s).\n", added_size);
    cur_off += added_size;
  }

  if (writePesPacketEsmsHandler(output, script) < 0)
    return -1;

  resetDtsAU(frm);
  return 0;
}

void discardWholeCurDtsAUFrame(
  DtsAUFramePtr frm
)
{
  resetDtsAU(frm);
}