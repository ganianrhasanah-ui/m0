# OS_panduan_M3.md

# Panduan Praktikum M3 — Panic Path, Kernel Logging, GDB Debug Workflow, Linker Map, dan Disassembly Audit MCSOS 260502

**Identitas akademik:** Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi:** Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia  
**Target sistem:** MCSOS versi 260502, x86_64, kernel monolitik pendidikan, C freestanding dengan inline assembly minimal  
**Host pengembangan:** Windows 11 x64 melalui WSL 2 Linux  
**Status readiness yang ditargetkan:** siap uji QEMU smoke test dan siap audit debug awal, bukan siap produksi.

> Catatan integritas teknis: source code inti M3 dalam panduan ini telah diperiksa melalui kompilasi dan link lokal memakai `clang` dan `ld.lld`, serta lulus audit ELF/disassembly pada lingkungan pembuat panduan. Karena lingkungan ini tidak menyediakan `qemu-system-x86_64`, validasi runtime QEMU/OVMF tetap wajib dilakukan ulang di WSL 2 mahasiswa. Klaim yang sah setelah praktikum adalah “siap uji QEMU dan GDB audit awal”, bukan “tanpa error” atau “siap produksi”.

---

## 0. Ringkasan Praktikum

Praktikum M3 melanjutkan M0, M1, dan M2. Pada M2 mahasiswa telah menghasilkan kernel ELF64 x86_64 minimal yang dapat diload oleh jalur boot berbasis Limine/UEFI dan menulis log awal ke serial COM1. M3 tidak langsung masuk ke interrupt controller, timer, scheduler, atau virtual memory lanjutan. Fokus M3 adalah memperkuat kemampuan diagnosis: panic path, logging yang konsisten, audit simbol, linker map, disassembly, dan workflow GDB.

Tujuan praktikum ini adalah membuat setiap kegagalan awal kernel menjadi teramati, terklasifikasi, dan dapat dianalisis. Kernel pendidikan yang hanya “berhasil boot” belum cukup untuk menjadi fondasi praktikum lanjutan. Pada tahap M3, mahasiswa harus dapat menunjukkan bukti bahwa kernel memiliki jalur berhenti terkendali, panic path yang mencetak alasan, lokasi file/baris, kode panic, sebagian state CPU, serta artefak debug yang dapat diperiksa menggunakan `readelf`, `nm`, `objdump`, linker map, log serial, dan GDB.

Panduan ini memakai prinsip official-first. WSL dipakai karena Microsoft mendokumentasikan bahwa WSL memungkinkan penggunaan aplikasi Linux, utilitas, dan Bash langsung di Windows tanpa dual boot dan menyediakan instalasi satu perintah pada Windows 10/11 yang didukung [1]. QEMU dipakai sebagai emulator sistem karena dokumentasi QEMU menjelaskan system emulation sebagai model virtual machine CPU, memori, dan perangkat untuk menjalankan guest OS [2]. QEMU juga menyediakan gdbstub untuk debugging guest dengan opsi `-s` dan `-S` [3]. Untuk arsitektur x86_64, sumber normatif yang dirujuk adalah Intel SDM yang mencakup environment dukungan OS, termasuk memory management, protection, task management, interrupt and exception handling, dan debugging [4]. Untuk build freestanding, panduan ini memakai Clang `-ffreestanding`, yang dalam dokumentasi resmi berarti kompilasi berlangsung pada freestanding environment [5]. Untuk link ELF dan linker script, panduan ini merujuk dokumentasi LLD dan GNU ld [6], [7]. Untuk jalur boot, panduan tetap kompatibel dengan pendekatan M2 berbasis Limine, bootloader modern multiprotocol yang menjadi reference implementation dari Limine boot protocol [8].

---

## 1. Capaian Pembelajaran Praktikum

Setelah menyelesaikan M3, mahasiswa mampu:

1. Memeriksa kesiapan hasil M0, M1, dan M2 sebelum mengubah kernel.
2. Menjelaskan perbedaan boot berhasil, controlled halt, panic, triple fault, hang, dan silent failure.
3. Membuat panic path awal yang memiliki kontrak `noreturn`, mematikan interrupt, mencetak bukti minimum, lalu masuk halt loop.
4. Membuat wrapper logging awal yang memisahkan API kernel logging dari driver serial.
5. Menghasilkan dua varian kernel: normal kernel dan intentional-panic kernel.
6. Menghasilkan dan menganalisis linker map, symbol table, readelf header, program header, dan disassembly.
7. Menjalankan QEMU smoke test dengan log serial berbasis file.
8. Menyiapkan sesi GDB untuk breakpoint pada `kmain` dan `kernel_panic_at`.
9. Mengumpulkan bukti praktikum secara reproducible ke direktori `evidence/M3`.
10. Menyusun failure analysis dan rollback bila source M0/M1/M2 belum konsisten.

---

## 2. Prasyarat Teori

Mahasiswa harus memahami minimal hal berikut sebelum menulis kode M3.

| Topik | Kebutuhan pada M3 | Bukti pemahaman |
|---|---|---|
| Freestanding C | Kernel tidak memakai libc host; fungsi runtime minimum harus disediakan sendiri. | Mahasiswa dapat menjelaskan mengapa `memcpy`, `memmove`, dan `memset` tetap diperlukan. |
| x86_64 System V ABI | Entry C, register, stack alignment, dan calling convention harus konsisten. | Mahasiswa dapat menjelaskan mengapa `-mno-red-zone` dipakai pada kernel. |
| Serial port COM1 | Logging awal memakai port I/O 0x3F8 pada QEMU. | Mahasiswa dapat menjelaskan konsekuensi busy-wait pada serial. |
| Panic path | Panic harus berhenti terkendali, bukan kembali ke caller. | Mahasiswa dapat menunjukkan fungsi `kernel_panic_at` bertipe `noreturn`. |
| Linker script | Layout section dan symbol `__kernel_start`/`__kernel_end` harus dapat diaudit. | Mahasiswa dapat membaca `kernel.map` dan `nm -n`. |
| Disassembly | Debug kernel awal tidak cukup dengan source-level review. | Mahasiswa dapat menemukan `cli`, `hlt`, `kmain`, dan `kernel_panic_at` pada disassembly. |
| QEMU/GDB | Debug awal memerlukan stop-before-run dan remote target. | Mahasiswa dapat menjalankan QEMU `-s -S` dan GDB `target remote`. |

---

## 3. Peta Skill yang Digunakan

| Skill | Peran pada M3 |
|---|---|
| `osdev-general` | Readiness gate, milestone, rollback, acceptance criteria, dan evidence-first workflow. |
| `osdev-01-computer-foundation` | Invariants, state machine panic, safety property, dan proof obligation. |
| `osdev-02-low-level-programming` | Freestanding C, inline assembly, ABI, linker, red-zone, object inspection, undefined behavior. |
| `osdev-03-computer-and-hardware-architecture` | x86_64 privilege behavior, I/O port, CPU flags, `cli`, `hlt`, dan QEMU/hardware boundary. |
| `osdev-04-kernel-development` | Panic path, logging, kernel invariants, debugging, dan observability awal. |
| `osdev-07-os-security` | Fail-closed panic, kernel/user isolation future, attack surface debug output. |
| `osdev-10-boot-firmware` | Boot-chain continuity dari M2, Limine/UEFI handoff, dan QEMU/OVMF verification. |
| `osdev-12-toolchain-devenv` | Build flags, linker map, disassembly audit, reproducibility, GDB workflow. |
| `osdev-14-cross-science` | Verification matrix, failure model, risk register, dan evidence collection. |

---

## 4. Asumsi Target dan Batasan M3

### 4.1 Asumsi utama

1. Target arsitektur adalah x86_64.
2. Host adalah Windows 11 x64 dengan WSL 2 Linux.
3. Kernel adalah kernel monolitik pendidikan.
4. Bahasa utama adalah C17 freestanding dengan inline assembly kecil untuk port I/O dan instruksi CPU dasar.
5. Boot path tetap mengikuti M2: kernel ELF64 diload oleh boot image berbasis Limine dan diuji dengan QEMU/OVMF.
6. M3 berjalan pada single-core QEMU (`-smp 1`) untuk menghindari masalah concurrency sebelum M4/M5.
7. Belum ada IDT, interrupt handler, timer, allocator, paging custom, scheduler, syscall, filesystem, networking, atau driver model penuh.

### 4.2 Non-goals

M3 tidak mengimplementasikan interrupt descriptor table, page fault handler, PIT/APIC timer, virtual memory manager, physical memory manager, scheduler, userspace, syscall ABI, filesystem, network stack, atau driver selain serial COM1 awal. Komponen tersebut masuk milestone berikutnya. M3 hanya membuat fondasi observability yang harus ada sebelum komponen tersebut ditambahkan.

