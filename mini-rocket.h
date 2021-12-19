#ifndef __MINIROCKET_H__
#define __MINIROCKET_H__

#include <stdbool.h>

#define RINGBUF_IMPLEMENTATION
#include "ringbuf.h"

#define MR_MAX_TRACKS 64
#define MR_MAX_KEYS 64

enum {CMD_SET_KEY, CMD_DELETE_KEY, CMD_GET_TRACK, CMD_SET_ROW, CMD_PAUSE, CMD_SAVE_TRACKS};

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
#ifndef MR_NO_NETWORK
  ringbuf_t	  *buf;
#endif
} mrocket_t;


#ifndef MR_NO_NETWORK
mrocket_t *minirocket_connect(const char *hostname, int port);
void       minirocket_disconnect(mrocket_t *r);
#endif
int        minirocket_time2row(mrocket_t *r,   float time);
float      minirocket_row2time(mrocket_t *r,   unsigned long row);
bool       mrocket_write_to_file(mrocket_t *r, const char *filename);
#endif
