# OS_panduan_M11.md

# Panduan Praktikum M11 — ELF64 User Program Loader Awal, Process Image Plan, User Address-Space Contract, dan Kesiapan Transisi Userspace pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M11  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: *siap uji QEMU untuk loader ELF64 user awal dan process-image planning single-core*, bukan siap produksi, bukan bukti isolasi user/kernel penuh, dan bukan kompatibilitas Linux/POSIX penuh.

---

## 1. Ringkasan Praktikum

Praktikum M11 melanjutkan M0 sampai M10. Pada M9 mahasiswa telah membuat kernel thread dan scheduler kooperatif awal. Pada M10 mahasiswa telah membuat ABI system call awal, dispatcher syscall, validasi argumen, dan jalur `int 0x80` terkontrol. M11 menambahkan fondasi untuk **program user**: kernel mulai mampu membaca struktur ELF64, memvalidasi program header `PT_LOAD`, membangun **process image plan**, menentukan segment yang perlu dipetakan, dan menyiapkan kontrak integrasi dengan VMM M7, heap M8, scheduler M9, serta syscall M10.

M11 sengaja tidak langsung menyatakan bahwa MCSOS sudah menjalankan userspace penuh. Loader program user adalah batas kepercayaan yang sangat sensitif. Kesalahan memvalidasi `e_phoff`, `e_phnum`, `p_offset`, `p_filesz`, `p_memsz`, `p_vaddr`, `p_align`, atau `e_entry` dapat menyebabkan pembacaan di luar image, pemetaan alamat kernel sebagai user, segment writable sekaligus executable, page fault tidak terkontrol, atau privilege escalation. Karena itu M11 memulai dari komponen yang deterministik: parser ELF64 dan penyusun rencana load diuji melalui host unit test, lalu dikompilasi sebagai object freestanding x86_64, kemudian diaudit dengan `nm`, `readelf`, `objdump`, dan checksum.

Rujukan teknis utama M11 adalah Intel SDM untuk proteksi, paging, privilege, interrupt/exception, dan task/system programming x86_64; x86-64 psABI untuk konsekuensi ABI dan object/executable format pada target AMD64; dokumentasi Oracle Linker and Libraries Guide untuk struktur program header ELF; dokumentasi Linux kernel ELF sebagai pembanding behavior ELF modern; QEMU gdbstub untuk inspeksi guest; Clang command-line reference untuk kompilasi freestanding; dan GNU ld/binutils untuk linker-script dan inspeksi object [1]–[7].

Keberhasilan M11 tidak boleh ditulis sebagai “tanpa error”. Kriteria minimum M11 adalah: seluruh pemeriksaan readiness M0–M10 terdokumentasi, parser ELF64 lulus host unit test, object freestanding x86_64 berhasil dikompilasi, `nm -u` kosong untuk object praktikum, `readelf` menunjukkan ELF64 relocatable object, `objdump` memuat symbol `m11_elf64_plan_load`, checksum artefak tersimpan, QEMU smoke test dapat dijalankan ulang pada WSL 2 mahasiswa, dan laporan memuat bukti build, test, audit, log, failure analysis, serta readiness review.

---

## 2. Assumptions and Target / Asumsi Target, Batasan, dan Non-Goals

| Aspek | Keputusan M11 |
|---|---|
| Arsitektur | x86_64 long mode |
| Lingkungan host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Boot path | Melanjutkan pipeline M2–M10; direkomendasikan Limine/UEFI atau ISO yang sudah lulus M2 |
| Bahasa | C17 freestanding untuk loader; host C17 untuk unit test |
| Format program user | ELF64 little-endian, `ET_EXEC` atau `ET_DYN`, `EM_X86_64`, `PT_LOAD` |
| Loader M11 | Validasi header dan program header, rencana pemetaan segment, rencana zero-fill BSS, dan W^X check awal |
| Integrasi runtime | Single-core, process-image planning, bukan ring 3 penuh final |
| Fondasi wajib | M4 IDT/trap, M5 timer, M6 PMM, M7 VMM, M8 heap, M9 scheduler, M10 syscall |
| Out of scope | Dynamic linker, shared library, `fork/exec/wait` lengkap, demand paging, copy-on-write, ASLR/KASLR penuh, signal, credential, file-backed mmap, SMP exec, dan kompatibilitas Linux penuh |

### 2.1 Goals

M11 bertujuan membuat mahasiswa mampu memvalidasi image ELF64, memisahkan konsep **file offset** dan **virtual address**, menolak segment berbahaya, membangun rencana pemetaan process image, menghubungkan rencana tersebut ke allocator dan VMM melalui kontrak yang eksplisit, dan menyiapkan jalur menuju transisi userspace pada modul berikutnya.

### 2.2 Non-Goals

M11 tidak membuktikan bahwa user/kernel isolation sudah final. M11 juga tidak menjamin bahwa semua program Linux dapat dijalankan. Loader yang dibuat hanya membaca subset ELF64 yang sengaja dibatasi. Fitur seperti dynamic relocation, interpreter `PT_INTERP`, TLS, auxiliary vector, environment vector, file descriptor inheritance, dan signal frame ditunda sampai fondasi process dan VFS lebih matang.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M11, mahasiswa mampu:

1. Menjelaskan hubungan ELF header, program header, segment, section, dan process image.
2. Menjelaskan mengapa loader harus menggunakan program header, bukan section header, untuk membangun image runtime.
3. Memvalidasi magic ELF, class, endianness, version, type, machine, ukuran ELF header, ukuran program header, dan batas tabel program header.
4. Memvalidasi `PT_LOAD` berdasarkan `p_offset`, `p_filesz`, `p_memsz`, `p_vaddr`, `p_align`, dan `p_flags`.
5. Mendeteksi integer overflow pada kalkulasi `offset + filesz` dan `vaddr + memsz`.
6. Menolak segment yang berada di luar user virtual region.
7. Menerapkan kebijakan awal W^X dengan menolak segment writable sekaligus executable.
8. Menyusun process image plan yang dapat dikonsumsi oleh VMM M7 dan PMM M6.
9. Menjelaskan kontrak zero-fill untuk `.bss` ketika `p_memsz > p_filesz`.
10. Menulis host unit test untuk kasus valid dan negative cases.
11. Mengompilasi source loader sebagai object freestanding x86_64.
12. Mengaudit object dengan `nm`, `readelf`, `objdump`, dan checksum.
13. Menjelaskan failure modes: malformed ELF, overflow, invalid alignment, invalid user range, W+X segment, bad entry, mapping failure, page fault, dan rollback.
14. Menulis laporan praktikum dengan bukti yang dapat diverifikasi.

---

## 4. Prasyarat Teori

