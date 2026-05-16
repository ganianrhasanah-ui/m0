# OS_panduan_M6.md

# Panduan Praktikum M6 — Physical Memory Manager, Boot Memory Map, dan Bitmap Frame Allocator pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M6  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: *siap uji QEMU untuk physical memory manager awal*, bukan siap produksi.

---

## 1. Ringkasan Praktikum

Praktikum M6 memperluas hasil M5 dari jalur external interrupt dan timer tick menjadi fondasi manajemen memori fisik. Pada tahap ini mahasiswa mengimplementasikan **Physical Memory Manager** atau PMM berbasis bitmap frame allocator. PMM bertugas mengubah informasi memory map dari bootloader menjadi himpunan frame fisik berukuran 4096 byte yang dapat dikelola secara deterministik oleh kernel. M6 belum mengaktifkan virtual memory manager baru, belum mengganti page table bootloader, dan belum menyediakan heap umum. Targetnya lebih sempit tetapi fundamental: kernel dapat mengetahui frame mana yang boleh dipakai, frame mana yang wajib tetap reserved, dan bagaimana satu frame fisik dapat dialokasikan serta dilepas tanpa merusak area kernel, modul, framebuffer, firmware, dan perangkat.

Praktikum ini memakai model konservatif. Semua frame pada awalnya dianggap **used**, lalu hanya region yang dinyatakan `USABLE` oleh memory map yang dibuka sebagai frame bebas. Frame 0 selalu dibuat reserved untuk menangkap kesalahan alamat fisik nol. Semua region non-usable ditandai used kembali setelah region usable diproses, karena dokumentasi Limine menyatakan region usable dan bootloader-reclaimable dijamin 4096-byte aligned dan tidak overlap, sedangkan tipe region lain tidak dijamin alignment maupun non-overlap [1]. Dengan urutan ini, jika ada region non-usable yang overlap akibat firmware atau loader, PMM tetap fail-closed.

Keberhasilan M6 tidak berarti sistem operasi bebas kesalahan. Target valid M6 adalah kode PMM dapat dibangun dari clean checkout, host unit test lulus, object freestanding tidak memiliki unresolved symbol, kernel dapat menampilkan ringkasan memori melalui serial log, dan alokasi frame sederhana dapat diuji tanpa panic maupun triple fault.

---

## 2. Asumsi Target dan Batasan

| Aspek | Keputusan M6 |
|---|---|
| Arsitektur | x86_64 long mode |
| Lingkungan host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Boot path | Melanjutkan M2–M5; direkomendasikan Limine/UEFI atau pipeline ISO yang sudah lulus M2 |
| Sumber memory map | Bootloader memory map; contoh konseptual kompatibel dengan Limine memory map |
| Toolchain | Clang/LLD atau GCC/binutils freestanding; contoh tervalidasi memakai Clang |
| Bahasa | C17 freestanding + assembly dari tahap M4/M5 |
| Kernel | Monolitik pendidikan, single-core awal, belum SMP |
| Page size awal | 4096 byte |
| PMM v0 scope | Frame allocator bitmap untuk memori fisik sampai batas konfigurasi awal `PMM_MAX_PHYS_BYTES` |
| Out of scope | Heap umum, paging baru, higher-half direct map buatan kernel, demand paging, user mode, copy-on-write, NUMA, hotplug memory, dan page cache |

Catatan batasan: Intel SDM mendeskripsikan lingkungan dukungan sistem operasi x86_64, termasuk memory management, protection, interrupt/exception, dan multiprocessing [2]. Akan tetapi M6 belum mengimplementasikan seluruh mekanisme paging. M6 hanya menyediakan daftar frame fisik yang akan dipakai M7/M8 untuk membangun page table dan allocator tingkat lebih tinggi.

---

## 2A. Goals dan Non-Goals

### Goals

1. Menghasilkan PMM awal berbasis bitmap yang dapat mengelola frame fisik 4096 byte.
2. Mengubah boot memory map menjadi status frame: used, free, reserved, allocated, atau ignored.
3. Menyediakan API `pmm_init_from_map`, `pmm_alloc_frame`, `pmm_free_frame`, `pmm_reserve_range`, dan query statistik.
4. Menyediakan host unit test agar logika PMM dapat diuji tanpa QEMU.
5. Menyediakan audit freestanding object agar PMM tidak memanggil libc host.
6. Menyediakan prosedur integrasi ke kernel MCSOS setelah serial log, panic path, IDT, dan timer dari M3–M5 stabil.
7. Menyediakan failure-mode diagnosis untuk triple fault, hang, salah hitung free frame, allocation leak, double free, dan reserved-region corruption.

### Non-Goals

1. Tidak membuat virtual memory manager penuh.
2. Tidak mengganti CR3 atau mengatur page table baru.
3. Tidak membangun heap dinamis umum seperti `kmalloc`.
4. Tidak mereklamasi `BOOTLOADER_RECLAIMABLE` secara otomatis sebelum kernel memiliki page table sendiri dan seluruh bootloader data tidak lagi dibutuhkan.
5. Tidak mengklaim dukungan hardware umum atau produksi.

---

## 2B. Architecture and Design Overview

Arsitektur M6 terdiri atas empat lapisan kecil.

| Lapisan | Tanggung jawab | Artefak |
|---|---|---|
| Boot memory input | Menyediakan daftar region fisik: base, length, type | `struct boot_mem_region` |
| PMM core | Menandai frame used/free/reserved dan melayani alokasi frame | `pmm.c`, `pmm.h` |
| Host test | Menguji invariants PMM tanpa boot QEMU | `tests/test_pmm_host.c` |
| Kernel integration | Memanggil PMM setelah serial/panic siap dan sebelum allocator lain | perubahan `kernel.c` atau adapter memory map |

Alur data utamanya adalah:

```text
Bootloader memory map
        |
        v
boot_mem_region[] normalisasi tipe dan range
        |
        v
pmm_init_from_map()
        |
        +--> semua frame awalnya used
        +--> region usable dibuka menjadi free
        +--> frame 0 dibuat used
        +--> region non-usable dipaksa used
        |
        v
pmm_alloc_frame() / pmm_free_frame() / pmm_reserve_range()
```

---

## 2C. Interfaces, ABI, dan API Boundary

Boundary utama M6 adalah API C internal kernel, bukan syscall. Semua fungsi PMM berjalan pada privilege kernel. API tidak boleh dipanggil dari interrupt handler sampai aturan locking SMP dan interrupt-context allocator dibuat pada milestone berikutnya.

