#include "hshg.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <shnet/error.h>

static void hshg_create_grid(struct hshg* const hshg) {
  ++hshg->grids_len;
  void* ptr;
  do {
    safe_execute(ptr = realloc(hshg->grids, sizeof(*hshg->grids) * hshg->grids_len), ptr == NULL, ENOMEM);
  } while(ptr == NULL);
  hshg->grids = ptr;
  struct hshg_grid* const current = hshg->grids + hshg->grids_len - 1;
  struct hshg_grid* const past = hshg->grids + hshg->grids_len - 2;
  current->cells_side = past->cells_side >> hshg->cell_div_log;
  current->cells_log = past->cells_log - hshg->cell_div_log;
  current->cells_mask = current->cells_side - 1;
  current->cell_size = past->cell_size << hshg->cell_div_log;
  current->inverse_cell_size = past->inverse_cell_size / (float)(1U << hshg->cell_div_log);
  do {
    safe_execute(ptr = calloc(current->cells_side * current->cells_side, sizeof(*current->cells)), ptr == NULL, ENOMEM);
  } while(ptr == NULL);
  current->cells = ptr;
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
  
  hshg->cell_log = 31 - __builtin_ctz(size);
  return 0;
}

void hshg_free(struct hshg* const hshg) {
  free(hshg->entities);
  hshg->entities = NULL;
  hshg->entities_used = 0;
  hshg->entities_size = 0;
  hshg->free_entity = 0;
  
  for(uint32_t i = 0; i < hshg->grids_len; ++i) {
    free(hshg->grids[i].cells);
  }
  free(hshg->grids);
  hshg->grids = NULL;
  hshg->grids_len = 0;
}

static uint32_t hshg_get_entity(struct hshg* const hshg) {
  if(hshg->free_entity != 0) {
    const uint32_t ret = hshg->free_entity;
    hshg->free_entity = hshg->entities[ret].next;
    return ret;
  }
  if(hshg->entities_used == hshg->entities_size) {
    hshg->entities_size <<= 1;
    void* ptr;
    do {
      safe_execute(ptr = realloc(hshg->entities, sizeof(*hshg->entities) * hshg->entities_size), ptr == NULL, ENOMEM);
    } while(ptr == NULL);
    hshg->entities = ptr;
  }
  return hshg->entities_used++;
}

static void hshg_return_entity(struct hshg* const hshg, const uint32_t idx) {
  hshg->entities[idx].cell = UINT32_MAX;
  hshg->entities[idx].next = hshg->free_entity;
  hshg->free_entity = idx;
}

static uint32_t grid_get_cell_(const struct hshg_grid* const grid, const float x) {
  const uint32_t cell = (x < 0 ? -x : x) * grid->inverse_cell_size;
  if(cell & grid->cells_side) {
    return grid->cells_mask - (cell & grid->cells_mask);
  } else {
    return cell & grid->cells_mask;
  }
}

static uint32_t grid_get_cell(const struct hshg_grid* const grid, const float x, const float y) {
  return grid_get_cell_(grid, x) | (grid_get_cell_(grid, y) << grid->cells_log);
}

static uint32_t hshg_get_grid(const struct hshg* const hshg, const float r) {
  const uint32_t rounded = r + r;
  if(rounded < hshg->grids[0].cell_size) {
    return 0;
  }
  return (hshg->cell_log - __builtin_clz(rounded)) / hshg->cell_div_log + 1;
}

static uint32_t hshg_get_grid_safe(const struct hshg* const hshg, const float r) {
  const uint32_t grid = hshg_get_grid(hshg, r);
  if(grid >= hshg->grids_len) {
    return hshg->grids_len - 1;
  }
  return grid;
}

uint32_t hshg_insert(struct hshg* const hshg, const struct hshg_entity* const entity) {
  uint32_t grid = hshg_get_grid(hshg, entity->r);
  if(grid >= hshg->grids_len) {
    for(uint32_t i = hshg->grids_len; i <= grid && hshg->grids[hshg->grids_len - 1].cells_side > 2; ++i) {
      hshg_create_grid(hshg);
    }
    grid = hshg->grids_len - 1;
  }
  
  const uint32_t idx = hshg_get_entity(hshg);
  const uint32_t cell = grid_get_cell(hshg->grids + grid, entity->x, entity->y);
  hshg->entities[idx].cell = cell;
  hshg->entities[idx].next = hshg->grids[grid].cells[cell];
  if(hshg->entities[idx].next != 0) {
    hshg->entities[hshg->entities[idx].next].prev = idx;
  }
  hshg->entities[idx].prev = 0;
  hshg->entities[idx].grid = grid;
  hshg->entities[idx].x = entity->x;
  hshg->entities[idx].y = entity->y;
  hshg->entities[idx].r = entity->r;
  hshg->entities[idx].ref = entity->ref;
  hshg->grids[grid].cells[cell] = idx;
  return idx;
}

