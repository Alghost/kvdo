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
 *
 * $Id: //eng/uds-releases/flanders/src/uds/sparseCache.c#2 $
 */

/**
 * The sparse chapter index cache is implemented as a simple array of cache
 * entries. Since the cache is small (seven chapters by default), searching
 * for a specific virtual chapter is implemented as a linear search. The cache
 * replacement policy is least-recently-used (LRU). Again, size of the cache
 * allows the LRU order to be maintained by shifting entries in an array list.
 *
 * The most important property of this cache is the absence of synchronization
 * for read operations. Safe concurrent access to the cache by the zone
 * threads is controlled by the triage queue and the barrier requests it
 * issues to the zone queues. The set of cached chapters does not and must not
 * change between the carefully coordinated calls to updateSparseCache() from
 * the zone threads.
 *
 * The critical invariant for that coordination is the cache membership must
 * not change between those updates; the calls to sparseCacheContains() from
 * the zone threads must all receive the same results for any virtual chapter
 * number. To ensure that critical invariant, state changes such as "that
 * virtual chapter is no longer in the volume" and "skip searching that
 * chapter because it has had too many cache misses" are represented
 * separately from the cache membership information (the virtual chapter
 * number).
 *
 * As a result of this invariant, we have the guarantee that every zone thread
 * will call updateSparseCache() once and exactly once to request a chapter
 * that is not in the cache, and the serialization of the barrier requests
 * from the triage queue ensures they will all request the same chapter
 * number. This means the only synchronzation we need can be provided by a
 * pair of thread barriers used only in the updateSparseCache() call,
 * providing a critical section where a single zone thread can drive the cache
 * update while all the other zone threads are known to be blocked, waiting in
 * the second barrier. Outside that critical section, all the zone threads
 * implicitly hold a shared lock. Inside it, the "captain" (the thread that
 * was uniquely flagged when passing through the first barrier) holds an
 * exclusive lock. No other threads may access or modify the cache, except for
 * accessing cache statistics and similar queries.
 *
 * Cache statistics must only be modified by a single thread, conventionally
 * the zone zero thread. All fields that might be frequently updated by that
 * thread are kept in separate cache-aligned structures so they will not cause
 * cache contention via "false sharing" with the fields that are frequently
 * accessed by all of the zone threads.
 *
 * LRU order is kept independently by each zone thread, and each zone uses its
 * own list for searching and cache membership queries. The zone zero list is
 * used to decide which chapter to evict when the cache is updated, and its
 * search list is copied to the other threads at that time.
 *
 * The virtual chapter number field of the cache entry is the single field
 * indicating whether a chapter is a member of the cache or not. The value
 * <code>UINT64_MAX</code> is used to represent a null, undefined, or wildcard
 * chapter number. When present in the virtual chapter number field
 * CachedChapterIndex, it indicates that the cache entry is dead, and all
 * the other fields of that entry (other than immutable pointers to cache
 * memory) are undefined and irrelevant. Any cache entry that is not marked as
 * dead is fully defined and a member of the cache--sparseCacheContains()
 * must always return true for any virtual chapter number that appears in any
 * of the cache entries.
 *
 * A chapter index that is a member of the cache may be marked for different
 * treatment (disabling search) between calls to updateSparseCache() in two
 * different ways. When a chapter falls off the end of the volume,
 * invalidateSparseCacheChapter() will be called with a virtual chapter
 * number. Since that chapter is no longer part of the volume, there's no
 * point in continuing to search that chapter index. That cache entry will be
 * flagged as invalid by setting the "invalid" field. Once invalidated, that
 * virtual chapter will still be considered a member of the cache, but it will
 * no longer be searched for matching chunk names.
 *
 * The second mechanism for disabling search is the heuristic based on keeping
 * track of the number of consecutive search misses in a given chapter index.
 * Once that count exceeds a threshold, the skipSearch flag will be set to
 * true, causing the chapter to be skipped in the fallback search of the
 * entire cache, but still allowing it to be found when searching for a hook
 * in that specific chapter. Finding a hook will clear the skipSearch flag,
 * once again allowing the non-hook searches to use the cache entry. Again,
 * regardless of the state of the skipSearch flag, the virtual chapter must
 * still considered to be a member of the cache for sparseCacheContains().
 *
 * Barrier requests and the sparse chapter index cache are also described in
 *
 * https://intranet.permabit.com/wiki/Chapter_Index_Cache_supports_concurrent_access
 *
 * and in a message to the albireo mailing list on 5/28/2011 titled "true
 * barriers with a hook resolution queue".
 **/

