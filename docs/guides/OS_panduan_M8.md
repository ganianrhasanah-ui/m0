# OS_panduan_M8.md

# Panduan Praktikum M8 — Kernel Heap Awal, Allocator Dinamis, Validasi Invariant, dan Integrasi Bertahap dengan PMM/VMM pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M8  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: *siap uji QEMU untuk kernel heap awal dan allocator dinamis tahap awal*, bukan siap produksi dan bukan bukti bahwa manajemen memori kernel lengkap aman untuk hardware umum.

---

## 1. Ringkasan Praktikum

Praktikum M8 melanjutkan M6 dan M7. Pada M6 mahasiswa telah membuat **Physical Memory Manager** berbasis frame 4 KiB. Pada M7 mahasiswa telah membuat **Virtual Memory Manager** awal berbasis page table x86_64, termasuk fungsi `map`, `query`, `unmap`, primitive `CR3/CR2/invlpg`, dan jalur diagnosis page fault. M8 menambahkan lapisan di atas keduanya, yaitu **kernel heap awal** atau **allocator dinamis kernel**.

Kernel heap diperlukan karena setelah boot awal, kernel tidak cukup hanya menggunakan variabel statik. Struktur seperti daftar proses, descriptor file, object VFS, buffer I/O, node timer, packet buffer, dan metadata driver membutuhkan alokasi dinamis dengan kontrak kepemilikan yang jelas. Linux membedakan beberapa strategi alokasi, seperti alokasi objek kecil dengan keluarga `kmalloc`, alokasi virtual kontigu dengan `vmalloc`, alokasi halaman langsung melalui page allocator, serta slab cache untuk banyak objek sejenis [3]. MCSOS M8 tidak meniru Linux secara penuh. M8 membuat allocator pendidikan yang lebih kecil, deterministik, dan mudah diaudit: **first-fit free-list allocator** dengan split, coalesce, alignment 16 byte, validasi header, host unit test, dan audit freestanding object.

Target M8 bersifat konservatif. Tugas wajib M8 **tidak mewajibkan heap tumbuh otomatis melalui mapping halaman baru**. Tugas wajibnya adalah membuat allocator yang benar pada arena heap yang sudah tersedia dan terpetakan. Arena tersebut dapat berasal dari array statik `.bss` saat bootstrap atau dari rentang virtual yang sudah dimap oleh VMM. Integrasi page-backed heap growth menjadi pengayaan setelah M7 benar-benar lulus dan page fault path stabil. Strategi ini menjaga agar bug allocator tidak langsung berubah menjadi triple fault atau korupsi page table.

Keberhasilan M8 tidak boleh dinyatakan sebagai “tanpa error” atau “siap produksi”. Kriteria minimum M8 adalah source code allocator dapat dikompilasi sebagai C17 freestanding, host unit test lulus, `nm -u` pada object freestanding kosong, audit `readelf` menunjukkan object ELF64 x86_64, dan integrasi kernel menghasilkan log serial yang menyatakan heap initialized beserta statistik awal heap. Validasi runtime QEMU tetap harus dilakukan ulang di lingkungan WSL 2 mahasiswa karena konfigurasi QEMU, OVMF, bootloader, filesystem host, dan versi toolchain dapat berbeda.

---

## 2. Asumsi Target dan Batasan

| Aspek | Keputusan M8 |
|---|---|
| Arsitektur | x86_64 long mode |
| Lingkungan host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Boot path | Melanjutkan pipeline M2–M7; direkomendasikan Limine/UEFI atau ISO yang sudah lulus M2 |
| Bahasa | C17 freestanding + assembly/inline assembly terbatas dari tahap sebelumnya |
| Toolchain | Clang/LLD atau GCC/binutils; contoh validasi memakai Clang dan GNU binutils tools |
| Dasar memori | PMM M6, VMM M7, dan arena heap yang sudah terpetakan |
| Unit wajib | Kernel heap awal berbasis free-list first-fit |
| Alignment wajib | 16 byte untuk payload `kmem_alloc` |
| Page-backed growth | Pengayaan, bukan syarat wajib |
| Slab/cache allocator | Dijelaskan sebagai konsep lanjutan, bukan implementasi wajib |
| Concurrency | Single-core early kernel; belum aman SMP tanpa lock |
| IRQ context | `kmem_alloc` tugas wajib tidak boleh dipakai dari interrupt handler |
| Out of scope | Per-CPU allocator, NUMA, slab penuh, `vmalloc`, `mmap`, user heap, copy-on-write, swapping, ASLR/KASLR penuh, garbage collection, dan allocator real-time deterministik penuh |

### 2A. Goals dan Non-Goals

**Goals** M8 adalah membangun allocator dinamis kernel awal yang dapat diuji secara deterministik, memiliki invariant eksplisit, dapat dikompilasi sebagai freestanding object, dan dapat diintegrasikan ke kernel setelah PMM/VMM siap. **Non-goals** M8 adalah allocator SMP-safe penuh, slab allocator produksi, user-space heap, `vmalloc`, page cache, DMA allocator, dan klaim keamanan produksi.

### 2B. Assumptions / Asumsi Implementasi

Assumptions M8 adalah: target x86_64 long mode, ABI internal kernel mengikuti konvensi C freestanding yang dikontrol toolchain, heap arena sudah berada pada memori virtual yang valid, semua page yang menampung arena sudah present dan writable, dan tidak ada pemanggilan allocator dari interrupt context. Jika salah satu asumsi ini tidak terpenuhi, hasil yang benar adalah gagal terdeteksi melalui log atau panic path, bukan melanjutkan eksekusi dengan metadata heap yang korup.

### 2C. Scope dan Target Matrix

| Scope | Target wajib | Target pengayaan | Non-scope |
|---|---|---|---|
| Allocator | First-fit free-list pada arena tetap | Page-backed growth melalui PMM/VMM | Slab penuh dan per-CPU cache |
| Target matrix | Host unit test + freestanding object x86_64 | QEMU smoke test dengan serial log | Hardware fisik umum |
| ABI | C17 freestanding | Integrasi formatter log kernel | ABI userspace |
| Runtime | Single-core early kernel | Preemption-disabled critical section | SMP-safe allocator |

### 2D. Toolchain BOM, Reproducibility, CI, dan Supply Chain

Toolchain bill of materials minimum adalah Clang atau GCC, LLD atau GNU ld, GNU Make, GNU binutils (`nm`, `readelf`, `objdump`), QEMU, GDB, dan Git. Reproducibility minimum dibuktikan dengan clean rebuild `make m8-clean && make m8-all`, log versi tool, dan commit hash. Untuk CI, target minimum yang dapat dibuat adalah job `m8-kmem-host-test` dan `m8-audit`. Untuk supply chain, artefak praktikum yang dikumpulkan sebaiknya disertai checksum, misalnya `sha256sum OS_panduan_M8.md build/m8/test_kmem.log build/m8/kmem.freestanding.o`, serta catatan provenance: versi toolchain, commit, dan waktu build. Signature/SBOM belum menjadi syarat wajib M8, tetapi istilah tersebut dicatat sebagai kontrol rilis lanjutan.

Intel SDM versi terbaru mendokumentasikan dukungan sistem pemrograman x86_64, termasuk memory management, protection, interrupt/exception handling, multiprocessor support, dan debugging [1]. AMD64 Architecture Programmer's Manual Volume 2 juga mendokumentasikan system programming, memory management, page translation, interrupts, dan privileged resources pada AMD64 [2]. Untuk M8, kedua manual tersebut menjadi sumber arsitektur, sedangkan desain allocator menggunakan prinsip umum kernel allocator yang dijelaskan dalam dokumentasi Linux memory allocation [3], [4], tetapi disederhanakan agar sesuai untuk praktikum pendidikan.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M8, mahasiswa mampu:

1. Menjelaskan perbedaan PMM, VMM, dan kernel heap.
2. Menjelaskan alasan kernel memerlukan allocator dinamis setelah boot awal.
3. Mendesain free-list allocator dengan metadata header, split, coalesce, dan statistik heap.
4. Menetapkan invariant allocator: alignment, batas arena, status free/used, double-free rejection, block linkage, dan total region coverage.
5. Mengimplementasikan `kmem_init`, `kmem_alloc`, `kmem_calloc`, `kmem_free_checked`, `kmem_get_stats`, dan `kmem_validate` dalam C17 freestanding.
6. Menyusun host unit test yang menguji alokasi, pembebasan, alignment, zeroing, overflow, fragmentasi, dan coalescing.
7. Melakukan audit freestanding object dengan `nm`, `readelf`, dan `objdump`.
8. Mengintegrasikan heap awal ke kernel MCSOS setelah PMM dan VMM initialized.
9. Membedakan allocator yang aman untuk early kernel dari allocator yang aman untuk interrupt, SMP, driver DMA, atau userspace.
10. Menyusun laporan praktikum dengan bukti build, test, log serial, audit object, failure analysis, rollback, dan readiness review.

---

## 4. Prasyarat Teori

Mahasiswa harus memahami materi berikut sebelum mengerjakan M8.

| Prasyarat | Uraian minimum |
|---|---|
| Pointer arithmetic C | Menghitung offset byte dengan `unsigned char *` dan menghindari aritmetika pointer yang tidak valid |
| Alignment | Makna alignment 16 byte, alignment 4 KiB, dan konsekuensi akses tidak aligned |
| Freestanding C | Lingkungan tanpa libc penuh; tidak boleh mengandalkan `malloc`, `free`, `printf`, atau `memset` dari libc di kernel |
| PMM | Frame fisik 4 KiB, bitmap allocator, usable memory, reserved memory, dan ownership frame |
| VMM | Page table x86_64 4-level, mapping virtual ke physical, page fault, CR2, CR3, dan HHDM |
| Invariant | Pernyataan kondisi yang harus selalu benar sebelum dan sesudah operasi allocator |
| Failure mode | Double free, use-after-free, overflow size, fragmentation, metadata corruption, out-of-memory, dan heap overrun |
| Debugging | `nm -u`, `readelf -h`, `objdump -dr`, QEMU serial log, dan GDB gdbstub |

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M8 |
|---|---|
| `@osdev-general` | Menetapkan milestone, readiness gate, acceptance criteria, dan batas klaim kesiapan |
| `@osdev-01-computer-foundation` | Merumuskan state machine allocator, invariant, kompleksitas first-fit, dan proof obligation |
| `@osdev-02-low-level-programming` | Mengontrol C freestanding, pointer arithmetic, alignment, undefined behavior, dan audit object |
| `@osdev-03-computer-and-hardware-architecture` | Menjaga konsistensi dengan x86_64 long mode, page alignment, dan page fault diagnostics |
| `@osdev-04-kernel-development` | Menghubungkan allocator dengan kernel init, panic path, logging, dan batas penggunaan IRQ/SMP |
| `@osdev-05-filesystem-development` | Menyiapkan pola alokasi object yang kelak dipakai VFS, inode, dentry, dan buffer cache |
| `@osdev-06-networking-stack` | Menyiapkan dasar allocator untuk packet buffer dan socket object pada modul lanjutan |
| `@osdev-07-os-security` | Membahas double-free, use-after-free, overflow, metadata corruption, dan hardening awal |
| `@osdev-08-device-driver-development` | Mengingatkan bahwa allocator umum belum cocok untuk DMA buffer tanpa constraint fisik dan cacheability |
| `@osdev-09-virtualization-and-containerization` | Menghubungkan allocator dengan boundary VM/container pada tahap lanjut, bukan cakupan wajib M8 |
| `@osdev-10-boot-firmware` | Mengaitkan arena heap dengan memory map dan HHDM bootloader |
| `@osdev-11-graphics-display` | Menjaga agar framebuffer tidak dialokasikan sebagai heap umum |
| `@osdev-12-toolchain-devenv` | Menyusun Makefile, compile freestanding, host test, reproducibility, dan audit `nm/readelf/objdump` |
| `@osdev-13-enterprise-features` | Menyiapkan jejak observability dan statistik heap untuk reliability lanjutan |
| `@osdev-14-cross-science` | Menetapkan verification matrix, risk register, failure modes, dan readiness review |

---

## 6. Alat dan Versi yang Harus Dicatat

Mahasiswa wajib mencatat versi alat berikut di laporan.

```bash
uname -a
cat /etc/os-release
clang --version || true
gcc --version || true
ld.lld --version || true
ld --version | head -n 1 || true
make --version | head -n 1
qemu-system-x86_64 --version || true
gdb --version | head -n 1 || true
readelf --version | head -n 1
objdump --version | head -n 1
nm --version | head -n 1
git --version
```

GNU Make digunakan untuk membangun target bertahap. Manual GNU Make menjelaskan bahwa `make` menentukan bagian program yang perlu dikompilasi ulang dan menjalankan command untuk memperbaruinya [8]. Linker script tetap relevan karena GNU ld/LLD harus mengontrol layout section kernel; dokumentasi GNU ld menyatakan linker script menentukan bagaimana section input dipetakan ke output dan mengontrol layout memori output [7].

---

## 7. Peta Ketergantungan dan Pemeriksaan Kesiapan M0–M7

Sebelum memulai M8, jangan menulis allocator baru terlebih dahulu. Jalankan audit kesiapan berikut agar bug dari tahap sebelumnya tidak tersembunyi sebagai bug heap.

| Tahap | Artefak wajib | Perintah pemeriksaan | Indikator lulus | Solusi jika gagal |
|---|---|---|---|---|
| M0 | WSL 2, Git, struktur repo, governance docs | `wsl --status`; `git status --short`; `pwd` | Repo berada di filesystem Linux WSL, Git bersih atau perubahan terdokumentasi | Pindahkan repo ke `~/mcsos`, bukan `/mnt/c/...`; commit atau stash perubahan sebelum M8 |
| M1 | Toolchain dan audit object | `clang --version`; `ld.lld --version`; `readelf --version`; `objdump --version` | Tool tersedia dan versi dicatat | Instal ulang `clang lld binutils make qemu-system-x86 gdb`; dokumentasikan versi |
| M2 | Boot image dan early console | `make run` atau target QEMU M2 | Serial log menampilkan entry kernel terkendali | Periksa Limine config, OVMF path, ISO recipe, linker script, dan serial redirection |
| M3 | Logging dan panic path | Jalankan varian panic | Panic log terbaca dan QEMU tidak hanya reset senyap | Pastikan serial init sebelum panic; jangan optimasi keluar infinite halt |
| M4 | IDT dan exception stubs | Trigger `int3` atau exception terkontrol | Vector exception dan register dasar tercetak | Audit `lidt`, stub error-code, `iretq`, stack alignment, dan trap frame layout |
| M5 | PIC/PIT/timer IRQ0 | Jalankan timer smoke test | Tick bertambah dan EOI benar | Remap PIC ke 0x20–0x2f; pastikan `sti` setelah IDT/PIC siap; hindari interrupt storm |
| M6 | PMM bitmap | Host unit test PMM; log frame stats | Total usable, free, allocated konsisten | Periksa parsing memory map, alignment 4096, region reserved, dan bitmap bounds |
| M7 | VMM awal dan page fault diagnostics | Host unit test VMM; audit `invlpg`; page-fault log | `map/query/unmap` lulus; CR2/error code tampil saat fault | Jangan aktifkan CR3 baru sebelum identity/HHDM/kernel/stack/IDT terpetakan; debug dengan GDB |

Perintah audit ringkas sebelum M8:

```bash
git status --short
make m6-all || true
make m7-all || true
find build -maxdepth 3 -type f \( -name '*.log' -o -name '*.map' -o -name '*.txt' \) | sort | tail -n 50
```

Jika target `m6-all` atau `m7-all` belum tersedia karena nama Makefile berbeda, jalankan target ekuivalen yang telah dibuat pada modul masing-masing dan catat nama target aktual dalam laporan.

---

## 8. Diagnosis Kendala dari Tahap Sebelumnya

Bagian ini harus dibaca sebagai checklist triase sebelum menganggap M8 bermasalah.

