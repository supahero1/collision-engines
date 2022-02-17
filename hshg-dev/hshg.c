#include "hshg.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <shnet/error.h>

#include <stdio.h>

void hshg_create_grid(struct hshg* const hshg) {
  /*
   * This function doesn't really need to be insanely fast,
   * because it will only be called a few times at the
   * beginning of the hshg's lifetime when entities of
   * varying size are inserted.
   *
   * Having said that, you can of course prepare your HSHG
   * for entities in advance by calling this function a
   * few times. There are no checks, so call it carefully.
   * If you call it too many times, you're gonna crash.
   */
  ++hshg->grids_len;
  do {
    safe_execute(hshg->grids = realloc(hshg->grids, sizeof(*hshg->grids) * hshg->grids_len), hshg->grids == NULL, ENOMEM);
  } while(hshg->grids == NULL);
  struct hshg_grid* const restrict current = hshg->grids + hshg->grids_len - 1;
  struct hshg_grid* const restrict past = hshg->grids + hshg->grids_len - 2;
  current->cells_side = past->cells_side >> hshg->cell_div_log;
  current->cells_log = past->cells_log - hshg->cell_div_log;
  current->cells_mask = current->cells_side - 1;
  current->cell_size = past->cell_size << hshg->cell_div_log;
  current->inverse_cell_size = past->inverse_cell_size / (float)(1U << hshg->cell_div_log);
  do {
    safe_execute(current->cells = calloc(current->cells_side * current->cells_side, sizeof(*current->cells)), current->cells == NULL, ENOMEM);
  } while(current->cells == NULL);
  do {
    safe_execute(current->used_cells = malloc(sizeof(*current->used_cells)), current->used_cells == NULL, ENOMEM);
  } while(current->used_cells == NULL);
  current->used = 0;
  current->size = 1;
}

int hshg_init(struct hshg* const hshg, const uint32_t side, const uint32_t size) {
  assert(__builtin_popcount(side) == 1);
  assert(size > 0);
  if(hshg->cell_div_log == 0) {
    hshg->cell_div_log = 1;
  }
  if(hshg->entities_size == 0) {
    hshg->entities_size = 1;
  }
  hshg->entities_used = 1;
  if(hshg->entities_size > 1) {
    safe_execute(hshg->entities = malloc(sizeof(*hshg->entities) * hshg->entities_size), hshg->entities == NULL, ENOMEM);
    if(hshg->entities == NULL) {
      return -1;
    }
  }
  safe_execute(hshg->grids = malloc(sizeof(*hshg->grids)), hshg->grids == NULL, ENOMEM);
  if(hshg->grids == NULL) {
    free(hshg->entities);
    return -1;
  }
  hshg->grids_len = 1;
  safe_execute(hshg->grids[0].cells = calloc(side * side, sizeof(*hshg->grids[0].cells)), hshg->grids[0].cells == NULL, ENOMEM);
  if(hshg->grids[0].cells == NULL) {
    free(hshg->entities);
    free(hshg->grids);
    return -1;
  }
  hshg->grids[0].cells_side = side;
  hshg->grids[0].cells_log = __builtin_ctz(side);
  hshg->grids[0].cells_mask = side - 1;
  hshg->grids[0].cell_size = size;
  hshg->grids[0].inverse_cell_size = 1.0f / size;
  
  safe_execute(hshg->grids[0].used_cells = malloc(sizeof(*hshg->grids[0].used_cells)), hshg->grids[0].used_cells == NULL, ENOMEM);
  if(hshg->grids[0].used_cells == NULL) {
    free(hshg->entities);
    free(hshg->grids);
    free(hshg->grids[0].cells);
    return -1;
  }
  hshg->grids[0].used = 0;
  hshg->grids[0].size = 1;
  
  hshg->cell_log = 31 - __builtin_ctz(size);
  return 0;
}

void hshg_free(struct hshg* const hshg) {
  free(hshg->entities);
  hshg->entities = NULL;
  hshg->entities_used = 0;
  hshg->entities_size = 0;
  
  for(uint32_t i = 0; i < hshg->grids_len; ++i) {
    free(hshg->grids[i].cells);
    free(hshg->grids[i].used_cells);
  }
  free(hshg->grids);
  hshg->grids = NULL;
  hshg->grids_len = 0;
  
  hshg->free_entity = 0;
}

