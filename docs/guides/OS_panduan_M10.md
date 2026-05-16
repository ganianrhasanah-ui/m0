# OS_panduan_M10.md

# Panduan Praktikum M10 — ABI System Call Awal, Dispatcher Syscall, Validasi Argumen, dan Jalur `int 0x80` Terkendali pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M10  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: *siap uji QEMU untuk syscall dispatcher awal single-core dan smoke test ABI kernel-side*, bukan siap produksi, bukan siap multi-user, dan bukan bukti isolasi user/kernel penuh.

---

## 1. Ringkasan Praktikum

Praktikum M10 melanjutkan fondasi M0 sampai M9. Pada M4 mahasiswa telah membuat IDT dan exception path. Pada M5 mahasiswa telah menambahkan interrupt eksternal dan timer. Pada M6 mahasiswa telah membuat Physical Memory Manager. Pada M7 mahasiswa telah membuat Virtual Memory Manager awal. Pada M8 mahasiswa telah membuat kernel heap. Pada M9 mahasiswa telah membuat kernel thread, runqueue, scheduler round-robin kooperatif, dan context switch x86_64 awal. M10 memakai seluruh fondasi tersebut untuk membangun **ABI system call awal**, **dispatcher syscall**, **tabel syscall versioned**, **validasi argumen**, **validasi rentang user buffer**, dan **jalur entry `int 0x80` terkendali**.

M10 belum boleh diperlakukan sebagai implementasi userspace lengkap. Praktikum ini sengaja membuat syscall layer secara bertahap: pertama dispatcher dan kontrak ABI diuji melalui host unit test; kedua object freestanding dan stub entry x86_64 diaudit; ketiga integrasi QEMU dilakukan sebagai smoke test yang terkontrol. Ring 3 penuh, ELF user loader, TSS/IST lengkap, `syscall/sysret` produksi, copy-on-write, signal, credential, dan process isolation penuh masih menjadi tahap lanjutan. Pembatasan ini penting karena jalur syscall adalah batas privilege yang rawan: kesalahan validasi pointer, register clobber, stack alignment, atau return path dapat menghasilkan page fault, general protection fault, triple fault, kebocoran memori kernel, atau corrupt scheduler state.

Intel SDM menjadi sumber utama untuk mekanisme arsitektur x86_64, termasuk memory management, protection, task management, interrupt/exception handling, multiprocessor support, dan instruksi system-level [1]. x86-64 psABI menjadi rujukan calling convention dan konsekuensi register/stack pada boundary assembly-ke-C [2]. QEMU gdbstub dipakai untuk debugging guest dengan remote GDB, breakpoint, inspeksi register, dan inspeksi memori [3]. Clang `-ffreestanding` dipakai untuk memastikan kompilasi berlangsung dalam lingkungan freestanding, bukan hosted userspace [4]. Dokumentasi Linux tentang penambahan system call dipakai sebagai pembanding metodologis: syscall harus mempunyai nomor, prototype, implementasi inti, wiring arsitektur, selftest, dan dokumentasi API; MCSOS M10 menerapkan prinsip tersebut dalam skala pendidikan [5]. Dokumentasi locking Linux digunakan sebagai pembanding konseptual bahwa jalur syscall yang menyentuh scheduler atau buffer bersama harus memisahkan konteks interrupt, preemption, dan lock ownership [6].

Keberhasilan M10 tidak boleh dinyatakan sebagai “tanpa error”. Kriteria minimum M10 adalah: source syscall dapat dikompilasi sebagai C17 freestanding, host unit test dispatcher lulus, object gabungan menunjukkan ELF64 x86_64, `nm -u` kosong untuk object gabungan praktikum, disassembly memuat symbol `x86_64_syscall_int80_stub` dan `iretq`, QEMU smoke test menghasilkan log syscall yang deterministik, panic path tetap terbaca, dan laporan memuat bukti build/test/audit. Runtime QEMU/OVMF tetap harus dijalankan ulang pada WSL 2 mahasiswa karena bergantung pada bootloader, layout kernel, OVMF, QEMU, dan toolchain setempat.

---

## 2. Asumsi Target dan Batasan

| Aspek | Keputusan M10 |
|---|---|
| Arsitektur | x86_64 long mode |
| Lingkungan host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Boot path | Melanjutkan pipeline M2–M9; direkomendasikan Limine/UEFI atau ISO yang sudah lulus M2 |
| Bahasa | C17 freestanding + assembly x86_64 kecil untuk stub entry syscall |
| ABI syscall M10 | `rax` = nomor syscall; argumen: `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`; return: `rax` |
| Entry awal | `int 0x80` sebagai jalur pendidikan yang mudah dihubungkan ke IDT M4 |
| Entry lanjutan | `syscall/sysret` hanya pengayaan setelah GDT/TSS/MSR/user-mode state siap |
| Fondasi wajib | M4 IDT/trap, M5 interrupt/timer, M6 PMM, M7 VMM, M8 heap, M9 scheduler |
| Mode eksekusi wajib | Single-core; syscall dari kernel-triggered smoke test atau test harness terkontrol |
| User pointer | Validasi rentang dan overflow arithmetic; belum cukup sebagai fault-containment penuh |
| Out of scope | ELF user loader penuh, ring 3 penuh, per-process address space, credential, fork/exec/wait, signal, VDSO, SMP syscall, `syscall/sysret` produksi, dan ABI kompatibel Linux |

### 2A. Goals dan Non-Goals

**Goals** M10 adalah membuat kontrak ABI system call yang eksplisit, table-driven dispatcher, validasi nomor syscall, validasi pointer/buffer, helper `copy_from_user` pendidikan, operasi syscall minimal, test host deterministik, object freestanding, dan jalur integrasi `int 0x80` yang dapat diaudit. **Non-goals** M10 adalah kompatibilitas Linux, ABI POSIX lengkap, security boundary final, syscall dari ring 3 yang sepenuhnya aman, atau scheduler preemptive multi-core.

### 2B. Assumptions / Asumsi Implementasi

1. IDT dari M4 dapat menambahkan vector `0x80` dengan gate yang sesuai. Untuk kernel-only smoke test, DPL dapat tetap 0. Untuk syscall dari ring 3 pada tahap lanjutan, gate perlu DPL 3 dan segment/user stack/TSS harus valid.
2. Scheduler M9 menyediakan fungsi `yield_current` dan `exit_current` atau minimal stub yang aman. Jika scheduler belum siap, syscall `yield` dan `exit_thread` wajib mengembalikan error terkontrol.
3. Validasi user buffer M10 hanya memeriksa rentang virtual dan overflow arithmetic. Validasi tersebut belum menggantikan mekanisme page fault recovery, permission bit page table, dan user/supervisor isolation.
4. Kode freestanding tidak boleh memanggil libc, tidak boleh memakai alokasi heap tersembunyi, dan tidak boleh mengandalkan red zone x86_64.
5. Semua perubahan harus dapat di-rollback dengan `git restore` atau `git reset --hard` ke checkpoint M9.

### 2C. Scope dan Target Matrix

| Scope | Target wajib | Target pengayaan | Non-scope |
|---|---|---|---|
| ABI syscall | Nomor, argumen, return, error code | ABI manifest auto-generated | ABI Linux lengkap |
| Dispatcher | Table-driven, bound check, `-ENOSYS` | Tracepoint per syscall | Seccomp/capability penuh |
| Usercopy | Range check + overflow check | Page-fault assisted usercopy | Demand paging user |
| Entry | Stub `int 0x80` terkontrol | `syscall/sysret` setelah MSR siap | Fast path produksi |
| Test | Host unit test + freestanding object audit | QEMU smoke test vector 0x80 | Conformance POSIX |
| Integrasi scheduler | `yield`, `exit_thread` via callback | Thread teardown lengkap | Process lifecycle penuh |

