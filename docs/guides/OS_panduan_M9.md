# OS_panduan_M9.md

# Panduan Praktikum M9 — Kernel Thread, Runqueue Round-Robin Kooperatif, Context Switch x86_64, dan Integrasi Scheduler Awal pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M9  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: *siap uji QEMU untuk kernel thread dan scheduler awal single-core*, bukan siap produksi dan bukan bukti bahwa scheduler aman untuk SMP, user process, atau hardware umum.

---

## 1. Ringkasan Praktikum

Praktikum M9 melanjutkan fondasi M0 sampai M8. Pada M5 mahasiswa telah mempunyai interrupt dan timer tick awal. Pada M6 mahasiswa telah membuat Physical Memory Manager. Pada M7 mahasiswa telah membuat Virtual Memory Manager awal. Pada M8 mahasiswa telah membuat kernel heap awal. M9 memakai seluruh fondasi tersebut untuk membangun unit eksekusi kernel yang dapat dijadwalkan: **kernel thread**, **runqueue**, **scheduler round-robin kooperatif**, dan **context switch x86_64**.

M9 sengaja tidak langsung membuat proses user, privilege transition ke ring 3, `syscall/sysret`, atau loader ELF userspace. Tahap tersebut berisiko lebih tinggi karena melibatkan validasi pointer user, address space per-proses, TSS/IST, segment selector user, page table isolation, dan ABI syscall. M9 dibatasi pada **thread kernel single-core** agar mahasiswa dapat memverifikasi invariant dasar scheduler terlebih dahulu: state transition thread, kepemilikan stack, antrian runnable, pemilihan thread berikutnya, penyimpanan register callee-saved, dan jalur debug jika context switch gagal.

Intel SDM mendokumentasikan lingkungan dukungan sistem x86_64, termasuk memory management, protection, task management, interrupt/exception handling, multiprocessor support, dan debugging [1]. Untuk boundary assembly-ke-C, M9 memakai subset konservatif dari x86-64 psABI, terutama konsep register dan stack frame yang relevan untuk pemanggilan fungsi C pada x86_64 [2]. QEMU gdbstub dipakai sebagai jalur debug karena QEMU mendukung koneksi remote GDB untuk menghentikan guest, memeriksa register/memori, dan memasang breakpoint/watchpoint [3]. Clang dan GNU binutils dipakai untuk kompilasi freestanding, audit ELF, dan inspeksi disassembly [4], [5]. Dokumentasi Linux scheduler dipakai hanya sebagai pembanding konseptual bahwa scheduler produksi memiliki kelas scheduling, fairness model, dan struktur data lebih kompleks; MCSOS M9 tetap memakai round-robin sederhana supaya invariant mudah diaudit [6].

Keberhasilan M9 tidak boleh dinyatakan sebagai “tanpa error” atau “siap produksi”. Kriteria minimum M9 adalah source code scheduler dapat dikompilasi sebagai C17 freestanding, assembly context switch dapat dirakit untuk target x86_64 ELF, host unit test runqueue lulus, object gabungan tidak memiliki unresolved symbol, audit `readelf` menunjukkan ELF64 x86_64 relocatable object, dan integrasi kernel menghasilkan log serial yang menunjukkan inisialisasi scheduler serta minimal satu perpindahan thread terkontrol. Validasi runtime QEMU/OVMF tetap harus dijalankan ulang di lingkungan WSL 2 mahasiswa karena paket QEMU, OVMF, bootloader, layout kernel, dan versi toolchain dapat berbeda.

---

## 2. Asumsi Target dan Batasan

| Aspek | Keputusan M9 |
|---|---|
| Arsitektur | x86_64 long mode |
| Lingkungan host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Boot path | Melanjutkan pipeline M2–M8; direkomendasikan Limine/UEFI atau ISO yang sudah lulus M2 |
| Bahasa | C17 freestanding + assembly x86_64 untuk context switch |
| Toolchain | Clang/LLD atau GCC/binutils; contoh validasi memakai Clang dan GNU binutils tools |
| Fondasi wajib | M5 timer/interrupt, M6 PMM, M7 VMM, M8 kernel heap |
| Unit wajib | Kernel thread, runqueue FIFO, round-robin kooperatif, context switch callee-saved register |
| Mode scheduling | Single-core cooperative yield; preemption penuh adalah pengayaan terkendali |
| Stack thread | Kernel stack per-thread yang dialokasikan melalui heap atau arena statik valid |
| IRQ context | Scheduler lock/preemption disable harus eksplisit sebelum integrasi timer preemption |
| User mode | Belum diimplementasikan pada M9 |
| Out of scope | Ring 3, `syscall/sysret`, ELF user loader, address space per-proses, SMP scheduler, priority scheduler, CFS/EEVDF, real-time scheduling, signal, wait/exit proses, dan IPC penuh |

### 2A. Goals dan Non-Goals

**Goals** M9 adalah membangun thread kernel awal yang dapat dijadwalkan, mendefinisikan invariant state-machine scheduler, menyediakan context switch x86_64 kecil yang dapat diaudit, dan menyediakan host unit test untuk logika runqueue. **Non-goals** M9 adalah scheduler produksi, fairness kompleks, SMP, real-time, userspace process, syscall ABI, dan isolasi user/kernel.

### 2B. Assumptions / Asumsi Implementasi

Asumsi M9 adalah: CPU berada pada x86_64 long mode; stack kernel aktif valid; paging dari M7 tidak merusak area stack; heap dari M8 dapat menyediakan objek thread atau mahasiswa memakai array statik untuk tahap bootstrap; interrupts dapat dimatikan sementara ketika runqueue dimodifikasi; dan context switch hanya dipanggil dari kernel context biasa, bukan langsung dari handler interrupt sebelum desain preemption disahkan.

Jika salah satu asumsi tersebut tidak terpenuhi, hasil yang benar adalah panic atau log kegagalan yang dapat ditelusuri, bukan eksekusi lanjut dengan runqueue korup. Scheduler adalah subsistem yang mudah menghasilkan bug laten: satu pointer `next` korup dapat menyebabkan infinite loop, lompat ke alamat invalid, stack overlap, atau return ke register yang salah.

### 2C. Scope dan Target Matrix

| Scope | Target wajib | Target pengayaan | Non-scope |
|---|---|---|---|
| Thread | Kernel thread dengan TCB | Thread exit/join sederhana | Process user |
| Scheduler | FIFO runqueue + round-robin kooperatif | Timer-driven need-resched | CFS/EEVDF/SMP |
| Context switch | Simpan/restore callee-saved + `rsp/rip` | Debug dump context | FPU/SSE/AVX context |
| Test | Host unit test + freestanding object audit | QEMU smoke test dua thread | Hardware umum |
| Integrasi | Log scheduler di serial | Timer tick accounting | Syscall ABI |

### 2D. Toolchain BOM, Reproducibility, CI, dan Supply Chain

Toolchain bill of materials minimum adalah Clang atau GCC, LLD atau GNU ld, GNU Make, GNU binutils (`nm`, `readelf`, `objdump`), QEMU, GDB, dan Git. Reproducibility minimum dibuktikan dengan clean rebuild `make m9-clean && make m9-all`, log versi tool, dan commit hash. Untuk CI, target minimum yang dapat dibuat adalah job `m9-host-test`, `m9-freestanding`, dan `m9-audit`. Untuk supply chain, artefak praktikum yang dikumpulkan sebaiknya disertai checksum, misalnya `sha256sum build/m9/m9_host_test build/m9/m9_scheduler_combined.o`.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M9, mahasiswa mampu:

1. Menjelaskan perbedaan thread kernel, proses, CPU context, dan scheduler.
2. Menjelaskan alasan setiap thread memerlukan kernel stack sendiri.
3. Mendesain Thread Control Block atau TCB dengan state, context, stack metadata, entry function, dan linkage runqueue.
4. Menetapkan invariant scheduler: satu thread hanya boleh berada pada satu state aktif, thread running tidak boleh ada di runqueue, ready list tidak boleh mengandung pointer siklik, dan `runnable_count` harus sama dengan jumlah node runqueue.
5. Mengimplementasikan round-robin kooperatif single-core dengan operasi enqueue, pick next, yield, block, dan mark ready.
6. Mengimplementasikan context switch x86_64 yang menyimpan `rsp`, `rbp`, `rbx`, `r12`–`r15`, dan continuation `rip`.
7. Menyusun host unit test untuk logika scheduler yang tidak bergantung pada QEMU.
8. Melakukan audit object freestanding dengan `nm`, `readelf`, dan `objdump`.
9. Menjelaskan failure modes scheduler seperti stack overlap, double enqueue, context corruption, lost wakeup, dan interrupt race.
10. Menulis laporan praktikum dengan bukti build, test, log, disassembly, analisis bug, rollback, dan readiness review.

