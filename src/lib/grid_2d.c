/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <grid-2d/grid_2d.h>
#include <basal/basal_rect_utils.h>
#include <clog/clog.h>
#include <imprint/memory.h>

#if _MSC_VER
#define GRID_2D_FORCE_INLINE __forceinline
#else
#define GRID_2D_FORCE_INLINE __attribute__((__always_inline__))
#endif

void grid2dInit(Grid2d *self, ImprintMemory *memory, bl_vector2i origo, bl_size2i gridSize, size_t factor)
{
    self->memory = memory;
    self->origo = origo;
    self->gridFactor = factor;
    self->gridSize = gridSize;

    self->preAllocatedNodeIndex = 0;
    self->preAllocatedNodeCapacity = 32 * 1024;
    self->preAllocatedNodes = IMPRINT_MEMORY_ALLOC_TYPE_COUNT(
        memory, Grid2dNode, self->preAllocatedNodeCapacity);

    self->preAllocatedSlotEntryIndex = 0;
    self->preAllocatedSlotEntryCapacity = 64 * 1024;
    self->preAllocatedSlotEntries = IMPRINT_MEMORY_ALLOC_TYPE_COUNT(
        memory, Grid2dSlotEntry, self->preAllocatedSlotEntryCapacity);

    self->gridSlotCount = gridSize.width * gridSize.height;
    self->grid = IMPRINT_MEMORY_CALLOC_TYPE_COUNT(
        memory, Grid2dSlot, self->gridSlotCount);
}

void grid2dClear(Grid2d *self)
{
    self->preAllocatedNodeIndex = 0;
    self->preAllocatedSlotEntryIndex = 0;
    tc_mem_clear_type_array(self->grid, self->gridSlotCount);
}

void grid2dDestroy(Grid2d *self)
{
    imprintMemoryFree(self->memory, self->preAllocatedNodes);
    imprintMemoryFree(self->memory, self->preAllocatedSlotEntries);
    imprintMemoryFree(self->memory, self->grid);
}

static inline GRID_2D_FORCE_INLINE Grid2dSlot *getSlotFromIndex(Grid2d *self, size_t offset)
{
    if (offset >= self->gridSlotCount)
    {
        CLOG_ERROR("Illegal position")
    }

    return &self->grid[offset];
}

static inline const Grid2dSlot *getConstSlotFromIndex(const Grid2d *self, size_t offset)
{
    return (const Grid2dSlot *)getSlotFromIndex((Grid2d *)self, offset);
}

void grid2dDebugOutput(const Grid2d* self)
{
  CLOG_OUTPUT_STDERR("grid2d slotCount: %zu slotEntries: %zu nodes: %zu", self->gridSlotCount, self->preAllocatedSlotEntryIndex, self->preAllocatedNodeIndex)
}

/*
static inline Grid2dSlot *getSlot(Grid2d *self, bl_vector2i pos)
{
    if (pos.x < 0 || pos.y < 0 || pos.x >= self->gridSize.width || pos.y >= self->gridSize.height)
    {
        CLOG_ERROR("Illegal position");
    }

    size_t offset = pos.y * self->gridSize.width + pos.x;

    return getSlotFromIndex(self, offset);
}


static inline void worldPositionToGridPosition(const Grid2d *self, bl_vector2i *result, const bl_vector2i *a)
{
    result->x = (a->x - self->origo.x) / self->gridFactor;
    result->y = (a->y - self->origo.y) / self->gridFactor;
}
*/

static inline GRID_2D_FORCE_INLINE size_t worldPositionToGridIndex(const Grid2d *self, const bl_vector2i *a)
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

static inline GRID_2D_FORCE_INLINE size_t worldRectToGridIndexes(const Grid2d *self, const bl_recti *worldRect, size_t *target, size_t maxCount)
{
    if (maxCount < 4)
    {
        CLOG_ERROR("must support four slots")
    }
    size_t lowerLeftGridIndex = worldPositionToGridIndex(self, &worldRect->vector);

    bl_vector2i lowerRight = {worldRect->vector.x + worldRect->size.width, worldRect->vector.y};
    size_t lowerRightGridIndex = worldPositionToGridIndex(self, &lowerRight);

    bl_vector2i upperRight = {worldRect->vector.x + worldRect->size.width, worldRect->vector.y + worldRect->size.height};
    size_t upperRightGridIndex = worldPositionToGridIndex(self, &upperRight);

    bl_vector2i upperLeft = {worldRect->vector.x, worldRect->vector.y + worldRect->size.height};
    size_t upperLeftGridIndex = worldPositionToGridIndex(self, &upperLeft);

    target[0] = lowerLeftGridIndex;
    target[1] = lowerRightGridIndex;
    target[2] = upperLeftGridIndex;
    target[3] = upperRightGridIndex;

    return 4;
}

