__asm__(".global _start\n"
	"_start:\n"
	"mov $60, %rax\n"
	"xor %rdi, %rdi\n"
	"syscall\n");
