# OS_panduan_M4.md

# Panduan Praktikum M4 — Interrupt Descriptor Table, Exception Trap Path, Trap Frame, dan Fault-Handling Awal MCSOS 260502

**Identitas akademik**  
Dosen: **Muhaemin Sidiq, S.Pd., M.Pd.**  
Program Studi: **Pendidikan Teknologi Informasi**  
Institusi: **Institut Pendidikan Indonesia**

## 0. Ringkasan Praktikum

Praktikum M4 membangun fondasi awal mekanisme trap dan exception pada kernel pendidikan **MCSOS 260502** untuk target **x86_64**. Modul ini melanjutkan M0 sampai M3: M0 menyiapkan baseline lingkungan dan tata kelola, M1 memvalidasi toolchain dan build reproducibility, M2 menghasilkan kernel ELF/ISO bootable awal, dan M3 memasang panic path, logging, halt path, serta workflow audit ELF/disassembly. M4 menambahkan **Interrupt Descriptor Table (IDT)**, stub exception awal untuk vektor 0 sampai 31, normalisasi stack exception ke bentuk **trap frame**, dispatcher C `x86_64_trap_dispatch`, dan jalur uji `int3` untuk membuktikan bahwa handler exception dapat dipanggil dan kembali melalui `iretq`.

Sumber kebenaran teknis utama M4 adalah Intel SDM untuk interrupt/exception handling dan format struktur sistem x86_64, dokumentasi QEMU untuk invocation dan gdbstub, dokumentasi GNU Binutils untuk linker script dan inspeksi ELF, dokumentasi LLVM/Clang/LLD untuk mode freestanding dan linking, serta dokumentasi Limine untuk boot path yang telah digunakan pada M2/M3 [1]–[7]. Karena perilaku trap/exception merupakan boundary correctness tingkat rendah, keberhasilan build saja tidak boleh ditafsirkan sebagai kernel bebas cacat. Status yang dapat diklaim setelah M4 adalah **siap uji QEMU untuk IDT dan breakpoint exception path** bila seluruh bukti build, audit, serial log, dan GDB tersedia.

## 1. Capaian Pembelajaran Praktikum

Setelah menyelesaikan M4, mahasiswa mampu:

1. Menjelaskan fungsi IDT pada x86_64, relasi IDTR, gate descriptor, vektor exception, dan handler stub.
2. Membuat struktur `x86_64_idt_entry_t` dan `x86_64_idtr_t` dengan ukuran, alignment, dan packing yang sesuai untuk mode 64-bit.
3. Mengisi IDT minimal untuk vektor exception 0 sampai 31 dengan handler assembly yang dapat dilink ke kernel ELF freestanding.
4. Menulis stub assembly yang menormalisasi exception dengan dan tanpa error code ke satu struktur `x86_64_trap_frame_t`.
5. Memanggil dispatcher C dari handler assembly dengan memperhatikan register preservation, stack layout, red-zone policy, dan ABI System V x86_64.
6. Menguji jalur exception yang recoverable melalui `int3`, lalu memastikan kernel dapat kembali dari handler menggunakan `iretq`.
7. Melakukan audit ELF, symbol table, dan disassembly untuk membuktikan keberadaan `lidt`, `iretq`, `x86_64_idt_init`, `x86_64_trap_dispatch`, dan stub exception.
8. Menganalisis failure modes seperti triple fault, general protection fault akibat gate salah, stack frame tidak cocok, infinite fault loop, log serial kosong, dan symbol unresolved.
9. Menyusun bukti praktikum dengan log build, log QEMU, file map, output `readelf`, output `nm`, output `objdump`, screenshot, dan commit Git.

## 2. Prasyarat Teori

Mahasiswa harus memahami bahwa CPU x86_64 menggunakan **IDT** untuk menemukan handler interrupt dan exception. Setiap entry IDT adalah descriptor gate 16 byte pada mode 64-bit. Alamat IDT aktif disimpan dalam register IDTR dan dimuat dengan instruksi `lidt`. Saat exception terjadi, CPU melakukan transisi kontrol ke handler dan menaruh state minimum pada stack, antara lain instruction pointer, code segment, flags, dan untuk sebagian exception juga error code. Stub assembly M4 menambahkan nomor vektor dan menyimpan register umum agar dispatcher C menerima satu layout trap frame yang seragam.

M4 belum mengaktifkan IRQ eksternal, PIC/APIC, timer, atau preemption. Seluruh cakupan M4 dibatasi pada **CPU exception vectors 0–31**. Vektor `#BP` atau breakpoint dipilih sebagai uji recoverable karena instruksi `int3` dapat kembali ke instruksi setelah breakpoint ketika handler melakukan `iretq`. Sebaliknya, exception seperti `#DE`, `#UD`, dan `#PF` umumnya tidak boleh langsung dikembalikan tanpa perbaikan state karena dapat menyebabkan fault berulang.

## 3. Peta Skill yang Digunakan

| Skill | Peran dalam M4 |
|---|---|
| `osdev-general` | Menetapkan gate M4, acceptance criteria, readiness review, dan batas klaim hasil praktikum. |
| `osdev-01-computer-foundation` | Menurunkan state machine exception, invariants, safety property, dan proof obligation. |
| `osdev-02-low-level-programming` | Mengatur ABI, stack frame, assembly stub, `lidt`, `iretq`, register preservation, dan freestanding C. |
| `osdev-03-computer-and-hardware-architecture` | Menjelaskan privilege mode, IDT, exception vector, error code, dan batas emulator vs hardware. |
| `osdev-04-kernel-development` | Mengintegrasikan trap path dengan panic/logging dan kernel control flow. |
| `osdev-07-os-security` | Menegaskan fail-closed behavior untuk exception non-recoverable dan menghindari return sembarangan. |
| `osdev-10-boot-firmware` | Memastikan kernel M4 tetap kompatibel dengan boot handoff M2/M3. |
| `osdev-12-toolchain-devenv` | Memastikan build, link, ELF inspection, disassembly, dan reproducibility tetap terkendali. |
| `osdev-14-cross-science` | Menyusun verification matrix, risk register, failure mode analysis, dan evidence policy. |

## 4. Asumsi Target dan Batasan M4

### 4.1 Asumsi utama

| Komponen | Asumsi M4 |
|---|---|
| Arsitektur | x86_64 long mode. |
| Host pengembangan | Windows 11 x64 dengan WSL 2 Linux. |
| Emulator | QEMU `qemu-system-x86_64`, headless serial log. |
| Firmware/boot path | Boot path M2/M3 berbasis Limine atau ISO yang telah berjalan. |
| Kernel | Monolitik pendidikan, ring 0, single-core, belum ada userspace. |
| Bahasa | C17 freestanding dan assembly x86_64 minimal. |
| ABI | x86_64 System V untuk boundary assembly ke C internal kernel, dengan `-mno-red-zone`. |
| Toolchain | `clang`, `ld.lld`, `readelf`, `objdump`, `nm`, `make`. |
| Non-goal | IRQ eksternal, PIC/APIC, LAPIC timer, HPET, syscall, user mode, paging lanjut, SMP, signal, dan scheduler. |

### 4.2 Non-goals

M4 tidak menyelesaikan seluruh subsistem interrupt. M4 tidak mengonfigurasi PIC, IOAPIC, LAPIC, x2APIC, MSI/MSI-X, timer interrupt, keyboard interrupt, atau preemptive scheduling. M4 juga tidak membuat mekanisme recovery kompleks untuk page fault. Untuk exception selain `#BP`, dispatcher default M4 memilih panic fail-closed agar kernel tidak kembali ke state yang tidak dapat dibuktikan aman.

## 4A. Architecture and Design M4

Arsitektur M4 terdiri atas lima komponen: tabel IDT statis, daftar pointer stub exception, stub assembly, dispatcher C, dan jalur uji breakpoint. IDT diletakkan dalam `.bss` atau `.data` kernel dan diisi saat `kmain`. Setiap gate exception memuat offset handler, selector kode kernel, field IST nol, type attribute interrupt/trap gate, dan reserved field nol. `#BP` menggunakan trap gate karena ia merupakan exception recoverable yang dipakai untuk uji return path; vektor lain menggunakan interrupt gate untuk menonaktifkan interrupt maskable ketika memasuki handler pada tahap awal.

