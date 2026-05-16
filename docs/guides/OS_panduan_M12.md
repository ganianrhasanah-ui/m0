# OS_panduan_M12.md

# Panduan Praktikum M12 — Sinkronisasi Kernel Awal: Spinlock, Mutex Kooperatif, Lock-Order Validator, dan Diagnosis Race/Deadlock pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M12  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: *siap uji QEMU untuk sinkronisasi kernel awal single-core menuju SMP*, bukan siap produksi, bukan bukti bebas deadlock, dan bukan bukti race-free penuh.

---

## 1. Ringkasan Praktikum

Praktikum M12 melanjutkan M0 sampai M11. M9 memperkenalkan kernel thread dan scheduler round-robin kooperatif. M10 menambahkan ABI system call awal dan validasi argumen. M11 menambahkan parser ELF64 user program loader awal dan rencana process image. Setelah kernel memiliki lebih dari satu alur eksekusi konseptual—interrupt, scheduler, syscall, loader, allocator, dan kelak proses user—MCSOS memerlukan fondasi sinkronisasi yang eksplisit.

M12 membuat tiga komponen inti. Pertama, **spinlock freestanding x86_64** berbasis operasi atomik acquire/release untuk melindungi struktur data pendek yang tidak boleh tidur. Kedua, **mutex kooperatif awal** dengan owner semantics untuk jalur task context yang kelak dapat dihubungkan dengan scheduler dan wait queue. Ketiga, **lock-order validator sederhana** bergaya lockdep untuk mendeteksi rekursi lock, pelepasan lock tidak sesuai urutan LIFO, dan akuisisi lock dengan urutan kelas yang menurun. Desain ini belum menggantikan lockdep Linux; tujuannya adalah menyediakan scaffolding praktikum yang dapat diaudit, diuji, dan diperluas.

Rujukan teknis utama M12 adalah Intel SDM untuk konsekuensi x86_64, multiprocessor support, interrupt/exception, dan instruksi sinkronisasi; dokumentasi Linux kernel untuk kategori lock, aturan konteks spinlock/mutex, lockdep, dan owner semantics; GCC `__atomic` builtins untuk model acquire/release; Clang command-line reference untuk kompilasi freestanding; GNU Binutils untuk `nm`, `readelf`, dan `objdump`; serta QEMU gdbstub untuk debugging guest OS [1]–[8].

Keberhasilan M12 tidak boleh ditulis sebagai “MCSOS bebas race/deadlock”. Kriteria minimum M12 adalah: readiness M0–M11 terdokumentasi, unit test host lulus, object freestanding x86_64 berhasil dikompilasi, audit `nm` tidak menunjukkan unresolved runtime helper pada object sinkronisasi, `readelf` menunjukkan ELF64 relocatable object, `objdump` menunjukkan operasi atomik/loop spin yang dapat diaudit, checksum artefak tersimpan, dan integrasi QEMU dapat diuji ulang pada WSL 2 mahasiswa.

---

## 2. Assumptions and Target / Asumsi Target, Batasan, dan Non-Goals

| Aspek | Keputusan M12 |
|---|---|
| Arsitektur | x86_64 long mode |
| Host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Bahasa | C17 freestanding untuk kernel; C17 hosted untuk host unit test |
| Compiler utama | Clang untuk target `x86_64-elf`; `cc`/GCC/Clang untuk host test |
| Komponen baru | `mcs_spinlock_t`, `mcs_mutex_t`, `mcs_lockdep_state_t` |
| Model concurrency praktikum | Single-core awal, interrupt-aware design, siap diperluas menuju SMP |
| Lock order | Ranking kelas lock monoton naik; release LIFO |
| Integrasi wajib | M4 trap, M5 timer, M6 PMM, M7 VMM, M8 heap, M9 scheduler, M10 syscall, M11 loader |
| Non-goals | Futex, priority inheritance penuh, RCU, rwlock, seqlock, lock-free queue, SMP AP bring-up penuh, preemptive scheduler final, dan pembuktian formal race freedom |

### 2.1 Goals

M12 bertujuan membuat mahasiswa mampu menjelaskan perbedaan spinlock dan mutex, memilih primitive berdasarkan konteks eksekusi, menerapkan acquire/release ordering secara minimal, menolak recursive lock, mendeteksi pelanggaran lock ordering, membuat unit test race menggunakan thread host, dan menyiapkan integrasi sinkronisasi ke subsystem MCSOS berikutnya.

### 2.2 Non-Goals

M12 tidak membangun scheduler SMP penuh. M12 juga tidak membuat lock yang aman untuk seluruh kelas interrupt/NMI/SMM. Primitive yang dibuat merupakan fondasi pendidikan. Untuk production-grade kernel, desain harus diperluas dengan per-CPU state, interrupt disable/restore yang benar, preemption control, wait queue, timeout, priority inheritance, lockdep graph, tracing, fuzzing, dan stress test multi-core yang jauh lebih luas.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M12, mahasiswa mampu:

1. Membedakan data race, race condition, deadlock, livelock, starvation, dan priority inversion.
2. Menjelaskan perbedaan spinlock, mutex, interrupt masking, dan preemption control.
3. Menjelaskan mengapa spinlock hanya cocok untuk critical section pendek.
4. Menjelaskan mengapa sleeping mutex tidak boleh diambil dari interrupt context.
5. Mengimplementasikan spinlock dengan `__atomic_exchange_n(..., __ATOMIC_ACQUIRE)` dan unlock dengan `__atomic_store_n(..., __ATOMIC_RELEASE)`.
6. Mengimplementasikan mutex kooperatif awal dengan owner checking dan rekursi ditolak.
7. Membuat lock-order validator sederhana dengan invariant monoton naik dan release LIFO.
8. Menulis host unit test yang memverifikasi race-protected counter, owner semantics, recursive rejection, dan order violation.
9. Mengompilasi source sinkronisasi sebagai object freestanding x86_64.
10. Mengaudit `nm -u`, `readelf -h`, `objdump -d`, dan checksum artefak.
11. Menjelaskan failure modes sinkronisasi: deadlock, recursive acquire, unlock non-owner, lock-order inversion, interrupt reentry, starvation, dan missed wakeup.
12. Menulis laporan praktikum dengan bukti yang dapat direproduksi.

---

## 4. Prasyarat Teori

| Materi | Kebutuhan dalam M12 |
|---|---|
| Atomics | Memahami acquire, release, relaxed, exchange, compare-exchange |
| x86_64 | Memahami efek instruksi locked/atomic dan konteks interrupt |
| Scheduler | Memahami perbedaan busy waiting dan blocking |
| Interrupt | Memahami mengapa interrupt handler tidak boleh tidur |
| Memory model | Memahami urutan akses data lintas core/interrupt |
| Data structure invariants | Memahami stack lock-held dan ordering monotonic |
| Debugging | Memahami bukti dari log, disassembly, symbol table, dan checksum |

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M12 |
|---|---|
| `osdev-general` | Readiness gate, roadmap, acceptance criteria, dan batas klaim |
| `osdev-01-computer-foundation` | Invariant, state machine, deadlock model, dan test obligation |
| `osdev-02-low-level-programming` | Atomics, ABI, freestanding C, undefined behavior, dan disassembly audit |
| `osdev-03-computer-and-hardware-architecture` | x86_64 memory ordering, interrupt context, multiprocessor assumptions |
| `osdev-04-kernel-development` | Scheduler interaction, lock ownership, panic path, dan observability |
| `osdev-07-os-security` | Privilege boundary dan risiko race sebagai vulnerability class |
| `osdev-12-toolchain-devenv` | Build reproducibility, object audit, QEMU/GDB workflow |
| `osdev-14-cross-science` | Reliability, failure mode analysis, dan verification matrix |

---

## 6. Alat dan Versi yang Harus Dicatat

Mahasiswa wajib mencatat versi aktual pada laporan, bukan menyalin contoh versi berikut.

```bash
uname -a
cat /etc/os-release
clang --version || true
cc --version | head -n 1 || true
make --version | head -n 1
qemu-system-x86_64 --version | head -n 1 || true
gdb --version | head -n 1 || true
nm --version | head -n 1 || true
readelf --version | head -n 1 || true
objdump --version | head -n 1 || true
git --version
```

Indikator lulus bagian ini adalah laporan memuat versi toolchain, target triple, lokasi repository, commit hash sebelum dan sesudah praktikum, serta log eksekusi perintah.

