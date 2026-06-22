// MCSOS Kernel Entry Point Minimal - M1 Verification
void _start(void) {
    // Loop selamanya, mencegah eksekusi rontok ke memori acak
    while (1) {
        __asm__ volatile("hlt");
    }
}
