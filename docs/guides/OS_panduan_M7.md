# OS_panduan_M7.md

# Panduan Praktikum M7 — Virtual Memory Manager Awal, Page Table x86_64, dan Page Fault Diagnostics pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M7  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: *siap uji QEMU untuk Virtual Memory Manager awal*, bukan siap produksi dan bukan bukti bahwa paging kernel lengkap aman untuk hardware umum.

---

## 1. Ringkasan Praktikum

Praktikum M7 memperluas hasil M6 dari **Physical Memory Manager** menjadi **Virtual Memory Manager** awal. Pada M6 kernel sudah memiliki model frame fisik berbasis bitmap. Pada M7 frame fisik tersebut dipakai sebagai bahan untuk membangun struktur paging x86_64 tingkat awal: PML4, PDPT, Page Directory, dan Page Table. Intel mendokumentasikan bahwa pada IA-32e mode terdapat empat level paging utama: PML4, page-directory-pointer table, page directory, dan page table; CR3 menyimpan alamat fisik basis hierarchy paging [1]. AMD64 juga mendokumentasikan translation dan system programming untuk long mode dalam manual system programming AMD64 [2].

Target M7 bersifat konservatif. Mahasiswa **tidak langsung diwajibkan mengganti CR3 bootloader**. Langkah wajib M7 adalah membuat library VMM yang dapat melakukan `map`, `query`, dan `unmap` halaman 4 KiB secara benar, dapat diuji melalui host unit test, dapat dikompilasi freestanding, dan menyediakan primitive arsitektural `invlpg`, `read_cr2`, `read_cr3`, serta `write_cr3`. Aktivasi page table baru melalui `vmm_write_cr3()` hanya boleh dilakukan sebagai pengayaan setelah mapping kernel, stack, IDT/GDT, framebuffer/serial MMIO, PMM metadata, dan HHDM sudah terverifikasi. Pendekatan bertahap ini mencegah triple fault akibat page table tidak lengkap.

M7 juga memperbaiki jalur diagnosis page fault. Pada tahap M4 IDT dan exception stub sudah dibuat. Pada M7 exception vector 14 atau page fault harus menampilkan alamat fault dari CR2, error code, RIP, RSP, dan klasifikasi bit `P`, `W/R`, `U/S`, `RSVD`, `I/D` jika error code tersedia. Tujuan diagnosa ini adalah agar bug paging tidak berubah menjadi gejala samar seperti hang, reset, atau triple fault tanpa bukti.

Keberhasilan M7 tidak berarti kernel siap produksi. Kriteria minimum M7 adalah kode VMM dapat dibangun dari clean checkout, host unit test lulus, object freestanding tidak memiliki unresolved symbol, disassembly menunjukkan primitive `invlpg` dan akses CR3, serta integrasi QEMU mampu menampilkan log ringkas bahwa VMM initialized dan page-fault path dapat dibaca.

---

## 2. Asumsi Target dan Batasan

| Aspek | Keputusan M7 |
|---|---|
| Arsitektur | x86_64 long mode, 4-level paging awal |
| Lingkungan host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Boot path | Melanjutkan pipeline M2–M6; direkomendasikan Limine/UEFI atau ISO yang sudah lulus M2 |
| Bahasa | C17 freestanding + assembly/inline assembly terbatas |
| Toolchain | Clang/LLD atau GCC/binutils; contoh validasi memakai Clang dan GNU binutils tools |
| Page size wajib | 4 KiB |
| Huge page | Tidak dipakai pada tugas wajib M7 |
| Page table level | PML4 -> PDPT -> PD -> PT |
| Sumber frame page table | PMM M6 atau mock allocator untuk host test |
| Akses fisik saat edit page table | HHDM atau direct-map sementara yang disediakan bootloader |
| Aktivasi CR3 baru | Pengayaan, bukan tugas wajib, karena berisiko tinggi |
| Out of scope | User mode, copy-on-write, demand paging, swapping, NUMA, PCID, 5-level paging, kernel heap penuh, ASLR/KASLR penuh, page cache |

Limine menyediakan request memory map dan HHDM. Dokumentasi Limine menyatakan `MemoryMapRequest` memberi memory map, entry disusun menurut base address, region usable dan bootloader-reclaimable tidak overlap dan aligned 4096 byte, sedangkan jenis region lain tidak memiliki jaminan yang sama [5]. Dokumentasi `HhdmRequest` menyatakan request tersebut meminta informasi Higher Half Direct Map [6]. Karena revisi base protocol Limine dapat memengaruhi cakupan HHDM, M7 tidak boleh mengasumsikan semua alamat fisik selalu terpetakan ke HHDM tanpa memeriksa response dan revisi protokol.

---

## 2A. Goals dan Non-Goals

### Goals

1. Mengimplementasikan VMM awal berbasis page table 4-level x86_64.
2. Menggunakan frame fisik dari PMM M6 atau allocator mock untuk membangun table baru.
3. Menyediakan API `vmm_space_init`, `vmm_map_page`, `vmm_query_page`, dan `vmm_unmap_page`.
4. Menyediakan validasi alamat canonical x86_64 48-bit.
5. Menyediakan validasi alignment 4 KiB untuk virtual address dan physical address.
6. Menyediakan primitive arsitektural `invlpg`, `read_cr2`, `read_cr3`, dan `write_cr3`.
7. Menyediakan host unit test deterministik tanpa QEMU.
8. Menyediakan audit `nm -u`, `objdump`, dan optional QEMU/GDB workflow.
9. Menyediakan prosedur diagnosis page fault agar bug mapping dapat dilokalisasi.

### Non-Goals

1. Tidak mewajibkan penggantian CR3 pada tugas wajib.
2. Tidak menggunakan page table untuk isolasi user/kernel penuh.
3. Tidak membuat demand paging atau page fault recovery otomatis.
4. Tidak membuat heap umum `kmalloc`.
5. Tidak mengaktifkan NXE/SMEP/SMAP sebagai syarat lulus wajib, tetapi dokumen menjelaskan konsekuensi security-nya.
6. Tidak mengklaim page table aman pada hardware fisik tanpa uji bring-up perangkat keras.

---

## 2B. Peta Ketergantungan M0–M6

| Tahap sebelumnya | Artefak yang harus sudah ada | Pemeriksaan sebelum M7 | Solusi jika belum siap |
|---|---|---|---|
| M0 | WSL 2, Git, baseline repository, governance docs | `wsl --status`, `git status`, struktur repo lengkap | Perbaiki WSL, pindahkan repo ke filesystem Linux WSL, bukan drive Windows yang lambat untuk build intensif |
| M1 | Toolchain, `make check`, audit object | `clang --version`, `ld.lld --version`, `readelf`, `objdump`, `nm` | Reinstal paket `clang lld binutils make qemu-system-x86 gdb`; pin versi di laporan |
| M2 | Bootable image/ISO dan kernel ELF | `make iso` atau target setara menghasilkan ISO/ELF | Perbaiki linker script, Limine config, path kernel, dan entry symbol sebelum lanjut |
| M3 | Serial log, panic path, halt path | Log panic terbaca melalui `-serial stdio` | Perbaiki UART/serial dan panic agar page fault tidak diam |
| M4 | IDT, exception stub 0–31, trap dispatch | `int3` atau exception test masuk dispatcher | Perbaiki `lidt`, gate descriptor, stub error-code/no-error-code, dan `iretq` |
| M5 | PIC/PIT atau timer tick awal | IRQ0 tidak storm, EOI benar | Mask IRQ yang tidak dipakai, kirim EOI tepat, jangan `sti` sebelum IDT siap |
| M6 | PMM bitmap frame allocator | `pmm_alloc_frame`, `pmm_free_frame`, host PMM test lulus | Perbaiki overflow, alignment, frame 0 reserved, dan non-usable region fail-closed |