Dispatcher C hanya boleh menerima trap frame yang telah dinormalisasi. Stub assembly bertanggung jawab menambahkan error code nol untuk exception yang tidak punya error code dan menambahkan nomor vektor. Semua register umum yang digunakan kernel awal disimpan ke stack sebelum memanggil C. Setelah dispatcher kembali, stub memulihkan register, membuang `vector` dan `error_code`, lalu menjalankan `iretq`.

## 4B. Interfaces, ABI, and API Boundary M4

| Interface | Tipe | Kontrak |
|---|---|---|
| `x86_64_idt_init()` | C API internal | Mengisi IDT, memuat IDTR dengan `lidt`, mencatat base/limit, dan menjalankan assert ukuran. |
| `x86_64_idt_set_gate()` | C API internal | Mengisi satu descriptor IDT; caller wajib memberi vector dan handler valid. |
| `x86_64_exception_stubs[32]` | Assembly symbol table | Menyediakan pointer handler exception 0–31 untuk IDT. |
| `isr_stub_N` | Assembly handler | Entry point CPU exception; tidak boleh dipanggil seperti fungsi C biasa. |
| `x86_64_trap_dispatch()` | C dispatcher | Menerima `x86_64_trap_frame_t *`, mencatat frame, recover untuk `#BP`, panic untuk exception lain. |
| `x86_64_trigger_breakpoint_for_test()` | C API internal | Memanggil `int3` untuk uji `#BP`. |

## 4C. Invariants and Correctness Obligations M4

| Invariant | Alasan | Bukti minimum |
|---|---|---|
| `sizeof(x86_64_idt_entry_t) == 16` | Entry IDT 64-bit harus 16 byte. | `KERNEL_ASSERT`, review struct, dan build. |
| `idtr.limit == 4095` | 256 entry × 16 byte dikurangi 1. | Serial log `idt_limit` dan assert. |
| Setiap exception vector 0–31 punya handler non-null. | Exception tanpa handler dapat menjadi #GP atau triple fault. | Symbol `x86_64_exception_stubs`, audit `nm`, dan QEMU log. |
| Exception dengan error code dan tanpa error code dinormalisasi ke frame yang sama. | Dispatcher C harus membaca field yang konsisten. | Review `isr.S`, komentar, dan intentional `int3` test. |
| Stub memulihkan register sebelum `iretq`. | Kernel state tidak boleh rusak setelah `#BP`. | Disassembly memuat push/pop dan `iretq`. |
| Dispatcher tidak return dari exception non-recoverable. | Menghindari infinite fault loop. | Review branch dispatcher dan intentional panic variant. |
| Build tetap freestanding dan tanpa undefined external symbol. | Kernel tidak boleh diam-diam bergantung pada libc host. | `nm -u` kosong dan `-nostdlib`. |

## 4D. Security and Threat Model M4

M4 berada sebelum userspace dan sebelum driver kompleks, tetapi tetap memiliki risiko keamanan dasar. Attack surface utama belum datang dari pengguna, melainkan dari bug internal: descriptor salah, stack frame salah, handler return ke state tidak valid, log membocorkan pointer tanpa kebijakan, dan exception loop yang merusak diagnosability. Kebijakan M4 adalah fail-closed: hanya `#BP` yang diperlakukan recoverable untuk uji; exception lain masuk panic path dengan log register minimum. Pada tahap ini pointer kernel dicetak ke serial karena tujuan praktikum adalah observability; pada rilis yang lebih matang, kebijakan redaction harus dipertimbangkan.

## 4E. State Machine Trap M4

```text
BOOTED_M3_READY
  -> IDT_ALLOCATED
  -> IDT_ENTRIES_FILLED
  -> IDTR_LOADED
  -> M4_SELFTEST_PASSED
  -> BREAKPOINT_OPTIONAL_TRIGGERED
  -> TRAP_DISPATCH_ENTERED
  -> BREAKPOINT_RETURNED | PANIC_FAIL_CLOSED
  -> HALT_LOOP
```

Transisi dianggap sah hanya bila precondition terpenuhi. `IDTR_LOADED` hanya boleh terjadi setelah minimal vector 0–31 terisi. `BREAKPOINT_RETURNED` hanya sah untuk vector 3. Untuk vector lain, state yang aman adalah `PANIC_FAIL_CLOSED`, bukan kembali ke instruksi penyebab fault.

## 4F. Validation and Verification Plan M4

| Validasi | Perintah | Bukti |
|---|---|---|
| Clean build | `make clean && make build` | `build/kernel.elf` ada. |
| Build varian breakpoint | `make breakpoint` | `build/kernel.breakpoint.elf` ada. |
| Build varian panic | `make panic` | `build/kernel.panic.elf` ada. |
| Audit ELF | `make inspect` | `readelf`, `nm`, `objdump` outputs. |
| Audit IDT/IRET | `tools/scripts/m4_audit_elf.sh build/kernel.elf` | `lidt`, `iretq`, `x86_64_trap_dispatch`, `isr_stub_14`. |
| Undefined symbol check | `nm -u build/kernel.elf` | Output kosong. |
| QEMU smoke | `tools/scripts/m4_qemu_run.sh build/mcsos.iso` | Log serial berisi `[M4] IDT loaded`. |
| GDB debug | `qemu -S -s` + `gdb -x tools/gdb_m4.gdb` | Breakpoint pada `x86_64_idt_init` dan `x86_64_trap_dispatch`. |

## 5. Struktur Repository Setelah M4

```text
mcsos/
├── Makefile
├── linker.ld
├── kernel/
│   ├── arch/x86_64/
│   │   ├── idt.c
│   │   ├── isr.S
│   │   └── include/mcsos/arch/
│   │       ├── cpu.h
│   │       ├── idt.h
│   │       ├── io.h
│   │       └── isr.h
│   ├── core/
│   │   ├── kmain.c
│   │   ├── log.c
│   │   ├── panic.c
│   │   ├── serial.c
│   │   └── trap.c
│   ├── include/mcsos/kernel/
│   │   ├── log.h
│   │   ├── panic.h
│   │   └── version.h
│   └── lib/
│       └── memory.c
├── tools/
│   ├── gdb_m4.gdb
│   └── scripts/
│       ├── m4_audit_elf.sh
│       ├── m4_collect_evidence.sh
│       ├── m4_preflight.sh
│       ├── m4_qemu_run.sh
│       └── grade_m4.sh
└── evidence/M4/
```

## 6. Pemeriksaan Kesiapan Hasil M0/M1/M2/M3 Sebelum M4

### 6.1 Pemeriksaan lokasi repository

Tujuan pemeriksaan ini adalah memastikan mahasiswa menjalankan perintah di root repository MCSOS, bukan di folder `build`, `kernel`, atau direktori unduhan. Kesalahan lokasi sering membuat `make` memakai Makefile yang salah atau gagal menemukan `linker.ld`.

```bash
pwd
ls -la
test -d .git && echo "OK: repository Git ditemukan"
test -f Makefile && echo "OK: Makefile ditemukan"
test -f linker.ld && echo "OK: linker.ld ditemukan"
```

Jika `.git` tidak ditemukan, pindah ke direktori repository yang benar. Jika repository hilang, clone ulang atau ekstrak arsip praktikum terakhir, lalu ulangi validasi M0/M1.

### 6.2 Pemeriksaan hasil M0 — tata kelola dan baseline lingkungan

M0 dianggap siap untuk M4 bila repository memiliki struktur dokumentasi minimum, Git aktif, dan mahasiswa dapat menjelaskan target x86_64/WSL/QEMU. Jalankan:

```bash
git status --short
git log --oneline -5
ls docs 2>/dev/null || true
ls evidence 2>/dev/null || true
```

Kendala umum: branch praktikum bercampur dengan eksperimen. Solusi konservatif adalah membuat branch baru sebelum M4:

```bash
git switch -c m4-idt-exception-path
```

Jika ada perubahan M3 yang belum dikomit, simpan terlebih dahulu:

```bash
git add .
git commit -m "Complete M3 panic logging baseline"
```

### 6.3 Pemeriksaan hasil M1 — toolchain dan reproducibility

M1 dianggap siap bila `clang`, `ld.lld`, `readelf`, `objdump`, `nm`, dan `make` tersedia. Jalankan:

```bash
clang --version | head -n 1
ld.lld --version | head -n 1
readelf --version | head -n 1
objdump --version | head -n 1
nm --version | head -n 1
make --version | head -n 1
```

Jika `clang` atau `ld.lld` tidak ditemukan, perbaiki instalasi toolchain di WSL:

