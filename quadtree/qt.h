#ifndef t5IQm__9iaUzSkVQGRArK__wWsvdTDp_
#define t5IQm__9iaUzSkVQGRArK__wWsvdTDp_ 1

#include <stdint.h>

struct qt_node {
  uint32_t head:31;
  uint32_t leaf:1;
};

struct qt_nodes {
  union {
    struct qt_node nodes[4];
    uint32_t next;
  };
};

struct qt_node_extended {
  uint32_t id;
  float x;
  float y;
  float w;
  float h;
};

struct qt_node_entity {
  uint32_t entity;
  uint32_t next;
};

struct qt_entity {
  union {
    float x;
    uint32_t next;
  };
  float y;
  float r;
};

struct qt {
  struct qt_nodes* nodes;
  struct qt_node_entity* node_entities;
  struct qt_entity* entities;
  
  uint32_t idx;
  
  float x;
  float y;
  float w;
  float h;
  
  float min_size;
  
  uint32_t free_entity;
  uint32_t entities_used;
  uint32_t entities_size;
  
  uint32_t free_node_entity;
  uint32_t node_entities_used;
  uint32_t node_entities_size;
  
  uint32_t free_node;
  uint32_t nodes_used;
  uint32_t nodes_size;
};

extern void qt_init(struct qt* const);

extern void qt_free(struct qt* const);

extern uint32_t qt_insert(struct qt* const, const struct qt_entity* const);

extern void qt_remove(struct qt* const, const uint32_t);

#endif // t5IQm__9iaUzSkVQGRArK__wWsvdTDp_