---

## 7. Pemeriksaan Kesiapan M0–M11

Sebelum menambahkan source M12, mahasiswa harus memastikan hasil praktikum sebelumnya tidak rusak. Jalankan dari root repository MCSOS.

```bash
git status --short
git branch --show-current
git log --oneline -5
```

Jika `git status --short` tidak kosong, simpan perubahan terlebih dahulu dalam commit atau stash. M12 menyentuh primitive kernel yang akan dipakai banyak subsystem; pengerjaan pada working tree kotor akan menyulitkan rollback.

### 7.1 Checklist readiness bertingkat

| Modul | Syarat sebelum M12 | Perintah bukti minimum | Kendala umum | Saran perbaikan |
|---|---|---|---|---|
| M0 | WSL 2, repository, struktur dokumentasi, dan baseline toolchain tersedia | `uname -a`, `git status`, `clang --version` | Path repository berada di `/mnt/c` sehingga build lambat | Pindahkan repository ke filesystem Linux WSL, misalnya `~/mcsos` |
| M1 | Toolchain audit, proof compile, reproducibility metadata tersedia | `make m1-check` atau script M1 setara | Target triple salah | Pastikan `clang -target x86_64-elf` digunakan untuk kernel object |
| M2 | Kernel boot image/ISO siap smoke test | `make run` atau `make qemu-smoke` | OVMF/Limine tidak ditemukan | Periksa paket OVMF dan path bootloader; ulangi fetch Limine sesuai M2 |
| M3 | Logging dan panic path tersedia | log serial memuat banner/panic terkontrol | Panic tidak terlihat | Pastikan serial QEMU diarahkan ke file dan early console aktif |
| M4 | IDT, exception stub, dan trap dispatcher tersedia | `objdump` memuat `lidt`/`iretq`; uji `int3` | Triple fault saat exception | Audit stack, selector, IDT gate type, dan ISR stub alignment |
| M5 | PIC/PIT/timer tick atau jalur interrupt awal tersedia | log tick deterministik | Interrupt storm | Periksa EOI PIC, mask IRQ, dan frekuensi PIT |
| M6 | PMM bitmap allocator lulus invariant test | host/unit test PMM | Frame reserved teralokasi | Audit parsing memory map dan alignment frame 4 KiB |
| M7 | VMM awal, page table, page fault diagnostics tersedia | page table dump, CR2 pada fault | Page fault saat akses heap | Periksa HHDM/direct map dan permission PTE |
| M8 | Kernel heap awal tersedia | host unit test allocator | Free-list korup | Periksa alignment, coalescing, dan double-free detection |
| M9 | Kernel thread dan scheduler kooperatif awal tersedia | context switch audit dan log scheduler | Stack thread rusak | Audit ABI callee-saved registers dan stack alignment 16 byte |
| M10 | Syscall dispatcher dan validasi argumen tersedia | syscall unit test dan `int 0x80` path | Handler salah nomor syscall | Periksa ABI register, trap frame, dan return value |
| M11 | ELF64 loader awal dan process image plan tersedia | ELF parser host test dan freestanding object audit | Loader menerima segment berbahaya | Periksa overflow, W^X, user range, dan `PT_LOAD` bounds |

### 7.2 Preflight M12

Perintah berikut membuat folder bukti M12 tanpa mengubah source lama. Tujuannya adalah memastikan toolchain, Git, dan artefak dasar tersedia sebelum implementasi sinkronisasi dimulai.

```bash
mkdir -p evidence/M12
{
  date -Is
  uname -a
  clang --version | head -n 1 || true
  cc --version | head -n 1 || true
  make --version | head -n 1
  git rev-parse --short HEAD
  git status --short
} | tee evidence/M12/preflight.log
```

Kriteria lulus preflight adalah `preflight.log` dibuat, commit hash tercatat, dan tidak ada perubahan tak terjelaskan pada working tree.

---

## 8. Konsep Inti M12

### 8.1 Spinlock

Spinlock adalah lock single-holder yang menunggu dengan busy-wait. Pada kernel pendidikan, spinlock digunakan hanya untuk critical section pendek, misalnya update counter, runqueue sederhana, allocator metadata kecil, atau statistik. Spinlock tidak boleh digunakan untuk jalur yang memanggil operasi yang dapat tidur, melakukan I/O lambat, menunggu disk, atau mengalokasikan memori dengan kemungkinan blocking.

Invariant spinlock M12:

1. Nilai `locked == 0` berarti lock bebas.
2. Nilai `locked == 1` berarti lock sedang dimiliki tepat satu eksekusi logis.
3. Acquire berhasil harus memberikan ordering acquire terhadap data dalam critical section.
4. Release harus memberikan ordering release agar update critical section terlihat sebelum lock dilepas.
5. Null pointer tidak boleh menyebabkan dereference.
6. Critical section wajib sesingkat mungkin dan tidak boleh memanggil fungsi blocking.

### 8.2 Mutex kooperatif awal

Mutex M12 belum memiliki wait queue penuh. Mutex ini merupakan owner-aware primitive yang menolak rekursi dan menolak unlock oleh non-owner. Pada modul berikutnya, mutex ini dapat diperluas agar thread yang gagal lock masuk wait queue lalu scheduler memilih thread lain.

Invariant mutex M12:

1. `locked == 0` berarti tidak ada owner.
2. `locked == 1` berarti `owner != 0`.
3. Owner yang sama tidak boleh mengambil mutex yang sama secara rekursif.
4. Hanya owner yang boleh unlock.
5. Unlock harus menghapus owner sebelum lock menjadi bebas.
6. Error path tidak boleh mengubah state lock secara parsial.

### 8.3 Lock-order validator sederhana

Lock-order validator M12 memakai model ranking kelas lock. Jika thread memegang lock kelas 20, ia tidak boleh mengambil lock kelas 10. Jika lock dilepas, urutannya harus LIFO. Model ini sengaja lebih sederhana daripada lockdep Linux, tetapi cukup untuk mengajarkan prinsip dasar: deadlock sering muncul karena dua jalur mengambil lock yang sama dengan urutan berbeda.

Invariant lock-order M12:

1. Stack `held_class[]` merepresentasikan lock yang sedang dipegang oleh satu thread atau satu konteks eksekusi.
2. Akuisisi kelas lock yang sama dua kali ditolak sebagai recursive acquire.
3. Akuisisi lock dengan kelas lebih kecil dari lock paling atas ditolak sebagai lock-order inversion.
4. Release wajib melepas lock paling atas.
5. Setiap pelanggaran menaikkan `violation_count` sebagai bukti observability.

---

## 9. Arsitektur Ringkas

```text
MCSOS M12 synchronization layer

thread/trap context
   |
   |-- lockdep_state per-thread/per-context   [M12: explicit object, M13+: per-thread]
   |       |-- held_class[]
   |       |-- depth
   |       '-- violation_count
   |
   |-- spinlock: atomic exchange acquire / store release
   |       '-- critical section pendek, non-blocking
   |
   '-- mutex: try-lock owner-aware
           '-- kandidat wait queue pada modul berikutnya

M12 evidence path:
source -> host unit test -> freestanding object -> nm/readelf/objdump -> checksum -> QEMU smoke integration
```

---

## 9A. Architecture and Design / Arsitektur dan Desain Kernel

Desain M12 memisahkan tiga interface: API spinlock untuk critical section non-blocking, API mutex untuk owner-aware locking pada task context, dan API lockdep-state untuk validasi urutan lock. Objek sinkronisasi tidak memiliki alokasi dinamis; pemilik memori lock wajib subsystem pemanggil. Kontrak ini mencegah allocator recursion pada M8 dan memudahkan penggunaan sebelum heap penuh siap.

## 9B. Interfaces, API, ABI, dan Integrasi Syscall

Interface M12 berada pada `include/mcs_sync.h`. API publiknya adalah `mcs_spin_init`, `mcs_spin_try_lock`, `mcs_spin_lock`, `mcs_spin_unlock`, `mcs_mutex_try_lock`, `mcs_mutex_unlock`, `mcs_lockdep_before_acquire`, dan `mcs_lockdep_after_release`. ABI syscall M10 tidak berubah pada M12; perubahan M12 berada di ABI internal kernel. Jika kelak syscall atau loader M11 mengambil lock, wrapper syscall wajib memastikan user pointer sudah divalidasi sebelum lock global diambil agar error path tidak meninggalkan lock terkunci.

