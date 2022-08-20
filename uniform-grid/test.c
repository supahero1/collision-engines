#include "grid.h"

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <shnet/test.h>

struct ball {
  float vx;
  float vy;
};

#define AGENTS_NUM 200000

#define COLLISION_STRENGTH 1.0
#define STARTING_SPEED_MUL 1
#define RADIUS_DIFF 200
#define MIN_RADIUS 10.0
#define MAX_SPEED 2.0

#define GRID_CELL_SIZE 512
#define GRID_WIDTH 262144
#define GRID_HEIGHT 262144

struct ball balls[AGENTS_NUM];

int grid_onupdate(struct grid* grid, struct grid_entity* a) {
  a->pos.x += balls[a->ref].vx;
	if(a->pos.x - a->pos.w <= 0) {
		balls[a->ref].vx = fmin(MAX_SPEED, balls[a->ref].vx + 1);
	} else if(a->pos.x + a->pos.w >= GRID_WIDTH) {
		balls[a->ref].vx = fmax(-MAX_SPEED, balls[a->ref].vx - 1);
	}
	a->pos.y += balls[a->ref].vy;
	if(a->pos.y - a->pos.h <= 0) {
		balls[a->ref].vy = fmin(MAX_SPEED, balls[a->ref].vy + 1);
	} else if(a->pos.y + a->pos.h >= GRID_HEIGHT) {
		balls[a->ref].vy = fmax(-MAX_SPEED, balls[a->ref].vy - 1);
	}
  return 1;
}

void grid_oncollision(struct grid* grid, struct grid_entity* a, struct grid_entity* b) {
  const double d = (a->pos.x - b->pos.x) * (a->pos.x - b->pos.x) + (a->pos.y - b->pos.y) * (a->pos.y - b->pos.y);
  if(d <= (a->pos.w + b->pos.w) * (a->pos.w + b->pos.w)) {
    const double angle = atan2(a->pos.y - b->pos.y, a->pos.x - b->pos.x);
    const float a_mass_diff = b->pos.w / a->pos.w;
    balls[a->ref].vx = fmax(fmin(balls[a->ref].vx + cos(angle) * a_mass_diff * COLLISION_STRENGTH, MAX_SPEED), -MAX_SPEED);
    balls[a->ref].vy = fmax(fmin(balls[a->ref].vy + sin(angle) * a_mass_diff * COLLISION_STRENGTH, MAX_SPEED), -MAX_SPEED);
  }
}

void grid_onnomem(struct grid* grid) {
  puts("no mem");
  exit(-1);
}

uint64_t time_get_time() {
  struct timespec tp;
  (void) clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_sec * 1000000000 + tp.tv_nsec;
}

int main() {
	srand(time_get_time());
  
  struct grid grid = {0};
  grid.cell_size = GRID_CELL_SIZE;
  grid.onnomem = grid_onnomem;
	grid.onupdate = grid_onupdate;
  grid.oncollision = grid_oncollision;
  grid.cells_x = GRID_WIDTH / GRID_CELL_SIZE;
  grid.cells_y = GRID_HEIGHT / GRID_CELL_SIZE;
  
  grid_init(&grid);
	
  for(int i = 0; i < AGENTS_NUM; ++i) {
		balls[i].vx = (rand() % (STARTING_SPEED_MUL << 1)) - STARTING_SPEED_MUL;
		balls[i].vy = (rand() % (STARTING_SPEED_MUL << 1)) - STARTING_SPEED_MUL;
    float smol = MIN_RADIUS + RADIUS_DIFF;
    for(int j = 0; j < 100; ++j) {
      const float r = MIN_RADIUS + (rand() % RADIUS_DIFF);
      if(r < smol) {
        smol = r;
      }
    }
		grid_insert(&grid, (struct grid_entity) {
      .pos = (struct grid_pos) { (((uint32_t) rand() << 16) | (uint32_t) rand()) % GRID_WIDTH, (((uint32_t) rand() << 16) | (uint32_t) rand()) % GRID_HEIGHT, smol, smol },
      .ref = i
    });
	}
  
  uint64_t times[50];
  int tim = 0;
  int h = 0;
  while(1) {
    const uint64_t time = time_get_time();
    grid_update(&grid);
    grid_collide(&grid);
    const uint64_t end_time = time_get_time();
    if(tim == 50) {
      (void) memmove(times, times + 1, 49 * 8);
      times[49] = end_time - time;
    } else {
      times[tim++] = end_time - time;
    }
    if(++h == 50) {
      h = 0;
      uint64_t sum = 0;
      for(int i = 0; i < 50; ++i) {
        sum += times[i];
      }
      printf("%lf fps\n", 50000000000.0 / sum );
    }
  }
  return 0;
}