---

## 4. Prasyarat Teori

Mahasiswa harus memahami materi berikut sebelum mengubah source code:

| Materi | Kebutuhan M9 |
|---|---|
| State machine | Untuk membuktikan transisi `NEW -> READY -> RUNNING -> BLOCKED/ZOMBIE` |
| Struktur data queue | Untuk runqueue FIFO dan invariant head/tail |
| ABI x86_64 | Untuk register yang harus disimpan saat pindah konteks |
| Stack discipline | Untuk mempersiapkan stack thread baru dan alignment |
| Interrupt/timer | Untuk integrasi tick accounting dan opsi preemption pada tahap pengayaan |
| Memory ownership | Untuk memastikan stack thread tidak tumpang tindih dan tidak dilepas saat masih running |
| Debugging QEMU/GDB | Untuk memeriksa `rsp`, `rip`, symbol context switch, dan panic path |

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M9 |
|---|---|
| `@osdev-general` | Gate, roadmap, readiness, dan batas klaim hasil praktikum |
| `@osdev-01-computer-foundation` | State machine, invariant runqueue, proof obligation, dan complexity bound |
| `@osdev-02-low-level-programming` | ABI, assembly, stack alignment, freestanding C, dan audit disassembly |
| `@osdev-03-computer-and-hardware-architecture` | x86_64 privilege state, interrupt/timer, dan register semantics |
| `@osdev-04-kernel-development` | Thread lifecycle, scheduler, blocking, yield, dan observability kernel |
| `@osdev-07-os-security` | Batas privilege, risiko corrupt context, dan validasi pointer internal kernel |
| `@osdev-12-toolchain-devenv` | Build, host test, freestanding object, `nm/readelf/objdump`, dan QEMU/GDB workflow |
| `@osdev-14-cross-science` | Requirements, verification matrix, risk register, dan evidence-based readiness |

---

## 6. Pemeriksaan Kesiapan Hasil Praktikum Sebelumnya

Pemeriksaan kesiapan M0–M8 wajib dilakukan sebelum source M9 ditulis. Tujuannya bukan mengulang seluruh praktikum, melainkan memastikan fondasi yang menjadi dependency langsung M9 tidak korup. Jika satu dependency gagal, M9 harus dihentikan, diperbaiki, dan diulang dari checkpoint terakhir yang stabil.

### 6.1 Checklist Kesiapan M0–M8

| Tahap | Bukti minimum | Gejala gagal | Solusi perbaikan |
|---|---|---|---|
| M0 | WSL 2 aktif, Git, Make, Clang/GCC, QEMU, GDB tersedia | Tool tidak ditemukan atau versi tidak tercatat | Jalankan ulang setup M0 dan simpan log versi |
| M1 | Build freestanding proof lulus, audit ELF64 x86_64 tersedia | Object salah target atau ada runtime libc tersembunyi | Periksa target triple, `-ffreestanding`, `-fno-stack-protector`, dan linker flags |
| M2 | Kernel boot ke early console/panic path | Bootloader tidak menemukan kernel atau hang tanpa log | Periksa ISO layout, Limine/UEFI config, serial log, dan entry symbol |
| M3 | Panic path dan logging deterministic | Panic tidak terlihat atau log serial kosong | Periksa early console, serial port, dan path `panic()` |
| M4 | IDT exception handler bekerja | Triple fault saat exception | Audit IDT descriptor, stub, error-code handling, dan `iretq` |
| M5 | IRQ0/PIT tick dan EOI bekerja | Interrupt storm atau timer tidak bertambah | Periksa PIC remap, mask IRQ, EOI, dan interrupt gate |
| M6 | PMM alloc/free frame lulus invariant | Frame double-free atau alokasi melewati usable memory | Jalankan host test PMM dan validasi memory map |
| M7 | VMM map/query/unmap dan page fault diagnostic bekerja | Page fault tanpa CR2/error code atau mapping salah | Audit flags page table, HHDM, `invlpg`, dan handler #PF |
| M8 | Kernel heap first-fit lulus host test | `kmem_alloc` null, double-free, coalesce gagal | Jalankan `make m8-all`, cek alignment dan arena heap |

### 6.2 Perintah Pemeriksaan Cepat

Jalankan perintah berikut dari root repository. Perintah ini mengumpulkan bukti status Git, versi toolchain, dan artefak M8. Output harus disimpan ke `evidence/m9/preflight_m9.log`.

```bash
mkdir -p evidence/m9
{
  echo "== git =="
  git rev-parse --show-toplevel
  git rev-parse --short HEAD
  git status --short
  echo
  echo "== tools =="
  clang --version || true
  gcc --version | head -n 1 || true
  ld.lld --version || true
  ld --version | head -n 1 || true
  make --version | head -n 1 || true
  qemu-system-x86_64 --version || true
  gdb --version | head -n 1 || true
  echo
  echo "== previous artifacts =="
  find build evidence -maxdepth 3 -type f 2>/dev/null | sort | grep -E 'M[0-8]|m[0-8]|kernel|iso|log|elf|map|o$' || true
} | tee evidence/m9/preflight_m9.log
```

Indikator lulus: log memuat versi toolchain, commit Git, dan tidak ada perubahan kerja tak-terjelaskan pada file M0–M8. Jika `git status --short` menampilkan modifikasi yang belum dikomit, lakukan commit checkpoint terlebih dahulu atau buat branch khusus M9.

```bash
git add .
git commit -m "checkpoint before M9 scheduler" || true
git switch -c m9-kernel-thread-scheduler
```

### 6.3 Pemeriksaan Khusus M8

Karena M9 dapat memakai heap untuk alokasi TCB dan stack, jalankan ulang audit M8 sebelum membuat thread. Jika repository sudah mempunyai target M8, jalankan:

```bash
make m8-clean
make m8-all
```

Jika target M8 belum tersedia atau berbeda, minimum jalankan host test allocator dan audit object freestanding yang setara. Bila allocator M8 belum stabil, M9 harus memakai array statik untuk TCB dan stack agar bug heap tidak menutupi bug scheduler.

---

## 7. Konsep Inti M9

### 7.1 Thread Control Block

Thread Control Block atau TCB adalah objek kernel yang menyimpan identitas, state, context register, entry function, argumen, stack, dan pointer runqueue. Pada M9, TCB bukan process descriptor. TCB belum memiliki address space sendiri, file descriptor table, credentials, signal state, atau resource accounting lengkap.

Invariant TCB minimum:

1. `magic == MCSOS_THREAD_MAGIC` untuk semua TCB valid.
2. `state` hanya salah satu nilai enum yang ditentukan.
3. `stack_base` dan `stack_size` valid untuk thread selain boot thread.
4. `context.rsp` berada di dalam rentang stack thread.
5. `context.rip` menunjuk ke entry trampoline atau continuation yang valid.
6. `next == NULL` untuk thread yang tidak sedang berada di runqueue.

### 7.2 State Machine Scheduler

State machine M9:

```text
NEW -> READY -> RUNNING -> READY
                    |         ^
                    v         |
                 BLOCKED -----+
                    |
                    v
                  ZOMBIE
```

Transisi yang diizinkan:

| Dari | Ke | Pemicu | Syarat |
|---|---|---|---|
| NEW | READY | `mcsos_sched_enqueue` | Stack dan context sudah valid |
| READY | RUNNING | `mcsos_sched_pick_next` + `mcsos_sched_yield` | Thread keluar dari runqueue |
| RUNNING | READY | `mcsos_sched_yield` | Thread tidak idle dan belum blocked |
| RUNNING | BLOCKED | `mcsos_thread_block_current` | Ada alasan blocking yang terdokumentasi |
| BLOCKED | READY | `mcsos_thread_mark_ready` | Event atau resource sudah tersedia |
| RUNNING | ZOMBIE | `thread_exit` pengayaan | Resource teardown jelas |

