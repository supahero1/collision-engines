#include "qt.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <shnet/error.h>

#define DEF(name, names) \
static void qt_resize_##names (struct qt* const qt, const uint32_t new_size) { \
  void* ptr; \
  do { \
    safe_execute(ptr = realloc(qt->##names , sizeof(*qt->##names ) * new_size), ptr == NULL, ENOMEM); \
  } while(ptr == NULL); \
  qt->##names = ptr; \
  qt->##names##_size = new_size; \
} \
 \
static uint32_t qt_get_##name (struct qt* const qt) { \
  if(qt->free_##name != 0) { \
    const uint32_t ret = qt->free_##name ; \
    qt->free_##name = qt->##names [ret].next; \
    return ret; \
  } \
  while(qt->##names##_used >= qt->##names##_size) { \
    qt_resize_##names (qt, qt->##names##_size << 1); \
  } \
  return qt->##names##_used++; \
} \
 \
static void qt_return_##name (struct qt* const qt, const uint32_t idx) { \
  qt->##names [idx].next = qt->free_##name; \
  qt->free_##name = idx; \
}

DEF(node, nodes)
DEF(node_entity, node_entities)
DEF(entity, entities)

#undef DEF

void qt_init(struct qt* const qt) {
  qt->nodes_used = 1;
  qt_resize_nodes(qt, qt->nodes_size ? qt->nodes_size : 1);
  
  qt->node_entities_used = 1;
  qt_resize_node_entities(qt, qt->node_entities_size ? qt->node_entities_size : 1);
  
  qt->entities_used = 1;
  qt_resize_entities(qt, qt->entities_size ? qt->entities_size : 1);
}

void qt_free(struct qt* const qt) {
  free(qt->copies);
  qt->copies = NULL;
  qt->copies_len = 0;
  
  free(qt->nodes);
  qt->nodes = NULL;
  qt->nodes_used = 0;
  qt->nodes_size = 0;
  qt->free_node = 0;
  
  free(qt->node_entities);
  qt->node_entities = NULL;
  qt->node_entities_used = 0;
  qt->node_entities_size = 0;
  qt->free_node_entity = 0;
  
  free(qt->entities);
  qt->entities = NULL;
  qt->entities_used = 0;
  qt->entities_size = 0;
  qt->free_entity = 0;
}

static int qt_within(const float x1, const float y1, const float w1, const float h1, const float x2, const float y2, const float w2, const float h2) {
  return x1 + w1 >= x2 && y1 + h1 >= y2 && x2 + w2 >= x1 && y2 + h2 >= y1;
}