| API | Fungsi |
|---|---|
| `pmm_zero_state()` | Menginisialisasi struktur state ke nol agar error path aman |
| `pmm_init_from_map()` | Membuat bitmap berdasarkan boot memory map |
| `pmm_alloc_frame()` | Mengambil satu frame fisik bebas dan menandainya used |
| `pmm_free_frame()` | Mengembalikan satu frame aligned yang sebelumnya allocated |
| `pmm_reserve_range()` | Menandai range tertentu sebagai tidak boleh dialokasikan |
| `pmm_is_frame_free()` | Query status frame untuk uji dan debug |
| `pmm_free_count()` | Statistik jumlah frame bebas |
| `pmm_used_count()` | Statistik jumlah frame used |
| `pmm_frame_count()` | Statistik jumlah frame yang dikelola |

Kontrak ABI C M6 sederhana: semua parameter pointer harus valid, semua alamat fisik harus direpresentasikan sebagai `uint64_t`, semua ukuran range harus bebas overflow, dan semua frame yang dilepas harus aligned 4096 byte. Nilai gagal untuk allocation adalah `PMM_INVALID_FRAME`.

---

## 2D. Security and Threat Model Ringkas

Threat model M6 meliputi firmware atau bootloader memberikan region tidak rapi, region non-usable overlap dengan region usable, alamat fisik nol dialokasikan secara tidak sengaja, double free, free terhadap alamat non-aligned, overflow pada `base + length`, dan penggunaan PMM dari interrupt context tanpa locking. Mitigasi minimum adalah fail-closed initialization, validasi alignment, validasi overflow, frame 0 selalu reserved, non-usable menimpa usable, dan PMM belum dipakai dari IRQ/scheduler sampai lock order didefinisikan.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M6, mahasiswa harus mampu:

1. Menjelaskan perbedaan memory map firmware/bootloader, physical memory manager, virtual memory manager, dan heap allocator.
2. Menjelaskan alasan PMM harus menganggap semua frame used sebelum membuka region usable.
3. Mengimplementasikan bitmap allocator untuk frame fisik 4096 byte.
4. Melakukan alignment `base` dan `length` agar frame partial tidak dialokasikan.
5. Menangani overflow `base + length` secara eksplisit.
6. Menghindari alokasi frame 0.
7. Menulis host unit test untuk logika kernel yang tidak membutuhkan hardware.
8. Menghasilkan bukti `make check`, `nm -u`, `objdump`, dan log QEMU/serial setelah integrasi.
9. Menjelaskan residual risk M6: belum ada VMM penuh, belum ada heap, belum ada reclamation aman untuk bootloader memory.

---

## 4. Prasyarat Teori

| Konsep | Makna Praktis di M6 | Bukti Minimal |
|---|---|---|
| Boot memory map | Sumber kebenaran awal untuk region fisik | Tabel region atau log serial ringkasan memory map |
| Physical frame | Unit alokasi fisik 4096 byte | Alamat hasil `pmm_alloc_frame()` selalu aligned |
| Bitmap | Satu bit mewakili status satu frame | `PMM_BITMAP_BYTES = PMM_MAX_FRAMES / 8` |
| Reserved memory | Region yang tidak boleh dipakai kernel umum | Kernel/modules/framebuffer/ACPI/bad memory tidak dialokasikan |
| Fail-closed | Default status frame adalah used, bukan free | Jika region tidak dikenal, allocator tidak menggunakannya |
| Overflow check | `base + length` tidak boleh wraparound | Fungsi `checked_add_u64()` ada dan diuji |
| Host test | Unit test logika murni tanpa QEMU | `build/test_pmm_host` PASS |
| Freestanding audit | Object kernel tidak bergantung libc | `nm -u build/pmm.o` kosong |

Limine memory map request menyatakan entries tersusun menurut base address, region usable dan bootloader-reclaimable aligned 4096 byte, dan area 0x0–0x1000 tidak ditandai usable pada dokumentasi Rust crate Limine terbaru yang merefleksikan protokol [1]. QEMU tetap dipakai sebagai emulator sistem penuh untuk menjalankan smoke test dan GDB stub dapat dipakai untuk menghentikan guest, memeriksa register, memori, breakpoint, dan watchpoint [4].

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M6 |
|---|---|
| `osdev-general` | Readiness gate, urutan milestone, integrasi M0–M6 |
| `osdev-01-computer-foundation` | State model PMM, invariants, test obligation |
| `osdev-02-low-level-programming` | Freestanding C, integer overflow, pointer/bitmap ownership, object audit |
| `osdev-03-computer-and-hardware-architecture` | x86_64 memory management, frame size, hardware/firmware memory assumptions |
| `osdev-04-kernel-development` | Kernel allocator boundary, panic path, observability |
| `osdev-07-os-security` | Fail-closed allocation, reserved memory protection, invalid free detection |
| `osdev-10-boot-firmware` | Bootloader memory map, Limine handoff, kernel entry assumptions |
| `osdev-12-toolchain-devenv` | Build freestanding, audit `nm`, `objdump`, QEMU/GDB workflow |
| `osdev-14-cross-science` | Verification matrix, risk register, failure mode, evidence-based readiness |

---

## 6. Alat dan Versi yang Disarankan

Jalankan perintah berikut di WSL 2. Catat output aktual pada laporan.

```bash
uname -a
cat /etc/os-release
clang --version || true
ld.lld --version || true
gcc --version || true
ld --version | head -n 1 || true
make --version | head -n 1
qemu-system-x86_64 --version || true
gdb --version | head -n 1 || true
readelf --version | head -n 1
objdump --version | head -n 1
nm --version | head -n 1
```

Clang memiliki mode C freestanding dan opsi yang perlu diaudit agar kernel tidak mengandalkan runtime hosted [5]. LLD mendukung target ELF dan banyak opsi kompatibel GNU linker, sedangkan GNU ld linker script menyediakan kontrol section dan entry symbol yang sudah dipakai sejak M2 [6], [7].

---

## 7. Struktur Repository yang Diharapkan

Struktur berikut kompatibel dengan M6. Jika repository M5 sudah memakai nama berbeda, lakukan mapping nama dengan konsisten dan dokumentasikan pada laporan.

```text
mcsos/
├── Makefile
├── linker.ld
├── include/
│   ├── idt.h
│   ├── io.h
│   ├── panic.h
│   ├── pic.h
│   ├── pit.h
│   ├── pmm.h
│   ├── serial.h
│   └── types.h
├── src/
│   ├── boot.S
│   ├── idt.c
│   ├── interrupts.S
│   ├── kernel.c
│   ├── panic.c
│   ├── pic.c
│   ├── pit.c
│   ├── pmm.c
│   └── serial.c
├── tests/
│   └── test_pmm_host.c
├── scripts/
│   ├── check_m5_static.sh
│   └── check_m6_static.sh
└── build/
    └── ... hasil build, map, symbol, disassembly, log
```

---

## 8. Pemeriksaan Kesiapan M0–M5 Sebelum M6

Bagian ini wajib dijalankan sebelum menulis source M6. Tujuannya bukan mengulang seluruh praktikum, tetapi memastikan fondasi yang dipakai M6 tidak rusak.

