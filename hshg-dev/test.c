#include <shnet/test.h>

#include "hshg.h"

#include <shnet/time.h>

#include <math.h>
#include <string.h>

#define AGENTS_NUM 500000

#define CELLS_SIDE /*32768*/ 16384
#define AGENT_R 2
#define CELL_SIZE 16
#define ARENA_WIDTH 100000
#define ARENA_HEIGHT 100000

#define LATENCY_NUM 1

struct ball {
  float vx;
  float vy;
};

struct ball balls[AGENTS_NUM];

void update(struct hshg* hshg, struct hshg_entity* restrict a) {
  a->x += balls[a->ref].vx;
	if(a->x < a->r) {
		++balls[a->ref].vx;
	} else if(a->x + a->r >= ARENA_WIDTH) {
		--balls[a->ref].vx;
	}
  
	a->y += balls[a->ref].vy;
	if(a->y < a->r) {
		++balls[a->ref].vy;
	} else if(a->y + a->r >= ARENA_HEIGHT) {
		--balls[a->ref].vy;
	}
  
  hshg_move(hshg, hshg_ptr_to_idx(hshg, a));
}

uint32_t maybe_collisions = 0;
uint32_t collisions = 0;

void mock_collide(struct hshg* hshg, struct hshg_entity* a, struct hshg_entity* b) {
  const float xd = a->x - b->x;
  const float yd = a->y - b->y;
  const float d = xd * xd + yd * yd;
  if(d <= (a->r + b->r) * (a->r + b->r)) {
    ++collisions;
  }
}

void collide(struct hshg* hshg, struct hshg_entity* a, struct hshg_entity* b) {
  const float xd = a->x - b->x;
  const float yd = a->y - b->y;
  const float d = xd * xd + yd * yd;
  ++maybe_collisions;
  if(d <= (a->r + b->r) * (a->r + b->r)) {
    ++collisions;
    const float angle = atan2f(yd, xd);
    const float a_mass_diff = b->r / a->r;
    const float b_mass_diff = a->r / b->r;
    balls[a->ref].vx += cosf(angle) * a_mass_diff;
    balls[a->ref].vy += sinf(angle) * a_mass_diff;
    balls[a->ref].vx -= cosf(angle) * b_mass_diff;
    balls[a->ref].vy -= sinf(angle) * b_mass_diff;
  }
}