### 7.3 Context Switch x86_64

Context switch M9 menyimpan register yang harus dipertahankan pada boundary fungsi C: `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, `r15`, dan continuation `rip`. M9 belum menyimpan FPU/SSE/AVX, MSR, CR3 per-proses, interrupt frame, atau user context. Hal ini disengaja karena M9 masih single-address-space kernel thread.

Kontrak `mcsos_context_switch(old, new)`:

Preconditions:

1. `old` dan `new` bukan `NULL`.
2. Interrupt state sudah dikendalikan oleh caller.
3. `new->rsp` menunjuk ke stack kernel valid.
4. `new->rip` menunjuk ke instruksi executable valid.
5. Caller tidak memegang lock yang dapat menyebabkan deadlock setelah switch.

Postconditions:

1. Context lama dapat dilanjutkan dari label continuation.
2. Context baru mulai atau melanjutkan eksekusi di `new->rip`.
3. Register callee-saved dipulihkan sesuai context baru.
4. Tidak ada alokasi heap dan tidak ada blocking di assembly switch.

### 7.4 Runqueue Round-Robin Kooperatif

Runqueue M9 adalah FIFO queue. Thread running yang melakukan yield dimasukkan kembali ke ekor queue, lalu scheduler memilih thread dari kepala queue. Kompleksitas operasi enqueue dan dequeue adalah O(1), sedangkan validasi runqueue O(n) untuk kepentingan debugging.

M9 tidak memakai priority. M9 tidak menjamin fairness kuat. M9 hanya menjamin bahwa thread ready diputar dalam urutan FIFO jika tidak ada blocking dan tidak ada korupsi queue.

### 7.5 Invariants and Correctness / Invariant dan Kebenaran

Invariant wajib M9 adalah: hanya satu thread boleh berstatus `RUNNING` pada satu CPU; thread `RUNNING` tidak boleh muncul di ready queue; setiap thread di ready queue harus berstatus `READY`; `ready_tail` harus sama dengan node terakhir; `runnable_count` harus sama dengan jumlah node ready queue; `context.rsp` thread baru harus berada dalam rentang stack miliknya; context switch tidak boleh mengubah struktur runqueue; dan setiap transisi state harus terjadi melalui API scheduler, bukan melalui penulisan field secara sembarang. Pelanggaran invariant harus menghasilkan nilai error, log, atau panic yang dapat ditelusuri.

---

## 8. Architecture and Design / Arsitektur Ringkas

```text
+--------------------------------------------------------------+
|                    MCSOS Kernel M9                           |
+--------------------------------------------------------------+
| Logging/Panic M3                                             |
| IDT/Trap M4    Timer/IRQ0 M5                                 |
| PMM M6         VMM M7         Kernel Heap M8                 |
|                                                              |
| M9 Thread Layer                                              |
|  - mcsos_thread_t                                            |
|  - mcsos_context_t                                           |
|  - kernel stack per thread                                   |
|                                                              |
| M9 Scheduler Layer                                           |
|  - current                                                   |
|  - idle                                                      |
|  - ready_head/ready_tail                                     |
|  - runnable_count                                            |
|                                                              |
| M9 Arch Switch                                               |
|  - save old rsp/rbp/rbx/r12-r15/rip                          |
|  - restore new rsp/rbp/rbx/r12-r15/rip                       |
+--------------------------------------------------------------+
```

File yang dibuat pada M9:

```text
include/mcsos_thread.h
kernel/mcsos_thread.c
arch/x86_64/context_switch.S
tests/test_scheduler.c
Makefile target m9-all, m9-host-test, m9-freestanding, m9-audit
```

---

## 9. Struktur Repository

Jika repository mengikuti struktur M8, tambahkan file M9 sebagai berikut:

```bash
mkdir -p include kernel arch/x86_64 tests evidence/m9
```

Keluaran minimum setelah M9:

```text
build/m9/m9_host_test
build/m9/test_scheduler.log
build/m9/mcsos_thread.freestanding.o
build/m9/context_switch.o
build/m9/m9_scheduler_combined.o
build/m9/nm_undefined.log
build/m9/readelf_header.log
build/m9/objdump_key.log
build/m9/sha256.log
evidence/m9/preflight_m9.log
evidence/m9/qemu_m9.log
```

---

## 10. Implementation Plan / Instruksi Implementasi Langkah demi Langkah

### Langkah 1 — Membuat Header Scheduler

Header mendefinisikan TCB, context, scheduler state, error code, dan API. Semua tipe memakai ukuran eksplisit agar hasil kompilasi mudah diaudit.

```c
#ifndef MCSOS_THREAD_H
#define MCSOS_THREAD_H

#include <stddef.h>
#include <stdint.h>

#define MCSOS_THREAD_MAGIC UINT64_C(0x4d43534f53544852)
#define MCSOS_THREAD_NAME_MAX 32u
#define MCSOS_STACK_ALIGN 16u
#define MCSOS_MIN_KERNEL_STACK 4096u

typedef enum mcsos_thread_state {
    MCSOS_THREAD_NEW = 0,
    MCSOS_THREAD_READY = 1,
    MCSOS_THREAD_RUNNING = 2,
    MCSOS_THREAD_BLOCKED = 3,
    MCSOS_THREAD_ZOMBIE = 4
} mcsos_thread_state_t;

typedef enum mcsos_sched_result {
    MCSOS_SCHED_OK = 0,
    MCSOS_SCHED_EINVAL = -1,
    MCSOS_SCHED_ESTATE = -2,
    MCSOS_SCHED_ESTACK = -3,
    MCSOS_SCHED_ECORRUPT = -4
} mcsos_sched_result_t;

typedef void (*mcsos_thread_entry_t)(void *arg);

typedef struct mcsos_context {
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
} mcsos_context_t;

typedef struct mcsos_thread {
    uint64_t magic;
    uint64_t id;
    const char *name;
    mcsos_thread_state_t state;
    mcsos_context_t context;
    mcsos_thread_entry_t entry;
    void *arg;
    uint8_t *stack_base;
    size_t stack_size;
    struct mcsos_thread *next;
    uint64_t switches;
    uint64_t ticks;
    int exit_code;
} mcsos_thread_t;

typedef struct mcsos_scheduler {
    mcsos_thread_t *current;
    mcsos_thread_t *idle;
    mcsos_thread_t *ready_head;
    mcsos_thread_t *ready_tail;
    uint64_t next_id;
    uint64_t runnable_count;
    uint64_t context_switches;
    uint64_t ticks;
    int initialized;
} mcsos_scheduler_t;

void mcsos_context_switch(mcsos_context_t *old_context, const mcsos_context_t *new_context);
void mcsos_thread_trampoline(void);

int mcsos_scheduler_init(mcsos_scheduler_t *sched, mcsos_thread_t *boot_thread);
int mcsos_thread_prepare(mcsos_thread_t *thread,
                         const char *name,
                         mcsos_thread_entry_t entry,
                         void *arg,
                         void *stack_base,
                         size_t stack_size,
                         uint64_t id);
int mcsos_sched_enqueue(mcsos_scheduler_t *sched, mcsos_thread_t *thread);
mcsos_thread_t *mcsos_sched_pick_next(mcsos_scheduler_t *sched);
int mcsos_sched_yield(mcsos_scheduler_t *sched);
int mcsos_sched_tick(mcsos_scheduler_t *sched);
int mcsos_thread_block_current(mcsos_scheduler_t *sched);
int mcsos_thread_mark_ready(mcsos_scheduler_t *sched, mcsos_thread_t *thread);
int mcsos_sched_validate(const mcsos_scheduler_t *sched);
size_t mcsos_sched_ready_count(const mcsos_scheduler_t *sched);