```bash
sudo apt update
sudo apt install -y clang lld llvm make binutils git xorriso qemu-system-x86 ovmf
```

Jika build menghasilkan warning dan `-Werror` membuat build gagal, jangan menonaktifkan `-Werror`. Perbaiki penyebab warning karena M4 berada di correctness boundary assembly/C.

### 6.4 Pemeriksaan hasil M2 — boot artifact dan kernel ELF

M2 dianggap siap bila kernel dapat dilink menjadi ELF64 x86_64 dan ISO dapat dibuat melalui boot path yang sudah dipilih. Jalankan:

```bash
make clean
make build
readelf -h build/kernel.elf | sed -n '1,25p'
nm -n build/kernel.elf | grep -E 'kmain|__kernel_start|__kernel_end'
```

Jika `readelf` tidak menunjukkan `ELF64` atau machine `Advanced Micro Devices X86-64`, periksa target clang dan linker script. Target M4 wajib memakai `--target=x86_64-unknown-none-elf`, bukan target Linux host default.

### 6.5 Pemeriksaan hasil M3 — logging, panic path, halt path, dan serial

M3 dianggap siap bila `log_init`, `kernel_panic_at`, `cpu_halt_forever`, dan driver serial tersedia. Jalankan:

```bash
grep -R "kernel_panic_at" -n kernel/include kernel/core
grep -R "log_writeln" -n kernel/include kernel/core
grep -R "cpu_halt_forever" -n kernel/arch/x86_64/include kernel/core
make audit
```

Jika `kernel_panic_at` undefined saat link, pastikan `kernel/core/panic.c` masih berada di bawah direktori `kernel` dan pola `find kernel -name '*.c'` di Makefile masih mengambil file tersebut. Jika log serial kosong saat QEMU, periksa bahwa `serial_init` masih menulis ke COM1 `0x3F8` dan QEMU dijalankan dengan `-serial file:...` atau `-serial stdio`.

### 6.6 Preflight otomatis M4

Script berikut menggabungkan pemeriksaan minimum M0/M1/M2/M3. Jalankan sebelum menulis source M4.

```bash
# buat tools/scripts/m4_preflight.sh dari bagian source code
chmod +x tools/scripts/m4_preflight.sh
tools/scripts/m4_preflight.sh
```

Output minimum yang diharapkan:

```text
[M4][PASS] clang: ...
[M4][PASS] ld.lld: ...
[M4][PASS] readelf: ...
[M4][PASS] M0/M1/M2/M3 readiness minimum untuk M4 terpenuhi.
```

## 7. Source Code M4

Source berikut adalah baseline M4 yang telah diperiksa melalui kompilasi, link, audit ELF, audit symbol, dan audit disassembly di lingkungan penyusun panduan. Mahasiswa tetap wajib menjalankan ulang seluruh perintah di WSL 2 masing-masing karena runtime QEMU bergantung pada paket QEMU, OVMF, ISO tool, dan boot path setempat.

#### File `kernel/arch/x86_64/include/mcsos/arch/idt.h`

```c
#ifndef MCSOS_ARCH_IDT_H
#define MCSOS_ARCH_IDT_H

#include <stdint.h>

#define X86_64_IDT_VECTOR_COUNT 256u
#define X86_64_KERNEL_CODE_SELECTOR 0x28u
#define X86_64_IDT_GATE_INTERRUPT 0x8Eu
#define X86_64_IDT_GATE_TRAP 0x8Fu

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} x86_64_idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} x86_64_idtr_t;

typedef struct __attribute__((packed)) {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
} x86_64_trap_frame_t;

void x86_64_idt_init(void);
void x86_64_idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attributes);
void x86_64_trap_dispatch(x86_64_trap_frame_t *frame);
uint64_t x86_64_idt_base_for_test(void);
uint16_t x86_64_idt_limit_for_test(void);
void x86_64_trigger_breakpoint_for_test(void);

#endif
```
#### File `kernel/arch/x86_64/include/mcsos/arch/isr.h`

```c
#ifndef MCSOS_ARCH_ISR_H
#define MCSOS_ARCH_ISR_H

#include <stdint.h>

typedef void (*x86_64_isr_handler_t)(void);
extern x86_64_isr_handler_t x86_64_exception_stubs[32];

#endif
```
#### File `kernel/arch/x86_64/idt.c`

```c
#include <stdint.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/isr.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>

static x86_64_idt_entry_t idt[X86_64_IDT_VECTOR_COUNT];
static x86_64_idtr_t idtr;

static inline void lidt(const x86_64_idtr_t *descriptor) {
    __asm__ volatile ("lidt (%0)" :: "r"(descriptor) : "memory");
}

void x86_64_idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attributes) {
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFFu);
    idt[vector].selector = (uint16_t)X86_64_KERNEL_CODE_SELECTOR;
    idt[vector].ist = 0u;
    idt[vector].type_attributes = type_attributes;
    idt[vector].offset_mid = (uint16_t)((handler >> 16u) & 0xFFFFu);
    idt[vector].offset_high = (uint32_t)((handler >> 32u) & 0xFFFFFFFFu);
    idt[vector].reserved = 0u;
}

uint64_t x86_64_idt_base_for_test(void) {
    return idtr.base;
}

uint16_t x86_64_idt_limit_for_test(void) {
    return idtr.limit;
}

void x86_64_idt_init(void) {
    for (uint16_t i = 0u; i < X86_64_IDT_VECTOR_COUNT; ++i) {
        x86_64_idt_set_gate((uint8_t)i, 0u, 0u);
    }

    for (uint8_t vector = 0u; vector < 32u; ++vector) {
        uint8_t gate_type = X86_64_IDT_GATE_INTERRUPT;
        if (vector == 3u) {
            gate_type = X86_64_IDT_GATE_TRAP;
        }
        x86_64_idt_set_gate(vector, (uint64_t)(uintptr_t)x86_64_exception_stubs[vector], gate_type);
    }

    idtr.limit = (uint16_t)(sizeof(idt) - 1u);
    idtr.base = (uint64_t)(uintptr_t)&idt[0];

    KERNEL_ASSERT(sizeof(x86_64_idt_entry_t) == 16u);
    KERNEL_ASSERT(idtr.limit == (uint16_t)((X86_64_IDT_VECTOR_COUNT * sizeof(x86_64_idt_entry_t)) - 1u));
    lidt(&idtr);
    log_key_value_hex64("idt_base", idtr.base);
    log_key_value_hex64("idt_limit", (uint64_t)idtr.limit);
    log_writeln("[M4] IDT loaded");
}

void x86_64_trigger_breakpoint_for_test(void) {
    __asm__ volatile ("int3");
}
```
#### File `kernel/arch/x86_64/isr.S`

```asm
.section .text

.macro ISR_NOERR vector
.global isr_stub_\vector
.type isr_stub_\vector, @function
isr_stub_\vector:
    pushq $0
    pushq $\vector
    jmp isr_common
.size isr_stub_\vector, . - isr_stub_\vector
.endm

.macro ISR_ERR vector
.global isr_stub_\vector
.type isr_stub_\vector, @function
isr_stub_\vector:
    pushq $\vector
    jmp isr_common
.size isr_stub_\vector, . - isr_stub_\vector
.endm

.global isr_common
.type isr_common, @function
isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rdi
    call x86_64_trap_dispatch
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq
.size isr_common, . - isr_common

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR 29
ISR_ERR 30
ISR_NOERR 31

.section .rodata
.global x86_64_exception_stubs
.type x86_64_exception_stubs, @object
.align 8
x86_64_exception_stubs:
    .quad isr_stub_0
    .quad isr_stub_1
    .quad isr_stub_2
    .quad isr_stub_3
    .quad isr_stub_4
    .quad isr_stub_5
    .quad isr_stub_6
    .quad isr_stub_7
    .quad isr_stub_8
    .quad isr_stub_9
    .quad isr_stub_10
    .quad isr_stub_11
    .quad isr_stub_12
    .quad isr_stub_13
    .quad isr_stub_14
    .quad isr_stub_15
    .quad isr_stub_16
    .quad isr_stub_17
    .quad isr_stub_18
    .quad isr_stub_19
    .quad isr_stub_20
    .quad isr_stub_21
    .quad isr_stub_22
    .quad isr_stub_23
    .quad isr_stub_24
    .quad isr_stub_25
    .quad isr_stub_26
    .quad isr_stub_27
    .quad isr_stub_28
    .quad isr_stub_29
    .quad isr_stub_30
    .quad isr_stub_31
.size x86_64_exception_stubs, . - x86_64_exception_stubs
```
#### File `kernel/core/trap.c`