### 8.1 Pemeriksaan M0: governance dan repository hygiene

Jalankan dari root repository.

```bash
git status --short
git branch --show-current
find . -maxdepth 2 -type f | sort | sed -n '1,120p'
```

Indikator lulus: working tree bersih atau perubahan M6 belum dikomit tetapi jelas; branch bukan `main` jika aturan kelas melarang langsung ke main; file dokumentasi M0 tersedia.

Jika `git status` menunjukkan banyak perubahan tidak terkait, buat commit pemisah atau stash sebelum M6:

```bash
git add docs/ README.md Makefile include src scripts tests || true
git commit -m "m5: stabilize interrupt and timer baseline" || true
git switch -c m6-pmm
```

### 8.2 Pemeriksaan M1: toolchain dan reproducibility

Perintah ini memastikan compiler, linker, dan binutils tersedia.

```bash
command -v clang || command -v gcc
command -v ld.lld || command -v ld
command -v make
command -v readelf
command -v objdump
command -v nm
```

Jika salah satu tidak ditemukan, perbaiki paket WSL:

```bash
sudo apt update
sudo apt install -y build-essential clang lld llvm binutils make gdb qemu-system-x86 xorriso mtools
```

### 8.3 Pemeriksaan M2: boot artifact masih dapat dibuat

M6 memerlukan pipeline build M2 yang stabil. Jalankan target build yang ada di repository Anda.

```bash
make clean
make all
find build -maxdepth 2 -type f | sort
```

Indikator lulus: kernel ELF atau ISO/disk image terbentuk; tidak ada warning kritis; linker map tersedia jika M2/M3 sudah mewajibkannya.

### 8.4 Pemeriksaan M3: serial log dan panic path

PMM debug bergantung pada serial log. Jalankan smoke test normal dan varian panic jika tersedia.

```bash
make run-qemu-smoke 2>&1 | tee build/m6_precheck_qemu.log || true
grep -E "MCSOS|panic|serial|kernel" build/m6_precheck_qemu.log || true
```

Jika log serial kosong, jangan lanjut ke M6. Periksa kembali `-serial stdio`, inisialisasi COM1, dan jalur `serial_write()` dari M3.

### 8.5 Pemeriksaan M4: IDT dan exception path

PMM bug sering muncul sebagai page fault, general protection fault, atau triple fault. Pastikan exception path masih ada.

```bash
nm -n build/*.elf 2>/dev/null | grep -E "idt|trap|isr|panic" || true
objdump -dr build/*.elf 2>/dev/null | grep -E "lidt|iretq" || true
```

Jika `lidt` atau `iretq` hilang pada kernel M4/M5, perbaiki M4 sebelum M6.

### 8.6 Pemeriksaan M5: timer dan external interrupt tidak rusak

PMM tidak bergantung langsung pada PIT, tetapi M5 memberi bukti bahwa interrupt gate dan return path stabil.

```bash
nm -n build/*.elf 2>/dev/null | grep -E "pic_|pit_|timer_|irq" || true
objdump -dr build/*.elf 2>/dev/null | grep -E "outb|sti|hlt" || true
```

Jika M5 menyebabkan interrupt storm atau QEMU hang, jalankan M6 dengan interrupt tetap disabled sampai PMM init selesai, lalu aktifkan kembali timer setelah PMM log keluar.

---

## 9. Failure Modes dari Praktikum Sebelumnya dan Solusi Perbaikan

| Gejala | Kemungkinan sebab | Solusi konservatif sebelum M6 |
|---|---|---|
| QEMU boot hang sebelum log pertama | Serial belum init, kernel entry salah, linker script rusak | Jalankan kembali M2/M3 smoke test; audit entry symbol dan `-serial stdio` |
| Triple fault setelah tambah source baru | Stack rusak, IDT rusak, section layout berubah | Audit `objdump`, `readelf -S`, dan pastikan M6 tidak mengubah stub interrupt |
| `nm -u` berisi `memset`, `memcpy`, `printf` | Compiler menghasilkan builtin atau test code masuk target kernel | Tambahkan `-ffreestanding -fno-builtin`; pisahkan host test dari kernel object |
| Log timer M5 berhenti setelah satu tick | EOI hilang atau PIC mask salah | Selesaikan M5 sebelum M6; PMM tidak memperbaiki interrupt path |
| Page fault ketika menyentuh memory map | Pointer memory map tidak valid, salah mapping virtual/physical | Untuk M6 awal, salin entry memory map ke `boot_mem_region` hanya melalui pointer yang valid dari bootloader |
| Free count tidak masuk akal | Range tidak aligned, overflow, atau region non-usable dianggap usable | Terapkan alignment dan fail-closed marking seperti kode M6 |
| `pmm_alloc_frame()` mengembalikan 0 | Frame 0 tidak direserve | Pastikan `mark_range_used(0, 4096)` dijalankan setelah membuka region usable |
| Double free tidak terdeteksi | `pmm_free_frame()` tidak mengecek bit | Gunakan implementasi M6 yang menolak free jika frame sudah free |

---

## 10. Kontrak Formal PMM M6

### 10.1 State variables

| Variabel | Makna |
|---|---|
| `bitmap[i]` | Bit status frame ke-i; 1 = used/reserved/allocated, 0 = free |
| `frame_count` | Jumlah frame yang dikelola dalam rentang `0..max_phys` |
| `free_frames` | Jumlah bit 0 dalam bitmap untuk frame valid |
| `used_frames` | Jumlah bit 1 dalam bitmap untuk frame valid |
| `reserved_frames` | Frame yang dipaksa used karena non-usable atau reserve manual |
| `next_hint` | Indeks awal pencarian allocation berikutnya |

### 10.2 Invariants

1. `free_frames + used_frames == frame_count` setelah inisialisasi sukses.
2. `bitmap == NULL` hanya valid sebelum `initialized == true`.
3. Frame 0 selalu used.
4. Alamat hasil `pmm_alloc_frame()` selalu aligned 4096 byte.
5. `pmm_alloc_frame()` tidak boleh mengembalikan frame dari region non-usable.
6. `pmm_free_frame()` menolak alamat non-aligned, alamat 0, alamat di luar `max_phys`, dan double free.
7. Range non-usable dapat overlap dengan usable; hasil akhir tetap non-usable karena non-usable diproses setelah usable.
8. Overflow `base + length` membatalkan operasi range.

### 10.3 Progress property

Jika `free_frames > 0`, maka `pmm_alloc_frame()` harus menemukan satu frame free dalam waktu terbatas `O(frame_count)` dan mengubahnya menjadi used. Jika `free_frames == 0`, fungsi mengembalikan `PMM_INVALID_FRAME`.

### 10.4 Concurrency rule