### 2D. Toolchain BOM, Reproducibility, CI, dan Supply Chain

Toolchain bill of materials minimum adalah Clang atau GCC, LLD/GNU ld, GNU Make, GNU binutils (`nm`, `readelf`, `objdump`), QEMU, GDB, dan Git. Reproducibility minimum dibuktikan dengan clean rebuild, log versi tool, checksum artefak, dan commit hash. Untuk CI lokal, target minimum adalah `m10-host-test`, `m10-freestanding`, `m10-audit`, dan `m10-qemu-smoke` jika boot image M2–M9 sudah tersedia.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M10, mahasiswa mampu:

1. Menjelaskan syscall sebagai boundary terkontrol antara kode pemanggil dan kernel service.
2. Mendesain ABI syscall sederhana berbasis register x86_64 dengan nomor syscall, enam argumen, nilai balik, dan error convention.
3. Menjelaskan perbedaan calling convention fungsi C, interrupt gate, dan mekanisme syscall CPU.
4. Mengimplementasikan syscall dispatcher yang menolak nomor tidak valid dengan `-ENOSYS`.
5. Mengimplementasikan validasi argumen dan validasi rentang pointer yang mencegah overflow arithmetic.
6. Menjelaskan keterbatasan range check tanpa page-fault recovery.
7. Menghubungkan syscall `yield` dan `exit_thread` ke scheduler M9 melalui callback/helper, bukan dengan dependency siklik langsung.
8. Menyusun host unit test untuk dispatcher dan usercopy tanpa menjalankan QEMU.
9. Melakukan audit object freestanding dengan `nm`, `readelf`, dan `objdump`.
10. Menjelaskan failure modes syscall: nomor invalid, pointer invalid, entry stack salah, register clobber, lock inversion, dan reentrancy dari interrupt context.
11. Menulis laporan praktikum dengan bukti build, test, log QEMU, disassembly, analisis bug, rollback, dan readiness review.

---

## 4. Prasyarat Teori

| Materi | Kebutuhan M10 |
|---|---|
| ABI dan calling convention | Untuk menentukan register argumen dan register return |
| Interrupt/trap frame | Untuk memahami bagaimana `int 0x80` masuk ke kernel |
| Register clobber | Untuk mencegah hilangnya nilai penting saat assembly memanggil C |
| User/kernel isolation | Untuk memahami mengapa pointer pemanggil tidak boleh dipercaya |
| Virtual memory | Untuk membedakan alamat valid, permission page, dan page fault |
| Scheduler | Untuk membuat syscall `yield` dan `exit_thread` tidak merusak state thread |
| Locking | Untuk mencegah race pada tabel syscall, log, dan scheduler path |
| Debugging QEMU/GDB | Untuk memeriksa `rax`, `rdi`, `rip`, `cs`, `rflags`, dan return path |

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M10 |
|---|---|
| `@osdev-general` | Gate, roadmap, acceptance evidence, dan readiness review |
| `@osdev-01-computer-foundation` | State machine syscall, invariant dispatcher, proof obligation, dan negative test |
| `@osdev-02-low-level-programming` | ABI, register, stack, freestanding C, assembly stub, dan audit disassembly |
| `@osdev-03-computer-and-hardware-architecture` | x86_64 interrupt gate, privilege, MSR/syscall awareness, dan page fault boundary |
| `@osdev-04-kernel-development` | Syscall layer, scheduler callback, error path, dan observability kernel |
| `@osdev-07-os-security` | Threat model syscall, user pointer validation, fail-closed behavior, dan least privilege |
| `@osdev-12-toolchain-devenv` | Build, host test, freestanding object, `nm/readelf/objdump`, QEMU/GDB workflow |
| `@osdev-14-cross-science` | Verification matrix, risk register, evidence-based readiness, dan laporan terstandar |

---

## 6. Pemeriksaan Kesiapan Hasil Praktikum Sebelumnya

Sebelum menulis source M10, mahasiswa wajib menjalankan pemeriksaan berikut dari root repository MCSOS. Bagian ini bertujuan memastikan kegagalan M10 tidak disebabkan oleh artefak lama yang belum stabil.

### 6.1 Pemeriksaan struktur repository

Perintah berikut memverifikasi bahwa struktur dasar dari M0–M9 tersedia. Jika nama direktori berbeda, sesuaikan path tetapi jangan menghapus bukti lama.

```bash
pwd
git status --short
find . -maxdepth 3 -type f   \( -name 'Makefile' -o -name 'kernel.c' -o -name 'scheduler.c' -o -name 'thread.c' -o -name 'idt.c' -o -name 'pmm.c' -o -name 'vmm.c' -o -name 'heap.c' \)   | sort
```

Indikator lulus: repository berada pada branch praktikum yang benar, `git status` tidak menunjukkan konflik merge, dan file M4–M9 dapat ditemukan. Jika ada perubahan tak terlacak, simpan dahulu sebagai commit atau stash sebelum melanjutkan.

### 6.2 Pemeriksaan M0/M1: toolchain dan metadata

Perintah berikut memastikan toolchain yang dipakai M10 sama dengan baseline M0/M1.

```bash
clang --version || true
gcc --version || true
ld --version | head -n 1
nm --version | head -n 1
readelf --version | head -n 1
objdump --version | head -n 1
qemu-system-x86_64 --version || true
gdb --version | head -n 1 || true
```

Indikator lulus: minimal satu compiler C tersedia, binutils tersedia, dan QEMU/GDB tersedia untuk smoke test. Jika `clang` tidak tersedia, pasang paket yang diperlukan di WSL 2. Jika `qemu-system-x86_64` tidak tersedia, instal `qemu-system-x86` dan `ovmf` sesuai distribusi WSL.

### 6.3 Pemeriksaan M2/M3: boot image, serial log, dan panic path

M10 membutuhkan boot path yang masih dapat mencapai kernel main dan panic path yang terbaca. Jalankan target build dan run yang sudah dibuat pada M2/M3.

```bash
make clean
make all
make run-qemu 2>&1 | tee logs/m10_preflight_qemu.log
```

Indikator lulus: serial log menampilkan banner MCSOS, versi milestone terakhir, dan tidak langsung triple fault. Jika QEMU hang tanpa log, kembali ke M2/M3: periksa bootloader config, linker script, entry symbol, dan serial init.

### 6.4 Pemeriksaan M4/M5: IDT, trap path, timer, dan vector bebas

Vector `0x80` akan dipakai untuk syscall pendidikan. Pastikan IDT dapat memasang handler tambahan dan trap path M4 tidak menganggap semua vector non-exception sebagai panic.

```bash
make test-idt || true
grep -R "idt_set_gate\|x86_64_trap_dispatch\|trap_frame\|IRQ0\|timer" -n kernel include | head -n 80
```

Indikator lulus: ada fungsi instalasi gate IDT, trap frame tersedia, dan dispatcher dapat mengenali vector tambahan. Jika semua vector selain exception langsung panic, tambahkan routing khusus vector `0x80` setelah kontrak M10 dipahami.

### 6.5 Pemeriksaan M6/M7: PMM/VMM dan batas user region

M10 memperkenalkan validasi user buffer. Validasi ini bergantung pada struktur virtual memory M7, meskipun praktikum wajib hanya memeriksa rentang sederhana.

```bash
make test-pmm || true
make test-vmm || true
grep -R "USER\|KERNEL_BASE\|HHDM\|PTE_USER\|PAGE_PRESENT\|page_fault" -n kernel include | head -n 120
```

Indikator lulus: konstanta layout virtual memory dapat ditemukan, page fault handler dapat mencetak alamat fault, dan PTE user/supervisor sudah mulai dipisahkan atau minimal didokumentasikan. Jika belum ada konstanta user range, M10 memakai region simulasi dahulu dan menandai user mode penuh sebagai non-scope.