M7 harus mulai dari repository yang clean. Jika M6 masih salah menghitung frame bebas, jangan paksa M7. Page table yang mengambil frame dari PMM rusak akan menghasilkan bug berlapis: entry page table menunjuk frame yang berisi kernel, stack, bitmap PMM, atau data firmware.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M7, mahasiswa harus mampu:

1. Menjelaskan translasi virtual address x86_64 melalui PML4, PDPT, PD, dan PT.
2. Menjelaskan peran CR3 sebagai basis fisik page-table hierarchy.
3. Menjelaskan alasan kernel membutuhkan direct map/HHDM untuk mengedit page table yang berada di memori fisik.
4. Menjelaskan perbedaan physical frame allocation dan virtual mapping.
5. Mengimplementasikan validasi alamat canonical 48-bit.
6. Mengimplementasikan map, query, dan unmap halaman 4 KiB secara deterministik.
7. Menghindari remap diam-diam terhadap virtual address yang sudah present.
8. Menggunakan `invlpg` setelah unmap agar TLB tidak menyimpan translasi lama.
9. Menjelaskan error code page fault minimal: present/protection, write/read, user/supervisor, reserved bit, instruction fetch.
10. Menghasilkan bukti host unit test, freestanding compile, object audit, disassembly, dan log QEMU jika diintegrasikan.

---

## 4. Prasyarat Teori

| Konsep | Makna praktis pada M7 | Bukti minimal |
|---|---|---|
| Virtual address | Alamat yang dipakai CPU sebelum translasi paging | Fungsi `vmm_is_canonical()` ada dan diuji |
| Physical address | Alamat frame fisik hasil translasi | `paddr` pada PTE selalu 4 KiB aligned |
| Page table | Struktur 512 entry per level | Test membuat intermediate table otomatis |
| CR3 | Register basis PML4 fisik | Disassembly memuat akses CR3 |
| TLB | Cache translasi virtual-physical | Unmap memanggil `invlpg` |
| PTE flags | Present, writable, user, global, NX, cache bits | `vmm_query_page()` mengembalikan flags |
| Page fault | Exception ketika translasi gagal atau proteksi dilanggar | Handler vector 14 membaca CR2 |
| HHDM/direct map | Cara kernel menulis memori fisik page table | Adapter `phys_to_virt` eksplisit |

Intel SDM menjelaskan bahwa paging memakai struktur di memori fisik dan entry menentukan base page frame serta access rights [1]. QEMU gdbstub dapat digunakan untuk remote debugging, termasuk memeriksa register dan memory, dengan opsi `-s -S` atau `-gdb` [3]. GNU ld menyatakan `SECTIONS` pada linker script menentukan layout output, sedangkan `MEMORY` dapat menggambarkan memori target [7]. LLD mengimplementasikan subset besar notasi linker script GNU ld dan mendokumentasikan kebijakan kompatibilitasnya [8]. Clang menyediakan opsi `-ffreestanding` untuk menyatakan kompilasi di lingkungan freestanding [9].

---

## 5. Peta Skill yang Digunakan

| Skill | Peran pada M7 |
|---|---|
| `osdev-general` | Readiness gate M7, dependency map, rollback, acceptance criteria |
| `osdev-01-computer-foundation` | Invariants, state machine map/query/unmap, proof obligations |
| `osdev-02-low-level-programming` | CR2/CR3/invlpg, ABI C, freestanding, pointer/alignment safety |
| `osdev-03-computer-and-hardware-architecture` | x86_64 paging, canonical address, TLB, page fault semantics |
| `osdev-04-kernel-development` | Trap integration, page fault diagnostics, kernel object lifetime |
| `osdev-07-os-security` | W^X, NX, user/supervisor bit, invalid mapping threat model |
| `osdev-10-boot-firmware` | HHDM, bootloader page table, memory map handoff |
| `osdev-12-toolchain-devenv` | Build, object audit, disassembly, QEMU/GDB workflow |
| `osdev-14-cross-science` | Verification matrix, risk register, evidence discipline |

---

## 6. Struktur Repository yang Diharapkan

Tambahkan atau sesuaikan file berikut pada repository hasil M6.

```text
mcsos/
├── include/
│   ├── types.h
│   ├── pmm.h
│   └── vmm.h
├── src/
│   ├── pmm.c
│   ├── vmm.c
│   ├── kernel.c
│   └── arch/x86_64/...
├── tests/
│   └── test_vmm_host.c
├── scripts/
│   ├── m7_preflight.sh
│   └── grade_m7.sh
├── build/
└── Makefile
```

M7 tidak menghapus file M0–M6. Semua perubahan harus berupa commit baru, misalnya `m7-vmm-core`.

---

## 7. Kontrak Desain VMM M7

### 7.1 Preconditions

1. Kernel sudah berada di long mode.
2. Interrupt dan exception path M4 minimal dapat mencetak panic/log.
3. PMM M6 dapat menyediakan frame 4 KiB yang tidak overlap dengan kernel dan data bootloader yang masih dipakai.
4. Page table frame dapat diakses melalui `phys_to_virt`, biasanya memakai HHDM bootloader.
5. Semua virtual address yang dipetakan harus canonical.
6. Semua virtual address dan physical address yang dipetakan harus aligned 4096 byte.
7. VMM M7 hanya memetakan 4 KiB page; huge page dilarang pada tugas wajib.

### 7.2 Invariants

| Kode | Invariant | Test/bukti |
|---|---|---|
| VMM-I1 | Root page table `root_paddr` selalu aligned 4 KiB | `vmm_space_init()` menolak unaligned root |
| VMM-I2 | Virtual address harus canonical 48-bit | Host test noncanonical gagal |
| VMM-I3 | `vaddr` dan `paddr` pada map/unmap/query harus 4 KiB aligned | Host test unaligned gagal |
| VMM-I4 | Intermediate table baru selalu di-zero sebelum present | `vmm_zero_page()` dipanggil sebelum entry dipasang |
| VMM-I5 | Remap leaf present tidak boleh diam-diam overwrite | Duplicate map mengembalikan `VMM_ERR_EXISTS` |
| VMM-I6 | Unmap leaf present menghapus entry dan invalidasi TLB | `vmm_unmap_page()` memanggil `vmm_invalidate_page()` |
| VMM-I7 | Huge page tidak dipakai pada tugas wajib | Jika bit huge ditemukan di intermediate, query/map/unmap menolak |
| VMM-I8 | Physical table diedit melalui adapter eksplisit `phys_to_virt` | Tidak ada cast fisik langsung ke pointer C tanpa adapter |
| VMM-I9 | Object freestanding tidak bergantung libc | `nm -u build/vmm.o` kosong |

### 7.3 Postconditions

1. `vmm_map_page()` berhasil hanya jika leaf sebelumnya tidak present.
2. `vmm_query_page()` mengembalikan physical address dan flags yang sama dengan mapping yang dibuat.
3. `vmm_unmap_page()` membuat query berikutnya mengembalikan `VMM_ERR_NOT_FOUND`.
4. `vmm_invalidate_page()` tersedia pada target x86_64 dan menjadi no-op pada host test.
5. Semua error path mengembalikan kode error eksplisit, bukan silent success.