static void qt_insert_(struct qt* const qt, const struct qt_node_extended* const node) {
  struct qt_node* const real_node = (uintptr_t) qt->nodes + sizeof(struct qt_node) * node->id;
  if(real_node->leaf) {
    if(real_node->head == 0) {
      const uint32_t ent = qt_get_node_entity(qt);
      qt->node_entities[ent].entity = qt->idx;
      qt->node_entities[ent].next = 0;
      real_node->head = ent;
    } else {
      uint32_t i = real_node->head;
      if(qt->node_entities[i].entity == qt->idx) return;
      uint32_t j = qt->node_entities[i].next;
      uint32_t k = 1;
      while(j != 0) {
        i = j;
        if(qt->node_entities[i].entity == qt->idx) return;
        j = qt->node_entities[i].next;
        ++k;
      }
      if(k == 4 && node->w > qt->min_size && node->h > qt->min_size) {
        real_node->leaf = 0;
        real_node->head = qt_get_node(qt);
        i = real_node->head;
        qt_insert_(qt, qt->node_entities[i].entity, node);
        i = qt->node_entities[i].next;
        qt_insert_(qt, qt->node_entities[i].entity, node);
        i = qt->node_entities[i].next;
        qt_insert_(qt, qt->node_entities[i].entity, node);
        i = qt->node_entities[i].next;
        qt_insert_(qt, qt->node_entities[i].entity, node);
        goto jump;
      }
      const uint32_t ent = qt_get_node_entity(qt);
      qt->node_entities[ent].entity = qt->idx;
      qt->node_entities[ent].next = 0;
      qt->node_entities[i].next = ent;
    }
    return;
  }
  jump:;
  const struct qt_entity* const ent = qt->entities + qt->idx;
  const float w_ = node->w / 2.0f;
  const float h_ = node->h / 2.0f;
  if(qt_within(ent->x, ent->y, ent->r, ent->r, node->x, node->y, w_, h_)) {
    qt_insert_(qt, &((struct qt_node_extended) {
      .id = real_node->head + 0,
      .x = node->x,
      .y = node->y,
      .w = w_,
      .h = h_
    }));
  }
  const float x_ = node->x + w_;
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x_, node->y, w_, h_)) {
    qt_insert_(qt, &((struct qt_node_extended) {
      .id = real_node->head + 1,
      .x = x_,
      .y = node->y,
      .w = w_,
      .h = h_
    }));
  }
  const float y_ = node->y + h_;
  if(qt_within(ent->x, ent->y, ent->r, ent->r, node->x, y_, w_, h_)) {
    qt_insert_(qt, &((struct qt_node_extended) {
      .id = real_node->head + 2,
      .x = node->x,
      .y = y_,
      .w = w_,
      .h = h_
    }));
  }
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x_, y_, w_, h_)) {
    qt_insert_(qt, &((struct qt_node_extended) {
      .id = real_node->head + 3,
      .x = x_,
      .y = y_,
      .w = w_,
      .h = h_
    }));
  }
}

uint32_t qt_insert(struct qt* const qt, const struct qt_entity* const entity) {
  const uint32_t idx = qt_get_entity(qt);
  qt->entities[idx].x = entity.x - entity.r;
  qt->entities[idx].y = entity.y - entity.r;
  qt->entities[idx].r = entity.r + entity.r;
  qt->idx = idx;
  qt_insert_(qt, &((struct qt_node_extended) {
    .id = 0,
    .x = qt->x,
    .y = qt->y,
    .w = qt->w,
    .h = qt->h
  }));
  qt->entities[idx].x += entity.r;
  qt->entities[idx].y += entity.r;
  qt->entities[idx].r -= entity.r;
  return idx;
}

static void qt_gather_entities(struct qt* const qt, const uint32_t id) {
  struct qt_node* const real_node = (uintptr_t) qt->nodes + sizeof(struct qt_node) * id;
  if(real_node->leaf) {
    uint32_t i = real_node->head;
    uint32_t prev = 0;
    while(i != 0) {
      const struct qt_node_entity* const ent = qt->node_entities + i;
      uint32_t j = qt->node_entities[qt->idx].next;
      uint32_t k = 0;
      while(j != 0) {
        if(ent->entity == qt->node_entities[j].entity) {
          if(prev == 0) {
            real_node->head = ent->next;
          } else {
            qt->node_entities[prev].next = ent->next;
          }
          goto next;
        }
        k = j;
        j = qt->node_entities[j].next;
      }
      qt->node_entities[k].next = i;
      prev = i;
      i = ent->next;
      qt->node_entities[prev].next = 0;
      next:;
    }
  } else {
    qt_gather_entities(qt, real_node->head + 0);
    qt_gather_entities(qt, real_node->head + 1);
    qt_gather_entities(qt, real_node->head + 2);
    qt_gather_entities(qt, real_node->head + 3);
  }
}

