//#define MR_NO_NETWORK

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#if defined(_WIN32)
#include <Ws2tcpip.h>
#include <winsock2.h>
#else
#include <netdb.h>
#endif

#include "mini-rocket.h"
#ifndef MR_NO_NETWORK
#define RINGBUF_IMPLEMENTATION
#include "ringbuf.h"
#endif

void minirocket_dump_to_file(mrocket_t *rocket, FILE *fd)
{
  for(unsigned int i=0; i < rocket->numtracks; i++) {
    mrocket_track_t *track = rocket->tracks[i];
    fprintf(fd, "#%s\n", track->name);
    for(unsigned int j=0; j < track->numkeys; j++) {
      mrocket_key_t *key = &track->keys[j];
      fprintf(fd, "%d %.6f %d\n", key->row, key->value, key->interp);
    }
  }
}

float minirocket_row2time(mrocket_t *rocket, unsigned long row) 
{
  const float rps = rocket->bpm / 60.0f * rocket->rows_per_beat;
  const float newtime = ((float)(row)) / rps;
  return newtime * 1000.0f + 0.5f;
}

float minirocket_time2rowf(mrocket_t *rocket, float time) 
{
  const float rps = rocket->bpm / 60.0f * rocket->rows_per_beat;
  const float row = rps * ((float)time) * 1.0f / 1000.0f;
  return row;
}

unsigned int minirocket_time2row(mrocket_t *rocket, float time) 
{
  return (unsigned int)(floor(minirocket_time2rowf(rocket, time)));
}

static mrocket_t *mrocket_init() {
  mrocket_t *r = malloc(sizeof(mrocket_t));
  if(r == NULL) {
    return NULL;
  }
  r->paused = true;
  r->numtracks = 0;
#ifndef MR_NO_NETWORK
  r->sock = -1;
#endif
  r->row = 0;
  r->time = 0;
  return r;
}

