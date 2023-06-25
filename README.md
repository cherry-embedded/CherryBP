# CherryBlockPool

[中文版](./README_zh.md)

CherryBlockPool is an efficient and easy-to-use block pool.

## Brief


### 1.Create and initialize a BlockPool
```c

#define BLOCK_SIZE 128
#define BLOCK_COUNT 16
#define POOL_SIZE BLOCK_COUNT * (BLOCK_SIZE + sizeof(void*))

chry_blockpool_t bp;
__ALIGNED(8) uint8_t mempool[POOL_SIZE];

int main(void){
    
    /**
     * The second parameter is the alignment size, given by the macro, 
     * which serves to limit the alignment of each memory block, 
     * the third parameter is the block size (bytes), 
     * the fourth parameter is the memory pool, 
     * which is preferably also aligned, the same as the second parameter, 
     * the fifth parameter is the memory pool size, 
     * which limits the maximum number of blocks this memory pool can be sliced into, 
     * each block of the block memory pool also occupies additional 
     * bytes of the pointer length, and Since the length of 
     * the internal ringbuffer must be a power of 2, 
     * there may be memory wastage due to alignment and ringbuffer length alignment, 
     * and the total available blocks should be based on the number 
     * obtained by chry_blockpool_get_size
     */
    if(0 == chry_blockpool_init(&bp, CHRY_BLOCKPOOL_ALIGN_8, BLOCK_SIZE, mempool, POOL_SIZE)){
        printf("success\r\n");
        printf("total block [%d]\r\n", chry_blockpool_get_size(&bp));
    }else{
        printf("error\r\n");
    }

    return 0;
}
```

### 2.Lock-free use of the producer-consumer model
The CherryBlockPool implementation is based on CherryRingBuffer, so it also inherits the lock-free feature.
If it is satisfied that the blockpool only allocates in one thread and frees in one thread, then no lock is required.
Then there is no need to add locks, because alloc and free use the ringbuffer for reading and writing.

```c
QueueHandle_t queue;

void thread_producer(void* param){
    uint8_t time = 100;
    uint32_t cnt = 0;

    printf("Producer task enter \r\n");
    vTaskDelay(1000);
    printf("Producer task start \r\n");

    void *block_ptr = (void *)0x12345678;

    while (1) {
        time = rand() % 64;

        if (chry_blockpool_alloc(&block_pool, &block_ptr)) {
            /*!< alloc faild, no memory yet */
            // printf("alloc block pool faild\r\n");
            // printf("total [%4ld] free [%4ld]\r\n", chry_blockpool_get_size(&block_pool), chry_blockpool_get_free(&block_pool));
        } else {
            printf("total [%4ld] free [%4ld] block[%08lx]\r\n", chry_blockpool_get_size(&block_pool), chry_blockpool_get_free(&block_pool), (uintptr_t)block_ptr);

            cnt++;
            sprintf((char *)block_ptr, "this is [%ld] info in block [%08lx]", cnt, (uintptr_t)block_ptr);
            printf("Producer send >> %s\r\n", (char *)block_ptr);
            if (pdTRUE != xQueueSend(queue, (void *)&block_ptr, 1000)) {
                printf("queue send faild\r\n");
            }
        }

        vTaskDelay(time);
    }
}

void thread_consumer(void* param){
    uint8_t time = 200;

    printf("Consumer task enter \r\n");
    vTaskDelay(1000);
    printf("Consumer task start \r\n");

    void *block_ptr;

    while (1) {
        time += 20;
        if (pdTRUE != xQueueReceive(queue, &block_ptr, portMAX_DELAY)) {
            printf("queue recv faild\r\n");
        } else {
            printf("Consumer recv block [%08lx] << %s\r\n", (uintptr_t)block_ptr, (char *)block_ptr);
#if 0
            int ret = chry_blockpool_free(&block_pool, block_ptr);
            if (ret) {
                printf("free block heap faild, ret = %d\r\n", ret);
            }
#else
            chry_blockpool_free_fast(&block_pool, block_ptr);
#endif
        }

        vTaskDelay(time);
    }
}

```

### 3. API简介

```c

    /**
     * Used to reset block memory pools, 
     * where multiple threads need to add locks or ensure that 
     * no threads are allocating and freeing at the time of reset
     */
    chry_blockpool_reset(&bp);

    /**
     * Get the total blockpool size (blocks)
     */
    uint32_t block_total = chry_blockpool_get_size(&bp);

    /**
     *Get the used blockpool size (blocks)
     */
    uint32_t block_used = chry_blockpool_get_used(&bp);

    /**
     * Get the free blockpool size (blocks)
     */
    uint32_t block_free = chry_blockpool_get_free(&bp);

    /**
     * Check if blockpool is depleted, return true if depleted
     */
    bool is_nomem = chry_blockpool_check_nomem(&bp);

    void *block;

    /**
     * alloc a block of memory
     * No lock is required on the only alloc thread call
     * Success returns 0, memory exhaustion returns -1
     */
    chry_blockpool_alloc(&bp, &block);

    /**
     * Free a block of memory with safety check
     * No locking on the only released thread call
     * Success returns 0
     * Incorrect memory address returns -1
     * Memory has been freed to return -2
     * Other errors return -3, which may occur when free is called without a safety check 
     * and the wrong block is freed
     */
    chry_blockpool_free(&bp, block);

    /**
     * Free a block of memory without safety checks, but more efficient
     * No locking on the only released thread call
     * No return value, this function does not check the memory address 
     * and whether the memory has been freed
     * The user needs to ensure that this address is correct 
     * and has not been released
     */
    chry_blockpool_free_fast(&bp, block);

```