```c
#include <stdint.h>
#include <mcsos/arch/idt.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>

static const char *exception_names[32] = {
    "#DE Divide Error",
    "#DB Debug",
    "NMI Interrupt",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack Segment Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "Reserved",
    "#MF x87 Floating-Point Exception",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD Floating-Point Exception",
    "#VE Virtualization Exception",
    "#CP Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "#HV Hypervisor Injection Exception",
    "#VC VMM Communication Exception",
    "#SX Security Exception",
    "Reserved"
};

static uint64_t trap_count;

static const char *trap_name(uint64_t vector) {
    if (vector < 32u) {
        return exception_names[vector];
    }
    return "external-or-user-defined-interrupt";
}

uint64_t m4_trap_count_for_test(void) {
    return trap_count;
}

static void log_trap_frame(const x86_64_trap_frame_t *frame) {
    log_key_value_hex64("trap_vector", frame->vector);
    log_key_value_hex64("trap_error", frame->error_code);
    log_key_value_hex64("trap_rip", frame->rip);
    log_key_value_hex64("trap_cs", frame->cs);
    log_key_value_hex64("trap_rflags", frame->rflags);
    log_key_value_hex64("trap_rax", frame->rax);
    log_key_value_hex64("trap_rbx", frame->rbx);
    log_key_value_hex64("trap_rcx", frame->rcx);
    log_key_value_hex64("trap_rdx", frame->rdx);
}

void x86_64_trap_dispatch(x86_64_trap_frame_t *frame) {
    KERNEL_ASSERT(frame != (x86_64_trap_frame_t *)0);
    ++trap_count;

    log_write("[M4] trap dispatch: ");
    log_writeln(trap_name(frame->vector));
    log_trap_frame(frame);

    if (frame->vector == 3u) {
        log_writeln("[M4] breakpoint handled; returning with iretq");
        return;
    }

    KERNEL_PANIC("unrecoverable CPU exception", frame->vector);
}
```
#### File `kernel/core/kmain.c`

```c
#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/version.h>

extern char __kernel_start[];
extern char __kernel_end[];

static void m4_selftest(void) {
    KERNEL_ASSERT(__kernel_end > __kernel_start);
    KERNEL_ASSERT(sizeof(uintptr_t) == 8u);
    KERNEL_ASSERT(sizeof(x86_64_idt_entry_t) == 16u);
    KERNEL_ASSERT(x86_64_idt_base_for_test() != 0u);
    KERNEL_ASSERT(x86_64_idt_limit_for_test() == 4095u);
    log_writeln("[M4] selftest: IDT invariants passed");
}

void kmain(void) {
    log_init();
    log_write(MCSOS_NAME);
    log_write(" ");
    log_write(MCSOS_VERSION);
    log_write(" ");
    log_write(MCSOS_MILESTONE);
    log_writeln(" kernel entered");
    log_key_value_hex64("kernel_start", (uint64_t)(uintptr_t)__kernel_start);
    log_key_value_hex64("kernel_end", (uint64_t)(uintptr_t)__kernel_end);
    log_key_value_hex64("rflags_before_idt", cpu_read_rflags());

    x86_64_idt_init();
    m4_selftest();

#ifdef MCSOS_M4_TRIGGER_BREAKPOINT
    log_writeln("[M4] triggering intentional breakpoint exception");
    x86_64_trigger_breakpoint_for_test();
    log_writeln("[M4] returned from breakpoint handler");
#endif

#ifdef MCSOS_M4_TRIGGER_PANIC
    KERNEL_PANIC("intentional M4 panic test", 0x4D43534F533034u);
#else
    log_writeln("[M4] IDT and exception dispatch path installed");
    log_writeln("[M4] ready for QEMU smoke test and GDB audit");
    cpu_halt_forever();
#endif
}
```
#### File `kernel/include/mcsos/kernel/version.h`

```c
#ifndef MCSOS_KERNEL_VERSION_H
#define MCSOS_KERNEL_VERSION_H

#define MCSOS_NAME "MCSOS"
#define MCSOS_VERSION "260502"
#define MCSOS_MILESTONE "M4"
#define MCSOS_BUILD_PROFILE "teaching-qemu-x86_64"

#endif
```
#### File `Makefile`

```makefile
.RECIPEPREFIX := >
SHELL := /usr/bin/env bash

BUILD_DIR := build
KERNEL := $(BUILD_DIR)/kernel.elf
BP_KERNEL := $(BUILD_DIR)/kernel.breakpoint.elf
PANIC_KERNEL := $(BUILD_DIR)/kernel.panic.elf
MAP := $(BUILD_DIR)/kernel.map
BP_MAP := $(BUILD_DIR)/kernel.breakpoint.map
PANIC_MAP := $(BUILD_DIR)/kernel.panic.map
DISASM := $(BUILD_DIR)/kernel.disasm.txt
SYMS := $(BUILD_DIR)/kernel.syms.txt
CC := clang
LD := ld.lld
OBJDUMP := objdump
READELF := readelf
NM := nm

COMMON_CFLAGS := --target=x86_64-unknown-none-elf -std=c17 -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -fno-lto -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror -Ikernel/arch/x86_64/include -Ikernel/include
COMMON_ASFLAGS := --target=x86_64-unknown-none-elf -ffreestanding -fno-pic -fno-pie -m64 -mno-red-zone -Wall -Wextra -Werror -Ikernel/arch/x86_64/include -Ikernel/include
CFLAGS := $(COMMON_CFLAGS)
ASFLAGS := $(COMMON_ASFLAGS)
BP_CFLAGS := $(COMMON_CFLAGS) -DMCSOS_M4_TRIGGER_BREAKPOINT=1
PANIC_CFLAGS := $(COMMON_CFLAGS) -DMCSOS_M4_TRIGGER_PANIC=1
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T linker.ld
SRC_C := $(shell find kernel -name '*.c' | LC_ALL=C sort)
SRC_S := $(shell find kernel -name '*.S' | LC_ALL=C sort)
OBJ := $(patsubst %.c,$(BUILD_DIR)/normal/%.o,$(SRC_C)) $(patsubst %.S,$(BUILD_DIR)/normal/%.o,$(SRC_S))
BP_OBJ := $(patsubst %.c,$(BUILD_DIR)/breakpoint/%.o,$(SRC_C)) $(patsubst %.S,$(BUILD_DIR)/breakpoint/%.o,$(SRC_S))
PANIC_OBJ := $(patsubst %.c,$(BUILD_DIR)/panic/%.o,$(SRC_C)) $(patsubst %.S,$(BUILD_DIR)/panic/%.o,$(SRC_S))

.PHONY: all build breakpoint panic inspect audit clean distclean

all: build inspect

build: $(KERNEL)

breakpoint: $(BP_KERNEL)

panic: $(PANIC_KERNEL)

$(BUILD_DIR)/normal/%.o: %.c
>mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/normal/%.o: %.S
>mkdir -p $(dir $@)
>$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/breakpoint/%.o: %.c
>mkdir -p $(dir $@)
>$(CC) $(BP_CFLAGS) -c $< -o $@

$(BUILD_DIR)/breakpoint/%.o: %.S
>mkdir -p $(dir $@)
>$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/panic/%.o: %.c
>mkdir -p $(dir $@)
>$(CC) $(PANIC_CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic/%.o: %.S
>mkdir -p $(dir $@)
>$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJ) linker.ld
>mkdir -p $(BUILD_DIR)
>$(LD) $(LDFLAGS) -Map=$(MAP) -o $@ $(OBJ)

$(BP_KERNEL): $(BP_OBJ) linker.ld
>mkdir -p $(BUILD_DIR)
>$(LD) $(LDFLAGS) -Map=$(BP_MAP) -o $@ $(BP_OBJ)

$(PANIC_KERNEL): $(PANIC_OBJ) linker.ld
>mkdir -p $(BUILD_DIR)
>$(LD) $(LDFLAGS) -Map=$(PANIC_MAP) -o $@ $(PANIC_OBJ)

inspect: $(KERNEL)
>$(READELF) -h $(KERNEL) > $(BUILD_DIR)/kernel.readelf.header.txt
>$(READELF) -l $(KERNEL) > $(BUILD_DIR)/kernel.readelf.programs.txt
>$(NM) -n $(KERNEL) > $(SYMS)
>$(OBJDUMP) -d -Mintel $(KERNEL) > $(DISASM)
>grep -q 'ELF64' $(BUILD_DIR)/kernel.readelf.header.txt
>grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64' $(BUILD_DIR)/kernel.readelf.header.txt
>grep -q 'kmain' $(SYMS)
>grep -q 'x86_64_idt_init' $(SYMS)
>grep -q 'x86_64_trap_dispatch' $(SYMS)
>grep -q 'iretq' $(DISASM)
>grep -q 'lidt' $(DISASM)

audit: inspect breakpoint panic
>! $(NM) -u $(KERNEL) | grep .
>! $(NM) -u $(BP_KERNEL) | grep .
>! $(NM) -u $(PANIC_KERNEL) | grep .
>grep -q 'isr_stub_14' $(SYMS)
>grep -q 'x86_64_exception_stubs' $(SYMS)
>$(READELF) -S $(KERNEL) | grep -q '.text'
>$(READELF) -S $(KERNEL) | grep -q '.rodata'

clean:
>rm -rf $(BUILD_DIR)

distclean: clean
>rm -rf iso_root limine evidence
```
#### File `tools/scripts/m4_preflight.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

