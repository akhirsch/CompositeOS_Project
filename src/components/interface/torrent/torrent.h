/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef TORRENT_H
#define TORRENT_H

#include <cos_component.h>
#include <cbuf_c.h>
#include <cbuf.h>
#include <evt.h>

/* torrent descriptor */
typedef int td_t;
static const td_t td_null = 0, td_root = 1;
typedef enum {
	TOR_WRITE = 0x1,
	TOR_READ  = 0x2,
	TOR_SPLIT = 0x4,
	TOR_RW    = TOR_WRITE | TOR_READ, 
	TOR_ALL   = TOR_RW    | TOR_SPLIT /* 0 is a synonym */
} tor_flags_t;

td_t tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
void trelease(spdid_t spdid, td_t tid);
int tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len);
int tread(spdid_t spdid, td_t td, int cbid, int sz);
int twrite(spdid_t spdid, td_t td, int cbid, int sz);

static inline int
tread_pack(spdid_t spdid, td_t td, char *data, int len)
{
	cbuf_t cb;
	char *d;
	int ret;

	d = cbuf_alloc(len, &cb);
	if (!d) return -1;

	ret = tread(spdid, td, cb, len);
	memcpy(data, d, len);
	cbuf_free(d);
	
	return ret;
}

static inline int
twrite_pack(spdid_t spdid, td_t td, char *data, int len)
{
	cbuf_t cb;
	char *d;
	int ret;

	d = cbuf_alloc(len, &cb);
	if (!d) return -1;

	memcpy(d, data, len);
	ret = twrite(spdid, td, cb, len);
	cbuf_free(d);
	
	return ret;
}

/* //int trmeta(td_t td, char *key, int flen, char *value, int vlen); */
/* struct trmeta_data { */
/* 	short int value, end; /\* offsets into data *\/ */
/* 	char data[0]; */
/* }; */
/* int trmeta(td_t td, int cbid, int sz); */

/* //int twmeta(td_t td, char *key, int flen, char *value, int vlen); */
/* struct twmeta_data { */
/* 	short int value, end; /\* offsets into data *\/ */
/* 	char data[0]; */
/* }; */
/* int twmeta(td_t td, int cbid, int sz); */

#endif /* TORRENT_H */ 
