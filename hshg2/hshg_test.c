#include <shnet/test.h>

#include "hshg.h"

#include <shnet/time.h>

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define AGENTS_NUM 50000

#define CELLS_SIDE 512
#define AGENT_R 7
#define CELL_SIZE 128
#define ARENA_WIDTH (1048576 >> 2)
#define ARENA_HEIGHT (1048576 >> 2)

#define LATENCY_NUM 40

struct ball {
  float vx;
  float vy;
};

struct ball balls[AGENTS_NUM];

void update(struct hshg* hshg, uint32_t x) {
  struct hshg_entity* const a = hshg->entities + x;
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

void collide(const struct hshg* hshg, const struct hshg_entity* const a, const struct hshg_entity* const b) {
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
    balls[b->ref].vx -= cosf(angle) * b_mass_diff;
    balls[b->ref].vy -= sinf(angle) * b_mass_diff;
  }
}

int main() {
  srand(time_get_time());
  struct hshg hshg = {0};

  test_begin("hshg benchmark");
  puts("");
  hshg.update = update;
  hshg.collide = collide;
  hshg.entities_size = AGENTS_NUM;
  assert(!hshg_init(&hshg, CELLS_SIDE, CELL_SIZE));
  uint64_t time1 = time_get_time();
  for(uint32_t i = 0; i < AGENTS_NUM; ++i) {
    /*float min_r = 999999.0f;
    for(int j = 0; j < 100; ++j) {
      float new = AGENT_R + ((float) rand() / RAND_MAX) * 500.0f;
      if(new < min_r) {
        min_r = new;
      }
    }*/
    hshg_insert(&hshg, &((struct hshg_entity) {
      .x = ((float) rand() / RAND_MAX) * ARENA_WIDTH,
      .y = ((float) rand() / RAND_MAX) * ARENA_HEIGHT,
      .r = AGENT_R,
      .ref = i
    }));
    balls[i].vx = ((float) rand() / RAND_MAX) * 2 - 1;
    balls[i].vy = ((float) rand() / RAND_MAX) * 2 - 1;
  }
  uint64_t time2 = time_get_time();
  printf("took %lu ms to insert %d entities\n%u grids\n\n", time_ns_to_ms(time2 - time1), AGENTS_NUM, hshg.grids_len);
  
  double upd[LATENCY_NUM];
  double col[LATENCY_NUM];
  double opt[LATENCY_NUM];
  int i = 0;
  while(1) {
    const uint64_t upd_time = time_get_time();
    hshg_update(&hshg);
    const uint64_t opt_time = time_get_time();
    hshg_optimize(&hshg);
    const uint64_t col_time = time_get_time();
    hshg_collide(&hshg);
    const uint64_t end_time = time_get_time();

    upd[i] = (double)(opt_time - upd_time) / 1000000.0f;
    opt[i] = (double)(col_time - opt_time) / 1000000.0f;
    col[i] = (double)(end_time - col_time) / 1000000.0f;

    if(i + 1 == LATENCY_NUM) {
      double upd_avg = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        upd_avg += upd[i];
      }
      upd_avg /= LATENCY_NUM;

      double opt_avg = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        opt_avg += opt[i];
      }
      opt_avg /= LATENCY_NUM;
      
      double col_avg = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        col_avg += col[i];
      }
      col_avg /= LATENCY_NUM;
      
      printf("upd %.2lf ms\nopt %.2lf ms\ncol %.2lf ms\nall %.2lf ms\nattempted collisions %u\nsucceeded collisions %u\n",
        upd_avg, opt_avg, col_avg, upd_avg + opt_avg + col_avg, maybe_collisions, collisions);

      maybe_collisions = 0;
      collisions = 0;
    }
    i = (i + 1) % LATENCY_NUM;
  }
  test_end();
  
  return 0;
}