---

## 4A. Architecture and Design M3

Arsitektur M3 adalah lapisan observability awal di atas kernel M2. Lapisan ini terdiri dari `serial.c` sebagai backend perangkat awal, `log.c` sebagai API logging kernel, `panic.c` sebagai fail-closed panic path, `cpu.h` sebagai boundary instruksi CPU x86_64, `linker.ld` sebagai kontrol layout ELF, dan script audit sebagai verifikator artefak. Desain ini sengaja kecil agar setiap komponen dapat diuji sebelum milestone M4 menambahkan IDT, exception handler, dan timer.

## 4B. Interfaces, ABI, and API Boundary M3

Boundary ABI M3 tetap memakai x86_64 System V calling convention untuk entry C `kmain`. Boundary API internal yang stabil untuk milestone ini adalah `log_init`, `log_write`, `log_writeln`, `log_hex64`, `kernel_panic_at`, `KERNEL_PANIC`, dan `KERNEL_ASSERT`. Semua API tersebut hanya valid dalam kernel early boot single-core dan belum boleh dipakai sebagai kontrak userspace, syscall, atau driver umum.

## 4C. Invariants and Correctness Obligations M3

Invariants utama M3 adalah: panic path tidak boleh kembali; semua jalur fatal harus berakhir pada `cpu_halt_forever`; kernel ELF tidak boleh memiliki undefined symbol; build normal dan build intentional-panic harus sama-sama berhasil; dan artefak audit harus mampu menunjukkan `kmain`, `kernel_panic_at`, `cli`, serta `hlt`. Invariants ini divalidasi melalui `make audit`, `m3_audit_elf.sh`, QEMU serial log, dan GDB breakpoint.

## 4D. Security and Threat Model M3

Security scope M3 masih terbatas pada kernel early boot. Threat utama adalah silent failure, panic yang kembali ke caller, dependency tidak sengaja pada libc host, debug output yang membocorkan path build, dan artefak boot yang tidak dapat diaudit. Mitigasinya adalah fail-closed panic, `-nostdlib`, audit undefined symbol, serial timeout, linker map, disassembly audit, dan evidence manifest. M3 belum mengklaim isolation, hardening, syscall security, secure boot, atau measured boot.

---

## 4E. Assumptions M3

Assumptions M3: kernel berjalan pada x86_64 single-core QEMU/OVMF; bootloader sudah menyerahkan kontrol ke `kmain`; stack awal valid menurut kontrak boot M2; compiler mengikuti C17 freestanding dengan target `x86_64-unknown-none-elf`; dan tidak ada interrupt eksternal yang sengaja diaktifkan sebelum M4. Assumption ini harus ditulis ulang dalam laporan bila mahasiswa mengubah bootloader, target arsitektur, compiler, atau emulator.

## 4F. Validation and Verification Plan M3

Validation M3 dilakukan melalui empat lapis: source syntax check (`bash -n` dan build C), ELF inspection (`readelf`, `nm`, `objdump`), emulator smoke test QEMU dengan serial log, dan GDB breakpoint pada `kmain`/`kernel_panic_at`. Verification evidence minimum adalah `make audit`, `build/kernel.map`, `build/kernel.disasm.txt`, `build/kernel.syms.txt`, `build/m3_serial.log`, dan `evidence/M3/manifest.txt`.

## 4G. Supply Chain, Artifact Provenance, and Checksum Policy

Supply chain M3 dikendalikan secara sederhana: versi compiler/linker/QEMU dicatat dalam manifest, artefak build tidak diambil dari sumber tidak jelas, dan ISO/kernel yang diuji harus berasal dari commit yang sama dengan laporan. Untuk pengumpulan akhir, mahasiswa disarankan menambahkan checksum SHA-256 berikut agar provenance artefak dapat diverifikasi ulang: `sha256sum build/kernel.elf build/kernel.panic.elf build/mcsos.iso > evidence/M3/sha256sums.txt`. Signature artefak belum diwajibkan pada M3, tetapi kebijakan signing akan diperkenalkan pada milestone release/update.

---

## 5. State Machine Kernel M3

Kernel M3 dapat dimodelkan dengan state machine berikut.

| State | Kondisi masuk | Aksi | Kondisi keluar |
|---|---|---|---|
| `BOOT_ENTERED` | Bootloader mentransfer kontrol ke `kmain`. | Inisialisasi logging serial. | `LOG_READY` atau hang bila serial fatal. |
| `LOG_READY` | `log_init()` selesai. | Cetak identitas kernel dan alamat layout. | `SELFTEST_RUNNING`. |
| `SELFTEST_RUNNING` | Kernel menjalankan invariant check ringan. | Cek pointer kernel start/end dan ukuran pointer. | `NORMAL_HALT` atau `PANIC`. |
| `NORMAL_HALT` | Kernel normal selesai menjalankan M3. | Cetak readiness dan masuk `cpu_halt_forever`. | Tidak keluar. |
| `PANIC` | `KERNEL_ASSERT` atau `KERNEL_PANIC` dipanggil. | `cli`, cetak bukti panic, lalu halt. | Tidak keluar. |

Invariants M3:

1. `kernel_panic_at()` tidak boleh kembali ke caller.
2. Setelah panic, CPU harus masuk loop halt dengan interrupt dimatikan.
3. `log_write()` tidak boleh dereference pointer null.
4. `__kernel_end` harus lebih besar dari `__kernel_start`.
5. Kernel ELF tidak boleh memiliki undefined symbol.
6. Kernel ELF harus bertipe ELF64 x86_64.
7. Source kernel tidak boleh bergantung pada libc host.
8. Jalur normal dan jalur intentional panic harus sama-sama dapat dikompilasi dan dilink.

---

## 6. Struktur Repository Setelah M3

Target struktur repository setelah M3 adalah sebagai berikut.

```text
mcsos/
├── Makefile
├── linker.ld
├── kernel/
│   ├── arch/
│   │   └── x86_64/
│   │       └── include/
│   │           └── mcsos/
│   │               └── arch/
│   │                   ├── cpu.h
│   │                   └── io.h
│   ├── core/
│   │   ├── kmain.c
│   │   ├── log.c
│   │   ├── panic.c
│   │   └── serial.c
│   ├── include/
│   │   └── mcsos/
│   │       └── kernel/
│   │           ├── log.h
│   │           ├── panic.h
│   │           └── version.h
│   └── lib/
│       └── memory.c
├── tools/
│   ├── gdb_m3.gdb
│   └── scripts/
│       ├── grade_m3.sh
│       ├── m3_audit_elf.sh
│       ├── m3_collect_evidence.sh
│       ├── m3_preflight.sh
│       ├── m3_qemu_debug.sh
│       └── m3_qemu_run.sh
├── build/                 # generated; tidak dikomit
└── evidence/M3/            # bukti praktikum; dikumpulkan dalam laporan
```

---

## 7. Pemeriksaan Kesiapan Hasil M0/M1/M2 Sebelum M3

Bagian ini wajib dijalankan sebelum menyalin source M3. Tujuannya adalah mencegah mahasiswa menumpuk bug dari praktikum sebelumnya. Jangan lanjut ke kode M3 bila salah satu pemeriksaan kritis gagal.

### 7.1 Pemeriksaan lokasi repository

Jalankan perintah berikut dari WSL.

```bash
pwd
case "$PWD" in /mnt/c/*|/mnt/d/*|/mnt/e/*) echo "WARN: pindahkan repository ke filesystem Linux WSL" ;; *) echo "OK: filesystem Linux" ;; esac
```

Repository sebaiknya berada di filesystem Linux WSL, misalnya `~/mcsos`, bukan `/mnt/c/...`. Build kernel menghasilkan banyak file object kecil, sehingga penyimpanan di filesystem Windows sering memperlambat build dan memperbesar peluang masalah permission/path.

Solusi bila repository masih di `/mnt/c`:

```bash
mkdir -p ~/osdev
rsync -a --delete /mnt/c/path/ke/mcsos/ ~/osdev/mcsos/
cd ~/osdev/mcsos
git status --short
```

### 7.2 Pemeriksaan Git dan branch

```bash
git status --short
git branch --show-current
git log --oneline -5
```

Kondisi ideal: branch praktikum aktif, perubahan M2 telah dikomit, dan tidak ada file build besar yang tidak sengaja masuk staging. Jika `git status --short` menunjukkan banyak file di `build/`, periksa `.gitignore`.

Solusi `.gitignore` minimal:

```bash
cat >> .gitignore <<'EOF'
build/
iso_root/
limine/
*.iso
*.log
*.o
*.elf
*.map
*.disasm.txt
EOF
```

### 7.3 Pemeriksaan artefak M0/M1

