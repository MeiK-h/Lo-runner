/**
 * Loco program runner core
 * Copyright (C) 2011  Lodevil(Du Jiong)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "limit.h"
#include <sys/resource.h>
#include <sys/time.h>

const char *last_limit_err;

/* 为进程设置资源限制，只用作防范，限制放宽 */
int setResLimit(struct Runobj *runobj) {
#define RAISE_EXIT(err) {last_limit_err = err;return -1;}
    /*
    参照：https://linux.die.net/man/2/setrlimit， https://linux.die.net/man/2/getrlimit
    结构体定义如下
    struct rlimit {
        rlim_t rlim_cur;  // Soft limit
        rlim_t rlim_max;  // Hard limit (ceiling for rlim_cur)
    };
    函数定义如下
    int getrlimit(int resource, struct rlimit *rlim);
    int setrlimit(int resource, const struct rlimit *rlim);
    int prlimit(pid_t pid, int resource, const struct rlimit *new_limit, struct rlimit *old_limit);
    */
    struct rlimit rl;

    /* 设置CPU运行时间限制 */
    rl.rlim_cur = runobj->time_limit / 1000 + 1;
    if (runobj->time_limit % 1000 > 800) {
        rl.rlim_cur += 1;
    }
    rl.rlim_max = rl.rlim_cur + 1;
    if (setrlimit(RLIMIT_CPU, &rl))
        RAISE_EXIT("set RLIMIT_CPU failure");

    /* 设置数据段与虚拟内存大小限制 */
    rl.rlim_cur = runobj->memory_limit * 1024;
    rl.rlim_max = rl.rlim_cur + 1024;
    if (setrlimit(RLIMIT_DATA, &rl))
        RAISE_EXIT("set RLIMIT_DATA failure");

    rl.rlim_cur = runobj->memory_limit * 1024 * 2;
    rl.rlim_max = rl.rlim_cur + 1024;
    if (setrlimit(RLIMIT_AS, &rl))
        RAISE_EXIT("set RLIMIT_AS failure");

    /* 设置进程堆栈的最大空间 */
    rl.rlim_cur = 256 * 1024 * 1024;
    rl.rlim_max = rl.rlim_cur + 1024;
    if (setrlimit(RLIMIT_STACK, &rl))
        RAISE_EXIT("set RLIMIT_STACK failure");

    /*
    参照：https://linux.die.net/man/2/setitimer https://linux.die.net/man/2/getitimer
    结构体定义如下
    struct itimerval {
        struct timeval it_interval; // next value
        struct timeval it_value;    // current value
    };

    struct timeval {
        time_t      tv_sec;         // seconds
        suseconds_t tv_usec;        // microseconds 1秒 = 1000000微秒
    };
    函数定义如下
    int getitimer(int which, struct itimerval *curr_value);
    int setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value);
    */
    struct itimerval p_realt;
    /* 设置实际运行时间限制，可以防止sleep等方式卡评测 */
    p_realt.it_interval.tv_sec = runobj->time_limit / 1000 + 2;
    p_realt.it_interval.tv_usec = 0;
    p_realt.it_value = p_realt.it_interval;
    if (setitimer(ITIMER_REAL, &p_realt, (struct itimerval *) 0) == -1)
        RAISE_EXIT("set ITIMER_REAL failure");

    return 0;
}
