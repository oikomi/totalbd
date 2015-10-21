/*
 * log.c
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

#include<time.h>
#include<string.h>
#include<stdio.h>
#include<assert.h>

int get_time(char *out, int fmt)
{
    if(out == NULL)
        return -1;

    time_t t;
    struct tm *tp;
    t = time(NULL);

    tp = localtime(&t);
    if(fmt == 0)
        sprintf(out, "%2.2d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", tp->tm_year+1900, tp->tm_mon+1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    else if(fmt == 1)
        sprintf(out, "%2.2d-%2.2d-%2.2d", tp->tm_year+1900, tp->tm_mon+1, tp->tm_mday);
    else if(fmt == 2)
        sprintf(out, "%2.2d:%2.2d:%2.2d", tp->tm_hour, tp->tm_min, tp->tm_sec);
    return 0;
}
