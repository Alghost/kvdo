/*
 * Copyright (c) 2017 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA. 
 */

#include "dedupeIndex.h"
#include "logger.h"
#include "poolSysfs.h"
#include "statistics.h"
#include "statusProcfs.h"
#include "threadDevice.h"
#include "vdo.h"

typedef struct poolStatsAttribute {
  struct attribute attr;
  ssize_t (*show)(KernelLayer *layer, char *buf);
} PoolStatsAttribute;

static ssize_t poolStatsAttrShow(struct kobject   *kobj,
                                 struct attribute *attr,
                                 char             *buf)
{
  PoolStatsAttribute *poolStatsAttr = container_of(attr, PoolStatsAttribute,
                                                   attr);

  if (poolStatsAttr->show == NULL) {
    return -EINVAL;
  }
  KernelLayer *layer = container_of(kobj, KernelLayer, statsDirectory);
  return poolStatsAttr->show(layer, buf);
}

static struct sysfs_ops poolStatsSysfsOps = {
  .show  = poolStatsAttrShow,
  .store = NULL,
};

/**********************************************************************/
/** Number of blocks used for data */
static ssize_t poolStatsDataBlocksUsedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->dataBlocksUsed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsDataBlocksUsedAttr = {
  .attr  = { .name = "data_blocks_used", .mode = 0444, },
  .show  = poolStatsDataBlocksUsedShow,
};

/**********************************************************************/
/** Number of blocks used for VDO metadata */
static ssize_t poolStatsOverheadBlocksUsedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->overheadBlocksUsed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsOverheadBlocksUsedAttr = {
  .attr  = { .name = "overhead_blocks_used", .mode = 0444, },
  .show  = poolStatsOverheadBlocksUsedShow,
};

/**********************************************************************/
/** Number of logical blocks that are currently mapped to physical blocks */
static ssize_t poolStatsLogicalBlocksUsedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->logicalBlocksUsed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsLogicalBlocksUsedAttr = {
  .attr  = { .name = "logical_blocks_used", .mode = 0444, },
  .show  = poolStatsLogicalBlocksUsedShow,
};

/**********************************************************************/
/** number of physical blocks */
static ssize_t poolStatsPhysicalBlocksShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->physicalBlocks);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsPhysicalBlocksAttr = {
  .attr  = { .name = "physical_blocks", .mode = 0444, },
  .show  = poolStatsPhysicalBlocksShow,
};

/**********************************************************************/
/** number of logical blocks */
static ssize_t poolStatsLogicalBlocksShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->logicalBlocks);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsLogicalBlocksAttr = {
  .attr  = { .name = "logical_blocks", .mode = 0444, },
  .show  = poolStatsLogicalBlocksShow,
};

/**********************************************************************/
/** Size of the block map page cache, in bytes */
static ssize_t poolStatsBlockMapCacheSizeShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMapCacheSize);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapCacheSizeAttr = {
  .attr  = { .name = "block_map_cache_size", .mode = 0444, },
  .show  = poolStatsBlockMapCacheSizeShow,
};

/**********************************************************************/
/** String describing the active write policy of the VDO */
static ssize_t poolStatsWritePolicyShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%s\n", layer->vdoStatsStorage->writePolicy);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsWritePolicyAttr = {
  .attr  = { .name = "write_policy", .mode = 0444, },
  .show  = poolStatsWritePolicyShow,
};

/**********************************************************************/
/** The physical block size */
static ssize_t poolStatsBlockSizeShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockSize);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockSizeAttr = {
  .attr  = { .name = "block_size", .mode = 0444, },
  .show  = poolStatsBlockSizeShow,
};

/**********************************************************************/
/** Number of times the VDO has successfully recovered */
static ssize_t poolStatsCompleteRecoveriesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->completeRecoveries);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsCompleteRecoveriesAttr = {
  .attr  = { .name = "complete_recoveries", .mode = 0444, },
  .show  = poolStatsCompleteRecoveriesShow,
};

/**********************************************************************/
/** Number of times the VDO has recovered from read-only mode */
static ssize_t poolStatsReadOnlyRecoveriesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->readOnlyRecoveries);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsReadOnlyRecoveriesAttr = {
  .attr  = { .name = "read_only_recoveries", .mode = 0444, },
  .show  = poolStatsReadOnlyRecoveriesShow,
};

/**********************************************************************/
/** String describing the operating mode of the VDO */
static ssize_t poolStatsModeShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%s\n", layer->vdoStatsStorage->mode);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsModeAttr = {
  .attr  = { .name = "mode", .mode = 0444, },
  .show  = poolStatsModeShow,
};

/**********************************************************************/
/** Whether the VDO is in recovery mode */
static ssize_t poolStatsInRecoveryModeShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%d\n", layer->vdoStatsStorage->inRecoveryMode);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsInRecoveryModeAttr = {
  .attr  = { .name = "in_recovery_mode", .mode = 0444, },
  .show  = poolStatsInRecoveryModeShow,
};

/**********************************************************************/
/** What percentage of recovery mode work has been completed */
static ssize_t poolStatsRecoveryPercentageShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%u\n", layer->vdoStatsStorage->recoveryPercentage);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsRecoveryPercentageAttr = {
  .attr  = { .name = "recovery_percentage", .mode = 0444, },
  .show  = poolStatsRecoveryPercentageShow,
};

/**********************************************************************/
/** Number of compressed data items written since startup */
static ssize_t poolStatsPackerCompressedFragmentsWrittenShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->packer.compressedFragmentsWritten);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsPackerCompressedFragmentsWrittenAttr = {
  .attr  = { .name = "packer_compressed_fragments_written", .mode = 0444, },
  .show  = poolStatsPackerCompressedFragmentsWrittenShow,
};

/**********************************************************************/
/** Number of blocks containing compressed items written since startup */
static ssize_t poolStatsPackerCompressedBlocksWrittenShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->packer.compressedBlocksWritten);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsPackerCompressedBlocksWrittenAttr = {
  .attr  = { .name = "packer_compressed_blocks_written", .mode = 0444, },
  .show  = poolStatsPackerCompressedBlocksWrittenShow,
};

