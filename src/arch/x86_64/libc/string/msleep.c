//#include <time.h>
#include <stdlib.h>

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long sec)
{
    //struct timespec ts;
    //int res;

    if (sec < 0)
    {
        //errno = EINVAL;
        return -1;
    }

    size_t msec = sec * 1000000;
    //size_t nsec = (msec % 1000) * 1000000;

    while (msec > 0)
    {
        msec--;
    }

    return msec;
}
