#include <shnet/tests.h>

#include "hshg.h"

#include <shnet/time.h>

#include <math.h>
#include <string.h>

#define AGENTS_NUM 1000000

#define CELLS_SIDE 8192
#define AGENT_R 7
#define CELL_SIZE 128
#define ARENA_WIDTH 1048576
#define ARENA_HEIGHT 1048576

#define LATENCY_NUM 10

struct ball {
  float vx;
  float vy;
};

struct ball balls[AGENTS_NUM];

void update(struct hshg* hshg, uint32_t x) {
  struct hshg_entity* const restrict a = hshg->entities + x;
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
  
  hshg_move(hshg, x);
}

uint32_t maybe_collisions = 0;
uint32_t collisions = 0;

void collide(struct hshg* hshg, uint32_t x, uint32_t y) {
  const struct hshg_entity* const restrict a = hshg->entities + x;
  const struct hshg_entity* const restrict b = hshg->entities + y;
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
  struct hshg hshg = {0};
  
  begin_test("hshg benchmark");
  puts("");
  hshg.update = update;
  hshg.collide = collide;
  assert(!hshg_init(&hshg, CELLS_SIDE, CELL_SIZE));
  uint64_t time1 = time_get_time();
  for(uint32_t i = 0; i < AGENTS_NUM; ++i) {
    float min_r = AGENT_R;/*999999.0f;
    for(int j = 0; j < 100; ++j) {
      float new = AGENT_R + ((float) rand() / RAND_MAX) * 200.0f;
      if(new < min_r) {
        min_r = new;
      }
    }*/
    hshg_insert(&hshg, &((struct hshg_entity) {
      .x = ((float) rand() / RAND_MAX) * ARENA_WIDTH,
      .y = ((float) rand() / RAND_MAX) * ARENA_HEIGHT,
      .r = min_r,
      .ref = i
    }));
    balls[i].vx = ((float) rand() / RAND_MAX) * 2 - 1;
    balls[i].vy = ((float) rand() / RAND_MAX) * 2 - 1;
  }
  uint64_t time2 = time_get_time();
  printf("took %lu ms to insert %d entities\n", time_ns_to_ms(time2 - time1), AGENTS_NUM);
  
  float times[LATENCY_NUM];
  float update[LATENCY_NUM];
  float colli[LATENCY_NUM];
  int times_i = 0;
  int logging_num = 0;
  while(1) {
    time1 = time_get_time();
    hshg_update(&hshg);
    const uint64_t t1 = time_get_time();
    update[times_i] = (float)(t1 - time1) / 1000000.0f;
    hshg_collide(&hshg);
    time2 = time_get_time();
    colli[times_i] = (float)(time2 - t1) / 1000000.0f;
    times[times_i++] = (float)(time2 - time1) / 1000000.0f;
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
      (void) memmove(times, times + 1, sizeof(times[0]) * (LATENCY_NUM - 1));
      (void) memmove(update, update + 1, sizeof(times[0]) * (LATENCY_NUM - 1));
      (void) memmove(colli, colli + 1, sizeof(times[0]) * (LATENCY_NUM - 1));
      --times_i;
    }
  }
  end_test();
  
  return 0;
}