#endif
```

Checkpoint:

```bash
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only include/mcsos_thread.h
```

Indikator lulus: tidak ada warning dan tidak ada error sintaks.

### Langkah 2 — Membuat Implementasi Scheduler C

File berikut mengimplementasikan runqueue FIFO, thread prepare, yield kooperatif, tick accounting, block/ready, dan validasi runqueue. Pada host unit test, `mcsos_context_switch` tidak dipanggil karena tujuan host test adalah memverifikasi state machine dan struktur data scheduler. Pada build freestanding, function tersebut dihubungkan dengan assembly `context_switch.S`.

```c
#include "mcsos_thread.h"

static uintptr_t align_down_uintptr(uintptr_t value, uintptr_t alignment) {
    return value & ~(alignment - 1u);
}

static int valid_thread_object(const mcsos_thread_t *thread) {
    return thread != (const mcsos_thread_t *)0 && thread->magic == MCSOS_THREAD_MAGIC;
}

static void zero_context(mcsos_context_t *context) {
    context->rsp = 0;
    context->rbp = 0;
    context->rbx = 0;
    context->r12 = 0;
    context->r13 = 0;
    context->r14 = 0;
    context->r15 = 0;
    context->rip = 0;
}

void mcsos_thread_trampoline(void) {
    for (;;) {
#if defined(__x86_64__)
        __asm__ volatile("hlt");
#else
        __builtin_trap();
#endif
    }
}

int mcsos_scheduler_init(mcsos_scheduler_t *sched, mcsos_thread_t *boot_thread) {
    if (sched == (mcsos_scheduler_t *)0 || boot_thread == (mcsos_thread_t *)0) {
        return MCSOS_SCHED_EINVAL;
    }
    boot_thread->magic = MCSOS_THREAD_MAGIC;
    boot_thread->id = 0;
    boot_thread->name = "boot";
    boot_thread->state = MCSOS_THREAD_RUNNING;
    boot_thread->entry = (mcsos_thread_entry_t)0;
    boot_thread->arg = (void *)0;
    boot_thread->stack_base = (uint8_t *)0;
    boot_thread->stack_size = 0;
    boot_thread->next = (mcsos_thread_t *)0;
    boot_thread->switches = 0;
    boot_thread->ticks = 0;
    boot_thread->exit_code = 0;
    zero_context(&boot_thread->context);

    sched->current = boot_thread;
    sched->idle = boot_thread;
    sched->ready_head = (mcsos_thread_t *)0;
    sched->ready_tail = (mcsos_thread_t *)0;
    sched->next_id = 1;
    sched->runnable_count = 0;
    sched->context_switches = 0;
    sched->ticks = 0;
    sched->initialized = 1;
    return MCSOS_SCHED_OK;
}

int mcsos_thread_prepare(mcsos_thread_t *thread,
                         const char *name,
                         mcsos_thread_entry_t entry,
                         void *arg,
                         void *stack_base,
                         size_t stack_size,
                         uint64_t id) {
    if (thread == (mcsos_thread_t *)0 || entry == (mcsos_thread_entry_t)0 || stack_base == (void *)0) {
        return MCSOS_SCHED_EINVAL;
    }
    if (stack_size < MCSOS_MIN_KERNEL_STACK) {
        return MCSOS_SCHED_ESTACK;
    }
    uintptr_t low = (uintptr_t)stack_base;
    uintptr_t high = low + (uintptr_t)stack_size;
    if (high <= low) {
        return MCSOS_SCHED_ESTACK;
    }
    uintptr_t top = align_down_uintptr(high, MCSOS_STACK_ALIGN);
    if (top <= low + 128u) {
        return MCSOS_SCHED_ESTACK;
    }
    top -= sizeof(uint64_t);
    *((uint64_t *)top) = UINT64_C(0);

    thread->magic = MCSOS_THREAD_MAGIC;
    thread->id = id;
    thread->name = name;
    thread->state = MCSOS_THREAD_NEW;
    zero_context(&thread->context);
    thread->context.rsp = (uint64_t)top;
    thread->context.rip = (uint64_t)(uintptr_t)mcsos_thread_trampoline;
    thread->entry = entry;
    thread->arg = arg;
    thread->stack_base = (uint8_t *)stack_base;
    thread->stack_size = stack_size;
    thread->next = (mcsos_thread_t *)0;
    thread->switches = 0;
    thread->ticks = 0;
    thread->exit_code = 0;
    return MCSOS_SCHED_OK;
}

int mcsos_sched_enqueue(mcsos_scheduler_t *sched, mcsos_thread_t *thread) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(thread)) {
        return MCSOS_SCHED_EINVAL;
    }
    if (thread->state != MCSOS_THREAD_NEW && thread->state != MCSOS_THREAD_READY && thread->state != MCSOS_THREAD_BLOCKED) {
        return MCSOS_SCHED_ESTATE;
    }
    thread->state = MCSOS_THREAD_READY;
    thread->next = (mcsos_thread_t *)0;
    if (sched->ready_tail == (mcsos_thread_t *)0) {
        sched->ready_head = thread;
        sched->ready_tail = thread;
    } else {
        sched->ready_tail->next = thread;
        sched->ready_tail = thread;
    }
    sched->runnable_count++;
    return MCSOS_SCHED_OK;
}

mcsos_thread_t *mcsos_sched_pick_next(mcsos_scheduler_t *sched) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0) {
        return (mcsos_thread_t *)0;
    }
    mcsos_thread_t *thread = sched->ready_head;
    if (thread == (mcsos_thread_t *)0) {
        return sched->idle;
    }
    sched->ready_head = thread->next;
    if (sched->ready_head == (mcsos_thread_t *)0) {
        sched->ready_tail = (mcsos_thread_t *)0;
    }
    thread->next = (mcsos_thread_t *)0;
    if (sched->runnable_count > 0u) {
        sched->runnable_count--;
    }
    return thread;
}

int mcsos_sched_yield(mcsos_scheduler_t *sched) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(sched->current)) {
        return MCSOS_SCHED_EINVAL;
    }
    mcsos_thread_t *old_thread = sched->current;
    mcsos_thread_t *next_thread = mcsos_sched_pick_next(sched);
    if (!valid_thread_object(next_thread)) {
        return MCSOS_SCHED_ECORRUPT;
    }
    if (next_thread == old_thread) {
        old_thread->state = MCSOS_THREAD_RUNNING;
        return MCSOS_SCHED_OK;
    }
    if (old_thread->state == MCSOS_THREAD_RUNNING && old_thread != sched->idle) {
        old_thread->state = MCSOS_THREAD_READY;
        int rc = mcsos_sched_enqueue(sched, old_thread);
        if (rc != MCSOS_SCHED_OK) {
            return rc;
        }
    }
    next_thread->state = MCSOS_THREAD_RUNNING;
    sched->current = next_thread;
    old_thread->switches++;
    next_thread->switches++;
    sched->context_switches++;
#if !defined(MCSOS_HOST_TEST)
    mcsos_context_switch(&old_thread->context, &next_thread->context);
#endif
    return MCSOS_SCHED_OK;
}

int mcsos_sched_tick(mcsos_scheduler_t *sched) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(sched->current)) {
        return MCSOS_SCHED_EINVAL;
    }
    sched->ticks++;
    sched->current->ticks++;
    return MCSOS_SCHED_OK;
}

int mcsos_thread_block_current(mcsos_scheduler_t *sched) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(sched->current)) {
        return MCSOS_SCHED_EINVAL;
    }
    if (sched->current == sched->idle) {
        return MCSOS_SCHED_ESTATE;
    }
    sched->current->state = MCSOS_THREAD_BLOCKED;
    return mcsos_sched_yield(sched);
}

int mcsos_thread_mark_ready(mcsos_scheduler_t *sched, mcsos_thread_t *thread) {
    if (!valid_thread_object(thread)) {
        return MCSOS_SCHED_EINVAL;
    }
    if (thread->state != MCSOS_THREAD_BLOCKED) {
        return MCSOS_SCHED_ESTATE;
    }
    return mcsos_sched_enqueue(sched, thread);
}

size_t mcsos_sched_ready_count(const mcsos_scheduler_t *sched) {
    if (sched == (const mcsos_scheduler_t *)0 || sched->initialized == 0) {
        return 0u;
    }
    size_t count = 0u;
    const mcsos_thread_t *cursor = sched->ready_head;
    while (cursor != (const mcsos_thread_t *)0) {
        count++;
        cursor = cursor->next;
    }
    return count;
}

