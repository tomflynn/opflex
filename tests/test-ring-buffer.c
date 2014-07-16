#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <unistd.h>

#include "ovs-thread.h"
#include "peovs_ring_buffer.h"
#include "dbug.h"
#include "vlog.h"

/* protos */
void *push_on(void *arg);
void *pop_off(void *arg);

/* private */
static int push_counter = 0;
static int push_sum = 0;
static int pop_counter = 0;
static int pop_sum = 0;
static int push[PE_RING_BUFFER_LENGTH+1] = { 0 };
static struct ovs_mutex pop_lock;

/* the following two values are determined along with
 * PE_RING_BUFFER_LENGTH such that
 * PE_TEST_POP_THREAD_COUNT*PE_TEST_MAX_POP_COUNT=PE_RING_BUFFER_LENGTH,
 * which leaves 1 final entry to pop.
 */
#define PE_TEST_POP_THREAD_COUNT 20
#define PE_TEST_MAX_POP_COUNT 50

VLOG_DEFINE_THIS_MODULE(test_peovs_ring_buffer);

static void push_pop_buffer(void **state) {
    (void) state;
    pthread_attr_t attr;
    pthread_t push_thread;
    pthread_t pop_thread[PE_TEST_POP_THREAD_COUNT];
    pthread_t last_pop_thread;
    int counter = 0;
    int arg = 0;

    VLOG_ENTER(__func__);
    VLOG_DBG("file %s/func %s/line %i\n",
                        __FILE__, __func__,__LINE__);

    ring_buffer_init();
    /* producer */
    (void) pthread_attr_init(&attr); //non portable
    (void) pthread_attr_setdetachstate(&attr,
                                       PTHREAD_CREATE_DETACHED);
    //TODO: check return codes`
    pthread_create(&push_thread, &attr, push_on, NULL);

    /* make sure the producer fills the buffer */
    while(push_counter != (PE_RING_BUFFER_LENGTH-1))
        sleep(1);

    /* consumer */
    ovs_mutex_init(&pop_lock);
    for(counter=0;counter<(PE_TEST_POP_THREAD_COUNT);counter++) {
        //TODO:check return codes
        pthread_create(&pop_thread[counter], NULL,
                        pop_off, (void *) &arg);
        VLOG_DBG("created pop thread %i",counter);
    }

    for(counter=0;counter<(PE_TEST_POP_THREAD_COUNT);counter++) {
        xpthread_join(pop_thread[counter], (void **) NULL);
        VLOG_DBG("joined pop thread %i",counter);
    }

    /* at this point, there should be 1 remaing entry to pop */
    
    arg = 1;
    //TODO:check return codes
    pthread_create(&last_pop_thread, NULL, pop_off, (void *) &arg);
    VLOG_DBG("created last pop thread");
    xpthread_join(last_pop_thread, (void **) NULL);
    VLOG_DBG("joined last pop thread");

    VLOG_DBG("results: push_count %i, push_sum %i",
                            push_counter,push_sum);
    VLOG_DBG("         pop_count %i,  pop_sum %i",
                            pop_counter,pop_sum);


    ring_buffer_destroy();

    assert_int_equal(pop_sum,push_sum);

    VLOG_LEAVE(__func__);
}

void *pop_off(void *arg) {
    void *pop = NULL;
    int count_pops = 0;
    int sentinel;

    VLOG_ENTER(__func__);

    sentinel = *((int *)arg);

    if(sentinel != 1) {
        for(count_pops = 0; count_pops < PE_TEST_MAX_POP_COUNT; count_pops++) {
            pop = ring_buffer_pop();
            ovs_mutex_lock(&pop_lock);
            pop_counter++;
            pop_sum += *((int *) pop);
            ovs_mutex_unlock(&pop_lock);
        }
    } else {
        pop = ring_buffer_pop();
        ovs_mutex_lock(&pop_lock);
        pop_counter++;
        pop_sum += *((int *) pop);
        ovs_mutex_unlock(&pop_lock);
    }
        
    VLOG_DBG("pop_off tid %p, pop_sum/push_sum %i/%i",
                        pthread_self(),pop_sum,push_sum);
    VLOG_LEAVE(__func__);

    pthread_exit((void *) NULL);
}

void *push_on(void *arg) {
    int counter = 0;
    (void) arg;

    VLOG_ENTER(__func__);

    for(counter=0;counter<(PE_RING_BUFFER_LENGTH+1);counter++) {
        push[counter] = counter + 1;
        ring_buffer_push((void *) &push[counter]);
        push_counter++;
        push_sum += push[counter];
    }

    VLOG_LEAVE(__func__);

    return((void *) NULL);
}

int main(void) {
    const UnitTest tests[] = {
    //    unit_test(fill_buffer),
    //    unit_test(consume_buffer),
        unit_test(push_pop_buffer),
    };
    putenv("CMOCKA_TEST_ABORT=1");
    return run_tests(tests);
}
