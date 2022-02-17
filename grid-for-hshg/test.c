#define GRID_FAST_MATH_X 1
#define GRID_FAST_MATH_Y 1

#include "grid.h"

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <shnet/time.h>
#include <shnet/error.h>

#define AGENTS_NUM 1000000

#define COLLISION_STRENGTH 1.0

#define CELLS_X 8192
#define CELLS_Y 8192
#define AGENT_R 7
#define CELL_SIZE 16
#define ARENA_WIDTH 1000000
#define ARENA_HEIGHT 1000000

#define LATENCY_NUM 5

struct ball {
  float vx;
  float vy;
};

struct ball balls[AGENTS_NUM];

int update(struct grid* grid, struct grid_entity* a) {
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
  
  return 1;
}

uint64_t collisions = 0;
uint64_t confirmed_collisions = 0;

void collision(struct grid* grid, struct grid_entity* a, struct grid_entity* b) {
  const float xd = a->x - b->x;
  const float yd = a->y - b->y;
  const float d = xd * xd + yd * yd;
  
  ++collisions;
  
  /*printf("----> entity collision\n"
  "first obj:\n"
  "x: %f\n"
  "y: %f\n"
  "r: %f\n"
  "secon obj: \n"
  "x: %f\n"
  "y: %f\n"
  "r: %f\n"
  "distance: %f\n"
  "min dist: %f\n",
  a_x, a_y, a_r, b_x, b_y, b_r, d, (a_r + b_r) * (a_r + b_r)
  );*/
  
  if(d <= (a->r + b->r) * (a->r + b->r)) {
    ++confirmed_collisions;
    const float angle = atan2f(yd, xd);
    const float a_mass_diff = b->r / a->r;
    //printf("^---- new velocities: %f %f\n", cosf(angle) * a_mass_diff * COLLISION_STRENGTH, sinf(angle) * a_mass_diff * COLLISION_STRENGTH);
    balls[a->ref].vx += cosf(angle) * a_mass_diff * COLLISION_STRENGTH;
    balls[a->ref].vy += sinf(angle) * a_mass_diff * COLLISION_STRENGTH;
  }
}



int error_handler(int code) {
  assert(0);
}

int main() {
  srand(time_get_time());
  
  struct grid g = {
    .cells_x = CELLS_X,
    .cells_y = CELLS_Y,
    .cell_size = CELL_SIZE,
    .entities_size = AGENTS_NUM,
    
    .update = update,
    .collision = collision
  };
  assert(!grid_init(&g));
  
  uint64_t time1 = time_get_time();
  for(uint32_t i = 0; i < AGENTS_NUM; ++i) {
    //printf("inserting %u/%u\n", i + 1, AGENTS_NUM);
    grid_insert(&g, &((struct grid_entity) {
      .x = ((float) rand() / RAND_MAX) * ARENA_WIDTH,
      .y = ((float) rand() / RAND_MAX) * ARENA_HEIGHT,
      .r = AGENT_R,
      .collides_with = 1,
      .collision_mask = 1,
      .ref = i
    }));
    balls[i].vx = ((float) rand() / RAND_MAX) * 2 - 1;
    balls[i].vy = ((float) rand() / RAND_MAX) * 2 - 1;
  }
  uint64_t time2 = time_get_time();
  printf("took %lu ms to insert %d entities\n", time_ns_to_ms(time2 - time1), AGENTS_NUM);
  
  uint64_t times[LATENCY_NUM];
  uint64_t update[LATENCY_NUM];
  uint64_t colli[LATENCY_NUM];
  int times_i = 0;
  int logging_num = 0;
  while(1) {
    time1 = time_get_time();
    grid_update(&g);
    const uint64_t t1 = time_get_time();
    update[times_i] = time_ns_to_ms(t1 - time1);
    grid_collide(&g);
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
      printf("avg %.2f ms\nupd %.2f ms\ncol %.2f ms\nattempted collisions %lu\nsucceeds collisions %lu\n", avg1, avg2, avg3, collisions, confirmed_collisions);
      logging_num = 0;
      collisions = 0;
      confirmed_collisions = 0;
    }
    if(times_i == LATENCY_NUM) {
      (void) memmove(times, times + 1, LATENCY_NUM - 1);
      (void) memmove(update, update + 1, LATENCY_NUM - 1);
      (void) memmove(colli, colli + 1, LATENCY_NUM - 1);
      --times_i;
    }
  }
  
  return 0;
}