fail() { echo "[M4][FAIL] $*" >&2; exit 1; }
pass() { echo "[M4][PASS] $*"; }
warn() { echo "[M4][WARN] $*" >&2; }

[[ -d .git ]] || fail "Jalankan dari root repository Git MCSOS."
[[ -f linker.ld ]] || fail "linker.ld belum ada. Selesaikan M2/M3 terlebih dahulu."
[[ -f Makefile ]] || fail "Makefile belum ada. Selesaikan M1/M2/M3 terlebih dahulu."
[[ -f kernel/include/mcsos/kernel/log.h ]] || fail "Header log M3 tidak ditemukan."
[[ -f kernel/include/mcsos/kernel/panic.h ]] || fail "Header panic M3 tidak ditemukan."
[[ -f kernel/core/panic.c ]] || fail "panic.c M3 tidak ditemukan."
[[ -f kernel/core/serial.c ]] || fail "serial.c M3 tidak ditemukan."

command -v clang >/dev/null || fail "clang tidak ditemukan. Jalankan setup toolchain M1."
command -v ld.lld >/dev/null || fail "ld.lld tidak ditemukan. Jalankan setup toolchain M1."
command -v readelf >/dev/null || fail "readelf tidak ditemukan. Instal binutils."
command -v objdump >/dev/null || fail "objdump tidak ditemukan. Instal binutils."
command -v nm >/dev/null || fail "nm tidak ditemukan. Instal binutils."

if ! command -v qemu-system-x86_64 >/dev/null; then
  warn "qemu-system-x86_64 tidak ditemukan. Build dapat diuji, tetapi smoke test QEMU belum dapat dijalankan."
else
  pass "QEMU tersedia: $(qemu-system-x86_64 --version | head -n 1)"
fi

pass "clang: $(clang --version | head -n 1)"
pass "ld.lld: $(ld.lld --version | head -n 1)"
pass "readelf: $(readelf --version | head -n 1)"
pass "M0/M1/M2/M3 readiness minimum untuk M4 terpenuhi."
```
#### File `tools/scripts/m4_audit_elf.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

kernel="${1:-build/kernel.elf}"
[[ -f "$kernel" ]] || { echo "[M4][FAIL] kernel ELF tidak ditemukan: $kernel" >&2; exit 1; }
mkdir -p build
readelf -h "$kernel" > build/m4.readelf.header.txt
readelf -l "$kernel" > build/m4.readelf.programs.txt
readelf -S "$kernel" > build/m4.readelf.sections.txt
nm -n "$kernel" > build/m4.syms.txt
objdump -d -Mintel "$kernel" > build/m4.disasm.txt

grep -q 'ELF64' build/m4.readelf.header.txt
grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64' build/m4.readelf.header.txt
grep -q 'x86_64_idt_init' build/m4.syms.txt
grep -q 'x86_64_trap_dispatch' build/m4.syms.txt
grep -q 'x86_64_exception_stubs' build/m4.syms.txt
grep -q 'isr_stub_14' build/m4.syms.txt
grep -q 'lidt' build/m4.disasm.txt
grep -q 'iretq' build/m4.disasm.txt
! nm -u "$kernel" | grep .
echo "[M4][PASS] ELF, symbol, IDT, LIDT, dan IRETQ audit lulus untuk $kernel"
```
#### File `tools/scripts/m4_qemu_run.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

iso="${1:-build/mcsos.iso}"
log="${2:-build/m4-qemu-serial.log}"
[[ -f "$iso" ]] || { echo "[M4][FAIL] ISO tidak ditemukan: $iso" >&2; exit 1; }
command -v qemu-system-x86_64 >/dev/null || { echo "[M4][FAIL] qemu-system-x86_64 tidak ditemukan" >&2; exit 1; }
mkdir -p "$(dirname "$log")"

timeout 20s qemu-system-x86_64 \
  -machine q35 \
  -cpu max \
  -m 256M \
  -cdrom "$iso" \
  -boot d \
  -serial file:"$log" \
  -display none \
  -no-reboot \
  -no-shutdown || true

grep -q '\[M4\] IDT loaded' "$log" || { echo "[M4][FAIL] Log tidak menunjukkan IDT loaded" >&2; exit 1; }
grep -q '\[M4\] IDT and exception dispatch path installed' "$log" || { echo "[M4][FAIL] Log tidak menunjukkan milestone M4 siap uji" >&2; exit 1; }
echo "[M4][PASS] QEMU smoke test lulus. Log: $log"
```
#### File `tools/scripts/m4_collect_evidence.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

out="evidence/M4"
mkdir -p "$out"
cp -f build/kernel.elf "$out/" 2>/dev/null || true
cp -f build/kernel.map "$out/" 2>/dev/null || true
cp -f build/kernel.syms.txt "$out/" 2>/dev/null || true
cp -f build/kernel.disasm.txt "$out/" 2>/dev/null || true
cp -f build/kernel.readelf.header.txt "$out/" 2>/dev/null || true
cp -f build/kernel.readelf.programs.txt "$out/" 2>/dev/null || true
cp -f build/m4-qemu-serial.log "$out/" 2>/dev/null || true
{
  echo "MCSOS M4 evidence manifest"
  date -u +"timestamp_utc=%Y-%m-%dT%H:%M:%SZ"
  git rev-parse HEAD 2>/dev/null | sed 's/^/commit=/g' || true
  clang --version | head -n 1 | sed 's/^/clang=/g' || true
  ld.lld --version | head -n 1 | sed 's/^/lld=/g' || true
  qemu-system-x86_64 --version 2>/dev/null | head -n 1 | sed 's/^/qemu=/g' || true
  find "$out" -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort
} > "$out/manifest.txt"
echo "[M4][PASS] Evidence dikumpulkan di $out"
```
#### File `tools/scripts/grade_m4.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail
score=0
make clean >/dev/null
make audit >/dev/null
score=$((score + 60))
tools/scripts/m4_audit_elf.sh build/kernel.elf >/dev/null
score=$((score + 20))
if [[ -f build/m4-qemu-serial.log ]]; then
  grep -q '\[M4\]' build/m4-qemu-serial.log && score=$((score + 10))