/**********************************************************************/
/** Number of VIOs that are pending in the packer */
static ssize_t poolStatsPackerCompressedFragmentsInPackerShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->packer.compressedFragmentsInPacker);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsPackerCompressedFragmentsInPackerAttr = {
  .attr  = { .name = "packer_compressed_fragments_in_packer", .mode = 0444, },
  .show  = poolStatsPackerCompressedFragmentsInPackerShow,
};

/**********************************************************************/
/** The total number of slabs from which blocks may be allocated */
static ssize_t poolStatsAllocatorSlabCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->allocator.slabCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsAllocatorSlabCountAttr = {
  .attr  = { .name = "allocator_slab_count", .mode = 0444, },
  .show  = poolStatsAllocatorSlabCountShow,
};

/**********************************************************************/
/** The total number of slabs from which blocks have ever been allocated */
static ssize_t poolStatsAllocatorSlabsOpenedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->allocator.slabsOpened);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsAllocatorSlabsOpenedAttr = {
  .attr  = { .name = "allocator_slabs_opened", .mode = 0444, },
  .show  = poolStatsAllocatorSlabsOpenedShow,
};

/**********************************************************************/
/** The number of times since loading that a slab has been re-opened */
static ssize_t poolStatsAllocatorSlabsReopenedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->allocator.slabsReopened);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsAllocatorSlabsReopenedAttr = {
  .attr  = { .name = "allocator_slabs_reopened", .mode = 0444, },
  .show  = poolStatsAllocatorSlabsReopenedShow,
};

/**********************************************************************/
/** Number of times the on-disk journal was full */
static ssize_t poolStatsJournalDiskFullShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->journal.diskFull);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsJournalDiskFullAttr = {
  .attr  = { .name = "journal_disk_full", .mode = 0444, },
  .show  = poolStatsJournalDiskFullShow,
};

/**********************************************************************/
/** Number of times the recovery journal requested slab journal commits. */
static ssize_t poolStatsJournalSlabJournalCommitsRequestedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->journal.slabJournalCommitsRequested);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsJournalSlabJournalCommitsRequestedAttr = {
  .attr  = { .name = "journal_slab_journal_commits_requested", .mode = 0444, },
  .show  = poolStatsJournalSlabJournalCommitsRequestedShow,
};

/**********************************************************************/
/** The total number of items on which processing has started */
static ssize_t poolStatsJournalEntriesStartedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->journal.entries.started);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsJournalEntriesStartedAttr = {
  .attr  = { .name = "journal_entries_started", .mode = 0444, },
  .show  = poolStatsJournalEntriesStartedShow,
};

/**********************************************************************/
/** The total number of items for which a write operation has been issued */
static ssize_t poolStatsJournalEntriesWrittenShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->journal.entries.written);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsJournalEntriesWrittenAttr = {
  .attr  = { .name = "journal_entries_written", .mode = 0444, },
  .show  = poolStatsJournalEntriesWrittenShow,
};

/**********************************************************************/
/** The total number of items for which a write operation has completed */
static ssize_t poolStatsJournalEntriesCommittedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->journal.entries.committed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsJournalEntriesCommittedAttr = {
  .attr  = { .name = "journal_entries_committed", .mode = 0444, },
  .show  = poolStatsJournalEntriesCommittedShow,
};

/**********************************************************************/
/** The total number of items on which processing has started */
static ssize_t poolStatsJournalBlocksStartedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->journal.blocks.started);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsJournalBlocksStartedAttr = {
  .attr  = { .name = "journal_blocks_started", .mode = 0444, },
  .show  = poolStatsJournalBlocksStartedShow,
};

/**********************************************************************/
/** The total number of items for which a write operation has been issued */
static ssize_t poolStatsJournalBlocksWrittenShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->journal.blocks.written);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsJournalBlocksWrittenAttr = {
  .attr  = { .name = "journal_blocks_written", .mode = 0444, },
  .show  = poolStatsJournalBlocksWrittenShow,
};

/**********************************************************************/
/** The total number of items for which a write operation has completed */
static ssize_t poolStatsJournalBlocksCommittedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->journal.blocks.committed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsJournalBlocksCommittedAttr = {
  .attr  = { .name = "journal_blocks_committed", .mode = 0444, },
  .show  = poolStatsJournalBlocksCommittedShow,
};

/**********************************************************************/
/** Number of times the on-disk journal was full */
static ssize_t poolStatsSlabJournalDiskFullCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->slabJournal.diskFullCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsSlabJournalDiskFullCountAttr = {
  .attr  = { .name = "slab_journal_disk_full_count", .mode = 0444, },
  .show  = poolStatsSlabJournalDiskFullCountShow,
};

/**********************************************************************/
/** Number of times an entry was added over the flush threshold */
static ssize_t poolStatsSlabJournalFlushCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->slabJournal.flushCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsSlabJournalFlushCountAttr = {
  .attr  = { .name = "slab_journal_flush_count", .mode = 0444, },
  .show  = poolStatsSlabJournalFlushCountShow,
};

/**********************************************************************/
/** Number of times an entry was added over the block threshold */
static ssize_t poolStatsSlabJournalBlockedCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->slabJournal.blockedCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsSlabJournalBlockedCountAttr = {
  .attr  = { .name = "slab_journal_blocked_count", .mode = 0444, },
  .show  = poolStatsSlabJournalBlockedCountShow,
};

/**********************************************************************/
/** Number of times a tail block was written */
static ssize_t poolStatsSlabJournalBlocksWrittenShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->slabJournal.blocksWritten);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsSlabJournalBlocksWrittenAttr = {
  .attr  = { .name = "slab_journal_blocks_written", .mode = 0444, },
  .show  = poolStatsSlabJournalBlocksWrittenShow,
};

/**********************************************************************/
/** Number of times we had to wait for the tail to write */
static ssize_t poolStatsSlabJournalTailBusyCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->slabJournal.tailBusyCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsSlabJournalTailBusyCountAttr = {
  .attr  = { .name = "slab_journal_tail_busy_count", .mode = 0444, },
  .show  = poolStatsSlabJournalTailBusyCountShow,
};

/**********************************************************************/
/** Number of blocks written */
static ssize_t poolStatsSlabSummaryBlocksWrittenShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->slabSummary.blocksWritten);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsSlabSummaryBlocksWrittenAttr = {
  .attr  = { .name = "slab_summary_blocks_written", .mode = 0444, },
  .show  = poolStatsSlabSummaryBlocksWrittenShow,
};