---

## 8. Pemeriksaan Kesiapan Sebelum Menulis Kode

Jalankan pemeriksaan berikut dari root repository. Perintah ini memastikan toolchain, QEMU, artefak M6, dan file M7 tersedia. Jika belum tersedia, buat file kosong sementara hanya setelah memahami fungsi yang akan diisi.

```bash
mkdir -p scripts
cat > scripts/m7_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

echo "[M7-PREFLIGHT] pemeriksaan lingkungan dan hasil M0-M6"

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[FAIL] command tidak ditemukan: $1" >&2
    exit 1
  fi
  echo "[OK] $1 -> $(command -v "$1")"
}

need_file() {
  if [ ! -f "$1" ]; then
    echo "[FAIL] file wajib tidak ada: $1" >&2
    exit 1
  fi
  echo "[OK] file ada: $1"
}

need_dir() {
  if [ ! -d "$1" ]; then
    echo "[FAIL] direktori wajib tidak ada: $1" >&2
    exit 1
  fi
  echo "[OK] direktori ada: $1"
}

need_cmd git
need_cmd make
need_cmd clang
need_cmd ld.lld
need_cmd readelf
need_cmd objdump
need_cmd nm
need_cmd qemu-system-x86_64

need_dir include
need_dir src
need_dir tests
need_file include/pmm.h
need_file src/pmm.c
need_file include/vmm.h
need_file src/vmm.c
need_file tests/test_vmm_host.c
need_file Makefile

if ! grep -R "pmm_alloc_frame" include src >/dev/null 2>&1; then
  echo "[FAIL] API pmm_alloc_frame dari M6 tidak ditemukan" >&2
  exit 1
fi
if ! grep -R "pmm_free_frame" include src >/dev/null 2>&1; then
  echo "[FAIL] API pmm_free_frame dari M6 tidak ditemukan" >&2
  exit 1
fi
if ! grep -R "x86_64_trap_dispatch" include src >/dev/null 2>&1; then
  echo "[WARN] dispatcher trap M4 belum ditemukan; page fault logging M7 harus diintegrasikan manual"
else
  echo "[OK] dispatcher trap M4 terdeteksi"
fi
if ! grep -R "timer" include src >/dev/null 2>&1; then
  echo "[WARN] artefak timer M5 belum terdeteksi; M7 tetap dapat diuji host-side, tetapi readiness M5 harus diperbaiki"
else
  echo "[OK] artefak timer M5 terdeteksi"
fi

make clean >/dev/null 2>&1 || true
make check

if nm -u build/vmm.o | grep -v '^$'; then
  echo "[FAIL] build/vmm.o memiliki unresolved symbol" >&2
  exit 1
fi

objdump -dr build/vmm.o > build/vmm.objdump.txt
grep -q "invlpg" build/vmm.objdump.txt || { echo "[FAIL] invlpg tidak terlihat pada disassembly" >&2; exit 1; }
grep -q "cr3" build/vmm.objdump.txt || { echo "[FAIL] akses CR3 tidak terlihat pada disassembly" >&2; exit 1; }

echo "[PASS] M7 preflight selesai. Lanjutkan integrasi QEMU hanya setelah laporan M0-M6 lengkap."

EOF
chmod +x scripts/m7_preflight.sh
./scripts/m7_preflight.sh
```

Indikator hasil yang dapat diterima:

```text
[OK] git -> ...
[OK] make -> ...
[OK] clang -> ...
[OK] qemu-system-x86_64 -> ...
[PASS] M7 preflight selesai...
```

Jika preflight gagal pada `pmm_alloc_frame`, kembali ke M6. Jangan menambal VMM untuk menutupi PMM yang rusak. Jika `x86_64_trap_dispatch` belum ditemukan, M7 tetap dapat berjalan untuk host test, tetapi page fault diagnosis harus diintegrasikan manual sebelum QEMU smoke test dinilai lulus.

---

## 9. Implementasi Langkah demi Langkah

### Langkah 1 — Buat Header `include/vmm.h`

Header ini mendefinisikan boundary API VMM. Fungsi tidak bergantung pada libc dan dapat dipakai oleh kernel maupun host unit test. Adapter `phys_to_virt` sengaja eksplisit agar mahasiswa tidak menganggap physical address otomatis dapat dereference sebagai pointer.

```bash
cat > include/vmm.h <<'EOF'
#ifndef MCSOS_VMM_H
#define MCSOS_VMM_H

#include "types.h"

#define VMM_PAGE_SIZE 4096ULL
#define VMM_ENTRIES_PER_TABLE 512U
#define VMM_INVALID_PHYS UINT64_MAX

#define VMM_PTE_PRESENT   (1ULL << 0)
#define VMM_PTE_WRITABLE  (1ULL << 1)
#define VMM_PTE_USER      (1ULL << 2)
#define VMM_PTE_WRITE_THROUGH (1ULL << 3)
#define VMM_PTE_CACHE_DISABLE (1ULL << 4)
#define VMM_PTE_ACCESSED  (1ULL << 5)
#define VMM_PTE_DIRTY     (1ULL << 6)
#define VMM_PTE_HUGE      (1ULL << 7)
#define VMM_PTE_GLOBAL    (1ULL << 8)
#define VMM_PTE_NO_EXECUTE (1ULL << 63)
#define VMM_PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define VMM_MAP_OK 0
#define VMM_ERR_INVAL -1
#define VMM_ERR_NOMEM -2
#define VMM_ERR_EXISTS -3
#define VMM_ERR_NOT_FOUND -4

typedef uint64_t (*vmm_alloc_frame_fn)(void *ctx);
typedef void (*vmm_free_frame_fn)(void *ctx, uint64_t frame_paddr);
typedef void *(*vmm_phys_to_virt_fn)(void *ctx, uint64_t paddr);

struct vmm_space {
    uint64_t root_paddr;
    void *ctx;
    vmm_alloc_frame_fn alloc_frame;
    vmm_free_frame_fn free_frame;
    vmm_phys_to_virt_fn phys_to_virt;
};

struct vmm_mapping {
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t flags;
};

bool vmm_is_aligned_4k(uint64_t value);
bool vmm_is_canonical(uint64_t vaddr);
int vmm_space_init(struct vmm_space *space,
                   uint64_t root_paddr,
                   void *ctx,
                   vmm_alloc_frame_fn alloc_frame,
                   vmm_free_frame_fn free_frame,
                   vmm_phys_to_virt_fn phys_to_virt);
int vmm_map_page(struct vmm_space *space, uint64_t vaddr, uint64_t paddr, uint64_t flags);
int vmm_unmap_page(struct vmm_space *space, uint64_t vaddr);
int vmm_query_page(struct vmm_space *space, uint64_t vaddr, struct vmm_mapping *out);
void vmm_invalidate_page(uint64_t vaddr);
uint64_t vmm_read_cr3(void);
void vmm_write_cr3(uint64_t value);
uint64_t vmm_read_cr2(void);

#endif

EOF
```

Periksa cepat isi header.

```bash
sed -n '1,220p' include/vmm.h
```