static inline GRID_2D_FORCE_INLINE Grid2dNode *grid2dAllocateNode(Grid2d *self, const bl_recti* rect, void *userData)
{
    if (self->preAllocatedNodeIndex >= self->preAllocatedNodeCapacity)
    {
        CLOG_ERROR("Out of node spaces for node. Allocated %zu of %zu",
                   self->preAllocatedNodeIndex, self->preAllocatedNodeCapacity)
    }
    Grid2dNode *node = &self->preAllocatedNodes[self->preAllocatedNodeIndex];
    node->userData = userData;
    node->rect = *rect;
    self->preAllocatedNodeIndex++;
    return node;
}

static void grid2dNodeResultInit(Grid2dNodeResult *self)
{
    self->capacity = 32;
    self->count = 0;
    self->debugDepth = 0;
}

static void addResult(Grid2dNodeResult *self, const Grid2dNode *node)
{
    if (self->count == self->capacity)
    {
        CLOG_ERROR("out of memory addResult")
        return;
    }

    for (size_t i = 0; i < self->count; ++i)
    {
        Grid2dNodeResultEntry *previousAddedEntry = &self->entries[i];
        if (previousAddedEntry->node == node)
        {
            return;
        }
    }

    Grid2dNodeResultEntry *entry = &self->entries[self->count++];
    entry->node = node;
    entry->userData = node->userData;
    entry->rect = node->rect;
}

static inline GRID_2D_FORCE_INLINE void
findOverlaps(const Grid2dSlot *slot, bl_recti *query,
             Grid2dNodeResult *results)
{
    Grid2dSlotEntry *entry = slot->firstEntry;
    while (entry)
    {
        bool intersects = bl_recti_is_intersect(&entry->node->rect, query);
        if (intersects)
        {
          addResult(results, entry->node);
        }

        entry = entry->nextEntry;
    }
}

void grid2dQueryIntersects(const Grid2d *self, bl_recti *query,
                           Grid2dNodeResult *results)
{
    size_t indexes[4];
    size_t foundIndexes = worldRectToGridIndexes(self, query, indexes, 4);

    grid2dNodeResultInit(results);

    for (size_t i = 0; i < foundIndexes; ++i)
    {
        const Grid2dSlot *slot = getConstSlotFromIndex(self, indexes[i]);
        findOverlaps(slot, query, results);
    }
}

static inline GRID_2D_FORCE_INLINE Grid2dSlotEntry *grid2dAllocateSlotEntry(Grid2d *self, Grid2dNode *node)
{
    if (self->preAllocatedSlotEntryIndex >= self->preAllocatedSlotEntryCapacity)
    {
        CLOG_ERROR("Out of node spaces for slot entry. Allocated %zu of %zu",
                   self->preAllocatedSlotEntryIndex, self->preAllocatedSlotEntryCapacity)
    }
    Grid2dSlotEntry *slotEntry = &self->preAllocatedSlotEntries[self->preAllocatedSlotEntryIndex];
    slotEntry->node = node;
    slotEntry->nextEntry = 0;
    self->preAllocatedSlotEntryIndex++;
    return slotEntry;
}

void grid2dAdd(Grid2d *self, const bl_recti *rect, void *userData)
{
    size_t indexes[4];
    size_t foundIndexes = worldRectToGridIndexes(self, rect, indexes, 4);

    Grid2dNode *newNode = grid2dAllocateNode(self, rect, userData);

    for (size_t i = 0; i < foundIndexes; ++i)
    {
        Grid2dSlot *slot = getSlotFromIndex(self, indexes[i]);

        Grid2dSlotEntry *entry = slot->firstEntry;
        Grid2dSlotEntry *newEntry = grid2dAllocateSlotEntry(self, newNode);

        if (!entry)
        {
            slot->firstEntry = newEntry;
            continue;
        }

        while (entry->nextEntry)
        {
            entry = entry->nextEntry;
        }

        entry->nextEntry = newEntry;
    }
}
