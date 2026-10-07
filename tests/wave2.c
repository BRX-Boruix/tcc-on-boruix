/* wave2.c —— 3P6-2（libc 宽度第二波）的**真实驱动 + 自检**。
 *
 * 为什么是这个程序：第二波的判据是「按 tcc/GCC 的**真实报错**补，不预猜」，所以需要一个
 * 真实会用到 libc 多面（而不只是 printf）的程序。这就是一个"系统内小工具"：
 * 打印自己的 argv、读一个文件并统计词尾、列一个目录、格式化当前时间、报告 errno、
 * 排序一小串字符串、读环境变量。
 *
 * 本轮由它**真实暴露**并已修的缺口（不是预猜的清单）：
 *   1. strftime —— 头文件没声明、库里也没实现（tcc: unresolved reference to 'strftime'）；
 *   2. getpid   —— 实现早就在（libc/src/process.rs），缺的只是 <unistd.h> 里的**声明**
 *                  （tcc: implicit declaration of function 'getpid'）。
 *
 * 它同时是 3P6-4（系统内编出一个新 cowsay 并运行）的前身——先把"能编能跑能自检"的底座打通。
 *
 * 退出码 0 = 全部断言通过。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <grp.h>
#include <fcntl.h>
#include <boruix.h>

#define DIRPATH "/volumes/BORUIX_DATA/3p"
#define FILEPATH DIRPATH "/hello.c"

static int fails = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        printf("[wave2] FAIL: %s\n", what);
        fails++;
    }
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* 用**固定**的 struct tm 校验 strftime 的确定性输出（不依赖真实时钟）。 */
static void check_strftime(void) {
    struct tm t;
    char buf[64];
    memset(&t, 0, sizeof t);
    t.tm_year = 123;   /* 2023 */
    t.tm_mon  = 9;     /* 十月（0 起） */
    t.tm_mday = 7;
    t.tm_hour = 0;
    t.tm_min  = 53;
    t.tm_sec  = 41;
    t.tm_wday = 6;     /* 周六 */
    t.tm_yday = 279;   /* 年内第 280 天 */

    memset(buf, 0, sizeof buf);
    check(strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &t) == 19,
          "strftime 返回长度 19");
    check(strcmp(buf, "2023-10-07 00:53:41") == 0, "strftime %Y-%m-%d %H:%M:%S");
    printf("[wave2] strftime: %s\n", buf);

    memset(buf, 0, sizeof buf);
    strftime(buf, sizeof buf, "%a %b %e %I:%M %p %j %w %u", &t);
    check(strcmp(buf, "Sat Oct  7 12:53 AM 280 6 6") == 0,
          "strftime %a %b %e %I:%M %p %j %w %u");
    printf("[wave2] strftime: [%s]\n", buf);

    memset(buf, 0, sizeof buf);
    strftime(buf, sizeof buf, "%F %T %D %R %% %Z %z", &t);
    check(strcmp(buf, "2023-10-07 00:53:41 10/07/23 00:53 % UTC +0000") == 0,
          "strftime %F %T %D %R %% %Z %z");
    printf("[wave2] strftime: %s\n", buf);

    /* 容量边界：POSIX 口径 max **含**结尾 NUL。 */
    memset(buf, 0, sizeof buf);
    check(strftime(buf, 11, "%Y-%m-%d", &t) == 10, "strftime 恰好放下 -> 10");
    memset(buf, 0, sizeof buf);
    check(strftime(buf, 10, "%Y-%m-%d", &t) == 0, "strftime 差一字节 -> 0");

    /* 未知说明符按 POSIX 允许的方式原样输出，绝不静默丢弃。 */
    memset(buf, 0, sizeof buf);
    strftime(buf, sizeof buf, "<%Q>", &t);
    check(strcmp(buf, "<%Q>") == 0, "strftime 未知说明符原样输出");
}

