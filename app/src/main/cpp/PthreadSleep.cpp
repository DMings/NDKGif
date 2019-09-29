//
// Created by Administrator on 2019/9/29.
//

#include "PthreadSleep.h"

pthread_mutex_t PthreadSleep::sleep_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t PthreadSleep::sleep_cond = PTHREAD_COND_INITIALIZER;

PthreadSleep::PthreadSleep() : mutex(NULL), cond(NULL) {

}

PthreadSleep::PthreadSleep(pthread_mutex_t *mutex, pthread_cond_t *cond) {
    this->mutex = mutex;
    this->cond = cond;
}

void PthreadSleep::msleep(unsigned int ms) {
    struct timespec deadline;
    struct timeval now;

    gettimeofday(&now, NULL);
    time_t seconds = (time_t)(ms / 1000);
    long nanoseconds = (long)((ms - seconds * 1000) * 1000000);

    deadline.tv_sec = now.tv_sec + seconds;
    deadline.tv_nsec = now.tv_usec * 1000 + nanoseconds;

    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_nsec -= 1000000000L;
        deadline.tv_sec++;
    }
    if (this->mutex != NULL && this->cond != NULL) {
        pthread_mutex_lock(this->mutex);
        pthread_cond_timedwait(this->cond, this->mutex, &deadline);
        pthread_mutex_unlock(this->mutex);
    } else {
        pthread_mutex_lock(&sleep_mutex);
        pthread_cond_timedwait(&sleep_cond, &sleep_mutex, &deadline);
        pthread_mutex_unlock(&sleep_mutex);
    }
}