M6 hanya valid untuk single-core early kernel. Jangan memanggil `pmm_alloc_frame()` dan `pmm_free_frame()` dari interrupt handler atau core lain. Pada milestone SMP, PMM harus dilindungi spinlock atau diganti dengan per-CPU page cache.

---

## 11. Instruksi Implementasi Langkah demi Langkah

### 11.1 Buat branch praktikum M6

Perintah ini memisahkan perubahan M6 dari baseline M5.

```bash
git switch -c m6-pmm
mkdir -p include src tests scripts build
```

### 11.2 Tulis `include/types.h`

File ini menyediakan tipe dasar untuk kernel freestanding. Jika repository Anda sudah memiliki `types.h`, lakukan merge hati-hati dan jangan menggandakan definisi yang sudah ada.

```bash
cat > include/types.h <<'EOF'
#ifndef MCSOS_TYPES_H
#define MCSOS_TYPES_H

typedef __SIZE_TYPE__ size_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef int bool;

#define true 1
#define false 0
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif
EOF
```

### 11.3 Tulis `include/pmm.h`

Header ini adalah kontrak API PMM. Simpan di `include/pmm.h`.

```bash
cat > include/pmm.h <<'EOF'
#ifndef MCSOS_PMM_H
#define MCSOS_PMM_H

#include "types.h"

#define PMM_PAGE_SIZE 4096ULL
#define PMM_MAX_PHYS_BYTES (64ULL * 1024ULL * 1024ULL * 1024ULL)
#define PMM_MAX_FRAMES (PMM_MAX_PHYS_BYTES / PMM_PAGE_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8ULL)
#define PMM_INVALID_FRAME 0xffffffffffffffffULL

enum boot_mem_type {
    BOOT_MEM_USABLE = 1,
    BOOT_MEM_RESERVED = 2,
    BOOT_MEM_BOOTLOADER_RECLAIMABLE = 3,
    BOOT_MEM_KERNEL_AND_MODULES = 4,
    BOOT_MEM_FRAMEBUFFER = 5,
    BOOT_MEM_ACPI_RECLAIMABLE = 6,
    BOOT_MEM_ACPI_NVS = 7,
    BOOT_MEM_BAD_MEMORY = 8
};

struct boot_mem_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

struct pmm_state {
    uint8_t *bitmap;
    uint64_t bitmap_bytes;
    uint64_t max_phys;
    uint64_t frame_count;
    uint64_t free_frames;
    uint64_t used_frames;
    uint64_t reserved_frames;
    uint64_t ignored_frames;
    uint64_t next_hint;
    bool initialized;
};

void pmm_zero_state(struct pmm_state *pmm);
bool pmm_init_from_map(struct pmm_state *pmm,
                       const struct boot_mem_region *regions,
                       size_t region_count,
                       uint8_t *bitmap_storage,
                       uint64_t bitmap_storage_bytes,
                       uint64_t max_phys_bytes);
uint64_t pmm_alloc_frame(struct pmm_state *pmm);
bool pmm_free_frame(struct pmm_state *pmm, uint64_t phys_addr);
bool pmm_reserve_range(struct pmm_state *pmm, uint64_t base, uint64_t length);
bool pmm_is_frame_free(const struct pmm_state *pmm, uint64_t phys_addr);
uint64_t pmm_free_count(const struct pmm_state *pmm);
uint64_t pmm_used_count(const struct pmm_state *pmm);
uint64_t pmm_frame_count(const struct pmm_state *pmm);

#endif
EOF
```

### 11.4 Tulis `src/pmm.c`

Implementasi berikut adalah PMM core. Kode ini sengaja tidak memanggil `printf`, `malloc`, `memset`, atau libc lain agar dapat dipakai di kernel freestanding.

