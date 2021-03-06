#ifndef __MINIROCKET_H__
#define __MINIROCKET_H__

#include <stdbool.h>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#ifndef MR_NO_NETWORK
#include "ringbuf.h"
#endif

#define MR_MAX_TRACKS 64
#define MR_MAX_KEYS 256

enum {CMD_SET_KEY, CMD_DELETE_KEY, CMD_GET_TRACK, CMD_SET_ROW, CMD_PAUSE, CMD_SAVE_TRACKS};

typedef unsigned int trackid_t;

typedef struct __mrocket_key {
  unsigned int	row;
  float		value;
  unsigned char interp;
  unsigned char p1,p2,p3;
} mrocket_key_t;

typedef struct __mrocket_track_t {
  char		*name;
  unsigned int	 id;
  unsigned int	 numkeys;
  mrocket_key_t	 keys[MR_MAX_KEYS];
  struct __mrocket_t *rocket;
} mrocket_track_t;

typedef struct __mrocket_t {
  bool		  paused;
  int             bpm;
  int             rows_per_beat;
  float           time;  // matches row via time2row
  unsigned int	  row;   // matches time via row2time
  unsigned int	  numtracks;
  mrocket_track_t *tracks[MR_MAX_TRACKS];
#ifndef MR_NO_NETWORK
  int		  sock;
  int		  handshake;
  fd_set          fds;
  ringbuf_t	  *buf;
#endif
} mrocket_t;


#ifndef MR_NO_NETWORK
mrocket_t		*minirocket_connect(const char *hostname, int port);
void			 minirocket_disconnect(mrocket_t *r);
void                     minirocket_socket_send_set_row(mrocket_t *rocket, unsigned int row);
void                     minirocket_socket_send_pause(mrocket_t *rocket, unsigned int pause);
#endif
unsigned int		 minirocket_time2row(mrocket_t *r,   float time);
float			 minirocket_row2time(mrocket_t *r,   unsigned long row);
mrocket_t *		 minirocket_read_from_file(const char *filename);
bool			 minirocket_write_to_file(mrocket_t *r, const char *filename);
bool			 minirocket_tick(mrocket_t *rocket);
mrocket_track_t *	 minirocket_create_track(mrocket_t *rocket, const char *name);
float			 minirocket_get_value(mrocket_track_t *track);
void                     minirocket_dump_to_file(mrocket_t *rocket, FILE *fd);
#endif