```bash
test -f README.md && echo "OK README"
test -d docs && echo "OK docs"
test -d tools/scripts && echo "OK scripts"
command -v clang
command -v ld.lld
command -v readelf
command -v objdump
command -v nm
command -v make
```

Jika `clang` atau `ld.lld` tidak ditemukan, ulangi instalasi paket toolchain dari M1. Pada Ubuntu/WSL biasanya:

```bash
sudo apt update
sudo apt install -y clang lld llvm binutils make git gdb xorriso mtools curl ca-certificates
```

### 7.4 Pemeriksaan artefak M2

```bash
test -f linker.ld || echo "MISSING linker.ld"
test -f kernel/core/kmain.c || echo "MISSING kernel/core/kmain.c"
test -f kernel/core/serial.c || echo "MISSING kernel/core/serial.c"
test -f kernel/lib/memory.c || echo "MISSING kernel/lib/memory.c"
test -f kernel/arch/x86_64/include/mcsos/arch/io.h || echo "MISSING io.h"
make clean
make build
make inspect || true
```

Jika M2 gagal build, jangan lanjut ke M3. Tabel diagnosis awal:

| Gejala | Penyebab mungkin | Perbaikan konservatif |
|---|---|---|
| `undefined symbol: memset` | `kernel/lib/memory.c` hilang atau tidak masuk `SRC_C`. | Pastikan `find kernel -name '*.c'` memasukkan file runtime. |
| `relocation R_X86_64_32S ...` | Flag code model/pic salah. | Gunakan `-mcmodel=kernel -fno-pic -fno-pie`. |
| `ld.lld: error: cannot find linker.ld` | Working directory salah. | Jalankan dari root repository. |
| Tidak ada log serial QEMU | QEMU command salah, serial diarahkan ke tempat lain, atau kernel tidak diload. | Jalankan run script M2 dan cek `-serial file:...`. |
| Boot loop/reboot | Triple fault atau bootloader gagal. | Jalankan QEMU dengan `-no-reboot -no-shutdown`, cek serial dan GDB. |
| Kernel kembali dari `kmain` | Tidak ada halt loop. | Pastikan `cpu_halt_forever()` atau `halt_forever()` dipanggil. |

### 7.5 Pemeriksaan OVMF, QEMU, dan ISO tool

```bash
command -v qemu-system-x86_64 || echo "QEMU belum tersedia"
command -v xorriso || echo "xorriso belum tersedia"
find /usr/share -iname 'OVMF_CODE*.fd' -o -iname 'OVMF_VARS*.fd' | sort
```

Jika OVMF tidak ditemukan:

```bash
sudo apt update
sudo apt install -y ovmf qemu-system-x86 xorriso mtools
```

---

## 8. Script Preflight M3

Script ini mengotomatisasi pemeriksaan kesiapan M0/M1/M2. Jalankan sebelum membuat source M3.

#### File `tools/scripts/m3_preflight.sh`

```bash
mkdir -p tools/scripts
cat > tools/scripts/m3_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail

fail() { echo "FAIL: $*" >&2; exit 1; }
warn() { echo "WARN: $*" >&2; }
pass() { echo "PASS: $*"; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || fail "command tidak ditemukan: $1"; }

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT"

echo "[M3 preflight] root=$ROOT"
case "$ROOT" in
  /mnt/c/*|/mnt/d/*|/mnt/e/*) warn "repository berada di filesystem Windows; pindahkan ke ~/mcsos untuk I/O build lebih stabil" ;;
  *) pass "repository berada di filesystem Linux/WSL" ;;
esac

need_cmd git
need_cmd clang
need_cmd ld.lld
need_cmd make
need_cmd readelf
need_cmd objdump
need_cmd nm

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
  pass "QEMU tersedia: $(qemu-system-x86_64 --version | head -n 1)"
else
  warn "qemu-system-x86_64 belum tersedia; build/audit tetap bisa berjalan, run QEMU belum bisa"
fi

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git status --short
else
  warn "direktori ini belum menjadi repository Git"
fi

for path in linker.ld kernel/core/serial.c kernel/lib/memory.c kernel/core/kmain.c; do
  test -e "$path" || fail "artefak M2 hilang: $path"
done

test -d kernel/arch/x86_64/include/mcsos/arch || fail "include arsitektur x86_64 belum ada"

echo "[M3 preflight] compiler=$(clang --version | head -n 1)"
echo "[M3 preflight] linker=$(ld.lld --version | head -n 1)"
pass "preflight M3 selesai"
EOF
```

Setelah file dibuat, aktifkan permission eksekusi.

```bash
chmod +x tools/scripts/m3_preflight.sh
./tools/scripts/m3_preflight.sh
```

Output yang diharapkan minimal menampilkan `PASS: preflight M3 selesai`. Warning tentang QEMU boleh muncul bila mahasiswa hanya sedang melakukan build/audit tanpa runtime test, tetapi M3 belum lulus penuh sampai QEMU test dijalankan.

---

## 9. Source Code M3

Bagian ini berisi source code implementasi M3. Semua file harus dibuat dari root repository. Jangan mencampur implementasi M3 dengan eksperimen M4 seperti IDT/timer sebelum M3 lulus audit.

### 9.1 Header port I/O x86_64

File ini mempertahankan port I/O dari M2. Kontraknya tetap: hanya untuk akses port I/O, bukan MMIO, dan tidak boleh dianggap sebagai mekanisme sinkronisasi antar-thread.

#### File `kernel/arch/x86_64/include/mcsos/arch/io.h`

```bash
mkdir -p kernel/arch/x86_64/include/mcsos/arch
cat > kernel/arch/x86_64/include/mcsos/arch/io.h <<'EOF'
#ifndef MCSOS_ARCH_IO_H
#define MCSOS_ARCH_IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

static inline void io_wait(void) {
    outb(0x80u, 0u);
}

#endif
EOF
```

### 9.2 Header CPU x86_64

File ini menambahkan wrapper kecil untuk `cli`, `hlt`, `pause`, `int3`, dan pembacaan RFLAGS. Wrapper dibuat sebagai inline function agar caller C tidak menulis inline assembly berulang-ulang.

Kontrak penting:

1. `cpu_cli()` mematikan maskable interrupt, tetapi belum mengatur IDT. Pada M3, ini dipakai untuk fail-closed panic/halt.
2. `cpu_hlt()` hanya aman dalam loop terkendali. Jangan memanggil `hlt` lalu membiarkan kernel melanjutkan tanpa rencana wake-up.
3. `cpu_breakpoint()` disediakan untuk eksperimen GDB, tetapi tidak dipanggil default.
4. `cpu_halt_forever()` adalah `noreturn`; fungsi ini adalah akhir eksekusi terkendali M3.

#### File `kernel/arch/x86_64/include/mcsos/arch/cpu.h`

```bash
mkdir -p kernel/arch/x86_64/include/mcsos/arch
cat > kernel/arch/x86_64/include/mcsos/arch/cpu.h <<'EOF'
#ifndef MCSOS_ARCH_CPU_H
#define MCSOS_ARCH_CPU_H

#include <stdint.h>

static inline void cpu_cli(void) {
    __asm__ volatile ("cli" : : : "memory");
}

static inline void cpu_hlt(void) {
    __asm__ volatile ("hlt" : : : "memory");
}

static inline void cpu_pause(void) {
    __asm__ volatile ("pause" : : : "memory");
}

static inline void cpu_breakpoint(void) {
    __asm__ volatile ("int3" : : : "memory");
}

static inline uint64_t cpu_read_rflags(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags) : : "memory");
    return flags;
}

__attribute__((noreturn)) static inline void cpu_halt_forever(void) {
    cpu_cli();
    for (;;) {
        cpu_hlt();
    }
}

#endif
EOF
```

### 9.3 Header versi kernel

Header ini memusatkan identitas kernel agar log serial dan panic output konsisten.

#### File `kernel/include/mcsos/kernel/version.h`

```bash
mkdir -p kernel/include/mcsos/kernel
cat > kernel/include/mcsos/kernel/version.h <<'EOF'
#ifndef MCSOS_KERNEL_VERSION_H
#define MCSOS_KERNEL_VERSION_H

#define MCSOS_NAME "MCSOS"
#define MCSOS_VERSION "260502"
#define MCSOS_MILESTONE "M3"
#define MCSOS_BUILD_PROFILE "teaching-qemu-x86_64"

#endif
EOF
```

### 9.4 Header logging

Logging API dipisahkan dari driver serial agar milestone berikutnya dapat mengganti backend log tanpa mengubah semua caller.

#### File `kernel/include/mcsos/kernel/log.h`

```bash
mkdir -p kernel/include/mcsos/kernel
cat > kernel/include/mcsos/kernel/log.h <<'EOF'
#ifndef MCSOS_KERNEL_LOG_H
#define MCSOS_KERNEL_LOG_H

#include <stdint.h>

void log_init(void);
void log_putc(char c);
void log_write(const char *s);
void log_writeln(const char *s);
void log_hex64(uint64_t value);
void log_key_value_hex64(const char *key, uint64_t value);

#endif
EOF
```