#include "sparseCache.h"

#include "cachedChapterIndex.h"
#include "chapterIndex.h"
#include "common.h"
#include "index.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"
#include "searchList.h"
#include "threads.h"
#include "util/statistic.h"
#include "zone.h"

enum {
  /** The number of consecutive search misses that will disable searching */
  SKIP_SEARCH_THRESHOLD = 20000,

  /** a named constant to use when identifying zone zero */
  ZONE_ZERO = 0
};

/**
 * These counter values are essentially fields of the SparseCache, but are
 * segregated into this structure because they are frequently modified. We
 * group them and align them to keep them on different cache lines from the
 * cache fields that are accessed far more often than they are updated.
 **/
typedef struct __attribute__((aligned(CACHE_LINE_BYTES))) sparseCacheCounters {
  /** the total number of virtual chapter probes that succeeded */
  uint64_t      chapterHits;

  /** the total number of virtual chapter probes that failed */
  uint64_t      chapterMisses;

  /** the total number of cache searches that found a possible match */
  uint64_t      searchHits;

  /** the total number of cache searches that found no matches */
  uint64_t      searchMisses;

  /** the number of cache entries that fell off the end of the volume */
  uint64_t      invalidations;

  /** the number of cache entries that were evicted while still valid */
  uint64_t      evictions;
} SparseCacheCounters;

/**
 * These statistic are essentially fields of the SparseCache, but are
 * segregated into this structure because they are frequently modified. We
 * group them and align them to keep them on different cache lines from the
 * cache fields that are accessed far more often than they are updated.
 **/
struct __attribute__((aligned(CACHE_LINE_BYTES))) sparseCacheStatistics {
  /** samples the total number of search hits in each cache entry */
  Statistic searchHitsPerChapter;

  /** samples the total number of search misses in each cache entry */
  Statistic searchMissesPerChapter;

  /** samples the number of chapters searched for each search hit */
  Statistic chaptersSearchedPerHit;

  /** samples the number of chapters searched for each search miss */
  Statistic chaptersSearchedPerMiss;
};
typedef struct sparseCacheStatistics SparseCacheStatistics;

/**
 * This is the private structure definition of a SparseCache.
 **/
struct sparseCache {
  /** the number of cache entries, which is the size of the chapters array */
  unsigned int           capacity;

  /** the number of zone threads using the cache */
  unsigned int           zoneCount;

  /** the geometry governing the volume */
  const Geometry        *geometry;

  /** the number of search misses in zone zero that will disable searching */
  unsigned int           skipSearchThreshold;

  /** pointers to the cache-aligned chapter search order for each zone */
  SearchList            *searchLists[MAX_ZONES];

  /** the thread barriers used to synchronize the zone threads for update */
  Barrier                beginCacheUpdate;
  Barrier                endCacheUpdate;

  /** frequently-updated counter fields (cache-aligned) */
  SparseCacheCounters    counters;

  /** frequently-updated statistics (cache-aligned) */
  SparseCacheStatistics  statistics;

  /** the counted array of chapter index cache entries (cache-aligned) */
  CachedChapterIndex     chapters[];
};

/**
 * Initialize a sparse chapter index cache.
 *
 * @param cache      the sparse cache to initialize
 * @param geometry   the geometry governing the volume
 * @param capacity   the number of chapters the cache will hold
 * @param zoneCount  the number of zone threads using the cache
 *
 * @return UDS_SUCCESS or an error code
 **/