int main() {
  srand(time_get_time());
  begin_test("hshg sanity");
  struct hshg hshg = {0};
  assert(!hshg_init(&hshg, 2, 4));
  assert(hshg_get_entity(&hshg) == 1);
  assert(hshg_get_entity(&hshg) == 2);
  assert(hshg.entities_used == 3);
  assert(hshg.entities_size == 4);
  hshg_return_entity(&hshg, 1);
  hshg_return_entity(&hshg, 2);
  assert(hshg_get_entity(&hshg) == 2);
  assert(hshg_get_entity(&hshg) == 1);
  hshg_return_entity(&hshg, 2);
  hshg_return_entity(&hshg, 1);
  assert(hshg_get_entity(&hshg) == 1);
  assert(hshg_get_entity(&hshg) == 2);
  hshg_return_entity(&hshg, 2);
  hshg_return_entity(&hshg, 1);
  end_test();
  
  begin_test("grid_get_cell()");
  assert(grid_get_cell(hshg.grids, 1, 1) == 0);
  assert(grid_get_cell(hshg.grids, -1, -1) == 0);
  assert(grid_get_cell(hshg.grids, -1, 1) == 0);
  assert(grid_get_cell(hshg.grids, 1, -1) == 0);
  
  assert(grid_get_cell(hshg.grids, 4, 4) == 3);
  assert(grid_get_cell(hshg.grids, -4, -4) == 3);
  assert(grid_get_cell(hshg.grids, -4, 4) == 3);
  assert(grid_get_cell(hshg.grids, 4, -4) == 3);
  
  assert(grid_get_cell(hshg.grids, 8, 0) == 1);
  assert(grid_get_cell(hshg.grids, -8, 0) == 1);
  assert(grid_get_cell(hshg.grids, 0, 8) == 2);
  assert(grid_get_cell(hshg.grids, 0, -8) == 2);
  assert(grid_get_cell(hshg.grids, 0, 12) == 0);
  assert(grid_get_cell(hshg.grids, 0, -12) == 0);
  end_test();
  
  begin_test("hshg_get_grid()");
  hshg_free(&hshg);
  assert(!hshg_init(&hshg, 16, 1));
  assert(hshg_get_grid(&hshg, 0.0f) == 0);
  assert(hshg_get_grid(&hshg, 0.49999f) == 0);
  assert(hshg_get_grid(&hshg, 0.5f) == 1);
  assert(hshg_get_grid(&hshg, 0.99999f) == 1);
  assert(hshg_get_grid(&hshg, 1.0f) == 2);
  assert(hshg_get_grid(&hshg, 1.99999f) == 2);
  assert(hshg_get_grid(&hshg, 2.0f) == 3);
  hshg.cell_div_log = 3;
  assert(hshg_get_grid(&hshg, 0.0f) == 0);
  assert(hshg_get_grid(&hshg, 0.49999f) == 0);
  assert(hshg_get_grid(&hshg, 0.5f) == 1);
  assert(hshg_get_grid(&hshg, 0.99999f) == 1);
  assert(hshg_get_grid(&hshg, 1.0f) == 1);
  assert(hshg_get_grid(&hshg, 1.99999f) == 1);
  assert(hshg_get_grid(&hshg, 2.0f) == 1);
  assert(hshg_get_grid(&hshg, 2.99999f) == 1);
  assert(hshg_get_grid(&hshg, 4.0f) == 2);
  assert(hshg_get_grid(&hshg, 8.0f) == 2);
  assert(hshg_get_grid(&hshg, 16.0f) == 2);
  assert(hshg_get_grid(&hshg, 31.9999f) == 2);
  assert(hshg_get_grid(&hshg, 32.0f) == 3);
  hshg_free(&hshg);
  assert(!hshg_init(&hshg, 16, 4));
  assert(hshg_get_grid(&hshg, 0.0f) == 0);
  assert(hshg_get_grid(&hshg, 0.49999f) == 0);
  assert(hshg_get_grid(&hshg, 0.5f) == 0);
  assert(hshg_get_grid(&hshg, 0.99999f) == 0);
  assert(hshg_get_grid(&hshg, 1.0f) == 0);
  assert(hshg_get_grid(&hshg, 1.99999f) == 0);
  assert(hshg_get_grid(&hshg, 2.0f) == 1);
  assert(hshg_get_grid(&hshg, 2.99999f) == 1);
  assert(hshg_get_grid(&hshg, 4.0f) == 1);
  assert(hshg_get_grid(&hshg, 8.0f) == 1);
  assert(hshg_get_grid(&hshg, 15.9999f) == 1);
  assert(hshg_get_grid(&hshg, 16.0f) == 2);
  assert(hshg_get_grid(&hshg, 31.9999f) == 2);
  assert(hshg_get_grid(&hshg, 32.0f) == 2);
  assert(hshg_get_grid(&hshg, 127.999f) == 2);
  assert(hshg_get_grid(&hshg, 128.0f) == 3);
  end_test();
  
  begin_test("hshg_insert() & hshg_remove()");
  assert(hshg.grids_len == 1);
  struct hshg_entity entity = {0};
  /* Large prime magic */
#define PRIME_MAGIC 549896681
  entity.ref = PRIME_MAGIC;
  entity.r = 1.0f;
  uint32_t idx = hshg_insert(&hshg, &entity);
  assert(idx == 1);
  assert(hshg.entities[idx].cell == 0);
  assert(hshg.entities[idx].next == 0);
  assert(hshg.entities[idx].x == 0);
  assert(hshg.entities[idx].y == 0);
  assert(hshg.entities[idx].r == 1.0f);
  assert(hshg.entities[idx].ref == PRIME_MAGIC);
  assert(hshg.grids[0].cells[0].head == 1);
  assert(hshg.grids[0].used == 1);
  assert(hshg.grids[0].size == 1);
  assert(hshg.grids[0].used_cells[0] == 0);
  hshg_remove(&hshg, idx);
  assert(hshg.grids[0].cells[0].head == 0);
  assert(hshg.grids[0].used == 0);
  assert(hshg.grids[0].size == 1);
  assert(hshg.free_entity == 1);
  assert(hshg.entities[idx].cell == UINT32_MAX);
  assert(hshg.entities[idx].next == 0);
  end_test();
  
  begin_test("hshg resize small");
  entity.r = 2.0f;
  idx = hshg_insert(&hshg, &entity);
  assert(idx == 1);
  assert(hshg.entities[idx].cell == 0);
  assert(hshg.entities[idx].next == 0);
  assert(hshg.entities[idx].x == 0);
  assert(hshg.entities[idx].y == 0);
  assert(hshg.entities[idx].r == 2.0f);
  assert(hshg.entities[idx].ref == PRIME_MAGIC);
  assert(hshg.grids_len == 2);
  assert(hshg.grids[1].cells[0].head == 1);
  assert(hshg.grids[1].cells_side == 2);
  assert(hshg.grids[1].cells_log == 1);
  assert(hshg.grids[1].cells_mask == 1);
  assert(hshg.grids[1].cell_size == 32);
  assert(hshg.grids[1].inverse_cell_size == 0.03125f);
  assert(hshg.grids[1].used == 1);
  assert(hshg.grids[1].size == 1);
  assert(hshg.grids[1].used_cells[0] == 0);
  hshg_remove(&hshg, idx);
  assert(hshg.grids[1].cells[0].head == 0);
  assert(hshg.grids[1].used == 0);
  assert(hshg.grids[1].size == 1);
  assert(hshg.free_entity == 1);
  assert(hshg.entities[idx].cell == UINT32_MAX);
  assert(hshg.entities[idx].next == 0);
  end_test();
  
  begin_test("hshg resize big 1");
  entity.r = 100.0f;
  idx = hshg_insert(&hshg, &entity);
  assert(idx == 1);
  assert(hshg.entities[idx].cell == 0);
  assert(hshg.entities[idx].next == 0);
  assert(hshg.entities[idx].x == 0);
  assert(hshg.entities[idx].y == 0);
  assert(hshg.entities[idx].r == 100.0f);
  assert(hshg.entities[idx].ref == PRIME_MAGIC);
  assert(hshg.grids_len == 2);
  assert(hshg.grids[1].cells[0].head == 1);
  hshg_remove(&hshg, idx);
  assert(hshg.grids[1].cells[0].head == 0);
  assert(hshg.free_entity == 1);
  assert(hshg.entities[idx].cell == UINT32_MAX);
  assert(hshg.entities[idx].next == 0);
  end_test();
  
  begin_test("hshg resize big 2");
  hshg_free(&hshg);
  hshg.cell_div_log = 1;
  assert(!hshg_init(&hshg, 256, 1));
  idx = hshg_insert(&hshg, &entity);
  assert(idx == 1);
  assert(hshg.entities[idx].cell == 0);
  assert(hshg.entities[idx].next == 0);
  assert(hshg.entities[idx].x == 0);
  assert(hshg.entities[idx].y == 0);
  assert(hshg.entities[idx].r == 100.0f);
  assert(hshg.entities[idx].ref == PRIME_MAGIC);
  assert(hshg.grids_len == 8);
  assert(hshg.grids[7].cells[0].head == 1);
  assert(hshg.grids[7].used == 1);
  assert(hshg.grids[7].size == 1);
  assert(hshg.grids[7].used_cells[0] == 0);
  hshg_remove_light(&hshg, idx, hshg_get_grid_safe(&hshg, entity.r));
  assert(hshg.grids[7].cells[0].head == 0);
  assert(hshg.grids[1].used == 0);
  assert(hshg.grids[1].size == 1);
  assert(hshg.free_entity == 0);
  assert(hshg.entities[idx].cell == 0);
  assert(hshg.entities[idx].next == 0);
  hshg_return_entity(&hshg, idx);
  assert(hshg.free_entity == 1);
  assert(hshg.entities[idx].cell == UINT32_MAX);
  assert(hshg.entities[idx].next == 0);
  end_test();
  
  begin_test("hshg_collide() 1 layer");
  hshg.collide = mock_collide;
  hshg_free(&hshg);
  assert(!hshg_init(&hshg, 256, 4));
  entity.x = 1.0f;
  entity.y = 1.0f;
  entity.r = 1.0f;
  uint32_t idx2 = hshg_insert(&hshg, &entity);
  entity.x = 2.0f;
  entity.y = 2.0f;
  entity.r = 1.0f;
  idx = hshg_insert(&hshg, &entity);
  hshg_collide(&hshg);
  assert(collisions == 1);
  end_test();
  
  begin_test("hshg_collide() 2 layers");
  collisions = 0;
  entity.x = 400.0f;
  entity.y = 400.0f;
  entity.r = 1024.0f;
  uint32_t idx3 = hshg_insert(&hshg, &entity);
  hshg_collide(&hshg);
  assert(collisions == 3);
  end_test();
  
  begin_test("hshg_collide() 2 layers 2");
  collisions = 0;
  entity.x = 1.5f;
  entity.y = 1.5f;
  entity.r = 1.0f;
  uint32_t idx4 = hshg_insert(&hshg, &entity);
  hshg_collide(&hshg);
  assert(collisions == 6);
  end_test();
  
  begin_test("hshg_collide() multi layer x");
  collisions = 0;
  entity.x = 4.1f;
  entity.y = 2.0f;
  entity.r = 1.9f;
  uint32_t idx5 = hshg_insert(&hshg, &entity);
  entity.x = -1.5f;
  entity.y = -1.5f;
  entity.r = 1.0f;
  uint32_t idx6 = hshg_insert(&hshg, &entity);
  entity.x = -1.5f;
  entity.y = -1.5f;
  entity.r = 4.0f;
  uint32_t idx7 = hshg_insert(&hshg, &entity);
  entity.x = 700.0f;
  entity.y = 100.0f;
  entity.r = 32.0f;
  uint32_t idx8 = hshg_insert(&hshg, &entity);
  entity.x = 1234567.0f;
  entity.y = -1234567.0f;
  entity.r = 32.0f;
  uint32_t idx9 = hshg_insert(&hshg, &entity);
  uint32_t grid = hshg_get_grid_safe(&hshg, 32.0f);
  assert(hshg.grids[grid].used == 2);
  hshg_collide(&hshg);
  assert(collisions == 16);
  end_test();
  
  begin_test("hshg_free()");
  hshg_remove(&hshg, idx);
  hshg_remove(&hshg, idx2);
  hshg_remove(&hshg, idx3);
  hshg_remove(&hshg, idx4);
  hshg_remove(&hshg, idx5);
  hshg_remove(&hshg, idx6);
  hshg_remove(&hshg, idx7);
  hshg_remove(&hshg, idx8);
  hshg_remove(&hshg, idx9);
  hshg_free(&hshg);
  end_test();
  
  begin_test("hshg benchmark");
  hshg.update = update;
  hshg.collide = collide;
  assert(!hshg_init(&hshg, CELLS_SIDE, CELL_SIZE));
  uint64_t time1 = time_get_time();
  int layers[16];
  for(uint32_t i = 0; i < AGENTS_NUM; ++i) {
    float min_r = 999999.0f;
    for(int j = 0; j < 5; ++j) {
      float new = AGENT_R + ((float) rand() / RAND_MAX) * 200.0f;
      if(new < min_r) {
        min_r = new;
      }
    }
    idx = hshg_insert(&hshg, &((struct hshg_entity) {
      .x = ((float) rand() / RAND_MAX) * ARENA_WIDTH,
      .y = ((float) rand() / RAND_MAX) * ARENA_HEIGHT,
      .r = min_r,
      .ref = i
    }));
    ++layers[hshg_get_grid_safe(&hshg, min_r)];
    balls[i].vx = ((float) rand() / RAND_MAX) * 2 - 1;
    balls[i].vy = ((float) rand() / RAND_MAX) * 2 - 1;
  }
  uint64_t time2 = time_get_time();
  printf("took %lu ms to insert %d entities\n", time_ns_to_ms(time2 - time1), AGENTS_NUM);
  
  for(uint32_t i = 0; i < 16; ++i) {
    printf("%u entities on grid %u\n", layers[i], i + 1);
  }
  
  uint64_t times[LATENCY_NUM];
  uint64_t update[LATENCY_NUM];
  uint64_t colli[LATENCY_NUM];
  int times_i = 0;
  int logging_num = 0;
  while(1) {
    time1 = time_get_time();
    hshg_update(&hshg);
    const uint64_t t1 = time_get_time();
    update[times_i] = time_ns_to_ms(t1 - time1);
    hshg_collide(&hshg);
    time2 = time_get_time();
    colli[times_i] = time_ns_to_ms(time2 - t1);
    times[times_i++] = time_ns_to_ms(time2 - time1);
    if(++logging_num == LATENCY_NUM) {
      float avg1 = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        avg1 += times[i];
      }
      avg1 /= LATENCY_NUM;
      float avg2 = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        avg2 += update[i];
      }
      avg2 /= LATENCY_NUM;
      float avg3 = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        avg3 += colli[i];
      }
      avg3 /= LATENCY_NUM;
      printf("avg %.2f ms\nupd %.2f ms\ncol %.2f ms\nattempted collisions %u\nsucceeded collisions %u\n", avg1, avg2, avg3, maybe_collisions, collisions);
      logging_num = 0;
      maybe_collisions = 0;
      collisions = 0;
    }
    if(times_i == LATENCY_NUM) {
      (void) memmove(times, times + 1, LATENCY_NUM - 1);
      (void) memmove(update, update + 1, LATENCY_NUM - 1);
      (void) memmove(colli, colli + 1, LATENCY_NUM - 1);
      --times_i;
    }
  }
  end_test();
  
  return 0;
}