```bash
cat > src/pmm.c <<'EOF'
#include "pmm.h"

#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffULL
#endif

static uint64_t align_down(uint64_t value, uint64_t align) {
    return value & ~(align - 1ULL);
}

static uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1ULL) & ~(align - 1ULL);
}

static bool checked_add_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (UINT64_MAX - a < b) {
        return false;
    }
    *out = a + b;
    return true;
}

static void bitmap_set(uint8_t *bitmap, uint64_t index) {
    bitmap[index >> 3] = (uint8_t)(bitmap[index >> 3] | (uint8_t)(1U << (index & 7U)));
}

static void bitmap_clear(uint8_t *bitmap, uint64_t index) {
    bitmap[index >> 3] = (uint8_t)(bitmap[index >> 3] & (uint8_t)~(uint8_t)(1U << (index & 7U)));
}

static bool bitmap_test(const uint8_t *bitmap, uint64_t index) {
    return (bitmap[index >> 3] & (uint8_t)(1U << (index & 7U))) != 0;
}

static void mark_frame_free(struct pmm_state *pmm, uint64_t frame) {
    if (frame >= pmm->frame_count) {
        return;
    }
    if (bitmap_test(pmm->bitmap, frame)) {
        bitmap_clear(pmm->bitmap, frame);
        pmm->free_frames++;
        if (pmm->used_frames > 0) {
            pmm->used_frames--;
        }
        if (frame < pmm->next_hint) {
            pmm->next_hint = frame;
        }
    }
}

static void mark_frame_used(struct pmm_state *pmm, uint64_t frame) {
    if (frame >= pmm->frame_count) {
        return;
    }
    if (!bitmap_test(pmm->bitmap, frame)) {
        bitmap_set(pmm->bitmap, frame);
        if (pmm->free_frames > 0) {
            pmm->free_frames--;
        }
        pmm->used_frames++;
    }
}

static void mark_range_free(struct pmm_state *pmm, uint64_t base, uint64_t length) {
    uint64_t end;
    if (length == 0 || !checked_add_u64(base, length, &end)) {
        return;
    }
    uint64_t start = align_up(base, PMM_PAGE_SIZE);
    uint64_t stop = align_down(end, PMM_PAGE_SIZE);
    if (stop <= start) {
        return;
    }
    if (start >= pmm->max_phys) {
        return;
    }
    if (stop > pmm->max_phys) {
        stop = pmm->max_phys;
        pmm->ignored_frames += (align_down(end, PMM_PAGE_SIZE) - stop) / PMM_PAGE_SIZE;
    }
    for (uint64_t addr = start; addr < stop; addr += PMM_PAGE_SIZE) {
        mark_frame_free(pmm, addr / PMM_PAGE_SIZE);
    }
}

static void mark_range_used(struct pmm_state *pmm, uint64_t base, uint64_t length) {
    uint64_t end;
    if (length == 0 || !checked_add_u64(base, length, &end)) {
        return;
    }
    uint64_t start = align_down(base, PMM_PAGE_SIZE);
    uint64_t stop = align_up(end, PMM_PAGE_SIZE);
    if (stop <= start) {
        return;
    }
    if (start >= pmm->max_phys) {
        return;
    }
    if (stop > pmm->max_phys) {
        stop = pmm->max_phys;
    }
    for (uint64_t addr = start; addr < stop; addr += PMM_PAGE_SIZE) {
        bool was_free = !bitmap_test(pmm->bitmap, addr / PMM_PAGE_SIZE);
        mark_frame_used(pmm, addr / PMM_PAGE_SIZE);
        if (was_free) {
            pmm->reserved_frames++;
        }
    }
}

void pmm_zero_state(struct pmm_state *pmm) {
    if (pmm == NULL) {
        return;
    }
    pmm->bitmap = NULL;
    pmm->bitmap_bytes = 0;
    pmm->max_phys = 0;
    pmm->frame_count = 0;
    pmm->free_frames = 0;
    pmm->used_frames = 0;
    pmm->reserved_frames = 0;
    pmm->ignored_frames = 0;
    pmm->next_hint = 0;
    pmm->initialized = false;
}

bool pmm_init_from_map(struct pmm_state *pmm,
                       const struct boot_mem_region *regions,
                       size_t region_count,
                       uint8_t *bitmap_storage,
                       uint64_t bitmap_storage_bytes,
                       uint64_t max_phys_bytes) {
    if (pmm == NULL || regions == NULL || bitmap_storage == NULL || region_count == 0) {
        return false;
    }
    if (max_phys_bytes == 0 || (max_phys_bytes & (PMM_PAGE_SIZE - 1ULL)) != 0) {
        return false;
    }
    uint64_t frame_count = max_phys_bytes / PMM_PAGE_SIZE;
    uint64_t required_bitmap_bytes = (frame_count + 7ULL) / 8ULL;
    if (bitmap_storage_bytes < required_bitmap_bytes) {
        return false;
    }

    pmm_zero_state(pmm);
    pmm->bitmap = bitmap_storage;
    pmm->bitmap_bytes = required_bitmap_bytes;
    pmm->max_phys = max_phys_bytes;
    pmm->frame_count = frame_count;
    pmm->free_frames = 0;
    pmm->used_frames = frame_count;
    pmm->next_hint = 0;

    for (uint64_t i = 0; i < required_bitmap_bytes; i++) {
        bitmap_storage[i] = 0xffU;
    }

    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].type == BOOT_MEM_USABLE) {
            mark_range_free(pmm, regions[i].base, regions[i].length);
        }
    }

    /* Frame 0 is never allocated in MCSOS. It catches null-like physical addresses. */
    mark_range_used(pmm, 0, PMM_PAGE_SIZE);

    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].type != BOOT_MEM_USABLE) {
            mark_range_used(pmm, regions[i].base, regions[i].length);
        }
    }

    pmm->initialized = true;
    return true;
}

uint64_t pmm_alloc_frame(struct pmm_state *pmm) {
    if (pmm == NULL || !pmm->initialized || pmm->free_frames == 0) {
        return PMM_INVALID_FRAME;
    }
    for (uint64_t frame = pmm->next_hint; frame < pmm->frame_count; frame++) {
        if (!bitmap_test(pmm->bitmap, frame)) {
            mark_frame_used(pmm, frame);
            pmm->next_hint = frame + 1ULL;
            return frame * PMM_PAGE_SIZE;
        }
    }
    for (uint64_t frame = 0; frame < pmm->next_hint; frame++) {
        if (!bitmap_test(pmm->bitmap, frame)) {
            mark_frame_used(pmm, frame);
            pmm->next_hint = frame + 1ULL;
            return frame * PMM_PAGE_SIZE;
        }
    }
    return PMM_INVALID_FRAME;
}

bool pmm_free_frame(struct pmm_state *pmm, uint64_t phys_addr) {
    if (pmm == NULL || !pmm->initialized) {
        return false;
    }
    if ((phys_addr & (PMM_PAGE_SIZE - 1ULL)) != 0 || phys_addr == 0 || phys_addr >= pmm->max_phys) {
        return false;
    }
    uint64_t frame = phys_addr / PMM_PAGE_SIZE;
    if (!bitmap_test(pmm->bitmap, frame)) {
        return false;
    }
    mark_frame_free(pmm, frame);
    return true;
}

bool pmm_reserve_range(struct pmm_state *pmm, uint64_t base, uint64_t length) {
    if (pmm == NULL || !pmm->initialized || length == 0) {
        return false;
    }
    mark_range_used(pmm, base, length);
    return true;
}

bool pmm_is_frame_free(const struct pmm_state *pmm, uint64_t phys_addr) {
    if (pmm == NULL || !pmm->initialized) {
        return false;
    }
    if ((phys_addr & (PMM_PAGE_SIZE - 1ULL)) != 0 || phys_addr >= pmm->max_phys) {
        return false;
    }
    return !bitmap_test(pmm->bitmap, phys_addr / PMM_PAGE_SIZE);
}

uint64_t pmm_free_count(const struct pmm_state *pmm) {
    return (pmm != NULL) ? pmm->free_frames : 0ULL;
}

uint64_t pmm_used_count(const struct pmm_state *pmm) {
    return (pmm != NULL) ? pmm->used_frames : 0ULL;
}

uint64_t pmm_frame_count(const struct pmm_state *pmm) {
    return (pmm != NULL) ? pmm->frame_count : 0ULL;
}
EOF
```

### 11.5 Tulis host unit test `tests/test_pmm_host.c`

Test ini berjalan sebagai program host biasa. Tujuannya menguji logika PMM sebelum diintegrasikan ke kernel QEMU.

```bash
cat > tests/test_pmm_host.c <<'EOF'
#include <assert.h>
#include <stdio.h>
#include "pmm.h"

static uint8_t bitmap[PMM_BITMAP_BYTES];

int main(void) {
    struct boot_mem_region regions[] = {
        { .base = 0x00000000ULL, .length = 0x0009f000ULL, .type = BOOT_MEM_USABLE },
        { .base = 0x0009f000ULL, .length = 0x00001000ULL, .type = BOOT_MEM_RESERVED },
        { .base = 0x00100000ULL, .length = 0x00300000ULL, .type = BOOT_MEM_USABLE },
        { .base = 0x00400000ULL, .length = 0x00100000ULL, .type = BOOT_MEM_KERNEL_AND_MODULES },
        { .base = 0x00500000ULL, .length = 0x00400000ULL, .type = BOOT_MEM_USABLE },
    };

    struct pmm_state pmm;
    assert(pmm_init_from_map(&pmm, regions, sizeof(regions) / sizeof(regions[0]),
                             bitmap, sizeof(bitmap), 64ULL * 1024ULL * 1024ULL));
    assert(pmm_frame_count(&pmm) == (64ULL * 1024ULL * 1024ULL) / PMM_PAGE_SIZE);
    assert(!pmm_is_frame_free(&pmm, 0));
    assert(pmm_is_frame_free(&pmm, 0x00100000ULL));
    assert(!pmm_is_frame_free(&pmm, 0x00400000ULL));

    uint64_t before = pmm_free_count(&pmm);
    uint64_t frame = pmm_alloc_frame(&pmm);
    assert(frame != PMM_INVALID_FRAME);
    assert((frame & (PMM_PAGE_SIZE - 1ULL)) == 0);
    assert(!pmm_is_frame_free(&pmm, frame));
    assert(pmm_free_count(&pmm) == before - 1ULL);
    assert(pmm_free_frame(&pmm, frame));
    assert(pmm_free_count(&pmm) == before);
    assert(!pmm_free_frame(&pmm, frame));

    assert(pmm_reserve_range(&pmm, 0x00500000ULL, 0x2000ULL));
    assert(!pmm_is_frame_free(&pmm, 0x00500000ULL));
    assert(!pmm_is_frame_free(&pmm, 0x00501000ULL));

    puts("M6 PMM host unit test: PASS");
    return 0;
}
EOF
```

