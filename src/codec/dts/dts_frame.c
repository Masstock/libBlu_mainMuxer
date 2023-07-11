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
    if (inner->replace)
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
  DtsAUCellPtr cell;

  assert(NULL != frm);

  if (frm->initializedCell)
    LIBBLU_DTS_ERROR_NRETURN("Attempt to double initialize a new AU cell.\n");

  if (frm->nbAllocatedContentCells <= frm->nbUsedContentCells + 1) {
    if (increaseContentCellsAllocationDtsAU(frm) < 0)
      return NULL;
  }

  cell = frm->contentCells + frm->nbUsedContentCells;
  cell->type = type;
  cell->skip = false;
  cell->replace = false;

  frm->initializedCell = true;

  return cell;
}

int replaceCurDtsAUCell(
  DtsAUFramePtr frm,
  DtsAUInnerReplacementParam param
)
{
  DtsAUCellPtr cell;

  if (NULL == (cell = recoverCurDtsAUCell(frm)))
    return -1;

  if (cell->replace)
    LIBBLU_DTS_ERROR_RETURN(
      "Replacement parameters already defined for current AU cell.\n"
    );

  if (frm->nbAllocatedReplacementParam <= frm->nbUsedReplacementParam) {
    if (increaseReplacementParamsAllocationDtsAU(frm) < 0)
      return -1;
  }

  frm->replacementParams[frm->nbUsedReplacementParam] = param;
  cell->param = frm->replacementParams + frm->nbUsedReplacementParam;

  frm->nbUsedReplacementParam++;
  cell->replace = true;

  return 0;
}

int discardCurDtsAUCell(
  DtsAUFramePtr frm
)
{
  DtsAUCellPtr cell;

  if (NULL == (cell = recoverCurDtsAUCell(frm)))
    return -1;

  if (cell->replace)
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
  unsigned i;
  DtsAUCell * destCell, * cell;

  destCell = NULL;
  for (i = 0; i < frm->nbUsedContentCells; i++) {
    cell = frm->contentCells + i;

    if (cell->replace) {
      /**
       * This cell is marked as replaced and so can't be used as destination
       * of a contiguous region and break current contiguous region.
       */
      destCell = NULL;
    }
    else {
      if (
        NULL == destCell
        || destCell->startOffset + destCell->length != cell->startOffset
      ) {
        /**
         * This cell is the first non-replaced one of potential contiguous
         * region or its start offset does not match the end offset of
         * the precedent one.
         * Setting it as a potential new contiguous region start.
         */
        destCell = cell;
      }
      else {
        /**
         * Start offset of this cell is contiguous with the end of
         * the precedent one.
         * Merging it with saved contiguous region start cell.
         */
        destCell->length += cell->length;
        cell->skip = true; /* Mark newly useless cell as skipped. */
      }
    }
  }
}

DtsAUContentType identifyContentTypeDtsAUFrame(
  DtsAUFramePtr frm
)
{
  unsigned i;
  bool coreSSPres;

  coreSSPres = false;
  for (i = 0; i < frm->nbUsedContentCells; i++) {
    switch (frm->contentCells[i].type) {
      case DTS_FRM_INNER_CORE_SS:
        coreSSPres = true;
        break;

      case DTS_FRM_INNER_EXT_SS_HDR:
        return DTS_AU_EXT_SS;

      default:
        break;
    }
  }

  return coreSSPres ? DTS_AU_CORE_SS : DTS_AU_EMPTY;
}

int processCompleteFrameDtsAUFrame(
  BitstreamWriterPtr output,
  EsmsFileHeaderPtr script,
  DtsAUFramePtr frm,
  unsigned srcFileScriptIdx,
  uint64_t framePts
)
{
  unsigned i;

  bool isExtFrame;

  size_t curInsertingOffset, writtenBytes;
  DtsAUCell * cell;

  if (frm->initializedCell)
    LIBBLU_DTS_ERROR_RETURN("Incomplete AU cell in pipeline.\n");

  if (!frm->nbUsedContentCells) {
    resetDtsAU(frm);
    return 0;
  }

  optimizeContiguousCellsDtsAUFrame(frm); /* Merge contiguous cells */

  switch (identifyContentTypeDtsAUFrame(frm)) {
    case DTS_AU_CORE_SS:
      isExtFrame = false;
      break;

    case DTS_AU_EXT_SS:
      isExtFrame = true;
      break;

    default:
      LIBBLU_DTS_ERROR_RETURN("Unexpected AU content.\n");
  }

  if (initEsmsAudioPesFrame(script, isExtFrame, false, framePts, 0) < 0)
    return -1;

  curInsertingOffset = 0;
  for (i = 0; i < frm->nbUsedContentCells; i++) {
    cell = frm->contentCells + i;

    if (cell->skip)
      continue;

    if (cell->replace) {
      switch (cell->type) {
        case DTS_FRM_INNER_EXT_SS_HDR:
          writtenBytes = appendDcaExtSSHeader(
            script,
            curInsertingOffset,
            &cell->param->extSSHdr
          );
          break;

        case DTS_FRM_INNER_EXT_SS_ASSET:
           writtenBytes = appendDcaExtSSAsset(
            script,
            curInsertingOffset,
            &cell->param->extSSAsset,
            srcFileScriptIdx
          );
          break;

        default:
          LIBBLU_DTS_ERROR_RETURN(
            "Unknown replacement for AU cell type code: %u.\n",
            cell->type
          );
      }

      if (!writtenBytes)
        return -1;

      curInsertingOffset += writtenBytes;
    }
    else {
      if (
        appendAddPesPayloadCommand(
          script, srcFileScriptIdx,
          (uint32_t) curInsertingOffset,
          cell->startOffset,
          cell->length
        ) < 0
      )
        return -1;

      curInsertingOffset += cell->length;
    }
  }

  if (writeEsmsPesPacket(output, script) < 0)
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