uint32_t hshg_get_entity(struct hshg* const hshg) {
  if(hshg->free_entity != 0) {
    const uint32_t ret = hshg->free_entity;
    hshg->free_entity = hshg->entities[ret].next;
    return ret;
  }
  if(hshg->entities_used == hshg->entities_size) {
    hshg->entities_size <<= 1;
    struct hshg_entity* ptr;
    do {
      safe_execute(ptr = realloc(hshg->entities, sizeof(*hshg->entities) * hshg->entities_size), ptr == NULL, ENOMEM);
    } while(ptr == NULL);
    hshg->entities = ptr;
  }
  return hshg->entities_used++;
}

void hshg_return_entity(struct hshg* const hshg, const uint32_t idx) {
  hshg->entities[idx].cell = UINT32_MAX;
  hshg->entities[idx].next = hshg->free_entity;
  hshg->free_entity = idx;
}

static int grid_is_empty(const struct hshg_grid* const grid, const uint32_t cell) {
  return grid->cells[cell].head == 0;
}

static void grid_get_used(struct hshg_grid* const grid) {
  if(grid->used < grid->size) return;
  do {
    grid->size <<= 1;
  } while(grid->used >= grid->size);
  do {
    safe_execute(grid->used_cells = realloc(grid->used_cells, sizeof(*grid->used_cells) * grid->size), grid->used_cells == NULL, ENOMEM);
  } while(grid->used_cells == NULL);
}

static void grid_set_used(struct hshg_grid* const grid, const uint32_t cell) {
  grid->used_cells[grid->used] = cell;
  grid->cells[cell].idx = grid->used++;
}

static void grid_set_unused(struct hshg_grid* const grid, const uint32_t cell) {
  grid->cells[grid->used_cells[--grid->used]].idx = grid->cells[cell].idx;
  grid->used_cells[grid->cells[cell].idx] = grid->used_cells[grid->used];
}

uint32_t grid_get_cell(const struct hshg_grid* const grid, const float x, const float y) {
  /*
   * This code presents the idea of "folding" the XOY plane
   * in order to map an infinite plane to finite number of
   * cells. It is optimised for cell numbers and cell sides
   * being powers of two. It can be implemented to work for
   * any numbers.
   *
   * This solution yields far better performance than
   * creating lookup tables to know which cells to collide
   * with, which is the way some old papers on HSHG claim
   * to be the best or "the only" one.
   */
  uint32_t ret;
  uint32_t cell = (x < 0 ? -x : x) * grid->inverse_cell_size;
  /*(cell % grid->cells_x) & 1 <- check if its odd */
  if(cell & grid->cells_side) {
    ret = grid->cells_mask - (cell & grid->cells_mask);
  } else {
    ret = cell & grid->cells_mask;
  }
  cell = (y < 0 ? -y : y) * grid->inverse_cell_size;
  if(cell & grid->cells_side) {
  /*ret += ----------------------------------------------  * grid->cells_x;*/
    ret |= (grid->cells_mask - (cell & grid->cells_mask)) << grid->cells_log;
  } else {
    ret |= (cell & grid->cells_mask) << grid->cells_log;
  }
  return ret;
}

uint32_t hshg_get_grid(const struct hshg* const hshg, const float r) {
  /*
   * Hacky way of getting the grid to which insert an
   * entity in O(1). Only works due to cell count and
   * cell sizes being a power of 2.
   *
   * Note that this function can go way out of bounds
   * of hshg->grids_len, but this is necessary to
   * be able to create new grids in hshg_insert().
   */
  const uint32_t rounded = r * 2.0f;
  if(rounded < hshg->grids[0].cell_size) {
    return 0;
  }
  /*
   * uint32_t grid = 32 - __builtin_clz(rounded);
   * grid -= __builtin_ctz(hshg->grids[0].cell_size);
   * grid += hshg->cell_div_log - 1;
   * grid /= hshg->cell_div_log;
   *
   * hshg->cell_log = 31 - __builtin_ctz(hshg->grids[0].cell_size);
   */
  return (hshg->cell_log - __builtin_clz(rounded)) / hshg->cell_div_log + 1;
}