| Materi | Kebutuhan dalam M11 |
|---|---|
| ELF executable format | Memahami `Elf64_Ehdr`, `Elf64_Phdr`, `PT_LOAD`, `PF_R`, `PF_W`, `PF_X` |
| Virtual memory | Menentukan region user dan mencegah pemetaan ke alamat kernel |
| Integer arithmetic safety | Mencegah overflow pada offset dan address bounds |
| Page table | Menyiapkan mapping user pages melalui VMM M7 |
| Physical allocator | Menyediakan frame untuk segment user melalui PMM M6 |
| Kernel heap | Menyimpan metadata process image dan plan loader melalui M8 |
| Scheduler | Membuat thread/process awal setelah image valid melalui M9 |
| Syscall | Menyediakan jalur kontrol balik dari user program ke kernel melalui M10 |
| Security boundary | Menolak input tak tepercaya dan menerapkan fail-closed behavior |

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M11 |
|---|---|
| `@osdev-general` | Gate, roadmap, acceptance evidence, dan readiness review |
| `@osdev-01-computer-foundation` | State machine loader, invariant, proof obligation, dan negative test |
| `@osdev-02-low-level-programming` | Freestanding C, ABI, object audit, alignment, overflow, dan ELF layout |
| `@osdev-03-computer-and-hardware-architecture` | x86_64 paging, user/supervisor bit, privilege boundary, dan fault model |
| `@osdev-04-kernel-development` | Process image, scheduler handoff, trap/syscall integration, dan observability |
| `@osdev-05-filesystem-development` | Sumber executable dari initrd/ramfs/file layer pada tahap lanjutan |
| `@osdev-07-os-security` | Threat model loader, W^X, user range, fail-closed, dan malformed input handling |
| `@osdev-10-boot-firmware` | Initrd/module handoff sebagai sumber program user awal |
| `@osdev-12-toolchain-devenv` | Build, host test, freestanding compile, audit, checksum, QEMU/GDB workflow |
| `@osdev-14-cross-science` | Verification matrix, risk register, evidence-based readiness, dan laporan |

---

## 6. Alat dan Versi yang Harus Dicatat

Mahasiswa wajib mencatat versi tool aktual. Jangan menyalin versi contoh tanpa memeriksa komputer sendiri.

```bash
uname -a
cat /etc/os-release | sed -n '1,8p'
clang --version | sed -n '1,4p'
gcc --version | sed -n '1p' || true
ld --version | sed -n '1p' || true
ld.lld --version || true
make --version | sed -n '1p'
qemu-system-x86_64 --version | sed -n '1p' || true
gdb --version | sed -n '1p' || true
nm --version | sed -n '1p'
readelf --version | sed -n '1p'
objdump --version | sed -n '1p'
git --version
```

Artefak yang harus disimpan pada laporan: output versi tool, commit hash, log host unit test, `readelf` header, `nm -u`, potongan `objdump`, checksum, dan log QEMU jika integrasi runtime dijalankan.

---

## 7. Repository Awal, Branch, dan Kebijakan Git

Praktikum M11 harus dimulai dari hasil M10 yang sudah dikomit. Tujuannya adalah memastikan rollback selalu tersedia jika loader merusak build kernel.

```bash
git status --short
git log --oneline -5
git checkout -b praktikum-m11-elf-user-loader
mkdir -p kernel/user include/mcsos/user tests/m11 scripts build
```

Jika `git status --short` menunjukkan perubahan dari M10 yang belum dikomit, hentikan pekerjaan M11. Komit atau stash perubahan M10 terlebih dahulu. Loader program user menyentuh area sensitif seperti VMM, scheduler, dan syscall; tanpa checkpoint Git, diagnosis regresi akan sulit dilakukan.

---

## 8. Pemeriksaan Kesiapan Hasil M0–M10

M11 hanya boleh dilanjutkan setelah readiness berikut diperiksa. Jika salah satu item gagal, jalankan perbaikan sebelum menambah source M11.

| Tahap | Bukti minimum | Perintah pemeriksaan | Kendala umum | Saran perbaikan |
|---|---|---|---|---|
| M0 | WSL 2, toolchain, QEMU/GDB tersedia | `wsl.exe --status`, `clang --version`, `qemu-system-x86_64 --version` | QEMU tidak ada di WSL | Instal paket QEMU di WSL, bukan hanya di Windows PATH |
| M1 | Toolchain audit dan proof compile lulus | `make m1-check` atau target sepadan | Target triple salah | Gunakan target x86_64 yang konsisten dan audit `readelf -h` |
| M2 | Kernel boot/ISO awal tersedia | `make run` atau `make qemu` | ISO tidak terbentuk | Periksa Limine/OVMF path dan linker script |
| M3 | Panic/logging terbaca | Cek serial log | Panic tidak mencetak file/line | Pastikan early console sudah aktif sebelum panic |
| M4 | IDT exception path stabil | Trigger `int3`/fault terkontrol | Triple fault | Audit IDT descriptor, selector, IST, dan `iretq` |
| M5 | Timer tick deterministik | Cek log IRQ0/PIT | Interrupt storm | Mask/unmask PIC dan EOI salah |
| M6 | PMM lulus invariant test | `make m6-test` | Frame reserved ikut dialokasi | Validasi memory map dan bitmap ownership |
| M7 | VMM dapat memetakan page kernel/user awal | `make m7-test` | Page fault pada HHDM | Audit CR3, PTE flags, dan mapping HHDM |
| M8 | Kernel heap tersedia | `make m8-test` | Fragmentasi/free list corrupt | Jalankan host test allocator dan guard check |
| M9 | Scheduler kooperatif berjalan | `make m9-all` | Context switch merusak register | Audit callee-saved registers dan stack alignment |
| M10 | Syscall dispatcher lulus test | `make m10-all` | Pointer validation lemah | Tambahkan overflow check dan negative tests |

Jalankan script preflight berikut untuk membantu pemeriksaan awal. Script ini bukan bukti correctness; ia hanya mempercepat deteksi tool dan marker source.

```bash
cat > scripts/m11_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

echo "[M11] Preflight lingkungan dan artefak M0-M10"
for tool in git make clang nm readelf objdump sha256sum; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "[FAIL] tool tidak ditemukan: $tool" >&2
    exit 1
  fi
  echo "[OK] $tool -> $(command -v "$tool")"
done

clang --version | sed -n '1,3p'
make --version | sed -n '1p'

required_dirs=(kernel arch include scripts tests)
for d in "${required_dirs[@]}"; do
  if [ ! -d "$d" ]; then
    echo "[WARN] direktori $d belum ada; sesuaikan dengan struktur repository MCSOS Anda"
  else
    echo "[OK] direktori $d tersedia"
  fi
done

required_markers=(
  "kernel_main"
  "panic"
  "idt"
  "pmm"
  "vmm"
  "kmalloc"
  "sched"
  "syscall"
)
for m in "${required_markers[@]}"; do
  if grep -R "${m}" -n kernel arch include 2>/dev/null | head -n 1 >/dev/null; then
    echo "[OK] marker ditemukan: $m"
  else
    echo "[WARN] marker belum ditemukan: $m"
  fi
done

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "[OK] commit: $(git rev-parse --short HEAD)"
  git status --short
else
  echo "[WARN] direktori ini belum menjadi repository Git"
fi

EOF
chmod +x scripts/m11_preflight.sh
./scripts/m11_preflight.sh | tee build/m11_preflight.log
```

Jika marker seperti `pmm`, `vmm`, `kmalloc`, `sched`, atau `syscall` tidak ditemukan, jangan otomatis menganggap praktikum gagal. Struktur repository tiap kelompok dapat berbeda. Periksa nama fungsi setara, lalu sesuaikan integrasi M11 dengan interface yang benar.

---

## 9. Konsep Inti M11

### 9.1 ELF Header dan Program Header

ELF executable memiliki ELF header yang mendeskripsikan identitas file, target machine, entry point, dan lokasi program header table. Program header table berisi segment yang perlu disiapkan oleh loader untuk process image. Dalam M11, section header tidak dipakai untuk loading runtime karena section adalah sudut pandang linker/debugger, sedangkan program header adalah sudut pandang loader.

