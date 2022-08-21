#include "hshg.h"

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <shnet/error.h>

static void hshg_create_grid(struct hshg* const hshg) {
  ++hshg->grids_len;
  hshg->grids = shnet_realloc(hshg->grids, sizeof(*hshg->grids) * hshg->grids_len);
  assert(hshg->grids);
  struct hshg_grid* const current = hshg->grids + hshg->grids_len - 1;
  struct hshg_grid* const past = hshg->grids + hshg->grids_len - 2;
  current->cells_side = past->cells_side >> hshg->cell_div_log;
  current->cells_log = past->cells_log - hshg->cell_div_log;
  current->cells_mask = current->cells_side - 1;
  current->cell_size = past->cell_size << hshg->cell_div_log;
  current->inverse_cell_size = past->inverse_cell_size / (float)(1U << hshg->cell_div_log);
  current->cells = shnet_calloc(current->cells_side * current->cells_side, sizeof(*current->cells));
  assert(current->cells);
}

int hshg_init(struct hshg* const hshg, const uint32_t side, const uint32_t size) {
  assert(__builtin_popcount(side) == 1);
  assert(size > 0);
  if(hshg->cell_div_log == 0) {
    hshg->cell_div_log = 1;
  }
  hshg->entities_used = 1;
  if(hshg->entities_size == 0) {
    hshg->entities_size = 1;
  } else {
    hshg->entities = shnet_malloc(sizeof(*hshg->entities) * hshg->entities_size);
    if(hshg->entities == NULL) {
      return -1;
    }
  }
  hshg->grids = shnet_malloc(sizeof(*hshg->grids));
  if(hshg->grids == NULL) {
    free(hshg->entities);
    return -1;
  }
  hshg->grids_len = 1;
  hshg->grids->cells = shnet_calloc(side * side, sizeof(*hshg->grids->cells));
  if(hshg->grids->cells == NULL) {
    free(hshg->entities);
    free(hshg->grids);
    return -1;
  }
  hshg->grids->cells_side = side;
  hshg->grids->cells_log = __builtin_ctz(side);
  hshg->grids->cells_mask = side - 1;
  hshg->grids->cell_size = size;
  hshg->grids->inverse_cell_size = 1.0f / size;
  
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
    hshg->entities = shnet_realloc(hshg->entities, sizeof(*hshg->entities) * hshg->entities_size);
    assert(hshg->entities);
  }
  return hshg->entities_used++;
}

static void hshg_return_entity(struct hshg* const hshg, const uint32_t idx) {
  hshg->entities[idx].cell = UINT32_MAX;
  hshg->entities[idx].next = hshg->free_entity;
  hshg->free_entity = idx;
}

