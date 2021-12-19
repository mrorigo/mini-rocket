//#define MR_NO_NETWORK

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "mini-rocket.h"

static float timedifference_msec(struct timeval t0, struct timeval t1)
{
  return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

int main(int argc, char *argv[]) {
  mrocket_t *rocket = NULL;

  if(argc < 1) {
    fprintf(stderr, "Usage: %s [[hostname] [port] | [filename]]\n", argv[0]);
    exit(1);
  }

  if(argc < 3) {
    fprintf(stderr, "Reading from file: %s\n", argv[1]);
    rocket = mrocket_read_from_file(argv[1]);
    rocket->paused = false;
  }
#ifndef MR_NO_NETWORK
  else {
    fprintf(stderr, "Connecting to: %s:%s\n", argv[1], argv[2]);
    rocket = minirocket_connect(argv[1], atoi(argv[2]));
  }
#endif

  if(rocket == NULL) {
    fprintf(stderr, "minirocket initialization failed :(\n");
    exit(3);
  }
  rocket->bpm = 125;
  rocket->rows_per_beat = 8;


  struct timeval t0;
  struct timeval t1;
  float prev_time = 0;

  rocket->time = 0;
  gettimeofday(&t0, NULL);

  mrocket_track_t *track1 = minirocket_create_track(rocket, "mygroup:mytrack");
  assert(track1 != NULL);

  while(1) {
    gettimeofday(&t1, NULL);

    float current_time = timedifference_msec(t0, t1);
    float delta_time = current_time - prev_time;
    if(isfinite(delta_time)) {
      if(minirocket_tick(rocket, delta_time)) {
	float t1val = mrocket_get_value(track1);
	fprintf(stderr, "EXAMPLE: delta_time=%f  rocket->time=%f\n", delta_time, rocket->time); fflush(stderr);
      }
      prev_time = current_time;
    }

    usleep(1);
  }

  return 0;
}
