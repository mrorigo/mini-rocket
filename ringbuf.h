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
} ringbuf_t;
#endif

#ifdef RINGBUF_IMPLEMENTATION
ringbuf_t *ringbuf_create(unsigned int max) {
  ringbuf_t *r = malloc(sizeof(ringbuf_t));
  memset(r, 0, sizeof(ringbuf_t));
  r->buf = malloc(max);
  memset(r->buf, 0, max);
  r->max = max;
  return r;
}

void ringbuf_free(ringbuf_t *r) {
  free(r->buf);
  free(r);
}

void ringbuf_reset(ringbuf_t *r) {
  r->read = r->write = 0;
}

unsigned int ringbuf_size(ringbuf_t *r) {
  if(r->write < r->read) {
    return r->max - r->read + r->write;
  } else {
    return r->write - r->read;
  }
}

void ringbuf_write_byte(ringbuf_t *r, unsigned char c) {
  r->buf[r->write] = c;
  r->write = (r->write + 1) % r->max;
  assert(r->write > r->read);
}

void ringbuf_write(ringbuf_t *r, unsigned char *buf, unsigned int size) {
  unsigned int i = 0;
  assert(size < ringbuf_size(r));
  while(size-- > 0) {
    ringbuf_write_byte(r, buf[i++]);
  }
}


unsigned char ringbuf_peek(ringbuf_t *r) {
  return r->buf[r->read];
}

void ringbuf_skip(ringbuf_t *r, unsigned int n) {
  r->read = (r->read + n) % r->max;
}

unsigned char ringbuf_read_byte(ringbuf_t *r) {
  assert(ringbuf_size(r) >= 1);
  const int pos = r->read;
  ringbuf_skip(r, 1);
  return r->buf[pos];
}

unsigned long ringbuf_read_long(ringbuf_t *r) {
  assert(ringbuf_size(r) >= sizeof(long));
  return (ringbuf_read_byte(r) << 24) |
    (ringbuf_read_byte(r) << 16) |
    (ringbuf_read_byte(r) << 8) |
    (ringbuf_read_byte(r));
}


unsigned long ringbuf_read_float(ringbuf_t *r) {
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
ringbuf_t	*ringbuf_create(unsigned int max);
void		 ringbuf_reset(ringbuf_t *r);
unsigned int	 ringbuf_size(ringbuf_t *r);
void		 ringbuf_write_byte(ringbuf_t *r, unsigned char c);
void		 ringbuf_write(ringbuf_t *r, unsigned char *buf, unsigned int size);
unsigned char	 ringbuf_peek(ringbuf_t *r);
void		 ringbuf_skip(ringbuf_t *r, unsigned int n);
unsigned char	 ringbuf_read_byte(ringbuf_t *r);
unsigned long	 ringbuf_read_long(ringbuf_t *r);
unsigned long	 ringbuf_read_float(ringbuf_t *r);
void		 ringbuf_free(ringbuf_t *r);

#endif