static uint32_t grid_get_cell_(const struct hshg_grid* const grid, const float x) {
  const uint32_t cell = fabsf(x) * grid->inverse_cell_size;
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

void hshg_insert(struct hshg* const hshg, const struct hshg_entity* const entity) {
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
}

static void hshg_remove_light(const struct hshg* const hshg, const uint32_t idx) {
  struct hshg_entity* const entity = hshg->entities + idx;
  if(entity->prev == 0) {
    hshg->grids[entity->grid].cells[entity->cell] = entity->next;
  } else {
    hshg->entities[entity->prev].next = entity->next;
  }
  hshg->entities[entity->next].prev = entity->prev;
}

void hshg_remove(struct hshg* const hshg, const uint32_t idx) {
  hshg_remove_light(hshg, idx);
  hshg_return_entity(hshg, idx);
}

void hshg_move(struct hshg* const hshg, const uint32_t idx) {
  struct hshg_entity* const entity = hshg->entities + idx;
  const struct hshg_grid* const grid = hshg->grids + entity->grid;
  const uint32_t cell = grid_get_cell(grid, entity->x, entity->y);
  if(entity->cell != cell) {
    hshg_remove_light(hshg, idx);
    entity->cell = cell;
    entity->next = grid->cells[cell];
    if(entity->next != 0) {
      hshg->entities[entity->next].prev = idx;
    }
    entity->prev = 0;
    grid->cells[cell] = idx;
  }
}

void hshg_resize(struct hshg* const hshg, const uint32_t idx) {
  const uint32_t grid = hshg_get_grid(hshg, hshg->entities[idx].r);
  if(hshg->entities[idx].grid != (grid > hshg->grids_len ? hshg->grids_len - 1 : grid)) {
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

void hshg_collide(const struct hshg* const hshg) {
  for(uint32_t i = 1; i < hshg->entities_used; ++i) {
    const struct hshg_entity* const entity = hshg->entities + i;
    if(entity->cell == UINT32_MAX) continue;
    const struct hshg_grid* grid = hshg->grids + entity->grid;
    for(uint32_t j = entity->next; j != 0; j = hshg->entities[j].next) {
      hshg->collide(hshg, entity, hshg->entities + j);
    }
    uint32_t cell_x = entity->cell & grid->cells_mask;
    uint32_t cell_y = entity->cell >> grid->cells_log;
    if(cell_x != 0) {
      for(uint32_t j = grid->cells[entity->cell - 1]; j != 0; j = hshg->entities[j].next) {
        hshg->collide(hshg, entity, hshg->entities + j);
      }
      if(cell_y != grid->cells_mask) {
        for(uint32_t j = grid->cells[entity->cell + grid->cells_side - 1]; j != 0; j = hshg->entities[j].next) {
          hshg->collide(hshg, entity, hshg->entities + j);
        }
      }
    }
    if(cell_y != grid->cells_mask) {
      for(uint32_t j = grid->cells[entity->cell + grid->cells_side]; j != 0; j = hshg->entities[j].next) {
        hshg->collide(hshg, entity, hshg->entities + j);
      }
      if(cell_x != grid->cells_mask) {
        for(uint32_t j = grid->cells[entity->cell + grid->cells_side + 1]; j != 0; j = hshg->entities[j].next) {
          hshg->collide(hshg, entity, hshg->entities + j);
        }
      }
    }
    for(uint32_t up_grid = entity->grid + 1; up_grid < hshg->grids_len; ++up_grid) {
      ++grid;
      cell_x >>= hshg->cell_div_log;
      cell_y >>= hshg->cell_div_log;
      const uint32_t min_x = cell_x == 0 ? 0 : cell_x - 1;
      const uint32_t min_y = cell_y == 0 ? 0 : cell_y - 1;
      const uint32_t max_x = cell_x == grid->cells_mask ? cell_x : cell_x + 1;
      const uint32_t max_y = cell_y == grid->cells_mask ? cell_y : cell_y + 1;
      for(uint32_t cur_y = min_y; cur_y <= max_y; ++cur_y) {
        for(uint32_t cur_x = min_x; cur_x <= max_x; ++cur_x) {
          for(uint32_t j = grid->cells[cur_x | (cur_y << grid->cells_log)]; j != 0; j = hshg->entities[j].next) {
            hshg->collide(hshg, entity, hshg->entities + j);
          }
        }
      }
    }
  }
}

void hshg_optimize(struct hshg* const hshg) {
  struct hshg_entity* const entities = shnet_malloc(sizeof(*hshg->entities) * hshg->entities_size);
  assert(entities);
  uint32_t idx = 1;
  for(uint32_t i = 0; i < hshg->grids_len; ++i) {
    const struct hshg_grid* const grid = hshg->grids + i;
    const uint32_t sq = grid->cells_side * grid->cells_side;
    for(uint32_t cell = 0; cell < sq; ++cell) {
      uint32_t i = grid->cells[cell];
      if(i == 0) continue;
      grid->cells[cell] = idx;
      while(1) {
        struct hshg_entity* const entity = entities + idx;
        *entity = hshg->entities[i];
        if(entity->prev != 0) {
          entity->prev = idx - 1;
        }
        ++idx;
        if(entity->next != 0) {
          i = entity->next;
          entity->next = idx;
        } else {
          break;
        }
      }
    }
  }
  free(hshg->entities);
  hshg->entities = entities;
  assert(hshg->entities_used == idx);
  hshg->free_entity = 0;
}
