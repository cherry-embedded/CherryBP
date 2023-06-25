/*
 * Copyright (c) 2022, Egahp
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "chry_blockpool.h"

static int util_fls(uint32_t word)
{
    int bit = 32;

    if (!word) {
        bit -= 1;
    }
    if (!(word & 0xffff0000)) {
        word <<= 16;
        bit -= 16;
    }
    if (!(word & 0xff000000)) {
        word <<= 8;
        bit -= 8;
    }
    if (!(word & 0xf0000000)) {
        word <<= 4;
        bit -= 4;
    }
    if (!(word & 0xc0000000)) {
        word <<= 2;
        bit -= 2;
    }
    if (!(word & 0x80000000)) {
        word <<= 1;
        bit -= 1;
    }

    return bit;
}

/*****************************************************************************
* @brief        init blockpool
* 
* @param[in]    bp          blockpool instance
* @param[in]    align       block align
* @param[in]    block_size  block size in byte
* @param[in]    pool        memory pool address
* @param[in]    size        memory size in byte
* 
* @retval int               0:Success -1:Error
*****************************************************************************/
int chry_blockpool_init(chry_blockpool_t *bp, uint32_t align, uint32_t block_size, void *pool, uint32_t size)
{
    uint32_t block_cnt;
    uint32_t align_rb_size;

    /*!< check param */
    if ((0 == block_size) || (0 == size) || (align < CHRY_BLOCKPOOL_ALIGN_4) || (align > CHRY_BLOCKPOOL_ALIGN_4096)) {
        return -1;
    }

    /*!< block size align up */
    if (block_size & ((0x1 << align) - 1)) {
        block_size = (block_size & (~((0x1 << align) - 1))) + (0x1 << align);
    }

    /*!< calculate max block cnt */
    block_cnt = size / block_size;

    while (1) {
        if (0 == block_cnt) {
            return -1;
        }

        /*!< free ringbuffer size in byte */
        uint32_t rb_size = block_cnt * sizeof(void *);

        /*!< align rb_size to power of 2 */
        align_rb_size = util_fls(rb_size);
        if (rb_size & ((1 << (align_rb_size - 1)) - 1)) {
            align_rb_size = 1 << align_rb_size;
        } else {
            align_rb_size = 1 << (align_rb_size - 1);
        }

        /*!< free space after block area */
        uint32_t free_size = size - (block_cnt * block_size);

        if (free_size >= align_rb_size) {
            break;
        } else {
            block_cnt -= 1;
        }
    }

    bp->block_size = block_size;
    bp->block_cnt = block_cnt;
    bp->pool = pool;

    /*!< init free block ringbuffer */
    if (chry_ringbuffer_init(&(bp->rb_free), (void *)((uintptr_t)pool + block_size * block_cnt), align_rb_size)) {
        return -1;
    }

    /*!< fill all free blocks to ringbuffer */
    for (uint32_t i = 0; i < block_cnt; i++) {
        chry_ringbuffer_write(&(bp->rb_free), (void *)&pool, sizeof(void *));
        pool = (void *)((uintptr_t)pool + block_size);
    }

    return 0;
}

/*****************************************************************************
* @brief        reset blockpool, free all block,
*               should be add lock in mutithread
* 
* @param[in]    bp          blockpool instance
* 
*****************************************************************************/
void chry_blockpool_reset(chry_blockpool_t *bp)
{
    void *pool = bp->pool;

    chry_ringbuffer_reset(&(bp->rb_free));

    /*!< fill all free blocks to ringbuffer */
    for (uint32_t i = 0; i < bp->block_cnt; i++) {
        chry_ringbuffer_write(&(bp->rb_free), (void *)&pool, sizeof(void *));
        pool = (void *)((uintptr_t)pool + bp->block_size);
    }
}

/*****************************************************************************
* @brief        get blockpool total size in block count
* 
* @param[in]    bp          blockpool instance
* 
* @retval uint32_t          total size in block count
*****************************************************************************/
uint32_t chry_blockpool_get_size(chry_blockpool_t *bp)
{
    return bp->block_cnt;
}