Keluaran yang benar harus memuat `struct vmm_space`, `vmm_map_page`, `vmm_query_page`, `vmm_unmap_page`, `vmm_read_cr2`, `vmm_read_cr3`, dan `vmm_write_cr3`.

### Langkah 2 — Buat Implementasi `src/vmm.c`

Implementasi berikut hanya memakai operasi C freestanding, validasi alignment, validasi canonical address, dan primitive assembly x86_64. Pada host test, primitive arsitektural dibuat no-op agar test tidak mencoba membaca CR3 dari proses Linux host.

```bash
cat > src/vmm.c <<'EOF'
#include "vmm.h"

static void vmm_zero_page(uint64_t *page) {
    for (size_t i = 0; i < VMM_ENTRIES_PER_TABLE; i++) {
        page[i] = 0;
    }
}

bool vmm_is_aligned_4k(uint64_t value) {
    return (value & (VMM_PAGE_SIZE - 1ULL)) == 0;
}

bool vmm_is_canonical(uint64_t vaddr) {
    uint64_t sign = (vaddr >> 47) & 1ULL;
    uint64_t upper = vaddr >> 48;
    return sign ? (upper == 0xFFFFULL) : (upper == 0ULL);
}

static unsigned idx_pml4(uint64_t vaddr) { return (unsigned)((vaddr >> 39) & 0x1FFULL); }
static unsigned idx_pdpt(uint64_t vaddr) { return (unsigned)((vaddr >> 30) & 0x1FFULL); }
static unsigned idx_pd(uint64_t vaddr) { return (unsigned)((vaddr >> 21) & 0x1FFULL); }
static unsigned idx_pt(uint64_t vaddr) { return (unsigned)((vaddr >> 12) & 0x1FFULL); }

static uint64_t *table_from_phys(struct vmm_space *space, uint64_t paddr) {
    if (space == 0 || space->phys_to_virt == 0 || !vmm_is_aligned_4k(paddr)) {
        return 0;
    }
    return (uint64_t *)space->phys_to_virt(space->ctx, paddr);
}

static int get_or_alloc_next_table(struct vmm_space *space, uint64_t *table, unsigned index, uint64_t **out) {
    uint64_t entry = table[index];
    if ((entry & VMM_PTE_PRESENT) != 0) {
        if ((entry & VMM_PTE_HUGE) != 0) {
            return VMM_ERR_EXISTS;
        }
        uint64_t next_paddr = entry & VMM_PTE_ADDR_MASK;
        uint64_t *next = table_from_phys(space, next_paddr);
        if (next == 0) {
            return VMM_ERR_INVAL;
        }
        *out = next;
        return VMM_MAP_OK;
    }

    if (space->alloc_frame == 0) {
        return VMM_ERR_NOMEM;
    }
    uint64_t new_paddr = space->alloc_frame(space->ctx);
    if (new_paddr == VMM_INVALID_PHYS || !vmm_is_aligned_4k(new_paddr)) {
        return VMM_ERR_NOMEM;
    }
    uint64_t *new_table = table_from_phys(space, new_paddr);
    if (new_table == 0) {
        if (space->free_frame != 0) {
            space->free_frame(space->ctx, new_paddr);
        }
        return VMM_ERR_INVAL;
    }
    vmm_zero_page(new_table);
    table[index] = (new_paddr & VMM_PTE_ADDR_MASK) | VMM_PTE_PRESENT | VMM_PTE_WRITABLE;
    *out = new_table;
    return VMM_MAP_OK;
}

int vmm_space_init(struct vmm_space *space,
                   uint64_t root_paddr,
                   void *ctx,
                   vmm_alloc_frame_fn alloc_frame,
                   vmm_free_frame_fn free_frame,
                   vmm_phys_to_virt_fn phys_to_virt) {
    if (space == 0 || phys_to_virt == 0 || !vmm_is_aligned_4k(root_paddr)) {
        return VMM_ERR_INVAL;
    }
    space->root_paddr = root_paddr;
    space->ctx = ctx;
    space->alloc_frame = alloc_frame;
    space->free_frame = free_frame;
    space->phys_to_virt = phys_to_virt;
    return VMM_MAP_OK;
}

int vmm_map_page(struct vmm_space *space, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (space == 0 || !vmm_is_canonical(vaddr) || !vmm_is_aligned_4k(vaddr) || !vmm_is_aligned_4k(paddr)) {
        return VMM_ERR_INVAL;
    }
    uint64_t *pml4 = table_from_phys(space, space->root_paddr);
    if (pml4 == 0) {
        return VMM_ERR_INVAL;
    }

    uint64_t *pdpt = 0;
    uint64_t *pd = 0;
    uint64_t *pt = 0;
    int rc = get_or_alloc_next_table(space, pml4, idx_pml4(vaddr), &pdpt);
    if (rc != VMM_MAP_OK) { return rc; }
    rc = get_or_alloc_next_table(space, pdpt, idx_pdpt(vaddr), &pd);
    if (rc != VMM_MAP_OK) { return rc; }
    rc = get_or_alloc_next_table(space, pd, idx_pd(vaddr), &pt);
    if (rc != VMM_MAP_OK) { return rc; }

    unsigned pti = idx_pt(vaddr);
    if ((pt[pti] & VMM_PTE_PRESENT) != 0) {
        return VMM_ERR_EXISTS;
    }
    uint64_t allowed = VMM_PTE_WRITABLE | VMM_PTE_USER | VMM_PTE_WRITE_THROUGH |
                       VMM_PTE_CACHE_DISABLE | VMM_PTE_GLOBAL | VMM_PTE_NO_EXECUTE;
    pt[pti] = (paddr & VMM_PTE_ADDR_MASK) | VMM_PTE_PRESENT | (flags & allowed);
    return VMM_MAP_OK;
}

int vmm_query_page(struct vmm_space *space, uint64_t vaddr, struct vmm_mapping *out) {
    if (space == 0 || out == 0 || !vmm_is_canonical(vaddr) || !vmm_is_aligned_4k(vaddr)) {
        return VMM_ERR_INVAL;
    }
    uint64_t *pml4 = table_from_phys(space, space->root_paddr);
    if (pml4 == 0) { return VMM_ERR_INVAL; }
    uint64_t e = pml4[idx_pml4(vaddr)];
    if ((e & VMM_PTE_PRESENT) == 0 || (e & VMM_PTE_HUGE) != 0) { return VMM_ERR_NOT_FOUND; }
    uint64_t *pdpt = table_from_phys(space, e & VMM_PTE_ADDR_MASK);
    if (pdpt == 0) { return VMM_ERR_INVAL; }
    e = pdpt[idx_pdpt(vaddr)];
    if ((e & VMM_PTE_PRESENT) == 0 || (e & VMM_PTE_HUGE) != 0) { return VMM_ERR_NOT_FOUND; }
    uint64_t *pd = table_from_phys(space, e & VMM_PTE_ADDR_MASK);
    if (pd == 0) { return VMM_ERR_INVAL; }
    e = pd[idx_pd(vaddr)];
    if ((e & VMM_PTE_PRESENT) == 0 || (e & VMM_PTE_HUGE) != 0) { return VMM_ERR_NOT_FOUND; }
    uint64_t *pt = table_from_phys(space, e & VMM_PTE_ADDR_MASK);
    if (pt == 0) { return VMM_ERR_INVAL; }
    e = pt[idx_pt(vaddr)];
    if ((e & VMM_PTE_PRESENT) == 0) { return VMM_ERR_NOT_FOUND; }
    out->vaddr = vaddr;
    out->paddr = e & VMM_PTE_ADDR_MASK;
    out->flags = e & ~VMM_PTE_ADDR_MASK;
    return VMM_MAP_OK;
}

int vmm_unmap_page(struct vmm_space *space, uint64_t vaddr) {
    if (space == 0 || !vmm_is_canonical(vaddr) || !vmm_is_aligned_4k(vaddr)) {
        return VMM_ERR_INVAL;
    }
    uint64_t *pml4 = table_from_phys(space, space->root_paddr);
    if (pml4 == 0) { return VMM_ERR_INVAL; }
    uint64_t e = pml4[idx_pml4(vaddr)];
    if ((e & VMM_PTE_PRESENT) == 0 || (e & VMM_PTE_HUGE) != 0) { return VMM_ERR_NOT_FOUND; }
    uint64_t *pdpt = table_from_phys(space, e & VMM_PTE_ADDR_MASK);
    if (pdpt == 0) { return VMM_ERR_INVAL; }
    e = pdpt[idx_pdpt(vaddr)];
    if ((e & VMM_PTE_PRESENT) == 0 || (e & VMM_PTE_HUGE) != 0) { return VMM_ERR_NOT_FOUND; }
    uint64_t *pd = table_from_phys(space, e & VMM_PTE_ADDR_MASK);
    if (pd == 0) { return VMM_ERR_INVAL; }
    e = pd[idx_pd(vaddr)];
    if ((e & VMM_PTE_PRESENT) == 0 || (e & VMM_PTE_HUGE) != 0) { return VMM_ERR_NOT_FOUND; }
    uint64_t *pt = table_from_phys(space, e & VMM_PTE_ADDR_MASK);
    if (pt == 0) { return VMM_ERR_INVAL; }
    unsigned pti = idx_pt(vaddr);
    if ((pt[pti] & VMM_PTE_PRESENT) == 0) { return VMM_ERR_NOT_FOUND; }
    pt[pti] = 0;
    vmm_invalidate_page(vaddr);
    return VMM_MAP_OK;
}

#if defined(__x86_64__) && !defined(MCSOS_HOST_TEST)
void vmm_invalidate_page(uint64_t vaddr) {
    __asm__ volatile("invlpg (%0)" :: "r"((void *)vaddr) : "memory");
}

uint64_t vmm_read_cr3(void) {
    uint64_t value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(value) :: "memory");
    return value;
}

void vmm_write_cr3(uint64_t value) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(value) : "memory");
}

uint64_t vmm_read_cr2(void) {
    uint64_t value;
    __asm__ volatile("mov %%cr2, %0" : "=r"(value) :: "memory");
    return value;
}
#else
void vmm_invalidate_page(uint64_t vaddr) { (void)vaddr; }
uint64_t vmm_read_cr3(void) { return 0; }
void vmm_write_cr3(uint64_t value) { (void)value; }
uint64_t vmm_read_cr2(void) { return 0; }
#endif

EOF
```

