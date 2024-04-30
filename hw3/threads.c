#include "threadtools.h"

bool isSetUp = false;

void fibonacci(int id, int arg) {
    thread_setup(id, arg);

    for (RUNNING->i = 1; ; RUNNING->i++) {
        if (RUNNING->i <= 2)
            RUNNING->x = RUNNING->y = 1;
        else {
            /* We don't need to save tmp, so it's safe to declare it here. */
            int tmp = RUNNING->y;  
            RUNNING->y = RUNNING->x + RUNNING->y;
            RUNNING->x = tmp;
        }
        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);

        if (RUNNING->i == RUNNING->arg) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}

void collatz(int id, int arg) {
    // TODO
    thread_setup(id, arg);
    for(RUNNING->i = 1; ; RUNNING->i++){
        if(RUNNING->i == 1){
            RUNNING->x = RUNNING->arg;
        }
        if(RUNNING->x % 2 == 0){
            RUNNING->x = RUNNING->x/2;
        }
        else{
            RUNNING->x = RUNNING->x*3 + 1;
        }
        printf("%d %d\n", RUNNING->id, RUNNING->x);
        sleep(1);

        if(RUNNING->x == 1){
            thread_exit();
        }
        else{
            thread_yield();
        }
    }
}

void max_subarray(int id, int arg) {
    // TODO
    thread_setup(id, arg);
    for(RUNNING->i = 1; ; RUNNING->i++){
        if(RUNNING->i == 1){
            RUNNING->x = 0;
            RUNNING->y = 0;
        }
        async_read(5);
        int num = atoi(RUNNING->buf);

        int a = RUNNING->x + num;
        int b = num;
        if(a > b){
            RUNNING->x = a;
        }
        else{
            RUNNING->x = b;
        }

        if(RUNNING->x > RUNNING->y){
            RUNNING->y = RUNNING->x;
        }

        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);

        if(RUNNING->i >= RUNNING->arg){
            thread_exit();
        }
        else{
            thread_yield();
        }
    }
}