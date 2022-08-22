#include "hshg.h"

#include <math.h>
#include <stdlib.h>
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
  current->inverse_cell_size = past->inverse_cell_size / (UINT32_C(1) << hshg->cell_div_log);
  current->cells = shnet_calloc((hshg_cell_sq_t) current->cells_side * current->cells_side, sizeof(*current->cells));
  assert(current->cells);
}

int hshg_init(struct hshg* const hshg, const hshg_cell_t side, const uint32_t size) {
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
  hshg->grids->cells = shnet_calloc((hshg_cell_sq_t) side * side, sizeof(*hshg->grids->cells));
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

  hshg->grid_size = (hshg_cell_sq_t) side * size;
  hshg->inverse_grid_size = 1.0f / hshg->grid_size;
  return 0;
}

void hshg_free(struct hshg* const hshg) {
  free(hshg->entities);
  hshg->entities = NULL;
  hshg->entities_used = 0;
  hshg->entities_size = 0;
  hshg->free_entity = 0;
  
  for(uint8_t i = 0; i < hshg->grids_len; ++i) {
    free(hshg->grids[i].cells);
  }
  free(hshg->grids);
  hshg->grids = NULL;
  hshg->grids_len = 0;
}

static hshg_entity_t hshg_get_entity(struct hshg* const hshg) {
  if(hshg->free_entity != 0) {
    const hshg_entity_t ret = hshg->free_entity;
    hshg->free_entity = hshg->entities[ret].next;
    return ret;
  }
  if(hshg->entities_used == hshg->entities_size) {
    const hshg_entity_t size = hshg->entities_size << 1;
    hshg->entities_size = hshg->entities_size > size ? hshg_entity_max : size;
    hshg->entities = shnet_realloc(hshg->entities, sizeof(*hshg->entities) * hshg->entities_size);
    assert(hshg->entities);
  }
  return hshg->entities_used++;
}

static void hshg_return_entity(struct hshg* const hshg, const hshg_entity_t idx) {
  hshg->entities[idx].cell = hshg_cell_sq_max;
  hshg->entities[idx].next = hshg->free_entity;
  hshg->free_entity = idx;
}

static hshg_cell_t grid_get_cell_(const struct hshg_grid* const grid, const hshg_pos_t x) {
  const hshg_cell_t cell = fabsf(x) * grid->inverse_cell_size;
  if(cell & grid->cells_side) {
    return grid->cells_mask - (cell & grid->cells_mask);
  } else {
    return cell & grid->cells_mask;
  }
}

static hshg_cell_sq_t grid_get_cell(const struct hshg_grid* const grid, const hshg_pos_t x, const hshg_pos_t y) {
  return (hshg_cell_sq_t) grid_get_cell_(grid, x) | ((hshg_cell_sq_t) grid_get_cell_(grid, y) << grid->cells_log);
}

static uint8_t hshg_get_grid(const struct hshg* const hshg, const hshg_pos_t r) {
  const uint32_t rounded = r + r;
  if(rounded < hshg->grids[0].cell_size) {
    return 0;
  }
  return (hshg->cell_log - __builtin_clz(rounded)) / hshg->cell_div_log + 1;
}

static uint8_t hshg_get_grid_resizable(struct hshg* const hshg, const hshg_pos_t r) {
  uint8_t grid = hshg_get_grid(hshg, r);
  if(grid >= hshg->grids_len) {
    for(uint8_t i = hshg->grids_len; i <= grid && hshg->grids[hshg->grids_len - 1].cells_side > 2; ++i) {
      hshg_create_grid(hshg);
    }
    grid = hshg->grids_len - 1;
  }
  return grid;
}

static void hshg_reinsert(const struct hshg* const hshg, const hshg_entity_t idx) {
  struct hshg_entity* const ent = hshg->entities + idx;
  ent->cell = grid_get_cell(hshg->grids + ent->grid, ent->x, ent->y);
  ent->next = hshg->grids[ent->grid].cells[ent->cell];
  if(ent->next != 0) {
    hshg->entities[ent->next].prev = idx;
  }
  ent->prev = 0;
  hshg->grids[ent->grid].cells[ent->cell] = idx;
}

void hshg_insert(struct hshg* const hshg, const struct hshg_entity* const entity) {
  const hshg_entity_t idx = hshg_get_entity(hshg);
  struct hshg_entity* const ent = hshg->entities + idx;
  ent->grid = hshg_get_grid_resizable(hshg, entity->r);
  ent->ref = entity->ref;
  ent->x = entity->x;
  ent->y = entity->y;
  ent->r = entity->r;
  hshg_reinsert(hshg, idx);
}