uint32_t hshg_get_grid_safe(const struct hshg* const hshg, const float r) {
  const uint32_t grid = hshg_get_grid(hshg, r);
  if(grid >= hshg->grids_len) {
    return hshg->grids_len - 1;
  }
  return grid;
}

uint32_t hshg_insert(struct hshg* const restrict hshg, const struct hshg_entity* const restrict entity) {
  /*
   * Insert the entity to a grid that can fit it.
   *
   * A grid can fit the entity if the entity's diameter is
   * smaller than the grid's cell size. Otherwise, rules
   * of a grid are fundamentally violated. Rules are NOT
   * violated when there's 4 or less cells in a grid (2x2).
   */
  uint32_t grid = hshg_get_grid(hshg, entity->r);
  /*
   * Create as many grids as necessary to fit the entity.
   */
  if(grid >= hshg->grids_len) {
    for(uint32_t i = hshg->grids_len; i <= grid && hshg->grids[hshg->grids_len - 1].cells_side > 2; ++i) {
      hshg_create_grid(hshg);
    }
    grid = hshg->grids_len - 1;
  }
  
  const uint32_t idx = hshg_get_entity(hshg);
  const uint32_t cell = grid_get_cell(hshg->grids + grid, entity->x, entity->y);
  hshg->entities[idx].cell = cell;
  hshg->entities[idx].next = hshg->grids[grid].cells[cell].head;
  hshg->entities[idx].x = entity->x;
  hshg->entities[idx].y = entity->y;
  hshg->entities[idx].r = entity->r;
  hshg->entities[idx].ref = entity->ref;
  if(grid_is_empty(hshg->grids + grid, cell)) {
    /*
     * At the cost of one cell being twice as big, an array
     * of used cells speeds up collision checks by a lot.
     */
    grid_get_used(hshg->grids + grid);
    grid_set_used(hshg->grids + grid, cell);
  }
  hshg->grids[grid].cells[cell].head = idx;
  return idx;
}

void hshg_remove_light(const struct hshg* const hshg, const uint32_t idx, uint32_t grid) {
  uint32_t i = hshg->grids[grid].cells[hshg->entities[idx].cell].head;
  uint32_t prev = 0;
  while(i != idx) {
    if(i == 0) {
      printf("grid %u cell %u idx %u\n", grid, hshg->entities[idx].cell, idx);
      assert(0);
    }
    prev = i;
    i = hshg->entities[i].next;
  }
  if(prev == 0) {
    hshg->grids[grid].cells[hshg->entities[i].cell].head = hshg->entities[i].next;
    if(grid_is_empty(hshg->grids + grid, hshg->entities[idx].cell)) {
      grid_set_unused(hshg->grids + grid, hshg->entities[idx].cell);
    }
  } else {
    hshg->entities[prev].next = hshg->entities[i].next;
  }
}

void hshg_remove(struct hshg* const hshg, const uint32_t idx) {
  const uint32_t grid = hshg_get_grid_safe(hshg, hshg->entities[idx].r);
  hshg_remove_light(hshg, idx, grid);
  hshg_return_entity(hshg, idx);
}

uint32_t hshg_ptr_to_idx(const struct hshg* const hshg, const struct hshg_entity* const entity) {
  return ((uintptr_t) entity - (uintptr_t) hshg->entities) / sizeof(*entity);
}

void hshg_move(struct hshg* const hshg, const uint32_t idx) {
  const uint32_t grid = hshg_get_grid_safe(hshg, hshg->entities[idx].r);
  const uint32_t cell = grid_get_cell(hshg->grids + grid, hshg->entities[idx].x, hshg->entities[idx].y);
  if(hshg->entities[idx].cell != cell) {
    hshg_remove_light(hshg, idx, grid);
    hshg->entities[idx].cell = cell;
    hshg->entities[idx].next = hshg->grids[grid].cells[cell].head;
    if(grid_is_empty(hshg->grids + grid, cell)) {
      grid_get_used(hshg->grids + grid);
      grid_set_used(hshg->grids + grid, cell);
    }
    hshg->grids[grid].cells[cell].head = idx;
  }
}