/**********************************************************************/
/** Number of reference blocks written */
static ssize_t poolStatsRefCountsBlocksWrittenShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->refCounts.blocksWritten);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsRefCountsBlocksWrittenAttr = {
  .attr  = { .name = "ref_counts_blocks_written", .mode = 0444, },
  .show  = poolStatsRefCountsBlocksWrittenShow,
};

/**********************************************************************/
/** number of dirty (resident) pages */
static ssize_t poolStatsBlockMapDirtyPagesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->vdoStatsStorage->blockMap.dirtyPages);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapDirtyPagesAttr = {
  .attr  = { .name = "block_map_dirty_pages", .mode = 0444, },
  .show  = poolStatsBlockMapDirtyPagesShow,
};

/**********************************************************************/
/** number of clean (resident) pages */
static ssize_t poolStatsBlockMapCleanPagesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->vdoStatsStorage->blockMap.cleanPages);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapCleanPagesAttr = {
  .attr  = { .name = "block_map_clean_pages", .mode = 0444, },
  .show  = poolStatsBlockMapCleanPagesShow,
};

/**********************************************************************/
/** number of free pages */
static ssize_t poolStatsBlockMapFreePagesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->vdoStatsStorage->blockMap.freePages);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapFreePagesAttr = {
  .attr  = { .name = "block_map_free_pages", .mode = 0444, },
  .show  = poolStatsBlockMapFreePagesShow,
};

/**********************************************************************/
/** number of pages in failed state */
static ssize_t poolStatsBlockMapFailedPagesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->vdoStatsStorage->blockMap.failedPages);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapFailedPagesAttr = {
  .attr  = { .name = "block_map_failed_pages", .mode = 0444, },
  .show  = poolStatsBlockMapFailedPagesShow,
};

/**********************************************************************/
/** number of pages incoming */
static ssize_t poolStatsBlockMapIncomingPagesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->vdoStatsStorage->blockMap.incomingPages);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapIncomingPagesAttr = {
  .attr  = { .name = "block_map_incoming_pages", .mode = 0444, },
  .show  = poolStatsBlockMapIncomingPagesShow,
};

/**********************************************************************/
/** number of pages outgoing */
static ssize_t poolStatsBlockMapOutgoingPagesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->vdoStatsStorage->blockMap.outgoingPages);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapOutgoingPagesAttr = {
  .attr  = { .name = "block_map_outgoing_pages", .mode = 0444, },
  .show  = poolStatsBlockMapOutgoingPagesShow,
};

/**********************************************************************/
/** how many times free page not avail */
static ssize_t poolStatsBlockMapCachePressureShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->vdoStatsStorage->blockMap.cachePressure);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapCachePressureAttr = {
  .attr  = { .name = "block_map_cache_pressure", .mode = 0444, },
  .show  = poolStatsBlockMapCachePressureShow,
};

/**********************************************************************/
/** number of getVDOPageAsync() for read */
static ssize_t poolStatsBlockMapReadCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.readCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapReadCountAttr = {
  .attr  = { .name = "block_map_read_count", .mode = 0444, },
  .show  = poolStatsBlockMapReadCountShow,
};

/**********************************************************************/
/** number or getVDOPageAsync() for write */
static ssize_t poolStatsBlockMapWriteCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.writeCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapWriteCountAttr = {
  .attr  = { .name = "block_map_write_count", .mode = 0444, },
  .show  = poolStatsBlockMapWriteCountShow,
};

/**********************************************************************/
/** number of times pages failed to read */
static ssize_t poolStatsBlockMapFailedReadsShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.failedReads);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapFailedReadsAttr = {
  .attr  = { .name = "block_map_failed_reads", .mode = 0444, },
  .show  = poolStatsBlockMapFailedReadsShow,
};

/**********************************************************************/
/** number of times pages failed to write */
static ssize_t poolStatsBlockMapFailedWritesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.failedWrites);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapFailedWritesAttr = {
  .attr  = { .name = "block_map_failed_writes", .mode = 0444, },
  .show  = poolStatsBlockMapFailedWritesShow,
};

/**********************************************************************/
/** number of gets that are reclaimed */
static ssize_t poolStatsBlockMapReclaimedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.reclaimed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapReclaimedAttr = {
  .attr  = { .name = "block_map_reclaimed", .mode = 0444, },
  .show  = poolStatsBlockMapReclaimedShow,
};

/**********************************************************************/
/** number of gets for outgoing pages */
static ssize_t poolStatsBlockMapReadOutgoingShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.readOutgoing);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapReadOutgoingAttr = {
  .attr  = { .name = "block_map_read_outgoing", .mode = 0444, },
  .show  = poolStatsBlockMapReadOutgoingShow,
};

/**********************************************************************/
/** number of gets that were already there */
static ssize_t poolStatsBlockMapFoundInCacheShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.foundInCache);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapFoundInCacheAttr = {
  .attr  = { .name = "block_map_found_in_cache", .mode = 0444, },
  .show  = poolStatsBlockMapFoundInCacheShow,
};

/**********************************************************************/
/** number of gets requiring discard */
static ssize_t poolStatsBlockMapDiscardRequiredShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.discardRequired);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapDiscardRequiredAttr = {
  .attr  = { .name = "block_map_discard_required", .mode = 0444, },
  .show  = poolStatsBlockMapDiscardRequiredShow,
};

/**********************************************************************/
/** number of gets enqueued for their page */
static ssize_t poolStatsBlockMapWaitForPageShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.waitForPage);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapWaitForPageAttr = {
  .attr  = { .name = "block_map_wait_for_page", .mode = 0444, },
  .show  = poolStatsBlockMapWaitForPageShow,
};

/**********************************************************************/
/** number of gets that have to fetch */
static ssize_t poolStatsBlockMapFetchRequiredShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.fetchRequired);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapFetchRequiredAttr = {
  .attr  = { .name = "block_map_fetch_required", .mode = 0444, },
  .show  = poolStatsBlockMapFetchRequiredShow,
};

/**********************************************************************/
/** number of page fetches */
static ssize_t poolStatsBlockMapPagesLoadedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.pagesLoaded);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapPagesLoadedAttr = {
  .attr  = { .name = "block_map_pages_loaded", .mode = 0444, },
  .show  = poolStatsBlockMapPagesLoadedShow,
};