| Gejala | Kemungkinan akar masalah dari M0–M7 | Perbaikan konservatif |
|---|---|---|
| Build M8 gagal karena header tidak ditemukan | Struktur include M1/M2 belum konsisten | Pastikan `-Iinclude` ada di Makefile dan header berada di `include/mcsos/` |
| `nm -u` memuat `memset`, `memcpy`, atau `malloc` | Source kernel memanggil libc atau compiler builtin tidak dikontrol | Gunakan helper lokal seperti `kmem_memset`; compile dengan `-ffreestanding -fno-builtin`; jangan memakai libc di kernel |
| QEMU reset saat heap init | Heap arena belum terpetakan atau pointer base salah | Pakai arena statik `.bss` dulu; jangan pakai virtual heap tinggi sebelum VMM map terbukti |
| Page fault saat `kmem_alloc` | Arena berada di virtual address yang belum present atau header menyeberang halaman tidak termap | Cek CR2, error code, dan mapping M7; map semua page arena sebelum `kmem_init` |
| Double free tidak terdeteksi | Metadata block tidak memuat status free/used atau validasi header lemah | Gunakan `magic`, flag `free`, dan `kmem_free_checked` yang mengembalikan error negatif |
| Host test lulus tetapi kernel hang | Host test memakai libc, alignment/ABI kernel berbeda, atau stack/heap region kernel tidak valid | Audit object freestanding; log alamat heap base/end; validasi `kmem_validate()` setelah init dan setelah setiap operasi awal |
| Allocator rusak setelah IRQ timer | `kmem_alloc` dipanggil dari interrupt handler tanpa lock atau reentrancy guard | Untuk M8, larang alokasi dari IRQ. Log dan panic jika ada penggunaan dari IRQ context |
| Fragmentasi cepat | First-fit memecah block terlalu kecil atau tidak coalesce | Terapkan `KMEM_MIN_SPLIT`, coalesce forward/backward saat free, dan tampilkan `largest_free` |
| Kernel image membesar tak terkontrol | Test host masuk ke build kernel atau debug symbol tidak dipisah | Pisahkan target host test dari target kernel; jangan link `tests/test_kmem.c` ke kernel image |
| Linker script membuang symbol heap | Section `.bss`/`.data` tidak disertakan atau alignment section salah | Audit linker map; pastikan `.bss` dan COMMON masuk dan page-aligned bila digunakan sebagai arena |

---

## 9. Architecture dan Design Allocator M8

Bagian ini menjelaskan architecture kernel heap M8 sebagai subsystem yang berada di atas PMM dan VMM. Design dibuat sengaja kecil agar setiap invariant dapat diuji dengan host unit test dan audit object.

## 9. Kontrak Desain M8

### 9.1. Peran PMM, VMM, dan Kernel Heap

PMM mengelola frame fisik. VMM mengelola pemetaan virtual ke physical. Kernel heap mengelola object berukuran byte di atas rentang virtual yang sudah terpetakan. Pemisahan ini wajib dijaga.

| Lapisan | Unit alokasi | Contoh API | Tanggung jawab | Tidak boleh dilakukan |
|---|---:|---|---|---|
| PMM M6 | Frame 4 KiB | `pmm_alloc_frame()` | Memilih frame fisik usable | Mengembalikan pointer object kecil langsung ke subsistem tinggi |
| VMM M7 | Page 4 KiB | `vmm_map_page()` | Membuat mapping virtual ke physical | Mengetahui layout object heap |
| Kernel heap M8 | Byte/object | `kmem_alloc()` | Mengelola object kecil dan sedang dalam arena | Mengubah page table tanpa kontrak VMM |

### 9.2. Invariant Wajib Allocator

1. `g_heap_base <= block_header < g_heap_end` untuk setiap block.
2. Header setiap block memiliki `magic == KMEM_MAGIC` selama block masih bagian dari list aktif.
3. Payload yang dikembalikan `kmem_alloc` aligned 16 byte.
4. `block->size` menyatakan kapasitas payload, bukan ukuran header.
5. Setiap block memiliki status tepat satu dari dua: free atau used.
6. Dua block free yang bertetangga harus dapat dicoalesce saat `kmem_free_checked` dipanggil.
7. `kmem_free_checked(NULL)` adalah no-op sukses.
8. Double free harus ditolak dengan error negatif.
9. Pointer di luar arena harus ditolak dengan error negatif.
10. Tidak boleh ada call ke `malloc`, `free`, `printf`, `memset`, atau fungsi libc lain dari object kernel freestanding.
11. `kmem_validate()` harus lulus setelah `kmem_init`, setelah alokasi, dan setelah free pada jalur uji.
12. Allocator M8 belum reentrant dan belum SMP-safe; pemakaian dari interrupt handler dilarang.

### 9.3. Kompleksitas Operasi

| Operasi | Kompleksitas M8 | Catatan |
|---|---:|---|
| `kmem_init` | O(1) | Membentuk satu block free besar |
| `kmem_alloc` | O(n) | First-fit scan terhadap jumlah block |
| `kmem_free_checked` | O(1) amortized lokal | Coalesce tetangga langsung; validasi penuh O(n) karena praktikum mengutamakan correctness |
| `kmem_validate` | O(n) | Dijalankan pada checkpoint dan debug, bukan jalur cepat produksi |
| `kmem_get_stats` | O(n) | Menghitung statistik observability |

---

## 10. Struktur Repository M8

Tambahkan file berikut pada repo MCSOS.

```text
include/
└── mcsos/
    └── kmem.h
kernel/
└── mm/
    └── kmem.c
tests/
└── test_kmem.c
scripts/
└── check_m8_kmem.sh
Makefile
```

Jika repo sebelumnya memakai nama direktori berbeda, pertahankan konvensi repo, tetapi dokumentasikan pemetaan nama direktori dalam laporan.

---

## 11. Implementation Plan / Rencana Implementasi M8

Implementation plan M8 terdiri atas branch kerja, penambahan header, implementasi allocator, host unit test, Makefile, script preflight, audit freestanding, dan integrasi kernel bertahap.

## 11. Langkah Praktikum M8

### 11.1. Buat Branch Kerja

Langkah ini memisahkan perubahan M8 dari modul sebelumnya agar rollback dapat dilakukan tanpa menghapus hasil M6/M7.

```bash
git status --short
git switch -c praktikum-m8-kernel-heap
mkdir -p include/mcsos kernel/mm tests scripts build/m8
```

Indikator hasil: branch baru aktif dan direktori M8 tersedia.

```bash
git branch --show-current
find include kernel tests scripts -maxdepth 3 -type d | sort
```

### 11.2. Tambahkan Header `include/mcsos/kmem.h`

Header ini mendefinisikan API publik allocator M8. API sengaja kecil. Fungsi `kmem_free_checked` mengembalikan kode error agar unit test dapat membedakan free valid, double free, pointer invalid, dan corruption.

```c
#ifndef MCSOS_KMEM_H
#define MCSOS_KMEM_H

#include <stddef.h>
#include <stdint.h>

#define KMEM_ALIGN 16u
#define KMEM_MAGIC 0x4d43534f53484541ull

typedef struct kmem_stats {
    size_t total_bytes;
    size_t used_bytes;
    size_t free_bytes;
    size_t block_count;
    size_t free_count;
    size_t largest_free;
} kmem_stats_t;

int kmem_init(void *base, size_t bytes);
void *kmem_alloc(size_t bytes);
void *kmem_calloc(size_t count, size_t bytes);
int kmem_free_checked(void *ptr);
void kmem_get_stats(kmem_stats_t *out);
int kmem_validate(void);

#endif

```

Simpan sebagai:

```bash
$EDITOR include/mcsos/kmem.h
```

### 11.3. Tambahkan Implementasi `kernel/mm/kmem.c`

Implementasi berikut adalah source inti M8. Source sudah diuji kompilasi freestanding dan host unit test pada lingkungan validasi lokal. Mahasiswa tetap wajib menjalankan ulang semua perintah di WSL 2 masing-masing.