### 9.2 `PT_LOAD`

Entry `PT_LOAD` menyatakan bagian file yang harus dimuat ke virtual memory. `p_offset` menunjuk lokasi data di file. `p_vaddr` menunjuk alamat virtual tujuan. `p_filesz` adalah jumlah byte dari file. `p_memsz` adalah jumlah byte di memori. Jika `p_memsz > p_filesz`, selisihnya harus diisi nol; kasus ini lazim untuk `.bss`.

### 9.3 User Virtual Region

M11 menetapkan user virtual region eksplisit. Semua `p_vaddr`, `p_vaddr + p_memsz`, dan `e_entry` harus berada dalam region tersebut. Region contoh dalam host test adalah `0x400000..0x8000000000`. Pada kernel nyata, region harus diselaraskan dengan layout VMM M7 dan tidak boleh bertabrakan dengan higher-half kernel, HHDM, MMIO, stack kernel, atau metadata page table.

### 9.4 W^X

M11 menolak segment yang writable sekaligus executable. Kebijakan ini bukan hardening lengkap, tetapi merupakan baseline yang penting agar loader tidak membuat page yang dapat ditulis sekaligus dieksekusi. Tahap lanjut dapat menambahkan NX bit, user/supervisor bit, per-process page table, ASLR, dan policy audit.

### 9.5 Process Image Plan

M11 tidak langsung memetakan semua page di test host. Fungsi `m11_elf64_plan_load` hanya menghasilkan rencana: entry point, jumlah segment, offset file, alamat virtual, ukuran file, ukuran memori, alignment, dan flags. Rencana ini kemudian dikonsumsi oleh integrasi kernel untuk melakukan alokasi frame, mapping user page, copy data dari image, dan zero-fill.

---

## 10. Architecture and Design / Arsitektur Ringkas

Alur M11 adalah sebagai berikut.

```text
initrd/ramfs ELF image
        |
        v
m11_elf64_plan_load()
        |
        +-- validasi ELF ident/type/machine/version
        +-- validasi program header bounds
        +-- validasi setiap PT_LOAD
        +-- validasi user range + overflow
        +-- validasi W^X dan alignment
        |
        v
m11_process_image_plan
        |
        +-- integrasi PMM: alokasi frame
        +-- integrasi VMM: map user pages
        +-- integrasi heap: metadata process/thread
        +-- integrasi scheduler: task pertama
        +-- integrasi syscall: exit/yield/write awal
        v
kandidat process user awal untuk smoke test
```

### 10.1 Invariants M11

| Invariant | Penjelasan | Bukti wajib |
|---|---|---|
| I1 — ELF ident valid | Magic, class, endian, dan version harus sesuai | Host unit test bad magic/class/endian/version |
| I2 — Machine valid | Hanya `EM_X86_64` diterima | Negative test bad machine |
| I3 — Header size valid | `e_ehsize` dan `e_phentsize` harus cocok struktur M11 | Negative test phentsize jika ditambahkan |
| I4 — PH table bounded | `e_phoff + e_phnum * e_phentsize` tidak overflow dan tidak keluar image | Host unit test bounds |
| I5 — Segment file bounded | `p_offset + p_filesz` tidak overflow dan tidak keluar image | Host unit test file range outside image |
| I6 — Memory size valid | `p_memsz >= p_filesz` | Host unit test memsz below filesz |
| I7 — User range valid | `p_vaddr..p_vaddr+p_memsz` berada dalam user region | Host unit test segment outside user range |
| I8 — Entry valid | `e_entry` berada dalam user region | Host unit test entry outside user range |
| I9 — Alignment valid | `p_align` adalah 0/1 atau power-of-two, dan offset/vaddr congruent | Host unit test bad alignment |
| I10 — W^X baseline | Segment tidak boleh writable sekaligus executable | Unit test tambahan/pemeriksaan kode |
| I11 — Fail-closed | Jika satu segment invalid, plan dikosongkan dan error dikembalikan | Review source dan negative test |

---

## 11. Implementation Plan / Instruksi Implementasi Langkah demi Langkah

### Langkah 1 — Siapkan direktori M11

Perintah berikut menambahkan struktur source yang terisolasi. Tujuannya agar loader M11 dapat diuji tanpa mengganggu source kernel utama.

```bash
mkdir -p kernel/user include/mcsos/user tests/m11 scripts build
```

Indikator berhasil: direktori `kernel/user`, `include/mcsos/user`, dan `tests/m11` tersedia. Jika repository Anda memakai struktur berbeda, tetap pertahankan pemisahan antara header publik, implementasi kernel, dan test.

### Langkah 2 — Tambahkan header loader

Header ini mendefinisikan subset ELF64 yang dipakai M11. Header sengaja tidak mengimpor libc selain header tipe compiler (`stddef.h`, `stdint.h`) agar dapat dikompilasi dalam mode freestanding.

```bash
cat > include/mcsos/user/m11_elf_loader.h <<'EOF'
#ifndef MCSOS_M11_ELF_LOADER_H
#define MCSOS_M11_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#define M11_EI_NIDENT 16u
#define M11_ELFMAG0 0x7fu
#define M11_ELFMAG1 'E'
#define M11_ELFMAG2 'L'
#define M11_ELFMAG3 'F'
#define M11_ELFCLASS64 2u
#define M11_ELFDATA2LSB 1u
#define M11_EV_CURRENT 1u
#define M11_ET_EXEC 2u
#define M11_ET_DYN 3u
#define M11_EM_X86_64 62u
#define M11_PT_LOAD 1u
#define M11_PF_X 1u
#define M11_PF_W 2u
#define M11_PF_R 4u
#define M11_MAX_LOAD_SEGMENTS 8u
#define M11_PAGE_SIZE 4096ull

#define M11_OK 0
#define M11_ERR_NULL -1
#define M11_ERR_SIZE -2
#define M11_ERR_MAGIC -3
#define M11_ERR_CLASS -4
#define M11_ERR_ENDIAN -5
#define M11_ERR_VERSION -6
#define M11_ERR_TYPE -7
#define M11_ERR_MACHINE -8
#define M11_ERR_EHSIZE -9
#define M11_ERR_PHENTSIZE -10
#define M11_ERR_PHBOUNDS -11
#define M11_ERR_ALIGN -12
#define M11_ERR_SEGBOUNDS -13
#define M11_ERR_SEGRANGE -14
#define M11_ERR_SEGCOUNT -15
#define M11_ERR_ENTRY -16
#define M11_ERR_FLAGS -17

struct m11_elf64_ehdr {
    unsigned char e_ident[M11_EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct m11_elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct m11_user_region {
    uint64_t base;
    uint64_t limit;
};

struct m11_segment_plan {
    uint64_t file_offset;
    uint64_t vaddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
    uint32_t flags;
};

struct m11_process_image_plan {
    uint64_t entry;
    uint32_t segment_count;
    struct m11_segment_plan segments[M11_MAX_LOAD_SEGMENTS];
};

int m11_validate_user_range(struct m11_user_region region, uint64_t base, uint64_t size);
int m11_elf64_plan_load(const void *image, size_t image_size,
                        struct m11_user_region region,
                        struct m11_process_image_plan *out_plan);
const char *m11_error_name(int code);

#endif

EOF
```