/**********************************************************************/
/** number of page saves */
static ssize_t poolStatsBlockMapPagesSavedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.pagesSaved);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapPagesSavedAttr = {
  .attr  = { .name = "block_map_pages_saved", .mode = 0444, },
  .show  = poolStatsBlockMapPagesSavedShow,
};

/**********************************************************************/
/** the number of flushes issued */
static ssize_t poolStatsBlockMapFlushCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->blockMap.flushCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBlockMapFlushCountAttr = {
  .attr  = { .name = "block_map_flush_count", .mode = 0444, },
  .show  = poolStatsBlockMapFlushCountShow,
};

/**********************************************************************/
/** number of times VDO got an invalid dedupe advice PBN from albireo */
static ssize_t poolStatsErrorsInvalidAdvicePBNCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->errors.invalidAdvicePBNCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsErrorsInvalidAdvicePBNCountAttr = {
  .attr  = { .name = "errors_invalid_advicePBNCount", .mode = 0444, },
  .show  = poolStatsErrorsInvalidAdvicePBNCountShow,
};

/**********************************************************************/
/** number of times VDO got an invalid rollover confirmation PBN from albireo */
static ssize_t poolStatsErrorsInvalidRolloverPBNCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->errors.invalidRolloverPBNCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsErrorsInvalidRolloverPBNCountAttr = {
  .attr  = { .name = "errors_invalid_rolloverPBNCount", .mode = 0444, },
  .show  = poolStatsErrorsInvalidRolloverPBNCountShow,
};

/**********************************************************************/
/** number of times the dedupe deadlock avoidance mechanism fired */
static ssize_t poolStatsErrorsDedupeDeadlockAvoidanceCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->errors.dedupeDeadlockAvoidanceCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsErrorsDedupeDeadlockAvoidanceCountAttr = {
  .attr  = { .name = "errors_dedupe_deadlock_avoidance_count", .mode = 0444, },
  .show  = poolStatsErrorsDedupeDeadlockAvoidanceCountShow,
};

/**********************************************************************/
/** number of times a VIO completed with a VDO_NO_SPACE error */
static ssize_t poolStatsErrorsNoSpaceErrorCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->errors.noSpaceErrorCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsErrorsNoSpaceErrorCountAttr = {
  .attr  = { .name = "errors_no_space_error_count", .mode = 0444, },
  .show  = poolStatsErrorsNoSpaceErrorCountShow,
};

/**********************************************************************/
/** number of times a VIO completed with a VDO_READ_ONLY error */
static ssize_t poolStatsErrorsReadOnlyErrorCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKVDOStatistics(&layer->kvdo, layer->vdoStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->vdoStatsStorage->errors.readOnlyErrorCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsErrorsReadOnlyErrorCountAttr = {
  .attr  = { .name = "errors_read_only_error_count", .mode = 0444, },
  .show  = poolStatsErrorsReadOnlyErrorCountShow,
};

/**********************************************************************/
/** The VDO instance */
static ssize_t poolStatsInstanceShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->kernelStatsStorage->instance);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsInstanceAttr = {
  .attr  = { .name = "instance", .mode = 0444, },
  .show  = poolStatsInstanceShow,
};

/**********************************************************************/
/** Current number of active VIOs */
static ssize_t poolStatsCurrentVIOsInProgressShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->kernelStatsStorage->currentVIOsInProgress);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsCurrentVIOsInProgressAttr = {
  .attr  = { .name = "currentVIOs_in_progress", .mode = 0444, },
  .show  = poolStatsCurrentVIOsInProgressShow,
};

/**********************************************************************/
/** Maximum number of active VIOs */
static ssize_t poolStatsMaxVIOsShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->kernelStatsStorage->maxVIOs);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsMaxVIOsAttr = {
  .attr  = { .name = "maxVIOs", .mode = 0444, },
  .show  = poolStatsMaxVIOsShow,
};

/**********************************************************************/
/** Current number of Dedupe queries that are in flight */
static ssize_t poolStatsCurrDedupeQueriesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->kernelStatsStorage->currDedupeQueries);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsCurrDedupeQueriesAttr = {
  .attr  = { .name = "curr_dedupe_queries", .mode = 0444, },
  .show  = poolStatsCurrDedupeQueriesShow,
};

/**********************************************************************/
/** Maximum number of Dedupe queries that have been in flight */
static ssize_t poolStatsMaxDedupeQueriesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu32 "\n", layer->kernelStatsStorage->maxDedupeQueries);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsMaxDedupeQueriesAttr = {
  .attr  = { .name = "max_dedupe_queries", .mode = 0444, },
  .show  = poolStatsMaxDedupeQueriesShow,
};

/**********************************************************************/
/** Number of times the Albireo advice proved correct */
static ssize_t poolStatsDedupeAdviceValidShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->dedupeAdviceValid);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsDedupeAdviceValidAttr = {
  .attr  = { .name = "dedupe_advice_valid", .mode = 0444, },
  .show  = poolStatsDedupeAdviceValidShow,
};

/**********************************************************************/
/** Number of times the Albireo advice proved incorrect */
static ssize_t poolStatsDedupeAdviceStaleShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->dedupeAdviceStale);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsDedupeAdviceStaleAttr = {
  .attr  = { .name = "dedupe_advice_stale", .mode = 0444, },
  .show  = poolStatsDedupeAdviceStaleShow,
};

/**********************************************************************/
/** Number of times the Albireo server was too slow in responding */
static ssize_t poolStatsDedupeAdviceTimeoutsShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->dedupeAdviceTimeouts);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsDedupeAdviceTimeoutsAttr = {
  .attr  = { .name = "dedupe_advice_timeouts", .mode = 0444, },
  .show  = poolStatsDedupeAdviceTimeoutsShow,
};

/**********************************************************************/
/** Number of flush requests submitted to the storage device */
static ssize_t poolStatsFlushOutShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->flushOut);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsFlushOutAttr = {
  .attr  = { .name = "flush_out", .mode = 0444, },
  .show  = poolStatsFlushOutShow,
};