## 9C. Invariants and Correctness / Invariant dan Kebenaran

Correctness M12 bertumpu pada invariant berikut: setiap lock memiliki state terdefinisi sebelum dipakai; acquire spinlock bersifat atomic; unlock spinlock hanya melakukan release setelah critical section selesai; mutex hanya dapat dilepas owner; lockdep-state mengikuti stack LIFO; lock class tidak boleh turun ketika nested lock diambil; dan setiap violation harus terukur melalui `violation_count` atau panic/log pada integrasi kernel.

## 9D. Security and Threat Model / Keamanan dan Model Ancaman

Threat model M12 mencakup bug internal kernel, bukan attacker user penuh. Risiko utama adalah race yang merusak metadata PMM/VMM/heap, deadlock yang mematikan scheduler, unlock non-owner yang membuka privilege boundary secara tidak langsung, serta interrupt reentry yang mengambil lock yang sedang dipegang task context. Mitigasi minimum adalah lock hierarchy, owner check, negative test, serial log, panic path, dan audit `objdump` untuk memastikan operasi atomik target benar-benar muncul.

## 9E. Implementation Plan / Rencana Implementasi

Implementasi dilakukan bertahap: buat header kontrak, implementasikan lockdep-state, implementasikan spinlock atomik, implementasikan mutex owner-aware, jalankan host unit test, kompilasi object freestanding, audit object, integrasikan self-test kernel, jalankan QEMU smoke test, lalu commit perubahan. Setiap tahap memiliki rollback point sehingga kegagalan tidak mencemari modul M0–M11.

## 9F. Validation Plan / Rencana Validasi

Validasi M12 mencakup unit test host untuk positive dan negative case, stress counter dengan pthread, freestanding compile untuk target x86_64, `nm -u` untuk runtime dependency, `readelf` untuk format object, `objdump` untuk instruksi atomic/spin loop, checksum untuk artefak, QEMU smoke test, dan GDB workflow untuk boot hang atau deadlock. Validasi ini belum membuktikan race freedom penuh; ia hanya memenuhi gate praktikum M12.

## 9G. Reproducibility, CI Matrix, dan Supply-Chain Evidence

Agar hasil reproducible, semua perintah build harus dijalankan dari clean checkout, versi toolchain dicatat, target triple dinyatakan eksplisit, build log disimpan, dan checksum artefak dicatat. Untuk CI awal, minimal matrix adalah host unit test dan freestanding object compile. Supply-chain evidence minimum adalah commit hash, `sha256sum`, dan catatan sumber toolchain. Clean rebuild dianggap lulus jika `make -f Makefile.m12 clean && make -f Makefile.m12 all CC=clang` menghasilkan test pass dan artefak audit baru tanpa warning kritis.

---

## 10. Instruksi Implementasi Langkah demi Langkah

### 10.1 Buat branch M12

Branch terpisah memudahkan rollback apabila sinkronisasi menyebabkan boot hang atau test lain gagal.

```bash
git checkout -b praktikum/m12-sync
mkdir -p include kernel/sync tests scripts evidence/M12
```

### 10.2 Tambahkan header `include/mcs_sync.h`

Header ini mendefinisikan kontrak data structure dan error code. Header tidak boleh bergantung pada libc hosted. Semua tipe berasal dari header C freestanding yang lazim tersedia pada toolchain kernel (`stdint.h`, `stddef.h`, `stdbool.h`).

```bash
cat > include/mcs_sync.h <<'EOF'
#ifndef MCS_SYNC_H
#define MCS_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MCS_LOCKDEP_MAX_HELD 16u
#define MCS_LOCK_NAME_MAX 32u

#define MCS_SYNC_OK 0
#define MCS_SYNC_EINVAL (-22)
#define MCS_SYNC_EBUSY (-16)
#define MCS_SYNC_EPERM (-1)
#define MCS_SYNC_EDEADLK (-35)
#define MCS_SYNC_EOVERFLOW (-75)

typedef struct mcs_lockdep_state {
    uint32_t held_class[MCS_LOCKDEP_MAX_HELD];
    const char *held_name[MCS_LOCKDEP_MAX_HELD];
    uint32_t depth;
    uint32_t violation_count;
} mcs_lockdep_state_t;

typedef struct mcs_spinlock {
    volatile uint32_t locked;
    uint32_t class_id;
    const char *name;
} mcs_spinlock_t;

typedef struct mcs_mutex {
    volatile uint32_t locked;
    uint64_t owner;
    uint32_t class_id;
    const char *name;
} mcs_mutex_t;

void mcs_lockdep_init(mcs_lockdep_state_t *state);
int mcs_lockdep_before_acquire(mcs_lockdep_state_t *state, uint32_t class_id, const char *name);
int mcs_lockdep_after_release(mcs_lockdep_state_t *state, uint32_t class_id, const char *name);
bool mcs_lockdep_is_held(const mcs_lockdep_state_t *state, uint32_t class_id);

void mcs_spin_init(mcs_spinlock_t *lock, uint32_t class_id, const char *name);
bool mcs_spin_try_lock(mcs_spinlock_t *lock);
void mcs_spin_lock(mcs_spinlock_t *lock);
void mcs_spin_unlock(mcs_spinlock_t *lock);
bool mcs_spin_is_locked(const mcs_spinlock_t *lock);

void mcs_mutex_init(mcs_mutex_t *mutex, uint32_t class_id, const char *name);
int mcs_mutex_try_lock(mcs_mutex_t *mutex, uint64_t owner_id);
int mcs_mutex_unlock(mcs_mutex_t *mutex, uint64_t owner_id);
bool mcs_mutex_is_locked(const mcs_mutex_t *mutex);
uint64_t mcs_mutex_owner(const mcs_mutex_t *mutex);

#endif
EOF
```

### 10.3 Tambahkan lock-order validator `kernel/sync/lockdep.c`

Validator ini bukan lockdep penuh. Ia hanya memeriksa rank monotonic, recursive acquire, depth overflow, dan release LIFO. Keuntungan pendekatan ini adalah mahasiswa dapat memahami invariant secara eksplisit sebelum berhadapan dengan graph dependency penuh.

```bash
cat > kernel/sync/lockdep.c <<'EOF'
#include "mcs_sync.h"

void mcs_lockdep_init(mcs_lockdep_state_t *state) {
    if (state == 0) {
        return;
    }
    for (uint32_t i = 0; i < MCS_LOCKDEP_MAX_HELD; i++) {
        state->held_class[i] = 0;
        state->held_name[i] = 0;
    }
    state->depth = 0;
    state->violation_count = 0;
}

bool mcs_lockdep_is_held(const mcs_lockdep_state_t *state, uint32_t class_id) {
    if (state == 0 || class_id == 0) {
        return false;
    }
    for (uint32_t i = 0; i < state->depth && i < MCS_LOCKDEP_MAX_HELD; i++) {
        if (state->held_class[i] == class_id) {
            return true;
        }
    }
    return false;
}

int mcs_lockdep_before_acquire(mcs_lockdep_state_t *state, uint32_t class_id, const char *name) {
    if (state == 0 || class_id == 0) {
        return MCS_SYNC_EINVAL;
    }
    if (state->depth >= MCS_LOCKDEP_MAX_HELD) {
        state->violation_count++;
        return MCS_SYNC_EOVERFLOW;
    }
    for (uint32_t i = 0; i < state->depth; i++) {
        if (state->held_class[i] == class_id) {
            state->violation_count++;
            return MCS_SYNC_EDEADLK;
        }
    }
    if (state->depth > 0) {
        uint32_t top = state->held_class[state->depth - 1u];
        if (class_id < top) {
            state->violation_count++;
            return MCS_SYNC_EDEADLK;
        }
    }
    state->held_class[state->depth] = class_id;
    state->held_name[state->depth] = name;
    state->depth++;
    return MCS_SYNC_OK;
}

int mcs_lockdep_after_release(mcs_lockdep_state_t *state, uint32_t class_id, const char *name) {
    (void)name;
    if (state == 0 || class_id == 0) {
        return MCS_SYNC_EINVAL;
    }
    if (state->depth == 0) {
        state->violation_count++;
        return MCS_SYNC_EPERM;
    }
    uint32_t index = state->depth - 1u;
    if (state->held_class[index] != class_id) {
        state->violation_count++;
        return MCS_SYNC_EDEADLK;
    }
    state->held_class[index] = 0;
    state->held_name[index] = 0;
    state->depth--;
    return MCS_SYNC_OK;
}
EOF
```