static void hshg_remove_light(const struct hshg* const hshg, const uint32_t idx, uint32_t grid) {
  if(hshg->entities[idx].prev == 0) {
    hshg->grids[grid].cells[hshg->entities[idx].cell] = hshg->entities[idx].next;
    hshg->entities[hshg->entities[idx].next].prev = 0;
  } else {
    hshg->entities[hshg->entities[idx].prev].next = hshg->entities[idx].next;
    hshg->entities[hshg->entities[idx].next].prev = hshg->entities[idx].prev;
  }
}

void hshg_remove(struct hshg* const hshg, const uint32_t idx) {
  const uint32_t grid = hshg->entities[idx].grid;
  hshg_remove_light(hshg, idx, grid);
  hshg_return_entity(hshg, idx);
}

void hshg_move(struct hshg* const hshg, const uint32_t idx) {
  const uint32_t grid = hshg->entities[idx].grid;
  const uint32_t cell = grid_get_cell(hshg->grids + grid, hshg->entities[idx].x, hshg->entities[idx].y);
  if(hshg->entities[idx].cell != cell) {
    hshg_remove_light(hshg, idx, grid);
    hshg->entities[idx].cell = cell;
    hshg->entities[idx].next = hshg->grids[grid].cells[cell];
    if(hshg->entities[idx].next != 0) {
      hshg->entities[hshg->entities[idx].next].prev = idx;
    }
    hshg->entities[idx].prev = 0;
    hshg->grids[grid].cells[cell] = idx;
  }
}

void hshg_resize(struct hshg* const hshg, const uint32_t idx) {
  const uint32_t grid = hshg_get_grid(hshg, hshg->entities[idx].r);
  if(hshg->entities[idx].grid != grid) {
    const struct hshg_entity ent = hshg->entities[idx];
    hshg_remove(hshg, idx);
    hshg_insert(hshg, &ent);
  }
}

void hshg_update(struct hshg* const hshg) {
  for(uint32_t i = 1; i < hshg->entities_used; ++i) {
    if(hshg->entities[i].cell == UINT32_MAX) continue;
    hshg->update(hshg, i);
  }
}

void hshg_collide(struct hshg* const hshg) {
  for(uint32_t i = 1; i < hshg->entities_used; ++i) {
    const uint32_t cell = hshg->entities[i].cell;
    if(cell == UINT32_MAX) continue;
    const struct hshg_grid* const grid = hshg->grids + hshg->entities[i].grid;
    for(uint32_t j = hshg->entities[i].next; j != 0; j = hshg->entities[j].next) {
      hshg->collide(hshg, i, j);
    }
    uint32_t cell_x = cell & grid->cells_mask;
    uint32_t cell_y = cell >> grid->cells_log;
    if(cell_x != 0) {
      for(uint32_t j = grid->cells[cell - 1]; j != 0; j = hshg->entities[j].next) {
        hshg->collide(hshg, i, j);
      }
      if(cell_y != 0) {
        for(uint32_t j = grid->cells[cell - grid->cells_side - 1]; j != 0; j = hshg->entities[j].next) {
          hshg->collide(hshg, i, j);
        }
      }
    }
    if(cell_y != 0) {
      for(uint32_t j = grid->cells[cell - grid->cells_side]; j != 0; j = hshg->entities[j].next) {
        hshg->collide(hshg, i, j);
      }
      if(cell_x != grid->cells_mask) {
        for(uint32_t j = grid->cells[cell - grid->cells_side + 1]; j != 0; j = hshg->entities[j].next) {
          hshg->collide(hshg, i, j);
        }
      }
    }
    for(uint32_t up_grid = hshg->entities[i].grid + 1; up_grid < hshg->grids_len; ++up_grid) {
      cell_x >>= hshg->cell_div_log;
      cell_y >>= hshg->cell_div_log;
      const uint32_t min_x = cell_x == 0 ? 0 : cell_x - 1;
      const uint32_t min_y = cell_y == 0 ? 0 : cell_y - 1;
      const uint32_t max_x = cell_x == hshg->grids[up_grid].cells_mask ? cell_x : cell_x + 1;
      const uint32_t max_y = cell_y == hshg->grids[up_grid].cells_mask ? cell_y : cell_y + 1;
      for(uint32_t cur_x = min_x; cur_x <= max_x; ++cur_x) {
        for(uint32_t cur_y = min_y; cur_y <= max_y; ++cur_y) {
          for(uint32_t j = hshg->grids[up_grid].cells[cur_x | (cur_y << hshg->grids[up_grid].cells_log)]; j != 0; j = hshg->entities[j].next) {
            hshg->collide(hshg, i, j);
          }
        }
      }
    }
  }
}