Checkpoint: header tidak boleh bergantung pada `stdio.h`, `stdlib.h`, `string.h`, thread library, atau API host.

### Langkah 3 — Tambahkan implementasi loader

Implementasi berikut melakukan validasi defensif. Perhatikan bahwa semua kalkulasi batas memakai helper overflow, plan dikosongkan saat gagal, dan segment W+X ditolak.

```bash
cat > kernel/user/m11_elf_loader.c <<'EOF'
#include "m11_elf_loader.h"

static int m11_add_overflow_u64(uint64_t a, uint64_t b, uint64_t *out) {
    uint64_t r = a + b;
    if (r < a) {
        return 1;
    }
    *out = r;
    return 0;
}

static int m11_is_power_of_two_u64(uint64_t v) {
    return v != 0u && (v & (v - 1u)) == 0u;
}

static void m11_zero_plan(struct m11_process_image_plan *plan) {
    plan->entry = 0u;
    plan->segment_count = 0u;
    for (uint32_t i = 0u; i < M11_MAX_LOAD_SEGMENTS; ++i) {
        plan->segments[i].file_offset = 0u;
        plan->segments[i].vaddr = 0u;
        plan->segments[i].filesz = 0u;
        plan->segments[i].memsz = 0u;
        plan->segments[i].align = 0u;
        plan->segments[i].flags = 0u;
    }
}

int m11_validate_user_range(struct m11_user_region region, uint64_t base, uint64_t size) {
    uint64_t end = 0u;
    if (region.base >= region.limit) {
        return M11_ERR_SEGRANGE;
    }
    if (size == 0u) {
        return M11_ERR_SEGRANGE;
    }
    if (m11_add_overflow_u64(base, size, &end) != 0) {
        return M11_ERR_SEGRANGE;
    }
    if (base < region.base || end > region.limit || end <= base) {
        return M11_ERR_SEGRANGE;
    }
    return M11_OK;
}

static int m11_validate_ident(const struct m11_elf64_ehdr *eh) {
    if (eh->e_ident[0] != M11_ELFMAG0 || eh->e_ident[1] != M11_ELFMAG1 ||
        eh->e_ident[2] != M11_ELFMAG2 || eh->e_ident[3] != M11_ELFMAG3) {
        return M11_ERR_MAGIC;
    }
    if (eh->e_ident[4] != M11_ELFCLASS64) {
        return M11_ERR_CLASS;
    }
    if (eh->e_ident[5] != M11_ELFDATA2LSB) {
        return M11_ERR_ENDIAN;
    }
    if (eh->e_ident[6] != M11_EV_CURRENT || eh->e_version != M11_EV_CURRENT) {
        return M11_ERR_VERSION;
    }
    return M11_OK;
}

static int m11_validate_phdr_bounds(const struct m11_elf64_ehdr *eh, size_t image_size) {
    uint64_t ph_table_bytes = 0u;
    uint64_t ph_end = 0u;
    if (eh->e_phnum == 0u) {
        return M11_ERR_PHBOUNDS;
    }
    if (eh->e_phentsize != sizeof(struct m11_elf64_phdr)) {
        return M11_ERR_PHENTSIZE;
    }
    ph_table_bytes = (uint64_t)eh->e_phentsize * (uint64_t)eh->e_phnum;
    if (eh->e_phnum != 0u && ph_table_bytes / eh->e_phnum != eh->e_phentsize) {
        return M11_ERR_PHBOUNDS;
    }
    if (m11_add_overflow_u64(eh->e_phoff, ph_table_bytes, &ph_end) != 0) {
        return M11_ERR_PHBOUNDS;
    }
    if (ph_end > (uint64_t)image_size || eh->e_phoff > (uint64_t)image_size) {
        return M11_ERR_PHBOUNDS;
    }
    return M11_OK;
}

static int m11_validate_load_segment(const struct m11_elf64_phdr *ph, size_t image_size,
                                     struct m11_user_region region) {
    uint64_t file_end = 0u;
    if ((ph->p_flags & ~(M11_PF_R | M11_PF_W | M11_PF_X)) != 0u) {
        return M11_ERR_FLAGS;
    }
    if ((ph->p_flags & M11_PF_W) != 0u && (ph->p_flags & M11_PF_X) != 0u) {
        return M11_ERR_FLAGS;
    }
    if (ph->p_memsz < ph->p_filesz) {
        return M11_ERR_SEGBOUNDS;
    }
    if (ph->p_align != 0u && ph->p_align != 1u) {
        if (!m11_is_power_of_two_u64(ph->p_align)) {
            return M11_ERR_ALIGN;
        }
        if ((ph->p_vaddr % ph->p_align) != (ph->p_offset % ph->p_align)) {
            return M11_ERR_ALIGN;
        }
    }
    if (m11_add_overflow_u64(ph->p_offset, ph->p_filesz, &file_end) != 0) {
        return M11_ERR_SEGBOUNDS;
    }
    if (file_end > (uint64_t)image_size || ph->p_offset > (uint64_t)image_size) {
        return M11_ERR_SEGBOUNDS;
    }
    return m11_validate_user_range(region, ph->p_vaddr, ph->p_memsz);
}

int m11_elf64_plan_load(const void *image, size_t image_size,
                        struct m11_user_region region,
                        struct m11_process_image_plan *out_plan) {
    const struct m11_elf64_ehdr *eh = (const struct m11_elf64_ehdr *)image;
    int rc = M11_OK;
    if (image == 0 || out_plan == 0) {
        return M11_ERR_NULL;
    }
    m11_zero_plan(out_plan);
    if (image_size < sizeof(struct m11_elf64_ehdr)) {
        return M11_ERR_SIZE;
    }
    rc = m11_validate_ident(eh);
    if (rc != M11_OK) {
        return rc;
    }
    if (eh->e_type != M11_ET_EXEC && eh->e_type != M11_ET_DYN) {
        return M11_ERR_TYPE;
    }
    if (eh->e_machine != M11_EM_X86_64) {
        return M11_ERR_MACHINE;
    }
    if (eh->e_ehsize != sizeof(struct m11_elf64_ehdr)) {
        return M11_ERR_EHSIZE;
    }
    rc = m11_validate_phdr_bounds(eh, image_size);
    if (rc != M11_OK) {
        return rc;
    }
    rc = m11_validate_user_range(region, eh->e_entry, 1u);
    if (rc != M11_OK) {
        return M11_ERR_ENTRY;
    }
    const unsigned char *bytes = (const unsigned char *)image;
    const struct m11_elf64_phdr *ph = (const struct m11_elf64_phdr *)(const void *)(bytes + eh->e_phoff);
    out_plan->entry = eh->e_entry;
    for (uint16_t i = 0u; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != M11_PT_LOAD) {
            continue;
        }
        if (out_plan->segment_count >= M11_MAX_LOAD_SEGMENTS) {
            m11_zero_plan(out_plan);
            return M11_ERR_SEGCOUNT;
        }
        rc = m11_validate_load_segment(&ph[i], image_size, region);
        if (rc != M11_OK) {
            m11_zero_plan(out_plan);
            return rc;
        }
        struct m11_segment_plan *seg = &out_plan->segments[out_plan->segment_count];
        seg->file_offset = ph[i].p_offset;
        seg->vaddr = ph[i].p_vaddr;
        seg->filesz = ph[i].p_filesz;
        seg->memsz = ph[i].p_memsz;
        seg->align = ph[i].p_align;
        seg->flags = ph[i].p_flags;
        out_plan->segment_count++;
    }
    if (out_plan->segment_count == 0u) {
        return M11_ERR_SEGCOUNT;
    }
    return M11_OK;
}

const char *m11_error_name(int code) {
    switch (code) {
        case M11_OK: return "M11_OK";
        case M11_ERR_NULL: return "M11_ERR_NULL";
        case M11_ERR_SIZE: return "M11_ERR_SIZE";
        case M11_ERR_MAGIC: return "M11_ERR_MAGIC";
        case M11_ERR_CLASS: return "M11_ERR_CLASS";
        case M11_ERR_ENDIAN: return "M11_ERR_ENDIAN";
        case M11_ERR_VERSION: return "M11_ERR_VERSION";
        case M11_ERR_TYPE: return "M11_ERR_TYPE";
        case M11_ERR_MACHINE: return "M11_ERR_MACHINE";
        case M11_ERR_EHSIZE: return "M11_ERR_EHSIZE";
        case M11_ERR_PHENTSIZE: return "M11_ERR_PHENTSIZE";
        case M11_ERR_PHBOUNDS: return "M11_ERR_PHBOUNDS";
        case M11_ERR_ALIGN: return "M11_ERR_ALIGN";
        case M11_ERR_SEGBOUNDS: return "M11_ERR_SEGBOUNDS";
        case M11_ERR_SEGRANGE: return "M11_ERR_SEGRANGE";
        case M11_ERR_SEGCOUNT: return "M11_ERR_SEGCOUNT";
        case M11_ERR_ENTRY: return "M11_ERR_ENTRY";
        case M11_ERR_FLAGS: return "M11_ERR_FLAGS";
        default: return "M11_ERR_UNKNOWN";
    }
}

EOF
```

