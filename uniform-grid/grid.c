#include "grid.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <shnet/error.h>

void grid_init(struct grid* const grid) {
  if(grid->entities_size == 0) {
    grid->entities_size = 1;
  }
  grid->entities_used = 1;
  
  if(grid->node_entities_size) {
    grid->node_entities_size = 1;
  }
  grid->node_entities_used = 1;
  
  safe_execute(grid->cells = calloc((uint32_t) grid->cells_x * (uint32_t) grid->cells_y, sizeof(uint32_t)), grid->cells == NULL, ENOMEM);
  safe_execute(grid->entities = malloc(sizeof(struct grid_entity) * grid->entities_size), grid->entities == NULL, ENOMEM);
  safe_execute(grid->node_entities = malloc(sizeof(struct grid_node_entity) * grid->node_entities_size), grid->node_entities == NULL, ENOMEM);
}

#define DEF(name, names) \
static void grid_resize_##names (struct grid* const grid, const uint32_t new_size) { \
  void* ptr; \
  do { \
    safe_execute(ptr = realloc(grid->##names , sizeof(*grid->##names ) * new_size), ptr == NULL, ENOMEM); \
  } while(ptr == NULL); \
  grid->##names = ptr; \
  grid->##names##_size = new_size; \
} \
 \
static uint32_t grid_get_##name (struct grid* const grid) { \
  if(grid->free_##name != 0) { \
    const uint32_t ret = grid->free_##name ; \
    grid->free_##name = grid->##names [ret].next; \
    return ret; \
  } \
  while(grid->##names##_used >= grid->##names##_size) { \
    grid_resize_##names (grid, grid->##names##_size << 1); \
  } \
  return grid->##names##_used++; \
}

DEF(node_entity, node_entities)
DEF(entity, entities)

static uint32_t clamp(const float x, const float min, const float max) {
  return x < min ? min : x > max ? max : x;
}

uint32_t grid_insert(struct grid* const grid, const struct grid_entity* const entity) {
  const uint32_t id = grid_get_entity(grid);
  grid->entities[id].ref = entity->ref;
  grid->entities[id].pos = entity->pos;
  
  grid->entities[id].min_x = clamp((entity.pos.x - entity.pos.w) / grid->cell_size, 0, grid->cells_x - 1);
  grid->entities[id].min_y = clamp((entity.pos.y - entity.pos.h) / grid->cell_size, 0, grid->cells_y - 1);
  grid->entities[id].max_x = clamp((entity.pos.x + entity.pos.w) / grid->cell_size, 0, grid->cells_x - 1);
  grid->entities[id].max_y = clamp((entity.pos.y + entity.pos.h) / grid->cell_size, 0, grid->cells_y - 1);
  
  for(uint16_t x = grid->entities[id].min_x; x <= grid->entities[id].max_x; ++x) {
    for(uint16_t y = grid->entities[id].min_y; y <= grid->entities[id].max_y; ++y) {
      const uint32_t idx = grid_get_node_entity(grid);
      grid->node_entities[idx].ref = id;
      grid->node_entities[idx].next = grid->cells[x * grid->cells_y + y];
      grid->cells[x * grid->cells_y + y] = idx;
    }
  }
  
  return id;
}

static void grid_remove_raw(struct grid* const grid, const uint32_t id) {
  for(uint16_t x = grid->entities[id].min_x; x <= grid->entities[id].max_x; ++x) {
    for(uint16_t y = grid->entities[id].min_y; y <= grid->entities[id].max_y; ++y) {
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
}

void grid_remove(struct grid* const grid, const uint32_t id) {
  grid_remove_raw(grid, id);
  grid->entities[i].dead = 1;
  grid->entities[i].next = grid->free_entity;
  grid->free_entity = id;
}

void grid_update(struct grid* const grid) {
  for(uint32_t i = 0; i < grid->entities_used; ++i) {
    if(grid->entities[i].dead == 1 || grid->entities[i].updated) {
      continue;
    }
    grid->entities[i].updated = 1;
    struct grid_entity* const entity = grid->entities + i;
    if(grid->update(grid, entity) == 0) {
      continue;
    }
    const uint16_t min_x = clamp((entity->pos.x - entity->pos.w) / grid->cell_size, 0, grid->cells_x - 1);
    const uint16_t min_y = clamp((entity->pos.y - entity->pos.h) / grid->cell_size, 0, grid->cells_y - 1);
    const uint16_t max_x = clamp((entity->pos.x + entity->pos.w) / grid->cell_size, 0, grid->cells_x - 1);
    const uint16_t max_y = clamp((entity->pos.y + entity->pos.h) / grid->cell_size, 0, grid->cells_y - 1);
    if(min_x != entity->min_x || min_y != entity->min_y || max_x != entity->max_x || max_y != entity->max_y) {
      grid_remove_raw(grid, i);
      entity->min_x = min_x;
      entity->min_y = min_y;
      entity->max_x = max_x;
      entity->max_y = max_y;
      for(uint16_t x = min_x; x <= max_x; ++x) {
        for(uint16_t y = min_y; y <= max_y; ++y) {
          const uint32_t idx = grid_get_node_entity(grid);
          grid->node_entities[idx].ref = i;
          grid->node_entities[idx].next = grid->cells[x * grid->cells_y + y];
          grid->cells[x * grid->cells_y + y] = idx;
        }
      }
    }
  }
}

void grid_collide(struct grid* const grid) {
  for(uint16_t x = 0; x < grid->cells_x; ++x) {
    for(uint16_t y = 0; y < grid->cells_y; ++y) {
      for(uint32_t i = grid->cells[x * grid->cells_y + y]; i != UINT32_MAX; i = grid->node_entities[i].next) {
        struct grid_entity* const entity = grid->entities + grid->node_entities[i].ref;
        for(uint32_t j = grid->node_entities[i].next; j != UINT32_MAX; j = grid->node_entities[j].next) {
          struct grid_entity* const entity = grid->entities + grid->node_entities[j].ref;
          if(e->pos.x + e->pos.w >= entity->pos.x - entity->pos.w &&
             e->pos.x - e->pos.w <= entity->pos.x + entity->pos.w &&
             e->pos.y + e->pos.h >= entity->pos.y - entity->pos.h &&
             e->pos.y - e->pos.h <= entity->pos.y + entity->pos.h
          ) {
            grid->collide(grid, entity, e);
          }
        }
      }
    }
  }
}

void grid_crosscollide(struct grid* const grid, struct grid* const grid2) {
  for(uint16_t x = 0; x < grid->cells_x; ++x) {
    for(uint16_t y = 0; y < grid->cells_y; ++y) {
      for(uint32_t i = grid->cells[x * grid->cells_y + y]; i != UINT32_MAX; i = grid->node_entities[i].next) {
        struct grid_entity* const entity = grid->entities + grid->node_entities[i].ref;
        for(uint32_t j = grid2->cells[x * grid->cells_y + y]; j != UINT32_MAX; j = grid2->node_entities[j].next) {
          struct grid_entity* const entity = grid2->entities + grid2->node_entities[j].ref;
          if(e->pos.x + e->pos.w >= entity->pos.x - entity->pos.w &&
             e->pos.x - e->pos.w <= entity->pos.x + entity->pos.w &&
             e->pos.y + e->pos.h >= entity->pos.y - entity->pos.h &&
             e->pos.y - e->pos.h <= entity->pos.y + entity->pos.h
          ) {
            grid->collide(grid, entity, e);
          }
        }
      }
    }
  }
}