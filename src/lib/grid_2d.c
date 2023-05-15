/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <basal/rect_utils.h>
#include <clog/clog.h>
#include <grid-2d/grid_2d.h>
#include <imprint/allocator.h>
#include <stdint.h>
#include <tiny-libc/tiny_libc.h>

#define GRID_2D_FORCE_INLINE TC_FORCE_INLINE

void grid2dInit(Grid2d *self, struct ImprintAllocator *memory, BlVector2i origo, BlSize2i gridSize, size_t factor)
{
    self->origo = origo;
    self->gridFactor = factor;
    self->gridSize = gridSize;

    self->preAllocatedSlotEntryIndex = 0;
    self->preAllocatedSlotEntryCapacity = 64 * 1024;
    self->preAllocatedSlotEntries = IMPRINT_CALLOC_TYPE_COUNT(
        memory, Grid2dSlotEntry, self->preAllocatedSlotEntryCapacity);

    self->gridSlotCount = gridSize.width * gridSize.height;
    self->grid = IMPRINT_CALLOC_TYPE_COUNT(
        memory, Grid2dSlot, self->gridSlotCount);
    self->maxDepth = 0;
}

void grid2dClear(Grid2d *self)
{
    self->preAllocatedSlotEntryIndex = 0;
    tc_mem_clear(self->grid, self->gridSlotCount * sizeof(Grid2dSlot));
}

void grid2dDestroy(Grid2d *self)
{
    //imprintMemoryFree(self->memory, self->preAllocatedSlotEntries);
    //imprintMemoryFree(self->memory, self->grid);
}

static inline GRID_2D_FORCE_INLINE Grid2dSlot *getSlotFromIndex(Grid2d *self, size_t offset)
{
    if (offset >= self->gridSlotCount)
    {
        CLOG_ERROR("Grid2dSlot: Illegal position %zu", offset)
    }

    return &self->grid[offset];
}

static inline const Grid2dSlot *getConstSlotFromIndex(const Grid2d *self, size_t offset)
{
    return (const Grid2dSlot *)getSlotFromIndex((Grid2d *)self, offset);
}

void grid2dDebugOutput(const Grid2d *self)
{
    CLOG_OUTPUT_STDERR("grid2d slotCount: %zu slotEntries: %zu", self->gridSlotCount, self->preAllocatedSlotEntryIndex)
}

static inline GRID_2D_FORCE_INLINE size_t worldPositionToGridIndex(const Grid2d *self, const BlVector2i *a)
{
    int x = (a->x - self->origo.x) / self->gridFactor;
    int y = (a->y - self->origo.y) / self->gridFactor;
#if CONFIGURATION_DEBUG
    if (x < 0 || y < 0)
    {
        CLOG_ERROR("negative coordinates is not supported")
    }

    if (x >= self->gridSize.width || y >= self->gridSize.height)
    {
        CLOG_ERROR("position is out of bounds world %d,%d grid %d x %d", a->x, a->y, self->gridSize.width, self->gridSize.height)
    }
#endif

    return y * self->gridSize.width + x;
}

static inline GRID_2D_FORCE_INLINE size_t worldRectToGridIndexes(const Grid2d *self, const BlRecti *worldRect, size_t *target, size_t maxCount)
{
    if (maxCount < 4)
    {
        CLOG_ERROR("must support four slots")
    }
    size_t lowerLeftGridIndex = worldPositionToGridIndex(self, &worldRect->vector);

    BlVector2i lowerRight = {worldRect->vector.x + worldRect->size.width, worldRect->vector.y};
    size_t lowerRightGridIndex = worldPositionToGridIndex(self, &lowerRight);

    BlVector2i upperRight = {worldRect->vector.x + worldRect->size.width, worldRect->vector.y + worldRect->size.height};
    size_t upperRightGridIndex = worldPositionToGridIndex(self, &upperRight);

    BlVector2i upperLeft = {worldRect->vector.x, worldRect->vector.y + worldRect->size.height};
    size_t upperLeftGridIndex = worldPositionToGridIndex(self, &upperLeft);

    target[0] = lowerLeftGridIndex;
    target[1] = lowerRightGridIndex;
    target[2] = upperLeftGridIndex;
    target[3] = upperRightGridIndex;

    return 4;
}

static void grid2dNodeResultInit(Grid2dNodeResult *self)
{
    self->capacity = 64;
    self->count = 0;
    self->debugDepth = 0;
}

static void addResult(Grid2dNodeResult *self, void *userData)
{
    if (self->count == self->capacity)
    {
        CLOG_ERROR("out of memory addResult")
        return;
    }

    for (size_t i = 0; i < self->count; ++i)
    {
        const Grid2dNodeResultEntry *previousAddedEntry = &self->entries[i];
        if ((uintptr_t)previousAddedEntry->userData == (uintptr_t)userData)
        {
            return;
        }
    }

    Grid2dNodeResultEntry *entry = &self->entries[self->count++];
    entry->userData = userData;
}

static inline GRID_2D_FORCE_INLINE void
findOverlaps(const Grid2dSlot *slot,
             Grid2dNodeResult *results)
{
    Grid2dSlotEntry *entry = slot->firstEntry;
    while (entry)
    {
        addResult(results, entry->userData);

        entry = entry->nextEntry;
    }
}

void grid2dQueryIntersects(const Grid2d *self, BlRecti *query,
                           Grid2dNodeResult *results)
{
    size_t indexes[4];
    size_t foundIndexes = worldRectToGridIndexes(self, query, indexes, 4);

    grid2dNodeResultInit(results);
    for (size_t i = 0; i < foundIndexes; ++i)
    {
        const Grid2dSlot *slot = getConstSlotFromIndex(self, indexes[i]);
        findOverlaps(slot, results);
    }
}

static inline GRID_2D_FORCE_INLINE Grid2dSlotEntry *grid2dAllocateSlotEntry(Grid2d *self, void *userData)
{
    if (self->preAllocatedSlotEntryIndex >= self->preAllocatedSlotEntryCapacity)
    {
        CLOG_ERROR("Out of node spaces for slot entry. Allocated %zu of %zu",
                   self->preAllocatedSlotEntryIndex, self->preAllocatedSlotEntryCapacity)
    }
    Grid2dSlotEntry *slotEntry = &self->preAllocatedSlotEntries[self->preAllocatedSlotEntryIndex];
    slotEntry->userData = userData;
    slotEntry->nextEntry = 0;
    self->preAllocatedSlotEntryIndex++;
    return slotEntry;
}

void grid2dAdd(Grid2d *self, const BlRecti *rect, void *userData)
{
    size_t indexes[4];
    size_t foundIndexes = worldRectToGridIndexes(self, rect, indexes, 4);

    for (size_t i = 0; i < foundIndexes; ++i)
    {
        Grid2dSlot *slot = getSlotFromIndex(self, indexes[i]);

        Grid2dSlotEntry *newEntry = grid2dAllocateSlotEntry(self, userData);
        newEntry->nextEntry = slot->firstEntry;
        slot->firstEntry = newEntry;

        slot->depth++;
        if (slot->depth > self->maxDepth) {
          self->maxDepth = slot->depth;
        }
    }
}