int mcsos_sched_validate(const mcsos_scheduler_t *sched) {
    if (sched == (const mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(sched->current)) {
        return MCSOS_SCHED_EINVAL;
    }
    size_t count = 0u;
    const mcsos_thread_t *cursor = sched->ready_head;
    const mcsos_thread_t *last = (const mcsos_thread_t *)0;
    while (cursor != (const mcsos_thread_t *)0) {
        if (!valid_thread_object(cursor) || cursor->state != MCSOS_THREAD_READY) {
            return MCSOS_SCHED_ECORRUPT;
        }
        if (cursor == sched->current) {
            return MCSOS_SCHED_ECORRUPT;
        }
        last = cursor;
        cursor = cursor->next;
        count++;
        if (count > sched->runnable_count + 1u) {
            return MCSOS_SCHED_ECORRUPT;
        }
    }
    if (last != sched->ready_tail) {
        return MCSOS_SCHED_ECORRUPT;
    }
    if (count != (size_t)sched->runnable_count) {
        return MCSOS_SCHED_ECORRUPT;
    }
    return MCSOS_SCHED_OK;
}
```

Checkpoint sintaks host:

```bash
clang -std=c17 -Wall -Wextra -Werror -DMCSOS_HOST_TEST -Iinclude -fsyntax-only kernel/mcsos_thread.c
```

Indikator lulus: tidak ada warning dan tidak ada error. Jika muncul error karena `stdint.h` atau `stddef.h`, periksa paket toolchain WSL 2 dan jalankan ulang setup M0/M1.

### Langkah 3 — Membuat Assembly Context Switch

Assembly berikut menyimpan callee-saved register dan continuation `rip` context lama, kemudian memulihkan context baru. Kode ini kecil supaya dapat diaudit dengan `objdump`.

```asm
    .section .text
    .globl mcsos_context_switch
    .type mcsos_context_switch, @function
mcsos_context_switch:
    leaq 1f(%rip), %rax
    movq %rsp, 0(%rdi)
    movq %rbp, 8(%rdi)
    movq %rbx, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 32(%rdi)
    movq %r14, 40(%rdi)
    movq %r15, 48(%rdi)
    movq %rax, 56(%rdi)

    movq 0(%rsi), %rsp
    movq 8(%rsi), %rbp
    movq 16(%rsi), %rbx
    movq 24(%rsi), %r12
    movq 32(%rsi), %r13
    movq 40(%rsi), %r14
    movq 48(%rsi), %r15
    jmp *56(%rsi)
1:
    ret
    .size mcsos_context_switch, . - mcsos_context_switch
```

Checkpoint assembly:

```bash
mkdir -p build/m9
clang -target x86_64-unknown-none-elf -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
  -c arch/x86_64/context_switch.S -o build/m9/context_switch.o
```

Indikator lulus: object `context_switch.o` terbentuk dan `objdump -d` menampilkan symbol `mcsos_context_switch`.

### Langkah 4 — Membuat Host Unit Test

Host unit test memverifikasi bahwa boot thread dapat diinisialisasi, thread baru dapat dipersiapkan, runqueue menghitung jumlah ready dengan benar, yield berpindah dari boot ke thread A lalu B, tick accounting bertambah, dan invariant runqueue tetap valid.

```c
#include <stdio.h>
#include <stdint.h>
#include "mcsos_thread.h"

static void noop(void *arg) { (void)arg; }

#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } \
} while (0)

int main(void) {
    mcsos_scheduler_t sched;
    mcsos_thread_t boot;
    mcsos_thread_t a;
    mcsos_thread_t b;
    unsigned char stack_a[8192];
    unsigned char stack_b[8192];

    REQUIRE(mcsos_scheduler_init(&sched, &boot) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_sched_validate(&sched) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_thread_prepare(&a, "a", noop, NULL, stack_a, sizeof(stack_a), sched.next_id++) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_thread_prepare(&b, "b", noop, NULL, stack_b, sizeof(stack_b), sched.next_id++) == MCSOS_SCHED_OK);
    REQUIRE((a.context.rsp & 0xfu) == 8u);
    REQUIRE(mcsos_sched_enqueue(&sched, &a) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_sched_enqueue(&sched, &b) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_sched_ready_count(&sched) == 2u);
    REQUIRE(mcsos_sched_validate(&sched) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_sched_yield(&sched) == MCSOS_SCHED_OK);
    REQUIRE(sched.current == &a);
    REQUIRE(a.state == MCSOS_THREAD_RUNNING);
    REQUIRE(mcsos_sched_ready_count(&sched) == 1u);
    REQUIRE(mcsos_sched_tick(&sched) == MCSOS_SCHED_OK);
    REQUIRE(a.ticks == 1u);
    REQUIRE(mcsos_sched_yield(&sched) == MCSOS_SCHED_OK);
    REQUIRE(sched.current == &b);
    REQUIRE(mcsos_sched_yield(&sched) == MCSOS_SCHED_OK);
    REQUIRE(sched.current == &a);
    REQUIRE(sched.context_switches == 3u);
    REQUIRE(mcsos_sched_validate(&sched) == MCSOS_SCHED_OK);
    puts("M9 scheduler host unit test PASS");
    return 0;
}
```

Jalankan host unit test:

```bash
mkdir -p build/m9
clang -std=c17 -Wall -Wextra -Werror -DMCSOS_HOST_TEST -Iinclude \
  tests/test_scheduler.c kernel/mcsos_thread.c -o build/m9/m9_host_test
build/m9/m9_host_test | tee build/m9/test_scheduler.log
```

Indikator lulus:

```text
M9 scheduler host unit test PASS
```

### Langkah 5 — Memperbarui Makefile

Tambahkan target M9. Jika repository sudah memiliki Makefile besar, adaptasikan isi target berikut, jangan menghapus target M0–M8.

```makefile
CC := clang
LD := ld.lld
OBJDUMP ?= objdump
READELF ?= readelf
NM ?= nm
SHA256SUM ?= sha256sum

BUILD := build/m9
CFLAGS_HOST := -std=c17 -Wall -Wextra -Werror -DMCSOS_HOST_TEST -Iinclude
CFLAGS_KERNEL := -target x86_64-unknown-none-elf -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra -Werror -Iinclude
ASFLAGS_KERNEL := -target x86_64-unknown-none-elf -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone

.PHONY: m9-all m9-host-test m9-freestanding m9-audit m9-clean

m9-all: m9-host-test m9-freestanding m9-audit

$(BUILD):
	mkdir -p $(BUILD)

m9-host-test: $(BUILD)
	$(CC) $(CFLAGS_HOST) tests/test_scheduler.c kernel/mcsos_thread.c -o $(BUILD)/m9_host_test
	$(BUILD)/m9_host_test | tee $(BUILD)/test_scheduler.log

m9-freestanding: $(BUILD)
	$(CC) $(CFLAGS_KERNEL) -c kernel/mcsos_thread.c -o $(BUILD)/mcsos_thread.freestanding.o
	$(CC) $(ASFLAGS_KERNEL) -c arch/x86_64/context_switch.S -o $(BUILD)/context_switch.o
	$(LD) -r $(BUILD)/mcsos_thread.freestanding.o $(BUILD)/context_switch.o -o $(BUILD)/m9_scheduler_combined.o

m9-audit: m9-freestanding
	$(NM) -u $(BUILD)/m9_scheduler_combined.o | tee $(BUILD)/nm_undefined.log
	$(READELF) -h $(BUILD)/m9_scheduler_combined.o | tee $(BUILD)/readelf_header.log
	$(OBJDUMP) -d $(BUILD)/m9_scheduler_combined.o | grep -E 'mcsos_context_switch|jmp|ret|hlt' | tee $(BUILD)/objdump_key.log
	$(SHA256SUM) $(BUILD)/m9_host_test $(BUILD)/m9_scheduler_combined.o | tee $(BUILD)/sha256.log

m9-clean:
	rm -rf $(BUILD)