### 6.6 Pemeriksaan M8: heap dan alokasi objek kernel

M10 tidak wajib menggunakan heap untuk tabel syscall, tetapi integrasi dengan scheduler/logging mungkin memakai heap. Pastikan heap tidak korup.

```bash
make test-heap || true
grep -R "kmalloc\|kfree\|heap_init" -n kernel include | head -n 80
```

Indikator lulus: host unit test heap lulus atau minimal kernel heap tidak panic saat boot. Jika heap belum stabil, gunakan tabel syscall statik dan hindari alokasi dinamis pada jalur syscall.

### 6.7 Pemeriksaan M9: scheduler callback

M10 akan membuat syscall `yield` dan `exit_thread`. Keduanya harus memanggil helper scheduler, bukan mengubah runqueue langsung dari syscall layer.

```bash
make test-scheduler || true
grep -R "sched_yield\|thread_exit\|context_switch\|runqueue\|scheduler" -n kernel include | head -n 120
```

Indikator lulus: ada fungsi yield atau rencana integrasi scheduler. Jika scheduler M9 belum punya `thread_exit`, M10 boleh memasang callback `exit_current` sebagai stub yang mencatat kode keluar dan memanggil panic terkontrol untuk tahap awal.

---

## 7. Saran Perbaikan Kendala dari M0–M9

| Gejala | Kemungkinan penyebab | Perbaikan konservatif |
|---|---|---|
| `clang: command not found` | Toolchain M1 belum lengkap | Instal clang di WSL 2; catat versi di laporan |
| `qemu-system-x86_64: command not found` | QEMU belum terpasang | Instal paket QEMU; ulangi smoke test M2 |
| Boot langsung reset | Triple fault akibat IDT/stack/paging | Jalankan QEMU dengan `-no-reboot -d int,cpu_reset`; cek M4/M7 |
| `nm -u` menunjukkan symbol libc | Source kernel memakai fungsi hosted | Ganti dengan implementasi kernel atau callback eksplisit |
| Syscall return salah | Register return tidak dipulihkan ke `rax` | Audit stub entry dengan `objdump -dr` |
| General protection fault saat `iretq` | Frame interrupt tidak sesuai privilege/stack | Jangan aktifkan ring 3; uji kernel-only terlebih dahulu |
| Page fault saat `copy_from_user` | Pointer tidak valid atau PTE user belum benar | Gunakan range check dan log alamat fault; jangan dereference sebelum check |
| Scheduler rusak setelah `yield` | Syscall memanggil scheduler saat lock/IRQ state tidak valid | Tambah precondition: syscall yield hanya dari task context |
| Hang setelah `int 0x80` | IDT gate vector 0x80 belum dipasang atau stub tidak cocok trap frame | Pasang gate eksplisit dan breakpoint di stub via GDB |
| Test host lulus tetapi QEMU gagal | Assembly/IDT/stack berbeda dari host path | Pisahkan bug dispatcher dari bug entry; audit trap frame |

---

## 8. Architecture and Design / Desain Teknis M10

### 8.1 Kontrak ABI syscall MCSOS M10

| Elemen | Kontrak M10 |
|---|---|
| Nomor syscall | `rax` |
| Argumen 0 | `rdi` |
| Argumen 1 | `rsi` |
| Argumen 2 | `rdx` |
| Argumen 3 | `r10` |
| Argumen 4 | `r8` |
| Argumen 5 | `r9` |
| Return value | `rax` |
| Error convention | Nilai negatif gaya errno internal: `-EINVAL`, `-ENOSYS`, `-EFAULT`, `-EBUSY` |
| Register clobber | Caller menganggap caller-save register dapat berubah |
| Stack | Tidak menggunakan red zone; kernel build memakai `-mno-red-zone` |
| Context | Syscall yang memodifikasi scheduler hanya boleh dari task context, bukan IRQ nested context |

Konvensi argumen keempat memakai `r10`, bukan `rcx`, agar ABI awal tetap kompatibel secara konseptual dengan jalur `syscall/sysret` masa depan karena instruksi `syscall` pada x86_64 memakai `rcx` dan `r11` untuk state return. M10 tetap memakai `int 0x80` sebagai checkpoint pendidikan karena lebih mudah dihubungkan ke IDT M4, tetapi desain ABI tidak dikunci pada `int 0x80` saja.

### 8.2 Tabel syscall minimum

| Nomor | Nama | Argumen | Return | Tujuan |
|---:|---|---|---|---|
| 0 | `ping` | tidak ada | magic `0x2605020A` | Smoke test dispatcher |
| 1 | `get_ticks` | tidak ada | tick counter atau `-EBUSY` | Integrasi timer M5 |
| 2 | `write_serial` | `buf`, `len` | jumlah byte atau error | Debug output terkendali |
| 3 | `yield` | tidak ada | 0 atau `-EBUSY` | Integrasi scheduler M9 |
| 4 | `exit_thread` | `code` | 0 atau `-EBUSY` | Stub lifecycle thread |

Tabel ini tidak dimaksudkan menjadi ABI final. Nomor syscall yang sudah dipakai tidak boleh diubah sembarangan setelah laporan M10, kecuali repository menambah file `docs/abi/syscall_abi_v2.md` yang menjelaskan breaking change.

### 8.3 Invariants wajib

1. `nr < MCSOS_SYS_MAX` sebelum indexing tabel syscall.
2. Entry tabel yang kosong harus mengembalikan `-ENOSYS`, bukan jump ke `NULL`.
3. Semua pointer dari caller diperlakukan tidak tepercaya sampai range check lulus.
4. Range check wajib mendeteksi overflow `addr + len - 1`.
5. `copy_from_user` tidak boleh membaca byte pertama sebelum validasi rentang lulus.
6. `yield` tidak boleh dipanggil dari interrupt context nested pada M10.
7. `exit_thread` tidak boleh melepas stack thread yang sedang dipakai sampai scheduler mempunyai teardown aman.
8. Stub assembly tidak boleh mengasumsikan red zone.
9. Jalur error harus mengembalikan nilai negatif yang terdokumentasi.
10. Log syscall tidak boleh mencetak isi buffer user tanpa batas panjang.

### 8.4 State machine syscall

```text
CALLER_PREPARES_REGS
  -> ENTER_SYSCALL_GATE
  -> BUILD_SYSCALL_FRAME
  -> VALIDATE_NR
  -> VALIDATE_ARGS
  -> CALL_HELPER
  -> STORE_RETURN
  -> RETURN_TO_CALLER
```

Transisi error:

```text
VALIDATE_NR fails     -> STORE_RETURN(-ENOSYS) -> RETURN_TO_CALLER
VALIDATE_ARGS fails   -> STORE_RETURN(-EINVAL/-EFAULT) -> RETURN_TO_CALLER
HELPER unavailable    -> STORE_RETURN(-EBUSY)  -> RETURN_TO_CALLER
UNRECOVERABLE BUG     -> panic with trap frame and syscall number
```

---

## 9. Struktur File yang Dibuat pada M10

Tambahkan file berikut ke repository. Path dapat disesuaikan dengan struktur MCSOS, tetapi nama interface harus konsisten agar unit test dan audit mudah dilakukan.

```text
include/mcsos/syscall.h
kernel/syscall/syscall.c
kernel/syscall/syscall_entry.S
tests/test_syscall_host.c
scripts/m10_preflight.sh
scripts/m10_qemu_smoke.sh
logs/.gitkeep
```

Jika repository Anda memakai struktur lain, misalnya `kernel/arch/x86_64/`, letakkan `syscall_entry.S` pada direktori arsitektur dan biarkan `syscall.c` pada direktori kernel generik.

---

## 10. Implementation Plan and Source Code / Rencana Implementasi dan Source Code M10

