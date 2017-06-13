#include "compile.h"
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "limit.h"

char * compileit(struct Runobj *comobj)
{
    pid_t pid;
    int fd_err[2];
    char * errbuffer;
    errbuffer = (char * ) malloc (sizeof(char) * 1010);

#define RAISE_EXITC(err) {\
            strcpy(errbuffer, err);\
            return errbuffer;\
        }

    if (pipe(fd_err) < 0)
        RAISE_EXITC("compile: pip(fd_err) failure");

    pid = vfork();
    if (pid < 0) {
        close(fd_err[0]);
        close(fd_err[1]);
        RAISE_EXITC("compile : vfork failure");
    }

    if (pid == 0) {
        close(fd_err[0]);
        /* 仅重定向error流 */
        if (dup2(fd_err[1], STDERR_FILENO) == -1)
            RAISE_EXITC("dup2 stderr failure!")
        /* 为编译过程设置限制 */
        if (setResLimit(comobj) == -1)
            RAISE_EXITC(last_limit_err)
        /* 修改运行用户(为确保安全，请务必提供此参数) */
        if (comobj->runner != -1)
            if (setuid(comobj->runner))
                RAISE_EXITC("setuid failure")

        /* 开始编译 */
        execvp(comobj->args[0], (char * const *) comobj->args);

        RAISE_EXITC("execvp failure")
    }
    else {
        close(fd_err[1]);

        int r;
        int status;
        struct rusage ru;

        /* 等待编译结束 */
        if (wait4(pid, &status, 0, &ru) == -1)
            RAISE_EXITC("wait4 failure")

        /* 判断是否发生异常 */
        if (status || WIFSIGNALED(status)) {
            switch (WTERMSIG(status)) {
                /* 若编译期间占用资源超出限制 */
                case SIGSEGV:
                case SIGALRM:
                case SIGXCPU:
                    strcpy(errbuffer, "Compile-time error\n");
                    break;
                default:
                    /* 读取编译错误的前一千个字符 */
                    r = read(fd_err[0], errbuffer, 1000);
                    errbuffer[r] = '\0';
                    break;
            }
            return errbuffer;
        }
        else
            return NULL;
    }
}