```

Jalankan target lengkap:

```bash
make m9-clean
make m9-all
```

Target ini melakukan tiga hal: menjalankan host unit test, membangun object freestanding x86_64, dan melakukan audit dasar dengan `nm`, `readelf`, `objdump`, serta `sha256sum`.

### Langkah 6 — Audit Freestanding Object

Perintah berikut memeriksa apakah object gabungan scheduler memiliki unresolved symbol, benar-benar ELF64 x86_64, dan memuat context switch.

```bash
nm -u build/m9/m9_scheduler_combined.o | tee build/m9/nm_undefined.log
readelf -h build/m9/m9_scheduler_combined.o | tee build/m9/readelf_header.log
objdump -d build/m9/m9_scheduler_combined.o | grep -E 'mcsos_context_switch|jmp|ret|hlt' | tee build/m9/objdump_key.log
```

Indikator lulus:

1. `nm_undefined.log` kosong.
2. `readelf_header.log` memuat `Class: ELF64` dan `Machine: Advanced Micro Devices X86-64`.
3. `objdump_key.log` memuat symbol `mcsos_context_switch`, instruksi `jmp`, `ret`, dan `hlt` pada trampoline atau idle path.

### Langkah 7 — Integrasi ke Kernel MCSOS

Integrasi ke kernel harus dilakukan konservatif. Jangan langsung mengganti seluruh kontrol boot dengan scheduler. Tambahkan scheduler setelah heap M8 siap dan sebelum menjalankan eksperimen dua thread.

Contoh pola integrasi pada `kernel_main`:

```c
#include "mcsos_thread.h"

static mcsos_scheduler_t g_sched;
static mcsos_thread_t g_boot_thread;
static mcsos_thread_t g_thread_a;
static mcsos_thread_t g_thread_b;
static unsigned char g_stack_a[8192] __attribute__((aligned(16)));
static unsigned char g_stack_b[8192] __attribute__((aligned(16)));

static void demo_thread_a(void *arg) {
    (void)arg;
    for (;;) {
        klog("[M9] thread A tick\n");
        mcsos_sched_yield(&g_sched);
    }
}

static void demo_thread_b(void *arg) {
    (void)arg;
    for (;;) {
        klog("[M9] thread B tick\n");
        mcsos_sched_yield(&g_sched);
    }
}

void kernel_main(void) {
    /* init M2-M8 lebih dulu: console, panic, IDT, timer, PMM, VMM, heap */
    mcsos_scheduler_init(&g_sched, &g_boot_thread);
    mcsos_thread_prepare(&g_thread_a, "demo-a", demo_thread_a, 0, g_stack_a, sizeof(g_stack_a), g_sched.next_id++);
    mcsos_thread_prepare(&g_thread_b, "demo-b", demo_thread_b, 0, g_stack_b, sizeof(g_stack_b), g_sched.next_id++);
    mcsos_sched_enqueue(&g_sched, &g_thread_a);
    mcsos_sched_enqueue(&g_sched, &g_thread_b);
    klog("[M9] scheduler initialized\n");
    mcsos_sched_yield(&g_sched);
    for (;;) __asm__ volatile("hlt");
}
```

Catatan penting: contoh integrasi di atas memakai stack statik agar mahasiswa dapat memisahkan bug scheduler dari bug heap. Setelah lulus, stack thread dapat dialokasikan dari M8 dengan wrapper `kstack_alloc()` yang mengembalikan memori aligned, terpetakan, dan tidak akan di-free saat thread masih hidup.

### Langkah 8 — QEMU Smoke Test

Jalankan QEMU dengan log serial. Sesuaikan nama ISO atau image sesuai pipeline M2–M8.

```bash
mkdir -p evidence/m9
qemu-system-x86_64 \
  -m 256M \
  -machine q35 \
  -serial file:evidence/m9/qemu_m9.log \
  -display none \
  -no-reboot \
  -no-shutdown \
  -cdrom build/mcsos.iso
```

Indikator lulus minimum:

```text
[M9] scheduler initialized
[M9] thread A tick
[M9] thread B tick
```

Jika log hanya menunjukkan thread A berulang tanpa thread B, runqueue atau yield tidak memutar thread dengan benar. Jika QEMU reboot atau berhenti tanpa log, curigai context switch, stack pointer, atau IDT/panic path.

### Langkah 9 — Debug dengan GDB

QEMU gdbstub digunakan untuk memeriksa symbol, register, dan stack saat context switch.

Terminal 1:

```bash
qemu-system-x86_64 \
  -m 256M \
  -machine q35 \
  -serial stdio \
  -display none \
  -no-reboot \
  -no-shutdown \
  -s -S \
  -cdrom build/mcsos.iso