```c
#include "mcsos/kmem.h"

#define KMEM_MIN_SPLIT 32u

typedef struct kmem_block {
    uint64_t magic;
    size_t size;
    int free;
    uint32_t reserved;
    uint64_t reserved2;
    struct kmem_block *prev;
    struct kmem_block *next;
} kmem_block_t;

static unsigned char *g_heap_base;
static unsigned char *g_heap_end;
static kmem_block_t *g_head;
static int g_initialized;

static size_t kmem_align_up_size(size_t value, size_t align) {
    if (align == 0u) {
        return value;
    }
    const size_t mask = align - 1u;
    if ((align & mask) != 0u) {
        return 0u;
    }
    if (value > (SIZE_MAX - mask)) {
        return 0u;
    }
    return (value + mask) & ~mask;
}

static uintptr_t kmem_align_up_ptr(uintptr_t value, uintptr_t align) {
    const uintptr_t mask = align - 1u;
    if ((align & mask) != 0u) {
        return 0u;
    }
    if (value > (UINTPTR_MAX - mask)) {
        return 0u;
    }
    return (value + mask) & ~mask;
}

static void *kmem_memset(void *dst, int value, size_t bytes) {
    unsigned char *p = (unsigned char *)dst;
    while (bytes-- > 0u) {
        *p++ = (unsigned char)value;
    }
    return dst;
}

static unsigned char *kmem_payload(kmem_block_t *block) {
    return ((unsigned char *)block) + sizeof(kmem_block_t);
}

static kmem_block_t *kmem_header_from_payload(void *ptr) {
    return (kmem_block_t *)(((unsigned char *)ptr) - sizeof(kmem_block_t));
}

static int kmem_ptr_in_heap(const void *ptr) {
    const unsigned char *p = (const unsigned char *)ptr;
    return g_initialized && p >= g_heap_base && p < g_heap_end;
}

static void kmem_split_if_useful(kmem_block_t *block, size_t wanted) {
    const size_t header = kmem_align_up_size(sizeof(kmem_block_t), KMEM_ALIGN);
    if (header == 0u) {
        return;
    }
    if (block->size < wanted + header + KMEM_MIN_SPLIT) {
        return;
    }

    unsigned char *new_addr = kmem_payload(block) + wanted;
    new_addr = (unsigned char *)kmem_align_up_ptr((uintptr_t)new_addr, KMEM_ALIGN);
    if (new_addr == (unsigned char *)0) {
        return;
    }
    if (new_addr + sizeof(kmem_block_t) >= g_heap_end) {
        return;
    }

    const size_t consumed = (size_t)(new_addr - kmem_payload(block));
    if (block->size <= consumed + sizeof(kmem_block_t) + KMEM_MIN_SPLIT) {
        return;
    }

    kmem_block_t *new_block = (kmem_block_t *)new_addr;
    new_block->magic = KMEM_MAGIC;
    new_block->size = block->size - consumed - sizeof(kmem_block_t);
    new_block->free = 1;
    new_block->prev = block;
    new_block->next = block->next;
    if (block->next != (kmem_block_t *)0) {
        block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = wanted;
}

static void kmem_coalesce_forward(kmem_block_t *block) {
    while (block != (kmem_block_t *)0 && block->next != (kmem_block_t *)0 && block->next->free) {
        kmem_block_t *next = block->next;
        unsigned char *expected = kmem_payload(block) + block->size;
        expected = (unsigned char *)kmem_align_up_ptr((uintptr_t)expected, KMEM_ALIGN);
        if (expected != (unsigned char *)next) {
            return;
        }
        block->size += sizeof(kmem_block_t) + next->size;
        block->next = next->next;
        if (next->next != (kmem_block_t *)0) {
            next->next->prev = block;
        }
        next->magic = 0u;
        next->size = 0u;
        next->prev = (kmem_block_t *)0;
        next->next = (kmem_block_t *)0;
    }
}

int kmem_init(void *base, size_t bytes) {
    if (base == (void *)0 || bytes < (sizeof(kmem_block_t) + KMEM_MIN_SPLIT)) {
        return -1;
    }

    uintptr_t start = kmem_align_up_ptr((uintptr_t)base, KMEM_ALIGN);
    if (start == 0u || start < (uintptr_t)base) {
        return -2;
    }
    const size_t lost = (size_t)(start - (uintptr_t)base);
    if (bytes <= lost + sizeof(kmem_block_t) + KMEM_MIN_SPLIT) {
        return -3;
    }

    size_t usable = bytes - lost;
    usable = usable & ~(size_t)(KMEM_ALIGN - 1u);
    if (usable <= sizeof(kmem_block_t) + KMEM_MIN_SPLIT) {
        return -4;
    }

    g_heap_base = (unsigned char *)start;
    g_heap_end = g_heap_base + usable;
    g_head = (kmem_block_t *)g_heap_base;
    g_head->magic = KMEM_MAGIC;
    g_head->size = usable - sizeof(kmem_block_t);
    g_head->free = 1;
    g_head->prev = (kmem_block_t *)0;
    g_head->next = (kmem_block_t *)0;
    g_initialized = 1;
    return kmem_validate();
}

void *kmem_alloc(size_t bytes) {
    if (!g_initialized || bytes == 0u) {
        return (void *)0;
    }
    const size_t wanted = kmem_align_up_size(bytes, KMEM_ALIGN);
    if (wanted == 0u) {
        return (void *)0;
    }

    for (kmem_block_t *cur = g_head; cur != (kmem_block_t *)0; cur = cur->next) {
        if (cur->magic != KMEM_MAGIC) {
            return (void *)0;
        }
        if (cur->free && cur->size >= wanted) {
            kmem_split_if_useful(cur, wanted);
            cur->free = 0;
            return (void *)kmem_payload(cur);
        }
    }
    return (void *)0;
}

void *kmem_calloc(size_t count, size_t bytes) {
    if (count != 0u && bytes > SIZE_MAX / count) {
        return (void *)0;
    }
    const size_t total = count * bytes;
    void *ptr = kmem_alloc(total);
    if (ptr != (void *)0) {
        (void)kmem_memset(ptr, 0, total);
    }
    return ptr;
}

int kmem_free_checked(void *ptr) {
    if (ptr == (void *)0) {
        return 0;
    }
    if (!kmem_ptr_in_heap(ptr)) {
        return -1;
    }
    if (((uintptr_t)ptr & (KMEM_ALIGN - 1u)) != 0u) {
        return -2;
    }
    kmem_block_t *block = kmem_header_from_payload(ptr);
    if (!kmem_ptr_in_heap(block) || block->magic != KMEM_MAGIC) {
        return -3;
    }
    if (block->free) {
        return -4;
    }
    block->free = 1;
    kmem_coalesce_forward(block);
    if (block->prev != (kmem_block_t *)0 && block->prev->free) {
        kmem_coalesce_forward(block->prev);
    }
    return kmem_validate();
}

void kmem_get_stats(kmem_stats_t *out) {
    if (out == (kmem_stats_t *)0) {
        return;
    }
    out->total_bytes = 0u;
    out->used_bytes = 0u;
    out->free_bytes = 0u;
    out->block_count = 0u;
    out->free_count = 0u;
    out->largest_free = 0u;

    if (!g_initialized) {
        return;
    }
    out->total_bytes = (size_t)(g_heap_end - g_heap_base);
    for (kmem_block_t *cur = g_head; cur != (kmem_block_t *)0; cur = cur->next) {
        out->block_count++;
        if (cur->free) {
            out->free_count++;
            out->free_bytes += cur->size;
            if (cur->size > out->largest_free) {
                out->largest_free = cur->size;
            }
        } else {
            out->used_bytes += cur->size;
        }
    }
}

int kmem_validate(void) {
    if (!g_initialized || g_heap_base == (unsigned char *)0 || g_heap_end <= g_heap_base || g_head == (kmem_block_t *)0) {
        return -1;
    }
    if ((unsigned char *)g_head != g_heap_base) {
        return -2;
    }

    kmem_block_t *prev = (kmem_block_t *)0;
    unsigned char *cursor = g_heap_base;
    size_t guard = 0u;
    for (kmem_block_t *cur = g_head; cur != (kmem_block_t *)0; cur = cur->next) {
        if (++guard > 1048576u) {
            return -3;
        }
        if ((unsigned char *)cur != cursor) {
            return -4;
        }
        if ((unsigned char *)cur < g_heap_base || ((unsigned char *)cur + sizeof(kmem_block_t)) > g_heap_end) {
            return -5;
        }
        if (cur->magic != KMEM_MAGIC) {
            return -6;
        }
        if (cur->prev != prev) {
            return -7;
        }
        if (cur->size > (size_t)(g_heap_end - kmem_payload(cur))) {
            return -8;
        }
        cursor = kmem_payload(cur) + cur->size;
        cursor = (unsigned char *)kmem_align_up_ptr((uintptr_t)cursor, KMEM_ALIGN);
        if (cursor == (unsigned char *)0 || cursor > g_heap_end) {
            return -9;
        }
        prev = cur;
    }
    return 0;
}

```

