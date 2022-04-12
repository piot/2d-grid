/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef GRID_2D_H
#define GRID_2D_H

#include <basal/basal_rect2.h>
#include <stddef.h>

struct ImprintAllocator;

typedef struct Grid2dSlotEntry
{
    void *userData;
    struct Grid2dSlotEntry *nextEntry;
} Grid2dSlotEntry;

typedef struct Grid2dSlot
{
    Grid2dSlotEntry *firstEntry;
    size_t depth;
} Grid2dSlot;

typedef struct Grid2d
{
    Grid2dSlotEntry *preAllocatedSlotEntries;
    size_t preAllocatedSlotEntryIndex;
    size_t preAllocatedSlotEntryCapacity;
    Grid2dSlot *grid;
    bl_size2i gridSize;
    size_t gridSlotCount;
    size_t gridFactor;
    bl_vector2i origo;
    size_t maxDepth;
} Grid2d;

typedef struct Grid2dNodeResultEntry
{
    void *userData;
} Grid2dNodeResultEntry;

typedef struct Grid2dNodeResult
{
    Grid2dNodeResultEntry entries[64];
    size_t count;
    size_t capacity;
    size_t debugDepth;
} Grid2dNodeResult;

void grid2dInit(Grid2d *self, struct ImprintAllocator *memory, bl_vector2i origo, bl_size2i gridSize, size_t factor);
void grid2dClear(Grid2d *self);
void grid2dDestroy(Grid2d *self);
void grid2dQueryIntersects(const Grid2d *self, bl_recti *query,
                           Grid2dNodeResult *results);

void grid2dAdd(Grid2d *self, const bl_recti *rect, void *userData);
void grid2dDebugOutput(const Grid2d* self);

#endif