```

Terminal 2:

```bash
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) break mcsos_context_switch
(gdb) break mcsos_sched_yield
(gdb) continue
(gdb) info registers rsp rbp rip rbx r12 r13 r14 r15
(gdb) x/16gx $rsp
```

Indikator lulus: GDB dapat berhenti di `mcsos_context_switch`; `rsp` berpindah ke rentang stack thread target; `rip` berada pada trampoline atau continuation yang valid.

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Bukti wajib |
|---|---|---|
| C1 Header valid | `clang ... -fsyntax-only include/mcsos_thread.h` | Tidak ada warning/error |
| C2 Scheduler C valid | `clang ... -fsyntax-only kernel/mcsos_thread.c` | Tidak ada warning/error |
| C3 Host test | `make m9-host-test` | `M9 scheduler host unit test PASS` |
| C4 Freestanding object | `make m9-freestanding` | `m9_scheduler_combined.o` terbentuk |
| C5 Audit object | `make m9-audit` | `nm -u` kosong, ELF64 x86_64, symbol context switch ada |
| C6 Integrasi kernel | build ISO MCSOS | kernel image/ISO terbentuk |
| C7 QEMU smoke | QEMU serial log | log scheduler dan dua thread bergantian |
| C8 Debug | GDB breakpoint | register dan stack dapat diperiksa |

---

## 12. Tugas Implementasi

### 12.1 Tugas Wajib

1. Membuat `mcsos_thread_t` dan `mcsos_context_t` sesuai kontrak.
2. Membuat `mcsos_scheduler_t` dengan `current`, `idle`, `ready_head`, `ready_tail`, `runnable_count`, dan statistik switch.
3. Mengimplementasikan `mcsos_scheduler_init`.
4. Mengimplementasikan `mcsos_thread_prepare` dengan validasi stack minimum dan alignment.
5. Mengimplementasikan FIFO enqueue dan pick next.
6. Mengimplementasikan `mcsos_sched_yield` kooperatif.
7. Mengimplementasikan context switch x86_64 assembly.
8. Menulis host unit test minimal seperti `tests/test_scheduler.c`.
9. Menambahkan Makefile target `m9-all`.
10. Mengumpulkan bukti `nm`, `readelf`, `objdump`, `sha256sum`, dan QEMU log.

### 12.2 Tugas Pengayaan

1. Tambahkan `thread_exit()` dan state `ZOMBIE` dengan statistik exit code.
2. Tambahkan `sleep_ticks` dan wakeup melalui timer M5.
3. Tambahkan flag `need_resched` pada tick handler, tetapi tetap panggil scheduler dari safe point, bukan sembarang lokasi interrupt.
4. Tambahkan ring buffer trace `sched_switch(prev, next, reason)`.
5. Tambahkan stack canary di ujung stack thread dan validasi pada yield.
6. Alokasikan stack dari heap M8 dengan wrapper `kstack_alloc()`.

### 12.3 Tantangan Riset

1. Rancang scheduler preemptive single-core dengan timer interrupt, namun sertakan proof obligation untuk interrupt state, lock ownership, dan nested interrupt.
2. Rancang transisi dari kernel thread ke user process: per-process page table, trap frame user, syscall ABI, dan user pointer validation.
3. Bandingkan round-robin M9 dengan model fairness CFS/EEVDF secara konseptual tanpa menyalin kompleksitas produksi.

---

## 13. Failure Modes dan Diagnosis

| Failure mode | Gejala | Penyebab umum | Diagnosis | Perbaikan |
|---|---|---|---|---|
| Stack pointer salah | Triple fault atau page fault saat switch | Stack tidak aligned atau tidak mapped | GDB `info registers rsp rip` | Pastikan stack 16-byte aligned dan page present |
| Double enqueue | Thread muncul dua kali di runqueue | `next` tidak dibersihkan atau state tidak dicek | `mcsos_sched_validate` gagal | Tolak enqueue thread RUNNING/READY yang sedang queued |
| Lost wakeup | Thread blocked tidak pernah running lagi | Event terjadi sebelum thread masuk wait queue | Trace state transition | Gunakan lock/interrupt disable di transisi block/wakeup |
| Context register hilang | Variabel lokal rusak setelah yield | Register callee-saved tidak disimpan | Audit `objdump` context switch | Simpan `rbx`, `rbp`, `r12`–`r15` |
| Scheduler dipanggil dari IRQ sembarang | Hang atau stack nested rusak | Preemption belum didesain | Log interrupt nesting | Batasi M9 ke cooperative yield dahulu |
| Idle thread korup | CPU lompat ke alamat nol | Idle TCB bukan object valid | Validasi magic idle/current | Inisialisasi boot thread sebagai idle sementara |
| Heap corrupt saat buat stack | Page fault setelah thread dibuat | Stack dari heap overlap atau free dini | PMM/VMM/heap log | Gunakan stack statik sampai heap terbukti stabil |
| `nm -u` tidak kosong | Link object belum lengkap | Symbol eksternal tidak didefinisikan | Lihat `nm_undefined.log` | Tambahkan object assembly atau stub valid |
| Log QEMU kosong | Tidak mencapai scheduler | Boot/panic/serial dari M2-M3 rusak | Periksa log M2-M3 | Rollback ke checkpoint M8 |
| Infinite loop satu thread | Thread lain tidak dipilih | Runqueue tidak diputar | Trace enqueue/dequeue | Pastikan old RUNNING masuk ekor ready queue |

---

## 13A. Security and Threat Model / Keamanan dan Model Ancaman

Model ancaman M9 masih internal-kernel karena belum ada user mode. Aktor yang dipertimbangkan adalah bug kernel, handler interrupt yang memanggil scheduler pada waktu yang salah, pointer TCB korup, stack overlap, dan source code yang salah target ABI. Aset yang dilindungi adalah integritas stack kernel, integritas runqueue, control-flow `rip`, register context, dan panic/log path. Enforcement point minimum adalah validasi `magic`, validasi state sebelum enqueue, pemisahan running thread dari ready queue, audit `nm/readelf/objdump`, dan larangan memakai scheduler preemptive sebelum lock ownership serta interrupt ownership terdokumentasi.

M9 belum memberikan boundary keamanan user/kernel. Oleh karena itu, tidak boleh ada klaim bahwa scheduler M9 aman terhadap proses malicious, syscall malicious, atau privilege escalation. Klaim yang diperbolehkan hanya bahwa struktur dasar scheduler telah memiliki validasi internal dan siap diuji pada QEMU untuk tahap kernel-thread single-core.

---

## 14. Prosedur Rollback

Rollback harus menjaga repository tetap dapat dibangun. Jangan menghapus perubahan secara manual tanpa commit checkpoint.

```bash
git status --short
git add .
git commit -m "wip M9 scheduler before rollback" || true
git switch main
git branch backup-m9-failed
```

Rollback file M9 saja:

```bash
git restore --source HEAD~1 -- include/mcsos_thread.h kernel/mcsos_thread.c arch/x86_64/context_switch.S tests/test_scheduler.c Makefile
make clean
make m8-all
```

Jika M9 menyebabkan boot kernel gagal, tetapi M8 masih baik, jalankan kembali ISO M8 dan lampirkan perbandingan log. Hasil rollback lulus jika M8 kembali dapat dibangun dan QEMU boot log M8 muncul.

---

## 15. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | TCB, runqueue, yield, context switch, host test, dan QEMU log bekerja sesuai target |
| Kualitas desain dan invariants | 20 | State machine, ownership stack, invariant ready queue, dan batas single-core jelas |
| Pengujian dan bukti | 20 | Host test, audit ELF/disassembly, serial log, GDB evidence, dan checksum lengkap |
| Debugging/failure analysis | 10 | Minimal lima failure mode dijelaskan dengan diagnosis dan perbaikan |
| Keamanan dan robustness | 10 | Tidak ada klaim berlebihan; risiko corrupt context, stack, interrupt race, dan privilege boundary dibahas |
| Dokumentasi/laporan | 10 | Laporan lengkap, reproducible, berisi commit hash, log, screenshot, dan referensi IEEE |

---

## 16. Pertanyaan Analisis

1. Mengapa thread running tidak boleh berada di runqueue?
2. Apa risiko jika stack dua thread overlap pada alamat fisik atau virtual yang sama?
3. Mengapa M9 hanya menyimpan callee-saved register, bukan semua register CPU?
4. Apa perbedaan context switch dari cooperative yield dengan context switch dari interrupt timer?
5. Mengapa `mcsos_context_switch` tidak boleh melakukan alokasi heap?
6. Apa bukti bahwa object yang dihasilkan adalah ELF64 x86_64?
7. Bagaimana cara mendeteksi runqueue cycle?
8. Mengapa preemption harus ditunda sampai lock/interrupt ownership jelas?
9. Apa perbedaan kernel thread M9 dengan proses userspace pada tahap berikutnya?
10. Bagaimana desain M9 harus berubah ketika per-thread address space diperkenalkan?

---

## 17. Acceptance Criteria / Kriteria Lulus Praktikum

M9 dinyatakan lulus minimum jika semua syarat berikut terpenuhi:

1. Repository dapat dibangun dari clean checkout.
2. Perintah build M9 terdokumentasi.
3. Host unit test scheduler lulus.
4. Freestanding object x86_64 berhasil dibuat.
5. `nm -u` pada object gabungan kosong atau hanya berisi symbol eksternal yang sengaja didokumentasikan. Untuk source panduan ini, targetnya kosong.
6. `readelf -h` menunjukkan ELF64 x86_64.
7. `objdump` menunjukkan `mcsos_context_switch` dan instruksi switch yang relevan.
8. Integrasi kernel menghasilkan log serial scheduler.
9. QEMU smoke test berjalan deterministik untuk minimal dua thread demo.
10. Panic path tetap terbaca jika context switch atau stack gagal.
11. Tidak ada warning kritis pada build host dan freestanding.
12. Perubahan Git dikomit.
13. Laporan memuat log, screenshot atau serial output, hash artefak, dan analisis failure mode.
14. Mahasiswa dapat menjelaskan batasan M9: single-core, cooperative, kernel-thread only, belum user process, belum SMP.

---

## 18. Bukti Validasi Source Code Panduan Ini

Source code inti M9 dalam panduan ini telah diperiksa secara lokal melalui host unit test, kompilasi freestanding, link relocatable, dan audit object. Bukti berikut adalah hasil dari proyek verifikasi panduan, bukan pengganti validasi ulang di WSL 2 mahasiswa.

### 18.1 Host Unit Test

```text
M9 scheduler host unit test PASS
```

### 18.2 `readelf -h`

```text
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00 
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0x0
  Start of program headers:          0 (bytes into file)
  Start of section headers:          3880 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           64 (bytes)
  Number of section headers:         10
  Section header string table index: 8
