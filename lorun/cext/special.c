#include "special.h"
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "limit.h"

char * special_judge(struct Runobj *spjobj)
{
    pid_t pid;
    int fd_err[2];
    char * outbuffer;
    outbuffer = (char * ) malloc (sizeof(char) * 110);

#define RAISE_EXITC(out) {\
            strcpy(outbuffer, out);\
            return outbuffer;\
        }

    if (pipe(fd_err) < 0)
        RAISE_EXITC("special: pip(fd_err) failure");

    pid = vfork();
    if (pid < 0) {
        close(fd_err[0]);
        close(fd_err[1]);
        RAISE_EXITC("special : vfork failure");
    }

    if (pid == 0) {
        close(fd_err[0]);
        /* 重定向stdout流 */
        if (dup2(fd_err[1], STDOUT_FILENO) == -1)
            RAISE_EXITC("dup2 stderr failure!")
        /* 为spj过程设置限制 */
        if (setResLimit(spjobj) == -1)
            RAISE_EXITC(last_limit_err)
        /* 修改运行用户(为确保安全，请务必提供此参数) */
        if (spjobj->runner != -1)
            if (setuid(spjobj->runner))
                RAISE_EXITC("setuid failure")

        /* 开始spj */
        execvp(spjobj->args[0], (char * const *) spjobj->args);

        RAISE_EXITC("execvp failure")
    }
    else {
        close(fd_err[1]);

        int r;
        int status;
        struct rusage ru;

        /* 等待spj结束 */
        if (wait4(pid, &status, 0, &ru) == -1)
            RAISE_EXITC("wait4 failure")

        /* 判断是否发生异常 */
        if (status || WIFSIGNALED(status)) {
            /* 程序运行无异常，结果错误 */
            if (status == 256) {
                r = read(fd_err[0], outbuffer, 100);
                outbuffer[r] = '\0';
                return outbuffer;
            }
            switch (WTERMSIG(status)) {
                /* 若编译期间占用资源超出限制 */
                case SIGSEGV:
                case SIGALRM:
                case SIGXCPU:
                    strcpy(outbuffer, "special error\n");
                    break;
                default:
                    /* 读取spj输出的前一百个字符 */
                    r = read(fd_err[0], outbuffer, 100);
                    outbuffer[r] = '\0';
                    break;
            }
            return outbuffer;
        }
        else
            return NULL;
    }
}