fi
[[ -f evidence/M4/manifest.txt ]] && score=$((score + 10))
echo "M4_LOCAL_SCORE=$score/100"
```
#### File `tools/gdb_m4.gdb`

```gdb
set pagination off
set disassembly-flavor intel
file build/kernel.elf
target remote :1234
break kmain
break x86_64_idt_init
break x86_64_trap_dispatch
continue
```


## 8. Urutan Kerja Langkah demi Langkah

### Langkah 1 — Buat branch M4

Branch terpisah diperlukan agar perubahan IDT dan assembly stub tidak merusak baseline M3. Jalankan dari root repository:

```bash
git status --short
git switch -c m4-idt-exception-path
```

Jika branch sudah ada:

```bash
git switch m4-idt-exception-path
```

Indikator berhasil: `git branch --show-current` menampilkan `m4-idt-exception-path`.

### Langkah 2 — Jalankan preflight M4

Preflight memastikan artefak M0/M1/M2/M3 tersedia dan toolchain tidak hilang.

```bash
chmod +x tools/scripts/m4_preflight.sh
tools/scripts/m4_preflight.sh
```

Jika preflight gagal karena file M3 hilang, jangan lanjut ke M4. Pulihkan file M3 dari commit terakhir atau salin ulang source M3 dari panduan M3, lalu jalankan `make audit`.

### Langkah 3 — Tambahkan header IDT dan ISR

Buat dua header berikut:

```bash
mkdir -p kernel/arch/x86_64/include/mcsos/arch
$EDITOR kernel/arch/x86_64/include/mcsos/arch/idt.h
$EDITOR kernel/arch/x86_64/include/mcsos/arch/isr.h
```

Isi file harus identik dengan bagian source code. Perhatikan `__attribute__((packed))`, ukuran entry 16 byte, dan field trap frame. Jangan menukar urutan field trap frame karena dispatcher C bergantung pada urutan push di `isr.S`.

### Langkah 4 — Tambahkan implementasi IDT

Buat `kernel/arch/x86_64/idt.c`.

```bash
$EDITOR kernel/arch/x86_64/idt.c
```

File ini mengisi entry IDT, memuat IDTR dengan `lidt`, dan mencatat base/limit. Jika kernel triple fault setelah `lidt`, kemungkinan selector kode kernel salah. Baseline ini memakai `0x28` karena banyak konfigurasi Limine/GDT menggunakan selector tersebut untuk kode kernel 64-bit. Jika boot path Anda memakai GDT berbeda, sesuaikan `X86_64_KERNEL_CODE_SELECTOR` berdasarkan bukti GDT aktual, bukan tebakan.

### Langkah 5 — Tambahkan stub assembly exception

Buat `kernel/arch/x86_64/isr.S`.

```bash
$EDITOR kernel/arch/x86_64/isr.S
```

Periksa tiga hal: macro `ISR_NOERR`, macro `ISR_ERR`, dan `isr_common`. Exception seperti `#DF`, `#TS`, `#NP`, `#SS`, `#GP`, `#PF`, `#AC`, `#CP`, `#VC`, dan `#SX` memiliki error code, sehingga stub tidak boleh menambahkan error code nol. Untuk exception tanpa error code, stub menambahkan error code nol agar frame dispatcher seragam.

### Langkah 6 — Tambahkan dispatcher trap

Buat `kernel/core/trap.c`.

```bash
$EDITOR kernel/core/trap.c
```

Dispatcher mencetak vector, error code, `rip`, `cs`, `rflags`, dan beberapa register umum. Dispatcher hanya return untuk vector 3 (`#BP`). Untuk vector lain, dispatcher memanggil `KERNEL_PANIC`. Kebijakan ini disengaja agar kernel tidak mengulang fault yang tidak recoverable.

### Langkah 7 — Update `kmain.c`

Edit `kernel/core/kmain.c` agar `log_init` berjalan sebelum `x86_64_idt_init`, lalu jalankan selftest M4.

```bash
$EDITOR kernel/core/kmain.c
```

Urutan wajib:

```text
log_init -> log banner -> x86_64_idt_init -> m4_selftest -> optional int3 -> halt
```

Jika `x86_64_idt_init` dipanggil sebelum logging siap, bukti serial IDT tidak akan tersedia. Jika `int3` dipanggil sebelum IDT loaded, CPU dapat triple fault.

### Langkah 8 — Update Makefile agar file `.S` dibangun

M4 menambah source assembly. Makefile M3 yang hanya mengambil `*.c` tidak cukup. Pastikan Makefile memiliki `SRC_S`, rule kompilasi `%.S`, target `breakpoint`, target `panic`, dan audit `lidt`/`iretq`.

```bash
$EDITOR Makefile
```

Jalankan pemeriksaan cepat:

```bash
grep -n "SRC_S" Makefile
grep -n "%.S" Makefile
grep -n "breakpoint" Makefile
```

### Langkah 9 — Build varian normal

```bash
make clean
make build
```

Artefak yang harus muncul:

```text
build/kernel.elf
build/kernel.map
```

Jika build gagal pada `isr.S`, baca error assembler. Kesalahan paling umum adalah macro typo, simbol ganda, atau file disimpan dengan ekstensi `.s` bukan `.S`. Gunakan `.S` agar preprocessor/assembler clang bekerja konsisten.

### Langkah 10 — Build varian breakpoint dan panic

```bash
make breakpoint
make panic
```

Varian breakpoint menyisipkan `int3` untuk menguji handler `#BP`. Varian panic menguji integrasi M3 panic path setelah IDT loaded.

### Langkah 11 — Audit ELF dan disassembly

```bash
make inspect
tools/scripts/m4_audit_elf.sh build/kernel.elf
```

Pemeriksaan manual:

```bash
nm -n build/kernel.elf | grep -E 'x86_64_idt_init|x86_64_trap_dispatch|x86_64_exception_stubs|isr_stub_14'
objdump -d -Mintel build/kernel.elf | grep -E 'lidt|iretq' -n
nm -u build/kernel.elf
```

`nm -u` harus kosong. Jika ada `memcpy`, `memset`, `__stack_chk_fail`, atau symbol libc lain, berarti flags freestanding/runtime belum bersih.

### Langkah 12 — Buat ISO seperti M2/M3

Gunakan prosedur ISO yang sudah berjalan pada M2/M3. Jika repository memakai script `m2_make_iso.sh` atau target `iso`, gunakan target tersebut. Contoh umum:

```bash
make iso
ls -lh build/*.iso
```

Jika belum ada target ISO, gunakan kembali script M2. Jangan mengubah bootloader sebelum M4 lulus karena perubahan boot path akan mencampur dua sumber bug: boot failure dan trap failure.

### Langkah 13 — Jalankan QEMU smoke test normal

```bash
tools/scripts/m4_qemu_run.sh build/mcsos.iso build/m4-qemu-serial.log
sed -n '1,120p' build/m4-qemu-serial.log
```

Log minimum:

```text
MCSOS 260502 M4 kernel entered
idt_base=...
idt_limit=0x0000000000000fff
[M4] IDT loaded
[M4] selftest: IDT invariants passed
[M4] IDT and exception dispatch path installed
```

Jika log berhenti tepat setelah `kernel entered`, debug area `lidt` atau selector gate. Jika QEMU reboot tanpa log, curigai triple fault sebelum serial flush.

### Langkah 14 — Jalankan QEMU smoke test varian breakpoint

Bangun ISO dengan `build/kernel.breakpoint.elf` sesuai mekanisme ISO repository Anda. Jika target ISO hanya mengambil `build/kernel.elf`, salin sementara:

```bash
cp build/kernel.breakpoint.elf build/kernel.elf
make iso
tools/scripts/m4_qemu_run.sh build/mcsos.iso build/m4-qemu-breakpoint.log || true
sed -n '1,160p' build/m4-qemu-breakpoint.log
```

Log yang diharapkan:

```text
[M4] triggering intentional breakpoint exception
[M4] trap dispatch: #BP Breakpoint
trap_vector=0x0000000000000003
[M4] breakpoint handled; returning with iretq
[M4] returned from breakpoint handler
```

Jika `trap_vector` bukan 3, urutan push `vector`/`error_code` di `isr.S` tidak cocok dengan `x86_64_trap_frame_t`.

### Langkah 15 — Jalankan GDB debug path

Di terminal pertama:

```bash
qemu-system-x86_64   -machine q35   -cpu max   -m 256M   -cdrom build/mcsos.iso   -boot d   -serial stdio   -display none   -no-reboot   -no-shutdown   -S -s
```

Di terminal kedua:

```bash
gdb -q -x tools/gdb_m4.gdb
```

Perintah GDB yang harus dicoba:

```gdb
info registers
break x86_64_idt_init
break x86_64_trap_dispatch
continue
disassemble isr_common
x/16gx &x86_64_exception_stubs
```

Bukti yang dikumpulkan: screenshot atau log terminal yang menunjukkan breakpoint pada `x86_64_idt_init` dan `x86_64_trap_dispatch`.

### Langkah 16 — Jalankan grading lokal

```bash
chmod +x tools/scripts/grade_m4.sh
tools/scripts/grade_m4.sh
```

Nilai lokal bukan nilai final dosen. Nilai lokal hanya memeriksa build/audit/evidence minimum.

### Langkah 17 — Kumpulkan evidence

```bash
tools/scripts/m4_collect_evidence.sh
find evidence/M4 -maxdepth 1 -type f -printf '%f
' | sort
```

Evidence minimum:

```text
kernel.elf
kernel.map
kernel.syms.txt
kernel.disasm.txt
kernel.readelf.header.txt
kernel.readelf.programs.txt
manifest.txt
m4-qemu-serial.log
```

### Langkah 18 — Commit hasil M4

```bash
git status --short
git add Makefile linker.ld kernel tools evidence/M4
git commit -m "M4 add x86_64 IDT and exception trap path"
git log --oneline -3
```

Commit wajib berisi source, script, dan evidence yang cukup. Jangan commit direktori Limine hasil download besar jika repository sebelumnya tidak menstandarkan vendoring bootloader.

## 9. Failure Modes dan Solusi Perbaikan

### 9.1 Build gagal: `x86_64_exception_stubs` undefined

Penyebab paling umum adalah `isr.S` tidak ikut dikompilasi. Perbaiki Makefile agar memiliki `SRC_S` dan rule `$(BUILD_DIR)/normal/%.o: %.S`.

```bash
grep -n "SRC_S" Makefile
find kernel -name '*.S' -print
make clean && make build
```

### 9.2 Build gagal karena assembler macro

Periksa bahwa macro memakai syntax GNU assembler yang diterima clang. File harus berekstensi `.S`, bukan `.asm`. Hindari komentar non-ASCII pada source assembly bila toolchain lokal bermasalah.

```bash
clang --target=x86_64-unknown-none-elf -ffreestanding -m64 -mno-red-zone -c kernel/arch/x86_64/isr.S -o /tmp/isr.o
```

### 9.3 `sizeof(x86_64_idt_entry_t)` bukan 16

Pastikan struct memakai `__attribute__((packed))` dan field sesuai urutan. Jangan memakai pointer function langsung di dalam descriptor. Descriptor harus dipisah menjadi `offset_low`, `offset_mid`, dan `offset_high`.

### 9.4 QEMU reboot setelah `lidt`

Kemungkinan: selector kode kernel salah, gate tidak present, handler address salah, atau IDT berada di alamat tidak valid. Lakukan audit:

```bash
nm -n build/kernel.elf | grep -E 'idt|isr_stub_3|isr_stub_14'
objdump -d -Mintel build/kernel.elf | grep -n "lidt"
```

Jika selector `0x28` tidak cocok dengan bootloader/GDT Anda, cek konfigurasi GDT yang diberikan boot path dan sesuaikan `X86_64_KERNEL_CODE_SELECTOR`.

### 9.5 Breakpoint tidak kembali ke kernel

Jika log menunjukkan `trap dispatch: #BP` tetapi tidak ada `returned from breakpoint handler`, periksa `iretq`, urutan pop register, dan `addq $16, %rsp`. Untuk vector `#BP`, stack harus dibersihkan dari `vector` dan `error_code` sebelum `iretq`.

### 9.6 `trap_vector` salah

Penyebab: urutan push di assembly tidak sesuai dengan struct. Pastikan untuk exception tanpa error code urutannya:

```asm
pushq $0
pushq $vector
jmp isr_common
```

Untuk exception dengan error code:

```asm
pushq $vector
jmp isr_common
```

### 9.7 Page fault loop

Jika Anda mencoba menguji `#PF` dengan akses memori ilegal lalu handler melakukan return, CPU akan mengeksekusi instruksi yang sama dan fault lagi. Untuk M4, page fault harus masuk panic. Recovery page fault baru dibahas pada milestone virtual memory.

### 9.8 `nm -u` menampilkan `__stack_chk_fail`

Penyebab: stack protector aktif. Pastikan CFLAGS berisi `-fno-stack-protector` dan tidak ada flags default distro yang masuk dari environment.

```bash
echo "$CFLAGS"
make clean && make build V=1
```

### 9.9 Serial log kosong

Penyebab: QEMU tidak diarahkan ke serial, kernel crash sebelum log, atau port COM1 tidak terinisialisasi. Jalankan:

```bash
qemu-system-x86_64 ... -serial stdio -display none -no-reboot -no-shutdown
```

Jika tetap kosong, pasang breakpoint GDB di `kmain` dan `log_init`.

### 9.10 GDB tidak berhenti di `x86_64_trap_dispatch`

Pastikan ISO memakai kernel varian breakpoint. Jika ISO masih memakai kernel normal, `int3` tidak pernah dipicu. Cek symbol dan checksum kernel yang masuk ISO.

```bash
nm -n build/kernel.breakpoint.elf | grep x86_64_trigger_breakpoint_for_test
sha256sum build/kernel.breakpoint.elf build/kernel.elf
```

## 10. Checkpoint Buildable

| Checkpoint | Perintah | Kriteria lulus |
|---|---|---|
| M4-C1 | `tools/scripts/m4_preflight.sh` | Toolchain dan baseline M0–M3 terdeteksi. |
| M4-C2 | `make clean && make build` | `build/kernel.elf` berhasil dibuat. |
| M4-C3 | `make breakpoint && make panic` | Dua varian kernel tambahan berhasil dibuat. |
| M4-C4 | `make inspect` | Header ELF, program header, symbol, disassembly dibuat. |
| M4-C5 | `tools/scripts/m4_audit_elf.sh build/kernel.elf` | `lidt`, `iretq`, `x86_64_trap_dispatch`, stub exception terdeteksi. |
| M4-C6 | `tools/scripts/m4_qemu_run.sh build/mcsos.iso` | Serial log menunjukkan IDT loaded dan M4 ready. |
| M4-C7 | GDB `break x86_64_trap_dispatch` | GDB dapat berhenti di dispatcher untuk varian breakpoint. |
| M4-C8 | `tools/scripts/m4_collect_evidence.sh` | Evidence M4 tersimpan. |

## 11. Tugas Implementasi Mahasiswa

### 11.1 Tugas wajib

1. Menambahkan source M4 sesuai panduan.
2. Memperbarui Makefile agar membangun file C dan `.S`.
3. Menjalankan preflight M4.
4. Membangun kernel normal, breakpoint, dan panic.
5. Menjalankan audit ELF dan disassembly.
6. Membuat ISO dan menjalankan QEMU smoke test.
7. Menjalankan GDB sampai dapat berhenti di `x86_64_idt_init` dan `x86_64_trap_dispatch`.
8. Mengumpulkan evidence ke `evidence/M4`.
9. Menjawab pertanyaan analisis dan menulis laporan dengan template praktikum.

### 11.2 Tugas pengayaan

1. Tambahkan counter per-vector, bukan hanya total trap count.
2. Tambahkan nama exception dan status recoverable/non-recoverable dalam tabel statis.
3. Tambahkan dump register `r8` sampai `r15` ke log.
4. Tambahkan guard agar vector di luar 0–31 selalu panic.
5. Tambahkan build target `fault-int3` yang otomatis membuat ISO varian breakpoint.

### 11.3 Tantangan riset

1. Rancang IST untuk double fault dan jelaskan mengapa double fault sebaiknya memakai stack terpisah.
2. Rancang strategi awal untuk page fault handler pada milestone virtual memory.
3. Bandingkan interrupt gate vs trap gate untuk `#BP`, `#DB`, dan exception non-recoverable.
4. Buat model state machine formal untuk trap dispatch dan buktikan bahwa `#BP` adalah satu-satunya path return pada M4.

## 12. Bukti yang Harus Dikumpulkan

Mahasiswa wajib mengumpulkan:

1. Commit hash M4.
2. Output `tools/scripts/m4_preflight.sh`.
3. Output `make clean && make audit`.
4. Output `tools/scripts/m4_audit_elf.sh build/kernel.elf`.
5. Output `readelf -h build/kernel.elf`.
6. Output `nm -n build/kernel.elf | grep -E 'idt|trap|isr_stub'`.
7. Potongan `objdump` yang menunjukkan `lidt` dan `iretq`.
8. Serial log QEMU normal.
9. Serial log QEMU varian breakpoint bila berhasil dibuat.
10. Screenshot/log GDB pada `x86_64_idt_init` dan `x86_64_trap_dispatch`.
11. Isi `evidence/M4/manifest.txt`.
12. Laporan praktikum lengkap.

## 13. Pertanyaan Analisis