### 10.4 Tambahkan spinlock `kernel/sync/spinlock.c`

Spinlock menggunakan `__atomic_exchange_n` dengan acquire pada lock dan `__atomic_store_n` dengan release pada unlock. Pada x86_64, loop gagal memakai instruksi `pause` agar busy-wait tidak terlalu agresif terhadap pipeline. Jangan menambahkan operasi blocking di dalam critical section spinlock.

```bash
cat > kernel/sync/spinlock.c <<'EOF'
#include "mcs_sync.h"

static inline void mcs_cpu_relax(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

void mcs_spin_init(mcs_spinlock_t *lock, uint32_t class_id, const char *name) {
    if (lock == 0) {
        return;
    }
    __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELAXED);
    lock->class_id = class_id;
    lock->name = name;
}

bool mcs_spin_try_lock(mcs_spinlock_t *lock) {
    if (lock == 0) {
        return false;
    }
    uint32_t old = __atomic_exchange_n(&lock->locked, 1u, __ATOMIC_ACQUIRE);
    return old == 0u;
}

void mcs_spin_lock(mcs_spinlock_t *lock) {
    while (!mcs_spin_try_lock(lock)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED) != 0u) {
            mcs_cpu_relax();
        }
    }
}

void mcs_spin_unlock(mcs_spinlock_t *lock) {
    if (lock == 0) {
        return;
    }
    __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELEASE);
}

bool mcs_spin_is_locked(const mcs_spinlock_t *lock) {
    if (lock == 0) {
        return false;
    }
    return __atomic_load_n(&lock->locked, __ATOMIC_RELAXED) != 0u;
}
EOF
```

### 10.5 Tambahkan mutex kooperatif awal `kernel/sync/mutex.c`

Mutex ini memvalidasi owner. Ia belum memanggil scheduler untuk sleep. Tujuannya adalah menyiapkan kontrak state yang benar sebelum wait queue dan wakeup ditambahkan.

```bash
cat > kernel/sync/mutex.c <<'EOF'
#include "mcs_sync.h"

void mcs_mutex_init(mcs_mutex_t *mutex, uint32_t class_id, const char *name) {
    if (mutex == 0) {
        return;
    }
    __atomic_store_n(&mutex->locked, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&mutex->owner, 0u, __ATOMIC_RELAXED);
    mutex->class_id = class_id;
    mutex->name = name;
}

int mcs_mutex_try_lock(mcs_mutex_t *mutex, uint64_t owner_id) {
    if (mutex == 0 || owner_id == 0u) {
        return MCS_SYNC_EINVAL;
    }
    uint32_t expected = 0u;
    if (!__atomic_compare_exchange_n(&mutex->locked, &expected, 1u, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        if (__atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) == owner_id) {
            return MCS_SYNC_EDEADLK;
        }
        return MCS_SYNC_EBUSY;
    }
    __atomic_store_n(&mutex->owner, owner_id, __ATOMIC_RELEASE);
    return MCS_SYNC_OK;
}

int mcs_mutex_unlock(mcs_mutex_t *mutex, uint64_t owner_id) {
    if (mutex == 0 || owner_id == 0u) {
        return MCS_SYNC_EINVAL;
    }
    uint64_t owner = __atomic_load_n(&mutex->owner, __ATOMIC_ACQUIRE);
    if (owner != owner_id) {
        return MCS_SYNC_EPERM;
    }
    __atomic_store_n(&mutex->owner, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&mutex->locked, 0u, __ATOMIC_RELEASE);
    return MCS_SYNC_OK;
}

bool mcs_mutex_is_locked(const mcs_mutex_t *mutex) {
    if (mutex == 0) {
        return false;
    }
    return __atomic_load_n(&mutex->locked, __ATOMIC_RELAXED) != 0u;
}

uint64_t mcs_mutex_owner(const mcs_mutex_t *mutex) {
    if (mutex == 0) {
        return 0u;
    }
    return __atomic_load_n(&mutex->owner, __ATOMIC_RELAXED);
}
EOF
```

### 10.6 Tambahkan host unit test `tests/m12_sync_host_test.c`

Unit test host menggunakan pthread untuk memverifikasi bahwa counter yang dilindungi spinlock memiliki hasil deterministik. Test juga memverifikasi negative cases lockdep dan mutex owner semantics.

```bash
cat > tests/m12_sync_host_test.c <<'EOF'
#include "mcs_sync.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4
#define ITERS 25000

static mcs_spinlock_t g_counter_lock;
static unsigned long g_counter;

static void require_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "[FAIL] %s\n", message);
        exit(1);
    }
}

static void *worker(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        mcs_spin_lock(&g_counter_lock);
        g_counter++;
        mcs_spin_unlock(&g_counter_lock);
    }
    return 0;
}

static void test_lockdep_order(void) {
    mcs_lockdep_state_t st;
    mcs_lockdep_init(&st);
    require_true(mcs_lockdep_before_acquire(&st, 10u, "pmm") == MCS_SYNC_OK, "acquire rank 10");
    require_true(mcs_lockdep_before_acquire(&st, 20u, "vmm") == MCS_SYNC_OK, "acquire rank 20");
    require_true(st.depth == 2u, "depth after two locks");
    require_true(mcs_lockdep_after_release(&st, 20u, "vmm") == MCS_SYNC_OK, "release rank 20");
    require_true(mcs_lockdep_after_release(&st, 10u, "pmm") == MCS_SYNC_OK, "release rank 10");
    require_true(st.depth == 0u, "depth zero after releases");
}

static void test_lockdep_negative(void) {
    mcs_lockdep_state_t st;
    mcs_lockdep_init(&st);
    require_true(mcs_lockdep_before_acquire(&st, 20u, "vmm") == MCS_SYNC_OK, "acquire rank 20 first");
    require_true(mcs_lockdep_before_acquire(&st, 10u, "pmm") == MCS_SYNC_EDEADLK, "reject descending rank");
    require_true(mcs_lockdep_before_acquire(&st, 20u, "vmm") == MCS_SYNC_EDEADLK, "reject recursion");
    require_true(st.violation_count == 2u, "two lockdep violations counted");
    require_true(mcs_lockdep_after_release(&st, 20u, "vmm") == MCS_SYNC_OK, "release rank 20 after negatives");
}

static void test_spinlock_threads(void) {
    pthread_t thread[THREADS];
    mcs_spin_init(&g_counter_lock, 100u, "counter");
    g_counter = 0;
    for (int i = 0; i < THREADS; i++) {
        require_true(pthread_create(&thread[i], 0, worker, 0) == 0, "pthread_create");
    }
    for (int i = 0; i < THREADS; i++) {
        require_true(pthread_join(thread[i], 0) == 0, "pthread_join");
    }
    require_true(g_counter == (unsigned long)THREADS * (unsigned long)ITERS, "spinlock-protected counter exact");
    require_true(!mcs_spin_is_locked(&g_counter_lock), "spinlock unlocked after test");
}

static void test_mutex_owner(void) {
    mcs_mutex_t mutex;
    mcs_mutex_init(&mutex, 200u, "proc_table");
    require_true(mcs_mutex_try_lock(&mutex, 1u) == MCS_SYNC_OK, "owner 1 lock");
    require_true(mcs_mutex_owner(&mutex) == 1u, "owner recorded");
    require_true(mcs_mutex_try_lock(&mutex, 1u) == MCS_SYNC_EDEADLK, "recursive mutex rejected");
    require_true(mcs_mutex_try_lock(&mutex, 2u) == MCS_SYNC_EBUSY, "other owner sees busy");
    require_true(mcs_mutex_unlock(&mutex, 2u) == MCS_SYNC_EPERM, "non-owner unlock rejected");
    require_true(mcs_mutex_unlock(&mutex, 1u) == MCS_SYNC_OK, "owner unlock");
    require_true(!mcs_mutex_is_locked(&mutex), "mutex unlocked");
}

int main(void) {
    test_lockdep_order();
    test_lockdep_negative();
    test_spinlock_threads();
    test_mutex_owner();
    puts("[PASS] M12 synchronization host tests passed");
    return 0;
}
EOF
```

### 10.7 Tambahkan `Makefile.m12`