### 11.6 Tambahkan atau merge Makefile M6

Jika repository Anda sudah memiliki Makefile M5, jangan mengganti seluruhnya tanpa review. Tambahkan target M6 berikut atau gunakan sebagai Makefile minimal di direktori uji terpisah.

```bash
cat > Makefile.m6.example <<'EOF'
CC ?= clang
HOSTCC ?= cc
CFLAGS := -std=c17 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -Iinclude
HOST_CFLAGS := -std=c17 -Wall -Wextra -Werror -Iinclude

all: build/pmm.o build/test_pmm_host

build/pmm.o: src/pmm.c include/pmm.h include/types.h
	mkdir -p build
	$(CC) $(CFLAGS) -c src/pmm.c -o build/pmm.o

build/test_pmm_host: src/pmm.c tests/test_pmm_host.c include/pmm.h include/types.h
	mkdir -p build
	$(HOSTCC) $(HOST_CFLAGS) src/pmm.c tests/test_pmm_host.c -o build/test_pmm_host

check: all
	./build/test_pmm_host
	nm -u build/pmm.o
	objdump -dr build/pmm.o > build/pmm.objdump.txt

clean:
	rm -rf build
EOF
```

Untuk repository kelas, integrasikan target berikut ke Makefile utama:

```makefile
M6_CFLAGS := -std=c17 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -Iinclude

build/pmm.o: src/pmm.c include/pmm.h include/types.h
	mkdir -p build
	$(CC) $(M6_CFLAGS) -c src/pmm.c -o build/pmm.o

build/test_pmm_host: src/pmm.c tests/test_pmm_host.c include/pmm.h include/types.h
	mkdir -p build
	$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Iinclude src/pmm.c tests/test_pmm_host.c -o build/test_pmm_host

check-m6: build/pmm.o build/test_pmm_host
	./build/test_pmm_host
	nm -u build/pmm.o | tee build/pmm.undefined.txt
	test ! -s build/pmm.undefined.txt
	objdump -dr build/pmm.o > build/pmm.objdump.txt
```

### 11.7 Tulis script audit `scripts/check_m6_static.sh`

Script ini membuat pemeriksaan statis dapat diulang oleh mahasiswa dan dosen.

```bash
cat > scripts/check_m6_static.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

mkdir -p build
: "${CC:=clang}"
: "${HOSTCC:=clang}"

${CC} -std=c17 -Wall -Wextra -Werror \
  -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone \
  -Iinclude -c src/pmm.c -o build/pmm.o

${HOSTCC} -std=c17 -Wall -Wextra -Werror \
  -Iinclude src/pmm.c tests/test_pmm_host.c -o build/test_pmm_host

./build/test_pmm_host
nm -u build/pmm.o | tee build/pmm.undefined.txt
objdump -dr build/pmm.o > build/pmm.objdump.txt

if grep -q . build/pmm.undefined.txt; then
  echo "[FAIL] pmm.o masih memiliki unresolved symbol" >&2
  exit 1
fi

echo "[PASS] M6 static check selesai"
EOF
chmod +x scripts/check_m6_static.sh
```

### 11.8 Jalankan build dan unit test lokal

Jalankan dari root repository.

```bash
make -f Makefile.m6.example clean
make -f Makefile.m6.example CC=clang HOSTCC=clang check
```

Jika Anda sudah mengintegrasikan target ke Makefile utama, jalankan:

```bash
./scripts/check_m6_static.sh
```

Indikator lulus:

```text
M6 PMM host unit test: PASS
[PASS] M6 static check selesai
```

### 11.9 Audit object freestanding

Perintah ini memastikan `pmm.o` tidak membawa dependency host.

```bash
nm -u build/pmm.o
objdump -dr build/pmm.o | sed -n '1,160p'
```

Indikator lulus: `nm -u build/pmm.o` kosong. Jika ada `memset`, `memcpy`, `printf`, `malloc`, atau `__stack_chk_fail`, periksa flags compiler dan source yang ikut target kernel.

### 11.10 Integrasi ke kernel MCSOS

Integrasi kernel tergantung boot protocol Anda. Konsep umumnya adalah menyiapkan array `struct boot_mem_region` dari memory map bootloader, lalu memanggil `pmm_init_from_map()` setelah serial dan panic siap tetapi sebelum kernel memerlukan frame allocation.

Skeleton integrasi konseptual:

```c
#include "pmm.h"
#include "serial.h"
#include "panic.h"

static struct pmm_state kernel_pmm;
static uint8_t kernel_pmm_bitmap[PMM_BITMAP_BYTES] __attribute__((aligned(4096)));

static void kernel_memory_init(const struct boot_mem_region *regions, size_t region_count) {
    bool ok = pmm_init_from_map(&kernel_pmm,
                                regions,
                                region_count,
                                kernel_pmm_bitmap,
                                sizeof(kernel_pmm_bitmap),
                                PMM_MAX_PHYS_BYTES);
    if (!ok) {
        panic("pmm_init_from_map failed");
    }

    serial_write("[m6] pmm initialized
");
    serial_write_u64_hex(pmm_frame_count(&kernel_pmm));
    serial_write(" frames managed
");
    serial_write_u64_hex(pmm_free_count(&kernel_pmm));
    serial_write(" frames free
");

    uint64_t f = pmm_alloc_frame(&kernel_pmm);
    if (f == PMM_INVALID_FRAME) {
        panic("pmm_alloc_frame returned invalid");
    }
    serial_write("[m6] sample frame = ");
    serial_write_u64_hex(f);
    serial_write("
");

    if (!pmm_free_frame(&kernel_pmm, f)) {
        panic("pmm_free_frame failed");
    }
}
```