### 9.5 Header panic

Makro `KERNEL_PANIC` dan `KERNEL_ASSERT` menyimpan file dan baris panggilan. Pada kernel freestanding, bentuk ini sederhana tetapi cukup untuk diagnosis awal.

#### File `kernel/include/mcsos/kernel/panic.h`

```bash
mkdir -p kernel/include/mcsos/kernel
cat > kernel/include/mcsos/kernel/panic.h <<'EOF'
#ifndef MCSOS_KERNEL_PANIC_H
#define MCSOS_KERNEL_PANIC_H

#include <stdint.h>

__attribute__((noreturn)) void kernel_panic_at(const char *file, int line, const char *reason, uint64_t code);

#define KERNEL_PANIC(reason, code) kernel_panic_at(__FILE__, __LINE__, (reason), (uint64_t)(code))
#define KERNEL_ASSERT(expr) do { \
    if (!(expr)) { \
        kernel_panic_at(__FILE__, __LINE__, "assertion failed: " #expr, 0xA55E4710u); \
    } \
} while (0)

#endif
EOF
```

### 9.6 Driver serial dengan timeout

M2 memakai busy-wait tanpa timeout. M3 menambahkan batas spin agar panic path tidak dapat terkunci selamanya jika serial line status tidak pernah siap. Timeout ini bukan driver serial final; ini hanya guardrail early boot.

#### File `kernel/core/serial.c`

```bash
mkdir -p kernel/core
cat > kernel/core/serial.c <<'EOF'
#include <stdint.h>
#include <stddef.h>
#include <mcsos/arch/io.h>

#define COM1_PORT 0x3F8u
#define SERIAL_TIMEOUT_LIMIT 100000u

static int serial_transmit_empty(void) {
    return (inb((uint16_t)(COM1_PORT + 5u)) & 0x20u) != 0;
}

void serial_init(void) {
    outb((uint16_t)(COM1_PORT + 1u), 0x00u);
    outb((uint16_t)(COM1_PORT + 3u), 0x80u);
    outb((uint16_t)(COM1_PORT + 0u), 0x03u);
    outb((uint16_t)(COM1_PORT + 1u), 0x00u);
    outb((uint16_t)(COM1_PORT + 3u), 0x03u);
    outb((uint16_t)(COM1_PORT + 2u), 0xC7u);
    outb((uint16_t)(COM1_PORT + 4u), 0x0Bu);
}

void serial_putc(char c) {
    uint32_t spin = 0u;

    if (c == '\n') {
        serial_putc('\r');
    }

    while (!serial_transmit_empty()) {
        if (++spin >= SERIAL_TIMEOUT_LIMIT) {
            return;
        }
    }

    outb((uint16_t)COM1_PORT, (uint8_t)c);
}

void serial_write(const char *s) {
    if (s == (const char *)0) {
        return;
    }
    while (*s != '\0') {
        serial_putc(*s++);
    }
}
EOF
```

### 9.7 Implementasi logging

`log_init()` memanggil `serial_init()`. Fungsi `log_hex64()` mencetak angka 64-bit tanpa `printf`, karena kernel belum memiliki libc.

#### File `kernel/core/log.c`

```bash
mkdir -p kernel/core
cat > kernel/core/log.c <<'EOF'
#include <stdint.h>
#include <mcsos/kernel/log.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);

static int g_log_ready = 0;

void log_init(void) {
    serial_init();
    g_log_ready = 1;
}

void log_putc(char c) {
    if (g_log_ready == 0) {
        serial_init();
        g_log_ready = 1;
    }
    serial_putc(c);
}

void log_write(const char *s) {
    if (g_log_ready == 0) {
        serial_init();
        g_log_ready = 1;
    }
    serial_write(s);
}

void log_writeln(const char *s) {
    log_write(s);
    log_putc('\n');
}

void log_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    log_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uint8_t nibble = (uint8_t)((value >> (uint32_t)shift) & 0x0Fu);
        log_putc(digits[nibble]);
    }
}

void log_key_value_hex64(const char *key, uint64_t value) {
    log_write(key);
    log_write("=");
    log_hex64(value);
    log_putc('\n');
}
EOF
```

### 9.8 Implementasi panic path

Panic path melakukan `cpu_cli()` sebelum mencetak informasi. Informasi yang dicetak: identitas kernel, reason, file/baris, panic code, RFLAGS sebelum `cli`, dan state akhir. Setelah itu kernel masuk halt loop.

Preconditions:

1. Serial/logging boleh belum diinisialisasi; `log_write()` akan menginisialisasi serial jika belum siap.
2. `reason` dan `file` boleh null, tetapi panic tetap harus mencetak fallback string.
3. Tidak ada alokasi memori dinamis.

Postconditions:

1. Fungsi tidak kembali.
2. Interrupt maskable dimatikan.
3. Kernel berhenti terkendali.

#### File `kernel/core/panic.c`

```bash
mkdir -p kernel/core
cat > kernel/core/panic.c <<'EOF'
#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/version.h>

static void log_dec_u32(uint32_t value) {
    char buf[11];
    uint32_t i = 0u;

    if (value == 0u) {
        log_putc('0');
        return;
    }

    while (value != 0u && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i != 0u) {
        log_putc(buf[--i]);
    }
}

__attribute__((noreturn)) void kernel_panic_at(const char *file, int line, const char *reason, uint64_t code) {
    uint64_t rflags = cpu_read_rflags();

    cpu_cli();
    log_writeln("");
    log_writeln("================ MCSOS KERNEL PANIC ================");
    log_write("system=");
    log_write(MCSOS_NAME);
    log_write(" version=");
    log_write(MCSOS_VERSION);
    log_write(" milestone=");
    log_writeln(MCSOS_MILESTONE);
    log_write("reason=");
    log_writeln(reason != (const char *)0 ? reason : "<null>");
    log_write("location=");
    log_write(file != (const char *)0 ? file : "<unknown>");
    log_write(":");
    log_dec_u32((uint32_t)line);
    log_putc('\n');
    log_key_value_hex64("panic_code", code);
    log_key_value_hex64("rflags_before_cli", rflags);
    log_writeln("state=halted");
    log_writeln("====================================================");

    cpu_halt_forever();
}
EOF
```

### 9.9 Runtime memori minimal

File ini dipertahankan dari M2 untuk menutup kemungkinan compiler menghasilkan panggilan runtime memori. Pada M3, fungsi ini tetap sederhana dan wajib bebas libc.

#### File `kernel/lib/memory.c`

```bash
mkdir -p kernel/lib
cat > kernel/lib/memory.c <<'EOF'
#include <stddef.h>

void *memset(void *dest, int value, size_t count) {
    unsigned char *d = (unsigned char *)dest;
    while (count-- != 0u) {
        *d++ = (unsigned char)value;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (count-- != 0u) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || count == 0u) {
        return dest;
    }

    if (d < s) {
        while (count-- != 0u) {
            *d++ = *s++;
        }
    } else {
        d += count;
        s += count;
        while (count-- != 0u) {
            *--d = *--s;
        }
    }

    return dest;
}
EOF
```

### 9.10 Kernel entry M3

`kmain()` menginisialisasi log, mencetak identitas kernel, mencetak alamat start/end, menjalankan selftest ringan, lalu masuk halt normal. Bila `MCSOS_M3_TRIGGER_PANIC` didefinisikan, kernel sengaja memanggil panic untuk menguji jalur fatal.

#### File `kernel/core/kmain.c`

```bash
mkdir -p kernel/core
cat > kernel/core/kmain.c <<'EOF'
#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/version.h>

extern char __kernel_start[];
extern char __kernel_end[];

static void m3_selftest(void) {
    KERNEL_ASSERT(__kernel_end > __kernel_start);
    KERNEL_ASSERT(sizeof(uintptr_t) == 8u);
    log_writeln("[M3] selftest: basic invariants passed");
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
    log_key_value_hex64("rflags", cpu_read_rflags());
    m3_selftest();

#ifdef MCSOS_M3_TRIGGER_PANIC
    KERNEL_PANIC("intentional M3 panic test", 0x4D43534F533033u);
#else
    log_writeln("[M3] panic path installed; intentional panic disabled");
    log_writeln("[M3] ready for QEMU smoke test and GDB audit");
    cpu_halt_forever();
#endif
}
EOF
```

---

## 10. Linker Script M3

Linker script M3 tetap konservatif: higher-half base, entry `kmain`, tiga program header konseptual untuk text/rodata/data, dan symbol `__kernel_start` serta `__kernel_end` untuk audit.

#### File `linker.ld`

