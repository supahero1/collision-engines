#include "grid.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void grid_init(struct grid* const grid) {
  while(1) {
    grid->cells = malloc(sizeof(uint32_t) * grid->cells_x * grid->cells_y);
    if(grid->cells == NULL) {
      grid->onnomem(grid);
      continue;
    }
    break;
  }
  (void) memset(grid->cells, 255, sizeof(uint32_t) * grid->cells_x * grid->cells_y);
  
  while(1) {
    grid->opt = calloc((grid->entities_size + 7) >> 3, sizeof(uint8_t));
    if(grid->opt == NULL) {
      grid->onnomem(grid);
      continue;
    }
    break;
  }
  
  while(1) {
    grid->entities = malloc(sizeof(struct grid_entity) * grid->entities_size);
    if(grid->entities == NULL) {
      grid->onnomem(grid);
      continue;
    }
    break;
  }
  
  while(1) {
    grid->node_entities = malloc(sizeof(struct grid_node_entity) * grid->node_entities_size);
    if(grid->node_entities == NULL) {
      grid->onnomem(grid);
      continue;
    }
    break;
  }
  
  grid->free_entity = UINT32_MAX;
  grid->free_node_entity = UINT32_MAX;
}

static uint32_t grid_get_entity(struct grid* const grid) {
  if(grid->free_entity != UINT32_MAX) {
    const uint32_t ret = grid->free_entity;
    grid->free_entity = grid->entities[ret].collides_with;
    return ret;
  }
  if(grid->entities_used == grid->entities_size) {
    const uint32_t old = (grid->entities_size + 7) >> 3;
    grid->entities_size = (grid->entities_size << 1) + 1;
    while(1) {
      void* const ptr = realloc(grid->entities, sizeof(struct grid_entity) * grid->entities_size);
      if(ptr == NULL) {
        grid->onnomem(grid);
        continue;
      }
      grid->entities = ptr;
      break;
    }
    while(1) {
      void* const ptr = realloc(grid->opt, sizeof(uint8_t) * ((grid->entities_size + 7) >> 3));
      if(ptr == NULL) {
        grid->onnomem(grid);
        continue;
      }
      grid->opt = ptr;
      (void) memset(grid->opt + old, 0, ((grid->entities_size + 7) >> 3) - old);
      break;
    }
  }
  return grid->entities_used++;
}

static uint32_t grid_get_node_entity(struct grid* const grid) {
  if(grid->free_node_entity != UINT32_MAX) {
    const uint32_t ret = grid->free_node_entity;
    grid->free_node_entity = grid->node_entities[ret].next;
    return ret;
  }
  if(grid->node_entities_used == grid->node_entities_size) {
    grid->node_entities_size = (grid->node_entities_size << 1) + 1;
    while(1) {
      void* const ptr = realloc(grid->node_entities, sizeof(struct grid_node_entity) * grid->node_entities_size);
      if(ptr == NULL) {
        grid->onnomem(grid);
        continue;
      }
      grid->node_entities = ptr;
      break;
    }
  }
  return grid->node_entities_used++;
}

static uint_fast16_t clamp(const uint_fast16_t x, const uint_fast16_t min, const uint_fast16_t max) {
  return x < min ? min : x > max ? max : x;
}

uint32_t grid_insert(struct grid* const grid, struct grid_entity entity) {
  const uint_fast16_t min_x = clamp((entity.pos.x - entity.pos.w) / grid->cell_size, 0, grid->cells_x - 1);
  const uint_fast16_t min_y = clamp((entity.pos.y - entity.pos.h) / grid->cell_size, 0, grid->cells_y - 1);
  const uint_fast16_t max_x = clamp((entity.pos.x + entity.pos.w) / grid->cell_size, 0, grid->cells_x - 1);
  const uint_fast16_t max_y = clamp((entity.pos.y + entity.pos.h) / grid->cell_size, 0, grid->cells_y - 1);
  entity.spatial_hash = ((uint64_t) min_x << 48) | ((uint64_t) min_y << 32) | ((uint64_t) max_x << 16) | (uint64_t) max_y;
  
  const uint32_t id = grid_get_entity(grid);
  grid->entities[id] = entity;
  
  for(uint_fast16_t x = min_x; x <= max_x; ++x) {
    for(uint_fast16_t y = min_y; y <= max_y; ++y) {
      const uint32_t idx = grid_get_node_entity(grid);
      grid->node_entities[idx].ref = id;
      grid->node_entities[idx].next = grid->cells[x * grid->cells_y + y];
      grid->cells[x * grid->cells_y + y] = idx;
    }
  }
  
  return id;
}