Checkpoint: source ini harus dapat dikompilasi sebagai object freestanding. Jika compiler menghasilkan panggilan tersembunyi ke libc, audit flag compiler dan pola kode yang memicu builtin.

### Langkah 4 — Tambahkan host unit test

Host unit test membuat ELF64 sintetis di memori. Tujuannya adalah menguji loader tanpa QEMU agar bug parser dapat ditemukan lebih cepat.

```bash
cat > tests/m11/m11_host_test.c <<'EOF'
#include "m11_elf_loader.h"
#include <stdio.h>
#include <string.h>

#define IMAGE_SIZE 12288u

static struct m11_user_region test_region(void) {
    struct m11_user_region r;
    r.base = 0x0000000000400000ull;
    r.limit = 0x0000008000000000ull;
    return r;
}

static void make_valid_image(unsigned char image[IMAGE_SIZE]) {
    memset(image, 0, IMAGE_SIZE);
    struct m11_elf64_ehdr *eh = (struct m11_elf64_ehdr *)(void *)image;
    eh->e_ident[0] = M11_ELFMAG0;
    eh->e_ident[1] = M11_ELFMAG1;
    eh->e_ident[2] = M11_ELFMAG2;
    eh->e_ident[3] = M11_ELFMAG3;
    eh->e_ident[4] = M11_ELFCLASS64;
    eh->e_ident[5] = M11_ELFDATA2LSB;
    eh->e_ident[6] = M11_EV_CURRENT;
    eh->e_type = M11_ET_EXEC;
    eh->e_machine = M11_EM_X86_64;
    eh->e_version = M11_EV_CURRENT;
    eh->e_entry = 0x0000000000401000ull;
    eh->e_phoff = sizeof(struct m11_elf64_ehdr);
    eh->e_ehsize = sizeof(struct m11_elf64_ehdr);
    eh->e_phentsize = sizeof(struct m11_elf64_phdr);
    eh->e_phnum = 2u;
    struct m11_elf64_phdr *ph = (struct m11_elf64_phdr *)(void *)(image + eh->e_phoff);
    ph[0].p_type = M11_PT_LOAD;
    ph[0].p_flags = M11_PF_R | M11_PF_X;
    ph[0].p_offset = 0x1000u;
    ph[0].p_vaddr = 0x0000000000400000ull;
    ph[0].p_filesz = 16u;
    ph[0].p_memsz = 4096u;
    ph[0].p_align = M11_PAGE_SIZE;
    ph[1].p_type = M11_PT_LOAD;
    ph[1].p_flags = M11_PF_R | M11_PF_W;
    ph[1].p_offset = 0x2000u;
    ph[1].p_vaddr = 0x0000000000401000ull;
    ph[1].p_filesz = 8u;
    ph[1].p_memsz = 4096u;
    ph[1].p_align = M11_PAGE_SIZE;
}

static int expect_code(const char *name, int got, int expected) {
    if (got != expected) {
        printf("FAIL %s: got=%s(%d) expected=%s(%d)\n", name, m11_error_name(got), got,
               m11_error_name(expected), expected);
        return 1;
    }
    printf("PASS %s: %s\n", name, m11_error_name(got));
    return 0;
}

int main(void) {
    unsigned failures = 0u;
    unsigned char image[IMAGE_SIZE];
    struct m11_process_image_plan plan;
    make_valid_image(image);
    int rc = m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan);
    failures += expect_code("valid ELF64 image", rc, M11_OK);
    if (rc == M11_OK && (plan.entry != 0x401000ull || plan.segment_count != 2u)) {
        printf("FAIL valid plan fields\n");
        failures++;
    } else if (rc == M11_OK) {
        printf("PASS valid plan fields: entry=0x%llx segments=%u\n",
               (unsigned long long)plan.entry, plan.segment_count);
    }

    make_valid_image(image);
    image[0] = 0u;
    failures += expect_code("bad magic", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_MAGIC);

    make_valid_image(image);
    ((struct m11_elf64_ehdr *)(void *)image)->e_machine = 3u;
    failures += expect_code("bad machine", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_MACHINE);

    make_valid_image(image);
    ((struct m11_elf64_ehdr *)(void *)image)->e_entry = 0x1000u;
    failures += expect_code("entry outside user range", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_ENTRY);

    make_valid_image(image);
    struct m11_elf64_phdr *ph = (struct m11_elf64_phdr *)(void *)(image + sizeof(struct m11_elf64_ehdr));
    ph[0].p_memsz = 4u;
    ph[0].p_filesz = 16u;
    failures += expect_code("memsz below filesz", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_SEGBOUNDS);

    make_valid_image(image);
    ph = (struct m11_elf64_phdr *)(void *)(image + sizeof(struct m11_elf64_ehdr));
    ph[0].p_offset = 0x3000u;
    ph[0].p_filesz = 1u;
    failures += expect_code("file range outside image", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_SEGBOUNDS);

    make_valid_image(image);
    ph = (struct m11_elf64_phdr *)(void *)(image + sizeof(struct m11_elf64_ehdr));
    ph[0].p_align = 24u;
    failures += expect_code("bad alignment", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_ALIGN);

    make_valid_image(image);
    ph = (struct m11_elf64_phdr *)(void *)(image + sizeof(struct m11_elf64_ehdr));
    ph[0].p_vaddr = 0x0000800000000000ull;
    failures += expect_code("segment outside user range", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_SEGRANGE);

    if (failures != 0u) {
        printf("M11 host tests failed: %u\n", failures);
        return 1;
    }
    printf("M11 host tests passed.\n");
    return 0;
}

EOF
```

