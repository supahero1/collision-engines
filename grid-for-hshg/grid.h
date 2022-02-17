#ifndef X__t_F_MV3Pa0F6b_h_WG_S_dgymu9aE
#define X__t_F_MV3Pa0F6b_h_WG_S_dgymu9aE 1

#include <stdint.h>

struct grid_pos {
  float x;
  float y;
  float w;
  float h;
};

struct grid_entity {
  uint32_t cell;
  uint32_t next;
  float x;
  float y;
  float r;
  uint32_t collides_with;
  uint32_t collision_mask;
  uint64_t ref;
};

struct grid {
  uint32_t* cells;
  struct grid_entity* entities;
  
  int  (*update)(struct grid*, struct grid_entity*);
  void (*collision)(struct grid*, struct grid_entity*, struct grid_entity*);
  
  uint32_t cells_x;
  uint32_t cells_y;
  uint32_t cells_x_mask;
  uint32_t cells_y_mask;
  
  uint32_t cell_size;
  float inverse_cell_size;
  
  uint32_t free_entity;
  uint32_t entities_used;
  uint32_t entities_size;
};

extern int  grid_init(struct grid* const);

extern uint32_t grid_insert(struct grid* const, const struct grid_entity* const);

extern void grid_remove(struct grid* const, const uint32_t);

extern void grid_query(struct grid* const, const struct grid_pos* const, void (*)(struct grid*, struct grid_entity*));

extern void grid_query_cheap(struct grid* const, const struct grid_pos* const, void (*)(struct grid*, struct grid_entity*));

extern void grid_update(struct grid* const);

extern void grid_collide(struct grid* const);

#endif // X__t_F_MV3Pa0F6b_h_WG_S_dgymu9aE