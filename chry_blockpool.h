/*
 * Copyright (c) 2022, Egahp
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CHRY_BLOCKPOOL_H
#define CHRY_BLOCKPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "chry_ringbuffer.h"

#define CHRY_BLOCKPOOL_ALIGN_4    0x02
#define CHRY_BLOCKPOOL_ALIGN_8    0x03
#define CHRY_BLOCKPOOL_ALIGN_16   0x04
#define CHRY_BLOCKPOOL_ALIGN_32   0x05
#define CHRY_BLOCKPOOL_ALIGN_64   0x06
#define CHRY_BLOCKPOOL_ALIGN_128  0x07
#define CHRY_BLOCKPOOL_ALIGN_256  0x08
#define CHRY_BLOCKPOOL_ALIGN_512  0x09
#define CHRY_BLOCKPOOL_ALIGN_1024 0x0A
#define CHRY_BLOCKPOOL_ALIGN_2048 0x0B
#define CHRY_BLOCKPOOL_ALIGN_4096 0x0C

typedef struct {
    uint32_t block_cnt;        /*!< Define the block count.           */
    uint32_t block_size;       /*!< Define the aligned block size.    */
    void *pool;                /*!< Define the memory pointer.        */
    chry_ringbuffer_t rb_free; /*!< Define the free block ringbuffer. */
} chry_blockpool_t;

extern int chry_blockpool_init(chry_blockpool_t *bp, uint32_t align, uint32_t block_size, void *pool, uint32_t size);
extern void chry_blockpool_reset(chry_blockpool_t *bp);

extern uint32_t chry_blockpool_get_size(chry_blockpool_t *bp);
extern uint32_t chry_blockpool_get_used(chry_blockpool_t *bp);
extern uint32_t chry_blockpool_get_free(chry_blockpool_t *bp);

extern bool chry_blockpool_check_nomem(chry_blockpool_t *bp);

extern int chry_blockpool_alloc(chry_blockpool_t *bp, void **addr);
extern int chry_blockpool_free(chry_blockpool_t *bp, void *addr);
extern void chry_blockpool_free_fast(chry_blockpool_t *bp, void *addr);

#ifdef __cplusplus
}
#endif

#endif