void hshg_resize(struct hshg* const hshg, const uint32_t idx, const uint32_t old_grid) {
  const uint32_t grid = hshg_get_grid(hshg, hshg->entities[idx].r);
  if(old_grid != grid) {
    struct hshg_entity ent = hshg->entities[idx];
    /*
     * The index stays the same.
     *
     * There's no need to adjust the "grid" variable.
     * hshg_insert() will add any new grids for us.
     */
    hshg_remove(hshg, idx);
    hshg_insert(hshg, &ent);
  }
}

void hshg_update(struct hshg* const hshg) {
  for(uint32_t i = 1; i < hshg->entities_used; ++i) {
    if(hshg->entities[i].cell == UINT32_MAX) continue;
    //uint32_t old_head = hshg->grids[1].cells[2].head;
    hshg->update(hshg, hshg->entities + i);
    /*if(hshg->grids[1].cells[2].head == 0 && old_head != 0) {
      printf("i = %u\n", i);
      assert(0);
    }*/
  }
}

void hshg_collide(struct hshg* const hshg) {
  for(uint32_t grid = 0; grid < hshg->grids_len; ++grid) {
    for(uint32_t id = 0, cid = hshg->grids[grid].used_cells[id]; id < hshg->grids[grid].used; ++id, cid = hshg->grids[grid].used_cells[id]) {
      for(uint32_t i = hshg->grids[grid].cells[cid].head; i != 0; i = hshg->entities[i].next) {
        /*
         * Collide within the same cell
         */
        for(uint32_t j = hshg->entities[i].next; j != 0; j = hshg->entities[j].next) {
          hshg->collide(hshg, hshg->entities + i, hshg->entities + j);
        }
        /*
         * Collide with 4 adjacent cells
         * o o o
         * x o o
         * x x x
         */
        uint32_t cell_x = cid & hshg->grids[grid].cells_mask;
        uint32_t cell_y = cid >> hshg->grids[grid].cells_log;
        if(cell_x != 0) {
          for(uint32_t j = hshg->grids[grid].cells[cid - 1].head; j != 0; j = hshg->entities[j].next) {
            hshg->collide(hshg, hshg->entities + i, hshg->entities + j);
          }
          if(cell_y != 0) {
            for(uint32_t j = hshg->grids[grid].cells[cid - hshg->grids[grid].cells_side - 1].head; j != 0; j = hshg->entities[j].next) {
              hshg->collide(hshg, hshg->entities + i, hshg->entities + j);
            }
          }
        }
        if(cell_y != 0) {
          for(uint32_t j = hshg->grids[grid].cells[cid - hshg->grids[grid].cells_side].head; j != 0; j = hshg->entities[j].next) {
            hshg->collide(hshg, hshg->entities + i, hshg->entities + j);
          }
          if(cell_x != hshg->grids[grid].cells_mask) {
            for(uint32_t j = hshg->grids[grid].cells[cid - hshg->grids[grid].cells_side + 1].head; j != 0; j = hshg->entities[j].next) {
              hshg->collide(hshg, hshg->entities + i, hshg->entities + j);
            }
          }
        }
        /*
         * Collide with upper layers
         */
        for(uint32_t up_grid = grid + 1; up_grid < hshg->grids_len; ++up_grid) {
          cell_x >>= 1;
          cell_y >>= 1;
          const uint32_t min_x = cell_x == 0 ? 0 : cell_x - 1;
          const uint32_t min_y = cell_y == 0 ? 0 : cell_y - 1;
          const uint32_t max_x = cell_x == hshg->grids[up_grid].cells_mask ? cell_x : cell_x + 1;
          const uint32_t max_y = cell_y == hshg->grids[up_grid].cells_mask ? cell_y : cell_y + 1;
          for(uint32_t cur_x = min_x; cur_x <= max_x; ++cur_x) {
            for(uint32_t cur_y = min_y; cur_y <= max_y; ++cur_y) {
              for(uint32_t j = hshg->grids[up_grid].cells[cur_x | (cur_y << hshg->grids[up_grid].cells_log)].head; j != 0; j = hshg->entities[j].next) {
                hshg->collide(hshg, hshg->entities + i, hshg->entities + j);
              }
            }
          }
        }
        /*
         * Done
         */
      }
    }
  }
}