Makefile ini menyediakan target `host-test`, `freestanding`, dan `audit`. Target freestanding menghasilkan object `x86_64-elf`; target audit mengumpulkan `nm`, `readelf`, `objdump`, dan checksum.

```bash
cat > Makefile.m12 <<'EOF'
CC ?= clang
HOSTCC ?= cc
NM ?= nm
READELF ?= readelf
OBJDUMP ?= objdump
CFLAGS_COMMON := -std=c17 -Wall -Wextra -Werror -Iinclude
KERNEL_CFLAGS := $(CFLAGS_COMMON) -target x86_64-elf -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -O2
HOST_CFLAGS := $(CFLAGS_COMMON) -O2 -pthread
SYNC_SRCS := kernel/sync/lockdep.c kernel/sync/spinlock.c kernel/sync/mutex.c
BUILD := build/m12

.PHONY: all clean host-test freestanding audit

all: host-test freestanding audit

$(BUILD):
	mkdir -p $(BUILD)

host-test: $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) $(SYNC_SRCS) tests/m12_sync_host_test.c -o $(BUILD)/m12_sync_host_test
	$(BUILD)/m12_sync_host_test | tee $(BUILD)/host-test.log

freestanding: $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/lockdep.c -o $(BUILD)/lockdep.o
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/spinlock.c -o $(BUILD)/spinlock.o
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/mutex.c -o $(BUILD)/mutex.o

audit: freestanding
	$(NM) -u $(BUILD)/lockdep.o $(BUILD)/spinlock.o $(BUILD)/mutex.o | tee $(BUILD)/nm-undefined.txt
	$(READELF) -h $(BUILD)/lockdep.o | tee $(BUILD)/readelf-lockdep.txt
	$(OBJDUMP) -d $(BUILD)/spinlock.o | tee $(BUILD)/objdump-spinlock.txt
	sha256sum $(BUILD)/lockdep.o $(BUILD)/spinlock.o $(BUILD)/mutex.o $(BUILD)/m12_sync_host_test > $(BUILD)/sha256sums.txt
	@! grep -q ' U ' $(BUILD)/nm-undefined.txt

clean:
	rm -rf build
EOF
```

---

## 11. Build dan Unit Test

Jalankan build M12 dari root repository.

```bash
make -f Makefile.m12 clean
make -f Makefile.m12 all CC=clang | tee evidence/M12/m12-build.log
```

Indikator hasil yang diharapkan:

```text
[PASS] M12 synchronization host tests passed
```

Jika compiler host default adalah `cc` dan tidak mendukung konfigurasi yang diperlukan, gunakan:

```bash
make -f Makefile.m12 all CC=clang HOSTCC=clang
```

---

## 12. Audit Object Freestanding

Audit object memastikan source dapat dikompilasi ke format kernel target dan tidak diam-diam bergantung pada runtime hosted.

```bash
nm -u build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o | tee evidence/M12/nm-undefined.txt
readelf -h build/m12/lockdep.o | tee evidence/M12/readelf-lockdep.txt
objdump -d build/m12/spinlock.o | tee evidence/M12/objdump-spinlock.txt
sha256sum build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o build/m12/m12_sync_host_test \
  | tee evidence/M12/sha256sums.txt
```

Kriteria audit minimum:

1. `readelf` menunjukkan `Class: ELF64` dan `Machine: Advanced Micro Devices X86-64`.
2. `nm -u` tidak menunjukkan unresolved external symbol untuk object sinkronisasi.
3. `objdump` menunjukkan `xchg` atau instruksi setara pada jalur `mcs_spin_try_lock`.
4. `objdump` menunjukkan `pause` pada loop spin x86_64.
5. Checksum tersimpan di `evidence/M12/sha256sums.txt`.

---

## 13. Integrasi Minimal ke Kernel MCSOS

Integrasi pertama tidak boleh langsung mengunci seluruh scheduler. Tambahkan self-test kernel kecil yang dipanggil setelah early console dan sebelum scheduler memulai thread normal. Contoh integrasi:

```c
#include "mcs_sync.h"

static mcs_spinlock_t boot_stats_lock;
static mcs_lockdep_state_t boot_lockdep;
static uint64_t boot_counter;

void m12_sync_selftest(void) {
    mcs_lockdep_init(&boot_lockdep);
    mcs_spin_init(&boot_stats_lock, 10u, "boot_stats");

    if (mcs_lockdep_before_acquire(&boot_lockdep, 10u, "boot_stats") != MCS_SYNC_OK) {
        kernel_panic("M12 lockdep acquire failed");
    }

    mcs_spin_lock(&boot_stats_lock);
    boot_counter++;
    mcs_spin_unlock(&boot_stats_lock);

    if (mcs_lockdep_after_release(&boot_lockdep, 10u, "boot_stats") != MCS_SYNC_OK) {
        kernel_panic("M12 lockdep release failed");
    }

    klog_info("M12 sync selftest passed");
}
```

Panggil `m12_sync_selftest()` dari `kernel_main()` setelah logging M3 siap. Jangan memanggil self-test ini sebelum console/panic path aktif, karena kegagalan sinkronisasi tanpa panic path akan terlihat sebagai boot hang.

---

## 14. Workflow QEMU Smoke Test

Perintah berikut harus disesuaikan dengan Makefile utama repository MCSOS masing-masing. Tujuannya adalah menangkap serial log, bukan sekadar melihat jendela QEMU.

```bash
mkdir -p evidence/M12/qemu
make clean
make all 2>&1 | tee evidence/M12/qemu/kernel-build.log
make run-headless 2>&1 | tee evidence/M12/qemu/qemu-run.log
```

Jika repository belum memiliki `run-headless`, gunakan pola umum berikut dan sesuaikan path ISO/kernel.

```bash
qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -serial file:evidence/M12/qemu/serial.log \
  -no-reboot \
  -no-shutdown \
  -d int,guest_errors \
  -D evidence/M12/qemu/qemu-debug.log \
  -cdrom build/mcsos.iso
```

Kriteria smoke test minimum adalah serial log memuat banner MCSOS, hasil self-test M12, dan tidak ada panic tidak terduga. Jika QEMU berhenti tanpa log, gunakan workflow GDB.

---

## 15. Workflow GDB untuk Deadlock atau Boot Hang

GDB dipakai saat QEMU hang, spin loop tidak keluar, atau trap terjadi saat self-test M12.

Terminal 1:

```bash
qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -serial stdio \
  -s -S \
  -no-reboot \
  -no-shutdown \
  -cdrom build/mcsos.iso
```

Terminal 2:

```bash
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) break m12_sync_selftest
(gdb) break mcs_spin_lock
(gdb) break mcs_lockdep_before_acquire
(gdb) continue
(gdb) info registers
(gdb) bt
(gdb) disassemble /m mcs_spin_lock
```

Jika breakpoint `mcs_spin_lock` sering terkena tetapi tidak pernah keluar, inspeksi nilai lock:

```gdb
(gdb) p/x boot_stats_lock
(gdb) x/8gx &boot_stats_lock
```

---

## 16. Failure Modes dan Solusi Perbaikan

| Gejala | Kemungkinan sebab | Perintah diagnosis | Solusi konservatif |
|---|---|---|---|
| Host test counter tidak sesuai | Spinlock tidak benar-benar atomic atau unlock terlalu awal | Jalankan `tests/m12_sync_host_test` beberapa kali | Pastikan `__atomic_exchange_n` acquire dan `__atomic_store_n` release digunakan |
| Freestanding object punya unresolved symbol `__atomic_*` | Operasi atomik tidak lock-free untuk ukuran tipe yang dipilih | `nm -u build/m12/*.o` | Gunakan tipe 32-bit/64-bit natural dan hindari generic atomic ukuran arbitrary |
| Boot hang setelah self-test | Spinlock diambil dua kali atau tidak di-unlock | GDB breakpoint `mcs_spin_lock` dan `mcs_spin_unlock` | Tambahkan lockdep sebelum spinlock; audit semua return path |
| Lockdep menolak urutan lock | Ranking kelas salah atau urutan akuisisi tidak konsisten | Log `class_id` dan `held_class[]` | Tetapkan global lock hierarchy dan dokumentasikan di ADR |
| Mutex tidak bisa dibuka | Unlock dilakukan oleh bukan owner | Cek `mcs_mutex_owner()` | Pastikan owner ID berasal dari thread/TCB yang sama |
| QEMU triple fault setelah integrasi | Self-test dipanggil sebelum console/panic/stack valid | `qemu -d int,guest_errors` | Pindahkan panggilan setelah init awal M3/M4 selesai |
| Deadlock saat interrupt | Spinlock task context juga diambil interrupt handler | Log jalur interrupt dan state IF | Tambahkan varian irqsave/restore pada modul lanjutan; hindari lock sharing task/IRQ tanpa kebijakan |
| Starvation | Critical section terlalu panjang | Tambahkan timestamp/counter | Pecah critical section; pindahkan operasi lambat ke luar lock |