Periksa bagian awal source.

```bash
sed -n '1,120p' src/vmm.c
```

Indikator benar: ada `vmm_zero_page`, `vmm_is_canonical`, dan indeks PML4/PDPT/PD/PT.

### Langkah 3 — Buat Host Unit Test `tests/test_vmm_host.c`

Unit test host ini membuat physical memory palsu sebanyak 64 frame, root page table pada frame 1, allocator mulai dari frame 2, lalu menguji map/query/unmap. Test ini tidak membuktikan paging hardware benar, tetapi membuktikan logika table walk dan invariant dasar.

```bash
mkdir -p tests
cat > tests/test_vmm_host.c <<'EOF'
#include "vmm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_FRAMES 64U
static unsigned char phys[TEST_FRAMES][VMM_PAGE_SIZE];
static bool used[TEST_FRAMES];

static void *host_phys_to_virt(void *ctx, uint64_t paddr) {
    (void)ctx;
    if (!vmm_is_aligned_4k(paddr)) { return 0; }
    uint64_t frame = paddr / VMM_PAGE_SIZE;
    if (frame >= TEST_FRAMES) { return 0; }
    return phys[frame];
}

static uint64_t host_alloc(void *ctx) {
    (void)ctx;
    for (uint64_t i = 2; i < TEST_FRAMES; i++) {
        if (!used[i]) {
            used[i] = true;
            memset(phys[i], 0xA5, VMM_PAGE_SIZE);
            return i * VMM_PAGE_SIZE;
        }
    }
    return VMM_INVALID_PHYS;
}

static void host_free(void *ctx, uint64_t paddr) {
    (void)ctx;
    assert(vmm_is_aligned_4k(paddr));
    uint64_t frame = paddr / VMM_PAGE_SIZE;
    assert(frame < TEST_FRAMES);
    used[frame] = false;
}

int main(void) {
    memset(phys, 0, sizeof(phys));
    memset(used, 0, sizeof(used));
    used[1] = true;

    struct vmm_space space;
    assert(vmm_space_init(&space, VMM_PAGE_SIZE, 0, host_alloc, host_free, host_phys_to_virt) == VMM_MAP_OK);
    assert(vmm_is_canonical(0xFFFF800000200000ULL));
    assert(!vmm_is_canonical(0x0000800000000000ULL));
    assert(vmm_map_page(&space, 0xFFFF800000200000ULL, 0x0000000000300000ULL,
                        VMM_PTE_WRITABLE | VMM_PTE_GLOBAL | VMM_PTE_NO_EXECUTE) == VMM_MAP_OK);

    struct vmm_mapping m;
    assert(vmm_query_page(&space, 0xFFFF800000200000ULL, &m) == VMM_MAP_OK);
    assert(m.vaddr == 0xFFFF800000200000ULL);
    assert(m.paddr == 0x0000000000300000ULL);
    assert((m.flags & VMM_PTE_PRESENT) != 0);
    assert((m.flags & VMM_PTE_WRITABLE) != 0);
    assert((m.flags & VMM_PTE_NO_EXECUTE) != 0);

    assert(vmm_map_page(&space, 0xFFFF800000200000ULL, 0x0000000000400000ULL, 0) == VMM_ERR_EXISTS);
    assert(vmm_map_page(&space, 0xFFFF800000201000ULL, 0x0000000000400001ULL, 0) == VMM_ERR_INVAL);
    assert(vmm_map_page(&space, 0x0000800000000000ULL, 0x0000000000400000ULL, 0) == VMM_ERR_INVAL);
    assert(vmm_unmap_page(&space, 0xFFFF800000200000ULL) == VMM_MAP_OK);
    assert(vmm_query_page(&space, 0xFFFF800000200000ULL, &m) == VMM_ERR_NOT_FOUND);
    assert(vmm_unmap_page(&space, 0xFFFF800000200000ULL) == VMM_ERR_NOT_FOUND);

    assert(vmm_map_page(&space, 0x0000000000400000ULL, 0x0000000000500000ULL, VMM_PTE_WRITABLE) == VMM_MAP_OK);
    assert(vmm_query_page(&space, 0x0000000000400000ULL, &m) == VMM_MAP_OK);
    assert(m.paddr == 0x0000000000500000ULL);

    puts("M7 VMM host tests PASS");
    return 0;
}

EOF
```

Periksa test case utama.

```bash
grep -n "vmm_map_page\|vmm_query_page\|vmm_unmap_page" tests/test_vmm_host.c
```

