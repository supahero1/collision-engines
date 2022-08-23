#include "qt.h"

#include <errno.h>
#include <stdlib.h>

#include <shnet/error.h>

#define DEF(name, names) \
static void qt_resize_##names (struct qt* const qt, const uint32_t new_size) { \
  void* ptr; \
  do { \
    safe_execute(ptr = realloc(qt-> names , sizeof(*qt-> names ) * new_size), ptr == NULL, ENOMEM); \
  } while(ptr == NULL); \
  qt-> names = ptr; \
  qt-> names##_size = new_size; \
} \
 \
static uint32_t qt_get_##name (struct qt* const qt) { \
  if(qt-> free_##name != 0) { \
    const uint32_t ret = qt-> free_##name ; \
    qt-> free_##name = qt-> names [ret].next; \
    return ret; \
  } \
  while(qt-> names##_used >= qt-> names##_size) { \
    qt_resize_##names (qt, qt-> names##_size << 1); \
  } \
  return qt-> names##_used ++; \
} \
 \
static void qt_return_##name (struct qt* const qt, const uint32_t idx) { \
  qt-> names [idx].next = qt-> free_##name; \
  qt-> free_##name = idx; \
}

DEF(node, nodes)
DEF(node_entity, node_entities)
DEF(entity, entities)

#undef DEF

void qt_init(struct qt* const qt) {
  qt->nodes_used = 1;
  qt_resize_nodes(qt, qt->nodes_size ? qt->nodes_size : 1);
  qt->nodes->nodes->head = 0;
  qt->nodes->nodes->leaf = 1;
  
  qt->node_entities_used = 1;
  qt_resize_node_entities(qt, qt->node_entities_size ? qt->node_entities_size : 1);
  
  qt->entities_used = 1;
  qt_resize_entities(qt, qt->entities_size ? qt->entities_size : 1);
}

void qt_free(struct qt* const qt) {
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

static int qt_completely_within(const float x1, const float y1, const float w1, const float h1, const float x2, const float y2, const float w2, const float h2) {
  return x1 >= x2 && y1 >= y2 && x2 + w2 >= x1 + w1 && y2 + h2 >= y1 + h1;
}

static void qt_insert_(struct qt* const qt, struct qt_node* const node, const float x, const float y, float w, float h) {
  if(node->leaf) {
    if(node->head == 0) {
      const uint32_t ent = qt_get_node_entity(qt);
      qt->node_entities[ent].entity = qt->idx;
      qt->node_entities[ent].next = 0;
      node->head = ent;
    } else {
      uint32_t i = node->head;
      if(qt->node_entities[i].entity == qt->idx) return;
      uint32_t j = qt->node_entities[i].next;
      uint32_t k = 1;
      while(j != 0) {
        i = j;
        if(qt->node_entities[i].entity == qt->idx) return;
        j = qt->node_entities[i].next;
        ++k;
      }
      if(k == 4 && w > qt->min_size && h > qt->min_size) {
        node->leaf = 0;
        node->head = qt_get_node(qt);
        const uint32_t save = qt->idx;
        i = node->head;
        qt->idx = qt->node_entities[i].entity;
        qt_insert_(qt, node, x, y, w, h);
        i = qt->node_entities[i].next;
        qt->idx = qt->node_entities[i].entity;
        qt_insert_(qt, node, x, y, w, h);
        i = qt->node_entities[i].next;
        qt->idx = qt->node_entities[i].entity;
        qt_insert_(qt, node, x, y, w, h);
        i = qt->node_entities[i].next;
        qt->idx = qt->node_entities[i].entity;
        qt_insert_(qt, node, x, y, w, h);
        qt->idx = save;
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
  w *= 0.5f;
  h *= 0.5f;
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x, y, w, h)) {
    qt_insert_(qt, qt->nodes[node->head].nodes + 0, x, y, w, h);
  }
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x, y + h, w, h)) {
    qt_insert_(qt, qt->nodes[node->head].nodes + 1, x, y + h, w, h);
  }
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x + w, y, w, h)) {
    qt_insert_(qt, qt->nodes[node->head].nodes + 2, x + w, y, w, h);
  }
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x + w, y + h, w, h)) {
    qt_insert_(qt, qt->nodes[node->head].nodes + 3, x + w, y + h, w, h);
  }
}

uint32_t qt_insert(struct qt* const qt, const struct qt_entity* const entity) {
  const uint32_t idx = qt_get_entity(qt);
  qt->entities[idx] = *entity;
  qt->idx = idx;
  qt_insert_(qt, qt->nodes->nodes, qt->x, qt->y, qt->w, qt->h);
  return idx;
}

static void qt_gather_entities(struct qt* const qt, struct qt_node* const node) {
  if(node->leaf) {
    uint32_t i = node->head;
    while(i != 0) {
      struct qt_node_entity* const ent = qt->node_entities + i;
      uint32_t j = qt->node_entities[qt->idx].next;
      uint32_t k = 0;
      while(j != 0) {
        if(ent->entity == qt->node_entities[j].entity) {
          const uint32_t next = ent->next;
          qt_return_node_entity(qt, i);
          i = next;
          goto next;
        }
        k = j;
        j = qt->node_entities[j].next;
      }
      qt->node_entities[k].next = i;
      i = ent->next;
      ent->next = 0;
      next:;
    }
  } else {
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 0);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 1);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 2);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 3);
    qt_return_node(qt, node->head);
  }
}