Jika Anda memakai Limine, buat adapter yang mengubah tipe Limine menjadi `BOOT_MEM_*`. Untuk M6 awal, perlakukan hanya `LIMINE_MEMMAP_USABLE` sebagai free. `BOOTLOADER_RECLAIMABLE` jangan dibuka otomatis sampai kernel selesai memakai semua struktur bootloader dan siap mengganti page table bootloader. Dokumentasi Limine menyatakan bootloader-reclaimable adalah RAM yang digunakan untuk data bootloader atau firmware dan baru aman direklamasi setelah executable memastikan data tersebut tidak lagi dibutuhkan [1].

---

## 12. Checkpoint Buildable

| Checkpoint | Perintah | Bukti wajib |
|---|---|---|
| CP1: Source PMM ada | `test -f include/pmm.h && test -f src/pmm.c` | Struktur repository |
| CP2: Compile freestanding | `clang ... -c src/pmm.c` | `build/pmm.o` |
| CP3: Host unit test | `./build/test_pmm_host` | Output PASS |
| CP4: Unresolved symbol audit | `nm -u build/pmm.o` | Output kosong |
| CP5: Disassembly tersedia | `objdump -dr build/pmm.o` | `build/pmm.objdump.txt` |
| CP6: Kernel integration | `make all` | kernel ELF/ISO terbentuk |
| CP7: QEMU smoke | `make run-qemu-smoke` | log `[m6] pmm initialized` |
| CP8: Git evidence | `git diff --stat && git status` | perubahan terkontrol |

---

## 13. Perintah Uji Wajib

### 13.1 Uji host PMM

```bash
./scripts/check_m6_static.sh
```

### 13.2 Uji kernel build

```bash
make clean
make all 2>&1 | tee build/m6_build.log
```

### 13.3 Audit ELF setelah integrasi

```bash
readelf -h build/*.elf 2>/dev/null | tee build/m6_readelf_header.log
readelf -S build/*.elf 2>/dev/null | tee build/m6_readelf_sections.log
nm -n build/*.elf 2>/dev/null | grep -E "pmm_|kernel_pmm|bitmap" | tee build/m6_symbols.log
objdump -dr build/*.elf 2>/dev/null | grep -E "pmm_init|pmm_alloc|pmm_free" | tee build/m6_disasm_probe.log
```

### 13.4 QEMU smoke test

Sesuaikan nama target dengan Makefile Anda. Jika target belum ada, gunakan QEMU command dari M2/M3.

```bash
make run-qemu-smoke 2>&1 | tee build/m6_qemu.log || true
grep -E "\[m6\]|pmm|panic|fault|trap" build/m6_qemu.log || true
```

Indikator lulus: log minimal memuat `[m6] pmm initialized`, jumlah frame managed/free, dan sample frame aligned.

### 13.5 GDB workflow untuk fault PMM

QEMU gdbstub mendukung opsi `-s -S` untuk membuka port 1234 dan menghentikan guest sampai GDB melanjutkan eksekusi [4].

Terminal 1:

```bash
make run-qemu-gdb
```

Terminal 2:

```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break pmm_init_from_map
(gdb) break pmm_alloc_frame
(gdb) continue
(gdb) info registers
(gdb) x/16gx &kernel_pmm
```

---

## 14. Bukti yang Harus Dikumpulkan

Mahasiswa wajib mengumpulkan bukti berikut pada laporan:

1. Output versi toolchain dan QEMU.
2. `git diff --stat` untuk perubahan M6.
3. Source `include/pmm.h` dan potongan penting `src/pmm.c`.
4. Output `./scripts/check_m6_static.sh`.
5. Output `nm -u build/pmm.o` kosong.
6. Potongan `objdump -dr build/pmm.o`.
7. Output build kernel M6.
8. Log QEMU yang menunjukkan PMM initialized.
9. Analisis apakah frame 0, kernel image, modules, framebuffer, ACPI, dan bad memory terlindungi.
10. Readiness review M6.

---

## 15. Static Verification yang Telah Dilakukan pada Source Contoh

Source inti yang dicantumkan pada panduan ini telah diperiksa di lingkungan container dengan Clang. Hasilnya:

```text
M6 PMM host unit test: PASS
[PASS] M6 static check selesai
```

Makna hasil tersebut:

1. `src/pmm.c` dapat dikompilasi sebagai object freestanding dengan `-ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone`.
2. Host unit test lulus.
3. `nm -u build/pmm.o` tidak mencetak unresolved symbol.
4. `objdump` berhasil menghasilkan disassembly.

Validasi runtime QEMU/OVMF tetap wajib dijalankan ulang di WSL 2 mahasiswa karena bergantung pada paket QEMU, OVMF, bootloader/ISO, dan konfigurasi host setempat.

---

## 16. Kriteria Lulus Praktikum

Praktikum M6 dinyatakan lulus minimum jika semua kriteria berikut terpenuhi:

1. Repository dapat dibangun dari clean checkout.
2. Source `include/pmm.h`, `src/pmm.c`, `tests/test_pmm_host.c`, dan script audit tersedia.
3. `./scripts/check_m6_static.sh` lulus.
4. `nm -u build/pmm.o` kosong.
5. Kernel MCSOS dapat dibangun setelah integrasi PMM.
6. QEMU boot atau smoke target berjalan deterministik sampai log PMM keluar.
7. Panic path tetap terbaca jika PMM sengaja dibuat gagal.
8. Tidak ada warning kritis pada compile PMM.
9. Perubahan Git dikomit dengan pesan jelas.
10. Laporan berisi screenshot/log yang cukup, analisis desain, invariants, failure modes, dan rollback.

Untuk nilai lebih tinggi, mahasiswa menambahkan dump memory map, uji edge-case overflow, uji region overlap, dan analisis mengapa bootloader-reclaimable belum direklamasi otomatis.

---

## 17. Rubrik Penilaian 100 Poin

| Komponen | Poin | Indikator |
|---|---:|---|
| Kebenaran fungsional | 30 | PMM init, alloc, free, reserve, statistik, dan unit test berjalan benar |
| Kualitas desain dan invariants | 20 | Invariants eksplisit, fail-closed, overflow/alignment ditangani, ownership jelas |
| Pengujian dan bukti | 20 | Host test, static audit, QEMU log, ELF/disassembly evidence lengkap |
| Debugging/failure analysis | 10 | Failure modes M0–M6 dianalisis dan ada prosedur diagnosis |
| Keamanan dan robustness | 10 | Reserved memory tidak dialokasikan, frame 0 protected, invalid free ditolak |
| Dokumentasi/laporan | 10 | Laporan rapi, command/log/screenshot lengkap, referensi IEEE digunakan |

---

## 18. Tugas Implementasi

### Tugas Wajib

1. Implementasikan `pmm.h` dan `pmm.c` sesuai kontrak M6.
2. Tambahkan host unit test untuk minimal tiga region memory map.
3. Pastikan object PMM tidak memiliki unresolved symbol.
4. Integrasikan PMM ke kernel setelah serial/panic siap.
5. Cetak ringkasan frame managed/free ke serial log.
6. Uji satu kali alloc/free pada kernel path dan pastikan tidak panic.