__attribute__((warn_unused_result))
static int initializeSparseCache(SparseCache    *cache,
                                 const Geometry *geometry,
                                 unsigned int    capacity,
                                 unsigned int    zoneCount)
{
  cache->geometry  = geometry;
  cache->capacity  = capacity;
  cache->zoneCount = zoneCount;

  // Scale down the skip threshold by the number of zones since we count the
  // chapter search misses only in zone zero.
  cache->skipSearchThreshold = (SKIP_SEARCH_THRESHOLD / zoneCount);

  int result = initializeBarrier(&cache->beginCacheUpdate, zoneCount);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = initializeBarrier(&cache->endCacheUpdate, zoneCount);
  if (result != UDS_SUCCESS) {
    return result;
  }
  for (unsigned int i = 0; i < capacity; i++) {
    result = initializeCachedChapterIndex(&cache->chapters[i], geometry);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  // Allocate each zone's independent LRU order.
  for (unsigned int i = 0; i < zoneCount; i++) {
    result = makeSearchList(capacity, &cache->searchLists[i]);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  resetStatistic(&cache->statistics.searchHitsPerChapter);
  resetStatistic(&cache->statistics.searchMissesPerChapter);
  resetStatistic(&cache->statistics.chaptersSearchedPerHit);
  resetStatistic(&cache->statistics.chaptersSearchedPerMiss);

  return UDS_SUCCESS;
}

/**********************************************************************/
int makeSparseCache(const Geometry  *geometry,
                    unsigned int     capacity,
                    unsigned int     zoneCount,
                    SparseCache    **cachePtr)
{
  unsigned int bytes
    = (sizeof(SparseCache) + (capacity * sizeof(CachedChapterIndex)));

  SparseCache *cache;
  int result = allocateCacheAligned(bytes, "sparse cache", &cache);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = initializeSparseCache(cache, geometry, capacity, zoneCount);
  if (result != UDS_SUCCESS) {
    freeSparseCache(cache);
    return result;
  }

  *cachePtr = cache;
  return UDS_SUCCESS;
}

/**********************************************************************/
size_t getSparseCacheMemorySize(const SparseCache *cache)
{
  // Count the ChapterIndexPage as cache memory, but ignore all other overhead.
  size_t pageSize = (sizeof(ChapterIndexPage) + cache->geometry->bytesPerPage);
  size_t chapterSize = (pageSize * cache->geometry->indexPagesPerChapter);
  return (cache->capacity * chapterSize);
}

/**
 * Update counters to reflect a chapter access hit and clear the skipSearch
 * flag on the chapter, if set.
 *
 * @param cache      the cache to update
 * @param chapter    the cache entry to update
 **/
static void scoreChapterHit(SparseCache        *cache,
                            CachedChapterIndex *chapter)
{
  cache->counters.chapterHits += 1;
  setSkipSearch(chapter, false);
}

/**
 * Update counters to reflect a chapter access miss.
 *
 * @param cache      the cache to update
 **/
static void scoreChapterMiss(SparseCache *cache)
{
  cache->counters.chapterMisses += 1;
}

/**
 * Check if the cache entry that is about to be replaced is already dead, and
 * if it's not, add to tally of evicted cache entries.
 *
 * @param cache      the cache to update
 * @param chapter    the cache entry about to be replaced
 **/
static void scoreEviction(SparseCache        *cache,
                          CachedChapterIndex *chapter)
{
  if (chapter->virtualChapter == UINT64_MAX) {
    return;
  }
  if (!chapter->invalid) {
    cache->counters.evictions += 1;
  }
  /*
   * Capture the hit/miss statistics from both evicted and invalidated
   * chapters--there's really no need to distinguish them, and it's safe to do
   * now since the zone zero thread is peforming the cache update.
   */
  sampleStatistic(&cache->statistics.searchHitsPerChapter,
                  chapter->counters.searchHits);
  sampleStatistic(&cache->statistics.searchMissesPerChapter,
                  chapter->counters.searchMisses);
}

/**
 * Update counters to reflect a cache search hit. This bumps the hit
 * count, clears the miss count, and clears the skipSearch flag.
 *
 * @param cache      the cache to update
 * @param chapter    the cache entry to update
 **/
static void scoreSearchHit(SparseCache        *cache,
                           CachedChapterIndex *chapter)
{
  cache->counters.searchHits += 1;
  chapter->counters.searchHits += 1;
  chapter->counters.consecutiveMisses = 0;
  setSkipSearch(chapter, false);
}

/**
 * Update counters to reflect a cache search miss. This bumps the consecutive
 * miss count, and if it goes over skipSearchThreshold, sets the skipSearch
 * flag on the chapter.
 *
 * @param cache      the cache to update
 * @param chapter    the cache entry to update
 **/
static void scoreSearchMiss(SparseCache        *cache,
                            CachedChapterIndex *chapter)
{
  cache->counters.searchMisses += 1;
  chapter->counters.searchMisses += 1;
  chapter->counters.consecutiveMisses += 1;
  if (chapter->counters.consecutiveMisses > cache->skipSearchThreshold) {
    setSkipSearch(chapter, true);
  }
}

/**********************************************************************/
void freeSparseCache(SparseCache *cache)
{
  if (cache == NULL) {
    return;
  }

  for (unsigned int i = 0; i < cache->zoneCount; i++) {
    freeSearchList(&cache->searchLists[i]);
  }

  for (unsigned int i = 0; i < cache->capacity; i++) {
    CachedChapterIndex *chapter = &cache->chapters[i];
    // Sample counters from the entries still in the cache at shutdown time.
    scoreEviction(cache, chapter);
    destroyCachedChapterIndex(chapter);
  }

  logStatistic(&cache->statistics.searchHitsPerChapter,
               "sparseCache searchHitsPerChapter");
  logStatistic(&cache->statistics.searchMissesPerChapter,
               "sparseCache searchMissesPerChapter");
  logStatistic(&cache->statistics.chaptersSearchedPerHit,
               "sparseCache chaptersSearchedPerHit");
  logStatistic(&cache->statistics.chaptersSearchedPerMiss,
               "sparseCache chaptersSearchedPerMiss");

  destroyBarrier(&cache->beginCacheUpdate);
  destroyBarrier(&cache->endCacheUpdate);
  FREE(cache);
}

/**********************************************************************/
CacheCounters getSparseCacheCounters(const SparseCache *cache)
{
  CacheCounters counters = {
    .sparseChapters = {
      .hits      = cache->counters.chapterHits,
      .misses    = cache->counters.chapterMisses,
    },
    .sparseSearches = {
      .hits      = cache->counters.searchHits,
      .misses    = cache->counters.searchMisses,
    },
    .evictions   = cache->counters.evictions,
    .expirations = cache->counters.invalidations,
  };
  return counters;
}

/**********************************************************************/
bool sparseCacheContains(SparseCache  *cache,
                         uint64_t      virtualChapter,
                         unsigned int  zoneNumber)
{
  /*
   * The correctness of the barriers depends on the invariant that between
   * calls to updateSparseCache(), the answers this function returns must
   * never vary--the result for a given chapter must be identical across
   * zones. That invariant must be maintained even if the chapter falls off
   * the end of the volume, or if searching it is disabled because of too many
   * search misses.
   */

  // Get the chapter search order for this zone thread.
  SearchListIterator iterator
    = iterateSearchList(cache->searchLists[zoneNumber], cache->chapters);
  while (hasNextChapter(&iterator)) {
    CachedChapterIndex *chapter = getNextChapter(&iterator);
    if (virtualChapter == chapter->virtualChapter) {
      if (zoneNumber == ZONE_ZERO) {
        scoreChapterHit(cache, chapter);
      }

      // Move the chapter to the front of the search list.
      rotateSearchList(iterator.list, iterator.nextEntry);
      return true;
    }
  }

  // The specified virtual chapter isn't cached.
  if (zoneNumber == ZONE_ZERO) {
    scoreChapterMiss(cache);
  }
  return false;
}

/**********************************************************************/
int updateSparseCache(SparseCache  *cache,
                      uint64_t      virtualChapter,
                      const Index  *index,
                      unsigned int  zoneNumber)
{
  // If the chapter is already in the cache, we don't need to do a thing
  // except update the search list order, which this check does.
  if (sparseCacheContains(cache, virtualChapter, zoneNumber)) {
    return UDS_SUCCESS;
  }

  // Wait for every zone thread to have reached its corresponding barrier
  // request and invoked this function before starting to modify the cache.
  enterBarrier(&cache->beginCacheUpdate, NULL);

  /*
   * This is the start of the critical section: the zone zero thread is
   * captain, effectively holding an exclusive lock on the sparse cache. All
   * the other zone threads must do nothing between the two barriers. They
   * will wait at the endCacheUpdate barrier for the captain to finish the
   * update.
   */

  int result = UDS_SUCCESS;
  if (zoneNumber == ZONE_ZERO) {
    // Purge invalid chapters from the LRU search list.
    SearchList *zoneZeroList = cache->searchLists[ZONE_ZERO];
    purgeSearchList(zoneZeroList, cache->chapters);

    // First check that the desired chapter is still in the volume. If it's
    // not, the hook fell out of the index and there's nothing to do for it.
    if (virtualChapter >= index->oldestVirtualChapter) {
      // Evict the least recently used live chapter, or replace a dead cache
      // entry, all by rotating the the last list entry to the front.
      CachedChapterIndex *victim
        = &cache->chapters[rotateSearchList(zoneZeroList, cache->capacity)];

      // Check if the victim is already dead, and if it's not, add to the
      // tally of evicted cache entries.
      scoreEviction(cache, victim);

      // Read the index page bytes and initialize the page array.
      result = cacheChapterIndex(victim, virtualChapter, index->volume);
    }

    // Copy the new search list state to all the other zone threads so they'll
    // get the result of pruning and see the new chapter.
    for (unsigned int zone = 1; zone < cache->zoneCount; zone++) {
      copySearchList(zoneZeroList, cache->searchLists[zone]);
    }
  }

  // This is the end of the critical section. All cache invariants must have
  // been restored--it will be shared/read-only again beyond the barrier.

  enterBarrier(&cache->endCacheUpdate, NULL);
  return result;
}

/**********************************************************************/
static void invalidateCachedChapter(SparseCache        *cache,
                                    CachedChapterIndex *chapter)
{
  // Don't count already-invalid entries as invalidated.
  if ((chapter->virtualChapter == UINT64_MAX) || chapter->invalid) {
    return;
  }

  // We must only flag the cache entry as invalid so the virtual chapter
  // will still appear to be in the cache for sparseCacheContains().
  chapter->invalid = true;

  // This chapter is getting kicked out for who it is, not because it it has
  // been selected as a victim.
  cache->counters.invalidations += 1;

  // This isn't being called by the zone zero thread, so it's not really safe
  // to mess with the cache statistics. scoreEviction() will handle it later.
}

/**********************************************************************/
void invalidateSparseCache(SparseCache *cache)
{
  if (cache == NULL) {
    return;
  }
  for (unsigned int i = 0; i < cache->capacity; i++) {
    invalidateCachedChapter(cache, &cache->chapters[i]);
  }
}

/**********************************************************************/
void invalidateSparseCacheChapter(SparseCache *cache,
                                  uint64_t     virtualChapter)
{
  if (cache == NULL) {
    return;
  }
  for (unsigned int i = 0; i < cache->capacity; i++) {
    CachedChapterIndex *chapter = &cache->chapters[i];
    if (virtualChapter == chapter->virtualChapter) {
      invalidateCachedChapter(cache, chapter);
      return;
    }
  }
}

/**********************************************************************/
int searchSparseCache(SparseCache        *cache,
                      const IndexPageMap *indexPageMap,
                      const UdsChunkName *name,
                      unsigned int        zoneNumber,
                      uint64_t           *virtualChapterPtr,
                      int                *recordPagePtr)
{
  // If the caller did not specify a virtual chapter, search the entire cache.
  bool searchAll = (*virtualChapterPtr == UINT64_MAX);
  unsigned int chaptersSearched = 0;

  // Get the chapter search order for this zone thread, searching the chapters
  // from most recently hit to least recently hit.
  SearchListIterator iterator
    = iterateSearchList(cache->searchLists[zoneNumber], cache->chapters);
  while (hasNextChapter(&iterator)) {
    CachedChapterIndex *chapter = getNextChapter(&iterator);

    // Skip chapters no longer cached, or that have too many search misses.
    if (shouldSkipChapterIndex(chapter, *virtualChapterPtr)) {
      continue;
    }

    int result = searchCachedChapterIndex(chapter, cache->geometry,
                                          indexPageMap, name, recordPagePtr);
    if (result != UDS_SUCCESS) {
      return result;
    }
    chaptersSearched += 1;

    // Did we find an index entry for the name?
    if (*recordPagePtr != NO_CHAPTER_INDEX_ENTRY) {
      if (zoneNumber == ZONE_ZERO) {
        sampleStatistic(&cache->statistics.chaptersSearchedPerHit,
                        chaptersSearched);
        scoreSearchHit(cache, chapter);
      }

      // Move the chapter to the front of the search list.
      rotateSearchList(iterator.list, iterator.nextEntry);

      // Return a matching entry as soon as it is found. It might be a false
      // collision that has a true match in another chapter, but that's a very
      // rare case and not worth the extra search cost or complexity.
      *virtualChapterPtr = chapter->virtualChapter;
      return UDS_SUCCESS;
    }

    if (zoneNumber == ZONE_ZERO) {
      scoreSearchMiss(cache, chapter);
    }

    if (!searchAll) {
      // We just searched the virtual chapter the caller specified and there
      // was no match, so we're done.
      break;
    }
  }

  // Don't bother sampling if we didn't search any chapters. Those misses are
  // very cheap, and the statistic is more informative without all the zeros.
  if ((zoneNumber == ZONE_ZERO) && (chaptersSearched > 0)) {
    sampleStatistic(&cache->statistics.chaptersSearchedPerMiss,
                    chaptersSearched);
  }

  // The name was not found in the cache.
  *recordPagePtr = NO_CHAPTER_INDEX_ENTRY;
  return UDS_SUCCESS;
}