/**********************************************************************/
/** Logical block size */
static ssize_t poolStatsLogicalBlockSizeShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->logicalBlockSize);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsLogicalBlockSizeAttr = {
  .attr  = { .name = "logical_block_size", .mode = 0444, },
  .show  = poolStatsLogicalBlockSizeShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosInReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosIn.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInReadAttr = {
  .attr  = { .name = "bios_in_read", .mode = 0444, },
  .show  = poolStatsBiosInReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosInWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosIn.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInWriteAttr = {
  .attr  = { .name = "bios_in_write", .mode = 0444, },
  .show  = poolStatsBiosInWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosInDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosIn.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInDiscardAttr = {
  .attr  = { .name = "bios_in_discard", .mode = 0444, },
  .show  = poolStatsBiosInDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosInFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosIn.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInFlushAttr = {
  .attr  = { .name = "bios_in_flush", .mode = 0444, },
  .show  = poolStatsBiosInFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosInFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosIn.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInFuaAttr = {
  .attr  = { .name = "bios_in_fua", .mode = 0444, },
  .show  = poolStatsBiosInFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosInPartialReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInPartial.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInPartialReadAttr = {
  .attr  = { .name = "bios_in_partial_read", .mode = 0444, },
  .show  = poolStatsBiosInPartialReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosInPartialWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInPartial.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInPartialWriteAttr = {
  .attr  = { .name = "bios_in_partial_write", .mode = 0444, },
  .show  = poolStatsBiosInPartialWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosInPartialDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInPartial.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInPartialDiscardAttr = {
  .attr  = { .name = "bios_in_partial_discard", .mode = 0444, },
  .show  = poolStatsBiosInPartialDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosInPartialFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInPartial.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInPartialFlushAttr = {
  .attr  = { .name = "bios_in_partial_flush", .mode = 0444, },
  .show  = poolStatsBiosInPartialFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosInPartialFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInPartial.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInPartialFuaAttr = {
  .attr  = { .name = "bios_in_partial_fua", .mode = 0444, },
  .show  = poolStatsBiosInPartialFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosOutReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOut.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutReadAttr = {
  .attr  = { .name = "bios_out_read", .mode = 0444, },
  .show  = poolStatsBiosOutReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosOutWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOut.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutWriteAttr = {
  .attr  = { .name = "bios_out_write", .mode = 0444, },
  .show  = poolStatsBiosOutWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosOutDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOut.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutDiscardAttr = {
  .attr  = { .name = "bios_out_discard", .mode = 0444, },
  .show  = poolStatsBiosOutDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosOutFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOut.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutFlushAttr = {
  .attr  = { .name = "bios_out_flush", .mode = 0444, },
  .show  = poolStatsBiosOutFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosOutFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOut.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutFuaAttr = {
  .attr  = { .name = "bios_out_fua", .mode = 0444, },
  .show  = poolStatsBiosOutFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosMetaReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMeta.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaReadAttr = {
  .attr  = { .name = "bios_meta_read", .mode = 0444, },
  .show  = poolStatsBiosMetaReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosMetaWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMeta.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaWriteAttr = {
  .attr  = { .name = "bios_meta_write", .mode = 0444, },
  .show  = poolStatsBiosMetaWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosMetaDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMeta.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaDiscardAttr = {
  .attr  = { .name = "bios_meta_discard", .mode = 0444, },
  .show  = poolStatsBiosMetaDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosMetaFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMeta.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaFlushAttr = {
  .attr  = { .name = "bios_meta_flush", .mode = 0444, },
  .show  = poolStatsBiosMetaFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosMetaFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMeta.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaFuaAttr = {
  .attr  = { .name = "bios_meta_fua", .mode = 0444, },
  .show  = poolStatsBiosMetaFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosJournalReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournal.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalReadAttr = {
  .attr  = { .name = "bios_journal_read", .mode = 0444, },
  .show  = poolStatsBiosJournalReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosJournalWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournal.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalWriteAttr = {
  .attr  = { .name = "bios_journal_write", .mode = 0444, },
  .show  = poolStatsBiosJournalWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosJournalDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournal.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalDiscardAttr = {
  .attr  = { .name = "bios_journal_discard", .mode = 0444, },
  .show  = poolStatsBiosJournalDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosJournalFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournal.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalFlushAttr = {
  .attr  = { .name = "bios_journal_flush", .mode = 0444, },
  .show  = poolStatsBiosJournalFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosJournalFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournal.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalFuaAttr = {
  .attr  = { .name = "bios_journal_fua", .mode = 0444, },
  .show  = poolStatsBiosJournalFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosPageCacheReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCache.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheReadAttr = {
  .attr  = { .name = "bios_page_cache_read", .mode = 0444, },
  .show  = poolStatsBiosPageCacheReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosPageCacheWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCache.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheWriteAttr = {
  .attr  = { .name = "bios_page_cache_write", .mode = 0444, },
  .show  = poolStatsBiosPageCacheWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosPageCacheDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCache.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheDiscardAttr = {
  .attr  = { .name = "bios_page_cache_discard", .mode = 0444, },
  .show  = poolStatsBiosPageCacheDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosPageCacheFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCache.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheFlushAttr = {
  .attr  = { .name = "bios_page_cache_flush", .mode = 0444, },
  .show  = poolStatsBiosPageCacheFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosPageCacheFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCache.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheFuaAttr = {
  .attr  = { .name = "bios_page_cache_fua", .mode = 0444, },
  .show  = poolStatsBiosPageCacheFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosOutCompletedReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOutCompleted.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutCompletedReadAttr = {
  .attr  = { .name = "bios_out_completed_read", .mode = 0444, },
  .show  = poolStatsBiosOutCompletedReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosOutCompletedWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOutCompleted.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutCompletedWriteAttr = {
  .attr  = { .name = "bios_out_completed_write", .mode = 0444, },
  .show  = poolStatsBiosOutCompletedWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosOutCompletedDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOutCompleted.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutCompletedDiscardAttr = {
  .attr  = { .name = "bios_out_completed_discard", .mode = 0444, },
  .show  = poolStatsBiosOutCompletedDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosOutCompletedFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOutCompleted.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutCompletedFlushAttr = {
  .attr  = { .name = "bios_out_completed_flush", .mode = 0444, },
  .show  = poolStatsBiosOutCompletedFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosOutCompletedFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosOutCompleted.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosOutCompletedFuaAttr = {
  .attr  = { .name = "bios_out_completed_fua", .mode = 0444, },
  .show  = poolStatsBiosOutCompletedFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosMetaCompletedReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMetaCompleted.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaCompletedReadAttr = {
  .attr  = { .name = "bios_meta_completed_read", .mode = 0444, },
  .show  = poolStatsBiosMetaCompletedReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosMetaCompletedWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMetaCompleted.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaCompletedWriteAttr = {
  .attr  = { .name = "bios_meta_completed_write", .mode = 0444, },
  .show  = poolStatsBiosMetaCompletedWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosMetaCompletedDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMetaCompleted.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaCompletedDiscardAttr = {
  .attr  = { .name = "bios_meta_completed_discard", .mode = 0444, },
  .show  = poolStatsBiosMetaCompletedDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosMetaCompletedFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMetaCompleted.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaCompletedFlushAttr = {
  .attr  = { .name = "bios_meta_completed_flush", .mode = 0444, },
  .show  = poolStatsBiosMetaCompletedFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosMetaCompletedFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosMetaCompleted.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosMetaCompletedFuaAttr = {
  .attr  = { .name = "bios_meta_completed_fua", .mode = 0444, },
  .show  = poolStatsBiosMetaCompletedFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosJournalCompletedReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournalCompleted.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalCompletedReadAttr = {
  .attr  = { .name = "bios_journal_completed_read", .mode = 0444, },
  .show  = poolStatsBiosJournalCompletedReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosJournalCompletedWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournalCompleted.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalCompletedWriteAttr = {
  .attr  = { .name = "bios_journal_completed_write", .mode = 0444, },
  .show  = poolStatsBiosJournalCompletedWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosJournalCompletedDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournalCompleted.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalCompletedDiscardAttr = {
  .attr  = { .name = "bios_journal_completed_discard", .mode = 0444, },
  .show  = poolStatsBiosJournalCompletedDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosJournalCompletedFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournalCompleted.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalCompletedFlushAttr = {
  .attr  = { .name = "bios_journal_completed_flush", .mode = 0444, },
  .show  = poolStatsBiosJournalCompletedFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosJournalCompletedFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosJournalCompleted.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosJournalCompletedFuaAttr = {
  .attr  = { .name = "bios_journal_completed_fua", .mode = 0444, },
  .show  = poolStatsBiosJournalCompletedFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosPageCacheCompletedReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCacheCompleted.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheCompletedReadAttr = {
  .attr  = { .name = "bios_page_cache_completed_read", .mode = 0444, },
  .show  = poolStatsBiosPageCacheCompletedReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosPageCacheCompletedWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCacheCompleted.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheCompletedWriteAttr = {
  .attr  = { .name = "bios_page_cache_completed_write", .mode = 0444, },
  .show  = poolStatsBiosPageCacheCompletedWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosPageCacheCompletedDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCacheCompleted.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheCompletedDiscardAttr = {
  .attr  = { .name = "bios_page_cache_completed_discard", .mode = 0444, },
  .show  = poolStatsBiosPageCacheCompletedDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosPageCacheCompletedFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCacheCompleted.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheCompletedFlushAttr = {
  .attr  = { .name = "bios_page_cache_completed_flush", .mode = 0444, },
  .show  = poolStatsBiosPageCacheCompletedFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosPageCacheCompletedFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosPageCacheCompleted.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosPageCacheCompletedFuaAttr = {
  .attr  = { .name = "bios_page_cache_completed_fua", .mode = 0444, },
  .show  = poolStatsBiosPageCacheCompletedFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosAcknowledgedReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledged.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedReadAttr = {
  .attr  = { .name = "bios_acknowledged_read", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosAcknowledgedWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledged.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedWriteAttr = {
  .attr  = { .name = "bios_acknowledged_write", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosAcknowledgedDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledged.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedDiscardAttr = {
  .attr  = { .name = "bios_acknowledged_discard", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosAcknowledgedFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledged.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedFlushAttr = {
  .attr  = { .name = "bios_acknowledged_flush", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosAcknowledgedFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledged.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedFuaAttr = {
  .attr  = { .name = "bios_acknowledged_fua", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosAcknowledgedPartialReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledgedPartial.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedPartialReadAttr = {
  .attr  = { .name = "bios_acknowledged_partial_read", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedPartialReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosAcknowledgedPartialWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledgedPartial.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedPartialWriteAttr = {
  .attr  = { .name = "bios_acknowledged_partial_write", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedPartialWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosAcknowledgedPartialDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledgedPartial.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedPartialDiscardAttr = {
  .attr  = { .name = "bios_acknowledged_partial_discard", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedPartialDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosAcknowledgedPartialFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledgedPartial.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedPartialFlushAttr = {
  .attr  = { .name = "bios_acknowledged_partial_flush", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedPartialFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosAcknowledgedPartialFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosAcknowledgedPartial.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosAcknowledgedPartialFuaAttr = {
  .attr  = { .name = "bios_acknowledged_partial_fua", .mode = 0444, },
  .show  = poolStatsBiosAcknowledgedPartialFuaShow,
};

/**********************************************************************/
/** Number of not REQ_WRITE bios */
static ssize_t poolStatsBiosInProgressReadShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInProgress.read);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInProgressReadAttr = {
  .attr  = { .name = "bios_in_progress_read", .mode = 0444, },
  .show  = poolStatsBiosInProgressReadShow,
};

/**********************************************************************/
/** Number of REQ_WRITE bios */
static ssize_t poolStatsBiosInProgressWriteShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInProgress.write);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInProgressWriteAttr = {
  .attr  = { .name = "bios_in_progress_write", .mode = 0444, },
  .show  = poolStatsBiosInProgressWriteShow,
};

/**********************************************************************/
/** Number of REQ_DISCARD bios */
static ssize_t poolStatsBiosInProgressDiscardShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInProgress.discard);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInProgressDiscardAttr = {
  .attr  = { .name = "bios_in_progress_discard", .mode = 0444, },
  .show  = poolStatsBiosInProgressDiscardShow,
};

/**********************************************************************/
/** Number of REQ_FLUSH bios */
static ssize_t poolStatsBiosInProgressFlushShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInProgress.flush);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInProgressFlushAttr = {
  .attr  = { .name = "bios_in_progress_flush", .mode = 0444, },
  .show  = poolStatsBiosInProgressFlushShow,
};

/**********************************************************************/
/** Number of REQ_FUA bios */
static ssize_t poolStatsBiosInProgressFuaShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->biosInProgress.fua);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsBiosInProgressFuaAttr = {
  .attr  = { .name = "bios_in_progress_fua", .mode = 0444, },
  .show  = poolStatsBiosInProgressFuaShow,
};

/**********************************************************************/
/** Number of times the read cache was asked for a specific pbn. */
static ssize_t poolStatsReadCacheAccessesShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->readCache.accesses);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsReadCacheAccessesAttr = {
  .attr  = { .name = "read_cache_accesses", .mode = 0444, },
  .show  = poolStatsReadCacheAccessesShow,
};

/**********************************************************************/
/** Number of times the read cache found the requested pbn. */
static ssize_t poolStatsReadCacheHitsShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->readCache.hits);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsReadCacheHitsAttr = {
  .attr  = { .name = "read_cache_hits", .mode = 0444, },
  .show  = poolStatsReadCacheHitsShow,
};

/**********************************************************************/
/** Number of times the found requested pbn had valid data. */
static ssize_t poolStatsReadCacheDataHitsShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->readCache.dataHits);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsReadCacheDataHitsAttr = {
  .attr  = { .name = "read_cache_data_hits", .mode = 0444, },
  .show  = poolStatsReadCacheDataHitsShow,
};

/**********************************************************************/
/** Tracked bytes currently allocated. */
static ssize_t poolStatsMemoryUsageBytesUsedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->memoryUsage.bytesUsed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsMemoryUsageBytesUsedAttr = {
  .attr  = { .name = "memory_usage_bytes_used", .mode = 0444, },
  .show  = poolStatsMemoryUsageBytesUsedShow,
};

/**********************************************************************/
/** Maximum tracked bytes allocated. */
static ssize_t poolStatsMemoryUsagePeakBytesUsedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->memoryUsage.peakBytesUsed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsMemoryUsagePeakBytesUsedAttr = {
  .attr  = { .name = "memory_usage_peak_bytes_used", .mode = 0444, },
  .show  = poolStatsMemoryUsagePeakBytesUsedShow,
};

/**********************************************************************/
/** Bio structures currently allocated (size not tracked). */
static ssize_t poolStatsMemoryUsageBiosUsedShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->memoryUsage.biosUsed);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsMemoryUsageBiosUsedAttr = {
  .attr  = { .name = "memory_usage_bios_used", .mode = 0444, },
  .show  = poolStatsMemoryUsageBiosUsedShow,
};

/**********************************************************************/
/** Maximum number of bios allocated. */
static ssize_t poolStatsMemoryUsagePeakBioCountShow(KernelLayer *layer, char *buf)
{
  ssize_t retval;
  spin_lock(&layer->statsLock);
  getKernelStats(layer, layer->kernelStatsStorage);
  retval = sprintf(buf, "%" PRIu64 "\n", layer->kernelStatsStorage->memoryUsage.peakBioCount);
  spin_unlock(&layer->statsLock);
  return retval;
}

static PoolStatsAttribute poolStatsMemoryUsagePeakBioCountAttr = {
  .attr  = { .name = "memory_usage_peak_bio_count", .mode = 0444, },
  .show  = poolStatsMemoryUsagePeakBioCountShow,
};

static struct attribute *poolStatsAttrs[] = {
  &poolStatsDataBlocksUsedAttr.attr,
  &poolStatsOverheadBlocksUsedAttr.attr,
  &poolStatsLogicalBlocksUsedAttr.attr,
  &poolStatsPhysicalBlocksAttr.attr,
  &poolStatsLogicalBlocksAttr.attr,
  &poolStatsBlockMapCacheSizeAttr.attr,
  &poolStatsWritePolicyAttr.attr,
  &poolStatsBlockSizeAttr.attr,
  &poolStatsCompleteRecoveriesAttr.attr,
  &poolStatsReadOnlyRecoveriesAttr.attr,
  &poolStatsModeAttr.attr,
  &poolStatsInRecoveryModeAttr.attr,
  &poolStatsRecoveryPercentageAttr.attr,
  &poolStatsPackerCompressedFragmentsWrittenAttr.attr,
  &poolStatsPackerCompressedBlocksWrittenAttr.attr,
  &poolStatsPackerCompressedFragmentsInPackerAttr.attr,
  &poolStatsAllocatorSlabCountAttr.attr,
  &poolStatsAllocatorSlabsOpenedAttr.attr,
  &poolStatsAllocatorSlabsReopenedAttr.attr,
  &poolStatsJournalDiskFullAttr.attr,
  &poolStatsJournalSlabJournalCommitsRequestedAttr.attr,
  &poolStatsJournalEntriesStartedAttr.attr,
  &poolStatsJournalEntriesWrittenAttr.attr,
  &poolStatsJournalEntriesCommittedAttr.attr,
  &poolStatsJournalBlocksStartedAttr.attr,
  &poolStatsJournalBlocksWrittenAttr.attr,
  &poolStatsJournalBlocksCommittedAttr.attr,
  &poolStatsSlabJournalDiskFullCountAttr.attr,
  &poolStatsSlabJournalFlushCountAttr.attr,
  &poolStatsSlabJournalBlockedCountAttr.attr,
  &poolStatsSlabJournalBlocksWrittenAttr.attr,
  &poolStatsSlabJournalTailBusyCountAttr.attr,
  &poolStatsSlabSummaryBlocksWrittenAttr.attr,
  &poolStatsRefCountsBlocksWrittenAttr.attr,
  &poolStatsBlockMapDirtyPagesAttr.attr,
  &poolStatsBlockMapCleanPagesAttr.attr,
  &poolStatsBlockMapFreePagesAttr.attr,
  &poolStatsBlockMapFailedPagesAttr.attr,
  &poolStatsBlockMapIncomingPagesAttr.attr,
  &poolStatsBlockMapOutgoingPagesAttr.attr,
  &poolStatsBlockMapCachePressureAttr.attr,
  &poolStatsBlockMapReadCountAttr.attr,
  &poolStatsBlockMapWriteCountAttr.attr,
  &poolStatsBlockMapFailedReadsAttr.attr,
  &poolStatsBlockMapFailedWritesAttr.attr,
  &poolStatsBlockMapReclaimedAttr.attr,
  &poolStatsBlockMapReadOutgoingAttr.attr,
  &poolStatsBlockMapFoundInCacheAttr.attr,
  &poolStatsBlockMapDiscardRequiredAttr.attr,
  &poolStatsBlockMapWaitForPageAttr.attr,
  &poolStatsBlockMapFetchRequiredAttr.attr,
  &poolStatsBlockMapPagesLoadedAttr.attr,
  &poolStatsBlockMapPagesSavedAttr.attr,
  &poolStatsBlockMapFlushCountAttr.attr,
  &poolStatsErrorsInvalidAdvicePBNCountAttr.attr,
  &poolStatsErrorsInvalidRolloverPBNCountAttr.attr,
  &poolStatsErrorsDedupeDeadlockAvoidanceCountAttr.attr,
  &poolStatsErrorsNoSpaceErrorCountAttr.attr,
  &poolStatsErrorsReadOnlyErrorCountAttr.attr,
  &poolStatsInstanceAttr.attr,
  &poolStatsCurrentVIOsInProgressAttr.attr,
  &poolStatsMaxVIOsAttr.attr,
  &poolStatsCurrDedupeQueriesAttr.attr,
  &poolStatsMaxDedupeQueriesAttr.attr,
  &poolStatsDedupeAdviceValidAttr.attr,
  &poolStatsDedupeAdviceStaleAttr.attr,
  &poolStatsDedupeAdviceTimeoutsAttr.attr,
  &poolStatsFlushOutAttr.attr,
  &poolStatsLogicalBlockSizeAttr.attr,
  &poolStatsBiosInReadAttr.attr,
  &poolStatsBiosInWriteAttr.attr,
  &poolStatsBiosInDiscardAttr.attr,
  &poolStatsBiosInFlushAttr.attr,
  &poolStatsBiosInFuaAttr.attr,
  &poolStatsBiosInPartialReadAttr.attr,
  &poolStatsBiosInPartialWriteAttr.attr,
  &poolStatsBiosInPartialDiscardAttr.attr,
  &poolStatsBiosInPartialFlushAttr.attr,
  &poolStatsBiosInPartialFuaAttr.attr,
  &poolStatsBiosOutReadAttr.attr,
  &poolStatsBiosOutWriteAttr.attr,
  &poolStatsBiosOutDiscardAttr.attr,
  &poolStatsBiosOutFlushAttr.attr,
  &poolStatsBiosOutFuaAttr.attr,
  &poolStatsBiosMetaReadAttr.attr,
  &poolStatsBiosMetaWriteAttr.attr,
  &poolStatsBiosMetaDiscardAttr.attr,
  &poolStatsBiosMetaFlushAttr.attr,
  &poolStatsBiosMetaFuaAttr.attr,
  &poolStatsBiosJournalReadAttr.attr,
  &poolStatsBiosJournalWriteAttr.attr,
  &poolStatsBiosJournalDiscardAttr.attr,
  &poolStatsBiosJournalFlushAttr.attr,
  &poolStatsBiosJournalFuaAttr.attr,
  &poolStatsBiosPageCacheReadAttr.attr,
  &poolStatsBiosPageCacheWriteAttr.attr,
  &poolStatsBiosPageCacheDiscardAttr.attr,
  &poolStatsBiosPageCacheFlushAttr.attr,
  &poolStatsBiosPageCacheFuaAttr.attr,
  &poolStatsBiosOutCompletedReadAttr.attr,
  &poolStatsBiosOutCompletedWriteAttr.attr,
  &poolStatsBiosOutCompletedDiscardAttr.attr,
  &poolStatsBiosOutCompletedFlushAttr.attr,
  &poolStatsBiosOutCompletedFuaAttr.attr,
  &poolStatsBiosMetaCompletedReadAttr.attr,
  &poolStatsBiosMetaCompletedWriteAttr.attr,
  &poolStatsBiosMetaCompletedDiscardAttr.attr,
  &poolStatsBiosMetaCompletedFlushAttr.attr,
  &poolStatsBiosMetaCompletedFuaAttr.attr,
  &poolStatsBiosJournalCompletedReadAttr.attr,
  &poolStatsBiosJournalCompletedWriteAttr.attr,
  &poolStatsBiosJournalCompletedDiscardAttr.attr,
  &poolStatsBiosJournalCompletedFlushAttr.attr,
  &poolStatsBiosJournalCompletedFuaAttr.attr,
  &poolStatsBiosPageCacheCompletedReadAttr.attr,
  &poolStatsBiosPageCacheCompletedWriteAttr.attr,
  &poolStatsBiosPageCacheCompletedDiscardAttr.attr,
  &poolStatsBiosPageCacheCompletedFlushAttr.attr,
  &poolStatsBiosPageCacheCompletedFuaAttr.attr,
  &poolStatsBiosAcknowledgedReadAttr.attr,
  &poolStatsBiosAcknowledgedWriteAttr.attr,
  &poolStatsBiosAcknowledgedDiscardAttr.attr,
  &poolStatsBiosAcknowledgedFlushAttr.attr,
  &poolStatsBiosAcknowledgedFuaAttr.attr,
  &poolStatsBiosAcknowledgedPartialReadAttr.attr,
  &poolStatsBiosAcknowledgedPartialWriteAttr.attr,
  &poolStatsBiosAcknowledgedPartialDiscardAttr.attr,
  &poolStatsBiosAcknowledgedPartialFlushAttr.attr,
  &poolStatsBiosAcknowledgedPartialFuaAttr.attr,
  &poolStatsBiosInProgressReadAttr.attr,
  &poolStatsBiosInProgressWriteAttr.attr,
  &poolStatsBiosInProgressDiscardAttr.attr,
  &poolStatsBiosInProgressFlushAttr.attr,
  &poolStatsBiosInProgressFuaAttr.attr,
  &poolStatsReadCacheAccessesAttr.attr,
  &poolStatsReadCacheHitsAttr.attr,
  &poolStatsReadCacheDataHitsAttr.attr,
  &poolStatsMemoryUsageBytesUsedAttr.attr,
  &poolStatsMemoryUsagePeakBytesUsedAttr.attr,
  &poolStatsMemoryUsageBiosUsedAttr.attr,
  &poolStatsMemoryUsagePeakBioCountAttr.attr,
  NULL,
};

/**********************************************************************/
static void poolStatsRelease(struct kobject *kobj)
{
  /* A no-op. */
}

struct kobj_type statsDirectoryKobjType = {
  .release       = poolStatsRelease,
  .sysfs_ops     = &poolStatsSysfsOps,
  .default_attrs = poolStatsAttrs,
};
