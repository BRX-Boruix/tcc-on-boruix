/* stubs.c —— 诊断桩：让 tcc 在没有 libc 的情况下也能走到"写可执行文件"这一步。
 *
 * 目的：区分崩溃发生在「读对象」还是「写 ELF 输出」。
 * 做法：把 hello.c 需要的两个外部符号提供成桩，于是链接没有未定义符号，
 * tcc 必须真的走完 ELF 写出。若此时崩 -> 崩在写出阶段。
 */
int printf(const char *fmt, ...) { (void)fmt; return 0; }
void __boruix_init_environ(int argc, char **argv) { (void)argc; (void)argv; }