Simpan sebagai:

```bash
$EDITOR kernel/mm/kmem.c
```

### 11.4. Tambahkan Host Unit Test `tests/test_kmem.c`

Host unit test dipakai karena allocator adalah algoritma murni yang bisa diuji sebelum masuk QEMU. Test ini tidak menggantikan QEMU test, tetapi mempercepat diagnosis bug pointer arithmetic, alignment, split, coalesce, dan overflow.

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mcsos/kmem.h"

static unsigned char arena[4096u * 8u];

static void test_basic_alloc_free(void) {
    assert(kmem_init(arena, sizeof(arena)) == 0);
    void *a = kmem_alloc(24);
    void *b = kmem_alloc(128);
    void *c = kmem_alloc(4096);
    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);
    assert(((uintptr_t)a & (KMEM_ALIGN - 1u)) == 0u);
    assert(((uintptr_t)b & (KMEM_ALIGN - 1u)) == 0u);
    assert(((uintptr_t)c & (KMEM_ALIGN - 1u)) == 0u);
    memset(a, 0x11, 24);
    memset(b, 0x22, 128);
    memset(c, 0x33, 4096);
    assert(kmem_validate() == 0);
    assert(kmem_free_checked(b) == 0);
    assert(kmem_free_checked(a) == 0);
    assert(kmem_free_checked(c) == 0);
    assert(kmem_validate() == 0);
}

static void test_calloc_and_overflow(void) {
    assert(kmem_init(arena, sizeof(arena)) == 0);
    unsigned char *z = (unsigned char *)kmem_calloc(64, 4);
    assert(z != NULL);
    for (size_t i = 0; i < 256; ++i) {
        assert(z[i] == 0u);
    }
    assert(kmem_calloc((size_t)-1, 2) == NULL);
    assert(kmem_free_checked(z) == 0);
}

static void test_double_free_rejected(void) {
    assert(kmem_init(arena, sizeof(arena)) == 0);
    void *p = kmem_alloc(512);
    assert(p != NULL);
    assert(kmem_free_checked(p) == 0);
    assert(kmem_free_checked(p) < 0);
}

static void test_fragmentation_and_coalesce(void) {
    assert(kmem_init(arena, sizeof(arena)) == 0);
    void *p[16];
    for (size_t i = 0; i < 16; ++i) {
        p[i] = kmem_alloc(256 + i);
        assert(p[i] != NULL);
    }
    for (size_t i = 0; i < 16; i += 2) {
        assert(kmem_free_checked(p[i]) == 0);
    }
    for (size_t i = 1; i < 16; i += 2) {
        assert(kmem_free_checked(p[i]) == 0);
    }
    kmem_stats_t st;
    kmem_get_stats(&st);
    assert(st.free_count == 1u);
    assert(st.block_count == 1u);
    assert(st.largest_free > 4096u);
}

int main(void) {
    test_basic_alloc_free();
    test_calloc_and_overflow();
    test_double_free_rejected();
    test_fragmentation_and_coalesce();
    puts("M8 kmem host tests: PASS");
    return 0;
}

```

Simpan sebagai:

```bash
$EDITOR tests/test_kmem.c
```

### 11.5. Tambahkan Target Makefile M8

Tambahkan target berikut ke `Makefile` utama. Jika Makefile Anda sudah memiliki variabel `CC`, `CFLAGS`, atau `BUILD_DIR`, integrasikan secara hati-hati agar tidak menimpa target M0–M7.

```makefile
CC ?= clang
CFLAGS_COMMON := -std=c17 -Wall -Wextra -Werror -Iinclude
CFLAGS_KERNEL := $(CFLAGS_COMMON) -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone
BUILD_DIR := build/m8

.PHONY: m8-clean m8-kmem-host-test m8-kmem-freestanding m8-audit m8-all

m8-clean:
	$(RM) -r $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

m8-kmem-freestanding: | $(BUILD_DIR)
	$(CC) $(CFLAGS_KERNEL) -c kernel/mm/kmem.c -o $(BUILD_DIR)/kmem.freestanding.o

m8-kmem-host-test: | $(BUILD_DIR)
	$(CC) $(CFLAGS_COMMON) tests/test_kmem.c kernel/mm/kmem.c -o $(BUILD_DIR)/test_kmem
	./$(BUILD_DIR)/test_kmem | tee $(BUILD_DIR)/test_kmem.log

m8-audit: m8-kmem-freestanding
	nm -u $(BUILD_DIR)/kmem.freestanding.o | tee $(BUILD_DIR)/nm_u.txt
	test ! -s $(BUILD_DIR)/nm_u.txt
	readelf -h $(BUILD_DIR)/kmem.freestanding.o > $(BUILD_DIR)/readelf_h.txt
	objdump -dr $(BUILD_DIR)/kmem.freestanding.o > $(BUILD_DIR)/kmem.objdump.txt

m8-all: m8-kmem-host-test m8-audit

```

Makna target:

| Target | Fungsi | Artefak |
|---|---|---|
| `m8-kmem-host-test` | Membangun dan menjalankan unit test allocator pada host WSL | `build/m8/test_kmem`, `build/m8/test_kmem.log` |
| `m8-kmem-freestanding` | Mengompilasi `kmem.c` sebagai object kernel freestanding | `build/m8/kmem.freestanding.o` |
| `m8-audit` | Memastikan object tidak punya unresolved symbol dan menyimpan audit ELF/disassembly | `nm_u.txt`, `readelf_h.txt`, `kmem.objdump.txt` |
| `m8-all` | Menjalankan host test dan audit freestanding | Semua artefak M8 |

### 11.6. Tambahkan Script Preflight `scripts/check_m8_kmem.sh`

Script ini menjadi checklist otomatis untuk memastikan file M8 tersedia, toolchain ada, source dapat dikompilasi freestanding, dan host unit test lulus.

```bash
#!/usr/bin/env bash
set -euo pipefail

printf '[M8] checking repository baseline...\n'
required=(
  include/mcsos/kmem.h
  kernel/mm/kmem.c
  tests/test_kmem.c
  Makefile
)
for f in "${required[@]}"; do
  if [[ ! -f "$f" ]]; then
    printf '[FAIL] missing %s\n' "$f" >&2
    exit 1
  fi
done

printf '[M8] checking toolchain...\n'
command -v clang >/dev/null
command -v nm >/dev/null
command -v objdump >/dev/null
command -v readelf >/dev/null
command -v make >/dev/null

printf '[M8] tool versions...\n'
clang --version | head -n 1
ld.lld --version 2>/dev/null | head -n 1 || true
make --version | head -n 1

printf '[M8] freestanding object check...\n'
mkdir -p build/m8
clang -std=c17 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
  -Iinclude -c kernel/mm/kmem.c -o build/m8/kmem.freestanding.o
nm -u build/m8/kmem.freestanding.o | tee build/m8/nm_u.txt
if [[ -s build/m8/nm_u.txt ]]; then
  printf '[FAIL] unresolved symbol found in kmem.freestanding.o\n' >&2
  exit 1
fi
objdump -dr build/m8/kmem.freestanding.o > build/m8/kmem.objdump.txt
readelf -h build/m8/kmem.freestanding.o > build/m8/readelf_h.txt

printf '[M8] host unit test...\n'
clang -std=c17 -Wall -Wextra -Werror -Iinclude \
  tests/test_kmem.c kernel/mm/kmem.c -o build/m8/test_kmem