### 10.1 Header `include/mcsos/syscall.h`

File header mendefinisikan nomor syscall, status error, frame syscall, user region, callback operasi kernel, dan API dispatcher. Tidak ada dependency ke libc selain header freestanding standar `stdint.h` dan `stddef.h`.

```c
#ifndef MCSOS_SYSCALL_H
#define MCSOS_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define MCSOS_SYSCALL_ABI_VERSION 1u
#define MCSOS_SYSCALL_MAX_ARGS 6u

typedef enum mcsos_syscall_nr {
    MCSOS_SYS_PING = 0,
    MCSOS_SYS_GET_TICKS = 1,
    MCSOS_SYS_WRITE_SERIAL = 2,
    MCSOS_SYS_YIELD = 3,
    MCSOS_SYS_EXIT_THREAD = 4,
    MCSOS_SYS_MAX = 5
} mcsos_syscall_nr_t;

typedef enum mcsos_syscall_status {
    MCSOS_OK = 0,
    MCSOS_EINVAL = -22,
    MCSOS_ENOSYS = -38,
    MCSOS_EFAULT = -14,
    MCSOS_EPERM = -1,
    MCSOS_EBUSY = -16
} mcsos_syscall_status_t;

typedef struct mcsos_syscall_frame {
    uint64_t nr;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
    int64_t  ret;
} mcsos_syscall_frame_t;

typedef struct mcsos_user_region {
    uintptr_t base;
    uintptr_t limit;
} mcsos_user_region_t;

typedef struct mcsos_syscall_ops {
    uint64_t (*get_ticks)(void);
    void (*yield_current)(void);
    void (*exit_current)(int code);
    int64_t (*write_serial)(const char *buf, size_t len);
} mcsos_syscall_ops_t;

void mcsos_syscall_init(const mcsos_syscall_ops_t *ops);
void mcsos_syscall_set_user_region(mcsos_user_region_t region);
int mcsos_user_check_range(uintptr_t addr, size_t len);
int mcsos_copy_from_user(void *dst, const void *src, size_t len);
int64_t mcsos_syscall_dispatch(uint64_t nr, uint64_t arg0, uint64_t arg1,
                               uint64_t arg2, uint64_t arg3, uint64_t arg4,
                               uint64_t arg5);
void mcsos_syscall_dispatch_frame(mcsos_syscall_frame_t *frame);

#endif

```

**Kontrak**: header ini adalah boundary publik internal kernel. Perubahan nomor syscall harus dicatat pada dokumentasi ABI. `mcsos_syscall_frame_t` harus tetap sinkron dengan stub assembly. Jika field berubah, audit ulang offset assembly dengan `objdump -dr`.

### 10.2 Implementasi `kernel/syscall/syscall.c`

File ini mengimplementasikan tabel syscall, validasi nomor, validasi user range, copy loop sederhana, dan callback ke subsystem lain. Callback digunakan agar syscall layer tidak bergantung langsung pada scheduler, timer, atau serial driver tertentu.

```c
#include "mcsos/syscall.h"

static mcsos_syscall_ops_t g_ops;
static mcsos_user_region_t g_user_region;

static int64_t default_write_serial(const char *buf, size_t len) {
    (void)buf;
    return (int64_t)len;
}

void mcsos_syscall_init(const mcsos_syscall_ops_t *ops) {
    g_ops.get_ticks = 0;
    g_ops.yield_current = 0;
    g_ops.exit_current = 0;
    g_ops.write_serial = default_write_serial;
    if (ops != 0) {
        if (ops->get_ticks != 0) g_ops.get_ticks = ops->get_ticks;
        if (ops->yield_current != 0) g_ops.yield_current = ops->yield_current;
        if (ops->exit_current != 0) g_ops.exit_current = ops->exit_current;
        if (ops->write_serial != 0) g_ops.write_serial = ops->write_serial;
    }
}

void mcsos_syscall_set_user_region(mcsos_user_region_t region) {
    g_user_region = region;
}

int mcsos_user_check_range(uintptr_t addr, size_t len) {
    if (len == 0u) return 1;
    if (g_user_region.base == 0u || g_user_region.limit <= g_user_region.base) return 0;
    if (addr < g_user_region.base) return 0;
    if (addr > g_user_region.limit) return 0;
    uintptr_t last = addr + (uintptr_t)len - 1u;
    if (last < addr) return 0;
    if (last >= g_user_region.limit) return 0;
    return 1;
}

int mcsos_copy_from_user(void *dst, const void *src, size_t len) {
    if (len == 0u) return MCSOS_OK;
    if (dst == 0 || src == 0) return MCSOS_EINVAL;
    if (!mcsos_user_check_range((uintptr_t)src, len)) return MCSOS_EFAULT;
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < len; ++i) d[i] = s[i];
    return MCSOS_OK;
}

static int64_t sys_ping(uint64_t a0, uint64_t a1, uint64_t a2,
                        uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return 0x2605020AL;
}

static int64_t sys_get_ticks(uint64_t a0, uint64_t a1, uint64_t a2,
                             uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (g_ops.get_ticks == 0) return MCSOS_EBUSY;
    return (int64_t)g_ops.get_ticks();
}

static int64_t sys_write_serial(uint64_t ptr, uint64_t len, uint64_t a2,
                                uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (ptr == 0u) return MCSOS_EINVAL;
    if (len > 4096u) return MCSOS_EINVAL;
    if (!mcsos_user_check_range((uintptr_t)ptr, (size_t)len)) return MCSOS_EFAULT;
    return g_ops.write_serial((const char *)(uintptr_t)ptr, (size_t)len);
}

static int64_t sys_yield(uint64_t a0, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (g_ops.yield_current == 0) return MCSOS_EBUSY;
    g_ops.yield_current();
    return MCSOS_OK;
}

static int64_t sys_exit_thread(uint64_t code, uint64_t a1, uint64_t a2,
                               uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (g_ops.exit_current == 0) return MCSOS_EBUSY;
    g_ops.exit_current((int)code);
    return MCSOS_OK;
}

typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t);

static syscall_fn_t g_table[MCSOS_SYS_MAX] = {
    sys_ping,
    sys_get_ticks,
    sys_write_serial,
    sys_yield,
    sys_exit_thread
};

int64_t mcsos_syscall_dispatch(uint64_t nr, uint64_t arg0, uint64_t arg1,
                               uint64_t arg2, uint64_t arg3, uint64_t arg4,
                               uint64_t arg5) {
    if (nr >= (uint64_t)MCSOS_SYS_MAX) return MCSOS_ENOSYS;
    syscall_fn_t fn = g_table[nr];
    if (fn == 0) return MCSOS_ENOSYS;
    return fn(arg0, arg1, arg2, arg3, arg4, arg5);
}

void mcsos_syscall_dispatch_frame(mcsos_syscall_frame_t *frame) {
    if (frame == 0) return;
    frame->ret = mcsos_syscall_dispatch(frame->nr, frame->arg0, frame->arg1,
                                        frame->arg2, frame->arg3, frame->arg4,
                                        frame->arg5);
}

```

**Preconditions**: `mcsos_syscall_init` dipanggil sebelum syscall yang membutuhkan callback. User region diset sebelum `write_serial` atau `copy_from_user` dipakai. Jika user region belum valid, syscall yang memakai pointer harus gagal dengan `-EFAULT`.

**Memory ownership**: buffer user tetap milik caller. Kernel hanya membaca setelah validasi rentang. M10 belum mempunyai pinning page atau page-fault-assisted usercopy, sehingga tidak boleh mengklaim aman terhadap perubahan page table serentak atau malicious user penuh.

### 10.3 Stub entry `kernel/syscall/syscall_entry.S`

