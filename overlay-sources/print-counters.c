int main(void) {
    asm volatile (".insn r 0x5b, 0x1, 0x46, x0, x0, x0");
    return 0;
}
