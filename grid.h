#ifndef U_Zm__U7HFiJDHbT_jY_m_k__G4xDr5o
#define U_Zm__U7HFiJDHbT_jY_m_k__G4xDr5o 1

#include <stdint.h>

struct grid_pos {
  float x;
  float y;
  float w;
  float h;
};

struct grid_entity {
  struct grid_pos pos;
  
  uint32_t collides_with;
  uint32_t collision_mask;
  uint64_t ref;
  
  uint64_t spatial_hash;
};

struct grid_node_entity {
  uint32_t ref;
  uint32_t next;
};

struct grid {
  uint8_t* opt;
  uint32_t* cells;
  struct grid_entity* entities;
  struct grid_node_entity* node_entities;
  
  void (*onnomem)(struct grid*);
  int  (*onupdate)(struct grid*, struct grid_entity*);
  void (*oncollision)(struct grid*, struct grid_entity*, struct grid_entity*);
  
  uint16_t cells_x;
  uint16_t cells_y;
  uint32_t cell_size;
  
  uint32_t free_entity;
  uint32_t entities_used;
  uint32_t entities_size;
  
  uint32_t free_node_entity;
  uint32_t node_entities_used;
  uint32_t node_entities_size;
};

extern void grid_init(struct grid* const);

extern uint32_t grid_insert(struct grid* const, struct grid_entity);

extern void grid_remove(struct grid* const, const uint32_t);

extern void grid_update(struct grid* const);

extern void grid_set_opt(struct grid* const, const uint32_t);

extern uint8_t grid_get_opt(struct grid* const, const uint32_t);

extern void grid_query(struct grid* const, const struct grid_pos* const, void (*)(struct grid*, struct grid_entity*));

extern void grid_collide(struct grid* const);

#endif // U_Zm__U7HFiJDHbT_jY_m_k__G4xDr5o