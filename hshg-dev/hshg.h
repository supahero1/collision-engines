#ifndef mH_GMB_7_UJ5KwrtJjWyT_H0pLYcs_E_
#define mH_GMB_7_UJ5KwrtJjWyT_H0pLYcs_E_ 1

#include <stdint.h>

struct hshg_pos {
  float x;
  float y;
  float w;
  float h;
};

struct hshg_entity {
  uint32_t cell;
  uint32_t next;
  float x;
  float y;
  float r;
  uint32_t ref;
};

struct hshg_grid_cell {
  uint32_t head;
  uint32_t idx;
};

struct hshg_grid {
  struct hshg_grid_cell* cells;
  uint32_t* used_cells;
  
  uint32_t used;
  uint32_t size;
  
  uint32_t cells_side;
  uint32_t cells_log;
  uint32_t cells_mask;
  
  uint32_t cell_size;
  float inverse_cell_size;
};

struct hshg {
  struct hshg_entity* entities;
  struct hshg_grid* grids;
  
  void (*update)(struct hshg*, struct hshg_entity* restrict);
  void (*collide)(struct hshg*, struct hshg_entity* restrict, struct hshg_entity* restrict);
  
  uint32_t cell_div_log;
  uint32_t grids_len;
  uint32_t cell_log;
  
  uint32_t free_entity;
  uint32_t entities_used;
  uint32_t entities_size;
};

extern int  hshg_init(struct hshg* const, const uint32_t, const uint32_t);

extern void hshg_free(struct hshg* const);

extern uint32_t hshg_get_entity(struct hshg* const);

extern void hshg_return_entity(struct hshg* const, const uint32_t);

extern uint32_t grid_get_cell(const struct hshg_grid* const, const float, const float);

extern uint32_t hshg_get_grid(const struct hshg* const, const float);

extern uint32_t hshg_get_grid_safe(const struct hshg* const, const float);

extern uint32_t hshg_insert(struct hshg* const restrict, const struct hshg_entity* const restrict);

extern void hshg_remove_light(const struct hshg* const, const uint32_t, uint32_t);

extern void hshg_remove(struct hshg* const, const uint32_t);

extern uint32_t hshg_ptr_to_idx(const struct hshg* const, const struct hshg_entity* const);

extern void hshg_move(struct hshg* const, const uint32_t);

extern void hshg_resize(struct hshg* const, const uint32_t, const uint32_t);

extern void hshg_update(struct hshg* const);

extern void hshg_collide(struct hshg* const);

#endif // mH_GMB_7_UJ5KwrtJjWyT_H0pLYcs_E_
