/** ***************************************************************************
* @file
* @author Mike Sharkey <mike@pikeaero.com>.
* @copyright © 2005-2013 by Pike Aerospace Research Corporation
* @copyright © 2014-2015 by Mike Sharkey
* @details This file is part of CARIBOU RTOS
* CARIBOU RTOS is free software: you can redistribute it and/or modify
* it under the terms of the Beerware License Version 43.
* "THE BEER-WARE LICENSE" (Revision 43):
* Mike Sharkey <mike@pikeaero.com> wrote this file. 
* As long as you retain this notice you can do whatever you want with
* this stuff. If we meet some day, and you think this stuff is 
* worth it, you can buy me a beer in return ~ Mike Sharkey
******************************************************************************/
#include <caribou.h>
#include <caribou/kernel/timer.h>
#include <caribou/kernel/swi.h>
#include <caribou/lib/queue.h>
#include <caribou/lib/mutex.h>
#include <caribou/dev/gpio.h>

#define THREAD_STACK_SIZE	(512)
#define THREAD_PRIORITY     (1)
#define QUEUE_DEPTH			(16)
#define QUEUE_TIMEOUT_MS    (1000)
#define	MESSAGE_SZ			(32)

typedef enum
{
    sQueueTest,
    sEND,                                   /* End of test regime */
} test_seq_t;

typedef struct
{
    char stack_thread1[THREAD_STACK_SIZE];
    char stack_thread2[THREAD_STACK_SIZE];
    char board_stack[THREAD_STACK_SIZE];
    caribou_mutex_t     mutex_a;
    caribou_queue_t     queue_a;
    void*               queue_msgs[QUEUE_DEPTH];
    test_seq_t          seq;                /* Test sequence */
} test_t;

test_t test;

/**
 * @brief Implement this callback if you which to trap heap allocation failures.
 * @param size The size of the allocation which triggered the failure.
 */
void notify_heap_alloc_failed(size_t size)
{
	for(;;);
}

void wakeup_timer_callback(caribou_thread_t* thread, struct _caribou_timer_t* timer, void* arg)
{
    caribou_thread_wakeup(thread);
}

__attribute__((weak)) void board_thread(void* arg)
{
    for(;;)
    {
        caribou_thread_yield();
    }
}

/*** Test Regimes */

static bool enqueue_test()
{
    bool passed=true;
    for( int n=0; passed && n < QUEUE_DEPTH; n++ )
    {
        char* message_a = (char*)malloc(MESSAGE_SZ);
        passed = ( message_a != NULL );
        if ( passed )
        {
            /** Populate the message, and post the message onto the queue... */
            memset(message_a,'A',MESSAGE_SZ); 
            passed = caribou_queue_post(&test.queue_a,message_a,from_ms(QUEUE_TIMEOUT_MS));
        }
    }
    return passed;
}

static bool dequeue_test()
{
    bool passed = true;
    
    static char message_b[MESSAGE_SZ];
    memset(message_b,'A',MESSAGE_SZ);

    for( int n=0; passed && n < QUEUE_DEPTH; n++ )
    {
		char* message_a;
		passed = caribou_queue_take_first(&test.queue_a,&message_a,from_ms(QUEUE_TIMEOUT_MS));
        if ( passed )
        {
            passed = (message_a != NULL);
            if ( passed )
            {
                passed = (memcmp(message_a,message_b,MESSAGE_SZ) == 0);
                free(message_a);
            }
        }
	}
    return passed;
}

static bool status(const char* fn, int seq, bool passed)
{
	if ( !passed )
		printf( "%s: %d %s\n", fn, seq, passed ? "passed" : "failed" );
    //fflush(stdout);
    return passed;
}

/**
 * @brief The second thread to participate in the test regime.
 * @param arg Optional data pointer passed at the creation of the thread.
 */
void test1_thread(void* arg)
{
    bool passed = true;
	for(;;)
	{chip_gpio_toggle(GPIOA, CARIBOU_GPIO_PIN_5);
        switch(test.seq)
        {
			
            case sQueueTest:
                passed = enqueue_test();
                break;
        }
        status( "test1_thread", (int)test.seq, passed );
    }
}

/**
 * @brief The second thread to participate in the test regime.
 * @param arg Optional data pointer passed at the creation of the thread.
 */
void test2_thread(void* arg)
{
    bool passed = true;
	for(;;)
	{
        switch(test.seq)
        {
            case sQueueTest:
                passed = dequeue_test();
                break;
        }
        status( "test2_thread", (int)test.seq, passed );
	}
}

void print_summary()
{
	RCC_ClocksTypeDef SYS_Clocks;
	RCC_GetClocksFreq(&SYS_Clocks);

	printf("  HCLK: %dMHz\n",SYS_Clocks.HCLK_Frequency/1000000);
	printf("SYSCLK: %dMHz\n",SYS_Clocks.SYSCLK_Frequency/1000000);
#if 0
	printf(" PCLK1: %dMHz\n",SYS_Clocks.PCLK1_Frequency/1000000);
	printf(" PCLK2: %dMHz\n",SYS_Clocks.PCLK2_Frequency/1000000);
#endif
}

int main(int argc,char* argv[])
{
    int rc;

    /** caribou_init() must first be called before any other CARIBOU function calls */
	caribou_init(0);
	print_summary();

    /** Initialize a mutex object */
	caribou_mutex_init(&test.mutex_a,0);

    /** Initialize a queue of the specified depth */
	caribou_queue_init(&test.queue_a,QUEUE_DEPTH,&test.queue_msgs);

    /** Allocate and start up the enqueue and dequeue threads... */
	caribou_thread_create("test1_thread",test1_thread,NULL,NULL,test.stack_thread1,THREAD_STACK_SIZE,THREAD_PRIORITY);
	caribou_thread_create("test2_thread",test2_thread,NULL,NULL,test.stack_thread2,THREAD_STACK_SIZE,THREAD_PRIORITY);
    caribou_thread_create("board_thread",board_thread,NULL,NULL,test.board_stack,THREAD_STACK_SIZE,THREAD_PRIORITY);
    /** 
     * House keep chores are managed from the main thread, and must be called via caribou_exec()
     * Never to return 
     */
	caribou_exec();
}