---

## 17. Lock Hierarchy Awal MCSOS

Gunakan ranking kelas lock berikut sebagai baseline konservatif. Ranking dapat berubah, tetapi perubahan wajib dicatat dalam ADR.

| Class ID | Lock class | Contoh lock | Aturan |
|---:|---|---|---|
| 10 | Boot/early stats | `boot_stats_lock` | Hanya self-test/early log |
| 20 | PMM | `pmm_lock` | Tidak boleh mengambil lock lebih rendah |
| 30 | VMM/page table | `vmm_lock` | Boleh mengambil PMM hanya jika desain eksplisit mengizinkan; default hindari nested PMM↔VMM |
| 40 | Heap | `heap_lock` | Critical section pendek |
| 50 | Thread/TCB | `task_lock` | Jangan pegang saat operasi I/O |
| 60 | Runqueue | `runqueue_lock` | Di jalur scheduler, critical section sangat pendek |
| 70 | Syscall table/policy | `syscall_lock` | Hindari mengambil runqueue lock dari syscall lock |
| 80 | Loader/process image | `loader_lock` | Tidak boleh memegang lock saat parsing image besar |
| 90 | VFS awal | `vfs_lock` | Untuk modul berikutnya |
| 100 | Device model | `device_lock` | Untuk driver/interrupt lanjutan |

Aturan konservatif M12 adalah lock boleh diambil dari class lebih rendah ke class lebih tinggi. Jika desain membutuhkan kebalikan, buat ADR dan tambahkan test negative/positive khusus.

---

## 18. Checkpoint Buildable

| Checkpoint | Artefak | Perintah | Bukti |
|---|---|---|---|
| C1 | Header sync | `test -f include/mcs_sync.h` | File header tersedia |
| C2 | Lockdep build | `make -f Makefile.m12 freestanding CC=clang` | `lockdep.o` dibuat |
| C3 | Host unit test | `make -f Makefile.m12 host-test` | `[PASS]` pada log |
| C4 | Object audit | `make -f Makefile.m12 audit CC=clang` | `nm`, `readelf`, `objdump`, checksum |
| C5 | Kernel integration | `make all` | Kernel link berhasil |
| C6 | QEMU smoke | `make run-headless` | Serial log memuat `M12 sync selftest passed` |
| C7 | Commit | `git commit` | Commit hash tercatat |

---

## 19. Pertanyaan Analisis

1. Mengapa `volatile` tidak cukup untuk sinkronisasi antar-core atau antar-thread?
2. Apa perbedaan acquire pada lock dan release pada unlock?
3. Mengapa spinlock tidak boleh melindungi operasi yang dapat tidur?
4. Mengapa mutex owner-aware dapat mendeteksi bug unlock oleh thread lain?
5. Mengapa lock release sebaiknya mengikuti urutan LIFO dalam validator sederhana?
6. Berikan contoh dua jalur kode yang dapat deadlock karena lock-order inversion.
7. Apa risiko jika interrupt handler mengambil lock yang sama dengan task context tanpa `irqsave`?
8. Apa batasan host pthread test dibanding QEMU/kernel test?
9. Mengapa `nm -u` penting dalam kode freestanding?
10. Apa perluasan yang diperlukan agar M12 siap untuk SMP sungguhan?

---

## 20. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | Spinlock, mutex, lockdep, host test, dan freestanding compile berjalan sesuai kontrak |
| Kualitas desain dan invariants | 20 | Invariant lock, owner, memory ordering, dan lock hierarchy dijelaskan benar |
| Pengujian dan bukti | 20 | Log host test, QEMU, `nm`, `readelf`, `objdump`, checksum, dan commit hash lengkap |
| Debugging/failure analysis | 10 | Failure modes, diagnosis, dan solusi kendala realistis |
| Keamanan dan robustness | 10 | Risiko race, deadlock, interrupt reentry, privilege effect, dan mitigasi dijelaskan |
| Dokumentasi/laporan | 10 | Laporan mengikuti template, jelas, dapat direproduksi, dan menggunakan referensi IEEE |

---

## 21. Kriteria Lulus Praktikum

Mahasiswa dinyatakan lulus M12 jika memenuhi seluruh kriteria minimum berikut:

1. Repository dapat dibangun dari clean checkout.
2. `Makefile.m12` tersedia dan target `all` berhasil pada lingkungan mahasiswa.
3. Host unit test M12 lulus.
4. Object freestanding x86_64 berhasil dibuat.
5. Audit `nm -u`, `readelf`, `objdump`, dan checksum tersedia.
6. Integrasi kernel tidak merusak boot path M2–M11.
7. Serial log QEMU disimpan.
8. Panic path tetap terbaca jika self-test digagalkan secara sengaja.
9. Tidak ada warning kritis pada build M12.
10. Perubahan Git dikomit.
11. Laporan menjelaskan desain, invariant, failure mode, dan readiness review.

---

## 22. Prosedur Rollback

Jika M12 menyebabkan boot hang atau test sebelumnya gagal, jalankan prosedur berikut.

```bash
git status --short
git diff --stat
git restore include/mcs_sync.h kernel/sync/lockdep.c kernel/sync/spinlock.c kernel/sync/mutex.c tests/m12_sync_host_test.c Makefile.m12
git status --short
```

Jika perubahan sudah dikomit pada branch M12, gunakan revert agar histori tetap terlacak.

```bash
git log --oneline -5
git revert <commit_m12>
```

Jangan menghapus log evidence. Simpan evidence kegagalan sebagai bahan analisis laporan.

---

## 23. Bukti Pemeriksaan Source Code Panduan Ini

Source code inti M12 pada panduan ini telah diperiksa di lingkungan eksekusi lokal dengan target host test dan object freestanding x86_64. Hasil ini merupakan bukti lokal untuk source panduan, bukan jaminan bahwa integrasi repository mahasiswa pasti identik.

### 23.1 Host unit test

```text
[PASS] M12 synchronization host tests passed
```

### 23.2 Checksum artefak lokal

```text
d266000e9a7a78feae925a8f9bdebcf7c59d649311e347d46716596af6184383  build/m12/lockdep.o
df3ca8b08259a0e23faa6c96f2011ef4018573b95499270ff5ed18f5ce334803  build/m12/spinlock.o
2ec39ec7ad71b80f4864d190faba01a17e167706c04f26b481ecd2088603667e  build/m12/mutex.o
ffda41169caf1b13574528f00f07aafc6d399726a14be6e4fea742371b3455d7  build/m12/m12_sync_host_test
```

### 23.3 Ringkasan `readelf`

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
  Start of section headers:          920 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           64 (bytes)
  Number of section headers:         7
  Section header string table index: 1
```

### 23.4 Ringkasan `objdump`

```text
0000000000000020 <mcs_spin_try_lock>:
  27:	74 0e                	je     37 <mcs_spin_try_lock+0x17>
  2e:	87 07                	xchg   %eax,(%rdi)
  5a:	87 07                	xchg   %eax,(%rdi)
  70:	f3 90                	pause
0000000000000080 <mcs_spin_unlock>:
  87:	74 06                	je     8f <mcs_spin_unlock+0xf>