static void grid_remove_raw(struct grid* const grid, const uint32_t id, const int preserve) {
  const uint_fast16_t min_x = grid->entities[id].spatial_hash >> 48;
  const uint_fast16_t min_y = (grid->entities[id].spatial_hash >> 32) & UINT16_MAX;
  const uint_fast16_t max_x = (grid->entities[id].spatial_hash >> 16) & UINT16_MAX;
  const uint_fast16_t max_y = grid->entities[id].spatial_hash & UINT16_MAX;
  
  for(uint_fast16_t x = min_x; x <= max_x; ++x) {
    for(uint_fast16_t y = min_y; y <= max_y; ++y) {
      for(uint32_t i = grid->cells[x * grid->cells_y + y], prev = UINT32_MAX; i != UINT32_MAX; prev = i, i = grid->node_entities[i].next) {
        if(grid->node_entities[i].ref != id) {
          continue;
        }
        if(prev == UINT32_MAX) {
          grid->cells[x * grid->cells_y + y] = grid->node_entities[i].next;
        } else {
          grid->node_entities[prev].next = grid->node_entities[i].next;
        }
        grid->node_entities[i].next = grid->free_node_entity;
        grid->free_node_entity = i;
        break;
      }
    }
  }
  
  if(!preserve) {
    grid->entities[id].collides_with = grid->free_entity;
    grid->entities[id].ref = UINT64_MAX;
    grid->free_entity = id;
  }
}

void grid_remove(struct grid* const grid, const uint32_t id) {
  grid_remove_raw(grid, id, 0);
}

void grid_update(struct grid* const grid) {
  for(uint32_t i = 0; i < grid->entities_used; ++i) {
    if(grid->entities[i].ref == UINT64_MAX) {
      continue;
    }
    struct grid_entity* const entity = grid->entities + i;
    if(grid->onupdate(grid, entity) == 0) {
      continue;
    }
    const uint_fast16_t min_x = clamp((entity->pos.x - entity->pos.w) / grid->cell_size, 0, grid->cells_x - 1);
    const uint_fast16_t min_y = clamp((entity->pos.y - entity->pos.h) / grid->cell_size, 0, grid->cells_y - 1);
    const uint_fast16_t max_x = clamp((entity->pos.x + entity->pos.w) / grid->cell_size, 0, grid->cells_x - 1);
    const uint_fast16_t max_y = clamp((entity->pos.y + entity->pos.h) / grid->cell_size, 0, grid->cells_y - 1);
    const uint64_t spatial_hash = ((uint64_t) min_x << 48) | ((uint64_t) min_y << 32) | ((uint64_t) max_x << 16) | (uint64_t) max_y;
    if(spatial_hash != entity->spatial_hash) {
      grid_remove_raw(grid, i, 1);
      entity->spatial_hash = spatial_hash;
      for(uint_fast16_t x = min_x; x <= max_x; ++x) {
        for(uint_fast16_t y = min_y; y <= max_y; ++y) {
          const uint32_t idx = grid_get_node_entity(grid);
          grid->node_entities[idx].ref = i;
          grid->node_entities[idx].next = grid->cells[x * grid->cells_y + y];
          grid->cells[x * grid->cells_y + y] = idx;
        }
      }
    }
  }
}

void grid_set_opt(struct grid* const grid, const uint32_t idx) {
  grid->opt[idx >> 3] |= 1u << (idx % 8);
}

uint8_t grid_get_opt(struct grid* const grid, const uint32_t idx) {
  return grid->opt[idx >> 3] & (1u << (idx % 8));
}