int main(int argc, char **argv) {
    int i;
    printf("[wave2] argc=%d pid=%d\n", argc, (int)getpid());
    check(getpid() > 0, "getpid() > 0");
    check(gettid() > 0, "gettid() > 0");
    for (i = 0; i < argc; i++)
        printf("[wave2] argv[%d]=%s\n", i, argv[i]);

    /* 1. errno + strerror */
    {
        FILE *f = fopen("/nope/nope", "r");
        check(f == NULL, "fopen(/nope/nope) 应失败");
        if (!f) {
            printf("[wave2] fopen(/nope/nope) -> %s (errno=%d)\n",
                   strerror(errno), errno);
            check(errno == ENOENT, "errno == ENOENT");
            check(strcmp(strerror(errno), "No such file or directory") == 0,
                  "strerror(ENOENT) 文本");
        } else {
            fclose(f);
        }
    }

    /* 2. strftime（确定性） */
    check_strftime();

    /* 3. time/localtime：只要求拿到一个合理的 epoch */
    {
        time_t now = time(NULL);
        struct tm *tmv = localtime(&now);
        char buf[64];
        check(now > 1600000000, "time() 合理");
        check(tmv != NULL, "localtime 非空");
        if (tmv && strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", tmv))
            printf("[wave2] now=%s (t=%ld)\n", buf, (long)now);
        else
            check(0, "真实时间 strftime");
    }

    /* 4. 目录枚举 */
    {
        DIR *d = opendir(DIRPATH);
        int n = 0;
        check(d != NULL, "opendir 非空");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL && n < 6) {
                printf("[wave2] dirent: %s\n", e->d_name);
                n++;
            }
            closedir(d);
        }
        check(n > 0, "readdir 至少一项");
    }

    /* 5. stat */
    {
        struct stat st;
        int rc = stat(FILEPATH, &st);
        check(rc == 0, "stat 成功");
        if (rc == 0) {
            printf("[wave2] stat %s: size=%ld mode=%o\n",
                   FILEPATH, (long)st.st_size, (unsigned)st.st_mode);
            check(st.st_size > 0, "hello.c 非空");
        }
    }

    /* 6. 读文件 + ctype（统计"以字母结尾的词"个数） */
    {
        FILE *f = fopen(FILEPATH, "r");
        int words = 0;
        check(f != NULL, "fopen(hello.c)");
        if (f) {
            char line[256];
            while (fgets(line, sizeof line, f)) {
                char *p;
                for (p = line; *p; p++)
                    if (isalpha((unsigned char)*p) &&
                        !isalpha((unsigned char)p[1]))
                        words++;
            }
            fclose(f);
            printf("[wave2] %s word-ends=%d\n", FILEPATH, words);
            check(words > 0, "词尾计数 > 0");
        }
    }

    /* 7. malloc + qsort + free */
    {
        const char *items[4];
        const char **arr;
        int n = 4;
        items[0] = "delta"; items[1] = "alpha";
        items[2] = "charlie"; items[3] = "bravo";
        arr = (const char **)malloc(sizeof(const char *) * (size_t)n);
        check(arr != NULL, "malloc 非空");
        if (!arr)
            return 1;
        for (i = 0; i < n; i++)
            arr[i] = items[i];
        qsort(arr, (size_t)n, sizeof(const char *), cmp_str);
        printf("[wave2] sorted:");
        for (i = 0; i < n; i++)
            printf(" %s", arr[i]);
        printf("\n");
        check(strcmp(arr[0], "alpha") == 0 && strcmp(arr[3], "delta") == 0,
              "qsort 顺序");
        free(arr);
    }

    /* 8. getenv */
    {
        const char *path = getenv("PATH");
        printf("[wave2] PATH=%s\n", path ? path : "(null)");
        check(path != NULL && path[0] != 0, "PATH 非空");
    }


    /* 9. 3P6-2 第二波：**头文件覆盖审计**列出的那些入口——按真实用法逐项验证。
     * 它们此前"实现了但没声明"（getpid/dup2/EINTR 那一类），审计一次列全后补上声明；
     * 这里用真实调用证明"声明可用、语义可跑"，而不是只看头文件里有没有那个词。 */
    {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        printf("[wave2] sysconf(_SC_NPROCESSORS_ONLN)=%ld\n", n);
        check(n > 0, "sysconf 在线 CPU 数 > 0");
        check(sysconf(99999) == -1, "sysconf 不支持的名字返回 -1");
    }
    {
        struct group *gr = getgrgid(0);
        printf("[wave2] getgrgid(0)=%s\n", gr ? gr->gr_name : "(null)");
        check(gr == NULL || gr->gr_name != NULL, "getgrgid 要么 NULL 要么有名字");
        endgrent();
    }
    check(boruix_malloc_corrupt() == 0, "boruix_malloc_corrupt() == 0");
    check(yield_sys() == 0, "yield_sys() == 0");

    /* rename / ftruncate / symlink / chown：在数据盘上**真做一遍**（不 mock）。 */
    {
        const char *fa = "w2tmp_a";
        const char *fb = "w2tmp_b";
        const char *fl = "w2tmp_link";
        FILE *f = fopen(fa, "w");
        check(f != NULL, "创建临时文件");
        if (f) {
            fputs("0123456789", f);
            fclose(f);
        }
        check(rename(fa, fb) == 0, "rename 成功");
        {
            int fd = open(fb, O_WRONLY);
            check(fd >= 0, "open 临时文件");
            if (fd >= 0) {
                check(ftruncate(fd, 4) == 0, "ftruncate 成功");
                close(fd);
            }
        }
        {
            int rc = symlink(fb, fl);
            printf("[wave2] symlink -> rc=%d errno=%d\n", rc, errno);
            check(rc == 0 || errno != 0, "symlink 有明确结果");
            if (rc == 0) remove(fl);
        }
        {
            int rc = chown(fb, 0, 0);
            printf("[wave2] chown -> rc=%d errno=%d\n", rc, errno);
            check(rc == 0 || errno != 0, "chown 有明确结果");
        }
        remove(fb);
    }

    /* fork + waitpid + W* 宏（<sys/wait.h>）：真起一个子进程、真收它的退出码。 */
    {
        int pid;
        fflush(stdout);
        pid = fork();
        if (pid == 0) {
            _exit(42);
        }
        check(pid > 0, "fork 返回子 pid");
        if (pid > 0) {
            int st = 0;
            int got = waitpid(pid, &st, 0);
            printf("[wave2] waitpid -> pid=%d status=%d WIFEXITED=%d WEXITSTATUS=%d\n",
                   got, st, WIFEXITED(st), WEXITSTATUS(st));
            check(got == pid, "waitpid 返回子 pid");
            check(WIFEXITED(st) != 0 && WEXITSTATUS(st) == 42, "WEXITSTATUS == 42");
        }
    }

    /* 10. 3P6-2「整项缺失」类：身份查询（getuid/geteuid/getgid/getegid）与 dup。
     * 这些此前**既没实现也没声明**（反向对账清单列出来的），所以不能只验"能编译"——
     * 要用**交叉证据**：新建文件的 st_uid/st_gid 必须等于 getuid()/getgid()。 */
    {
        uid_t u = getuid();
        uid_t eu = geteuid();
        gid_t g = getgid();
        gid_t eg = getegid();
        printf("[wave2] getuid=%d geteuid=%d getgid=%d getegid=%d\n",
               (int)u, (int)eu, (int)g, (int)eg);
        check(u == eu, "geteuid()==getuid()（本系统无 real/effective 之分）");
        check(g == eg, "getegid()==getgid()");
        {
            FILE *f = fopen("w2id.tmp", "w");
            check(f != NULL, "创建身份交叉验证文件");
            if (f) {
                fputs("x", f);
                fclose(f);
            }
            {
                struct stat st;
                if (stat("w2id.tmp", &st) == 0) {
                    printf("[wave2] stat.st_uid=%u st_gid=%u\n",
                           (unsigned)st.st_uid, (unsigned)st.st_gid);
                    check((uid_t)st.st_uid == u, "新建文件 st_uid == getuid()（交叉验证）");
                    check((gid_t)st.st_gid == g, "新建文件 st_gid == getgid()（交叉验证）");
                } else {
                    check(0, "stat 身份交叉验证文件");
                }
            }
            remove("w2id.tmp");
        }
    }
    {
        /* dup：真读一个文件，验证 (a) 得到不同的新 fd、(b) 与原 fd **共享文件偏移** */
        int fd = open(FILEPATH, O_RDONLY);
        check(fd >= 0, "open 供 dup 测试");
        if (fd >= 0) {
            char a[8];
            char b[8];
            ssize_t n1 = read(fd, a, 4);
            int d;
            a[n1 > 0 ? n1 : 0] = 0;
            d = dup(fd);
            check(d >= 0, "dup 返回新 fd");
            check(d != fd, "dup 的 fd 与原 fd 不同");
            if (d >= 0) {
                ssize_t n2 = read(d, b, 4);
                b[n2 > 0 ? n2 : 0] = 0;
                printf("[wave2] dup: 原读='%s' 副本续读='%s'\n", a, b);
                check(n1 == 4 && n2 == 4, "两次各读到 4 字节");
                /* hello.c 前 8 字节是 "#include"，故副本必须从第 5 字节 'l' 续读。 */
                check(n2 == 4 && b[0] == 'l', "副本从原 fd 当前位置续读（共享偏移）");
                close(d);
            }
            close(fd);
        }
    }
    {
        /* fcntl(F_DUPFD)：与 dup 共用同一实现的回归保护 */
        int fd = open(FILEPATH, O_RDONLY);
        if (fd >= 0) {
            int d = fcntl(fd, F_DUPFD, 0);
            check(d >= 0 && d != fd, "fcntl(F_DUPFD,0) 与 dup 同实现且可用");
            if (d >= 0) close(d);
            close(fd);
        }
    }
    if (fails) {
        printf("[wave2] %d 项断言失败\n", fails);
        return 1;
    }
    printf("[wave2] all checks passed\n");
    return 0;
}
