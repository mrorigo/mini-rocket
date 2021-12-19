#ifndef __MINIROCKET_H__
#define __MINIROCKET_H__

#include <stdbool.h>

#define RINGBUF_IMPLEMENTATION
#include "ringbuf.h"

#define MR_MAX_TRACKS 64
#define MR_MAX_KEYS 64

enum Commands {
  CMD_SET_KEY	  = 0,
  CMD_DELETE_KEY  = 1,
  CMD_GET_TRACK	  = 2,
  CMD_SET_ROW	  = 3,
  CMD_PAUSE	  = 4,
  CMD_SAVE_TRACKS = 5
};

typedef struct __mrocket_key {
  unsigned int	row;
  float		value;
  unsigned char interp;
} mrocket_key_t;

typedef struct __mrocket_track_t {
  char		*name;
  unsigned int	 id;
  unsigned int	 numkeys;
  mrocket_key_t	 keys[MR_MAX_KEYS];
} mrocket_track_t;

typedef struct __mrocket_t {
  int		  sock;
  bool		  paused;
  int		  handshake;
  int             bpm;
  int             rows_per_beat;
  unsigned long	  row;   // matches time via row2time
  float           time;  // matches row via time2row
  unsigned int	  numtracks;
  mrocket_track_t *tracks[MR_MAX_TRACKS];
  ringbuf_t	  *buf;
} mrocket_t;


#ifndef MR_NO_NETWORK
mrocket_t *minirocket_connect(const char *hostname, int port);
void       minirocket_disconnect(mrocket_t *r);
#endif
int        minirocket_time2row(mrocket_t *rocket, float time);
float      minirocket_row2time(mrocket_t *rocket, unsigned long row);

#endif
