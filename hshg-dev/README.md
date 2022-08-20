# Hierarchical Spatial Hashing Grid

# OUTDATED! See ../hshg2 for the new version

This directory is a dev version of a HSHG implementation. To find out production-suited code, see the `hshg` folder instead. Files in this folder might be behind the production code. The only reason for keeping them are useful comments in the code.

Everything that is said here is also true for the production version.

The source file (`hshg.c`) contains a lot of comments in different places explaining what's going on, so if this readme doesn't satisfy you, check it out.

To see usage examples, see `test.c`.

This implementation has been optimised to suit my personal needs. It might not be the best depending on what you want to do. Benchmark first to find out.

### How it works

An entity can only be assigned to one cell at a time, based on its middle point (`entity.x` and `entity.y` in `hshg_insert(hshg, entity)`). All entities have a squared AABB. Half of that square's side length is `entity.r`.

At initialisation (`hshg_init()`), you must tell how many cells on one axis you want to have, and what their size should be. These values will be put on the very first grid of the HSHG and the grid will be declared "the smallest". Any subsequent grids created to fit large entities will be created with twice as large cells, and on an axis there will be 2 times less of them.

All cells and cell sizes must be a power of 2.

It is a good idea to put a tight grid at the first layer (say, `1024x1024` with cell size being 16) to let the HSHG grow when needed.

It is legal to insert entities and let them live outside of the HSHG's reach - the grid can be 2x2 with a cell size of 1 pixel, and entities can still be millions of pixels away from it. Additionally, a system that I call "folding the XOY plane" is employed to speed up collision calculation. The way how it works can be illustrated like the following:

```
^ y
|6 7 8 8 7 6 6 7 8
|6 7 8 8 7 6 6 7 8
|3 4 5 5 4 3 3 4 5
|0 1 2 2 1 0 0 1 2
|0 1 2 2 1 0 0 1 2
|3 4 5 5 4 3 3 4 5
|6 7 8 8 7 6 6 7 8
|6 7 8 8 7 6 6 7 8
|3 4 5 5 4 3 3 4 5
|0 1 2 2 1 0 0 1 2
==================> x
```

The core grid is 3x3 cells with cell IDs `[0, 1, 2, 3, 4, 5, 6, 7, 8]` starting at `(0, 0)`. Later on, when entities outside of the grid need to be mapped to it, a system like shown above is employed to do so. Planes on the negative `x` and `y` are simply mirrored to achieve the same "folding" result as above. This system ensures that even if an entity is on an edge of 2 grids, or even 4, it can still interact with all of them in the 3x3 cell range that a HSHG requires to perform collision.

By default, some HSHG implementations do the following:

```
^ y
|6 7 8 6 7 8 6 7 8
|3 4 5 3 4 5 3 4 5
|0 1 2 0 1 2 0 1 2
|6 7 8 6 7 8 6 7 8
|3 4 5 3 4 5 3 4 5
|0 1 2 0 1 2 0 1 2
|6 7 8 6 7 8 6 7 8
|3 4 5 3 4 5 3 4 5
|0 1 2 0 1 2 0 1 2
==================> x
```

This system has a fundamental flaw. If an entity is on the edge of multiple grids, it can't trivially just check adjacent cells for collision - it needs to jump to the opposite side of these grids:

```
^ y
|6 7 8 6 7 8
|3 4 5 3 4 5
|0 1 2 0 1 2
============> x
```

Say we insert an entity at cell `3` to the right of middle. Due to spatial hashing, the entity's real cell will be `3` to the left, since in theory only the 3x3 grid to the left exists, and we only visualise the grid to the right and all other grids in the infinite plane. But then, as seen above, the middle cell `3` also collides with the cell `5` to the left of it. When spatial hashing is applied, the cell `5` is on the opposite side of the grid. That creates a problem when checking for collision.

On the other hand, when "folding the XOY plane":

```
^ y
|6 7 8 8 7 6
|3 4 5 5 4 3
|0 1 2 2 1 0
============> x
```

If we insert the entity at the exact same place, which is now cell `5` to the right of middle, and we apply spatial hashing, the real cell will turn out to be `5` to the left of middle, and we can still collide with every cell around the `5` like before without needing to check cells on the opposite side of the grid or other such shenanigans.

Note that the system is just about mirroring every even grid, but this is very similar to folding a linear piece of paper to form a square (our core grid) - you can fold it once to the right, once to the left.

Aside from this collision checking speedup, due to the number of cells and cell size needing to be a power of 2, it is also possible to know in constant time O(1) which grid to insert an entity to based on its radius and parameters of the first grid in the HSHG. See `hshg_get_grid()` in `hshg.c` for more details.

A query function doesn't exist. I haven't been able to think of a fast way of doing it with the "folding" idea. Best bet you have is to check for entities being in any range in the update callback (`hshg.update`, executed on every entity when `hshg_update()` is called).

In the update callback, if the entity moves (or you do anything that could move it), you must call `hshg_move(hshg, entity_id)`. If the entity changes its radius as well, call `hshg_resize(hshg, entity_id)`, `entity_id` being an `uint32_t` that's passed to the callbacks.

Given an entity ID, you can access it within the HSHG using `hshg->entities[entity_id]`.

**DO NOT** modify an entity during `hshg_collide()`. Instead, you should offload any changes to its properties on any external object (that you can reference to using `entity.ref`, you can set that property before `hshg_insert()`) and then apply changes in the update handler. Otherwise, collision might not be accurate, although faster and/or using less memory.

When deleting entities, the array of them doesn't shrink. To do that, use `safe_execute(hshg->entities = realloc(hshg->entities, sizeof(*hshg->entities) * hshg->entities_used), hshg->entities == NULL, ENOMEM)` and then `hshg->entities_size = hshg->entities_used`. **DO NOT** resize to quantities smaller than `1`, or the HSHG will break apart. The same stands for `hshg->grids[some_grid].used_cells`. You can use `hshg->grids_len` to loop through all layers and free up memory.