Stub ini adalah checkpoint pendidikan untuk menghubungkan vector `0x80` ke dispatcher. Stub ini telah dikompilasi dan diaudit sebagai object x86_64, tetapi tetap harus disesuaikan dengan trap frame M4 sebelum dipakai sebagai jalur runtime final.

```asm
.section .text
.global x86_64_syscall_int80_stub
.type x86_64_syscall_int80_stub, @function
.extern mcsos_syscall_dispatch_frame

# Educational checkpoint stub for vector 0x80 integration.
# Contract: M4 IDT must route vector 0x80 here only after the kernel has
# installed a compatible trap frame and stack discipline. This stub preserves
# caller-save inputs into mcsos_syscall_frame_t and returns through iretq.
x86_64_syscall_int80_stub:
    cld
    subq $64, %rsp
    movq %rax, 0(%rsp)
    movq %rdi, 8(%rsp)
    movq %rsi, 16(%rsp)
    movq %rdx, 24(%rsp)
    movq %r10, 32(%rsp)
    movq %r8,  40(%rsp)
    movq %r9,  48(%rsp)
    movq $0,   56(%rsp)
    movq %rsp, %rdi
    call mcsos_syscall_dispatch_frame
    movq 56(%rsp), %rax
    addq $64, %rsp
    iretq
.size x86_64_syscall_int80_stub, . - x86_64_syscall_int80_stub

```

**Kontrak kritis**: stub ini tidak boleh dipakai untuk ring 3 penuh sebelum GDT selector user, TSS, kernel stack per-CPU, IDT gate DPL, page table user/supervisor, dan return frame privilege transition diverifikasi. Pada M10, pakai sebagai smoke test kernel-only atau sebagai integrasi terkontrol pada IDT M4.

### 10.4 Host unit test `tests/test_syscall_host.c`

Host test memvalidasi logika dispatcher dan range check tanpa QEMU. Ini tidak membuktikan entry assembly benar, tetapi memisahkan bug logika C dari bug trap/assembly.

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mcsos/syscall.h"

static uint64_t fake_ticks(void) { return 12345u; }
static int g_yield_count = 0;
static int g_exit_code = 0;
static void fake_yield(void) { g_yield_count++; }
static void fake_exit(int code) { g_exit_code = code; }
static int64_t fake_write(const char *buf, size_t len) {
    assert(buf != NULL);
    assert(len == 5u);
    assert(memcmp(buf, "hello", 5u) == 0);
    return (int64_t)len;
}

int main(void) {
    char user_buf[16] = "hello";
    char kernel_buf[16] = {0};
    mcsos_syscall_ops_t ops = {
        .get_ticks = fake_ticks,
        .yield_current = fake_yield,
        .exit_current = fake_exit,
        .write_serial = fake_write
    };
    mcsos_syscall_init(&ops);
    mcsos_syscall_set_user_region((mcsos_user_region_t){
        .base = (uintptr_t)&user_buf[0],
        .limit = (uintptr_t)&user_buf[0] + sizeof(user_buf)
    });

    assert(mcsos_syscall_dispatch(MCSOS_SYS_PING,0,0,0,0,0,0) == 0x2605020AL);
    assert(mcsos_syscall_dispatch(MCSOS_SYS_GET_TICKS,0,0,0,0,0,0) == 12345);
    assert(mcsos_syscall_dispatch(MCSOS_SYS_WRITE_SERIAL,(uintptr_t)user_buf,5,0,0,0,0) == 5);
    assert(mcsos_copy_from_user(kernel_buf, user_buf, 5) == MCSOS_OK);
    assert(memcmp(kernel_buf, "hello", 5u) == 0);
    assert(mcsos_copy_from_user(kernel_buf, (void *)1, 5) == MCSOS_EFAULT);
    assert(mcsos_syscall_dispatch(999,0,0,0,0,0,0) == MCSOS_ENOSYS);
    assert(mcsos_syscall_dispatch(MCSOS_SYS_YIELD,0,0,0,0,0,0) == MCSOS_OK);
    assert(g_yield_count == 1);
    assert(mcsos_syscall_dispatch(MCSOS_SYS_EXIT_THREAD,7,0,0,0,0,0) == MCSOS_OK);
    assert(g_exit_code == 7);

    mcsos_syscall_frame_t frame = { .nr = MCSOS_SYS_GET_TICKS };
    mcsos_syscall_dispatch_frame(&frame);
    assert(frame.ret == 12345);

    puts("M10 syscall host tests passed");
    return 0;
}

```

### 10.5 Makefile target M10

Jika repository sudah mempunyai Makefile utama, gabungkan target-target berikut ke Makefile tersebut. Untuk validasi mandiri, Makefile berikut telah diuji dalam direktori pemeriksaan lokal.

```makefile
CC ?= clang
HOST_CC ?= clang
OBJDUMP ?= objdump
READELF ?= readelf
NM ?= nm
CFLAGS_COMMON := -Iinclude -Wall -Wextra -Werror -std=c17
KERNEL_CFLAGS := $(CFLAGS_COMMON) -target x86_64-elf -ffreestanding -fno-stack-protector -fno-builtin -mno-red-zone -O2 -g
HOST_CFLAGS := $(CFLAGS_COMMON) -O2 -g

all: build/test_syscall_host build/syscall.o build/syscall_entry.o build/m10_syscall_combined.o audit

build:
	mkdir -p build

build/test_syscall_host: tests/test_syscall_host.c kernel/syscall/syscall.c include/mcsos/syscall.h | build
	$(HOST_CC) $(HOST_CFLAGS) tests/test_syscall_host.c kernel/syscall/syscall.c -o $@

build/syscall.o: kernel/syscall/syscall.c include/mcsos/syscall.h | build
	$(CC) $(KERNEL_CFLAGS) -c kernel/syscall/syscall.c -o $@

build/syscall_entry.o: kernel/syscall/syscall_entry.S | build
	$(CC) -target x86_64-elf -c kernel/syscall/syscall_entry.S -o $@

build/m10_syscall_combined.o: build/syscall.o build/syscall_entry.o
	ld -r $^ -o $@

host-test: build/test_syscall_host
	./build/test_syscall_host

audit: build/m10_syscall_combined.o
	$(NM) -u build/m10_syscall_combined.o > build/nm_undefined.txt
	$(READELF) -h build/m10_syscall_combined.o > build/readelf_header.txt
	$(OBJDUMP) -dr build/m10_syscall_combined.o > build/objdump.txt
	sha256sum build/test_syscall_host build/m10_syscall_combined.o > build/SHA256SUMS
	grep -q "Machine:.*Advanced Micro Devices X86-64" build/readelf_header.txt
	grep -q "x86_64_syscall_int80_stub" build/objdump.txt
	grep -q "iretq" build/objdump.txt

test: host-test audit

clean:
	rm -rf build