Checkpoint: host test harus mencakup minimal satu kasus valid dan beberapa kasus negatif. Jangan hanya menguji happy path.

### Langkah 5 — Tambahkan Makefile M11

Makefile ini menyediakan target host test, freestanding object, dan audit object.

```bash
cat > Makefile.m11 <<'EOF'
CC ?= clang
OBJDUMP ?= objdump
READELF ?= readelf
NM ?= nm
SHA256SUM ?= sha256sum
HOST_CFLAGS := -std=c17 -Wall -Wextra -Werror -O2 -g
TARGET_CFLAGS := --target=x86_64-unknown-none -std=c17 -Wall -Wextra -Werror -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -c

.PHONY: all host-test freestanding audit clean
all: host-test freestanding audit

host-test: m11_host_test
	./m11_host_test

m11_host_test: m11_elf_loader.c m11_elf_loader.h m11_host_test.c
	$(CC) $(HOST_CFLAGS) m11_elf_loader.c m11_host_test.c -o $@

freestanding: build/m11_elf_loader.o

build/m11_elf_loader.o: m11_elf_loader.c m11_elf_loader.h
	mkdir -p build
	$(CC) $(TARGET_CFLAGS) m11_elf_loader.c -o $@

audit: build/m11_elf_loader.o
	$(NM) -u build/m11_elf_loader.o > build/m11_nm_undefined.txt
	test ! -s build/m11_nm_undefined.txt
	$(READELF) -h build/m11_elf_loader.o > build/m11_readelf_header.txt
	$(OBJDUMP) -dr build/m11_elf_loader.o > build/m11_objdump.txt
	$(SHA256SUM) build/m11_elf_loader.o m11_elf_loader.c m11_elf_loader.h m11_host_test.c > build/m11_sha256.txt
	grep -q 'ELF64' build/m11_readelf_header.txt
	grep -q 'm11_elf64_plan_load' build/m11_objdump.txt

clean:
	rm -rf build m11_host_test

EOF
```

Jika repository utama sudah memiliki Makefile, gabungkan target berikut sebagai target `m11-all`, `m11-host-test`, `m11-freestanding`, dan `m11-audit`, bukan mengganti seluruh Makefile proyek.

### Langkah 6 — Jalankan host unit test

Perintah berikut menjalankan test parser dan process-image plan pada host WSL.

```bash
make -f Makefile.m11 CC=clang host-test | tee build/m11_host_test.log
```

Output yang diharapkan minimal:

```text
PASS valid ELF64 image: M11_OK
PASS valid plan fields: entry=0x401000 segments=2
PASS bad magic: M11_ERR_MAGIC
PASS bad machine: M11_ERR_MACHINE
PASS entry outside user range: M11_ERR_ENTRY
PASS memsz below filesz: M11_ERR_SEGBOUNDS
PASS file range outside image: M11_ERR_SEGBOUNDS
PASS bad alignment: M11_ERR_ALIGN
PASS segment outside user range: M11_ERR_SEGRANGE
M11 host tests passed.
```

Jika test gagal, jangan lanjut ke QEMU. Perbaiki parser lebih dahulu karena QEMU hanya akan memperbesar biaya debugging.

### Langkah 7 — Kompilasi freestanding object

Perintah berikut memeriksa bahwa source loader dapat dikompilasi untuk target x86_64 freestanding.

```bash
make -f Makefile.m11 CC=clang freestanding | tee build/m11_freestanding.log
```

Jika `--target=x86_64-unknown-none` tidak dikenali, besar kemungkinan `CC` masih menunjuk ke `cc`/GCC, bukan Clang. Jalankan `make -f Makefile.m11 CC=clang freestanding`. Jika tetap gagal, catat versi Clang dan sesuaikan target triple dengan toolchain M1.

### Langkah 8 — Audit object

Perintah berikut memeriksa undefined symbols, format object, disassembly, dan checksum.

```bash
make -f Makefile.m11 CC=clang audit | tee build/m11_audit.log
cat build/m11_nm_undefined.txt
sed -n '1,40p' build/m11_readelf_header.txt
grep -n "m11_elf64_plan_load" build/m11_objdump.txt | head
cat build/m11_sha256.txt
```

Kriteria lulus audit:

1. `build/m11_nm_undefined.txt` kosong.
2. `readelf -h` menunjukkan `ELF64`.
3. `objdump` memuat symbol `m11_elf64_plan_load`.
4. Checksum tersimpan di `build/m11_sha256.txt`.

### Langkah 9 — Integrasikan dengan kernel MCSOS secara konservatif

Integrasi kernel harus dilakukan setelah host test dan audit object lulus. Jangan langsung mengaktifkan ring 3 penuh. Integrasi pertama cukup memanggil loader terhadap ELF sintetis atau ELF kecil yang disisipkan sebagai initrd/module, lalu mencetak plan ke serial log.

Contoh kontrak integrasi yang harus dipenuhi oleh kernel:

```c
/* Kontrak integrasi, bukan pengganti interface M7-M10 yang sudah ada. */
struct mcsos_user_loader_ops {
    int (*alloc_user_page)(uint64_t user_va, uint32_t flags);
    int (*copy_to_user_mapping)(uint64_t user_va, const void *src, uint64_t len);
    int (*zero_user_mapping)(uint64_t user_va, uint64_t len);
    void (*trace)(const char *msg);
};
```

Mapping page aktual harus mengikuti aturan berikut:

1. Page untuk segment `PF_X` harus tidak writable.
2. Page untuk segment `PF_W` harus tidak executable jika NX sudah tersedia.
3. Semua page user harus memakai bit user/supervisor sesuai page table M7.
4. Copy file bytes dilakukan sebelum thread user dijadwalkan.
5. Area `p_memsz - p_filesz` harus di-zero.
6. Jika satu mapping gagal, seluruh process image harus dibatalkan dan frame yang sudah dialokasikan harus dilepas.

### Langkah 10 — Tambahkan QEMU smoke test

Script berikut menjalankan QEMU secara headless dan menyimpan serial log. Karena struktur boot image tiap kelompok dapat berbeda, script menerima path ISO sebagai argumen.

```bash
cat > scripts/m11_qemu_smoke.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

ISO_PATH="${1:-build/mcsos.iso}"
LOG_PATH="${2:-build/m11_qemu_serial.log}"
mkdir -p "$(dirname "$LOG_PATH")"

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
  echo "[FAIL] qemu-system-x86_64 tidak ditemukan" >&2
  exit 1
fi
if [ ! -f "$ISO_PATH" ]; then
  echo "[FAIL] ISO tidak ditemukan: $ISO_PATH" >&2
  echo "Jalankan target build ISO dari M2-M10 terlebih dahulu." >&2
  exit 1
fi

timeout 20s qemu-system-x86_64 \
  -M q35 \
  -m 256M \
  -no-reboot \
  -no-shutdown \
  -serial file:"$LOG_PATH" \
  -display none \
  -cdrom "$ISO_PATH" || true

if grep -E "M11|ELF|user|loader|panic" "$LOG_PATH" >/dev/null 2>&1; then
  echo "[OK] log M11 terdeteksi di $LOG_PATH"
else
  echo "[WARN] marker M11 belum terlihat. Periksa integrasi kernel_main dan jalur serial."
fi

EOF
chmod +x scripts/m11_qemu_smoke.sh
./scripts/m11_qemu_smoke.sh build/mcsos.iso build/m11_qemu_serial.log
```

