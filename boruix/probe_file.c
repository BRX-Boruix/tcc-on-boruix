/* probe_file.c —— 移植期诊断工具：从 **BORUIX 内部**报告一个文件的真实内容（3P6-1）。
 *
 * 为什么需要它：上一轮我试图在宿主机上直接搜 `disk.img` 的字节来判断"盘上内容是否完整"，
 * 但那条测量**无效**（75 MB 镜像经 FileStream 读回 0 字节）。与其猜，不如让系统自己报。
 *
 * 用法：probe_file <路径>...（本系统 argv 语义见 csrc/user_main.c，故此处用绝对路径写死一个）。
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static void report(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("%s: open 失败\n", path);
        return;
    }
    unsigned char buf[64];
    long n = read(fd, buf, sizeof buf);
    long sz = lseek(fd, 0, SEEK_END);
    printf("%s\n", path);
    printf("  read() 返回 %ld 字节, lseek(END) = %ld\n", n, sz);
    printf("  头 %ld 字节:", n < 16 ? n : 16);
    for (long i = 0; i < (n < 16 ? n : 16); i++) {
        printf(" %02x", buf[i]);
    }
    printf("\n");
    printf("  是 ELF 吗: %s\n", (n >= 4 && buf[0] == 0x7f && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') ? "是" : "否");
    close(fd);
}

int main(void) {
    report("/volumes/BORUIX_DATA/3p/tcc/crt1.o");
    report("/volumes/BORUIX_DATA/3p/tcc/crti.o");
    report("/volumes/BORUIX_DATA/3p/tcc/libc.a");
    report("/volumes/BORUIX_DATA/3p/tcc/include/stdio.h");
    return 0;
}