./build/m8/test_kmem | tee build/m8/test_kmem.log

grep -q 'PASS' build/m8/test_kmem.log
printf '[PASS] M8 preflight completed.\n'

```

Simpan dan beri izin eksekusi.

```bash
chmod +x scripts/check_m8_kmem.sh
```

### 11.7. Jalankan Host Unit Test

Perintah ini menjalankan test algoritmik. Hasil yang diharapkan adalah `M8 kmem host tests: PASS`.

```bash
make m8-kmem-host-test
```

Alternatif langsung tanpa Makefile:

```bash
mkdir -p build/m8
clang -std=c17 -Wall -Wextra -Werror -Iinclude   tests/test_kmem.c kernel/mm/kmem.c -o build/m8/test_kmem
./build/m8/test_kmem | tee build/m8/test_kmem.log
```

Output minimum:

```text
M8 kmem host tests: PASS
```

### 11.8. Jalankan Audit Freestanding Object

Audit ini memastikan source inti allocator tidak bergantung pada libc host. Dalam kernel freestanding, `malloc`, `free`, `printf`, atau `memset` dari libc tidak boleh menjadi dependensi implisit.

```bash
make m8-audit
```

Alternatif langsung:

```bash
mkdir -p build/m8
clang -std=c17 -Wall -Wextra -Werror -ffreestanding -fno-builtin   -Iinclude -c kernel/mm/kmem.c -o build/m8/kmem.freestanding.o
nm -u build/m8/kmem.freestanding.o | tee build/m8/nm_u.txt
readelf -h build/m8/kmem.freestanding.o | tee build/m8/readelf_h.txt
objdump -dr build/m8/kmem.freestanding.o > build/m8/kmem.objdump.txt
test ! -s build/m8/nm_u.txt
```

Indikator hasil:

1. `build/m8/nm_u.txt` kosong.
2. `readelf -h` menunjukkan `ELF64`, little endian, dan machine `Advanced Micro Devices X86-64` atau ekuivalen x86-64.
3. `objdump -dr` memiliki symbol `kmem_init`, `kmem_alloc`, `kmem_calloc`, `kmem_free_checked`, `kmem_get_stats`, dan `kmem_validate`.

### 11.9. Integrasikan ke Kernel MCSOS

Untuk tugas wajib, gunakan arena bootstrap statik terlebih dahulu. Arena ini berada pada `.bss`, sehingga harus sudah terpetakan oleh kernel mapping yang sudah stabil dari M2–M7. Jangan langsung memakai virtual heap tinggi sebelum VMM M7 benar-benar tervalidasi.

Contoh integrasi minimal di file init kernel, misalnya `kernel/kernel.c` atau file yang memuat `kernel_main`.

```c
#include "mcsos/kmem.h"

#define M8_BOOT_HEAP_SIZE (64u * 1024u)
static unsigned char m8_boot_heap[M8_BOOT_HEAP_SIZE] __attribute__((aligned(4096)));

static void m8_heap_bootstrap(void) {
    int rc = kmem_init(m8_boot_heap, sizeof(m8_boot_heap));
    if (rc != 0) {
        kernel_panic("M8 kmem_init failed");
    }

    void *probe = kmem_alloc(128);
    if (probe == 0) {
        kernel_panic("M8 kmem_alloc probe failed");
    }

    if (kmem_free_checked(probe) != 0) {
        kernel_panic("M8 kmem_free_checked probe failed");
    }

    kmem_stats_t st;
    kmem_get_stats(&st);
    klog_info("M8 kmem initialized: total=%zu free=%zu largest=%zu blocks=%zu",
              st.total_bytes, st.free_bytes, st.largest_free, st.block_count);
}
```

Jika formatter kernel belum mendukung `%zu`, cetak sebagai `uint64_t` atau pecah menjadi fungsi log angka yang sudah tersedia. Jangan menarik `printf` libc ke kernel.

Panggil setelah logging, panic path, PMM, dan VMM tahap dasar siap:

```c
void kernel_main(void) {
    serial_init();
    klog_info("MCSOS entering kernel_main");

    trap_init();
    timer_init();
    pmm_init_from_boot_memory_map();
    vmm_init_minimal();
    m8_heap_bootstrap();

    klog_info("M8 checkpoint reached");
    for (;;) {
        arch_hlt();
    }
}
```

### 11.10. Integrasi Page-Backed Heap sebagai Pengayaan

Pengayaan hanya boleh dikerjakan jika M7 stabil. Konsepnya adalah menyediakan rentang virtual heap, meminta frame dari PMM, memetakan frame dengan VMM, lalu memberi arena tersebut ke `kmem_init`.

Pseudocode konservatif:

```c
#define KHEAP_BASE 0xffffffff90000000ull
#define KHEAP_SIZE (256ull * 1024ull)

int kheaphys_map_initial_pages(void) {
    for (uint64_t va = KHEAP_BASE; va < KHEAP_BASE + KHEAP_SIZE; va += 4096) {
        uint64_t pa = pmm_alloc_frame();
        if (pa == 0) {
            return -1;
        }
        int rc = vmm_map_page(kernel_space, va, pa, VMM_PRESENT | VMM_WRITABLE | VMM_NO_USER);
        if (rc != 0) {
            pmm_free_frame(pa);
            return -2;
        }
    }
    return kmem_init((void *)KHEAP_BASE, KHEAP_SIZE);
}
```

Pengayaan ini wajib dilengkapi rollback. Jika satu mapping gagal, frame yang sudah dialokasikan harus dilepas atau ditandai sebagai leaked dengan bukti log. Jangan meninggalkan state sebagian tanpa catatan.

---

## 12. Validation Plan / Rencana Validasi M8

Validation plan M8 menggabungkan unit test host, compile freestanding, audit unresolved symbol, audit ELF/disassembly, dan QEMU smoke test bila integrasi kernel sudah dilakukan.

## 12. Perintah Uji Wajib

Jalankan perintah berikut dari clean checkout atau setelah `make m8-clean`.

```bash
make m8-clean
make m8-all
./scripts/check_m8_kmem.sh
git status --short
```

Jika integrasi QEMU sudah dilakukan:

```bash
make clean
make
make run 2>&1 | tee build/m8/qemu_m8.log
```

Jika target QEMU Anda berbeda, gunakan target yang sudah dipakai pada M2–M7. Yang penting adalah serial log disimpan.

### 12.1. Bukti Minimal yang Harus Disimpan

| Bukti | File disarankan | Isi minimum |
|---|---|---|
| Host test | `build/m8/test_kmem.log` | `M8 kmem host tests: PASS` |
| Unresolved symbol | `build/m8/nm_u.txt` | Kosong |
| ELF header | `build/m8/readelf_h.txt` | ELF64 x86-64 relocatable object |
| Disassembly | `build/m8/kmem.objdump.txt` | Symbol allocator terlihat |
| QEMU log | `build/m8/qemu_m8.log` | `M8 kmem initialized` dan statistik heap |
| Git diff | `build/m8/git_diff.patch` | Perubahan source M8 |
| Laporan | `laporan_M8_<nama>.md` atau PDF sesuai instruksi dosen | Analisis, bukti, failure mode, dan readiness review |

---

## 13. Hasil Validasi Source Inti M8 pada Lingkungan Penyusunan

Validasi ini dilakukan pada source yang disertakan dalam panduan. Mahasiswa tetap wajib menjalankan ulang di WSL 2 masing-masing.

### 13.1. Host Unit Test

```text
M8 kmem host tests: PASS
```

### 13.2. `nm -u`

```text
kosong
```

Makna: object freestanding tidak memuat unresolved external symbol pada source inti `kmem.c`.

### 13.3. `readelf -h`

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
  Start of section headers:          4728 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           64 (bytes)
  Number of section headers:         9
  Section header string table index: 1
```

### 13.4. Symbol yang Terlihat pada Disassembly

```text
0000000000000000 <kmem_init>:
00000000000001c0 <kmem_validate>:
0000000000000390 <kmem_alloc>:
0000000000000670 <kmem_calloc>:
0000000000000750 <kmem_free_checked>:
00000000000009c0 <kmem_get_stats>:
```