Marker log yang disarankan dari kernel:

```text
[M11] elf: ident ok
[M11] elf: phnum=...
[M11] elf: load segment vaddr=... filesz=... memsz=... flags=...
[M11] elf: plan ok entry=...
[M11] user image plan ready
```

Jika marker tidak muncul, periksa apakah `kernel_main` benar-benar memanggil test integrasi M11, apakah serial logger aktif, dan apakah ISO yang dijalankan adalah hasil build terbaru.

---

## 12. Validation Plan / Checkpoint Buildable

| Checkpoint | Perintah | Artefak | Kriteria lulus |
|---|---|---|---|
| C1 — Preflight | `./scripts/m11_preflight.sh` | `build/m11_preflight.log` | Tool utama ditemukan dan status M0–M10 diketahui |
| C2 — Host test | `make -f Makefile.m11 CC=clang host-test` | `build/m11_host_test.log` | Semua kasus PASS |
| C3 — Freestanding compile | `make -f Makefile.m11 CC=clang freestanding` | `build/m11_elf_loader.o` | Object x86_64 terbentuk |
| C4 — Object audit | `make -f Makefile.m11 CC=clang audit` | `m11_nm_undefined.txt`, `m11_readelf_header.txt`, `m11_objdump.txt`, `m11_sha256.txt` | Undefined symbols kosong; ELF64; symbol loader ada |
| C5 — Kernel integration | `make` target proyek | kernel/ISO | Build M0–M11 tidak regresi |
| C6 — QEMU smoke | `./scripts/m11_qemu_smoke.sh ...` | `build/m11_qemu_serial.log` | Marker M11 muncul tanpa panic tidak terkendali |
| C7 — Git evidence | `git status`, `git log` | commit M11 | Semua perubahan dikomit |

---

## 13. Tugas Implementasi Mahasiswa

### Tugas Wajib

1. Menambahkan header `m11_elf_loader.h`.
2. Menambahkan implementasi `m11_elf_loader.c`.
3. Menambahkan host unit test `m11_host_test.c`.
4. Menambahkan target Makefile untuk host test, freestanding compile, audit object, dan checksum.
5. Menjalankan semua checkpoint C1–C4.
6. Mengintegrasikan loader minimal ke kernel untuk mencetak process image plan pada serial log.
7. Menjalankan QEMU smoke test jika boot image M2–M10 tersedia.
8. Menulis laporan dengan bukti lengkap.

### Tugas Pengayaan

1. Tambahkan negative test untuk `e_phentsize` salah.
2. Tambahkan negative test untuk segment W+X.
3. Tambahkan sorting/check overlap segment virtual address.
4. Tambahkan policy bahwa user stack berada pada region khusus dan tidak bertabrakan dengan segment ELF.
5. Tambahkan loader untuk ELF yang berasal dari initrd M2/M10, bukan hanya image sintetis.
6. Tambahkan tracepoint `loader.reject.reason` agar error M11 mudah didiagnosis.

### Tantangan Riset

1. Rancang `execve` subset: validasi path, buka file dari VFS, load ELF, susun argv/envp, dan commit address space secara atomik.
2. Rancang page-fault-assisted `copy_from_user` dan `copy_to_user` yang dapat memulihkan fault terkontrol.
3. Rancang per-process ASLR sederhana dengan tetap mempertahankan reproducible debug mode.
4. Rancang threat model untuk malicious ELF yang mencoba memicu overflow, overmapping, W+X, atau segment overlap.

---

## 14. Failure Modes dan Solusi Perbaikan

| Gejala | Dugaan penyebab | Pemeriksaan | Perbaikan |
|---|---|---|---|
| Host test gagal pada valid image | Struktur ELF tidak sesuai alignment/offset | Cek `p_offset % p_align` dan `p_vaddr % p_align` | Samakan congruence offset dan vaddr |
| `M11_ERR_MAGIC` pada image valid | Header corrupt atau pointer image salah | Dump 16 byte pertama image | Pastikan magic `7f 45 4c 46` |
| `M11_ERR_PHBOUNDS` | `e_phoff/e_phnum/e_phentsize` keluar dari image | Cetak ukuran image dan PH table | Validasi image size dan struktur header |
| `M11_ERR_SEGBOUNDS` | `p_offset + p_filesz` keluar image atau `p_memsz < p_filesz` | Cetak nilai segment | Koreksi file size dan memory size |
| `M11_ERR_ALIGN` | `p_align` bukan power-of-two atau congruence salah | Cetak `p_align`, `p_offset`, `p_vaddr` | Gunakan alignment page dan offset/vaddr kongruen |
| `M11_ERR_SEGRANGE` | Segment keluar user region | Cek layout VMM M7 | Revisi user base/limit atau linker script user program |
| Undefined symbols pada object | Source memanggil libc/builtin tidak tersedia | `cat build/m11_nm_undefined.txt` | Hilangkan `memcpy/memset/printf`, pakai loop eksplisit di loader |
| `readelf` bukan ELF64 | Target compiler salah | `readelf -h build/m11_elf_loader.o` | Gunakan `CC=clang --target=x86_64-unknown-none` |
| QEMU boot hang setelah integrasi | Loader memetakan alamat salah atau panic sebelum serial siap | Gunakan log serial dan GDB breakpoint | Jalankan host test, rollback integrasi, aktifkan marker log sebelum mapping |
| Page fault saat user image plan | VMM user bit/NX/CR3 salah | Dump CR2, error code, PTE | Audit page table flags dan region user |
| Triple fault saat enter user | GDT/TSS/stack/iret frame salah | QEMU GDB, `info registers` | Jangan lanjut ring3; kembalikan ke planning-only sampai M12 siap |

---

## 15. Prosedur Rollback

Rollback wajib tersedia karena loader menyentuh boundary privilege dan memory mapping.

```bash
git status --short
git diff --stat
# Simpan bukti terlebih dahulu jika diperlukan
mkdir -p rollback-evidence/m11
cp -r build/m11_* rollback-evidence/m11/ 2>/dev/null || true
# Rollback file tertentu
git restore kernel/user/m11_elf_loader.c include/mcsos/user/m11_elf_loader.h tests/m11/m11_host_test.c Makefile.m11
# Atau rollback seluruh branch ke commit terakhir
# git reset --hard HEAD
```

Jika perubahan sudah dikomit tetapi perlu dibatalkan tanpa menghapus riwayat:

```bash
git log --oneline -5
git revert <commit_m11>
```

---

## 16. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator |
|---|---:|---|
| Kebenaran fungsional | 30 | Loader memvalidasi ELF64, PH table, segment, entry, range, alignment, W^X, dan menghasilkan plan benar |
| Kualitas desain dan invariants | 20 | Invariant tertulis; fail-closed; tidak ada dependency libc; interface integrasi jelas |
| Pengujian dan bukti | 20 | Host unit test, negative test, freestanding compile, `nm/readelf/objdump`, checksum, QEMU smoke jika tersedia |
| Debugging/failure analysis | 10 | Failure modes dianalisis dengan log, root cause, dan perbaikan |
| Keamanan dan robustness | 10 | Overflow check, user range, W^X, malformed ELF handling, rollback on partial failure |
| Dokumentasi/laporan | 10 | Laporan lengkap, rapi, mencantumkan environment, commit hash, bukti, analisis, referensi IEEE |