```bash
# root file
cat > linker.ld <<'EOF'
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0xffffffff80000000;
    __kernel_start = .;

    .text : ALIGN(4096)
    {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4096)
    {
        *(.rodata .rodata.*)
    } :rodata

    .data : ALIGN(4096)
    {
        *(.data .data.*)
    } :data

    .bss : ALIGN(4096)
    {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    __kernel_end = .;
}
EOF
```

Catatan audit:

1. Jangan mengubah alamat `0xffffffff80000000` tanpa memperbarui laporan dan acceptance criteria.
2. Pastikan `ENTRY(kmain)` cocok dengan simbol di `nm -n build/kernel.elf`.
3. Pastikan `__kernel_end > __kernel_start` melalui selftest M3.
4. Jika `.data` atau `.bss` kosong, interpretasi program header dapat bervariasi; gunakan `readelf -l` untuk bukti aktual.

---

## 11. Makefile M3

Makefile M3 menghasilkan dua kernel:

1. `build/kernel.elf`: varian normal.
2. `build/kernel.panic.elf`: varian intentional panic dengan `-DMCSOS_M3_TRIGGER_PANIC=1`.

Makefile juga menghasilkan `kernel.map`, `kernel.disasm.txt`, `kernel.syms.txt`, dan hasil `readelf`.

#### File `Makefile`

```bash
# root file
cat > Makefile <<'EOF'
.RECIPEPREFIX := >
SHELL := /usr/bin/env bash

BUILD_DIR := build
KERNEL := $(BUILD_DIR)/kernel.elf
PANIC_KERNEL := $(BUILD_DIR)/kernel.panic.elf
MAP := $(BUILD_DIR)/kernel.map
PANIC_MAP := $(BUILD_DIR)/kernel.panic.map
DISASM := $(BUILD_DIR)/kernel.disasm.txt
SYMS := $(BUILD_DIR)/kernel.syms.txt
CC := clang
LD := ld.lld
OBJDUMP := objdump
READELF := readelf
NM := nm

COMMON_CFLAGS := --target=x86_64-unknown-none-elf -std=c17 -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -fno-lto -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror -Ikernel/arch/x86_64/include -Ikernel/include
CFLAGS := $(COMMON_CFLAGS)
PANIC_CFLAGS := $(COMMON_CFLAGS) -DMCSOS_M3_TRIGGER_PANIC=1
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T linker.ld
SRC_C := $(shell find kernel -name '*.c' | LC_ALL=C sort)
OBJ := $(patsubst %.c,$(BUILD_DIR)/normal/%.o,$(SRC_C))
PANIC_OBJ := $(patsubst %.c,$(BUILD_DIR)/panic/%.o,$(SRC_C))

.PHONY: all build panic inspect audit clean distclean

all: build inspect

build: $(KERNEL)

panic: $(PANIC_KERNEL)

$(BUILD_DIR)/normal/%.o: %.c
>mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic/%.o: %.c
>mkdir -p $(dir $@)
>$(CC) $(PANIC_CFLAGS) -c $< -o $@

$(KERNEL): $(OBJ) linker.ld
>mkdir -p $(BUILD_DIR)
>$(LD) $(LDFLAGS) -Map=$(MAP) -o $@ $(OBJ)

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
>grep -q 'kernel_panic_at' $(SYMS)
>grep -q 'cpu_halt_forever' $(DISASM)

# audit memeriksa undefined symbol dan properti ELF yang harus tetap stabil.
audit: inspect panic
>! $(NM) -u $(KERNEL) | grep .
>! $(NM) -u $(PANIC_KERNEL) | grep .
>grep -q 'kernel_panic_at' $(BUILD_DIR)/kernel.disasm.txt
>$(READELF) -S $(KERNEL) | grep -q '.text'
>$(READELF) -S $(KERNEL) | grep -q '.rodata'

clean:
>rm -rf $(BUILD_DIR)

distclean: clean
>rm -rf iso_root limine
EOF
```

Perintah utama:

```bash
make clean
make build
make panic
make inspect
make audit
```

Indikator berhasil:

1. `build/kernel.elf` ada.
2. `build/kernel.panic.elf` ada.
3. `build/kernel.map` ada.
4. `build/kernel.disasm.txt` ada.
5. `make audit` tidak mencetak undefined symbol.

---

## 12. Script Audit ELF dan Disassembly

Script ini memeriksa tipe ELF, machine x86_64, simbol wajib, undefined symbol, dynamic section, dan keberadaan instruksi `cli` serta `hlt`.

#### File `tools/scripts/m3_audit_elf.sh`

```bash
mkdir -p tools/scripts
cat > tools/scripts/m3_audit_elf.sh <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail

KERNEL="${1:-build/kernel.elf}"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

test -f "$KERNEL" || fail "kernel ELF tidak ditemukan: $KERNEL"

readelf -h "$KERNEL" | tee build/m3_audit_readelf_header.txt
readelf -l "$KERNEL" | tee build/m3_audit_readelf_programs.txt
nm -n "$KERNEL" | tee build/m3_audit_symbols.txt >/dev/null
objdump -d -Mintel "$KERNEL" > build/m3_audit_disasm.txt

grep -q 'ELF64' build/m3_audit_readelf_header.txt || fail "bukan ELF64"
grep -q 'Advanced Micro Devices X86-64' build/m3_audit_readelf_header.txt || fail "machine bukan x86-64"
grep -q 'kmain' build/m3_audit_symbols.txt || fail "simbol kmain tidak ditemukan"
grep -q 'kernel_panic_at' build/m3_audit_symbols.txt || fail "simbol kernel_panic_at tidak ditemukan"
if nm -u "$KERNEL" | grep .; then
  fail "masih ada undefined symbol"
fi
if readelf -d "$KERNEL" >/tmp/m3_dynamic.$$ 2>&1 && grep -q 'Dynamic section' /tmp/m3_dynamic.$$; then
  rm -f /tmp/m3_dynamic.$$
  fail "kernel memiliki dynamic section; harus static freestanding"
fi
rm -f /tmp/m3_dynamic.$$

grep -q 'cli' build/m3_audit_disasm.txt || fail "instruksi cli tidak terlihat dalam disassembly"
grep -q 'hlt' build/m3_audit_disasm.txt || fail "instruksi hlt tidak terlihat dalam disassembly"
pass "audit ELF M3 selesai"
EOF
```

Jalankan:

```bash
chmod +x tools/scripts/m3_audit_elf.sh
make build
./tools/scripts/m3_audit_elf.sh build/kernel.elf
```

---

## 13. Script QEMU Smoke Test

Script ini menjalankan ISO hasil M2/M3 dengan QEMU/OVMF dan menyimpan serial log ke file. Dokumentasi QEMU menyatakan `-serial dev` mengalihkan virtual serial port ke host character device [2], [9]. Karena itu M3 menggunakan `-serial file:build/m3_serial.log` agar bukti dapat dilampirkan dalam laporan.

#### File `tools/scripts/m3_qemu_run.sh`

```bash
mkdir -p tools/scripts
cat > tools/scripts/m3_qemu_run.sh <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail

ISO="${1:-build/mcsos.iso}"
LOG="${2:-build/m3_serial.log}"
TIMEOUT_SEC="${MCSOS_QEMU_TIMEOUT:-8}"
OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}"
OVMF_VARS="${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS.fd}"

fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ISO" || fail "ISO tidak ditemukan: $ISO"
command -v qemu-system-x86_64 >/dev/null 2>&1 || fail "qemu-system-x86_64 tidak ditemukan"
test -f "$OVMF_CODE" || fail "OVMF_CODE tidak ditemukan: $OVMF_CODE"
test -f "$OVMF_VARS" || fail "OVMF_VARS tidak ditemukan: $OVMF_VARS"
mkdir -p "$(dirname "$LOG")"
rm -f "$LOG"

timeout "$TIMEOUT_SEC" qemu-system-x86_64 \
  -machine q35 \
  -m 256M \
  -smp 1 \
  -cpu qemu64 \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$OVMF_VARS" \
  -cdrom "$ISO" \
  -boot d \
  -serial file:"$LOG" \
  -display none \
  -no-reboot \
  -no-shutdown || true

cat "$LOG"
grep -q 'MCSOS 260502 M3 kernel entered' "$LOG" || fail "log boot M3 tidak ditemukan"
grep -q '\[M3\] selftest: basic invariants passed' "$LOG" || fail "selftest M3 tidak lulus"
echo "PASS: QEMU smoke test M3 selesai"
EOF
```

Jalankan setelah ISO berhasil dibuat:

```bash
./tools/scripts/m3_qemu_run.sh build/mcsos.iso build/m3_serial.log
```

Indikator berhasil:

```text
MCSOS 260502 M3 kernel entered
[M3] selftest: basic invariants passed
[M3] panic path installed; intentional panic disabled
[M3] ready for QEMU smoke test and GDB audit
```