Hasil ini hanya membuktikan bahwa source inti M8 dapat dikompilasi dan diuji pada lingkungan penyusunan. Ini bukan bukti bahwa integrasi repo mahasiswa, ISO, QEMU, OVMF, dan bootloader sudah benar.

---

## 14. Workflow QEMU dan GDB

QEMU mendukung debugging guest melalui gdbstub; opsi `-s` membuat QEMU mendengarkan koneksi GDB pada port TCP 1234 dan `-S` membuat guest berhenti sampai GDB melanjutkan eksekusi [9]. Gunakan ini ketika kernel hang atau page fault terjadi saat heap init.

Jalankan QEMU dalam mode menunggu GDB:

```bash
qemu-system-x86_64   -machine q35   -m 512M   -serial stdio   -no-reboot   -no-shutdown   -s -S   -cdrom build/mcsos.iso
```

Pada terminal kedua:

```bash
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) break m8_heap_bootstrap
(gdb) break kmem_init
(gdb) break kmem_alloc
(gdb) continue
(gdb) info registers
(gdb) bt
(gdb) x/32gx &m8_boot_heap
```

Jika terjadi page fault, catat CR2 dan error code dari handler M7. Jika CR2 berada di sekitar alamat heap, kemungkinan arena heap belum terpetakan atau metadata melewati batas page.

---

## 15. Security dan Threat Model Ringkas M8

Threat model M8 mencakup bug lokal yang berasal dari kernel sendiri: double free, use-after-free, heap overflow, integer overflow saat menghitung ukuran, pointer di luar arena, metadata corruption, fragmentasi, dan pemanggilan allocator dari interrupt context. M8 belum menghadapi attacker userspace karena user mode belum menjadi syarat wajib. Security control minimum adalah validasi range, magic header, flag free/used, overflow check pada `kmem_calloc`, larangan dependensi libc, dan audit `nm -u`.

## 15. Failure Modes M8 dan Perbaikan

| Failure mode | Gejala | Penyebab umum | Langkah diagnosis | Perbaikan |
|---|---|---|---|---|
| Header tidak aligned | Host test alignment gagal | `sizeof(kmem_block_t)` bukan kelipatan 16 | Print `sizeof(kmem_block_t)` di host test | Tambahkan padding atau gunakan header size yang di-align |
| Double free | `free` kedua diterima | Flag `free` tidak dicek | Tambahkan test double free | Tolak jika `block->free != 0` |
| Out-of-arena pointer | Kernel menerima pointer liar | Validasi range kurang | Test `kmem_free_checked((void*)0x1234)` pada host | Cek `g_heap_base <= ptr < g_heap_end` |
| Metadata corruption | `kmem_validate` gagal | Overwrite sebelum payload atau use-after-free | Audit lokasi CR2, objdump, dan guard pattern | Tambahkan magic, red zone pengayaan, atau panic saat validate gagal |
| Fragmentasi | Large alloc gagal walau total free besar | Block free tidak dicoalesce | Periksa `free_count` dan `largest_free` | Coalesce block tetangga saat free |
| `nm -u` tidak kosong | Ada dependensi libc | Memakai `memset`, `printf`, `malloc`, atau builtins | Buka `nm_u.txt` | Gunakan helper lokal dan flag `-ffreestanding -fno-builtin` |
| Page fault saat heap init | QEMU reset/hang | Arena virtual belum dimap | Baca CR2 dari page fault handler | Gunakan arena `.bss` atau map semua page heap dahulu |
| IRQ corrupt heap | Kerusakan setelah timer tick | Allocator dipanggil dari handler IRQ | Tambahkan log context | Larang allocator dari IRQ pada M8; lock baru modul SMP/sync |
| Panic saat `klog_info` mencetak `%zu` | Formatter kernel belum mendukung size_t | Formatter minimal belum lengkap | Uji formatter terpisah | Cast ke `uint64_t` atau tulis helper angka |
| Build host test masuk kernel image | Linker gagal atau symbol libc muncul | Makefile mencampur target host dan kernel | Audit command line make | Pisahkan target `m8-kmem-host-test` dari target kernel |

---

## 16. Prosedur Rollback

Jika M8 menyebabkan boot gagal, lakukan rollback bertahap.

1. Nonaktifkan panggilan `m8_heap_bootstrap()` dari `kernel_main`, tetapi biarkan source `kmem.c` tetap ada untuk host test.
2. Jalankan kembali target M7 untuk memastikan VMM dan page fault path masih sehat.
3. Jika M7 sehat, jalankan `make m8-kmem-host-test`. Jika gagal, bug ada pada allocator murni.
4. Jika host test lulus tetapi QEMU gagal, bug kemungkinan berada pada arena heap, mapping, log formatter, atau urutan init.
5. Gunakan `git diff` untuk melihat perubahan integrasi.
6. Jika perlu rollback penuh:

```bash
git restore kernel include tests scripts Makefile
git clean -fd build/m8
```

Jangan menghapus bukti failure. Simpan log gagal sebagai lampiran laporan.

---

## 17. Acceptance Criteria / Kriteria Penerimaan M8

Acceptance criteria M8 adalah bukti konkret bahwa allocator dapat dibangun, diuji, diaudit, dan dijelaskan failure mode-nya.

## 17. Kriteria Lulus Praktikum

Minimum lulus M8:

1. Proyek dapat dibangun dari clean checkout atau dari state repo yang dijelaskan jelas.
2. `include/mcsos/kmem.h`, `kernel/mm/kmem.c`, dan `tests/test_kmem.c` tersedia.
3. `make m8-kmem-host-test` lulus.
4. `make m8-audit` lulus dan `build/m8/nm_u.txt` kosong.
5. `readelf_h.txt` menunjukkan object ELF64 x86-64.
6. `objdump` memuat symbol allocator utama.
7. Integrasi kernel minimal menghasilkan log `M8 kmem initialized` atau, jika belum diintegrasikan ke QEMU, alasan teknis dan rencana integrasi tertulis jelas.
8. Mahasiswa menjelaskan invariant allocator dan failure mode utama.
9. Tidak ada klaim “tanpa error” atau “siap produksi”.
10. Perubahan Git terkomit.
11. Laporan memuat bukti build, host test, audit object, serial log atau alasan belum ada serial log, analisis bug, rollback, dan readiness review.

Kriteria pengayaan:

1. Page-backed heap growth memakai PMM M6 dan VMM M7 dengan rollback saat mapping gagal.
2. Red-zone/canary sederhana untuk mendeteksi heap overrun.
3. Poison pattern saat free untuk membantu diagnosis use-after-free.
4. Statistik heap dicetak secara periodik atau saat panic.
5. `kmem_alloc_aligned(size, align)` dengan validasi power-of-two.
6. Lock awal untuk single CPU preemption-disabled context, disertai batasan bahwa belum SMP-safe.

---

## 18. Rubrik Penilaian 100 Poin

| Komponen | Poin | Indikator |
|---|---:|---|
| Kebenaran fungsional | 30 | API allocator lengkap, alignment benar, split/coalesce bekerja, double free ditolak, host test lulus |
| Kualitas desain dan invariants | 20 | Invariant ditulis jelas, ownership arena benar, batas PMM/VMM/heap tidak tercampur, error path terdefinisi |
| Pengujian dan bukti | 20 | Host unit test, `nm -u`, `readelf`, `objdump`, QEMU log, Git diff, dan bukti reproducible tersedia |
| Debugging/failure analysis | 10 | Failure modes dianalisis, CR2/page fault dipakai bila relevan, rollback terdokumentasi |
| Keamanan dan robustness | 10 | Overflow dicegah, pointer invalid ditolak, double free ditolak, libc dependency tidak ada, batas IRQ/SMP dinyatakan |
| Dokumentasi/laporan | 10 | Laporan lengkap, runtut, menyertakan lingkungan, desain, hasil, analisis, lampiran, dan referensi IEEE |
| **Total** | **100** |  |

---

## 19. Pertanyaan Analisis

Jawab pertanyaan berikut dalam laporan.