```

### 18.3 `objdump` Key Audit

```text
4:	eb 00                	jmp    6 <mcsos_thread_trampoline+0x6>
   6:	f4                   	hlt
   7:	eb fd                	jmp    6 <mcsos_thread_trampoline+0x6>
  35:	e9 23 01 00 00       	jmp    15d <mcsos_scheduler_init+0x14d>
 165:	c3                   	ret
 1dd:	c3                   	ret
 220:	e9 5a 01 00 00       	jmp    37f <mcsos_thread_prepare+0x19f>
 236:	e9 44 01 00 00       	jmp    37f <mcsos_thread_prepare+0x19f>
 260:	e9 1a 01 00 00       	jmp    37f <mcsos_thread_prepare+0x19f>
 292:	e9 e8 00 00 00       	jmp    37f <mcsos_thread_prepare+0x19f>
 387:	c3                   	ret
 3b8:	c3                   	ret
 3f6:	e9 9c 00 00 00       	jmp    497 <mcsos_sched_enqueue+0xd7>
 420:	eb 75                	jmp    497 <mcsos_sched_enqueue+0xd7>
 45f:	eb 1f                	jmp    480 <mcsos_sched_enqueue+0xc0>
 49f:	c3                   	ret
 4dc:	c3                   	ret
 505:	eb 7d                	jmp    584 <mcsos_sched_pick_next+0xa4>
 526:	eb 5c                	jmp    584 <mcsos_sched_pick_next+0xa4>
 58d:	c3                   	ret
 5c5:	e9 04 01 00 00       	jmp    6ce <mcsos_sched_yield+0x13e>
 5f7:	e9 d2 00 00 00       	jmp    6ce <mcsos_sched_yield+0x13e>
 618:	e9 b1 00 00 00       	jmp    6ce <mcsos_sched_yield+0x13e>
 65c:	eb 70                	jmp    6ce <mcsos_sched_yield+0x13e>
 65e:	eb 00                	jmp    660 <mcsos_sched_yield+0xd0>
 6d6:	c3                   	ret
 715:	eb 30                	jmp    747 <mcsos_sched_tick+0x67>
 74f:	c3                   	ret
 785:	eb 34                	jmp    7bb <mcsos_thread_block_current+0x6b>
 79f:	eb 1a                	jmp    7bb <mcsos_thread_block_current+0x6b>
 7c3:	c3                   	ret
 7f5:	eb 23                	jmp    81a <mcsos_thread_mark_ready+0x4a>
 808:	eb 10                	jmp    81a <mcsos_thread_mark_ready+0x4a>
 822:	c3                   	ret
 855:	eb 40                	jmp    897 <mcsos_sched_ready_count+0x67>
 88d:	eb dc                	jmp    86b <mcsos_sched_ready_count+0x3b>
 8a0:	c3                   	ret
 8e5:	e9 dc 00 00 00       	jmp    9c6 <mcsos_sched_validate+0x116>
 930:	e9 91 00 00 00       	jmp    9c6 <mcsos_sched_validate+0x116>
 949:	eb 7b                	jmp    9c6 <mcsos_sched_validate+0x116>
 98a:	eb 3a                	jmp    9c6 <mcsos_sched_validate+0x116>
 98c:	e9 75 ff ff ff       	jmp    906 <mcsos_sched_validate+0x56>
 9a6:	eb 1e                	jmp    9c6 <mcsos_sched_validate+0x116>
 9bd:	eb 07                	jmp    9c6 <mcsos_sched_validate+0x116>
 9ce:	c3                   	ret
00000000000009d0 <mcsos_context_switch>:
 9d0:	48 8d 05 3d 00 00 00 	lea    0x3d(%rip),%rax        # a14 <mcsos_context_switch+0x44>
 a11:	ff 66 38             	jmp    *0x38(%rsi)
 a14:	c3                   	ret
```

### 18.4 SHA-256 Artefak Verifikasi

```text
e3a4a12942237e6eadc8b632535324df345e7e7f6665fb49b062a13d3369c0ac  build/m9/m9_host_test
ee820d4eca8430330fcbc986822484d8cc6b40ef766dc4b91b8cf49b09db6788  build/m9/m9_scheduler_combined.o
```

Interpretasi: host unit test lulus, object gabungan adalah ELF64 x86_64 relocatable object, disassembly memuat symbol `mcsos_context_switch`, dan artefak memiliki checksum yang dapat dicatat. Namun, validasi runtime QEMU/OVMF tetap wajib dilakukan ulang di repository mahasiswa.

---

## 19. Template Laporan Praktikum M9

Gunakan format laporan seragam berikut.

### 19.1 Sampul

- Judul: Praktikum M9 — Kernel Thread, Scheduler, dan Context Switch x86_64 pada MCSOS
- Nama mahasiswa:
- NIM:
- Kelas:
- Jika kelompok: nama anggota dan pembagian kerja:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia

### 19.2 Tujuan

Tuliskan tujuan teknis dan konseptual M9, termasuk thread kernel, runqueue, context switch, host test, dan QEMU smoke test.

### 19.3 Dasar Teori Ringkas

Jelaskan TCB, kernel stack, context, scheduler state machine, round-robin, ABI x86_64, dan batas antara cooperative scheduler dan preemptive scheduler.

### 19.4 Lingkungan

Isi tabel berikut:

| Item | Versi / Nilai |
|---|---|
| Windows | |
| WSL distro | |
| Kernel WSL | |
| Clang/GCC | |
| LLD/GNU ld | |
| QEMU | |
| GDB | |
| Target | x86_64 |
| Commit hash | |

### 19.5 Desain

Lampirkan diagram state machine, struktur TCB, invariant runqueue, dan alur `sched_yield -> pick_next -> context_switch`.

### 19.6 Langkah Kerja

Tuliskan perintah yang dijalankan, file yang dibuat/diubah, alasan teknis, dan output ringkas.

### 19.7 Hasil Uji

| Uji | Perintah | Hasil | Bukti |
|---|---|---|---|
| Host unit test | `make m9-host-test` | PASS/FAIL | `test_scheduler.log` |
| Freestanding compile | `make m9-freestanding` | PASS/FAIL | object file |
| Undefined symbol | `nm -u ...` | PASS/FAIL | `nm_undefined.log` |
| ELF audit | `readelf -h ...` | PASS/FAIL | `readelf_header.log` |
| Disassembly audit | `objdump -d ...` | PASS/FAIL | `objdump_key.log` |
| QEMU smoke | QEMU command | PASS/FAIL | `qemu_m9.log` |
| GDB | breakpoint context switch | PASS/FAIL | screenshot/log |

### 19.8 Analisis

Jelaskan penyebab keberhasilan, bug yang ditemukan, penyebab bug, solusi, dan batasan hasil.

### 19.9 Keamanan dan Reliability

Bahas risiko stack corruption, context corruption, lost wakeup, double enqueue, race interrupt, dan privilege boundary yang belum ada.

### 19.10 Kesimpulan

Nyatakan apa yang berhasil, apa yang belum, dan rencana M10. Gunakan istilah readiness yang terukur, misalnya “siap uji QEMU untuk kernel scheduler awal”.

### 19.11 Lampiran

Lampirkan diff ringkas, potongan kode penting, log penuh, screenshot QEMU/GDB, dan referensi.

---

## 20. Readiness Review

| Kriteria | Status yang diharapkan | Bukti |
|---|---|---|
| Build host test | Lulus | `test_scheduler.log` |
| Build freestanding | Lulus | object dan log build |
| Audit symbol | Lulus | `nm_undefined.log` |
| Audit ELF | Lulus | `readelf_header.log` |
| Audit disassembly | Lulus | `objdump_key.log` |
| Integrasi kernel | Lulus di WSL 2 mahasiswa | QEMU serial log |
| Debug path | Lulus | GDB breakpoint dan register dump |
| Security boundary | Terbatas | Belum ada user mode; risiko didokumentasikan |
| SMP readiness | Tidak lulus / out of scope | M9 single-core |
| Production readiness | Tidak lulus / out of scope | Belum ada stress, fuzzing, SMP, user mode, security review penuh |

Kesimpulan readiness M9: hasil yang memenuhi seluruh kriteria di atas hanya dapat disebut **siap uji QEMU untuk kernel thread dan scheduler awal single-core**. Hasil M9 belum boleh disebut siap produksi, belum siap hardware umum, belum siap multi-core, dan belum membuktikan correctness scheduler secara formal.

---

## 21. Referensi

[1] Intel Corporation, “Intel® 64 and IA-32 Architectures Software Developer Manuals,” Intel Developer Zone, 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

[2] x86 psABIs, “x86-64 psABI,” GitLab project, 2019–2026. [Online]. Available: https://gitlab.com/x86-psABIs/x86-64-ABI

[3] QEMU Project, “GDB usage,” QEMU System Emulation Documentation, 2026. [Online]. Available: https://qemu-project.gitlab.io/qemu/system/gdb.html

[4] LLVM Project, “Clang command line argument reference,” Clang Documentation, 2026. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html

[5] GNU Project, “LD: the GNU linker,” GNU Binutils Documentation, 2026. [Online]. Available: https://sourceware.org/binutils/docs/ld/

[6] The Linux Kernel Documentation, “CFS Scheduler,” kernel.org documentation, 2026. [Online]. Available: https://www.kernel.org/doc/html/latest/scheduler/sched-design-CFS.html