static uint32_t qt_remove_(struct qt* const qt, const struct qt_node_extended* const node) {
  /*
   * This is a relaxed implementation that doesn't rule out
   * duplicates in the leaf nodes. It will prevent merging
   * to some degree in certain scenarios, but performance
   * gains outweigh that.
   */
  struct qt_node* const real_node = (uintptr_t) qt->nodes + sizeof(struct qt_node) * node->id;
  if(real_node->leaf) {
    uint32_t i = real_node->head;
    uint32_t j = 0;
    uint32_t k = 0;
    while(i != 0) {
      ++k;
      const struct qt_node_entity* const ent = qt->node_entities + i;
      if(ent->entity == idx) {
        if(j == 0) {
          real_node->head = ent->next;
        } else {
          qt->node_entities[j].next = ent->next;
        }
        qt_return_node_entity(qt, i);
        j = i;
        i = ent->next;
        while(i != 0) {
          ++k;
          j = i;
          i = qt->node_entities[i].next;
        }
        return k;
      }
      j = i;
      i = ent->next;
    }
    return k;
  }
  const struct qt_entity* const ent = qt->entities + qt->idx;
  const uint32_t total = 0;
  const float w_ = node->w / 2.0f;
  const float h_ = node->h / 2.0f;
  if(qt_within(ent->x, ent->y, ent->r, ent->r, node->x, node->y, w_, h_)) {
    total += qt_remove_(qt, &((struct qt_node_extended) {
      .id = real_node->head + 0,
      .x = node->x,
      .y = node->y,
      .w = w_,
      .h = h_
    }));
  }
  const float x_ = node->x + w_;
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x_, node->y, w_, h_)) {
    total += qt_remove_(qt, &((struct qt_node_extended) {
      .id = real_node->head + 1,
      .x = x_,
      .y = node->y,
      .w = w_,
      .h = h_
    }));
  }
  const float y_ = node->y + h_;
  if(qt_within(ent->x, ent->y, ent->r, ent->r, node->x, y_, w_, h_)) {
    total += qt_remove_(qt, &((struct qt_node_extended) {
      .id = real_node->head + 2,
      .x = node->x,
      .y = y_,
      .w = w_,
      .h = h_
    }));
  }
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x_, y_, w_, h_)) {
    total += qt_remove_(qt, &((struct qt_node_extended) {
      .id = real_node->head + 3,
      .x = x_,
      .y = y_,
      .w = w_,
      .h = h_
    }));
  }
  if(total <= 4) { /* At most 4 entities */
    qt->idx = 0;
    qt->node_entities->next = 0;
    qt_gather_entities(qt, real_node->head + 0);
    qt_gather_entities(qt, real_node->head + 1);
    qt_gather_entities(qt, real_node->head + 2);
    qt_gather_entities(qt, real_node->head + 3);
    real_node->head = qt->node_entities->next;
    real_node->leaf = 1;
  }
}

void qt_remove(struct qt* const qt, const uint32_t idx) {
  qt->idx = idx;
  qt_remove_(qt, &((struct qt_node_extended) {
    .id = 0,
    .x = qt->x,
    .y = qt->y,
    .w = qt->w,
    .h = qt->h
  }));
  qt_return_entity(idx);
}

static void qt_update_(struct qt* const qt, const uint32_t id) {
  struct qt_node* const real_node = (uintptr_t) qt->nodes + sizeof(struct qt_node) * id;
  if(real_node->leaf) {
    
  } else {
    qt_update_(qt, real_node->head + 0);
    qt_update_(qt, real_node->head + 1);
    qt_update_(qt, real_node->head + 2);
    qt_update_(qt, real_node->head + 3);
  }
}

void qt_update(struct qt* const qt) {
  qt_update_(qt, 0);
}

static void qt_collide_(struct qt* const qt, const uint32_t id) {
  struct qt_node* const real_node = (uintptr_t) qt->nodes + sizeof(struct qt_node) * id;
  if(real_node->leaf) {
    
  } else {
    qt_collide_(qt, real_node->head + 0);
    qt_collide_(qt, real_node->head + 1);
    qt_collide_(qt, real_node->head + 2);
    qt_collide_(qt, real_node->head + 3);
  }
}

void qt_collide(struct qt* const qt) {
  qt_collide_(qt, 0);
}