```

---

## 11. Instruksi Langkah demi Langkah

### Langkah 1 — Buat branch M10

Buat branch terpisah agar perubahan syscall dapat di-review tanpa mencampur artefak M9. Branch ini menjadi titik rollback jika integrasi IDT menyebabkan boot gagal.

```bash
git checkout -b praktikum/m10-syscall-abi
mkdir -p include/mcsos kernel/syscall tests scripts logs
```

Indikator hasil: branch baru aktif dan direktori target tersedia.

### Langkah 2 — Tambahkan header syscall

Salin isi `include/mcsos/syscall.h` dari bagian 10.1. Header harus dikompilasi oleh host test dan freestanding kernel object.

```bash
$EDITOR include/mcsos/syscall.h
grep -n "MCSOS_SYS_MAX\|mcsos_syscall_dispatch" include/mcsos/syscall.h
```

Indikator hasil: enum syscall dan prototype dispatcher terlihat.

### Langkah 3 — Tambahkan dispatcher C

Salin isi `kernel/syscall/syscall.c` dari bagian 10.2. Jangan menambahkan `printf`, `malloc`, `memcpy`, atau fungsi libc lain pada file kernel.

```bash
$EDITOR kernel/syscall/syscall.c
grep -n "mcsos_user_check_range\|mcsos_syscall_dispatch" kernel/syscall/syscall.c
```

Indikator hasil: fungsi validasi rentang dan dispatcher tersedia.

### Langkah 4 — Tambahkan stub assembly entry

Salin isi `kernel/syscall/syscall_entry.S` dari bagian 10.3. Jika struktur trap frame M4 berbeda, jangan langsung memasang stub ke IDT runtime; kompilasi dan audit dulu.

```bash
$EDITOR kernel/syscall/syscall_entry.S
grep -n "x86_64_syscall_int80_stub\|iretq" kernel/syscall/syscall_entry.S
```

Indikator hasil: symbol stub dan instruksi return interrupt tersedia.

### Langkah 5 — Tambahkan host unit test

Host test memeriksa syscall `ping`, `get_ticks`, `write_serial`, `copy_from_user`, nomor invalid, `yield`, `exit_thread`, dan frame dispatch.

```bash
$EDITOR tests/test_syscall_host.c
```

Indikator hasil: test file dapat dikompilasi dengan compiler host.

### Langkah 6 — Tambahkan target Makefile

Gabungkan target Makefile M10 ke build system. Jika Makefile utama sudah punya variable berbeda, pertahankan prinsipnya: host test memakai compiler host; kernel object memakai target freestanding x86_64.

```bash
make m10-clean || true
make m10-host-test
make m10-freestanding
make m10-audit
```

Jika belum menambahkan target `m10-*`, gunakan validasi sementara berikut:

```bash
clang -Iinclude -Wall -Wextra -Werror -std=c17 -O2 -g   tests/test_syscall_host.c kernel/syscall/syscall.c   -o build/test_syscall_host
./build/test_syscall_host

clang -Iinclude -Wall -Wextra -Werror -std=c17   -target x86_64-elf -ffreestanding -fno-stack-protector   -fno-builtin -mno-red-zone -O2 -g   -c kernel/syscall/syscall.c -o build/syscall.o

clang -target x86_64-elf -c kernel/syscall/syscall_entry.S   -o build/syscall_entry.o

ld -r build/syscall.o build/syscall_entry.o   -o build/m10_syscall_combined.o

nm -u build/m10_syscall_combined.o
readelf -h build/m10_syscall_combined.o
objdump -dr build/m10_syscall_combined.o | grep -E "x86_64_syscall_int80_stub|iretq"
```

Indikator hasil: host test mencetak `M10 syscall host tests passed`, object freestanding berhasil dibuat, `readelf` menunjukkan `Machine: Advanced Micro Devices X86-64`, dan `objdump` menemukan `iretq`.

### Langkah 7 — Hubungkan dispatcher ke kernel init

Tambahkan inisialisasi syscall setelah timer/scheduler/logging siap. Gunakan callback agar syscall layer tidak mengimpor semua subsystem secara langsung.

```c
static uint64_t k_get_ticks(void) {
    return timer_ticks();
}

static void k_yield_current(void) {
    sched_yield();
}

static void k_exit_current(int code) {
    thread_exit(code);
}

static int64_t k_write_serial(const char *buf, size_t len) {
    return serial_write_bounded(buf, len);
}

void kernel_main(void) {
    /* ... init M0-M9 ... */
    mcsos_syscall_ops_t ops = {
        .get_ticks = k_get_ticks,
        .yield_current = k_yield_current,
        .exit_current = k_exit_current,
        .write_serial = k_write_serial,
    };
    mcsos_syscall_init(&ops);
}
```

Jika fungsi `timer_ticks`, `sched_yield`, `thread_exit`, atau `serial_write_bounded` belum ada, buat stub aman yang mengembalikan error atau hanya mencatat log. Jangan memanggil fungsi scheduler yang belum lulus M9.

### Langkah 8 — Tetapkan user region sementara

Untuk M10, user region boleh berupa rentang simulasi yang didokumentasikan. Setelah user process penuh tersedia, region ini harus diganti dengan validasi berbasis address space dan permission page table.

```c
#define MCSOS_USER_BASE  0x0000000000400000ULL
#define MCSOS_USER_LIMIT 0x0000800000000000ULL

mcsos_syscall_set_user_region((mcsos_user_region_t){
    .base = MCSOS_USER_BASE,
    .limit = MCSOS_USER_LIMIT,
});
```

Jika kernel belum mempunyai mapping user, jangan dereference pointer dari region ini dalam QEMU. Untuk smoke test kernel-only, gunakan buffer yang valid dan region simulasi yang mengelilingi buffer tersebut.

### Langkah 9 — Pasang vector 0x80 secara terkendali

Integrasi ke IDT harus mengikuti API M4. Contoh abstrak:

```c
extern void x86_64_syscall_int80_stub(void);

void syscall_arch_init(void) {
    /* Kernel-only smoke test: DPL 0. Ring 3 tahap lanjutan: DPL 3 setelah TSS/user stack valid. */
    idt_set_gate(0x80, x86_64_syscall_int80_stub, IDT_GATE_INTERRUPT, 0);
}
```

Indikator hasil: build tidak gagal, symbol stub ditemukan oleh linker, dan boot masih mencapai serial console. Jika terjadi triple fault, rollback pemasangan IDT vector 0x80 terlebih dahulu dan uji dispatcher C saja.

### Langkah 10 — Smoke test syscall tanpa ring 3

Buat fungsi smoke test yang memanggil dispatcher langsung terlebih dahulu. Jangan langsung memakai `int $0x80` sebelum dispatcher terbukti benar.

```c
void m10_syscall_smoke_direct(void) {
    int64_t r = mcsos_syscall_dispatch(MCSOS_SYS_PING, 0, 0, 0, 0, 0, 0);
    if (r != 0x2605020A) {
        panic("M10 syscall ping failed");
    }
    klog("[M10] syscall ping ok");
}
```

Setelah direct dispatch lulus, baru jalankan test entry sesuai struktur trap M4. Jika memakai inline assembly kernel-only, buat path debug yang dapat dimatikan dengan flag build.

```c
static inline long m10_int80_ping_kernel_only(void) {
    long ret;
    __asm__ volatile (
        "movq $0, %%rax
	"
        "int $0x80
	"
        : "=a"(ret)
        :
        : "rcx", "r11", "memory"
    );
    return ret;
}
```

Jika inline assembly ini menyebabkan fault, cek IDT gate, stub, stack alignment, dan return frame. Jangan lanjut ke ring 3.

### Langkah 11 — Jalankan QEMU smoke test

Jalankan QEMU dengan serial log dan no reboot. Perintah konkret bergantung pada pipeline M2. Contoh:

```bash
mkdir -p logs
make image
qemu-system-x86_64   -machine q35   -m 256M   -serial file:logs/m10_serial.log   -no-reboot -no-shutdown   -cdrom build/mcsos.iso
```

Indikator hasil minimum pada `logs/m10_serial.log`:

```text
[M10] syscall init
[M10] syscall ping ok
[M10] syscall get_ticks ok
[M10] syscall smoke done
```

Jika `get_ticks` mengembalikan `-EBUSY`, callback timer belum dipasang. Jika `yield` hang, scheduler callback belum aman; matikan test yield dan perbaiki M9.

### Langkah 12 — Debug dengan GDB jika QEMU gagal

QEMU gdbstub memungkinkan guest dihentikan sebelum boot berjalan, kemudian GDB dapat memeriksa register dan memasang breakpoint [3].

```bash
qemu-system-x86_64   -machine q35   -m 256M   -serial stdio   -no-reboot -no-shutdown   -s -S   -cdrom build/mcsos.iso
```

Pada terminal lain:

```bash
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) b mcsos_syscall_dispatch
(gdb) b x86_64_syscall_int80_stub
(gdb) c
(gdb) info registers rax rdi rsi rdx r10 r8 r9 rsp rip cs eflags
(gdb) x/16gx $rsp
```

Indikator hasil: breakpoint dispatcher tercapai untuk direct dispatch. Untuk entry `int 0x80`, breakpoint stub tercapai sebelum return. Jika tidak tercapai, gate IDT belum terpasang atau inline test tidak berjalan.

---

## 12. Checkpoint Buildable

| Checkpoint | Perintah | Bukti wajib |
|---|---|---|
| C1: host test | `make m10-host-test` | output `M10 syscall host tests passed` |
| C2: freestanding compile | `make m10-freestanding` | `build/syscall.o`, `build/syscall_entry.o` |
| C3: object audit | `make m10-audit` | `nm_undefined.txt`, `readelf_header.txt`, `objdump.txt` |
| C4: kernel link | `make all` | kernel ELF/ISO terbentuk |
| C5: QEMU direct dispatch | `make run-qemu` | log `[M10] syscall ping ok` |
| C6: QEMU entry smoke | `make run-qemu M10_INT80=1` atau target setara | log vector 0x80 atau breakpoint stub |
| C7: commit | `git commit` | commit hash di laporan |

---

## 13. Bukti Pemeriksaan Source Code Lokal

Source M10 pada dokumen ini telah diperiksa dengan kompilasi host, kompilasi freestanding x86_64, link relocatable, `nm`, `readelf`, `objdump`, dan checksum. Hasil pemeriksaan lokal menunjukkan host unit test lulus dan object gabungan dapat diaudit. Bukti ini tidak menggantikan QEMU runtime test pada repository mahasiswa.

### 13.1 Perintah yang dijalankan

```bash
cd /mnt/data/m10_check
make clean
make CC=clang HOST_CC=clang all
make CC=clang HOST_CC=clang test
```

### 13.2 Ringkasan hasil

```text
M10 syscall host tests passed
[OK] M10 sample source build and audit passed
```

### 13.3 `nm -u` object gabungan

`nm -u build/m10_syscall_combined.o` menghasilkan file kosong. Artinya tidak ada unresolved symbol pada object gabungan praktikum ini. Dalam integrasi repository nyata, symbol eksternal dari kernel utama boleh muncul jika memang sengaja di-link pada tahap akhir, tetapi harus dijelaskan.

```text
<kosong>
```

### 13.4 Header ELF hasil `readelf -h`

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
  Start of section headers:          12480 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           64 (bytes)
  Number of section headers:         27
  Section header string table index: 26

```

