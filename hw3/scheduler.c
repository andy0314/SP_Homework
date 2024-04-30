#include "threadtools.h"
#include <unistd.h>
#include <stdio.h>


/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call longjmp(sched_buf, 1).
 */
void sighandler(int signo) {
    // TODO
    if(signo == SIGTSTP){
        printf("caught SIGTSTP\n");
        sigprocmask(SIG_SETMASK, &base_mask, NULL);
        signal(SIGTSTP, sighandler);
        longjmp(sched_buf, 1);
    }
    else if(signo == SIGALRM){
        printf("caught SIGALRM\n");
        alarm(timeslice);
        sigprocmask(SIG_SETMASK, &base_mask, NULL);
        signal(SIGALRM, sighandler);
        longjmp(sched_buf, 1);
    }
}

/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
    // TODO
    rq_current = 0;
    int maxfd = getdtablesize()+1;
    struct timeval time_out;

    int jmpval = setjmp(sched_buf);

    time_out.tv_sec = 0;
    time_out.tv_usec = 0;

    fd_set waiting_set;
    FD_ZERO(&waiting_set);
    for(int i = 0; i < wq_size; i++){
        FD_SET(waiting_queue[i]->fd, &waiting_set);
    }
    if(wq_size){
        //printf("sel\n");
        int ee = select(maxfd, &waiting_set, NULL, NULL, &time_out);
        //printf("caught 0 %d\n", ee);
        //if(FD_ISSET(waiting_queue[0]->fd, &waiting_set) == true){
            //printf("a\n");
        //}
    }
    for(int i = 0; i < wq_size; i++){
        if(FD_ISSET(waiting_queue[i]->fd, &waiting_set)){
            ready_queue[rq_size] = waiting_queue[i];
            rq_size++;
            waiting_queue[i] = NULL;
            //printf("caught\n");
            for(int j = i; j < wq_size - 1; j++){
                waiting_queue[j] = waiting_queue[j+1];
            }
            i--;
            wq_size--;
        }
    }
    if(jmpval == 0){
        longjmp(RUNNING->environment, 1);
    }
    else if(jmpval == 1){
        if(rq_current == rq_size - 1){
            rq_current = 0;
        }
        else{
            rq_current++;
        }
        longjmp(RUNNING->environment, 1);
    }
    else if(jmpval == 2){
        waiting_queue[wq_size] = ready_queue[rq_current];
        wq_size++;
        if(rq_current == rq_size - 1){
            rq_current = 0;
        }
        else{
            ready_queue[rq_current] = ready_queue[rq_size - 1];
        }
        rq_size--;
        if(rq_size == 0){

            FD_ZERO(&waiting_set);

            for(int i = 0; i < wq_size; i++){
                FD_SET(waiting_queue[i]->fd, &waiting_set);
            }

            int x = select(maxfd, &waiting_set, NULL, NULL, NULL);
            //printf("caught 2 %d\n", x);

            for(int i = 0; i < wq_size; i++){
                if(FD_ISSET(waiting_queue[i]->fd, &waiting_set)){
                    ready_queue[rq_size] = waiting_queue[i];
                    rq_size++;
                    waiting_queue[i] = NULL;
                    for(int j = i; j < wq_size - 1; j++){
                        waiting_queue[j] = waiting_queue[j+1];
                    }
                    i--;
                    wq_size--;
                }
            }

        }
        longjmp(RUNNING->environment, 1);
    }
    else if(jmpval == 3){
        if(rq_current == rq_size - 1){
            rq_current = 0;
        }
        else{
            ready_queue[rq_current] = ready_queue[rq_size - 1];
        }
        rq_size--;

        if(rq_size == 0 && wq_size == 0){
            return;
        }
        else if(rq_size == 0){
            FD_ZERO(&waiting_set);
            
            for(int i = 0; i < wq_size; i++){
                FD_SET(waiting_queue[i]->fd, &waiting_set);
            }

            int y = select(maxfd, &waiting_set, NULL, NULL, NULL);
            //printf("caught 3 %d\n", y);
            for(int i = 0; i < wq_size; i++){
                if(FD_ISSET(waiting_queue[i]->fd, &waiting_set)){
                    ready_queue[rq_size] = waiting_queue[i];
                    rq_size++;
                    waiting_queue[i] = NULL;
                    for(int j = i; j < wq_size - 1; j++){
                        waiting_queue[j] = waiting_queue[j+1];
                    }
                    i--;
                    wq_size--;
                }
            }

        }
        longjmp(RUNNING->environment, 1);
    }
}