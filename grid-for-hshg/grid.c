#include "grid.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <shnet/error.h>

int grid_init(struct grid* const grid) {
  safe_execute(grid->cells = malloc(sizeof(*grid->cells) * grid->cells_x * grid->cells_y), grid->cells == NULL, ENOMEM);
  if(grid->cells == NULL) {
    return -1;
  }
  
  if(grid->entities_size != 0) {
    safe_execute(grid->entities = malloc(sizeof(*grid->entities) * grid->entities_size), grid->entities == NULL, ENOMEM);
    if(grid->entities == NULL) {
      free(grid->cells);
      return -1;
    }
  }
  
  grid->free_entity = UINT32_MAX;
  grid->inverse_cell_size = 1.0f / grid->cell_size;
  grid->cells_x_mask = grid->cells_x - 1;
  grid->cells_y_mask = grid->cells_y - 1;
  (void) memset(grid->cells, UINT8_MAX, sizeof(*grid->cells) * grid->cells_x * grid->cells_y);
  return 0;
}

static uint32_t grid_get_entity(struct grid* const grid) {
  if(grid->free_entity != UINT32_MAX) {
    const uint32_t ret = grid->free_entity;
    grid->free_entity = grid->entities[ret].next;
    return ret;
  }
  if(grid->entities_used == grid->entities_size) {
    grid->entities_size = (grid->entities_size << 1) + 1;
    void* ptr;
    do {
      safe_execute(ptr = realloc(grid->entities, sizeof(*grid->entities) * grid->entities_size), ptr == NULL, ENOMEM);
    } while(ptr == NULL);
    grid->entities = ptr;
  }
  return grid->entities_used++;
}

static void grid_return_entity(struct grid* const grid, const uint32_t idx) {
  grid->entities[idx].next = grid->free_entity;
  grid->entities[idx].cell = UINT32_MAX;
  grid->free_entity = idx;
}

static uint32_t grid_get_cell_x(const struct grid* const grid, const float x) {
  const uint32_t cell = (x < 0 ? -x : x) * grid->inverse_cell_size;
  if((cell / grid->cells_x) & 1) {
#ifdef GRID_FAST_MATH_X
    return grid->cells_x_mask - (cell & grid->cells_x_mask);
#else
    return grid->cells_x_mask - (cell % grid->cells_x);
#endif
  } else {
#ifdef GRID_FAST_MATH_X
    return cell & grid->cells_x_mask;
#else
    return cell % grid->cells_x;
#endif
  }
}

static uint32_t grid_get_cell_y(const struct grid* const grid, const float y) {
  const uint32_t cell = (y < 0 ? -y : y) * grid->inverse_cell_size;
  if((cell / grid->cells_y) & 1) {
#ifdef GRID_FAST_MATH_Y
    return grid->cells_y_mask - (cell & grid->cells_y_mask);
#else
    return grid->cells_y_mask - (cell % grid->cells_y);
#endif
  } else {
#ifdef GRID_FAST_MATH_Y
    return cell & grid->cells_y_mask;
#else
    return cell % grid->cells_y;
#endif
  }
}

static uint32_t grid_get_cell(const struct grid* const grid, const float x, const float y) {
  return grid_get_cell_x(grid, x) + grid_get_cell_y(grid, y) * grid->cells_x;
}

uint32_t grid_insert(struct grid* const grid, const struct grid_entity* const entity) {
  const uint32_t idx = grid_get_entity(grid);
  const uint32_t cell = grid_get_cell(grid, entity->x, entity->y);
  grid->entities[idx].cell = cell;
  grid->entities[idx].next = grid->cells[cell];
  grid->entities[idx].x = entity->x;
  grid->entities[idx].y = entity->y;
  grid->entities[idx].r = entity->r;
  grid->entities[idx].collides_with = entity->collides_with;
  grid->entities[idx].collision_mask = entity->collision_mask;
  grid->entities[idx].ref = entity->ref;
  grid->cells[cell] = idx;
  return idx;
}

static void grid_remove_internal(const struct grid* const grid, const uint32_t idx) {
  uint32_t i = grid->cells[grid->entities[idx].cell];
  uint32_t prev = UINT32_MAX;
  while(1) {
    if(i == idx) {
      if(prev == UINT32_MAX) {
        grid->cells[grid->entities[i].cell] = grid->entities[i].next;
      } else {
        grid->entities[prev].next = grid->entities[i].next;
      }
      return;
    }
    prev = i;
    i = grid->entities[i].next;
  }
}

void grid_remove(struct grid* const grid, const uint32_t idx) {
  grid_remove_internal(grid, idx);
  grid_return_entity(grid, idx);
}

static uint32_t grid_clamp(const float num, const float min, const float max) {
  return num > max ? max : (num < min ? min : num);
}

void grid_query(struct grid* const grid, const struct grid_pos* const pos, void (*func)(struct grid*, struct grid_entity*)) {
  uint32_t start_x = grid_clamp(pos->x / grid->cell_size, 0, grid->cells_x_mask);
  const uint32_t start_y = grid_clamp(pos->y / grid->cell_size, 0, grid->cells_y_mask);
  const uint32_t end_x = grid_clamp((pos->x + pos->w) / grid->cell_size, 0, grid->cells_x_mask);
  const uint32_t end_y = grid_clamp((pos->y + pos->h) / grid->cell_size, 0, grid->cells_y_mask);
  for(; start_x <= end_x; ++start_x) {
    for(uint32_t y = start_y; y <= end_y; ++y) {
      for(uint32_t i = grid->cells[start_x + y * grid->cells_x]; i != UINT32_MAX; i = grid->entities[i].next) {
        if(
          pos->x + pos->w >= grid->entities[i].x - grid->entities[i].r &&
          grid->entities[i].x + grid->entities[i].r >= pos->x &&
          pos->y + pos->h >= grid->entities[i].y - grid->entities[i].r &&
          grid->entities[i].y + grid->entities[i].r >= pos->y
        ) {
          func(grid, grid->entities + i);
        }
      }
    }
  }
}