### Langkah 4 — Sesuaikan Makefile

Jika repository sudah mempunyai Makefile lengkap dari M2–M6, jangan menggantinya seluruhnya tanpa review. Gabungkan target M7 berikut ke Makefile yang sudah ada. Contoh di bawah dapat digunakan untuk subdirektori minimal atau sebagai referensi target.

```bash
cat > Makefile.m7.example <<'EOF'
CC ?= clang
HOSTCC ?= cc
CFLAGS := -std=c17 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -Iinclude
HOST_CFLAGS := -std=c17 -Wall -Wextra -Werror -DMCSOS_HOST_TEST -Iinclude

all: build/vmm.o build/test_vmm_host

build/vmm.o: src/vmm.c include/vmm.h include/types.h
	mkdir -p build
	$(CC) $(CFLAGS) -c src/vmm.c -o build/vmm.o

build/test_vmm_host: src/vmm.c tests/test_vmm_host.c include/vmm.h include/types.h
	mkdir -p build
	$(HOSTCC) $(HOST_CFLAGS) src/vmm.c tests/test_vmm_host.c -o build/test_vmm_host

check: all
	./build/test_vmm_host
	nm -u build/vmm.o
	objdump -dr build/vmm.o > build/vmm.objdump.txt
	grep -q "invlpg" build/vmm.objdump.txt
	grep -q "cr3" build/vmm.objdump.txt

clean:
	rm -rf build

EOF
```

Untuk repository praktikum yang masih minimal, Makefile contoh dapat disalin sebagai Makefile sementara:

```bash
cp Makefile.m7.example Makefile
make clean
make check
```

Indikator benar:

```text
M7 VMM host tests PASS
```

### Langkah 5 — Buat Script Grading Lokal

Script grading lokal mengumpulkan bukti build, object audit, disassembly, dan host test.

```bash
cat > scripts/grade_m7.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/evidence
make clean >/dev/null 2>&1 || true
make check 2>&1 | tee build/evidence/m7_make_check.log
readelf -h build/vmm.o > build/evidence/m7_vmm_readelf_header.txt
readelf -S build/vmm.o > build/evidence/m7_vmm_readelf_sections.txt
nm -u build/vmm.o > build/evidence/m7_vmm_nm_undefined.txt
objdump -dr build/vmm.o > build/evidence/m7_vmm_objdump.txt
if [ -s build/evidence/m7_vmm_nm_undefined.txt ]; then
  echo "[FAIL] unresolved symbol ditemukan pada build/vmm.o" >&2
  exit 1
fi
grep -q "invlpg" build/evidence/m7_vmm_objdump.txt
grep -q "cr3" build/evidence/m7_vmm_objdump.txt
echo "[PASS] static grade M7 selesai"

EOF
chmod +x scripts/grade_m7.sh
./scripts/grade_m7.sh
```

Artefak yang harus muncul:

```text
build/evidence/m7_make_check.log
build/evidence/m7_vmm_readelf_header.txt
build/evidence/m7_vmm_readelf_sections.txt
build/evidence/m7_vmm_nm_undefined.txt
build/evidence/m7_vmm_objdump.txt
```

File `m7_vmm_nm_undefined.txt` harus kosong. Jika tidak kosong, VMM memanggil symbol luar yang belum disediakan kernel atau libc host.

---

## 10. Integrasi dengan PMM M6

Pada kernel nyata, `alloc_frame` harus memanggil PMM M6 dan `phys_to_virt` harus memakai HHDM/direct map yang valid. Contoh adapter konseptual:

```c
static uint64_t kernel_vmm_alloc(void *ctx) {
    (void)ctx;
    return pmm_alloc_frame();
}

static void kernel_vmm_free(void *ctx, uint64_t frame_paddr) {
    (void)ctx;
    pmm_free_frame(frame_paddr);
}

static void *kernel_phys_to_virt(void *ctx, uint64_t paddr) {
    const uint64_t hhdm_offset = *(const uint64_t *)ctx;
    return (void *)(hhdm_offset + paddr);
}
```

Precondition penting: `hhdm_offset + paddr` tidak boleh overflow dan region fisik tersebut benar-benar dipetakan oleh bootloader. Pada base revision Limine tertentu, cakupan HHDM dapat berubah. Jangan memakai HHDM untuk region yang tidak dijamin mapped.

Contoh integrasi awal di `kernel_main`:

```c
static struct vmm_space kernel_space;

void kernel_main(void) {
    serial_init();
    panic_init();
    idt_init();
    pmm_init_from_boot_map();

    uint64_t root = pmm_alloc_frame();
    if (root == PMM_INVALID_FRAME) {
        panic("M7: cannot allocate root page table");
    }

    void *root_virt = kernel_phys_to_virt(&hhdm_offset, root);
    memzero(root_virt, 4096);

    int rc = vmm_space_init(&kernel_space, root, &hhdm_offset,
                            kernel_vmm_alloc, kernel_vmm_free, kernel_phys_to_virt);
    if (rc != VMM_MAP_OK) {
        panic("M7: vmm_space_init failed");
    }

    serial_write("M7: VMM core initialized
");

    /* Tugas wajib berhenti di sini. Jangan write_cr3 sebelum mapping lengkap. */
}
```

Catatan: contoh di atas memakai `memzero`. Jika kernel belum mempunyai `memzero`, buat fungsi freestanding internal, bukan memanggil `memset` libc host.

---

## 11. Page Fault Diagnostics

Tambahkan dekoder page fault pada dispatcher M4. Pada x86_64, page fault adalah exception vector 14. CR2 berisi linear address yang menyebabkan fault. Error code page fault berisi bit penting yang harus dicetak.

```c
static void page_fault_dump(uint64_t error_code, const struct trap_frame *tf) {
    uint64_t cr2 = vmm_read_cr2();
    log("#PF page fault
");
    log_hex("cr2", cr2);
    log_hex("error", error_code);
    log_hex("rip", tf->rip);
    log_hex("rsp", tf->rsp);
    log_bool("present/protection", (error_code & 1) != 0);
    log_bool("write", (error_code & 2) != 0);
    log_bool("user", (error_code & 4) != 0);
    log_bool("reserved", (error_code & 8) != 0);
    log_bool("instruction_fetch", (error_code & 16) != 0);
}
```

Jika logging function belum ada, gunakan serial log minimal dari M3. Jangan menggunakan `printf` hosted libc. Setelah page fault dump dicetak, kernel pada M7 boleh `panic` atau `halt`. Recovery demand paging belum menjadi target M7.

---

## 12. QEMU Smoke Test

QEMU smoke test M7 bertujuan memverifikasi bahwa integrasi VMM tidak merusak boot dan page fault path dapat diamati. Perintah berikut diasumsikan pipeline ISO dari M2 masih dipakai.

```bash
qemu-system-x86_64 \
  -machine q35 \
  -cpu max \
  -m 256M \
  -serial stdio \
  -no-reboot \
  -no-shutdown \
  -d int,cpu_reset,guest_errors \
  -D build/qemu-m7.log \
  -cdrom build/mcsos.iso

```

Indikator benar pada serial log:

```text
MCSOS M7 boot
M6 PMM initialized
M7 VMM core initialized
M7 ready for QEMU smoke test
```

Jika melakukan uji fault terkendali, log minimal harus menampilkan:

```text
#PF page fault
cr2=...
error=...
rip=...
rsp=...
```

