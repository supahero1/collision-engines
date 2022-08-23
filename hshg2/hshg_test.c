#define TEST_NO_ERR_HANDLER
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
#define ARENA_WIDTH 22500
#define ARENA_HEIGHT 22500

#define LATENCY_NUM 20

#define SINGLE_LAYER 0

struct ball {
  float vx;
  float vy;
};

struct ball balls[AGENTS_NUM];

void update(struct hshg* hshg, hshg_entity_t x) {
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

uint64_t maybe_collisions = 0;
uint64_t collisions = 0;

void collide(const struct hshg* hshg, const struct hshg_entity* a, const struct hshg_entity* b) {
  (void) hshg;
  const float xd = a->x - b->x;
  const float yd = a->y - b->y;
  const float d = xd * xd + yd * yd;
  ++maybe_collisions;
  if(d <= (a->r + b->r) * (a->r + b->r)) {
    ++collisions;
    const float angle = atan2f(yd, xd);
    balls[a->ref].vx += cosf(angle);
    balls[a->ref].vy += sinf(angle);
    balls[b->ref].vx -= cosf(angle);
    balls[b->ref].vy -= sinf(angle);
  }
}

uint64_t queries = 0;

void query(const struct hshg* hshg, const struct hshg_entity* a) {
  ++queries;
}

int main() {
  srand(time_get_time());
  struct hshg hshg = {0};

  test_begin("hshg benchmark");
  puts("");
  hshg.update = update;
  hshg.collide = collide;
  hshg.query = query;
  hshg.entities_size = AGENTS_NUM;
  assert(!hshg_init(&hshg, CELLS_SIDE, CELL_SIZE));

  uint64_t ins_time = time_get_time();
  for(hshg_entity_t i = 0; i < AGENTS_NUM; ++i) {
#if SINGLE_LAYER == 0
    float min_r = 999999.0f;
    for(int j = 0; j < 30; ++j) {
      float new = AGENT_R + ((float) rand() / RAND_MAX) * 300.0f;
      if(new < min_r) {
        min_r = new;
      }
    }
#else
    float min_r = AGENT_R;
#endif
    hshg_insert(&hshg, &((struct hshg_entity) {
      .x = ((float) rand() / RAND_MAX) * ARENA_WIDTH,
      .y = ((float) rand() / RAND_MAX) * ARENA_HEIGHT,
      .r = min_r,
      .ref = i
    }));
    balls[i].vx = ((float) rand() / RAND_MAX) * 8 - 4;
    balls[i].vy = ((float) rand() / RAND_MAX) * 8 - 4;
  }
  uint64_t ins_end_time = time_get_time();
  printf("took %lu ms to insert %d entities\n%u grids\n\n", time_ns_to_ms(ins_end_time - ins_time), AGENTS_NUM, hshg.grids_len);
  
  double upd[LATENCY_NUM];
  double opt[LATENCY_NUM];
  double col[LATENCY_NUM];
  double qry[LATENCY_NUM];
  int i = 0;
  while(1) {
    const uint64_t upd_time = time_get_time();
    hshg_update(&hshg);
    const uint64_t opt_time = time_get_time();
    hshg_optimize(&hshg);
    const uint64_t col_time = time_get_time();
    hshg_collide(&hshg);
    const uint64_t qry_time = time_get_time();
    for(int x = 0; x < 10; ++x) {
      for(int y = 0; y < 10; ++y) {
        hshg_query(&hshg, x * 1920, y * 1080, (x + 1) * 1920, (y + 1) * 1080);
      }
    }
    const uint64_t end_time = time_get_time();

    upd[i] = (double)(opt_time - upd_time) / 1000000.0;
    opt[i] = (double)(col_time - opt_time) / 1000000.0;
    col[i] = (double)(qry_time - col_time) / 1000000.0;
    qry[i] = (double)(end_time - qry_time) / 1000000.0;

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

      double qry_avg = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        qry_avg += qry[i];
      }
      qry_avg /= LATENCY_NUM;
      
      printf("upd %.2lf ms\nopt %.2lf ms\ncol %.2lf ms\nqry %.2lf ms\nall %.2lf ms\nattempted collisions %lu\nsucceeded collisions %lu\nqueried entities %lu\n",
        upd_avg, opt_avg, col_avg, qry_avg, upd_avg + opt_avg + col_avg + qry_avg, maybe_collisions, collisions, queries);

      maybe_collisions = 0;
      collisions = 0;
      queries = 0;
    }
    i = (i + 1) % LATENCY_NUM;
  }
  assert(0);
}