### 13.5 Checksum artefak pemeriksaan

```text
29b34600a985956cd325bf1bdc4e9a4e00a2e16d7ec1de4a7b30e50b187e49ff  build/test_syscall_host
4dcb58140ab77d7158d84ac1bafa94324bd8c114a649efc39b8c9be25d77a6d9  build/m10_syscall_combined.o

```

---

## 14. Validation Plan and Perintah Uji Lengkap untuk Mahasiswa

Jalankan perintah berikut dari clean checkout atau branch M10.

```bash
git status --short
make clean
make m10-host-test
make m10-freestanding
make m10-audit
make all
make run-qemu 2>&1 | tee logs/m10_qemu_run.log
sha256sum build/* 2>/dev/null | tee logs/m10_sha256.txt || true
git diff --stat
git status --short
```

Jika target `m10-*` belum tersedia, pakai perintah fallback dari Langkah 6. Semua output wajib disimpan ke laporan.

---

## 15. Security, Threat Model, Failure Modes, dan Diagnosis

| Failure mode | Gejala | Diagnosis | Perbaikan |
|---|---|---|---|
| Nomor syscall tidak dicek | Jump ke alamat random | GDB menunjukkan `nr >= max` tetapi tetap call | Tambah bound check dan `-ENOSYS` |
| User pointer invalid | Page fault pada `copy_from_user` | CR2 berisi alamat user invalid | Range check sebelum dereference |
| Overflow range | Buffer besar lolos validasi | `addr + len` wrap-around | Gunakan `last < addr` guard |
| Register argumen salah | Syscall menerima argumen tertukar | GDB register tidak cocok frame | Samakan offset assembly dan struct C |
| Return value hilang | Caller melihat nilai acak | `rax` ditimpa setelah dispatch | Simpan return ke frame dan restore ke `rax` |
| `iretq` fault | #GP/#SS/triple fault | Return frame tidak sesuai privilege | Uji kernel-only; jangan ring 3 dulu |
| Scheduler hang | `yield` tidak kembali | Callback scheduler men-switch saat lock/IRQ salah | Batasi yield pada task context |
| Deadlock logging | Syscall write memegang lock saat serial IRQ aktif | Log berhenti di tengah | Pakai lock order dan bounded write |
| ABI drift | Test lama gagal setelah menambah syscall | Nomor syscall berubah | Tambah versi ABI dan changelog |
| Build menarik libc | `nm -u` berisi `memcpy`, `printf` | Compiler generate call atau source memakai libc | `-fno-builtin`; buat helper kernel |

---

## 16. Prosedur Rollback

Jika M10 menyebabkan boot gagal, rollback bertahap berikut harus dilakukan secara disiplin.

```bash
# 1. Matikan smoke test int 0x80 terlebih dahulu.
git diff
git restore kernel/arch kernel/syscall || true
make clean && make all

# 2. Jika masih gagal, rollback integrasi IDT saja.
git checkout HEAD -- kernel/arch/x86_64/idt.c kernel/arch/x86_64/trap.c || true
make clean && make all

# 3. Jika dispatcher C dicurigai, jalankan host test terpisah.
make m10-host-test

# 4. Jika perlu kembali penuh ke M9.
git reset --hard <commit_m9_lulus>
```

Rollback yang benar harus disertai catatan: commit sumber masalah, gejala, log QEMU, keputusan rollback, dan rencana perbaikan.

---

## 17. Tugas Implementasi

### Tugas Wajib

1. Tambahkan `include/mcsos/syscall.h`.
2. Tambahkan `kernel/syscall/syscall.c`.
3. Tambahkan `kernel/syscall/syscall_entry.S` atau adaptasi ke trap framework M4.
4. Tambahkan host unit test dispatcher.
5. Tambahkan target build/audit M10 pada Makefile.
6. Integrasikan `mcsos_syscall_init` ke kernel init.
7. Buat direct dispatch smoke test `MCSOS_SYS_PING`.
8. Simpan log build, host test, object audit, dan QEMU serial.

### Tugas Pengayaan

1. Tambahkan trace counter per syscall.
2. Tambahkan syscall manifest `docs/abi/syscalls_v1.md`.
3. Tambahkan negative test untuk pointer overflow.
4. Tambahkan `sys_get_tid` yang membaca TCB M9.
5. Tambahkan opsi build `M10_INT80_SMOKE=1` untuk memicu `int $0x80` kernel-only.

### Tantangan Riset

1. Rancang transisi dari `int 0x80` ke `syscall/sysret` dengan MSR `STAR`, `LSTAR`, `FMASK`, `EFER.SCE`, TSS, dan return-frame policy.
2. Rancang usercopy yang tahan page fault dengan recovery label.
3. Rancang ABI versioning dan compatibility layer.
4. Rancang policy capability minimal untuk syscall privileged.

---

## 18. Pertanyaan Analisis