Jangan melakukan write ke alamat liar tanpa memastikan panic path dan `-no-reboot -no-shutdown` aktif. Jika QEMU langsung reset, kemungkinan besar terjadi double fault/triple fault akibat handler fault atau stack fault tidak valid.

---

## 13. Workflow GDB

Jalankan QEMU dalam mode menunggu GDB.

```bash
qemu-system-x86_64 -cdrom build/mcsos.iso -serial stdio -no-reboot -no-shutdown -s -S
```

Buat file command GDB.

```bash
cat > scripts/m7_gdb.cmd <<'EOF'
set confirm off
set pagination off
file build/kernel.elf
target remote localhost:1234
break kernel_main
break vmm_map_page
break x86_64_trap_dispatch
continue
# Setelah breakpoint tercapai, gunakan:
# info registers cr2 cr3 rip rsp
# x/16gx $rsp
# x/8i $rip

EOF
```

Hubungkan GDB.

```bash
gdb -x scripts/m7_gdb.cmd
```

Perintah GDB yang relevan:

```gdb
info registers cr2 cr3 rip rsp
x/16gx $rsp
x/16gx 0xffff800000200000
break vmm_map_page
break vmm_unmap_page
continue
```

Jika `info registers cr3` gagal pada GDB tertentu, gunakan `p/x $cr3` atau `maintenance print raw-registers` sesuai dukungan GDB/QEMU setempat.

---

## 14. Failure Modes dan Solusi Perbaikan

| Gejala | Kemungkinan penyebab | Diagnosis | Perbaikan konservatif |
|---|---|---|---|
| Host test gagal pada duplicate map | `vmm_map_page` overwrite leaf present | Cek test `VMM_ERR_EXISTS` | Jangan overwrite PTE; return error |
| Host test gagal pada unaligned paddr | Validasi alignment hilang | Jalankan `./build/test_vmm_host` | Tambahkan `vmm_is_aligned_4k()` pada map |
| `nm -u build/vmm.o` tidak kosong | Memanggil libc atau symbol kernel yang belum link | Baca `m7_vmm_nm_undefined.txt` | Pindahkan dependency ke adapter; hindari `memset` di object freestanding |
| Disassembly tidak memuat `invlpg` | Build host-test saja atau macro salah | Cek `objdump -dr build/vmm.o` | Pastikan build freestanding tanpa `-DMCSOS_HOST_TEST` |
| QEMU reset setelah `write_cr3` | Mapping kernel/stack/IDT/serial belum lengkap | Jalankan `-d int,cpu_reset` dan GDB | Jangan aktifkan CR3 baru pada tugas wajib; lengkapi mapping dulu |
| Page fault tidak mencetak CR2 | Handler #PF belum terhubung atau vmm_read_cr2 tidak dilink | Break `x86_64_trap_dispatch` | Integrasikan vector 14 dan log CR2 |
| Hang setelah unmap | TLB masih menyimpan translasi lama atau unmap alamat aktif | Cek `invlpg` dan alamat yang di-unmap | Jangan unmap stack/current code; panggil `invlpg` |
| #PF error bit reserved aktif | PTE berisi reserved bit | Dump PTE dengan GDB | Mask flags dan physical address dengan `VMM_PTE_ADDR_MASK` |
| Mapping HHDM invalid | Salah asumsi cakupan HHDM | Log HHDM offset dan memory map | Validasi region mapped; jangan akses region reserved tanpa mapping |
| `vmm_query_page` menemukan huge bit | Bootloader page table memakai huge page atau table rusak | Dump entry PD/PT | M7 wajib hanya mengelola table sendiri; jangan parse semua bootloader mappings sebagai table 4 KiB |

---

## 15. Prosedur Rollback

Rollback harus mempertahankan artefak M6 yang sudah lulus.

```bash
git status
git diff > build/m7_failed_attempt.diff || true
git restore include/vmm.h src/vmm.c tests/test_vmm_host.c scripts/m7_preflight.sh scripts/grade_m7.sh Makefile
git status
make clean
make check
```

Jika Makefile sudah digabung dengan target M2–M6, jangan restore seluruh Makefile tanpa menyimpan diff. Gunakan `git checkout -- Makefile` hanya jika yakin tidak menghapus target valid dari tahap sebelumnya.

---

## 16. Checkpoint Buildable

| Checkpoint | Perintah | Bukti wajib |
|---|---|---|
| C1 Header VMM | `sed -n '1,220p' include/vmm.h` | API dan flags terlihat |
| C2 Compile object | `make build/vmm.o` | `build/vmm.o` ada |
| C3 Host unit test | `make check` | `M7 VMM host tests PASS` |
| C4 Undefined symbol audit | `nm -u build/vmm.o` | Output kosong |
| C5 Disassembly audit | `objdump -dr build/vmm.o` | Ada `invlpg`, akses `cr3` |
| C6 Kernel integration | `make iso` atau target setara | ISO/ELF baru terbentuk |
| C7 QEMU smoke | QEMU command M7 | Serial log M7 terbaca |
| C8 Page fault diagnostics | Fault test terkendali | Log CR2/error/RIP/RSP terbaca |

---

## 17. Verification Matrix

| Requirement | Implementasi | Test | Evidence |
|---|---|---|---|
| VMM menolak noncanonical VA | `vmm_is_canonical` | Host test noncanonical | `build/evidence/m7_make_check.log` |
| VMM menolak unaligned PA | `vmm_map_page` | Host test unaligned | `build/evidence/m7_make_check.log` |
| VMM tidak overwrite mapping | `VMM_ERR_EXISTS` | Duplicate map test | `build/evidence/m7_make_check.log` |
| VMM dapat query mapping | `vmm_query_page` | Host test query | `build/evidence/m7_make_check.log` |
| VMM dapat unmap | `vmm_unmap_page` | Host test unmap | `build/evidence/m7_make_check.log` |
| Object freestanding | No libc call | `nm -u` | `m7_vmm_nm_undefined.txt` kosong |
| Target x86 memiliki TLB invalidation | `invlpg` | `objdump` grep | `m7_vmm_objdump.txt` |
| Target x86 memiliki CR3 primitive | `read_cr3/write_cr3` | `objdump` grep | `m7_vmm_objdump.txt` |
| Page fault diagnosis | `vmm_read_cr2` di handler #PF | QEMU fault test | Serial log |

---

## 18. Kriteria Lulus Praktikum

Minimum lulus M7:

1. Repository dapat dibangun dari clean checkout.
2. Semua artefak M0–M6 yang wajib masih tersedia dan tidak rusak.
3. `make check` lulus.
4. `tests/test_vmm_host.c` menjalankan semua assertion tanpa gagal.
5. `nm -u build/vmm.o` kosong.
6. `objdump -dr build/vmm.o` menunjukkan `invlpg` dan akses CR3 pada object target.
7. `vmm_map_page`, `vmm_query_page`, dan `vmm_unmap_page` memiliki error path eksplisit.
8. VMM menolak alamat noncanonical dan unaligned.
9. Integrasi kernel minimal mencetak `M7 VMM core initialized` pada QEMU, jika target ISO sudah ada.
10. Page fault path mampu menampilkan CR2/error/RIP/RSP, minimal dalam rancangan integrasi atau uji QEMU.
11. Tidak ada warning kritis saat build.
12. Perubahan Git terkomit.
13. Laporan berisi log build, log test, disassembly evidence, dan failure analysis.

