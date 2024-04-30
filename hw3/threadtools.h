#ifndef THREADTOOL
#define THREADTOOL
#include <setjmp.h>
#include <sys/signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>


#define THREAD_MAX 16  // maximum number of threads created
#define BUF_SIZE 512
typedef struct tcb {
    int id;  // the thread id
    jmp_buf environment;  // where the scheduler should jump to
    int arg;  // argument to the function
    int fd;  // file descriptor for the thread
    char buf[BUF_SIZE];  // buffer for the thread
    int i, x, y;  // declare the variables you wish to keep between switches
    int fifoName;
}tcb;

extern int timeslice;
extern jmp_buf sched_buf;
extern struct tcb *ready_queue[THREAD_MAX], *waiting_queue[THREAD_MAX];
/*
 * rq_size: size of the ready queue
 * rq_current: current thread in the ready queue
 * wq_size: size of the waiting queue
 */
extern int rq_size, rq_current, wq_size;
extern bool isSetUp;

/*
* base_mask: blocks both SIGTSTP and SIGALRM
* tstp_mask: blocks only SIGTSTP
* alrm_mask: blocks only SIGALRM
*/
extern sigset_t base_mask, tstp_mask, alrm_mask;
/*
 * Use this to access the running thread.
 */
#define RUNNING (ready_queue[rq_current])

void sighandler(int signo);
void scheduler();

#define thread_create(func, id, arg) {\
    (func)((id), (arg));\
}

#define thread_setup(id, arg) {\
    char fifoName[20];\
    sprintf(fifoName, "%d_%s", id, __func__);\
    mkfifo(fifoName, 0777);\
    int fifoFd = open(fifoName, O_RDONLY | O_NONBLOCK);\
    ready_queue[rq_size] = (tcb*)calloc(1, sizeof(tcb));\
    ready_queue[rq_size]->fd = fifoFd;\
    ready_queue[rq_size]->id = (id);\
    ready_queue[rq_size]->arg = (arg);\
    rq_size++;\
    int jmpval = setjmp(ready_queue[rq_size-1]->environment);\
    if(jmpval == 0){\
        return;\
    }\
}

#define thread_exit() {\
    close(RUNNING->fd);\
    char fifoName[20];\
    sprintf(fifoName, "%d_%s", RUNNING->id, __func__);\
    remove(fifoName);\
    longjmp(sched_buf, 3);\
}

#define thread_yield() {\
    int jmpval_inside = setjmp(RUNNING->environment);\
    if(jmpval_inside == 0){\
        sigprocmask(SIG_SETMASK, &alrm_mask, NULL);\
        sigprocmask(SIG_SETMASK, &tstp_mask, NULL);\
    }\
    sigprocmask(SIG_SETMASK, &base_mask, NULL);\
}

#define async_read(count) ({\
    int jmpval_rd = setjmp(RUNNING->environment);\
    if(jmpval_rd == 0){\
        longjmp(sched_buf, 2);\
    }\
    else{\
        int x = read(RUNNING->fd, RUNNING->buf, count);\
    }\
})

#endif // THREADTOOL