static void hshg_remove_light(const struct hshg* const hshg, const hshg_entity_t idx) {
  struct hshg_entity* const entity = hshg->entities + idx;
  if(entity->prev == 0) {
    hshg->grids[entity->grid].cells[entity->cell] = entity->next;
  } else {
    hshg->entities[entity->prev].next = entity->next;
  }
  hshg->entities[entity->next].prev = entity->prev;
}

void hshg_remove(struct hshg* const hshg, const hshg_entity_t idx) {
  hshg_remove_light(hshg, idx);
  hshg_return_entity(hshg, idx);
}

void hshg_move(const struct hshg* const hshg, const hshg_entity_t idx) {
  struct hshg_entity* const entity = hshg->entities + idx;
  const struct hshg_grid* const grid = hshg->grids + entity->grid;
  const hshg_cell_sq_t cell = grid_get_cell(grid, entity->x, entity->y);
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

void hshg_resize(struct hshg* const hshg, const hshg_entity_t idx) {
  const uint8_t grid = hshg_get_grid_resizable(hshg, hshg->entities[idx].r);
  if(hshg->entities[idx].grid != grid) {
    hshg_remove_light(hshg, idx);
    hshg->entities[idx].grid = grid;
    hshg_reinsert(hshg, idx);
  }
}

void hshg_update(struct hshg* const hshg) {
  for(hshg_entity_t i = 1; i < hshg->entities_used; ++i) {
    if(hshg->entities[i].cell == hshg_cell_sq_max) continue;
    hshg->update(hshg, i);
  }
}

void hshg_collide(const struct hshg* const hshg) {
  for(hshg_entity_t i = 1; i < hshg->entities_used; ++i) {
    const struct hshg_entity* const entity = hshg->entities + i;
    if(entity->cell == hshg_cell_sq_max) continue;
    const struct hshg_grid* grid = hshg->grids + entity->grid;
    for(hshg_entity_t j = entity->next; j != 0; j = hshg->entities[j].next) {
      hshg->collide(hshg, entity, hshg->entities + j);
    }
    hshg_cell_t cell_x = entity->cell & grid->cells_mask;
    hshg_cell_t cell_y = entity->cell >> grid->cells_log;
    if(cell_x != 0) {
      for(hshg_entity_t j = grid->cells[entity->cell - 1]; j != 0; j = hshg->entities[j].next) {
        hshg->collide(hshg, entity, hshg->entities + j);
      }
      if(cell_y != grid->cells_mask) {
        for(hshg_entity_t j = grid->cells[entity->cell + grid->cells_side - 1]; j != 0; j = hshg->entities[j].next) {
          hshg->collide(hshg, entity, hshg->entities + j);
        }
      }
    }
    if(cell_y != grid->cells_mask) {
      for(hshg_entity_t j = grid->cells[entity->cell + grid->cells_side]; j != 0; j = hshg->entities[j].next) {
        hshg->collide(hshg, entity, hshg->entities + j);
      }
      if(cell_x != grid->cells_mask) {
        for(hshg_entity_t j = grid->cells[entity->cell + grid->cells_side + 1]; j != 0; j = hshg->entities[j].next) {
          hshg->collide(hshg, entity, hshg->entities + j);
        }
      }
    }
    if(cell_x != 0) {
      --cell_x;
    }
    if(cell_y != 0) {
      --cell_y;
    }
    hshg_cell_t max_cell_x = cell_x != grid->cells_mask ? cell_x + 1 : cell_x;
    hshg_cell_t max_cell_y = cell_y != grid->cells_mask ? cell_y + 1 : cell_y;
    for(uint8_t up_grid = entity->grid + 1; up_grid < hshg->grids_len; ++up_grid) {
      ++grid;
      cell_x >>= hshg->cell_div_log;
      cell_y >>= hshg->cell_div_log;
      max_cell_x >>= hshg->cell_div_log;
      max_cell_y >>= hshg->cell_div_log;
      for(hshg_cell_t cur_y = cell_y; cur_y <= max_cell_y; ++cur_y) {
        for(hshg_cell_t cur_x = cell_x; cur_x <= max_cell_x; ++cur_x) {
          for(hshg_entity_t j = grid->cells[(hshg_cell_sq_t) cur_x | (cur_y << grid->cells_log)]; j != 0; j = hshg->entities[j].next) {
            hshg->collide(hshg, entity, hshg->entities + j);
          }
        }
      }
    }
  }
}

void hshg_optimize(struct hshg* const hshg) {
  const hshg_entity_t size = hshg->entities_used << 1;
  hshg->entities_size = hshg->entities_size > size ? hshg_entity_max : size;
  struct hshg_entity* const entities = shnet_malloc(sizeof(*hshg->entities) * hshg->entities_size);
  assert(entities);
  hshg_entity_t idx = 1;
  for(uint8_t i = 0; i < hshg->grids_len; ++i) {
    const struct hshg_grid* const grid = hshg->grids + i;
    const hshg_cell_sq_t sq = grid->cells_side * grid->cells_side;
    for(hshg_cell_sq_t cell = 0; cell < sq; ++cell) {
      hshg_entity_t i = grid->cells[cell];
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

#define min(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a > _b ? _b : _a; \
})

#define max(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a > _b ? _a : _b; \
})