Kriteria pengayaan:

1. Mengaktifkan page table baru dengan `write_cr3()` setelah mapping kernel lengkap.
2. Menambahkan recursive mapping atau self-map untuk introspeksi page table.
3. Menambahkan NX policy jika EFER.NXE diverifikasi.
4. Menambahkan W^X policy awal untuk region kernel text/data.
5. Menambahkan TLB shootdown design note untuk masa SMP.

---

## 19. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | API VMM bekerja, host test lulus, map/query/unmap benar, validasi canonical/alignment benar |
| Kualitas desain dan invariants | 20 | Kontrak VMM jelas, ownership frame jelas, HHDM boundary eksplisit, tidak overwrite mapping |
| Pengujian dan bukti | 20 | `make check`, `nm -u`, `objdump`, QEMU/GDB/log disertakan |
| Debugging dan failure analysis | 10 | Mampu menjelaskan #PF, CR2, error code, TLB stale, triple fault |
| Keamanan dan robustness | 10 | W^X/NX dibahas, user/supervisor bit tidak disalahgunakan, reserved bit dimask |
| Dokumentasi/laporan | 10 | Laporan mengikuti template, commit hash, lingkungan, screenshot/log lengkap |

---

## 20. Pertanyaan Analisis

1. Mengapa `root_paddr` page table harus berupa alamat fisik, bukan virtual address?
2. Mengapa kernel membutuhkan HHDM/direct map untuk mengedit page table yang dialokasikan PMM?
3. Apa risiko jika `vmm_map_page()` mengizinkan remap diam-diam terhadap leaf present?
4. Mengapa `invlpg` dipanggil setelah unmap?
5. Mengapa huge page tidak dipakai pada tugas wajib M7?
6. Jelaskan perbedaan page fault karena non-present page dan page fault karena protection violation.
7. Mengapa akses `write_cr3()` terlalu berisiko jika mapping kernel stack belum lengkap?
8. Apa konsekuensi security jika semua halaman kernel dibuat writable dan executable?
9. Bagaimana desain M7 harus berubah ketika SMP dan TLB shootdown masuk tahap lanjut?
10. Mengapa host unit test tidak cukup untuk membuktikan paging hardware benar?

---

## 21. Template Laporan Praktikum M7

Gunakan template laporan umum praktikum. Isi minimal untuk M7 adalah:

1. Sampul: judul praktikum, nama mahasiswa, NIM, kelas, dosen Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.
2. Tujuan: jelaskan target VMM awal, page table 4-level, dan page fault diagnostics.
3. Dasar teori: x86_64 paging, CR3, TLB, canonical address, page fault error code.
4. Lingkungan: Windows 11, WSL 2, distro, compiler, linker, QEMU, GDB, commit hash.
5. Desain: diagram page-table walk, API VMM, invariants, ownership frame, HHDM boundary.
6. Langkah kerja: perintah, file yang berubah, dan alasan teknis.
7. Hasil uji: `make check`, host unit test, `nm -u`, `objdump`, QEMU log, GDB log jika ada.
8. Analisis: bug yang ditemukan, penyebab, perbaikan, dan residual risk.
9. Keamanan dan reliability: W^X/NX, TLB stale, reserved bit, CR3 rollback, page fault path.
10. Kesimpulan: apa yang berhasil, apa yang belum, dan rencana M8.
11. Lampiran: potongan kode penting, diff ringkas, log penuh, dan referensi.

---

## 22. Readiness Review

| Area | Status jika semua checkpoint lulus | Catatan residual risk |
|---|---|---|
| Toolchain | Siap uji M7 | Tetap perlu pin versi dan clean rebuild |
| PMM dependency | Siap dipakai VMM awal | Hanya jika M6 free/reserved map benar |
| VMM core | Siap uji host dan QEMU smoke | Belum membuktikan page table baru aman di hardware |
| Page fault diagnostics | Siap demonstrasi praktikum | Recovery belum ada; panic/halt masih sah |
| CR3 activation | Belum wajib siap | Pengayaan hanya setelah mapping lengkap dan rollback jelas |
| Security | Baseline awareness | Belum ada user/kernel isolation penuh, W^X enforcement penuh, atau KASLR |
| Overall M7 | *Siap uji QEMU untuk VMM awal* | Bukan siap produksi dan bukan kandidat penggunaan terbatas |

Keputusan readiness: jika semua checkpoint minimum lulus, hasil M7 boleh diberi label **siap uji QEMU untuk Virtual Memory Manager awal**. Jika hanya host unit test lulus tetapi QEMU belum berjalan, label yang benar adalah **siap audit statis dan host-test M7**, belum siap uji QEMU.

---

## 23. Bukti Validasi Source Code Panduan Ini

Source code inti yang dicantumkan pada panduan ini telah diperiksa secara lokal dengan perintah ekuivalen berikut:

```bash
make -C /mnt/data/m7_validate CC=clang HOSTCC=cc check
```

Ringkasan hasil validasi:

```text
clang -std=c17 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -Iinclude -c src/vmm.c -o build/vmm.o
cc -std=c17 -Wall -Wextra -Werror -DMCSOS_HOST_TEST -Iinclude src/vmm.c tests/test_vmm_host.c -o build/test_vmm_host
./build/test_vmm_host
M7 VMM host tests PASS
nm -u build/vmm.o
objdump -dr build/vmm.o > build/vmm.objdump.txt
grep -q "invlpg" build/vmm.objdump.txt
grep -q "cr3" build/vmm.objdump.txt
```

Batasan bukti: validasi tersebut membuktikan compile, host unit test, dan audit object/disassembly lokal. Validasi runtime QEMU/OVMF tetap harus dijalankan ulang pada WSL 2 mahasiswa karena bergantung pada paket QEMU, OVMF, bootloader/ISO, dan konfigurasi host setempat.

---

## References

[1] Intel, “Intel® 64 and IA-32 Architectures Software Developer Manuals,” Intel, updated Apr. 6, 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

[2] Advanced Micro Devices, “AMD64 Architecture Programmer's Manual Volume 2: System Programming,” AMD, Rev. 3.44, Mar. 6, 2026. [Online]. Available: https://docs.amd.com/v/u/en-US/24593_3.44_APM_Vol2

[3] QEMU Project, “GDB usage — QEMU documentation,” QEMU. [Online]. Available: https://qemu.eu/doc/6.0/system/gdb.html

[4] Limine Bootloader Organization, “Limine,” GitHub organization and official mirror information. [Online]. Available: https://github.com/limine-bootloader

[5] `limine` crate documentation, “MemoryMapRequest in limine::request,” docs.rs. [Online]. Available: https://docs.rs/limine/latest/limine/request/struct.MemoryMapRequest.html

[6] `limine-protocol` crate documentation, “HHDMRequest,” docs.rs. [Online]. Available: https://docs.rs/limine-protocol/latest/limine_protocol/struct.HHDMRequest.html

[7] Free Software Foundation, “Using LD, the GNU linker — Scripts,” GNU manuals. [Online]. Available: https://ftp.gnu.org/old-gnu/Manuals/ld/html_node/ld_6.html

[8] LLVM Project, “Linker Script implementation notes and policy — LLD documentation,” LLVM. [Online]. Available: https://lld.llvm.org/ELF/linker_script.html

[9] LLVM Project, “Clang command line argument reference,” LLVM. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html
