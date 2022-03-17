#ifndef vEm56sSh94N3_iSXU__ed_i18R8mnO9_
#define vEm56sSh94N3_iSXU__ed_i18R8mnO9_ 1

#include <stdint.h>

struct hshg_entity {
  uint32_t cell;
  uint32_t next;
  uint32_t prev;
  uint32_t grid;
  float x;
  float y;
  float r;
  uint32_t ref;
};

struct hshg_grid {
  uint32_t* cells;
  
  uint32_t cells_side;
  uint32_t cells_log;
  uint32_t cells_mask;
  
  uint32_t cell_size;
  float inverse_cell_size;
};

struct hshg {
  struct hshg_entity* entities;
  struct hshg_grid* grids;
  
  void (*update)(struct hshg*, uint32_t);
  void (*collide)(struct hshg*, uint32_t, uint32_t);
  
  uint32_t cell_div_log;
  uint32_t grids_len;
  uint32_t cell_log;
  
  uint32_t free_entity;
  uint32_t entities_used;
  uint32_t entities_size;
};

struct hshg_query {
  void (*cb)(struct hshg*, uint32_t);
  float x;
  float y;
  float w;
  float h;
};

extern int  hshg_init(struct hshg* const, const uint32_t, const uint32_t);

extern void hshg_free(struct hshg* const);

extern uint32_t hshg_insert(struct hshg* const restrict, const struct hshg_entity* const restrict);

extern void hshg_remove(struct hshg* const, const uint32_t);

extern void hshg_move(struct hshg* const, const uint32_t);

extern void hshg_resize(struct hshg* const, const uint32_t);

extern void hshg_update(struct hshg* const);

extern void hshg_collide(struct hshg* const);

#endif // vEm56sSh94N3_iSXU__ed_i18R8mnO9_
