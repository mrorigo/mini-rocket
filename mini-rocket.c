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
#endif

#include "mini-rocket.h"


float minirocket_row2time(mrocket_t *rocket, unsigned long row) 
{
  const float rps = rocket->bpm / 60.0f * rocket->rows_per_beat;
  const float newtime = ((float)(row)) / rps;
  return newtime * 1000.0f + 0.5f;
}

int minirocket_time2row(mrocket_t *rocket, float time) 
{
  const float rps = rocket->bpm / 60.0f * rocket->rows_per_beat;
  const float row = rps * ((float)time) * 1.0f / 1000.0f;
  return (int)(floor(row + 0.5f));
}

mrocket_t *mrocket_init() {
  mrocket_t *r = malloc(sizeof(mrocket_t));
  if(r == NULL) {
    return NULL;
  }
  r->paused = true;
  r->numtracks = 0;
  r->sock = -1;
  r->row = 0;
  r->time = 0;
  return r;
}

#ifndef MR_NO_NETWORK
mrocket_t *minirocket_connect(const char *hostname, int port) {
  mrocket_t *r = mrocket_init();
  r->buf = ringbuf_create(32);
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

static void minirocket_socket_send_set_row(mrocket_t *rocket, unsigned long row)
{
  if(rocket->sock <= 0) {
    return;
  }
  const char head[5] = {CMD_SET_ROW, 
			(row>>24)&0xff,  (row>>16)&0xff, (row>>8)&0xff, row&0xff};

  if (send(rocket->sock, head, 5, 0) == -1){
    perror("minirocket_socket_send_set_row");
  }

}

static bool _minirocket_socket_send_get_track(mrocket_t *rocket, const char *name) 
{
  if(rocket->sock <= 0) {
    return;
  }
  unsigned int len = strlen(name);
  const char head[5] = {CMD_GET_TRACK, 
			(len>>24)&0xff,  (len>>16)&0xff, (len>>8)&0xff, len&0xff};

  if (send(rocket->sock, head, 5, 0) == -1){
    perror("minirocket_socket_send_get_track: send cmd");
    return false;
  }

  if (send(rocket->sock, name, len, 0) == -1){
    perror("minirocket_socket_send_get_track: send name");
    return false;
  }

  return true;
}


static int _minirocket_socket_ringbuf_read(mrocket_t *rocket) {
  struct timeval to = {0, 0};
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(rocket->sock, &fds);

  if(select((int)rocket->sock + 1, &fds, NULL, NULL, &to) <= 0) {
    return ringbuf_size(rocket->buf);
  }

  int numbytes;
  unsigned int max = rocket->buf->max - rocket->buf->write;
  
  if ((numbytes=recv(rocket->sock, (char *)rocket->buf->buf+rocket->buf->write, max, 0x0)) == -1) {
    if(errno != EAGAIN && errno != 0) {
      perror("recv"); fflush(stderr);
      return -1;
    }
  }
  else if(numbytes > 0) {
    rocket->buf->write = (rocket->buf->write + numbytes) % rocket->buf->max;
  }

  return ringbuf_size(rocket->buf);
}

#endif // #ifndef MR_NO_NETWORK

mrocket_t *mrocket_read_from_file(const char *filename) 
{
  FILE *fd = fopen(filename, "r");
  if(fd == NULL) {
    perror("fopen");
    return NULL;
  }
  mrocket_t *rocket = mrocket_init();
  char buf[256];
  mrocket_track_t *track = NULL;
  while((fgets(buf, 256, fd) != NULL)) {
    if(buf[0] == '#') { // track name
      track = malloc(sizeof(mrocket_track_t));
      track->numkeys = 0;
      track->id = rocket->numtracks;
      track->name = strdup(buf+1);
      // fprintf(stderr, "TRACK %d name %s\n", track->id, track->name); fflush(stderr);
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
      key->interp = (unsigned char)atoi(vb);

      // fprintf(stderr, "%d %.4f %d\n", key->row, key->value, key->interp); fflush(stderr);
    }
  }
  return rocket;
}

bool mrocket_write_to_file(mrocket_t *rocket, const char *filename) 
{
  FILE *fd = fopen(filename, "w");
  if(fd == NULL) {
    perror("fopen");
    return false;
  }

  for(int i=0; i < rocket->numtracks; i++) {
    mrocket_track_t *track = rocket->tracks[i];
    fprintf(fd, "#%s\n", track->name);
    for(int j=0; j < track->numkeys; j++) {
      mrocket_key_t *key = &track->keys[j];
      fprintf(fd, "%d %.4f %d\n", key->row, key->value, key->interp);
    }
  }

  fclose(fd);
  return true;
}


static int _mrocket_track_sort_compare(const void *a, const void *b) {
  mrocket_key_t *k1 = (mrocket_key_t *)a;
  mrocket_key_t *k2 = (mrocket_key_t *)b;
  return k2->row - k1->row;
}

void _minirocket_sort_keys(mrocket_track_t *track) {
  qsort(track->keys, track->numkeys, sizeof(mrocket_key_t), _mrocket_track_sort_compare);
}

void minirocket_delete_key(mrocket_t *rocket, 
			   unsigned int track_no, 
			   unsigned int row) {

  assert(track_no <= rocket->numtracks);
  mrocket_track_t *track = rocket->tracks[track_no];
  for(int i=0; i < track->numkeys; i++) {
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
  fprintf(stderr, "FAILED delete key: %d %d  numkeys:%d~\n", track_no, row, track->numkeys); fflush(stderr);
  assert(false);
}

void minirocket_set_key(mrocket_t *rocket, 
			unsigned int track_no, 
			unsigned int row, 
			float value, 
			unsigned char interp) 
{
  assert(track_no <= rocket->numtracks);
  mrocket_track_t *track = rocket->tracks[track_no];

  for(int i=0; i < track->numkeys; i++) {
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
  _minirocket_sort_keys(track);
}

mrocket_track_t *minirocket_create_track(mrocket_t *rocket, const char *name) 
{
  for(int i=0; i < rocket->numtracks; i++) {
    if(strcmp(name, rocket->tracks[i]->name) == 0) {
      return rocket->tracks[i];
    }
  }

#ifndef MR_NO_NETWORK
  if(!_minirocket_socket_send_get_track(rocket, name)) {
    return NULL;
  }
#endif

  mrocket_track_t *track = malloc(sizeof(mrocket_track_t));
  track->name = strdup(name);
  track->numkeys = 0;
  track->id = rocket->numtracks;
  rocket->tracks[rocket->numtracks++] = track;
  return track;
}


bool minirocket_tick(mrocket_t *rocket, float dTime) {

  if(!rocket->paused) {
    rocket->time += dTime;

    int nrow = minirocket_time2row(rocket, rocket->time);
    if(nrow != rocket->row) {
      rocket->row = nrow;
#ifndef MR_NO_NETWORK
      minirocket_socket_send_set_row(rocket, rocket->row);
#endif
      fprintf(stderr, "row: %d\n", rocket->row); fflush(stderr);
    }
  } else {
    rocket->time = minirocket_row2time(rocket, rocket->row);
  }

  if(rocket->sock <= 0) {
    return true;
  }


#ifndef MR_NO_NETWORK
  int r = _minirocket_socket_ringbuf_read(rocket);
  if(r == -1) {
    return false;
  }
  ringbuf_t *buf = rocket->buf;
    
  if(rocket->handshake > 0) {
    int skip = r > rocket->handshake ? rocket->handshake : r;
    rocket->handshake -= skip; // must become 0 once
    //fprintf(stderr, "Handshake %d\n", rocket->handshake); fflush(stderr);
    buf->write -= skip; // safe? we'll see
  }
  else if(rocket->handshake == 0) {
    rocket->handshake = -1;
    fprintf(stderr, "Handshake done~\n"); fflush(stderr);
    //    minirocket_create_track(rocket, "mygroup:mytrack");
  }

  if(ringbuf_size(buf) > 1) {
    unsigned char peek = ringbuf_peek(buf);
    if(peek == CMD_PAUSE) {
      ringbuf_skip(buf, 1);
      rocket->paused = ringbuf_read_byte(buf) == 1;
      fprintf(stderr, "Pause: %s~\n", rocket->paused?"Y":"N"); fflush(stderr);
    }
    else if(peek == CMD_SET_ROW) { 
      if(ringbuf_size(buf) > sizeof(long)) {
	ringbuf_skip(buf, 1);
	rocket->row = ringbuf_read_long(buf);
	//fprintf(stderr, "Row: %ld~\n", rocket->row); fflush(stderr);
      }
    }
    else if(peek == CMD_SET_KEY) {
      if(ringbuf_size(buf) > sizeof(long)*3+1) {
	ringbuf_skip(buf, 1);
	unsigned long track = ringbuf_read_long(buf);
	unsigned long row = ringbuf_read_long(buf);
	float value = ringbuf_read_float(buf);
	unsigned char interp = ringbuf_read_byte(buf);
	minirocket_set_key(rocket, track, row, value, interp);
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
      fprintf(stderr, "saving to file demo.rkt!\n"); fflush(stderr);
      mrocket_write_to_file(rocket, "demo.rkt");
      fprintf(stderr, "saved to file!\n"); fflush(stderr);
    }
    else {
      fprintf(stderr, "protocol error: %d\n", peek); fflush(stderr);
      ringbuf_skip(buf, 1);
    }
  }
#endif

  return true;
}

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

  while(1) {
    gettimeofday(&t1, NULL);

    float current_time = timedifference_msec(t0, t1);

    minirocket_tick(rocket, current_time - prev_time);

    prev_time = current_time;
  }

  return 0;
}