void hshg_query(const struct hshg* const hshg, const hshg_pos_t _x1, const hshg_pos_t _y1, const hshg_pos_t _x2, const hshg_pos_t _y2) {
  /* ^ +y
     -------------
     |      x2,y2|
     |           |
     |x1,y1      |
     -------------> +x */
  hshg_pos_t x1;
  hshg_pos_t x2;
  if(_x1 < 0) {
    const hshg_pos_t shift = (((hshg_cell_t)(-_x1 * hshg->inverse_grid_size) << 1) + 2) * hshg->grid_size;
    x1 = _x1 + shift;
    x2 = _x2 + shift;
  } else {
    x1 = _x1;
    x2 = _x2;
  }
  hshg_cell_t start_x;
  hshg_cell_t end_x;
  hshg_cell_t folds = (x2 - (hshg_cell_t)(x1 * hshg->inverse_grid_size) * hshg->grid_size) * hshg->inverse_grid_size;
  switch(folds) {
    case 0: {
      const hshg_cell_t temp = grid_get_cell_(hshg->grids, x1);
      end_x = grid_get_cell_(hshg->grids, x2);
      start_x = min(temp, end_x);
      end_x = max(temp, end_x);
      break;
    }
    case 1: {
      const hshg_cell_t cell = fabsf(x1) * hshg->grids->inverse_cell_size;
      if(cell & hshg->grids->cells_side) {
        start_x = 0;
        end_x = max(hshg->grids->cells_mask - (cell & hshg->grids->cells_mask), grid_get_cell_(hshg->grids, x2));
      } else {
        start_x = min(cell & hshg->grids->cells_mask, grid_get_cell_(hshg->grids, x2));
        end_x = hshg->grids->cells_mask;
      }
      break;
    }
    default: {
      start_x = 0;
      end_x = hshg->grids->cells_mask;
      break;
    }
  }

  hshg_pos_t y1;
  hshg_pos_t y2;
  if(_y1 < 0) {
    const hshg_pos_t shift = (((hshg_cell_t)(-_y1 * hshg->inverse_grid_size) << 1) + 2) * hshg->grid_size;
    y1 = _y1 + shift;
    y2 = _y2 + shift;
  } else {
    y1 = _y1;
    y2 = _y2;
  }
  hshg_cell_t start_y;
  hshg_cell_t end_y;
  folds = (y2 - (hshg_cell_t)(y1 * hshg->inverse_grid_size) * hshg->grid_size) * hshg->inverse_grid_size;
  switch(folds) {
    case 0: {
      const hshg_cell_t temp = grid_get_cell_(hshg->grids, y1);
      end_y = grid_get_cell_(hshg->grids, y2);
      start_y = min(temp, end_y);
      end_y = max(temp, end_y);
      break;
    }
    case 1: {
      const hshg_cell_t cell = fabsf(y1) * hshg->grids->inverse_cell_size;
      if(cell & hshg->grids->cells_side) {
        start_y = 0;
        end_y = max(hshg->grids->cells_mask - (cell & hshg->grids->cells_mask), grid_get_cell_(hshg->grids, y2));
      } else {
        start_y = min(cell & hshg->grids->cells_mask, grid_get_cell_(hshg->grids, y2));
        end_y = hshg->grids->cells_mask;
      }
      break;
    }
    default: {
      start_y = 0;
      end_y = hshg->grids->cells_mask;
      break;
    }
  }

  const struct hshg_grid* grid = hshg->grids;
  uint8_t i = 0;
  while(1) {
    for(hshg_cell_t y = start_y; y <= end_y; ++y) {
      for(hshg_cell_t x = start_x; x <= end_x; ++x) {
        for(hshg_entity_t j = grid->cells[(hshg_cell_sq_t) x | (y << grid->cells_log)]; j != 0;) {
          const struct hshg_entity* const entity = hshg->entities + j;
          if(entity->x + entity->r >= _x1 && entity->x - entity->r <= _x2 && entity->y + entity->r >= _y1 && entity->y - entity->r <= _y2) {
            hshg->query(hshg, entity);
          }
          j = entity->next;
        }
      }
    }
    if(++i == hshg->grids_len) break;
    ++grid;
    start_x >>= hshg->cell_div_log;
    start_y >>= hshg->cell_div_log;
    end_x >>= hshg->cell_div_log;
    end_y >>= hshg->cell_div_log;
  }
}

#undef max
#undef min