void grid_query_cheap(struct grid* const grid, const struct grid_pos* const pos, void (*func)(struct grid*, struct grid_entity*)) {
  uint32_t start_x = grid_clamp(pos->x / grid->cell_size, 0, grid->cells_x_mask);
  const uint32_t start_y = grid_clamp(pos->y / grid->cell_size, 0, grid->cells_y_mask);
  const uint32_t end_x = grid_clamp((pos->x + pos->w) / grid->cell_size, 0, grid->cells_x_mask);
  const uint32_t end_y = grid_clamp((pos->y + pos->h) / grid->cell_size, 0, grid->cells_y_mask);
  for(; start_x <= end_x; ++start_x) {
    for(uint32_t y = start_y; y <= end_y; ++y) {
      for(uint32_t i = grid->cells[start_x + y * grid->cells_x]; i != UINT32_MAX; i = grid->entities[i].next) {
        func(grid, grid->entities + i);
      }
    }
  }
}

void grid_update(struct grid* const grid) {
  for(uint32_t i = 0; i < grid->entities_used; ++i) {
    if(grid->entities[i].cell == UINT32_MAX || grid->update(grid, grid->entities + i) == 0) continue;
    const uint32_t cell = grid_get_cell(grid, grid->entities[i].x, grid->entities[i].y);
    if(grid->entities[i].cell != cell) {
      grid_remove_internal(grid, i);
      grid->entities[i].cell = cell;
      grid->entities[i].next = grid->cells[cell];
      grid->cells[cell] = i;
    }
  }
}

void grid_collide(struct grid* const grid) {
  for(uint32_t i = 0; i < grid->entities_used; ++i) {
    if(grid->entities[i].cell == UINT32_MAX) continue;
#ifdef GRID_FAST_MATH_X
    const uint32_t x = grid->entities[i].cell & grid->cells_x_mask;
#else
    const uint32_t x = grid->entities[i].cell % grid->cells_x;
#endif
    const uint32_t y = grid->entities[i].cell / grid->cells_x;
    const uint32_t min_x = x == 0 ? 0 : x - 1;
    const uint32_t min_y = y == 0 ? 0 : y - 1;
    const uint32_t max_x = x == grid->cells_x_mask ? x : x + 1;
    const uint32_t max_y = y == grid->cells_y_mask ? y : y + 1;
    for(uint32_t cur_x = min_x; cur_x <= max_x; ++cur_x) {
      for(uint32_t cur_y = min_y; cur_y <= max_y; ++cur_y) {
        for(uint32_t j = grid->cells[cur_x + cur_y * grid->cells_x]; j != UINT32_MAX; j = grid->entities[j].next) {
          if(i != j && (grid->entities[i].collides_with & grid->entities[j].collision_mask) &&
            grid->entities[j].x + grid->entities[j].r >= grid->entities[i].x - grid->entities[i].r &&
            grid->entities[i].x + grid->entities[i].r >= grid->entities[j].x - grid->entities[j].r &&
            grid->entities[j].y + grid->entities[j].r >= grid->entities[i].y - grid->entities[i].r &&
            grid->entities[i].y + grid->entities[i].r >= grid->entities[j].y - grid->entities[j].r
          ) {
            grid->collision(grid, grid->entities + i, grid->entities + j);
          }
        }
      }
    }
  }
}

void grid_collide1(struct grid* const grid) {
  for(uint32_t x = 0; x < grid->cells_x; ++x) {
    for(uint32_t y = 0; y < grid->cells_y; ++y) {
      const uint32_t min_x = x == 0 ? 0 : x - 1;
      const uint32_t min_y = y == 0 ? 0 : y - 1;
      const uint32_t max_x = x == grid->cells_x_mask ? x : x + 1;
      const uint32_t max_y = y == grid->cells_y_mask ? y : y + 1;
      for(uint32_t cur_x = min_x; cur_x <= max_x; ++cur_x) {
        for(uint32_t cur_y = min_y; cur_y <= max_y; ++cur_y) {
          for(uint32_t i = grid->cells[x + y * grid->cells_x]; i != UINT32_MAX; i = grid->entities[i].next) {
            for(uint32_t j = grid->cells[cur_x + cur_y * grid->cells_x]; j != UINT32_MAX; j = grid->entities[j].next) {
              if(i != j && (grid->entities[i].collides_with & grid->entities[j].collision_mask) &&
                grid->entities[j].x + grid->entities[j].r >= grid->entities[i].x - grid->entities[i].r &&
                grid->entities[i].x + grid->entities[i].r >= grid->entities[j].x - grid->entities[j].r &&
                grid->entities[j].y + grid->entities[j].r >= grid->entities[i].y - grid->entities[i].r &&
                grid->entities[i].y + grid->entities[i].r >= grid->entities[j].y - grid->entities[j].r
              ) {
                grid->collision(grid, grid->entities + i, grid->entities + j);
              }
            }
          }
        }
      }
    }
  }
}