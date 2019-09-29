//
// Created by Administrator on 2019/9/29.
//

#ifndef TESTGIF_PTHREADSLEEP_H
#define TESTGIF_PTHREADSLEEP_H

#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

class PthreadSleep {
public:
    PthreadSleep();

    PthreadSleep(pthread_mutex_t* mutex, pthread_cond_t* cond);

    void msleep(unsigned int ms);

    void interrupt();

private:
    static pthread_mutex_t sleep_mutex;
    static pthread_cond_t sleep_cond;
    pthread_mutex_t* mutex;
    pthread_cond_t* cond;
};

#endif //TESTGIF_PTHREADSLEEP_H
