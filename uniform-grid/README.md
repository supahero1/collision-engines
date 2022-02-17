# Uniform grid

The grid may be used for other structures such as a Hierarchical Spatial Hashing Grid (HSHG) or as a standalone collision engine. That's because it has support for objects larger than a cell.

The grid is using simple spatial hashing to find cells objects belong to in constant time. Removal is amortized constant time, given that the grid will be properly optimised (the right number of cells and cell size compared to size and number of objects).

Right now there are no cell-based optimisations that aim to bring down memory requirements. A cell only requires 4 bytes of memory, so optimisations shouldn't be necessary. A 1024x1024 grid will take only roughly 4MB.

The grid is making sure that the same objects aren't collided multiple times, or queried multiple times, by keeping track of a bitfield that is zeroed at the start. Each object gets a bit in the bitfield and its bit is set to 1 if it collided with the currently examined object. When the currently examined object is fully examined, and the next one is picked, the bits are zeroed in amortized constant time (yes, not all of the bits are zeroed).

Object spatial hash constrains (64bits) limit the maximum amount of cells to 65535. That should be more than enough though.

The grid's origin is always at `(0, 0)`. Only positive width and height may be specified. Besides of the grid itself, objects and positions, including the rectangle viewport given to `grid_query()` have their center at the specified `(x, y)`. Width and height must be half of the real width and height of the AABB, so that `(x - w, y - h)` is the top left corner of the AABB.

Over time, if amount of entities varies or their position changes from distributed to localized, it might be worth cleaning up unused memory by quietly doing
```c
grid.entities = realloc(grid.entities, sizeof(struct grid_entity) * grid->entities_size);
```
and
```c
grid.node_entities = realloc(grid.node_entities, sizeof(struct grid_node_entity) * grid->entities_size);
```

# benchmarking
The `test.c` file may be used to benchmark the grid with various settings. On an i5-9600K 3.7GHZ 1 core, 300k objects on an 200kx200k arena with grid resolution 125 results in 60fps, compiled with `-O3 -fomit-frame-pointer`.

# usage
Look `test.c`. The grid shall be zeroed, then basic members shall be initialised and `grid_init()` shall be called. At this point the grid may be used for collision.

`grid_insert()` inserts a new object with the given AABB and reference that may be a pointer or an index so that the application can map the grid object to it's external object or do anything else. The function returns an index of the created object that may be used to remove it from the grid. Otherwise it is meaningless to the application. Setting `spatial_hash` is meaningless and should not be done.

`grid_update()` will call `grid.onupdate(grid, object)` on every object in the grid. The purpose is to update positions of objects. If it was done when colliding, it could be that a collision would only result in 1 object being informed of the collision, or other weird phenomena.

`grid_collide()` will test all objects for collision. The application MUST NOT update position of the objects, but rather save the new acceleration and/or velocity in an external object (that it can access using `object.ref`) and then apply it in the `onupdate` function.

`grid_query()` will call an application-provided callback on every object that fits within the provided rectangle. It will never call the function with a duplicate object.