void grid_query(struct grid* const grid, const struct grid_pos* const pos, void (*cb)(struct grid*, struct grid_entity*)) {
  const uint_fast16_t min_x = clamp((pos->x - pos->w) / grid->cell_size, 0, grid->cells_x - 1);
  const uint_fast16_t min_y = clamp((pos->y - pos->h) / grid->cell_size, 0, grid->cells_y - 1);
  const uint_fast16_t max_x = clamp((pos->x + pos->w) / grid->cell_size, 0, grid->cells_x - 1);
  const uint_fast16_t max_y = clamp((pos->y + pos->h) / grid->cell_size, 0, grid->cells_y - 1);
  uint32_t min = UINT32_MAX;
  uint32_t max = 0;
  for(uint_fast16_t x = min_x; x <= max_x; ++x) {
    for(uint_fast16_t y = min_y; y <= max_y; ++y) {
      if(x == min_x || x == max_x || y == min_y || y == max_y) {
        for(uint32_t i = grid->cells[x * grid->cells_y + y]; i != UINT32_MAX; i = grid->node_entities[i].next) {
          struct grid_entity* const e = grid->entities + grid->node_entities[i].ref;
          if(grid_get_opt(grid, grid->node_entities[i].ref) == 0) {
            grid_set_opt(grid, grid->node_entities[i].ref);
            if(grid->node_entities[i].ref > max) {
              max = grid->node_entities[i].ref;
            }
            if(grid->node_entities[i].ref < min) {
              min = grid->node_entities[i].ref;
            }
            if(
              e->pos.x + e->pos.w >= pos->x - pos->w &&
              e->pos.x - e->pos.w <= pos->x + pos->w &&
              e->pos.y + e->pos.h >= pos->y - pos->h &&
              e->pos.y - e->pos.h <= pos->y + pos->h
            ) {
              cb(grid, e);
            }
          }
        }
      } else {
        for(uint32_t i = grid->cells[x * grid->cells_y + y]; i != UINT32_MAX; i = grid->node_entities[i].next) {
          if(grid_get_opt(grid, grid->node_entities[i].ref) == 0) {
            grid_set_opt(grid, grid->node_entities[i].ref);
            if(grid->node_entities[i].ref > max) {
              max = grid->node_entities[i].ref;
            }
            if(grid->node_entities[i].ref < min) {
              min = grid->node_entities[i].ref;
            }
            cb(grid, grid->entities + grid->node_entities[i].ref);
          }
        }
      }
    }
  }
  if(min != UINT32_MAX) {
    (void) memset(grid->opt + (min >> 3), 0, sizeof(uint8_t) * ((max - min) >> 3));
  }
}

void grid_collide(struct grid* const grid) {
  for(uint32_t i = 0; i < grid->entities_used; ++i) {
    if(grid->entities[i].ref == UINT64_MAX) {
      continue;
    }
    struct grid_entity* const entity = grid->entities + i;
    const uint_fast16_t min_x = entity->spatial_hash >> 48;
    const uint_fast16_t min_y = (entity->spatial_hash >> 32) & UINT16_MAX;
    const uint_fast16_t max_x = (entity->spatial_hash >> 16) & UINT16_MAX;
    const uint_fast16_t max_y = entity->spatial_hash & UINT16_MAX;
    uint32_t min = UINT32_MAX;
    uint32_t max = 0;
    for(uint_fast16_t x = min_x; x <= max_x; ++x) {
      for(uint_fast16_t y = min_y; y <= max_y; ++y) {
        if(x == min_x || x == max_x || y == min_y || y == max_y) {
          for(uint32_t j = grid->cells[x * grid->cells_y + y]; j != UINT32_MAX; j = grid->node_entities[j].next) {
            if(i == grid->node_entities[j].ref) {
              continue;
            }
            struct grid_entity* const e = grid->entities + grid->node_entities[j].ref;
            if((entity->collides_with & e->collision_mask) && grid_get_opt(grid, grid->node_entities[j].ref) == 0 &&
              e->pos.x + e->pos.w >= entity->pos.x - entity->pos.w &&
              e->pos.x - e->pos.w <= entity->pos.x + entity->pos.w &&
              e->pos.y + e->pos.h >= entity->pos.y - entity->pos.h &&
              e->pos.y - e->pos.h <= entity->pos.y + entity->pos.h
            ) {
              grid_set_opt(grid, grid->node_entities[j].ref);
              if(grid->node_entities[j].ref > max) {
                max = grid->node_entities[j].ref;
              }
              if(grid->node_entities[j].ref < min) {
                min = grid->node_entities[j].ref;
              }
              grid->oncollision(grid, entity, e);
            }
          }
        } else {
          for(uint32_t j = grid->cells[x * grid->cells_y + y]; j != UINT32_MAX; j = grid->node_entities[j].next) {
            if(i == grid->node_entities[j].ref) {
              continue;
            }
            struct grid_entity* const e = grid->entities + grid->node_entities[j].ref;
            if((entity->collides_with & e->collision_mask) && grid_get_opt(grid, grid->node_entities[j].ref) == 0) {
              grid_set_opt(grid, grid->node_entities[j].ref);
              if(grid->node_entities[j].ref > max) {
                max = grid->node_entities[j].ref;
              }
              if(grid->node_entities[j].ref < min) {
                min = grid->node_entities[j].ref;
              }
              grid->oncollision(grid, entity, e);
            }
          }
        }
      }
    }
    if(min != UINT32_MAX) {
      (void) memset(grid->opt + (min >> 3), 0, sizeof(uint8_t) * ((max - min) >> 3));
    }
  }
}