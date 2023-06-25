# CherryBlockPool

[English](./README.md)

CherryBlockPool 是一个高效、易用的块内存池。

## 简介

### 1.创建并初始化一个BlockPool
```c

#define BLOCK_SIZE 128
#define BLOCK_COUNT 16
#define POOL_SIZE BLOCK_COUNT * (BLOCK_SIZE + sizeof(void*))

chry_blockpool_t bp;
__ALIGNED(8) uint8_t mempool[POOL_SIZE];

int main(void){
    
    /**
     * 第二个参数为对齐大小，由宏给出，作用是限制每个内存块的对齐
     * 第三个参数为块的大小（字节）
     * 第四个参数为内存池，内存池最好也进行对齐，与第二个参数相同
     * 第五个参数为内存池大小，限制了这块内存池最多可以切分为多少块
     * 块内存池的每个块还额外占用指针长度的字节，并且由于内部ringbuffer的长度
     * 必须为2的幂次，可能会出现因为对齐和ringbuffer长度对齐导致的内存浪费
     * 总共可用的块请以 chry_blockpool_get_size 获取到的数量为准
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

### 2.生产者消费者模型的无锁使用
CherryBlockPool的实现基于CherryRingBuffer，所以也继承了无锁功能。
如果满足blockpool只在一个线程里面进行alloc，并且只在一个线程里面free，
那么无须加锁，因为alloc和free利用的是ringbuffer的读和写。

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
     * 用于重置块内存池，多线程需要加锁，或者保证reset的时候没有线程正在
     * alloc 和free
     */
    chry_blockpool_reset(&bp);

    /**
     * 获取blockpool总大小（块）
     */
    uint32_t block_total = chry_blockpool_get_size(&bp);

    /**
     * 获取blockpool使用大小（块）
     */
    uint32_t block_used = chry_blockpool_get_used(&bp);

    /**
     * 获取blockpool空闲大小（块）
     */
    uint32_t block_free = chry_blockpool_get_free(&bp);

    /**
     * 检查blockpool是否耗尽，耗尽返回true
     */
    bool is_nomem = chry_blockpool_check_nomem(&bp);

    void *block;

    /**
     * 申请一块内存
     * 在唯一的申请线程调用无须加锁
     * 申请成功返回0，内存耗尽返回-1
     */
    chry_blockpool_alloc(&bp, &block);

    /**
     * 释放一块内存，带安全检查
     * 在唯一的释放线程调用无须加锁
     * 释放成功返回0
     * 内存地址不正确返回-1
     * 内存已经被释放过返回-2
     * 其他错误返回-3，当调用不带安全检查的free并且释放了错误的块可能会出现
     */
    chry_blockpool_free(&bp, block);

    /**
     * 释放一块内存，不带安全检查，但效率更高
     * 在唯一的释放线程调用无须加锁
     * 无返回值，此函数不检查内存地址和内存是否被释放过
     * 用户需要保证此地址正确且未被释放
     */
    chry_blockpool_free_fast(&bp, block);

```