Jika menggunakan intentional-panic kernel, indikator berhasil adalah munculnya blok:

```text
================ MCSOS KERNEL PANIC ================
reason=intentional M3 panic test
panic_code=...
state=halted
```

---

## 14. Script QEMU Debug dengan GDB Stub

QEMU mendukung debugging guest melalui gdbstub. Dokumentasi QEMU menjelaskan bahwa opsi `-s` membuka gdbserver pada TCP port 1234, sedangkan `-S` membuat guest tidak mulai berjalan sebelum GDB memberi perintah lanjut [3].

#### File `tools/scripts/m3_qemu_debug.sh`

```bash
mkdir -p tools/scripts
cat > tools/scripts/m3_qemu_debug.sh <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail

ISO="${1:-build/mcsos.iso}"
OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}"
OVMF_VARS="${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS.fd}"

test -f "$ISO" || { echo "FAIL: ISO tidak ditemukan: $ISO" >&2; exit 1; }
exec qemu-system-x86_64 \
  -machine q35 \
  -m 256M \
  -smp 1 \
  -cpu qemu64 \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$OVMF_VARS" \
  -cdrom "$ISO" \
  -boot d \
  -serial mon:stdio \
  -display none \
  -no-reboot \
  -no-shutdown \
  -s -S
EOF
```

Jalankan pada terminal pertama:

```bash
./tools/scripts/m3_qemu_debug.sh build/mcsos.iso
```

Lalu pada terminal kedua:

```bash
gdb -x tools/gdb_m3.gdb
```

---

## 15. Script GDB M3

File GDB ini memasang breakpoint pada `kmain` dan `kernel_panic_at`, lalu mencetak register, backtrace, dan disassembly mixed mode bila debug info tersedia.

#### File `tools/gdb_m3.gdb`

```bash
mkdir -p tools
cat > tools/gdb_m3.gdb <<'EOF'
set pagination off
set confirm off
file build/kernel.elf
target remote localhost:1234
break kmain
break kernel_panic_at
continue
info registers
bt
disassemble /m kmain
EOF
```

Jika GDB tidak menemukan simbol:

1. Pastikan `build/kernel.elf` adalah kernel yang sama dengan ISO.
2. Pastikan tidak menjalankan `make clean` setelah membuat ISO tanpa rebuild.
3. Pastikan GDB memakai `file build/kernel.elf`, bukan ISO.
4. Pastikan linker tidak menghapus simbol penting.

---

## 16. Script Pengumpulan Bukti

Script ini mengumpulkan artefak audit ke `evidence/M3`. Direktori ini menjadi lampiran laporan praktikum.

#### File `tools/scripts/m3_collect_evidence.sh`

```bash
mkdir -p tools/scripts
cat > tools/scripts/m3_collect_evidence.sh <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail

OUT="${1:-evidence/M3}"
mkdir -p "$OUT"
cp -v build/kernel.elf "$OUT/" 2>/dev/null || true
cp -v build/kernel.map "$OUT/" 2>/dev/null || true
cp -v build/kernel.readelf.header.txt "$OUT/" 2>/dev/null || true
cp -v build/kernel.readelf.programs.txt "$OUT/" 2>/dev/null || true
cp -v build/kernel.disasm.txt "$OUT/" 2>/dev/null || true
cp -v build/kernel.syms.txt "$OUT/" 2>/dev/null || true
cp -v build/m3_serial.log "$OUT/" 2>/dev/null || true
cp -v build/m3_audit_readelf_header.txt "$OUT/" 2>/dev/null || true
cp -v build/m3_audit_readelf_programs.txt "$OUT/" 2>/dev/null || true
cp -v build/m3_audit_symbols.txt "$OUT/" 2>/dev/null || true
cp -v build/m3_audit_disasm.txt "$OUT/" 2>/dev/null || true
{
  echo "# M3 evidence manifest"
  date -u +"generated_utc=%Y-%m-%dT%H:%M:%SZ"
  git rev-parse HEAD 2>/dev/null | sed 's/^/commit=/g' || true
  clang --version | head -n 1 | sed 's/^/clang=/g'
  ld.lld --version | head -n 1 | sed 's/^/lld=/g'
  qemu-system-x86_64 --version 2>/dev/null | head -n 1 | sed 's/^/qemu=/g' || true
  find "$OUT" -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort
} > "$OUT/manifest.txt"
echo "PASS: evidence tersimpan di $OUT"
EOF
```

Jalankan:

```bash
chmod +x tools/scripts/m3_collect_evidence.sh
./tools/scripts/m3_collect_evidence.sh evidence/M3
```

---

## 17. Script Grading Lokal M3

Script grading lokal memberi skor mekanis awal. Skor ini bukan nilai akhir, tetapi membantu mahasiswa menemukan kegagalan sebelum mengumpulkan laporan.

#### File `tools/scripts/grade_m3.sh`

```bash
mkdir -p tools/scripts
cat > tools/scripts/grade_m3.sh <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail

score=0
failures=0
check() {
  local points="$1"; shift
  local name="$1"; shift
  if "$@"; then
    echo "PASS[$points]: $name"
    score=$((score + points))
  else
    echo "FAIL[$points]: $name" >&2
    failures=$((failures + 1))
  fi
}

check 10 "preflight script valid" bash -n tools/scripts/m3_preflight.sh
check 10 "audit script valid" bash -n tools/scripts/m3_audit_elf.sh
check 20 "normal kernel build" make build
check 10 "panic-test kernel build" make panic
check 20 "ELF/disassembly audit" make audit
check 10 "panic symbol exists" grep -q kernel_panic_at build/kernel.syms.txt
check 10 "no undefined symbols" bash -c '! nm -u build/kernel.elf | grep .'
check 10 "evidence collection" tools/scripts/m3_collect_evidence.sh evidence/M3

echo "SCORE=$score/100"
if [ "$failures" -ne 0 ]; then
  exit 1
fi
EOF
```

Jalankan:

```bash
chmod +x tools/scripts/grade_m3.sh
./tools/scripts/grade_m3.sh
```

---

## 18. Urutan Kerja Langkah demi Langkah

### Langkah 1 — Pastikan repository bersih

Jalankan:

```bash
cd ~/osdev/mcsos
git status --short
```

Jika ada perubahan yang belum dikomit dari M2, commit terlebih dahulu atau buat branch baru. Jangan menimpa source M2 tanpa checkpoint Git.

```bash
git add .
git commit -m "M2 bootable early serial baseline" || true
git switch -c praktikum/m3-panic-debug-audit || git switch praktikum/m3-panic-debug-audit
```

### Langkah 2 — Jalankan preflight M3

```bash
mkdir -p tools/scripts
# buat tools/scripts/m3_preflight.sh dari bagian 8
chmod +x tools/scripts/m3_preflight.sh
./tools/scripts/m3_preflight.sh
```

Jangan lanjut bila script melaporkan artefak M2 hilang. Perbaiki M2 terlebih dahulu.

### Langkah 3 — Tambahkan source M3

Buat semua file pada bagian 9 sampai 17. Disarankan menyalin secara urut agar dependency header tersedia sebelum C file dikompilasi.

### Langkah 4 — Bersihkan build lama

```bash
make clean
```

Pembersihan memastikan kegagalan build tidak tertutup artefak lama. Bila `make clean` gagal, periksa Makefile dan pastikan berada di root repository.

### Langkah 5 — Build varian normal

```bash
make build
```

Artefak yang diharapkan:

```text
build/kernel.elf
build/kernel.map
```

### Langkah 6 — Build varian intentional panic

```bash
make panic
```

Artefak yang diharapkan:

```text
build/kernel.panic.elf
build/kernel.panic.map
```

Varian panic tidak harus menjadi ISO default. Tujuannya adalah membuktikan panic path dapat dikompilasi dan dilink.

### Langkah 7 — Inspeksi ELF dan disassembly

```bash
make inspect
./tools/scripts/m3_audit_elf.sh build/kernel.elf
```

Buka file berikut:

```bash
sed -n '1,80p' build/kernel.readelf.header.txt
sed -n '1,120p' build/kernel.readelf.programs.txt
grep -n 'kmain\|kernel_panic_at\|cpu_halt_forever' build/kernel.syms.txt
grep -n 'cli\|hlt\|int3' build/kernel.disasm.txt | head -20
```

Mahasiswa harus menjelaskan hasil inspeksi pada laporan, bukan sekadar melampirkan screenshot.

### Langkah 8 — Buat ISO seperti M2

Jika Makefile M2 memiliki target `image`, jalankan:

```bash
make image
```

Jika target tersebut belum ada, gunakan script ISO M2. Pastikan kernel yang dimasukkan ke ISO adalah `build/kernel.elf` hasil M3, bukan artefak M2 lama.