/*****************************************************************************
* @brief        get blockpool used size in block count
* 
* @param[in]    bp          blockpool instance
* 
* @retval uint32_t          used size in block count
*****************************************************************************/
uint32_t chry_blockpool_get_used(chry_blockpool_t *bp)
{
    return bp->block_cnt - chry_ringbuffer_get_used(&(bp->rb_free)) / sizeof(void *);
}

/*****************************************************************************
* @brief        get blockpool free size in block count
* 
* @param[in]    bp          blockpool instance
* 
* @retval uint32_t          free size in block count
*****************************************************************************/
uint32_t chry_blockpool_get_free(chry_blockpool_t *bp)
{
    return chry_ringbuffer_get_used(&(bp->rb_free)) / sizeof(void *);
}

/*****************************************************************************
* @brief        check if blockpool is no free block
* 
* @param[in]    bp          blockpool instance
* 
* @retval true              no free block
* @retval false             has free block
*****************************************************************************/
bool chry_blockpool_check_nomem(chry_blockpool_t *bp)
{
    return chry_ringbuffer_check_empty(&(bp->rb_free));
}

/*****************************************************************************
* @brief        alloc one block from blockpool,
*               should be add lock in mutithread,
*               in single alloc thread not need lock
* 
* @param[in]    bp          blockpool instance
* @param[in]    addr        pointer to save alloc block pointer
* 
* @retval int               0:Success -1:Nomem
*****************************************************************************/
int chry_blockpool_alloc(chry_blockpool_t *bp, void **addr)
{
    if (sizeof(void *) != chry_ringbuffer_read(&(bp->rb_free), addr, sizeof(void *))) {
        return -1;
    }

    return 0;
}

static uint32_t util_read(chry_ringbuffer_t *rb, uint32_t *out, void *data, uint32_t size)
{
    uint32_t used;
    uint32_t offset;
    uint32_t remain;

    used = rb->in - *out;
    if (size > used) {
        size = used;
    }

    offset = *out & rb->mask;

    remain = rb->mask + 1 - offset;
    remain = remain > size ? size : remain;

    memcpy(data, ((uint8_t *)(rb->pool)) + offset, remain);
    memcpy((uint8_t *)data + remain, rb->pool, size - remain);

    *out += size;

    return size;
}

/*****************************************************************************
* @brief        free one block from blockpool,
*               should be add lock in mutithread,
*               in single free thread not need lock
* 
* @param[in]    bp          blockpool instance
* @param[in]    addr        pointer to free block
* 
* @retval int               0:Success 
* @retval int               -1:Error addr
* @retval int               -2:Already free
* @retval int               -3:Error
*****************************************************************************/
int chry_blockpool_free(chry_blockpool_t *bp, void *addr)
{
    void *block;
    uintptr_t pool = (uintptr_t)(bp->pool);
    uintptr_t address = (uintptr_t)addr;
    uint32_t out = bp->rb_free.out;

    /*!< check is addr is our block */
    if (address < pool) {
        return -1;
    }

    address -= pool;

    if (address % bp->block_size) {
        return -1;
    }

    if ((address / bp->block_size) >= bp->block_cnt) {
        return -1;
    }

    /*!< check is addr is already free */
    while (sizeof(void *) == util_read(&(bp->rb_free), &out, &block, sizeof(void *))) {
        if (block == addr) {
            return -2;
        }
    }

    /*!< check is free success */
    if (sizeof(void *) != chry_ringbuffer_write(&(bp->rb_free), &addr, sizeof(void *))) {
        return -3;
    }

    return 0;
}

/*****************************************************************************
* @brief        free one block from blockpool without check,
*               should be add lock in mutithread,
*               in single free thread not need lock
* 
* @param[in]    bp          blockpool instance
* @param[in]    addr        pointer to free block
* 
*****************************************************************************/
void chry_blockpool_free_fast(chry_blockpool_t *bp, void *addr)
{
    chry_ringbuffer_write(&(bp->rb_free), &addr, sizeof(void *));
}