1. Mengapa entry IDT 64-bit harus 16 byte?
2. Mengapa IDTR `limit` berisi ukuran tabel dikurangi satu?
3. Mengapa beberapa exception memiliki error code dan sebagian lain tidak?
4. Mengapa M4 menormalisasi error code ke nol untuk exception tanpa error code?
5. Mengapa `#BP` dipilih sebagai uji recoverable?
6. Mengapa page fault tidak boleh langsung dikembalikan pada M4?
7. Apa risiko jika urutan push register di assembly tidak sama dengan urutan field `x86_64_trap_frame_t`?
8. Apa akibat jika selector kode kernel pada IDT gate salah?
9. Mengapa kernel memakai `-mno-red-zone`?
10. Mengapa `nm -u` harus kosong untuk kernel freestanding?
11. Bagaimana cara membedakan boot failure, triple fault, dan exception handler bug dari log QEMU/GDB?
12. Apa bukti minimum sebelum M4 boleh disebut siap uji QEMU?

## 14. Rubrik Penilaian 100 Poin

| Komponen | Poin | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | IDT terisi, `lidt` dieksekusi, stub 0–31 tersedia, dispatcher bekerja, `#BP` dapat ditangani. |
| Kualitas desain dan invariants | 20 | Struct benar, frame konsisten, error-code handling jelas, non-recoverable exception fail-closed. |
| Pengujian dan bukti | 20 | Build normal/breakpoint/panic, audit ELF, disassembly, QEMU log, GDB evidence, manifest. |
| Debugging/failure analysis | 10 | Failure modes dianalisis dan solusi perbaikan tepat. |
| Keamanan dan robustness | 10 | Tidak return dari fault berbahaya, tidak bergantung libc, warning dianggap error, log cukup untuk triase. |
| Dokumentasi/laporan | 10 | Laporan lengkap, command reproducible, screenshot/log cukup, referensi IEEE. |

## 15. Kriteria Lulus Praktikum

Mahasiswa lulus M4 bila memenuhi semua syarat minimum berikut:

1. Repository dapat dibangun dari clean checkout.
2. `tools/scripts/m4_preflight.sh` lulus atau semua peringatannya dijelaskan.
3. `make clean && make audit` lulus.
4. `build/kernel.elf`, `build/kernel.map`, `build/kernel.syms.txt`, dan `build/kernel.disasm.txt` tersedia.
5. `nm -u build/kernel.elf` kosong.
6. `objdump` menunjukkan instruksi `lidt` dan `iretq`.
7. Symbol `x86_64_idt_init`, `x86_64_trap_dispatch`, `x86_64_exception_stubs`, dan minimal `isr_stub_14` ditemukan.
8. QEMU normal boot menghasilkan serial log yang menunjukkan `[M4] IDT loaded`.
9. Varian breakpoint menghasilkan log trap vector 3 atau, jika belum berhasil, mahasiswa memberikan analisis GDB yang valid.
10. Panic path M3 tetap terbaca.
11. Perubahan Git dikomit.
12. Laporan berisi bukti, analisis, failure modes, dan readiness review.

## 16. Prosedur Rollback

Jika M4 menyebabkan kernel tidak boot, rollback secara bertahap.

### 16.1 Rollback source M4 saja

```bash
git status --short
git restore kernel/arch/x86_64/idt.c kernel/arch/x86_64/isr.S kernel/core/trap.c
git restore kernel/arch/x86_64/include/mcsos/arch/idt.h kernel/arch/x86_64/include/mcsos/arch/isr.h
git restore kernel/core/kmain.c Makefile
make clean && make audit
```

### 16.2 Kembali ke commit M3

```bash
git log --oneline --decorate -10
git switch main
git switch -c rollback-before-m4 <COMMIT_M3>
make clean && make audit
```

### 16.3 Menonaktifkan uji breakpoint tanpa menghapus IDT

Jika hanya varian breakpoint bermasalah, gunakan kernel normal tanpa macro `MCSOS_M4_TRIGGER_BREAKPOINT`.

```bash
make clean && make build
make iso
tools/scripts/m4_qemu_run.sh build/mcsos.iso build/m4-normal.log
```

## 17. Template Laporan Praktikum M4

Gunakan template laporan umum praktikum OS, lalu isi khusus M4 sebagai berikut:

1. **Sampul**: judul M4, nama, NIM, kelas, dosen Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.
2. **Tujuan**: membangun IDT, exception stub, trap frame, dan dispatcher awal.
3. **Dasar teori ringkas**: IDT, IDTR, gate descriptor, exception vector, error code, `lidt`, `iretq`, dan trap frame.
4. **Lingkungan**: Windows 11, WSL distro, versi clang, lld, QEMU, binutils, commit hash.
5. **Desain**: diagram alur `kmain -> idt_init -> int3 -> isr_stub_3 -> trap_dispatch -> iretq`.
6. **Invariants**: ukuran IDT entry, IDTR limit, frame layout, fail-closed policy.
7. **Langkah kerja**: perintah yang dijalankan dan perubahan file.
8. **Hasil uji**: output build, audit, QEMU log, GDB log, screenshot.
9. **Analisis**: keberhasilan, bug, penyebab, solusi, dan kaitan dengan teori.
10. **Keamanan dan reliability**: risiko return dari exception, triple fault, logging pointer, dan mitigasi.
11. **Kesimpulan**: status readiness dan batasan M4.
12. **Lampiran**: diff, log penuh, disassembly penting, manifest evidence, dan referensi.

## 18. Readiness Review Akhir M4

| Aspek | Status yang boleh diklaim bila bukti lengkap |
|---|---|
| Build dan link | Siap audit toolchain M4. |
| ELF dan disassembly | Siap review IDT/trap assembly. |
| QEMU normal | Siap uji QEMU bila serial log menunjukkan IDT loaded. |
| Breakpoint exception | Siap demonstrasi praktikum bila `#BP` masuk dispatcher dan kembali. |
| Exception non-recoverable | Siap uji panic fail-closed, bukan siap recovery. |
| Hardware nyata | Belum siap bring-up hardware; perlu GDT/IDT/IST/APIC/timer review lebih lanjut. |
| Production readiness | Tidak boleh diklaim. |

Kesimpulan konservatif: M4 menghasilkan **kandidat siap uji QEMU untuk IDT dan exception path awal**. M4 belum memenuhi syarat untuk hardware bring-up umum, IRQ eksternal, scheduler preemption, syscall, user mode, atau recovery page fault.

## 19. Catatan Verifikasi Source oleh Penyusun Panduan

Source M4 dalam panduan ini telah diperiksa di lingkungan penyusun dengan hasil berikut:

```text
make -C /mnt/data/m4_verify audit
# lulus: build kernel normal, breakpoint, panic; nm -u kosong; symbol IDT/trap/stub tersedia; disassembly memuat lidt dan iretq.

tools/scripts/m4_audit_elf.sh build/kernel.elf
# lulus: ELF64 x86_64, x86_64_idt_init, x86_64_trap_dispatch, x86_64_exception_stubs, isr_stub_14, lidt, iretq.
```

Batas verifikasi: container penyusun memiliki compiler/linker/binutils, tetapi tidak memiliki QEMU/OVMF/ISO runtime. Karena itu, validasi runtime QEMU wajib dijalankan di WSL 2 mahasiswa. Klaim final M4 harus berdasarkan bukti lokal mahasiswa, bukan hanya keberhasilan kompilasi dalam panduan ini.

## 20. References

[1] Intel Corporation, “Intel® 64 and IA-32 Architectures Software Developer Manuals,” Intel, 2026. [Online]. Available: Intel Developer Manuals page. Accessed: May 2026.

[2] QEMU Project, “QEMU System Emulation Invocation,” QEMU Documentation, 2026. [Online]. Available: QEMU system invocation documentation. Accessed: May 2026.

[3] QEMU Project, “GDB usage / gdbstub,” QEMU Documentation, 2026. [Online]. Available: QEMU gdbstub documentation. Accessed: May 2026.

[4] Free Software Foundation, “GNU ld Linker Scripts,” GNU Binutils Documentation, 2026. [Online]. Available: GNU Binutils ld documentation. Accessed: May 2026.

[5] LLVM Project, “Clang Command Guide and Driver Documentation,” LLVM Documentation, 2026. [Online]. Available: LLVM Clang documentation. Accessed: May 2026.

[6] LLVM Project, “LLD ELF Linker,” LLVM Documentation, 2026. [Online]. Available: LLVM LLD documentation. Accessed: May 2026.

[7] Limine Project, “Limine Documentation,” Limine, 2026. [Online]. Available: Limine documentation. Accessed: May 2026.

[8] Microsoft, “Install WSL,” Microsoft Learn, 2026. [Online]. Available: Microsoft Learn WSL installation documentation. Accessed: May 2026.