Minimal cek timestamp:

```bash
ls -lh build/kernel.elf build/mcsos.iso
stat build/kernel.elf build/mcsos.iso
```

### Langkah 9 — Jalankan QEMU smoke test

```bash
./tools/scripts/m3_qemu_run.sh build/mcsos.iso build/m3_serial.log
```

Bila QEMU timeout tetapi log sudah berisi pesan M3 dan halt normal, itu dapat diterima untuk kernel yang memang berhenti dalam halt loop. Yang tidak dapat diterima adalah log kosong, reboot tanpa pesan, atau panic tanpa reason.

### Langkah 10 — Jalankan GDB debug path

Terminal pertama:

```bash
./tools/scripts/m3_qemu_debug.sh build/mcsos.iso
```

Terminal kedua:

```bash
gdb -x tools/gdb_m3.gdb
```

Bukti minimum:

1. Screenshot atau log GDB menunjukkan breakpoint `kmain` kena.
2. `info registers` menampilkan register.
3. `disassemble /m kmain` atau `disassemble kmain` menampilkan instruksi.
4. Bila memakai intentional-panic ISO, breakpoint `kernel_panic_at` kena.

### Langkah 11 — Jalankan grading lokal

```bash
./tools/scripts/grade_m3.sh
```

Target: `SCORE=100/100` untuk pemeriksaan mekanis. Nilai akhir tetap memakai rubrik dosen.

### Langkah 12 — Kumpulkan evidence

```bash
./tools/scripts/m3_collect_evidence.sh evidence/M3
find evidence/M3 -maxdepth 1 -type f -print | sort
```

### Langkah 13 — Commit hasil M3

```bash
git status --short
git add Makefile linker.ld kernel tools docs evidence/M3 || true
git commit -m "M3 panic path logging gdb and disassembly audit"
```

Jika `evidence/M3` tidak dikomit karena kebijakan repository, tetap lampirkan dalam laporan.

---

## 19. Failure Modes dan Solusi Perbaikan

### 19.1 `kernel_panic_at` tidak ditemukan pada `nm`

Penyebab mungkin:

1. File `panic.c` tidak berada di bawah `kernel/`.
2. `SRC_C := $(shell find kernel -name '*.c')` tidak dijalankan ulang karena Makefile lama.
3. Linker memakai artefak lama.

Perbaikan:

```bash
find kernel -name '*.c' | sort
grep -n 'SRC_C' Makefile
make clean
make build
nm -n build/kernel.elf | grep kernel_panic_at
```

### 19.2 Undefined symbol saat link

Penyebab mungkin: compiler menghasilkan panggilan builtin atau runtime yang belum disediakan.

Perbaikan:

```bash
nm -u build/kernel.elf || true
grep -R "printf\|puts\|malloc\|free" -n kernel || true
make clean
make build
```

Pastikan flag `-ffreestanding -fno-builtin -nostdlib -static` ada.

### 19.3 QEMU log kosong

Penyebab mungkin:

1. ISO masih memakai kernel M2 lama.
2. Serial QEMU diarahkan ke `stdio`, bukan file.
3. OVMF gagal load bootloader.
4. Limine config salah.

Perbaikan:

```bash
stat build/kernel.elf build/mcsos.iso
strings build/kernel.elf | grep 'MCSOS 260502 M3'
./tools/scripts/m3_qemu_run.sh build/mcsos.iso build/m3_serial.log
cat build/m3_serial.log
```

Jika `strings build/kernel.elf` tidak menemukan string M3, source belum terbuild.

### 19.4 Kernel reboot berulang

Penyebab mungkin triple fault atau firmware reset.

Perbaikan:

1. Tambahkan QEMU `-no-reboot -no-shutdown`.
2. Jalankan `-s -S` dan GDB.
3. Cek entry symbol:

```bash
readelf -h build/kernel.elf | grep 'Entry point'
nm -n build/kernel.elf | grep kmain
```

### 19.5 GDB tidak connect

Penyebab mungkin QEMU tidak berjalan dengan `-s -S`, port 1234 dipakai proses lain, atau firewall/namespace WSL bermasalah.

Perbaikan:

```bash
ss -ltnp | grep 1234 || true
pkill -f qemu-system-x86_64 || true
./tools/scripts/m3_qemu_debug.sh build/mcsos.iso
```

Di terminal kedua:

```bash
gdb build/kernel.elf
(gdb) target remote localhost:1234
```

### 19.6 Panic mencetak sebagian lalu berhenti

Penyebab mungkin timeout serial atau QEMU serial device tidak siap.

Perbaikan:

1. Pastikan script QEMU memakai `-serial file:build/m3_serial.log` atau `-serial mon:stdio`.
2. Naikkan `SERIAL_TIMEOUT_LIMIT` hanya untuk diagnosis, lalu dokumentasikan.
3. Jangan menghapus timeout permanen tanpa alasan; timeout mencegah panic path hang selamanya.

### 19.7 Disassembly tidak menampilkan `cli` atau `hlt`

Penyebab mungkin fungsi inline tidak terpakai atau optimizer mengubah layout.

Perbaikan:

```bash
grep -R "cpu_halt_forever\|cpu_cli\|cpu_hlt" -n kernel
objdump -d -Mintel build/kernel.elf | grep -n "cli\|hlt" | head
```

Pastikan `kmain()` atau `kernel_panic_at()` benar-benar memanggil `cpu_halt_forever()`.

### 19.8 `readelf -d` menunjukkan dynamic section

Penyebab: link tidak memakai `-nostdlib -static`, atau memakai compiler driver tanpa konfigurasi tepat.

Perbaikan:

```bash
grep -n 'LDFLAGS' Makefile
make clean
make build
readelf -d build/kernel.elf || true
```

LDFLAGS harus memuat `-nostdlib -static -T linker.ld`.

---

## 20. Checkpoint Buildable

| Checkpoint | Perintah | Artefak | Lulus bila |
|---|---|---|---|
| C0 | `./tools/scripts/m3_preflight.sh` | output preflight | Tidak ada FAIL. |
| C1 | `make build` | `build/kernel.elf`, `build/kernel.map` | ELF berhasil dibuat. |
| C2 | `make panic` | `build/kernel.panic.elf` | Varian intentional panic berhasil dibuat. |
| C3 | `make inspect` | `readelf`, `nm`, `objdump` output | Simbol wajib ditemukan. |
| C4 | `make audit` | audit terminal | Tidak ada undefined symbol. |
| C5 | `./tools/scripts/m3_qemu_run.sh` | `build/m3_serial.log` | Log M3 muncul. |
| C6 | GDB session | screenshot/log GDB | Breakpoint `kmain` atau `kernel_panic_at` valid. |
| C7 | `./tools/scripts/m3_collect_evidence.sh` | `evidence/M3` | Manifest dan artefak debug terkumpul. |

---

## 21. Tugas Implementasi Mahasiswa

### 21.1 Tugas wajib

1. Jalankan preflight M3 dan perbaiki semua error dari M0/M1/M2.
2. Implementasikan seluruh source code M3 sesuai panduan.
3. Build normal kernel dan intentional-panic kernel.
4. Jalankan ELF/disassembly audit.
5. Jalankan QEMU smoke test normal kernel.
6. Jalankan GDB session minimal sampai breakpoint `kmain`.
7. Kumpulkan evidence ke `evidence/M3`.
8. Isi laporan praktikum memakai template laporan standar.

### 21.2 Tugas pengayaan

1. Tambahkan `log_hex32()` dan `log_dec_u64()` tanpa memakai libc.
2. Tambahkan build profile `debug` dengan `-g` dan `-O0`, serta profile `audit` dengan `-O2`.
3. Tambahkan target `make size` untuk mencetak ukuran section.
4. Tambahkan audit untuk memastikan tidak ada simbol `printf`, `malloc`, `free`, atau dependency libc.

### 21.3 Tantangan riset

1. Rancang format panic record yang kelak dapat disimpan ke ring buffer setelah allocator tersedia.
2. Bandingkan output disassembly `-O0`, `-O1`, dan `-O2` untuk `kernel_panic_at()`.
3. Rancang proof obligation untuk memastikan semua jalur fatal berakhir di `kernel_panic_at()` atau `cpu_halt_forever()`.

---

## 22. Bukti yang Harus Dikumpulkan

Minimal bukti:

1. `git log --oneline -5`.
2. Output `./tools/scripts/m3_preflight.sh`.
3. Output `make clean && make build && make panic && make audit`.
4. `build/kernel.map`.
5. `build/kernel.readelf.header.txt`.
6. `build/kernel.readelf.programs.txt`.
7. `build/kernel.syms.txt`.
8. Potongan `build/kernel.disasm.txt` yang menunjukkan `kmain`, `kernel_panic_at`, `cli`, dan `hlt`.
9. `build/m3_serial.log` dari QEMU normal kernel.
10. Screenshot/log GDB pada breakpoint `kmain`.
11. Jika intentional panic diuji, log panic yang memuat reason, location, panic code, dan state halted.
12. `evidence/M3/manifest.txt`.

