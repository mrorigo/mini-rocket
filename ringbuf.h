/**
 * Simple ringbuffer
 *
 * (C) orIgo 2021
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#ifndef __RINGBUF_TYPE_H__
#define __RINGBUF_TYPE_H__
typedef struct __ringbuf_t {
  unsigned char *buf;
  unsigned int write;
  unsigned int read;
  unsigned int max;
  int size;
} ringbuf_t;
#endif

#ifdef RINGBUF_IMPLEMENTATION
static ringbuf_t *ringbuf_create(unsigned int max) {
  ringbuf_t *r = malloc(sizeof(ringbuf_t));
  memset(r, 0, sizeof(ringbuf_t));
  r->buf = malloc(max);
  memset(r->buf, 0x41, max);
  r->max = max;
  return r;
}

static inline void ringbuf_reset(ringbuf_t *r) {
  r->read = r->write = 0;
}

static inline unsigned int ringbuf_size(ringbuf_t *r) {
  if(r->write < r->read) {
    return r->max - r->read + r->write;
  } else {
    return r->write - r->read;
  }

  void ringbuf_print(ringbuf_t *r) {
  fprintf(stderr, "\n");
  for(unsigned int i=0; i < r->max; i++) {
    fprintf(stderr, "%02x|", r->buf[i]);
  }
  fprintf(stderr, "\n");
  for(unsigned int i=0; i < r->max; i++) {
    if(r->read == r->write) {
      fprintf(stderr, "%s|", i==r->read?"RW":"  ");
    } else {
      fprintf(stderr, "%s|", i==r->read?"R^":(i==r->write?"W^":"  "));
    }
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "R: %d  W:%d  S:%d\n", r->read, r->write, r->size);
  fflush(stderr);
}

void ringbuf_free(ringbuf_t *r) {
  free(r->buf);
  free(r);
}

void ringbuf_reset(ringbuf_t *r) {
  r->read = r->write = 0;
}

unsigned int ringbuf_size(ringbuf_t *r) {
  return r->size;
>>>>>>> a700a0f7193850c0b30e256922f16c846a5f0670
}

void ringbuf_write_byte(ringbuf_t *r, unsigned char c) {
  r->buf[r->write] = c;
  r->write = (r->write + 1) % r->max;
  r->size++;
}

void ringbuf_write(ringbuf_t *r, unsigned char *buf, unsigned int size) {
  unsigned int i = 0;
  assert(size <= r->max - ringbuf_size(r));
  while(size-- > 0) {
    ringbuf_write_byte(r, buf[i++]);
  }
}

unsigned char ringbuf_peek(ringbuf_t *r) {
  return r->buf[r->read];
}

void ringbuf_skip(ringbuf_t *r, unsigned int n) {
  for(int i = 0; i < n; i++) {
    r->buf[(r->read + i) % r->max] = 0x42;
  }
  r->read = (r->read + n) % r->max;
  r->size -= n;
  assert(r->size >= 0);
}

unsigned char ringbuf_read_byte(ringbuf_t *r) {
  assert(ringbuf_size(r) >= 1);
  unsigned char ret = r->buf[r->read];
  ringbuf_skip(r, 1);
  return ret;
}

unsigned long ringbuf_read_long(ringbuf_t *r) {
  assert(ringbuf_size(r) >= sizeof(long));
  return (ringbuf_read_byte(r) << 24) |
    (ringbuf_read_byte(r) << 16) |
    (ringbuf_read_byte(r) << 8) |
    (ringbuf_read_byte(r));
}

float ringbuf_read_float(ringbuf_t *r) {
  assert(ringbuf_size(r) >= sizeof(long));
  float value = 0;
  unsigned char *v = (unsigned char *)&value;
  v[3]= ringbuf_read_byte(r);
  v[2]= ringbuf_read_byte(r);
  v[1]= ringbuf_read_byte(r);
  v[0]= ringbuf_read_byte(r);
  return value;
}

void ringbuf_read(ringbuf_t *r, unsigned char *buf, unsigned int size) {
  unsigned int i = 0;
  assert(size <= ringbuf_size(r));
  while(size-- > 0) {
    buf[i++] = ringbuf_read_byte(r);
  }
}

#else
static ringbuf_t	*ringbuf_create(unsigned int max);
static void		 ringbuf_reset(ringbuf_t *r);
static unsigned int	 ringbuf_size(ringbuf_t *r);
void		 ringbuf_write_byte(ringbuf_t *r, unsigned char c);
void		 ringbuf_write(ringbuf_t *r, unsigned char *buf, unsigned int size);
unsigned char	 ringbuf_peek(ringbuf_t *r);
void		 ringbuf_skip(ringbuf_t *r, unsigned int n);
unsigned char	 ringbuf_read_byte(ringbuf_t *r);
unsigned long	 ringbuf_read_long(ringbuf_t *r);
float	         ringbuf_read_float(ringbuf_t *r);
void		 ringbuf_free(ringbuf_t *r);
void             ringbuf_print(ringbuf_t *r);

#endif