static uint32_t qt_remove_(struct qt* const qt, struct qt_node* const node, const float x, const float y, float w, float h) {
  /*
   * This is a relaxed implementation that doesn't rule out
   * duplicates in leaf nodes. It will prevent merging to
   * some degree in certain scenarios, but performance
   * gains outweigh that.
   */
  if(node->leaf) {
    uint32_t i = node->head;
    uint32_t j = 0;
    uint32_t k = 0;
    while(i != 0) {
      ++k;
      const struct qt_node_entity* const ent = qt->node_entities + i;
      if(ent->entity == qt->idx) {
        if(j == 0) {
          node->head = ent->next;
        } else {
          qt->node_entities[j].next = ent->next;
        }
        j = ent->next;
        qt_return_node_entity(qt, i);
        i = j;
        while(i != 0) {
          ++k;
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
  w *= 0.5f;
  h *= 0.5f;
  uint32_t total = 0;
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x, y, w, h)) {
    total += qt_remove_(qt, qt->nodes[node->head].nodes + 0, x, y, w, h);
  }
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x, y + h, w, h)) {
    total += qt_remove_(qt, qt->nodes[node->head].nodes + 1, x, y + h, w, h);
  }
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x + w, y, w, h)) {
    total += qt_remove_(qt, qt->nodes[node->head].nodes + 2, x + w, y, w, h);
  }
  if(qt_within(ent->x, ent->y, ent->r, ent->r, x + w, y + h, w, h)) {
    total += qt_remove_(qt, qt->nodes[node->head].nodes + 3, x + w, y + h, w, h);
  }
  if(total <= 4) { /* At most 4 entities, could be 1 entity duplicated 4 times */
    qt->idx = 0;
    qt->node_entities->next = 0;
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 0);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 1);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 2);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 3);
    node->head = qt->node_entities->next;
    node->leaf = 1;
  }
}

void qt_remove(struct qt* const qt, const uint32_t idx) {
  qt->idx = idx;
  (void) qt_remove_(qt, qt->nodes->nodes, qt->x, qt->y, qt->w, qt->h);
  qt_return_entity(qt, idx);
}

static uint32_t qt_update_(struct qt* const qt, struct qt_node* const node, const float x, const float y, float w, float h) {
  if(node->leaf) {
    uint32_t i = node->head;
    uint32_t j = 0;
    uint32_t k = 0;
    while(i != 0) {
      ++k;
      struct qt_node_entity* const node_entity = qt->node_entities + i;
      struct qt_entity* const entity = qt->entities + node_entity->entity;
      //if(entity->flag == 0) {
      //  entity->flag = 1;
        qt->update(qt, i);
      //}
      if(qt_completely_within(entity->x, entity->y, entity->r, entity->r, x, y, w, h)) continue;
      qt->idx = node_entity->entity;
      qt_insert_(qt, qt->nodes->nodes, qt->x, qt->y, qt->w, qt->h);
      if(!qt_within(entity->x, entity->y, entity->r, entity->r, x, y, w, h)) {
        if(j == 0) {
          node->head = node_entity->next;
        } else {
          qt->node_entities[j].next = node_entity->next;
        }
        const uint32_t next = node_entity->next;
        qt_return_node_entity(qt, i);
        i = next;
        --k;
      } else {
        j = i;
        i = node_entity->next;
      }
    }
    return k;
  }
  w *= 0.5f;
  h *= 0.5f;
  uint32_t total = 0;
  total += qt_update_(qt, qt->nodes[node->head].nodes + 0, x, y, w, h);
  total += qt_update_(qt, qt->nodes[node->head].nodes + 1, x, y + h, w, h);
  total += qt_update_(qt, qt->nodes[node->head].nodes + 2, x + w, y, w, h);
  total += qt_update_(qt, qt->nodes[node->head].nodes + 3, x + w, y + h, w, h);
  if(total <= 4) {
    qt->idx = 0;
    qt->node_entities->next = 0;
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 0);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 1);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 2);
    qt_gather_entities(qt, qt->nodes[node->head].nodes + 3);
    node->head = qt->node_entities->next;
    node->leaf = 1;
  }
  return total;
}

void qt_update(struct qt* const qt) {
  (void) qt_update_(qt, qt->nodes->nodes, qt->x, qt->y, qt->w, qt->h);
}

static void qt_collide_(struct qt* const qt, struct qt_node* const node) {
  if(node->leaf) {
    
  } else {
    qt_collide_(qt, qt->nodes[node->head].nodes + 0);
    qt_collide_(qt, qt->nodes[node->head].nodes + 1);
    qt_collide_(qt, qt->nodes[node->head].nodes + 2);
    qt_collide_(qt, qt->nodes[node->head].nodes + 3);
  }
}

void qt_collide(struct qt* const qt) {
  qt_collide_(qt, qt->nodes->nodes);
}