1. Mengapa nomor syscall harus dicek sebelum indexing tabel function pointer?
2. Mengapa pointer dari caller tidak boleh dipercaya walaupun QEMU smoke test lulus?
3. Apa perbedaan bukti host unit test dan bukti QEMU runtime test?
4. Mengapa `r10` dipilih sebagai argumen keempat pada ABI M10?
5. Mengapa syscall layer sebaiknya memanggil helper scheduler, bukan mengubah runqueue langsung?
6. Apa risiko memasang IDT gate vector `0x80` dengan DPL 3 sebelum TSS dan user stack siap?
7. Bagaimana cara membuktikan `copy_from_user` tidak membaca memori sebelum validasi lulus?
8. Apa failure mode paling berbahaya dari `iretq` pada transisi privilege?
9. Mengapa error code negatif lebih aman daripada panic untuk input syscall invalid?
10. Apa evidence minimum sebelum M10 boleh diberi label “siap uji QEMU”?

---

## 19. Rubrik Penilaian 100 Poin

| Komponen | Poin | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | Dispatcher benar, syscall table aman, error path valid, host test lulus, integrasi kernel tidak merusak boot |
| Kualitas desain dan invariants | 20 | ABI terdokumentasi, invariant usercopy jelas, callback subsystem tidak membentuk dependency siklik, state machine eksplisit |
| Pengujian dan bukti | 20 | Host unit test, freestanding compile, `nm/readelf/objdump`, QEMU serial log, checksum, commit hash |
| Debugging/failure analysis | 10 | Failure modes dianalisis, GDB/QEMU evidence tersedia, rollback dilakukan bila perlu |
| Keamanan dan robustness | 10 | Pointer validation, overflow check, fail-closed `-ENOSYS/-EFAULT`, tidak ada libc tersembunyi, tidak mengklaim aman penuh |
| Dokumentasi/laporan | 10 | Laporan lengkap, perintah reproducible, screenshot/log cukup, referensi IEEE, readiness review jujur |

---

## 20. Acceptance Criteria / Kriteria Lulus Praktikum

M10 dinyatakan lulus praktikum jika seluruh kriteria berikut terpenuhi:

1. Proyek dapat dibangun dari clean checkout.
2. Perintah build dan test terdokumentasi di laporan.
3. Host unit test syscall lulus.
4. Source kernel M10 dapat dikompilasi sebagai freestanding x86_64 object.
5. `nm -u` pada object gabungan M10 kosong atau semua symbol eksternal dijelaskan sebagai dependency kernel final.
6. `readelf -h` menunjukkan object target x86_64 yang benar.
7. `objdump -dr` menunjukkan symbol entry syscall dan instruksi return interrupt jika stub dipakai.
8. QEMU boot atau test target berjalan deterministik sampai log M10 minimal.
9. Serial log disimpan pada `logs/m10_serial.log` atau nama setara.
10. Panic path tetap terbaca setelah integrasi M10.
11. Tidak ada warning kritis pada source M10 dengan `-Wall -Wextra -Werror`.
12. Perubahan Git dikomit.
13. Mahasiswa dapat menjelaskan desain ABI, failure mode, dan alasan pembatasan M10.
14. Laporan berisi screenshot/log yang cukup, bukan hanya klaim lisan.

---

## 21. Template Laporan Praktikum M10

Gunakan template berikut agar format laporan tetap seragam untuk individu maupun kelompok.

```markdown
# Laporan Praktikum M10 — ABI System Call Awal dan Dispatcher Syscall MCSOS

## 1. Sampul
- Judul praktikum:
- Nama mahasiswa / kelompok:
- NIM:
- Kelas:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi: Pendidikan Teknologi Informasi
- Institusi: Institut Pendidikan Indonesia
- Tanggal:
- Repository:
- Commit hash:

## 2. Tujuan
Tuliskan capaian teknis dan konseptual M10.

## 3. Dasar Teori Ringkas
Jelaskan syscall, ABI, IDT vector, trap frame, pointer validation, usercopy, dan error convention.

## 4. Lingkungan
- OS host:
- WSL distro:
- Compiler dan versi:
- Linker/binutils:
- QEMU:
- GDB:
- Target arsitektur:
- Bootloader:
- Commit M9 lulus:
- Commit M10:

## 5. Desain
- ABI register:
- Syscall table:
- Error code:
- User region:
- Callback scheduler/timer/serial:
- Diagram alur syscall:
- Invariants:
- Batasan:

## 6. Langkah Kerja
Tuliskan perintah, perubahan file, dan alasan teknis setiap langkah.

## 7. Hasil Uji
| Uji | Perintah | Hasil | Bukti |
|---|---|---|---|
| Host test | | | |
| Freestanding compile | | | |
| nm audit | | | |
| readelf audit | | | |
| objdump audit | | | |
| QEMU smoke | | | |
| GDB debug bila perlu | | | |

## 8. Analisis
Bahas keberhasilan, bug, kegagalan, dan perbandingan dengan desain.

## 9. Keamanan dan Reliability
Bahas risiko pointer user, invalid syscall number, overflow, scheduler reentrancy, lock order, dan mitigasinya.

## 10. Failure Modes dan Rollback
Tuliskan failure mode yang ditemukan dan prosedur rollback yang dilakukan.

## 11. Readiness Review
Pilih satu: belum siap uji, siap uji QEMU, siap demonstrasi praktikum terbatas. Jelaskan bukti dan batasannya.

## 12. Kesimpulan
Tuliskan apa yang berhasil, apa yang belum, dan rencana M11.

## 13. Lampiran
- Potongan kode penting
- Diff ringkas
- Log build penuh
- Log QEMU penuh
- Output `nm/readelf/objdump`
- Screenshot
- Referensi
```

---

## 22. Readiness Review

| Area | Status minimum setelah M10 | Bukti |
|---|---|---|
| Toolchain | Siap uji bila clean build berhasil | versi tool, log build |
| Dispatcher | Siap uji bila host test lulus | output host test |
| Freestanding object | Siap uji bila object x86_64 valid | `readelf`, `nm`, `objdump` |
| Entry `int 0x80` | Siap smoke test bila IDT dan stub cocok | QEMU log/GDB breakpoint |
| User pointer | Belum security-complete | range/overflow test saja |
| Scheduler syscall | Siap terbatas bila callback M9 aman | log `yield`/`exit` terkendali |
| Ring 3 | Belum siap | non-scope M10 |
| SMP | Belum siap | non-scope M10 |
| Release | Belum siap | perlu user mode, fuzzing, security review |

**Keputusan readiness M10**: hasil praktikum hanya boleh dinilai **siap uji QEMU untuk syscall dispatcher awal dan smoke test ABI kernel-side** jika semua checkpoint lulus. Hasil M10 belum boleh disebut siap produksi, aman penuh, atau kompatibel POSIX/Linux.

---

## 23. Rencana Lanjutan M11

M11 disarankan mengembangkan **user-mode bring-up terbatas**: GDT selector user, TSS kernel stack, page table user/supervisor, return-to-user path, program user minimal, dan syscall dari ring 3. Gate M11 harus lebih ketat daripada M10 karena mulai ada boundary privilege nyata.

---

## References

[1] Intel Corporation, “Intel® 64 and IA-32 Architectures Software Developer Manuals,” Intel Developer Zone, updated Apr. 6, 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

[2] x86 psABIs Project, “x86-64 psABI,” GitLab, created Mar. 1, 2019. [Online]. Available: https://gitlab.com/x86-psABIs/x86-64-ABI

[3] QEMU Project, “GDB usage,” QEMU documentation. [Online]. Available: https://qemu-project.gitlab.io/qemu/system/gdb.html

[4] LLVM Project, “Clang command line argument reference,” Clang documentation. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html

[5] Linux Kernel Documentation, “Adding a New System Call,” kernel.org documentation. [Online]. Available: https://www.kernel.org/doc/html/latest/process/adding-syscalls.html

[6] Linux Kernel Documentation, “Lock types and their rules,” kernel.org documentation. [Online]. Available: https://www.kernel.org/doc/html/latest/locking/locktypes.html