---

## 23. Pertanyaan Analisis

1. Mengapa M3 tidak langsung mengimplementasikan IDT dan timer meskipun sudah memakai `cli` dan `hlt`?
2. Apa perbedaan controlled halt, panic, hang, dan triple fault?
3. Mengapa panic path harus `noreturn`?
4. Mengapa `serial_putc()` diberi timeout pada M3?
5. Mengapa kernel freestanding tidak boleh memakai `printf` dari libc host?
6. Apa risiko menggunakan `__FILE__` di panic path dari sisi informasi build path?
7. Bagaimana cara membuktikan bahwa kernel ELF tidak memiliki dynamic dependency?
8. Apa hubungan linker map dengan debugging bug boot awal?
9. Mengapa GDB harus memakai `build/kernel.elf`, bukan ISO?
10. Apa acceptance criteria sebelum M3 boleh menjadi fondasi M4?

---

## 24. Rubrik Penilaian 100 Poin

| Komponen | Poin | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | Kernel normal dan panic variant build; panic path mencetak bukti; halt loop terkendali. |
| Kualitas desain dan invariants | 20 | API logging/panic jelas; kontrak `noreturn`, no-libc, no-allocation, fail-closed terdokumentasi. |
| Pengujian dan bukti | 20 | `make audit`, QEMU log, GDB evidence, readelf/nm/objdump/linker map lengkap. |
| Debugging/failure analysis | 10 | Laporan mampu mendiagnosis minimal lima failure modes M3. |
| Keamanan dan robustness | 10 | Panic tidak kembali; serial timeout; tidak ada dynamic dependency; debug output dievaluasi risikonya. |
| Dokumentasi/laporan | 10 | Laporan rapi, berisi commit hash, environment, screenshot/log, analisis, dan referensi. |

---

## 25. Kriteria Lulus Praktikum

Mahasiswa dinyatakan lulus M3 bila memenuhi semua syarat berikut.

1. Repository dapat dibangun dari clean checkout.
2. Perintah build terdokumentasi dan dapat diulang.
3. `make build`, `make panic`, dan `make audit` berhasil.
4. Tidak ada undefined symbol pada kernel ELF.
5. Linker map dan disassembly tersedia.
6. QEMU smoke test menghasilkan log M3 yang deterministik.
7. Panic path dapat diuji melalui intentional-panic kernel atau breakpoint GDB.
8. GDB dapat connect ke QEMU gdbstub dan mencapai breakpoint `kmain`.
9. Panic path tidak kembali ke caller.
10. Mahasiswa menjelaskan minimal lima failure modes dan perbaikannya.
11. Laporan memuat evidence yang cukup.
12. Perubahan Git terkomit atau terdokumentasi.

---

## 26. Prosedur Rollback

Rollback standar bila M3 merusak boot M2:

```bash
git status --short
git switch praktikum/m2-boot-baseline || true
git log --oneline -5
```

Jika M3 sudah berada pada branch sendiri:

```bash
git switch praktikum/m3-panic-debug-audit
git restore --source praktikum/m2-boot-baseline -- linker.ld kernel/core/kmain.c kernel/core/serial.c kernel/lib/memory.c
make clean
make build
```

Rollback parsial bila hanya Makefile rusak:

```bash
git checkout HEAD~1 -- Makefile
make clean
make build
```

Rollback tidak boleh dilakukan dengan menghapus repository penuh sebelum evidence kegagalan disalin. Simpan minimal:

```bash
mkdir -p evidence/failure-M3
cp -a build/*.log build/*.txt build/*.map evidence/failure-M3/ 2>/dev/null || true
git diff > evidence/failure-M3/diff.patch || true
```

---

## 27. Template Laporan Praktikum M3

Gunakan template laporan standar `os_template_laporan_praktikum.md`. Untuk M3, isi bagian khusus berikut.

1. **Sampul:** judul Praktikum M3, nama mahasiswa/kelompok, NIM, kelas, dosen Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.
2. **Tujuan:** jelaskan panic path, logging, GDB, linker map, dan disassembly audit.
3. **Dasar teori:** freestanding C, panic path, x86_64 halt/interrupt masking, linker script, QEMU gdbstub.
4. **Lingkungan:** Windows build, WSL distro, clang/lld/binutils/QEMU/GDB/OVMF version, commit hash.
5. **Desain:** state machine M3, invariants, file layout, API logging/panic.
6. **Langkah kerja:** perintah dan alasan teknis untuk setiap langkah.
7. **Hasil uji:** output build, audit ELF, QEMU log, GDB screenshot/log, evidence manifest.
8. **Analisis:** penyebab keberhasilan, bug yang ditemukan, failure modes, dan perbandingan dengan teori.
9. **Keamanan dan reliability:** fail-closed panic, serial timeout, debug information risk, no dynamic dependency.
10. **Kesimpulan:** status readiness M3 dan rencana M4.
11. **Lampiran:** kode penting, diff ringkas, log penuh, dan referensi.

---

## 28. Readiness Review Akhir M3

| Gate | Status yang harus dibuktikan | Evidence |
|---|---|---|
| Gate M3-0 | M0/M1/M2 preflight lulus atau warning terdokumentasi. | Output `m3_preflight.sh`. |
| Gate M3-1 | Kernel normal build. | `build/kernel.elf`. |
| Gate M3-2 | Panic variant build. | `build/kernel.panic.elf`. |
| Gate M3-3 | ELF audit lulus. | Output `make audit` dan `m3_audit_elf.sh`. |
| Gate M3-4 | Panic/logging symbol tersedia. | `nm`, `objdump`. |
| Gate M3-5 | QEMU smoke test berjalan. | `build/m3_serial.log`. |
| Gate M3-6 | GDB workflow valid. | Screenshot/log GDB. |
| Gate M3-7 | Evidence terkumpul. | `evidence/M3/manifest.txt`. |

Keputusan readiness yang diperbolehkan:

1. **Belum siap M4** bila build/audit gagal.
2. **Siap audit ulang M3** bila build lulus tetapi QEMU/GDB belum lulus.
3. **Siap uji QEMU dan siap lanjut M4 secara terbatas** bila build, audit, QEMU log, GDB, dan laporan evidence lengkap.

Tidak boleh menyatakan “sistem operasi sudah sempurna” atau “tanpa error”. M3 hanya membuktikan fondasi observability awal dan panic path untuk melanjutkan ke M4.

---

## 29. Catatan Verifikasi Source oleh Penyusun Panduan

Source code inti M3 telah diuji secara lokal dengan hasil ringkas:

```text
clang: clang version 17.0.0
lld: LLD 17.0.0
binutils readelf/objdump: GNU Binutils 2.44
make audit: lulus
local grade_m3.sh: SCORE=100/100
```

Batasan verifikasi: lingkungan penyusun tidak memiliki `qemu-system-x86_64`, sehingga validasi runtime boot ISO, serial log QEMU, dan sesi GDB harus dilakukan di lingkungan WSL 2 mahasiswa. Panduan menyediakan script untuk menjalankan validasi tersebut secara deterministik.

---

## 30. References

[1] Microsoft, “Install WSL,” *Microsoft Learn*. Accessed: 2026-05-02. [Online]. Available: https://learn.microsoft.com/windows/wsl/install

[2] QEMU Project, “System Emulation — Introduction,” *QEMU documentation*. Accessed: 2026-05-02. [Online]. Available: https://www.qemu.org/docs/master/system/introduction.html

[3] QEMU Project, “GDB usage,” *QEMU documentation*. Accessed: 2026-05-02. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html

[4] Intel Corporation, “Intel® 64 and IA-32 Architectures Software Developer’s Manuals,” Intel Developer Documentation. Accessed: 2026-05-02. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

[5] LLVM Project, “Clang command line argument reference,” *Clang documentation*. Accessed: 2026-05-02. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html

[6] LLVM Project, “LLD — The LLVM Linker,” *LLD documentation*. Accessed: 2026-05-02. [Online]. Available: https://lld.llvm.org/

[7] GNU Binutils Project, “LD — Linker Scripts,” *GNU Binutils documentation*. Accessed: 2026-05-02. [Online]. Available: https://sourceware.org/binutils/docs/ld/Scripts.html

[8] Limine Project, “Limine,” *Limine Bootloader*. Accessed: 2026-05-02. [Online]. Available: https://limine-bootloader.org/

[9] QEMU Project, “Invocation,” *QEMU documentation*. Accessed: 2026-05-02. [Online]. Available: https://www.qemu.org/docs/master/system/invocation.html