### Tugas Pengayaan

1. Tambahkan test untuk region overlap non-usable terhadap usable.
2. Tambahkan test untuk `base + length` overflow.
3. Tambahkan counter `largest_free_run` untuk debugging fragmentasi awal.
4. Tambahkan opsi build `PMM_MAX_PHYS_BYTES=128GiB` dan ukur ukuran bitmap.

### Tantangan Riset

1. Rancang dynamic bitmap placement di region usable terbesar.
2. Buat proof sketch bahwa `free_frames + used_frames == frame_count` terjaga oleh semua operasi.
3. Rancang protocol aman untuk mereklamasi bootloader-reclaimable memory setelah kernel memiliki page table sendiri.

---

## 19. Failure Modes M6 dan Triage

| Failure mode | Gejala | Diagnosis | Perbaikan |
|---|---|---|---|
| Bitmap terlalu kecil | `pmm_init_from_map` false | Cek `PMM_MAX_PHYS_BYTES` dan `PMM_BITMAP_BYTES` | Perbesar bitmap atau turunkan max phys sementara |
| Alokasi frame reserved | Kernel crash setelah write frame | Cek tipe memory map dan order marking | Pastikan non-usable diproses setelah usable |
| Double free | Free count naik terlalu banyak | Host test gagal atau statistik tidak konsisten | `pmm_free_frame` harus menolak frame yang sudah free |
| Frame 0 allocated | Sample frame `0x0` | Frame 0 tidak direserve | Pastikan `mark_range_used(0, 4096)` ada |
| Overflow range | Free count sangat besar/tidak masuk akal | `base + length` wraparound | Gunakan `checked_add_u64` |
| QEMU page fault saat PMM init | Salah membaca pointer bootloader | Break di `pmm_init_from_map`, dump memory map | Validasi adapter Limine dan jangan akses physical pointer tanpa mapping |
| Timer M5 rusak setelah M6 | PMM mengubah interrupt code | `objdump` dan `git diff` menunjukkan perubahan tak terkait | Rollback perubahan M6 yang menyentuh M5 |

---

## 20. Prosedur Rollback

Jika M6 menyebabkan kernel tidak boot, rollback harus terukur.

```bash
git status --short
git diff --stat
git restore include/pmm.h src/pmm.c tests/test_pmm_host.c scripts/check_m6_static.sh Makefile || true
make clean
make all
```

Jika perubahan sudah dikomit:

```bash
git log --oneline -5
git revert <commit-m6>
make clean
make all
```

Jika rollback berhasil dan M5 kembali stabil, dokumentasikan gejala M6, commit yang direvert, dan hipotesis penyebab.

---

## 21. Template Laporan Praktikum M6

Gunakan template ini agar format laporan konsisten.

### 21.1 Sampul

- Judul: Praktikum M6 — Physical Memory Manager, Boot Memory Map, dan Bitmap Frame Allocator pada MCSOS
- Nama mahasiswa:
- NIM:
- Kelas:
- Mode pengerjaan: Individu / Kelompok
- Anggota kelompok jika ada:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi Pendidikan Teknologi Informasi
- Institut Pendidikan Indonesia

### 21.2 Tujuan

Tuliskan capaian teknis dan konseptual praktikum M6.

### 21.3 Dasar Teori Ringkas

Jelaskan memory map, frame fisik, PMM, bitmap allocator, reserved memory, fail-closed, dan hubungan PMM dengan VMM.

### 21.4 Lingkungan

| Komponen | Versi / Nilai |
|---|---|
| Windows | |
| WSL distro | |
| Kernel WSL | |
| Compiler | |
| Linker | |
| QEMU | |
| GDB | |
| Target | x86_64 |
| Commit hash | |

### 21.5 Desain

Sertakan diagram ringkas, struktur data, invariants, alur kontrol, dan batasan.

### 21.6 Langkah Kerja

Tuliskan perintah, file yang diubah, dan alasan teknis.

### 21.7 Hasil Uji

| Uji | Perintah | Hasil | Bukti |
|---|---|---|---|
| Host PMM test | `./scripts/check_m6_static.sh` | PASS/FAIL | log |
| Freestanding audit | `nm -u build/pmm.o` | PASS/FAIL | output kosong |
| Kernel build | `make all` | PASS/FAIL | log |
| QEMU smoke | `make run-qemu-smoke` | PASS/FAIL | log/screenshot |

### 21.8 Analisis

Bahas penyebab keberhasilan, bug yang ditemukan, dan perbandingan dengan teori.

### 21.9 Keamanan dan Reliability

Analisis risiko reserved memory corruption, invalid free, double free, overflow, page fault, dan mitigasi.

### 21.10 Kesimpulan

Tuliskan apa yang berhasil, apa yang belum, dan rencana perbaikan untuk M7.

### 21.11 Lampiran

Masukkan potongan kode penting, diff ringkas, log penuh, dan referensi.

---

## 22. Readiness Review

| Area | Status yang diharapkan | Bukti |
|---|---|---|
| Build | Siap uji lokal | `make check` atau script audit PASS |
| PMM logic | Siap host unit test | `M6 PMM host unit test: PASS` |
| Freestanding object | Siap integrasi kernel | `nm -u build/pmm.o` kosong |
| Kernel integration | Siap uji QEMU jika log PMM keluar | `build/m6_qemu.log` |
| Security | Fail-closed awal | frame 0 reserved, non-usable reserved, invalid free ditolak |
| Runtime | Belum siap hardware umum | QEMU-only / selected emulator evidence |

Kesimpulan readiness M6 yang valid: **siap uji QEMU untuk PMM awal** apabila seluruh kriteria lulus terpenuhi. Jika hanya host unit test yang lulus tetapi QEMU belum dijalankan, statusnya adalah **siap integrasi lokal, belum siap uji QEMU penuh**. Jangan menyatakan “tanpa error” atau “siap produksi”.

---

## 23. Referensi

[1] `limine` Rust crate documentation, “MemoryMapRequest,” docs.rs, accessed May 2026.  
[2] Intel Corporation, *Intel® 64 and IA-32 Architectures Software Developer’s Manual*, latest public version, 2026.  
[3] Limine Bootloader Project, “Limine,” GitHub repository and bootloader documentation, 2026.  
[4] QEMU Project, “GDB usage,” *QEMU System Emulation Documentation*, accessed May 2026.  
[5] LLVM Project, “Clang command line argument reference and freestanding compilation behavior,” accessed May 2026.  
[6] LLVM Project, “LLD ELF Linker,” accessed May 2026.  
[7] GNU Project, “GNU ld Linker Scripts,” *GNU Binutils Documentation*, accessed May 2026.