```

---

## 24. Template Laporan Praktikum M12

Gunakan template laporan umum `os_template_laporan_praktikum.md`, lalu isi bagian spesifik M12 berikut.

1. Sampul: judul praktikum, nama mahasiswa, NIM, kelas, dosen Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.
2. Tujuan: jelaskan target spinlock, mutex, lockdep, host test, dan object audit.
3. Dasar teori ringkas: atomics, memory ordering, spinlock, mutex, deadlock, lock order.
4. Lingkungan: Windows 11, WSL 2, distro, compiler, QEMU, GDB, commit hash.
5. Desain: struktur `mcs_spinlock_t`, `mcs_mutex_t`, `mcs_lockdep_state_t`, invariant, dan lock hierarchy.
6. Langkah kerja: semua perintah build/test/audit dan alasan teknisnya.
7. Hasil uji: host test, QEMU log, `nm`, `readelf`, `objdump`, checksum.
8. Analisis: mengapa test lulus/gagal, bug yang ditemukan, dan perbaikan.
9. Keamanan dan reliability: risiko race, deadlock, priority inversion, interrupt reentry, privilege impact.
10. Kesimpulan: status readiness dan pekerjaan berikutnya.
11. Lampiran: diff ringkas, source penting, log penuh, dan referensi.

---

## 25. Readiness Review M12

| Aspek | Status minimum setelah M12 | Bukti yang wajib ada |
|---|---|---|
| Build | Siap uji host dan object freestanding | `m12-build.log`, object `.o` |
| Runtime emulator | Siap uji QEMU smoke | serial log QEMU |
| Correctness | Siap evaluasi invariant dasar | host unit test dan negative test |
| Security | Belum siap klaim aman | threat/failure analysis |
| SMP | Belum siap SMP penuh | batasan tertulis |
| Observability | Awal | violation counter, logs, GDB workflow |
| Release | Bukan release candidate | hanya artefak praktikum |

**Kesimpulan readiness**: hasil M12 hanya boleh diberi label **siap uji QEMU untuk sinkronisasi kernel awal single-core menuju SMP** apabila semua checkpoint lulus dan evidence lengkap. Hasil M12 belum boleh disebut siap produksi, bebas deadlock, bebas race, atau enterprise-ready.

---

## 26. Lampiran Source Code Lengkap

### 26.1 `include/mcs_sync.h`

```c
#ifndef MCS_SYNC_H
#define MCS_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MCS_LOCKDEP_MAX_HELD 16u
#define MCS_LOCK_NAME_MAX 32u

#define MCS_SYNC_OK 0
#define MCS_SYNC_EINVAL (-22)
#define MCS_SYNC_EBUSY (-16)
#define MCS_SYNC_EPERM (-1)
#define MCS_SYNC_EDEADLK (-35)
#define MCS_SYNC_EOVERFLOW (-75)

typedef struct mcs_lockdep_state {
    uint32_t held_class[MCS_LOCKDEP_MAX_HELD];
    const char *held_name[MCS_LOCKDEP_MAX_HELD];
    uint32_t depth;
    uint32_t violation_count;
} mcs_lockdep_state_t;

typedef struct mcs_spinlock {
    volatile uint32_t locked;
    uint32_t class_id;
    const char *name;
} mcs_spinlock_t;

typedef struct mcs_mutex {
    volatile uint32_t locked;
    uint64_t owner;
    uint32_t class_id;
    const char *name;
} mcs_mutex_t;

void mcs_lockdep_init(mcs_lockdep_state_t *state);
int mcs_lockdep_before_acquire(mcs_lockdep_state_t *state, uint32_t class_id, const char *name);
int mcs_lockdep_after_release(mcs_lockdep_state_t *state, uint32_t class_id, const char *name);
bool mcs_lockdep_is_held(const mcs_lockdep_state_t *state, uint32_t class_id);

void mcs_spin_init(mcs_spinlock_t *lock, uint32_t class_id, const char *name);
bool mcs_spin_try_lock(mcs_spinlock_t *lock);
void mcs_spin_lock(mcs_spinlock_t *lock);
void mcs_spin_unlock(mcs_spinlock_t *lock);
bool mcs_spin_is_locked(const mcs_spinlock_t *lock);

void mcs_mutex_init(mcs_mutex_t *mutex, uint32_t class_id, const char *name);
int mcs_mutex_try_lock(mcs_mutex_t *mutex, uint64_t owner_id);
int mcs_mutex_unlock(mcs_mutex_t *mutex, uint64_t owner_id);
bool mcs_mutex_is_locked(const mcs_mutex_t *mutex);
uint64_t mcs_mutex_owner(const mcs_mutex_t *mutex);

#endif
```

### 26.2 `kernel/sync/lockdep.c`

```c
#include "mcs_sync.h"

void mcs_lockdep_init(mcs_lockdep_state_t *state) {
    if (state == 0) {
        return;
    }
    for (uint32_t i = 0; i < MCS_LOCKDEP_MAX_HELD; i++) {
        state->held_class[i] = 0;
        state->held_name[i] = 0;
    }
    state->depth = 0;
    state->violation_count = 0;
}

bool mcs_lockdep_is_held(const mcs_lockdep_state_t *state, uint32_t class_id) {
    if (state == 0 || class_id == 0) {
        return false;
    }
    for (uint32_t i = 0; i < state->depth && i < MCS_LOCKDEP_MAX_HELD; i++) {
        if (state->held_class[i] == class_id) {
            return true;
        }
    }
    return false;
}

int mcs_lockdep_before_acquire(mcs_lockdep_state_t *state, uint32_t class_id, const char *name) {
    if (state == 0 || class_id == 0) {
        return MCS_SYNC_EINVAL;
    }
    if (state->depth >= MCS_LOCKDEP_MAX_HELD) {
        state->violation_count++;
        return MCS_SYNC_EOVERFLOW;
    }
    for (uint32_t i = 0; i < state->depth; i++) {
        if (state->held_class[i] == class_id) {
            state->violation_count++;
            return MCS_SYNC_EDEADLK;
        }
    }
    if (state->depth > 0) {
        uint32_t top = state->held_class[state->depth - 1u];
        if (class_id < top) {
            state->violation_count++;
            return MCS_SYNC_EDEADLK;
        }
    }
    state->held_class[state->depth] = class_id;
    state->held_name[state->depth] = name;
    state->depth++;
    return MCS_SYNC_OK;
}

int mcs_lockdep_after_release(mcs_lockdep_state_t *state, uint32_t class_id, const char *name) {
    (void)name;
    if (state == 0 || class_id == 0) {
        return MCS_SYNC_EINVAL;
    }
    if (state->depth == 0) {
        state->violation_count++;
        return MCS_SYNC_EPERM;
    }
    uint32_t index = state->depth - 1u;
    if (state->held_class[index] != class_id) {
        state->violation_count++;
        return MCS_SYNC_EDEADLK;
    }
    state->held_class[index] = 0;
    state->held_name[index] = 0;
    state->depth--;
    return MCS_SYNC_OK;
}
```

### 26.3 `kernel/sync/spinlock.c`

```c
#include "mcs_sync.h"

static inline void mcs_cpu_relax(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

void mcs_spin_init(mcs_spinlock_t *lock, uint32_t class_id, const char *name) {
    if (lock == 0) {
        return;
    }
    __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELAXED);
    lock->class_id = class_id;
    lock->name = name;
}

bool mcs_spin_try_lock(mcs_spinlock_t *lock) {
    if (lock == 0) {
        return false;
    }
    uint32_t old = __atomic_exchange_n(&lock->locked, 1u, __ATOMIC_ACQUIRE);
    return old == 0u;
}

void mcs_spin_lock(mcs_spinlock_t *lock) {
    while (!mcs_spin_try_lock(lock)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED) != 0u) {
            mcs_cpu_relax();
        }
    }
}

void mcs_spin_unlock(mcs_spinlock_t *lock) {
    if (lock == 0) {
        return;
    }
    __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELEASE);
}

bool mcs_spin_is_locked(const mcs_spinlock_t *lock) {
    if (lock == 0) {
        return false;
    }
    return __atomic_load_n(&lock->locked, __ATOMIC_RELAXED) != 0u;
}
```

### 26.4 `kernel/sync/mutex.c`

```c
#include "mcs_sync.h"

void mcs_mutex_init(mcs_mutex_t *mutex, uint32_t class_id, const char *name) {
    if (mutex == 0) {
        return;
    }
    __atomic_store_n(&mutex->locked, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&mutex->owner, 0u, __ATOMIC_RELAXED);
    mutex->class_id = class_id;
    mutex->name = name;
}

int mcs_mutex_try_lock(mcs_mutex_t *mutex, uint64_t owner_id) {
    if (mutex == 0 || owner_id == 0u) {
        return MCS_SYNC_EINVAL;
    }
    uint32_t expected = 0u;
    if (!__atomic_compare_exchange_n(&mutex->locked, &expected, 1u, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        if (__atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) == owner_id) {
            return MCS_SYNC_EDEADLK;
        }
        return MCS_SYNC_EBUSY;
    }
    __atomic_store_n(&mutex->owner, owner_id, __ATOMIC_RELEASE);
    return MCS_SYNC_OK;
}

