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

#include "run.h"
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <fcntl.h>
#include "access.h"
#include "limit.h"

const char *last_run_err;
#define RAISE_RUN(err) {last_run_err = err;return -1;}

/* 监控系统调用运行子进程 */
int traceLoop(struct Runobj *runobj, struct Result *rst, pid_t pid) {
    int status, incall = 0;
    struct rusage ru;
    struct user_regs_struct regs;

    while (1) {
        if (wait4(pid, &status, WSTOPPED, &ru) == -1)
            RAISE_RUN("wait4 [WSTOPPED] failure");

        /* 检查是否停止 */
        if (WIFEXITED(status))
            break;
        else if (WSTOPSIG(status) != SIGTRAP) {
            /* 非内核产生的调停 */
            ptrace(PTRACE_KILL, pid, NULL, NULL);
            waitpid(pid, NULL, 0);

            rst->time_used = ru.ru_utime.tv_sec * 1000
                             + ru.ru_utime.tv_usec / 1000
                             + ru.ru_stime.tv_sec * 1000
                             + ru.ru_stime.tv_usec / 1000;
            rst->memory_used = ru.ru_maxrss;

            switch (WSTOPSIG(status)) {
                case SIGSEGV:
                    if (rst->memory_used > runobj->memory_limit)
                        rst->judge_result = MLE;
                    else
                        rst->judge_result = RE;
                    break;
                case SIGALRM:
                case SIGXCPU:
                    rst->judge_result = TLE;
                    break;
                default:
                    rst->judge_result = RE;
                    break;
            }

            rst->re_signum = WSTOPSIG(status);
            return 0;
        }

        /* 复制跟踪器信息 */
        if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1)
            RAISE_RUN("PTRACE_GETREGS failure");

        if (incall) {
            int ret = checkAccess(runobj, pid, &regs);
            if (ret != ACCESS_OK) {
                ptrace(PTRACE_KILL, pid, NULL, NULL);
                waitpid(pid, NULL, 0);

                rst->time_used = ru.ru_utime.tv_sec * 1000
                        + ru.ru_utime.tv_usec / 1000
                        + ru.ru_stime.tv_sec * 1000
                        + ru.ru_stime.tv_usec / 1000;
                rst->memory_used = ru.ru_maxrss
                        * (sysconf(_SC_PAGESIZE) / 1024);

                rst->judge_result = RE;
                if (ret == ACCESS_CALL_ERR) {
                    rst->re_call = REG_SYS_CALL(&regs);
                }
                else {
                    rst->re_file = lastFileAccess();
                    rst->re_file_flag = REG_ARG_2(&regs);
                }
                return 0;
            }
            incall = 0;
        }
        else
            incall = 1;

        /* 重新启动跟踪 */
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
    }
    
    
    rst->time_used = ru.ru_utime.tv_sec * 1000
                     + ru.ru_utime.tv_usec / 1000
                     + ru.ru_stime.tv_sec * 1000
                     + ru.ru_stime.tv_usec / 1000;
    rst->memory_used = ru.ru_maxrss;


    if (rst->time_used > runobj->time_limit)
        rst->judge_result = TLE;
    else if (rst->memory_used > runobj->memory_limit)
        rst->judge_result = MLE;
    else
        rst->judge_result = AC;

    return 0;
}

/* 不监控系统调用 */
int waitExit(struct Runobj *runobj, struct Result *rst, pid_t pid) {
    int status;
    struct rusage ru;

    /* 等待子进程结束 */
    if (wait4(pid, &status, 0, &ru) == -1)
        RAISE_RUN("wait4 failure");

    /* 获得子进程的资源占用 */
    rst->time_used = ru.ru_utime.tv_sec * 1000
                     + ru.ru_utime.tv_usec / 1000
                     + ru.ru_stime.tv_sec * 1000
                     + ru.ru_stime.tv_usec / 1000;
    rst->memory_used = ru.ru_maxrss;

    /* 判断是否为异常退出 */
    if (WIFSIGNALED(status)) {
        /* 获得退出原因 */
        switch (WTERMSIG(status)) {
            case SIGSEGV:
                if (rst->memory_used > runobj->memory_limit)
                    rst->judge_result = MLE;
                else
                    rst->judge_result = RE;
                break;
            case SIGALRM:
            case SIGXCPU:
                rst->judge_result = TLE;
                break;
            default:
                rst->judge_result = RE;
                break;
        }
        rst->re_signum = WTERMSIG(status);
    }
    else {
        /* 判断是否超过限制，此处的AC并不代表正确，只是说明没有异常错误 */
        if (rst->time_used > runobj->time_limit)
            rst->judge_result = TLE;
        else if (rst->memory_used > runobj->memory_limit)
            rst->judge_result = MLE;
        else
            rst->judge_result = AC;
    }

    return 0;
}

int runit(struct Runobj *runobj, struct Result *rst) {
    pid_t pid;
    int fd_err[2];

    if (pipe2(fd_err, O_NONBLOCK))
        RAISE1("run :pipe2(fd_err) failure");

    pid = vfork();
    if (pid < 0) {
        close(fd_err[0]);
        close(fd_err[1]);
        RAISE1("run : vfork failure");
    }

    if (pid == 0) {
        close(fd_err[0]);

#define RAISE_EXIT(err) {\
            int r = write(fd_err[1],err,strlen(err));\
            _exit(r);\
        }

        /* 重定向输入输出和错误流 */
        if (runobj->fd_in != -1)
            if (dup2(runobj->fd_in, 0) == -1)
                RAISE_EXIT("dup2 stdin failure!")

        if (runobj->fd_out != -1)
            if (dup2(runobj->fd_out, 1) == -1)
                RAISE_EXIT("dup2 stdout failure")

        if (runobj->fd_err != -1)
            if (dup2(runobj->fd_err, 2) == -1)
                RAISE_EXIT("dup2 stderr failure")

        /* 为进程设置限制 */
        if (setResLimit(runobj) == -1)
            RAISE_EXIT(last_limit_err)

        /* 修改运行用户(如果提供了此参数的话)，防止恶意代码或者自行修改限制 */
        if (runobj->runner != -1)
            if (setuid(runobj->runner))
                RAISE_EXIT("setuid failure")

        /* 监控系统调用(如果开启了的话)，防止恶意代码 */
        if (runobj->trace)
            if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1)
                RAISE_EXIT("TRACEME failure")

        /* 开始执行 */
        execvp(runobj->args[0], (char * const *) runobj->args);

        RAISE_EXIT("execvp failure")
    }
    else {
        int r;
        char errbuffer[100] = { 0 };

        close(fd_err[1]);
        r = read(fd_err[0], errbuffer, 90);
        close(fd_err[0]);
        if (r > 0) {
            waitpid(pid, NULL, WNOHANG);
            RAISE(errbuffer);
            return -1;
        }

        /* 根据是否提供trace来决定使用哪种运行方式 */
        if (runobj->trace)
            r = traceLoop(runobj, rst, pid);
        else
            r = waitExit(runobj, rst, pid);

        if (r)
            RAISE1(last_run_err);
        return 0;
    }
}