---

## 17. Acceptance Criteria / Kriteria Lulus Praktikum

Mahasiswa dinyatakan lulus M11 jika memenuhi semua kriteria minimum berikut:

1. Repository dapat dibangun dari clean checkout.
2. Hasil M0–M10 sudah diperiksa dan kendala dicatat.
3. `m11_elf_loader.h`, `m11_elf_loader.c`, `m11_host_test.c`, dan target Makefile tersedia.
4. Host unit test M11 lulus.
5. Source loader dapat dikompilasi sebagai C17 freestanding target x86_64.
6. `nm -u` untuk object loader kosong.
7. `readelf -h` menunjukkan ELF64 object.
8. `objdump` memuat symbol loader utama.
9. Checksum artefak disimpan.
10. Integrasi kernel tidak merusak panic path dan serial log.
11. QEMU smoke test dijalankan jika boot image tersedia.
12. Semua perubahan dikomit dengan pesan Git yang jelas.
13. Laporan menyertakan bukti build, test, audit, failure analysis, security review, rollback, dan readiness review.

---

## 18. Security Review dan Pertanyaan Analisis

1. Mengapa loader menggunakan program header, bukan section header, untuk membangun process image?
2. Apa risiko jika `p_memsz < p_filesz` tidak ditolak?
3. Apa risiko jika `p_offset + p_filesz` tidak diperiksa overflow?
4. Mengapa `p_vaddr + p_memsz` harus berada dalam user region?
5. Apa konsekuensi segment writable sekaligus executable?
6. Mengapa zero-fill `.bss` harus dilakukan setelah file bytes disalin?
7. Apa perbedaan `ET_EXEC` dan `ET_DYN` untuk loader pendidikan?
8. Mengapa dynamic linker dan relocation ditunda pada M11?
9. Bagaimana loader harus membersihkan frame jika mapping segment ke-2 gagal setelah segment ke-1 berhasil?
10. Apa bukti minimum sebelum MCSOS boleh mencoba transisi ring 3 penuh?

---

## 19. Template Laporan Praktikum M11

Gunakan format laporan yang sama dengan template umum praktikum.

### 19.1 Sampul

- Judul praktikum: Praktikum M11 — ELF64 User Program Loader Awal dan Process Image Plan
- Nama mahasiswa / kelompok
- NIM
- Kelas
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi Pendidikan Teknologi Informasi
- Institut Pendidikan Indonesia

### 19.2 Tujuan

Tuliskan capaian teknis dan konseptual M11.

### 19.3 Dasar Teori Ringkas

Jelaskan ELF64, program header, `PT_LOAD`, user region, W^X, zero-fill BSS, dan process image plan.

### 19.4 Lingkungan

Catat OS host, WSL, compiler, linker, QEMU, GDB, binutils, commit hash, dan branch.

### 19.5 Desain

Sertakan diagram loader, struktur data, invariants, alur kontrol, dan batasan.

### 19.6 Langkah Kerja

Tuliskan perintah, file yang dibuat, perubahan yang dilakukan, dan alasan teknis.

### 19.7 Hasil Uji

Lampirkan output:

```text
build/m11_preflight.log
build/m11_host_test.log
build/m11_freestanding.log
build/m11_audit.log
build/m11_readelf_header.txt
build/m11_objdump.txt
build/m11_sha256.txt
build/m11_qemu_serial.log jika tersedia
```

### 19.8 Analisis

Bahas keberhasilan, bug yang ditemukan, root cause, perbaikan, dan keterbatasan.

### 19.9 Keamanan dan Reliability

Bahas risiko malformed ELF, overflow, segment overlap, W+X, user/kernel isolation, page fault, dan rollback.

### 19.10 Kesimpulan

Tuliskan apa yang berhasil, apa yang belum, dan rencana perbaikan.

### 19.11 Lampiran

Sertakan potongan kode penting, diff ringkas, log penuh, dan referensi.

---

## 20. Readiness Review M11

| Area | Status minimum | Bukti |
|---|---|---|
| Build | Siap host test dan freestanding compile | Log Makefile M11 |
| Loader correctness | Siap validasi subset ELF64 | Host unit test dan negative test |
| Security boundary | Siap baseline fail-closed, user range, W^X | Review source dan test |
| Runtime integration | Siap QEMU smoke test terbatas | Serial log M11 jika tersedia |
| Ring 3 penuh | Belum siap | Perlu GDT/TSS/user stack/page permission/page-fault recovery lengkap |
| Release label | Siap uji QEMU terbatas | Bukan siap produksi, bukan siap multi-user |

**Keputusan readiness**: hasil M11 dapat dinilai sebagai **siap uji QEMU terbatas untuk ELF64 user loader planning** jika semua checkpoint C1–C4 lulus dan integrasi kernel tidak merusak boot/logging. Hasil M11 belum boleh disebut siap produksi, belum boleh disebut secure, dan belum boleh dianggap mampu menjalankan program POSIX umum.

---

## 21. Bukti Pemeriksaan Source Code Panduan Ini

Source inti M11 pada panduan ini telah diperiksa melalui build lokal dengan perintah setara:

```bash
cd /mnt/data/m11_check
make CC=clang all
```

Hasil pemeriksaan lokal:

```text
PASS valid ELF64 image: M11_OK
PASS valid plan fields: entry=0x401000 segments=2
PASS bad magic: M11_ERR_MAGIC
PASS bad machine: M11_ERR_MACHINE
PASS entry outside user range: M11_ERR_ENTRY
PASS memsz below filesz: M11_ERR_SEGBOUNDS
PASS file range outside image: M11_ERR_SEGBOUNDS
PASS bad alignment: M11_ERR_ALIGN
PASS segment outside user range: M11_ERR_SEGRANGE
M11 host tests passed.
nm -u build/m11_elf_loader.o -> kosong
readelf -h build/m11_elf_loader.o -> ELF64
objdump -> memuat m11_elf64_plan_load
```

Catatan batasan: pemeriksaan lokal membuktikan source host-test dan object freestanding dalam panduan ini dapat dikompilasi dan diuji pada lingkungan pemeriksa. Validasi runtime QEMU/OVMF tetap harus dijalankan ulang di lingkungan WSL 2 mahasiswa karena bergantung pada source MCSOS hasil M0–M10, bootloader, ISO, OVMF, QEMU, dan konfigurasi host setempat.

---

## 22. References

[1] Intel Corporation, “Intel® 64 and IA-32 Architectures Software Developer Manuals,” Intel, updated Apr. 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

[2] x86 psABIs, “x86-64 psABI,” GitLab. [Online]. Available: https://gitlab.com/x86-psABIs/x86-64-ABI

[3] Oracle, “Program Header,” *Linker and Libraries Guide*. [Online]. Available: https://docs.oracle.com/cd/E26502_01/html/E26507/chapter6-83432.html

[4] The Linux Kernel Documentation, “ELF,” kernel.org. [Online]. Available: https://www.kernel.org/doc/html/next/ELF/index.html

[5] QEMU Project, “GDB usage / gdbstub documentation,” QEMU Documentation. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html

[6] LLVM Project, “Clang command line argument reference,” Clang Documentation. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html

[7] GNU Binutils, “Linker Scripts,” Sourceware GNU ld Documentation. [Online]. Available: https://sourceware.org/binutils/docs/ld/Scripts.html