int mcs_mutex_unlock(mcs_mutex_t *mutex, uint64_t owner_id) {
    if (mutex == 0 || owner_id == 0u) {
        return MCS_SYNC_EINVAL;
    }
    uint64_t owner = __atomic_load_n(&mutex->owner, __ATOMIC_ACQUIRE);
    if (owner != owner_id) {
        return MCS_SYNC_EPERM;
    }
    __atomic_store_n(&mutex->owner, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&mutex->locked, 0u, __ATOMIC_RELEASE);
    return MCS_SYNC_OK;
}

bool mcs_mutex_is_locked(const mcs_mutex_t *mutex) {
    if (mutex == 0) {
        return false;
    }
    return __atomic_load_n(&mutex->locked, __ATOMIC_RELAXED) != 0u;
}

uint64_t mcs_mutex_owner(const mcs_mutex_t *mutex) {
    if (mutex == 0) {
        return 0u;
    }
    return __atomic_load_n(&mutex->owner, __ATOMIC_RELAXED);
}
```

### 26.5 `tests/m12_sync_host_test.c`

```c
#include "mcs_sync.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4
#define ITERS 25000

static mcs_spinlock_t g_counter_lock;
static unsigned long g_counter;

static void require_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "[FAIL] %s\n", message);
        exit(1);
    }
}

static void *worker(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        mcs_spin_lock(&g_counter_lock);
        g_counter++;
        mcs_spin_unlock(&g_counter_lock);
    }
    return 0;
}

static void test_lockdep_order(void) {
    mcs_lockdep_state_t st;
    mcs_lockdep_init(&st);
    require_true(mcs_lockdep_before_acquire(&st, 10u, "pmm") == MCS_SYNC_OK, "acquire rank 10");
    require_true(mcs_lockdep_before_acquire(&st, 20u, "vmm") == MCS_SYNC_OK, "acquire rank 20");
    require_true(st.depth == 2u, "depth after two locks");
    require_true(mcs_lockdep_after_release(&st, 20u, "vmm") == MCS_SYNC_OK, "release rank 20");
    require_true(mcs_lockdep_after_release(&st, 10u, "pmm") == MCS_SYNC_OK, "release rank 10");
    require_true(st.depth == 0u, "depth zero after releases");
}

static void test_lockdep_negative(void) {
    mcs_lockdep_state_t st;
    mcs_lockdep_init(&st);
    require_true(mcs_lockdep_before_acquire(&st, 20u, "vmm") == MCS_SYNC_OK, "acquire rank 20 first");
    require_true(mcs_lockdep_before_acquire(&st, 10u, "pmm") == MCS_SYNC_EDEADLK, "reject descending rank");
    require_true(mcs_lockdep_before_acquire(&st, 20u, "vmm") == MCS_SYNC_EDEADLK, "reject recursion");
    require_true(st.violation_count == 2u, "two lockdep violations counted");
    require_true(mcs_lockdep_after_release(&st, 20u, "vmm") == MCS_SYNC_OK, "release rank 20 after negatives");
}

static void test_spinlock_threads(void) {
    pthread_t thread[THREADS];
    mcs_spin_init(&g_counter_lock, 100u, "counter");
    g_counter = 0;
    for (int i = 0; i < THREADS; i++) {
        require_true(pthread_create(&thread[i], 0, worker, 0) == 0, "pthread_create");
    }
    for (int i = 0; i < THREADS; i++) {
        require_true(pthread_join(thread[i], 0) == 0, "pthread_join");
    }
    require_true(g_counter == (unsigned long)THREADS * (unsigned long)ITERS, "spinlock-protected counter exact");
    require_true(!mcs_spin_is_locked(&g_counter_lock), "spinlock unlocked after test");
}

static void test_mutex_owner(void) {
    mcs_mutex_t mutex;
    mcs_mutex_init(&mutex, 200u, "proc_table");
    require_true(mcs_mutex_try_lock(&mutex, 1u) == MCS_SYNC_OK, "owner 1 lock");
    require_true(mcs_mutex_owner(&mutex) == 1u, "owner recorded");
    require_true(mcs_mutex_try_lock(&mutex, 1u) == MCS_SYNC_EDEADLK, "recursive mutex rejected");
    require_true(mcs_mutex_try_lock(&mutex, 2u) == MCS_SYNC_EBUSY, "other owner sees busy");
    require_true(mcs_mutex_unlock(&mutex, 2u) == MCS_SYNC_EPERM, "non-owner unlock rejected");
    require_true(mcs_mutex_unlock(&mutex, 1u) == MCS_SYNC_OK, "owner unlock");
    require_true(!mcs_mutex_is_locked(&mutex), "mutex unlocked");
}

int main(void) {
    test_lockdep_order();
    test_lockdep_negative();
    test_spinlock_threads();
    test_mutex_owner();
    puts("[PASS] M12 synchronization host tests passed");
    return 0;
}
```

### 26.6 `Makefile.m12`

```makefile
CC ?= clang
HOSTCC ?= cc
NM ?= nm
READELF ?= readelf
OBJDUMP ?= objdump
CFLAGS_COMMON := -std=c17 -Wall -Wextra -Werror -Iinclude
KERNEL_CFLAGS := $(CFLAGS_COMMON) -target x86_64-elf -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -O2
HOST_CFLAGS := $(CFLAGS_COMMON) -O2 -pthread
SYNC_SRCS := kernel/sync/lockdep.c kernel/sync/spinlock.c kernel/sync/mutex.c
BUILD := build/m12

.PHONY: all clean host-test freestanding audit

all: host-test freestanding audit

$(BUILD):
	mkdir -p $(BUILD)

host-test: $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) $(SYNC_SRCS) tests/m12_sync_host_test.c -o $(BUILD)/m12_sync_host_test
	$(BUILD)/m12_sync_host_test | tee $(BUILD)/host-test.log

freestanding: $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/lockdep.c -o $(BUILD)/lockdep.o
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/spinlock.c -o $(BUILD)/spinlock.o
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/mutex.c -o $(BUILD)/mutex.o

audit: freestanding
	$(NM) -u $(BUILD)/lockdep.o $(BUILD)/spinlock.o $(BUILD)/mutex.o | tee $(BUILD)/nm-undefined.txt
	$(READELF) -h $(BUILD)/lockdep.o | tee $(BUILD)/readelf-lockdep.txt
	$(OBJDUMP) -d $(BUILD)/spinlock.o | tee $(BUILD)/objdump-spinlock.txt
	sha256sum $(BUILD)/lockdep.o $(BUILD)/spinlock.o $(BUILD)/mutex.o $(BUILD)/m12_sync_host_test > $(BUILD)/sha256sums.txt
	@! grep -q ' U ' $(BUILD)/nm-undefined.txt

clean:
	rm -rf build
```

---

## 27. References

[1] Intel Corporation, “Intel® 64 and IA-32 Architectures Software Developer Manuals,” Intel Developer Zone, 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html. Accessed: May 3, 2026.

[2] The Linux Kernel Documentation, “Lock types and their rules,” kernel.org, 2026. [Online]. Available: https://www.kernel.org/doc/html/latest/locking/locktypes.html. Accessed: May 3, 2026.

[3] The Linux Kernel Documentation, “Runtime locking correctness validator,” kernel.org, 2026. [Online]. Available: https://www.kernel.org/doc/html/latest/locking/lockdep-design.html. Accessed: May 3, 2026.

[4] The Linux Kernel Documentation, “Generic Mutex Subsystem,” kernel.org, 2026. [Online]. Available: https://docs.kernel.org/locking/mutex-design.html. Accessed: May 3, 2026.

[5] Free Software Foundation, “Built-in Functions for Memory Model Aware Atomic Operations,” GCC Online Documentation, 2026. [Online]. Available: https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html. Accessed: May 3, 2026.

[6] LLVM Project, “Clang command line argument reference,” Clang Documentation, 2026. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html. Accessed: May 3, 2026.

[7] QEMU Project, “GDB usage,” QEMU Documentation, 2026. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html. Accessed: May 3, 2026.

[8] GNU Binutils, “GNU Binary Utilities,” Sourceware, 2025. [Online]. Available: https://www.sourceware.org/binutils/docs/binutils.html. Accessed: May 3, 2026.