1. Mengapa kernel heap tidak boleh langsung menggantikan PMM?
2. Apa perbedaan tanggung jawab PMM, VMM, dan `kmem_alloc`?
3. Mengapa payload allocator harus aligned?
4. Mengapa `kmem_free_checked(NULL)` dibuat sukses?
5. Mengapa double free lebih baik ditolak daripada diabaikan?
6. Mengapa `kmem_validate()` O(n) masih dapat diterima pada praktikum ini?
7. Apa risiko memanggil allocator dari interrupt handler pada M8?
8. Mengapa host unit test tidak cukup untuk membuktikan integrasi kernel benar?
9. Bagaimana CR2 dan page fault error code membantu debugging heap?
10. Jika `nm -u` menampilkan `memset`, apa konsekuensinya untuk kernel freestanding?
11. Bagaimana first-fit dapat menyebabkan fragmentasi?
12. Kapan slab allocator lebih tepat daripada free-list umum?
13. Apa bukti minimum sebelum heap boleh dipakai oleh scheduler atau VFS?
14. Bagaimana prosedur rollback jika page-backed heap growth gagal di tengah mapping?
15. Apa residual risk M8 yang harus dibawa ke modul berikutnya?

---

## 20. Template Laporan Praktikum M8

Gunakan template berikut agar laporan konsisten dengan modul lain.

```markdown
# Laporan Praktikum M8 — Kernel Heap Awal dan Allocator Dinamis MCSOS

## 1. Sampul
- Judul praktikum: Praktikum M8 — Kernel Heap Awal dan Allocator Dinamis
- Nama mahasiswa/kelompok:
- NIM:
- Kelas:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi: Pendidikan Teknologi Informasi
- Institusi: Institut Pendidikan Indonesia
- Tanggal:

## 2. Tujuan
Tuliskan tujuan teknis dan konseptual M8.

## 3. Dasar Teori Ringkas
Jelaskan PMM, VMM, kernel heap, first-fit allocator, split, coalesce, alignment, double free, dan freestanding C.

## 4. Lingkungan
- Windows version:
- WSL distro:
- Kernel Linux WSL:
- Compiler:
- Linker:
- Make:
- QEMU:
- GDB:
- Commit hash:

## 5. Desain
- Diagram hubungan PMM, VMM, dan heap.
- Struktur `kmem_block`.
- Invariant allocator.
- Error path.
- Batasan IRQ/SMP.

## 6. Langkah Kerja
Tuliskan perintah yang dijalankan, file yang diubah, dan alasan teknis perubahan.

## 7. Hasil Uji
| Uji | Perintah | Hasil | Bukti |
|---|---|---|---|
| Host unit test | `make m8-kmem-host-test` | PASS/FAIL | `build/m8/test_kmem.log` |
| Freestanding compile | `make m8-kmem-freestanding` | PASS/FAIL | object file |
| Unresolved symbol | `nm -u` | kosong/tidak | `build/m8/nm_u.txt` |
| ELF audit | `readelf -h` | ELF64 x86-64/tidak | `build/m8/readelf_h.txt` |
| QEMU log | `make run` | PASS/FAIL | `build/m8/qemu_m8.log` |

## 8. Analisis
Jelaskan keberhasilan, bug yang ditemukan, penyebab, dan perbaikan.

## 9. Keamanan dan Reliability
Bahas double free, use-after-free, overflow, metadata corruption, fragmentation, IRQ/SMP risk, dan mitigasi.

## 10. Failure Modes dan Rollback
Tuliskan failure mode aktual dan prosedur rollback yang digunakan.

## 11. Kesimpulan
Tuliskan apa yang berhasil, apa yang belum, dan rencana M9.

## 12. Readiness Review
Pilih salah satu:
- Belum siap uji QEMU
- Siap uji QEMU untuk kernel heap awal
- Siap demonstrasi praktikum terbatas

Berikan alasan berbasis bukti.

## 13. Lampiran
- Potongan kode penting.
- `git diff`.
- Log penuh.
- Screenshot jika ada.
- Referensi IEEE.
```

---

## 21. Readiness Review M8

Gunakan matriks berikut saat menutup praktikum.

| Gate | Pertanyaan | Bukti minimum | Status |
|---|---|---|---|
| M8-G0 | Source allocator tersedia dan dapat dikompilasi? | `kmem.h`, `kmem.c`, object freestanding | Ya/Tidak |
| M8-G1 | Host unit test lulus? | `test_kmem.log` | Ya/Tidak |
| M8-G2 | Tidak ada dependensi libc pada object kernel? | `nm_u.txt` kosong | Ya/Tidak |
| M8-G3 | Invariant allocator tervalidasi? | `kmem_validate()` dipakai dalam test | Ya/Tidak |
| M8-G4 | Integrasi kernel tidak merusak M7? | QEMU log atau rollback evidence | Ya/Tidak |
| M8-G5 | Failure mode dan rollback terdokumentasi? | Bagian laporan | Ya/Tidak |
| M8-G6 | Git commit tersedia? | `git log --oneline -1` | Ya/Tidak |

Status yang boleh diberikan:

1. **Belum siap uji QEMU**: host test atau audit freestanding gagal.
2. **Siap uji QEMU untuk kernel heap awal**: host test dan audit freestanding lulus; integrasi kernel sudah disiapkan tetapi runtime QEMU perlu divalidasi pada host mahasiswa.
3. **Siap demonstrasi praktikum terbatas**: host test lulus, audit freestanding lulus, QEMU log menunjukkan heap initialized, panic path tetap terbaca, dan laporan lengkap.

Jangan memakai status “siap produksi” atau “tanpa error”.

---

## 22. Commit Akhir

Setelah semua bukti terkumpul, commit perubahan.

```bash
git add include/mcsos/kmem.h kernel/mm/kmem.c tests/test_kmem.c scripts/check_m8_kmem.sh Makefile
git add build/m8/test_kmem.log build/m8/nm_u.txt build/m8/readelf_h.txt || true
git commit -m "M8: add early kernel heap allocator"
git status --short
```

Jika kebijakan repo tidak memperbolehkan commit artefak build, jangan commit folder `build`; simpan artefak di laporan atau lampiran.

---

## 23. Referensi

[1] Intel Corporation, “Intel® 64 and IA-32 Architectures Software Developer Manuals,” updated Apr. 6, 2026. Accessed: May 3, 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

[2] Advanced Micro Devices, Inc., “AMD64 Architecture Programmer's Manual Volume 2: System Programming,” Publication No. 24593, Rev. 3.44, Mar. 6, 2026. Accessed: May 3, 2026. [Online]. Available: https://docs.amd.com/v/u/en-US/24593_3.44_APM_Vol2

[3] The Linux Kernel Documentation, “Memory Allocation Guide.” Accessed: May 3, 2026. [Online]. Available: https://docs.kernel.org/core-api/memory-allocation.html

[4] The Linux Kernel Documentation, “Memory Management Documentation.” Accessed: May 3, 2026. [Online]. Available: https://docs.kernel.org/mm/index.html

[5] `limine` crate documentation, “MemoryMapRequest.” Accessed: May 3, 2026. [Online]. Available: https://docs.rs/limine/latest/limine/request/struct.MemoryMapRequest.html

[6] `limine-protocol` crate documentation, “Limine protocol modules and requests.” Accessed: May 3, 2026. [Online]. Available: https://docs.rs/limine-protocol/latest/limine_protocol/

[7] GNU Binutils Documentation, “Linker Scripts.” Accessed: May 3, 2026. [Online]. Available: https://sourceware.org/binutils/docs/ld/Scripts.html

[8] Free Software Foundation, “GNU Make Manual,” GNU Make 4.4.1 manual edition 0.77, Feb. 26, 2023. Accessed: May 3, 2026. [Online]. Available: https://www.gnu.org/software/make/manual/make.html

[9] QEMU Project, “GDB usage,” QEMU documentation. Accessed: May 3, 2026. [Online]. Available: https://qemu-project.gitlab.io/qemu/system/gdb.html

[10] LLVM Project, “Clang command line argument reference.” Accessed: May 3, 2026. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html