#ifndef MR_NO_NETWORK
mrocket_t *minirocket_connect(const char *hostname, int port) {
  mrocket_t *r = mrocket_init();
  r->buf = ringbuf_create(512);
  r->handshake = 12;

#if __WIN32__
  int iResult;
  WSADATA wsaData;
  iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if(iResult != 0) {
    fprintf(stderr, "WinSock init failed\n");
    return NULL;
  }
#endif

  struct hostent *hostent = gethostbyname(hostname);
  if(hostent == NULL) {
    fprintf(stderr, "Host %s not found\n", hostname);
    return NULL;
  }

  struct sockaddr_in server = {.sin_family      = AF_INET,
			       .sin_port        = htons(port),
			       .sin_addr.s_addr = *((unsigned long *)hostent->h_addr) };
  if ((r->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return NULL;
  }

  if (connect(r->sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("connect");
    return NULL;
  }

  if (send(r->sock, "hello, synctracker!", 19, 0) == -1){
    perror("send hello");
    return NULL;//exit(5);
  }
  FD_ZERO(&r->fds);
  FD_SET(r->sock, &r->fds);
  return r;
}

void minirocket_disconnect(mrocket_t *r) {
#if defined(_WIN32)
    closesocket(r->sock);
#else
    close(r->sock);
#endif
  free(r);
}

void minirocket_socket_send_pause(mrocket_t *rocket, unsigned int pause)
{
  if(rocket->sock <= 0) {
    return;
  }
  const char head[2] = {CMD_PAUSE, pause};

  if (send(rocket->sock, head, 2, 0) == -1){
    perror("minirocket_socket_send_pause");
  }
}

void minirocket_socket_send_set_row(mrocket_t *rocket, unsigned int row)
{
  if(rocket->sock <= 0) {
    return;
  }
  assert((int)row >= 0);
  const char head[5] = {CMD_SET_ROW, 
			(row>>24)&0xff,  (row>>16)&0xff, (row>>8)&0xff, row&0xff};

  if (send(rocket->sock, head, 5, 0) == -1){
    perror("minirocket_socket_send_set_row");
  }

}

static bool _minirocket_socket_send_get_track(mrocket_t *rocket, const char *name) 
{
  if(rocket->sock <= 0) {
    fprintf(stderr, "minirocket_socket_send_get_track: no socket"); fflush(stderr); fflush(stdout);
    return false;
  }
  unsigned int len = strlen(name);
  const char head[5] = {CMD_GET_TRACK, 
			(len>>24)&0xff,  (len>>16)&0xff, (len>>8)&0xff, len&0xff};

  if (send(rocket->sock, head, 5, 0) == -1){
    perror("minirocket_socket_send_get_track: send cmd"); fflush(stderr); fflush(stdout);
    return false;
  }

  if (send(rocket->sock, name, len, 0) == -1){
    perror("minirocket_socket_send_get_track: send name"); fflush(stderr); fflush(stdout);
    return false;
  }

  return true;
}


static int _minirocket_socket_ringbuf_read(mrocket_t *rocket, int max_bytes) {
  struct timeval to = {0, 0};

  FD_SET(rocket->sock, &rocket->fds);

  if(select((int)rocket->sock + 1, &rocket->fds, NULL, NULL, &to) <= 0) {
    return ringbuf_size(rocket->buf);
  }

  unsigned char buf[max_bytes];
  int numbytes;
  /**
   *   0 1 2 3 4 5 6 7 8 9
   *                     w
   *                   r
   *
   */
  int max = rocket->buf->max - rocket->buf->size;
  max = max > max_bytes ? max_bytes : max;
  //  fprintf(stderr, "ringbuf_read max %d bytes\n", max);
  
  if(max <= 0) {
    ringbuf_print(rocket->buf);
    return ringbuf_size(rocket->buf);
  }
  assert(max > 0);
  if ((numbytes=recv(rocket->sock, (char *)buf, max, 0x0)) == -1) {
    if(errno != EAGAIN && errno != 0) {
      perror("recv"); fflush(stderr);
      return -1;
    }
  }
  else if(numbytes > 0) {
    ringbuf_write(rocket->buf, buf, numbytes);
  }

  return ringbuf_size(rocket->buf);
}

#endif // #ifndef MR_NO_NETWORK

static int _mrocket_track_sort_compare(const void *a, const void *b) {
  mrocket_key_t *k1 = (mrocket_key_t *)a;
  mrocket_key_t *k2 = (mrocket_key_t *)b;
  return k1->row - k2->row;
}

static void _minirocket_sort_keys(mrocket_track_t *track) {
  qsort(track->keys, track->numkeys, sizeof(mrocket_key_t), _mrocket_track_sort_compare);
}

mrocket_t *minirocket_read_from_file(const char *filename) 
{
  FILE *fd = fopen(filename, "r");
  if(fd == NULL) {
    perror("fopen");
    return NULL;
  }
  mrocket_t *rocket = mrocket_init();

  char buf[512];
  mrocket_track_t *track = NULL;
  while((fgets(buf, 512, fd) != NULL)) {
    if(buf[0] == '#') { // track name
      track = malloc(sizeof(mrocket_track_t));
      memset(track, 0, sizeof(mrocket_track_t));
      track->rocket = rocket;
      track->numkeys = 0;
      track->id = rocket->numtracks;
      track->name = strdup(buf+1);
      track->name[strlen(track->name)-1] = 0;
      rocket->tracks[rocket->numtracks++] = track;
    }
    else {
      assert(track != NULL);

      char *b = buf;
      mrocket_key_t *key = &track->keys[track->numkeys++];
      while(isalnum(*b++)) {} *(b-1)=0;
      char *vb = b;
      key->row = atol(buf);
      while(isalnum(*b++)) {} *(b-1)=0;
      key->value = (float)atof(vb);
      vb = b;
      while(isalnum(*b++)) {} *(b-1)=0;
      key->interp = (unsigned char)b[0]-'0';

      _minirocket_sort_keys(track);
    }
  }
  return rocket;
}


bool minirocket_write_to_file(mrocket_t *rocket, const char *filename) 
{
  FILE *fd = fopen(filename, "w");
  if(fd == NULL) {
    perror("fopen");
    return false;
  }
  minirocket_dump_to_file(rocket, fd);

  fclose(fd);
  return true;
}




static void minirocket_delete_key(mrocket_t *rocket, 
				  unsigned int track_no, 
				  unsigned int row) {

  assert(track_no <= rocket->numtracks);
  mrocket_track_t *track = rocket->tracks[track_no];
  for(unsigned int i=0; i < track->numkeys; i++) {
    mrocket_key_t *key = &track->keys[i];
    if(key->row == row) {
      // Delete this key
      memcpy(((char *)track->keys) + i * sizeof(mrocket_key_t),
	     ((char *)track->keys) + (i+1) * sizeof(mrocket_key_t),
	     (track->numkeys - i)* sizeof(mrocket_key_t));
      track->numkeys--;
      _minirocket_sort_keys(track);
      return;
    }
  }
  fprintf(stderr, "minirocket: FAILED delete key: %d %d  numkeys:%d~\n", track_no, row, track->numkeys); fflush(stderr);
  assert(false);
}

static void minirocket_set_key(mrocket_t *rocket, 
			       unsigned int track_no, 
			       unsigned int row, 
			       float value, 
			       unsigned char interp) 
{
  if(track_no >= rocket->numtracks) {
    fprintf(stderr, "minirocket: track_no %d is not valid. max %d \n", track_no, rocket->numtracks);
    return;
  }

  mrocket_track_t *track = rocket->tracks[track_no];

  for(unsigned int i=0; i < track->numkeys; i++) {
    mrocket_key_t *key = &track->keys[i];
    if(key->row == row) {
      key->value = value;
      key->interp = interp;
      return;
    }
  }

  // new key
  mrocket_key_t *key = &track->keys[track->numkeys];
  key->row = row;
  key->value = value;
  key->interp = interp;
  track->numkeys++;
  assert(track->numkeys < MR_MAX_KEYS);
  _minirocket_sort_keys(track);
}

mrocket_track_t * minirocket_create_track(mrocket_t *rocket, const char *name) 
{
  for(unsigned int i=0; i < rocket->numtracks; i++) {
    if(strcmp(name, rocket->tracks[i]->name) == 0) {
      return rocket->tracks[i];
    }
  }

#ifndef MR_NO_NETWORK
  if(!_minirocket_socket_send_get_track(rocket, name)) {
    fprintf(stderr, "rocket ERROR: could not send GET_TRACK\n"); fflush(stderr);
    return NULL;
  }
#endif

  mrocket_track_t *track = malloc(sizeof(mrocket_track_t));
  track->name = strdup(name);
  track->numkeys = 0;
  track->id = rocket->numtracks;
  track->rocket = rocket;
  rocket->tracks[rocket->numtracks++] = track;
  // fprintf(stderr, "rocket: created track %s\n", name); fflush(stderr);
  return track;
}

static int _find_key_index(mrocket_key_t *keys, unsigned int numkeys, unsigned int row)
{
  int lo = 0, hi = numkeys;
  while (lo < hi) {
    unsigned int mi = ((hi + lo) >> 1);

    if (keys[mi].row < row) {
      lo = mi + 1;
    } else if (keys[mi].row > row) {
      hi = mi;
    } else {
      return mi;
    }
  }
  return lo - 1;
}

float minirocket_get_value(mrocket_track_t *track) 
{
  float rowf = minirocket_time2rowf(track->rocket, track->rocket->time);
  unsigned int row = (unsigned int)floor(rowf);
  int index = _find_key_index(track->keys, track->numkeys, row);

  if(index < 0) {
    return track->keys[0].value;
  }

  if((unsigned int)index > track->numkeys - 2) {
    return track->keys[track->numkeys-1].value;
  }
  
  unsigned int k0 = track->keys[index].row;
  unsigned int k1 = track->keys[index+1].row;
  float t = (rowf - (float)k0) / ((float)k1 - (float)k0);
  float a = track->keys[index].value;
  float b = track->keys[index+1].value;
  //  fprintf(stderr, "index:%d: %f -> %f  %f  (%d)\n", index, a, b, t, track->keys[index].interp);  
  switch(track->keys[index].interp) {
  case 0:
    return a;
  case 1:
    return a + (b - a) * t;
  case 2:
    return a + (b - a) * t * t * (3 - 2 * t);
  case 3:
    return a + (b - a) * pow(t, 2.0);
  default:
    fprintf(stderr, "minirocket_get_value for %s: index: %d  nkeys: %d   interp: %d\n", track->name, index, track->numkeys, track->keys[index].interp);
    assert(false);
  }
}

bool minirocket_tick(mrocket_t *rocket) {
  bool new_row = false;

  if(!rocket->paused) {

    unsigned int nrow = minirocket_time2row(rocket, rocket->time);
    if(nrow != rocket->row) {
      // fprintf(stderr, "minirocket_tick: row: %d   new row: %d\n", rocket->row, nrow); fflush(stderr);
      rocket->row = nrow;
      new_row = true;
#ifndef MR_NO_NETWORK
      minirocket_socket_send_set_row(rocket, rocket->row);
#endif
    }
  } else {
    rocket->time = minirocket_row2time(rocket, rocket->row);
  }

  if(rocket->sock <= 0) {
    return new_row;
  }


#ifndef MR_NO_NETWORK
  int r = _minirocket_socket_ringbuf_read(rocket, 32);  // largest single packet is 13 bytes
  if(r == -1) {
    return new_row;
  }
  ringbuf_t *buf = rocket->buf;
  /* if(r != 0) { */
  /*   fprintf(stderr, "ringbuf read %d bytes\n", r); */
  /* } */
  if(rocket->handshake > 0) {
    int skip = r > rocket->handshake ? rocket->handshake : r;
    if(skip > 0) {
      rocket->handshake -= skip; // must become 0 once
      //fprintf(stderr, "Handshake %d\n", rocket->handshake); fflush(stderr);
      //ringbuf_print(buf);
      ringbuf_skip(buf, skip);
    }
    return false;
  }
  else if(rocket->handshake == 0) {
    rocket->handshake = -1;
    //fprintf(stderr, "minirocket: Handshake done~\n"); fflush(stderr);
    //ringbuf_print(buf);
  }

  else if(ringbuf_size(buf) > 1) {
    unsigned char peek = ringbuf_peek(buf);
    if(peek == CMD_PAUSE) {
      ringbuf_skip(buf, 1);
      rocket->paused = ringbuf_read_byte(buf) == 1;
      //      fprintf(stderr, "Pause: %s~\n", rocket->paused?"Y":"N"); fflush(stderr);
    }
    else if(peek == CMD_SET_ROW) { 
      if(ringbuf_size(buf) > sizeof(long)) {
	ringbuf_skip(buf, 1);
	rocket->row = ringbuf_read_long(buf);
      } else {
	// fprintf(stderr, "peek: %02x, waiting> %d\n", peek, ringbuf_size(buf)); fflush(stderr);
      }
    }
    else if(peek == CMD_SET_KEY) {
      if(ringbuf_size(buf) > sizeof(long)*3+1) {

	//	fprintf(stderr, "rb when SET_KEY"); fflush(stderr);
	//	ringbuf_print(buf);

	ringbuf_skip(buf, 1);
	unsigned long track = ringbuf_read_long(buf);
	unsigned long row = ringbuf_read_long(buf);
	float value = ringbuf_read_float(buf);
	unsigned char interp = ringbuf_read_byte(buf);

	minirocket_set_key(rocket, track, row, value, interp);
      }
      else {
	// fprintf(stderr, "peek: %02x, waiting for 13> %d\n", peek, ringbuf_size(buf)); fflush(stderr);
      }    
    }
    else if(peek == CMD_DELETE_KEY) {
      if(ringbuf_size(buf) > sizeof(long)*2+1) {
	ringbuf_skip(buf, 1);
	unsigned long track = ringbuf_read_long(buf);
	unsigned long row = ringbuf_read_long(buf);
	minirocket_delete_key(rocket, track, row);
      }
    }
    else if(peek == CMD_SAVE_TRACKS) {
      ringbuf_skip(buf, 1);
      fprintf(stderr, "minirocket: saving to file 'demo.rkt'!\n"); fflush(stderr);
      minirocket_write_to_file(rocket, "demo.rkt");
      fprintf(stderr, "minirocket: saved to file!\n"); fflush(stderr);
    }
    else {
      fprintf(stderr, "minirocket: protocol error: %d\n", peek); fflush(stderr);
      ringbuf_print(buf);
      ringbuf_skip(buf, 1);
    }
  }
#endif

  return new_row;
}


