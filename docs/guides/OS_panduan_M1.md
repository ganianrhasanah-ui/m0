# Panduan Praktikum M1 - Toolchain Reproducible dan Pemeriksaan Kesiapan Lingkungan Pengembangan MCSOS 260502

**Mata kuliah:** Sistem Operasi Lanjut / Praktikum Sistem Operasi  
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi:** Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia  
**Target praktikum:** MCSOS versi 260502  
**Target arsitektur:** x86_64  
**Host pengembangan:** Windows 11 x64 dengan WSL 2 Linux environment  
**Model kernel awal:** kernel monolitik pendidikan dengan boundary modular dan POSIX-like subset  
**Bahasa utama tahap awal:** freestanding C17, assembly x86_64 minimal, script Bash/Python untuk tooling  
**Status keluaran:** siap validasi lingkungan dan siap menjadi prasyarat M2, bukan siap boot kernel  

---

## 0. Ringkasan Praktikum

Praktikum M1 berfokus pada validasi kesiapan lingkungan pengembangan dan pembuatan toolchain baseline yang dapat direproduksi. Pada tahap ini mahasiswa belum diminta membuat kernel bootable. Keluaran utama M1 adalah repository MCSOS yang memiliki struktur awal, Makefile minimum, script pemeriksaan toolchain, metadata versi, bukti kompilasi freestanding object untuk target x86_64 ELF, bukti inspeksi ELF, bukti kesiapan QEMU/OVMF/GDB, dan dokumen readiness M1.

M1 adalah gate kritis. Kesalahan pada tahap ini dapat menyebabkan semua praktikum berikutnya tampak gagal padahal sumber masalahnya adalah compiler host yang salah, linker yang memakai ABI hosted, repository yang ditempatkan di filesystem Windows, OVMF tidak tersedia, QEMU tidak dapat berjalan, atau build tidak dapat diulang dari clean checkout. Karena itu M1 tidak dinilai dari banyaknya kode kernel, tetapi dari ketepatan bukti bahwa lingkungan build sudah terkendali, terukur, dan dapat diaudit.

Klaim yang diperbolehkan setelah M1 lulus adalah: **lingkungan siap untuk praktikum M2 dan siap menghasilkan artefak toolchain proof**. M1 tidak boleh diklaim sebagai bukti bahwa MCSOS sudah boot, sudah stabil, atau bebas error.

---

## 1. Capaian Pembelajaran

Setelah menyelesaikan praktikum ini, mahasiswa mampu:

1. Menjelaskan mengapa pengembangan kernel memerlukan toolchain freestanding dan tidak boleh bergantung pada hosted libc.
2. Mengonfigurasi Windows 11 x64, WSL 2, dan repository Linux filesystem agar cocok untuk pengembangan MCSOS.
3. Memasang dan memverifikasi tool build utama: Git, Make, CMake, Ninja, Clang/LLVM, LLD, Binutils, NASM, QEMU, OVMF, GDB, Python, ShellCheck, Cppcheck, dan Clang-Tidy.
4. Membuat script pemeriksaan toolchain yang dapat dijalankan ulang secara deterministik.
5. Menghasilkan metadata versi toolchain sebagai evidence reproduksi build.
6. Mengompilasi source C kecil menjadi object freestanding target x86_64 ELF dan memeriksa hasilnya dengan `readelf`, `objdump`, dan `nm`.
7. Menjelaskan failure modes umum pada toolchain OSDev: salah target triple, red zone aktif, linker memakai startup object host, undefined symbol runtime, path berada di `/mnt/c`, dan QEMU/OVMF tidak siap.
8. Menyusun readiness review M1 dengan bukti yang dapat diperiksa.

---

## 2. Prasyarat Teori

Mahasiswa harus memahami konsep berikut sebelum mengerjakan M1:

| Konsep | Keterangan ringkas | Relevansi M1 |
|---|---|---|
| Hosted vs freestanding | Hosted C berjalan di atas OS dan libc, sedangkan freestanding tidak mengasumsikan fasilitas OS. | Kernel MCSOS tidak boleh bergantung pada libc host. |
| Target triple | Identitas arsitektur, vendor, sistem, dan ABI target. | Menghindari kompilasi tidak sengaja untuk Linux/Windows host. |
| ELF object | Format object/executable yang akan dipakai kernel tahap awal. | Harus dapat diperiksa dengan `readelf` dan `objdump`. |
| Linker dan startup object | Hosted program biasa memakai startup object dan library default. | Kernel harus mengontrol entry dan linking sendiri. |
| Red zone x86_64 | Area 128 byte di bawah RSP yang dapat dipakai ABI userland, tetapi berbahaya untuk kernel/interrupt. | Kernel harus memakai `-mno-red-zone`. |
| QEMU dan OVMF | Emulator dan firmware UEFI untuk uji OS tanpa hardware fisik. | M2 bergantung pada jalur ini. |
| Reproducible build | Build dapat diulang dari clean checkout dengan metadata dan output yang dapat dibandingkan. | Syarat readiness M1. |

---

## 3. Peta Skill yang Digunakan

| Skill | Fokus pada M1 | Artefak bukti |
|---|---|---|
| osdev-general | Gate, milestone, readiness, acceptance criteria | `docs/readiness/M1-toolchain.md` |
| osdev-01-computer-foundation | Invariants, state machine, proof obligation | `docs/architecture/invariants.md` |
| osdev-02-low-level-programming | Freestanding C, ABI, red zone, ELF, linker checks | `build/proof/*.o`, `readelf`, `objdump`, `nm` |
| osdev-03-computer-and-hardware-architecture | Target x86_64, QEMU machine, OVMF, CPU capability baseline | `build/meta/qemu-capabilities.txt` |
| osdev-04-kernel-development | Kernel stage boundary dan panic path planning | `docs/architecture/kernel_stage.md` |
| osdev-05-filesystem-development | Policy path dan generated artifact | `.gitignore`, `build/` tidak dikomit |
| osdev-06-networking-stack | Tidak mengimplementasi jaringan, hanya memastikan future QEMU network tidak mengganggu M1 | catatan non-goal |
| osdev-07-os-security | Supply-chain, trust boundary, command provenance | `docs/security/toolchain_threat_model.md` |
| osdev-08-device-driver-development | Tidak mengikat driver, hanya memeriksa emulator/device model | `qemu-system-x86_64 -machine help` |
| osdev-09-virtualization-and-containerization | WSL 2 dan QEMU sebagai virtualized development path | `wsl --list --verbose`, QEMU probe |
| osdev-10-boot-firmware | OVMF dan jalur UEFI sebagai prasyarat M2 | path OVMF tervalidasi |
| osdev-11-graphics-display | Belum ada GUI, hanya memastikan tidak bergantung pada display | QEMU headless readiness |
| osdev-12-toolchain-devenv | Toolchain BOM, build system, debugging, reproducibility | `build/meta/toolchain-versions.txt` |
| osdev-13-enterprise-features | Observability, runbook, rollback | `docs/readiness/M1-toolchain.md` |
| osdev-14-cross-science | Requirements, risk register, verification matrix | `docs/testing/verification_matrix.md` |

---

## 4. Alat dan Versi Minimum

Versi aktual tidak dipaksa seragam secara mutlak karena paket WSL/Ubuntu dapat berubah. Namun setiap kelompok wajib mencatat versi aktual pada evidence. Dosen dapat menetapkan versi pinning tambahan apabila praktikum berjalan pada laboratorium terkontrol.

| Tool | Peran | Versi minimum praktis | Bukti yang harus disimpan |
|---|---|---:|---|
| Windows 11 x64 | Host administratif | Windows 11 | screenshot atau output `systeminfo` ringkas |
| WSL 2 | Linux build environment | WSL 2 | `wsl --version`, `wsl --list --verbose` |
| Ubuntu/Debian WSL | Distribusi Linux | Ubuntu LTS/Debian stable | `lsb_release -a` atau `/etc/os-release` |
| Git | Version control | 2.x | `git --version` |
| GNU Make | Orkestrasi praktikum | 4.x | `make --version` |
| CMake | Generator build opsional | 3.x | `cmake --version` |
| Ninja | Build executor opsional | 1.x | `ninja --version` |
| Clang/LLVM | Compiler cross-capable | 16+ direkomendasikan | `clang --version`, `ld.lld --version` |
| GCC | Compiler host dan fallback | 12+ direkomendasikan | `gcc --version` |
| Binutils | `readelf`, `objdump`, `nm`, `objcopy` | 2.40+ direkomendasikan | `readelf --version`, `objdump --version` |
| NASM | Assembly x86_64 | 2.15+ | `nasm -v` |
| QEMU | Emulator system x86_64 | 8.x+ direkomendasikan | `qemu-system-x86_64 --version` |
| OVMF | Firmware UEFI untuk QEMU | paket distro | path file firmware |
| GDB | Debugger | 13+ direkomendasikan | `gdb --version` |
| Python 3 | Script test dan tooling | 3.10+ | `python3 --version` |
| ShellCheck | Lint shell script | distro package | `shellcheck --version` |
| Cppcheck | Static analysis C/C++ | distro package | `cppcheck --version` |
| Clang-Tidy | Static analysis LLVM | sesuai clang | `clang-tidy --version` |

---

## 5. Repository Awal

Gunakan repository dari M0. Jika belum ada, buat repository baru dengan struktur berikut. Repository harus berada di filesystem Linux WSL, misalnya `~/src/mcsos`, bukan di `/mnt/c/...`.

```text
mcsos/
  README.md
  LICENSE
  Makefile
  .gitignore
  docs/
    architecture/
    readiness/
    security/
    testing/
  tools/
    scripts/
  tests/
    toolchain/
  build/                  # generated, tidak dikomit
```

Alasan teknis: filesystem Linux WSL mengurangi risiko masalah case sensitivity, executable bit, permission bit, newline conversion, symlink, dan performa I/O. Praktikum kernel sangat sensitif terhadap detail tersebut.

---

## 6. Target Praktikum M1

Target M1 adalah menghasilkan artefak berikut:

1. `build/meta/toolchain-versions.txt`
2. `build/meta/host-readiness.txt`
3. `build/meta/qemu-capabilities.txt`
4. `build/proof/freestanding_probe.o`
5. `build/proof/freestanding_probe.elf`
6. `build/proof/readelf-header.txt`
7. `build/proof/readelf-sections.txt`
8. `build/proof/objdump-disassembly.txt`
9. `build/proof/nm-undefined.txt`
10. `tools/scripts/check_toolchain.sh`
11. `tools/scripts/collect_meta.sh`
12. `tools/scripts/proof_compile.sh`
13. `tools/scripts/qemu_probe.sh`
14. `tools/scripts/repro_check.sh`
15. `docs/readiness/M1-toolchain.md`
16. Commit Git dengan pesan `M1: add reproducible toolchain readiness baseline`

---

## 7. Konsep Inti M1

### 7.1 Prinsip Buildable-First

Setiap tahap MCSOS harus dapat dijalankan dari clean checkout. M1 menetapkan mekanisme `make check`, `make meta`, `make proof`, dan `make test` sebagai kontrak awal. Jika kontrak ini gagal, M2 tidak boleh dimulai.

### 7.2 Prinsip Evidence-First

Fitur dianggap selesai hanya bila ada evidence. Pada M1, evidence tidak berupa screenshot saja. Evidence utama adalah output tekstual yang dapat diperiksa: versi toolchain, hasil inspeksi ELF, daftar undefined symbol, dan status readiness.

### 7.3 Prinsip Freestanding Runtime

Kernel tidak boleh mengandalkan `main`, `crt0`, libc host, dynamic linker, exception runtime, atau startup file host. M1 memverifikasi kompilasi freestanding kecil agar mahasiswa memahami batas ini sebelum menulis kernel sebenarnya.

### 7.4 Prinsip Toolchain sebagai Trusted Computing Base

Compiler, linker, assembler, emulator, debugger, dan script build adalah bagian dari trust boundary. Kesalahan konfigurasi toolchain dapat menghasilkan binary yang tampak valid tetapi salah ABI, salah format, atau tidak sesuai target kernel.

---

## 8. Arsitektur Ringkas Lingkungan M1

```text
Windows 11 x64
  |
  | PowerShell Admin: install dan verifikasi WSL 2
  v
WSL 2 Linux Distribution
  |
  | repository berada di ~/src/mcsos
  v
MCSOS Repository
  |
  | Makefile memanggil script validasi
  v
Toolchain Proof
  |-- clang --target=x86_64-unknown-elf
  |-- ld.lld -m elf_x86_64
  |-- readelf / objdump / nm
  |-- qemu-system-x86_64 probe
  |-- gdb readiness probe
  v
Evidence di build/meta dan build/proof
```

---

---

## 8A. Assumptions and Target

Bagian ini merangkum asumsi formal M1 agar dokumen dapat diaudit sebagai engineering gate. Target arsitektur adalah x86_64, host adalah Windows 11 x64, lingkungan build adalah WSL 2 Linux filesystem, compiler utama adalah Clang/LLVM dengan target `x86_64-unknown-elf`, linker utama adalah LLD, emulator utama adalah QEMU system x86_64, dan firmware emulator utama adalah OVMF. M1 belum membuat boot image, belum menjalankan kernel, dan belum mengaktifkan interrupt, paging, syscall, filesystem, driver, networking, grafis, virtualisasi, atau userspace MCSOS.

## 8B. Goals and Non-goals

Tujuan M1 adalah membuktikan kesiapan lingkungan dan toolchain secara reproducible melalui Makefile, script pemeriksaan, metadata versi, proof object, proof ELF, inspeksi ELF, QEMU/OVMF probe, dan reproducibility hash. Non-goals M1 adalah membuat bootloader, membuat kernel entry, membuat linker script final, boot di QEMU, menjalankan GDB pada kernel, mengimplementasikan syscall, menjalankan userspace, dan mengklaim stabilitas sistem operasi.

## 8C. Architecture and Design

Desain M1 memakai pola satu repository dengan satu antarmuka build utama, yaitu Makefile. Makefile memanggil script di `tools/scripts/`, source proof berada di `tests/toolchain/`, dokumentasi berada di `docs/`, dan semua artefak generated berada di `build/`. Pembagian ini mencegah pencampuran source dengan output build dan membuat setiap evidence dapat ditelusuri ke perintah yang membuatnya.

## 8D. Interfaces and ABI

Interface utama M1 adalah command-line contract: `make meta`, `make check`, `make proof`, `make qemu-probe`, `make repro`, dan `make test`. ABI proof yang diuji adalah ELF64 untuk x86_64 dengan fungsi entry simbolik `mcsos_toolchain_probe`. Proof ini tidak mendefinisikan ABI kernel final; ia hanya membuktikan bahwa toolchain dapat menghasilkan object dan ELF freestanding tanpa unresolved hosted runtime symbol.

## 8E. Testing Validation and Verification

Validasi M1 dilakukan dengan pemeriksaan tool, inspeksi object, inspeksi ELF, pemeriksaan undefined symbol, pemeriksaan QEMU/OVMF, dan pemeriksaan reproduksi hash. Verifikasi minimum dinyatakan lulus hanya bila `make test` berhasil dari kondisi `make distclean`, `nm-undefined.txt` kosong, `readelf` menunjukkan target x86_64 ELF, dan readiness review diisi berdasarkan evidence, bukan asumsi.

## 9. Instruksi Langkah demi Langkah

### Langkah 1 - Verifikasi Windows dan WSL dari PowerShell

Langkah ini dijalankan di PowerShell Windows. Tujuannya adalah memastikan WSL tersedia, distribusi Linux dapat dipasang, dan distribusi berjalan sebagai WSL 2. Jika tahap ini gagal, jangan lanjut ke shell Linux karena seluruh alur M1 bergantung pada WSL 2.

```powershell
wsl --version
wsl --status
wsl --list --verbose
```

Indikator hasil yang benar:

1. Perintah `wsl --version` menampilkan versi WSL.
2. `wsl --list --verbose` menampilkan distribusi Linux dengan kolom `VERSION` bernilai `2`.
3. Jika distribusi belum ada, pasang distribusi melalui perintah berikut.

```powershell
wsl --list --online
wsl --install -d Ubuntu
```

Jika WSL baru dipasang, restart Windows sebelum melanjutkan. Setelah restart, jalankan kembali pemeriksaan `wsl --list --verbose`.

### Langkah 2 - Buat atau periksa `.wslconfig`

Langkah ini mengatur resource global WSL 2. File `.wslconfig` berada di direktori profil Windows, misalnya `C:\Users\NamaUser\.wslconfig`. Konfigurasi ini membantu menghindari kondisi WSL kekurangan RAM atau CPU saat kompilasi dan pengujian QEMU. Nilai berikut adalah baseline; dosen dapat menyesuaikan dengan hardware laboratorium.

```ini
[wsl2]
memory=12GB
processors=6
swap=8GB
localhostForwarding=true
nestedVirtualization=true

[experimental]
autoMemoryReclaim=gradual
```

Setelah mengubah file, matikan VM WSL agar konfigurasi dibaca ulang.

```powershell
wsl --shutdown
wsl --list --verbose
```

Catatan teknis: jangan menetapkan `memory` lebih besar daripada RAM fisik yang aman. Untuk laptop 16 GB, nilai 8 GB sampai 12 GB biasanya lebih realistis. Untuk PC 32 GB, nilai 16 GB dapat digunakan. Catat konfigurasi aktual pada laporan.

### Langkah 3 - Masuk ke WSL dan validasi distribusi Linux

Langkah ini dijalankan di terminal Linux WSL. Tujuannya adalah mengumpulkan informasi OS Linux, kernel WSL, CPU, memori, dan path kerja.

```bash
cat /etc/os-release
uname -a
nproc
free -h
pwd
```

Indikator hasil yang benar:

1. Distribusi Linux terdeteksi.
2. `nproc` menunjukkan jumlah vCPU WSL yang masuk akal.
3. `free -h` menunjukkan memori sesuai `.wslconfig` atau default WSL.
4. `pwd` tidak berada di `/mnt/c`, `/mnt/d`, atau mount Windows lain untuk repository MCSOS.

### Langkah 4 - Buat direktori kerja di filesystem Linux WSL

Perintah berikut membuat direktori kerja yang aman untuk repository MCSOS. Gunakan `~/src` agar mudah diaudit.

```bash
mkdir -p ~/src
cd ~/src
```

Jika repository sudah ada dari M0, masuk ke repository tersebut.

```bash
cd ~/src/mcsos
```

Jika repository belum ada, buat repository baru.

```bash
mkdir -p ~/src/mcsos
cd ~/src/mcsos
git init
```

Validasi bahwa path repository tidak berada di filesystem Windows.

```bash
case "$PWD" in
  /mnt/*)
    echo "ERROR: repository berada di mount Windows: $PWD" >&2
    exit 1
    ;;
  *)
    echo "OK: repository berada di filesystem Linux WSL: $PWD"
    ;;
esac
```

### Langkah 5 - Pasang paket toolchain dasar

Perintah berikut memasang paket yang dibutuhkan M1. Jalankan dengan koneksi internet aktif. Paket `ovmf` menyediakan firmware UEFI untuk QEMU, sedangkan `qemu-system-x86` menyediakan emulator target x86_64.

```bash
sudo apt update
sudo apt install -y \
  build-essential git make cmake ninja-build pkg-config \
  clang lld llvm binutils nasm \
  qemu-system-x86 qemu-utils ovmf \
  gdb gdb-multiarch \
  python3 python3-pip python3-venv \
  shellcheck cppcheck clang-tidy \
  xorriso mtools dosfstools file coreutils findutils
```

Jika paket tertentu tidak tersedia karena versi distribusi berbeda, catat nama paket pengganti pada laporan dan minta validasi dosen/asisten. Jangan mengganti compiler atau emulator tanpa mencatat alasan teknis.

### Langkah 6 - Buat struktur repository M1

Langkah ini membuat direktori yang dipakai untuk script, test, dokumentasi, dan evidence. Direktori `build` sengaja dibuat sebagai generated output dan tidak dikomit.

```bash
mkdir -p \
  docs/architecture \
  docs/readiness \
  docs/security \
  docs/testing \
  tools/scripts \
  tests/toolchain \
  build/meta \
  build/proof
```

Buat `.gitignore` agar artefak generated tidak masuk commit.

```bash
cat > .gitignore <<'GITIGNORE'
build/
*.o
*.elf
*.bin
*.iso
*.img
*.map
*.log
.cache/
.vscode/
GITIGNORE
```

### Langkah 7 - Buat script `collect_meta.sh`

Script ini mengumpulkan versi toolchain dan informasi host. Tujuannya adalah memastikan semua laporan praktikum memiliki bukti versi yang konsisten.

```bash
cat > tools/scripts/collect_meta.sh <<'SH'
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/meta"
mkdir -p "$OUT"

{
  echo "mcsos_milestone=M1"
  echo "date_utc=$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  echo "repo=$ROOT"
  echo "pwd=$(pwd)"
  echo "user=$(id -un)"
  echo "uname=$(uname -a)"
  echo "nproc=$(nproc)"
  echo "shell=$SHELL"
  echo "path=$PATH"
  echo
  echo "[os-release]"
  cat /etc/os-release || true
  echo
  echo "[tool-versions]"
  git --version || true
  make --version | head -n 1 || true
  cmake --version | head -n 1 || true
  ninja --version || true
  clang --version | head -n 1 || true
  ld.lld --version | head -n 1 || true
  llvm-objdump --version | head -n 1 || true
  gcc --version | head -n 1 || true
  readelf --version | head -n 1 || true
  objdump --version | head -n 1 || true
  nm --version | head -n 1 || true
  nasm -v || true
  qemu-system-x86_64 --version | head -n 1 || true
  gdb --version | head -n 1 || true
  python3 --version || true
  shellcheck --version | head -n 2 || true
  cppcheck --version || true
  clang-tidy --version | head -n 1 || true
} | tee "$OUT/toolchain-versions.txt"

{
  echo "[filesystem]"
  df -h "$ROOT"
  echo
  echo "[memory]"
  free -h || true
  echo
  echo "[cpu]"
  lscpu || true
} | tee "$OUT/host-readiness.txt"
SH
chmod +x tools/scripts/collect_meta.sh
```

Jalankan script dan periksa output.

```bash
./tools/scripts/collect_meta.sh
ls -l build/meta
```

### Langkah 8 - Buat script `check_toolchain.sh`

Script ini memeriksa keberadaan tool wajib, memvalidasi repository path, dan memeriksa OVMF. Script harus gagal jika tool inti tidak ditemukan. Tujuan script ini adalah menyediakan gate `make check` yang objektif.

```bash
cat > tools/scripts/check_toolchain.sh <<'SH'
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STATUS=0

check_cmd() {
  local name="$1"
  if command -v "$name" >/dev/null 2>&1; then
    printf "OK: %-24s %s\n" "$name" "$(command -v "$name")"
  else
    printf "ERROR: missing command: %s\n" "$name" >&2
    STATUS=1
  fi
}

case "$ROOT" in
  /mnt/*)
    echo "ERROR: repository must not be under /mnt/*; current root=$ROOT" >&2
    STATUS=1
    ;;
  *)
    echo "OK: repository path is WSL Linux filesystem: $ROOT"
    ;;
esac

for cmd in git make cmake ninja clang ld.lld llvm-objdump gcc readelf objdump nm nasm qemu-system-x86_64 gdb python3 shellcheck cppcheck clang-tidy file; do
  check_cmd "$cmd"
done

OVMF_FOUND=0
for path in \
  /usr/share/OVMF/OVMF_CODE.fd \
  /usr/share/OVMF/OVMF_CODE_4M.fd \
  /usr/share/ovmf/OVMF.fd \
  /usr/share/qemu/OVMF.fd; do
  if [ -r "$path" ]; then
    echo "OK: OVMF firmware found: $path"
    OVMF_FOUND=1
  fi
done

if [ "$OVMF_FOUND" -eq 0 ]; then
  echo "ERROR: OVMF firmware not found. Install package: ovmf" >&2
  STATUS=1
fi

exit "$STATUS"
SH
chmod +x tools/scripts/check_toolchain.sh
```

Jalankan pemeriksaan.

```bash
./tools/scripts/check_toolchain.sh
```

Jika gagal, baca pesan error terlebih dahulu. Jangan menghapus script atau menonaktifkan check. Perbaiki tool yang hilang melalui paket yang benar, lalu ulangi.

### Langkah 9 - Buat source proof freestanding

Source berikut tidak mengimplementasikan kernel. Source ini hanya dipakai untuk memverifikasi bahwa compiler dapat menghasilkan object x86_64 ELF freestanding tanpa libc. Fungsi tidak memakai `printf`, `malloc`, file I/O, thread, atau syscall host.

```bash
cat > tests/toolchain/freestanding_probe.c <<'C'
#include <stdint.h>
#include <stddef.h>

volatile uint64_t mcsos_probe_sink;

static uint64_t rotl64(uint64_t x, unsigned int r) {
    return (x << r) | (x >> (64U - r));
}

uint64_t mcsos_toolchain_probe(uint64_t seed) {
    uint64_t x = seed ^ 0x4d43534f32363035ULL;
    for (size_t i = 0; i < 16; ++i) {
        x ^= (uint64_t)i * 0x9e3779b97f4a7c15ULL;
        x = rotl64(x, 13);
    }
    mcsos_probe_sink = x;
    return x;
}
C
```

### Langkah 10 - Buat script `proof_compile.sh`

Script ini mengompilasi object dan ELF proof. Flag disusun untuk menyerupai batas kernel: freestanding, tanpa stack protector host, tanpa PIC, tanpa red zone, tanpa SSE otomatis, dan tanpa standard library. Pada M2 flag dapat diperluas sesuai linker script kernel.

```bash
cat > tools/scripts/proof_compile.sh <<'SH'
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/proof"
SRC="$ROOT/tests/toolchain/freestanding_probe.c"
mkdir -p "$OUT"

CFLAGS=(
  --target=x86_64-unknown-elf
  -std=c17
  -ffreestanding
  -fno-stack-protector
  -fno-pic
  -mno-red-zone
  -mno-mmx
  -mno-sse
  -mno-sse2
  -Wall
  -Wextra
  -Werror
  -O2
  -c
)

clang "${CFLAGS[@]}" "$SRC" -o "$OUT/freestanding_probe.o"

ld.lld \
  -m elf_x86_64 \
  -nostdlib \
  --entry=mcsos_toolchain_probe \
  -Ttext=0xffffffff80000000 \
  -o "$OUT/freestanding_probe.elf" \
  "$OUT/freestanding_probe.o"

readelf -hW "$OUT/freestanding_probe.o" | tee "$OUT/readelf-object-header.txt"
readelf -hW "$OUT/freestanding_probe.elf" | tee "$OUT/readelf-header.txt"
readelf -SW "$OUT/freestanding_probe.elf" | tee "$OUT/readelf-sections.txt"
objdump -drwC "$OUT/freestanding_probe.o" | tee "$OUT/objdump-disassembly.txt"
nm -u "$OUT/freestanding_probe.elf" | tee "$OUT/nm-undefined.txt"
file "$OUT/freestanding_probe.o" "$OUT/freestanding_probe.elf" | tee "$OUT/file-type.txt"

if [ -s "$OUT/nm-undefined.txt" ]; then
  echo "ERROR: undefined symbols detected in freestanding ELF" >&2
  exit 1
fi

echo "OK: freestanding x86_64 ELF proof generated"
SH
chmod +x tools/scripts/proof_compile.sh
```

Jalankan proof compile.

```bash
./tools/scripts/proof_compile.sh
```

Indikator hasil yang benar:

1. `freestanding_probe.o` bertipe ELF64 relocatable x86_64.
2. `freestanding_probe.elf` bertipe ELF64 executable x86_64.
3. `nm-undefined.txt` kosong.
4. `readelf-header.txt` menunjukkan `Machine: Advanced Micro Devices X86-64` atau padanan x86-64.
5. Tidak ada pemanggilan libc seperti `printf`, `malloc`, `memcpy`, atau `__stack_chk_fail`.

### Langkah 11 - Buat script `qemu_probe.sh`

Script ini tidak mem-boot MCSOS. Script hanya memeriksa bahwa QEMU tersedia, machine `q35` dikenali, accelerator fallback dapat didokumentasikan, dan OVMF ada. M2 akan memakai informasi ini untuk boot image.

```bash
cat > tools/scripts/qemu_probe.sh <<'SH'
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/meta"
mkdir -p "$OUT"

{
  echo "[qemu-version]"
  qemu-system-x86_64 --version
  echo
  echo "[qemu-machine-help-q35]"
  qemu-system-x86_64 -machine help | grep -E "q35|pc-q35" || true
  echo
  echo "[qemu-accel-help]"
  qemu-system-x86_64 -accel help || true
  echo
  echo "[ovmf-candidates]"
  for path in /usr/share/OVMF/OVMF_CODE.fd /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/ovmf/OVMF.fd /usr/share/qemu/OVMF.fd; do
    if [ -r "$path" ]; then
      echo "$path"
    fi
  done
} | tee "$OUT/qemu-capabilities.txt"

if ! grep -q "q35" "$OUT/qemu-capabilities.txt"; then
  echo "ERROR: q35 machine not found in QEMU machine list" >&2
  exit 1
fi

if ! grep -q "OVMF" "$OUT/qemu-capabilities.txt" && ! grep -q "ovmf" "$OUT/qemu-capabilities.txt"; then
  echo "ERROR: OVMF firmware candidate not found" >&2
  exit 1
fi

echo "OK: QEMU and OVMF probe complete"
SH
chmod +x tools/scripts/qemu_probe.sh
```

Jalankan probe.

```bash
./tools/scripts/qemu_probe.sh
```

### Langkah 12 - Buat script `repro_check.sh`

Script ini menjalankan proof compile dua kali dengan membersihkan artefak proof di antara build. Karena pada M1 object proof sederhana tidak memuat timestamp eksplisit, hash dua build diharapkan sama. Jika tidak sama, mahasiswa harus mencatat penyebab nondeterminism.

```bash
cat > tools/scripts/repro_check.sh <<'SH'
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/repro"
mkdir -p "$OUT"

rm -rf "$ROOT/build/proof"
"$ROOT/tools/scripts/proof_compile.sh" >/dev/null
sha256sum "$ROOT/build/proof/freestanding_probe.o" "$ROOT/build/proof/freestanding_probe.elf" | tee "$OUT/sha256-run1.txt"

rm -rf "$ROOT/build/proof"
"$ROOT/tools/scripts/proof_compile.sh" >/dev/null
sha256sum "$ROOT/build/proof/freestanding_probe.o" "$ROOT/build/proof/freestanding_probe.elf" | tee "$OUT/sha256-run2.txt"

if diff -u "$OUT/sha256-run1.txt" "$OUT/sha256-run2.txt" > "$OUT/sha256-diff.txt"; then
  echo "OK: proof build is reproducible for M1 inputs" | tee "$OUT/repro-status.txt"
else
  echo "ERROR: proof build hash differs; inspect $OUT/sha256-diff.txt" | tee "$OUT/repro-status.txt" >&2
  exit 1
fi
SH
chmod +x tools/scripts/repro_check.sh
```

Jalankan reproducibility check.

```bash
./tools/scripts/repro_check.sh
```

### Langkah 13 - Buat Makefile minimum M1

Makefile menjadi antarmuka tunggal praktikum. Mahasiswa tidak boleh hanya menjalankan script manual tanpa Makefile karena milestone berikutnya akan memakai target yang sama.

```bash
cat > Makefile <<'MAKE'
SHELL := /usr/bin/env bash
.DEFAULT_GOAL := help

.PHONY: help meta check proof qemu-probe repro test clean distclean

help:
	@echo "MCSOS M1 targets:"
	@echo "  make meta        - collect host and toolchain metadata"
	@echo "  make check       - verify required tools and repository path"
	@echo "  make proof       - build freestanding x86_64 ELF proof"
	@echo "  make qemu-probe  - verify QEMU machine and OVMF availability"
	@echo "  make repro       - run reproducibility check for proof artifact"
	@echo "  make test        - run all M1 checks"
	@echo "  make clean       - remove generated proof output"
	@echo "  make distclean   - remove all generated build output"

meta:
	@./tools/scripts/collect_meta.sh

check:
	@./tools/scripts/check_toolchain.sh

proof:
	@./tools/scripts/proof_compile.sh

qemu-probe:
	@./tools/scripts/qemu_probe.sh

repro:
	@./tools/scripts/repro_check.sh

test: meta check proof qemu-probe repro
	@echo "OK: M1 test suite passed"

clean:
	@rm -rf build/proof build/repro
	@echo "OK: cleaned proof and reproducibility outputs"

distclean:
	@rm -rf build
	@echo "OK: removed build directory"
MAKE
```

Jalankan seluruh target M1.

```bash
make test
```

Indikator hasil yang benar adalah baris akhir:

```text
OK: M1 test suite passed
```

### Langkah 14 - Buat dokumen invariants awal

Dokumen ini menjadi penghubung M1 ke M2. Tuliskan invariants lingkungan yang harus tetap benar selama praktikum berikutnya.

```bash
cat > docs/architecture/invariants.md <<'MD'
# MCSOS Toolchain and Environment Invariants

## M1 invariants

1. Repository MCSOS berada di filesystem Linux WSL, bukan di `/mnt/c` atau mount Windows lain.
2. Semua generated artifact berada di `build/` dan tidak dikomit ke Git.
3. Semua build tool wajib tersedia melalui PATH WSL dan tercatat di `build/meta/toolchain-versions.txt`.
4. Proof object harus bertipe ELF64 x86_64 dan dihasilkan dengan mode freestanding.
5. Proof ELF tidak boleh memiliki undefined symbol.
6. Kompilasi kernel/proof tidak boleh bergantung pada hosted libc, startup object, dynamic linker, exception runtime, atau stack protector runtime host.
7. QEMU x86_64, machine q35, dan OVMF harus terdeteksi sebelum M2 dimulai.
8. Setiap perubahan toolchain atau versi distro harus dicatat dalam readiness review.
MD
```

### Langkah 15 - Buat threat model toolchain ringkas

M1 juga menilai risiko supply-chain dan konfigurasi. Threat model berikut bersifat awal dan akan diperluas pada M12/security.

```bash
cat > docs/security/toolchain_threat_model.md <<'MD'
# Threat Model Ringkas M1 - Toolchain dan Lingkungan

## Assets

1. Source code MCSOS.
2. Script build dan test.
3. Toolchain compiler, linker, assembler, emulator, debugger.
4. Artefak generated: object, ELF, log, metadata.
5. Bukti praktikum dan laporan.

## Trust assumptions

1. Paket Ubuntu/Debian berasal dari repository resmi atau mirror kampus yang disetujui.
2. Mahasiswa tidak mengubah binary compiler/linker secara manual.
3. Repository berada pada filesystem Linux WSL agar permission dan executable bit stabil.
4. Build M1 belum mengeksekusi kode guest MCSOS; risiko utama adalah salah konfigurasi dan supply-chain.

## Threats

| Threat | Dampak | Mitigasi M1 |
|---|---|---|
| Compiler host salah target | Object tidak cocok untuk kernel | Inspect `readelf` dan target triple |
| Linker memakai libc/startup host | Kernel bergantung pada runtime tidak tersedia | Gunakan `-nostdlib`, cek `nm -u` |
| Repository di `/mnt/c` | permission, symlink, case sensitivity, I/O tidak stabil | Check path pada `check_toolchain.sh` |
| Generated artifact dikomit | repository kotor dan sulit direproduksi | `.gitignore`, `make distclean` |
| OVMF tidak tersedia | M2 gagal boot UEFI | `qemu_probe.sh` |
| Versi tool tidak dicatat | build tidak dapat diaudit | `collect_meta.sh` |
MD
```

### Langkah 16 - Buat readiness review M1

Readiness review harus diisi setelah `make test` berhasil. Jangan mengisi status lulus sebelum evidence ada.

```bash
cat > docs/readiness/M1-toolchain.md <<'MD'
# Readiness Review M1 - Toolchain Reproducible

## Identitas

- Nama mahasiswa/kelompok:
- NIM anggota:
- Kelas:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi: Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia
- Tanggal:
- Commit hash:

## Ringkasan hasil

Tuliskan ringkasan singkat hasil M1. Gunakan istilah terukur: `siap untuk M2` hanya bila semua acceptance criteria M1 terpenuhi.

## Evidence checklist

| Evidence | Path | Status | Catatan |
|---|---|---|---|
| Toolchain versions | `build/meta/toolchain-versions.txt` | | |
| Host readiness | `build/meta/host-readiness.txt` | | |
| QEMU capabilities | `build/meta/qemu-capabilities.txt` | | |
| Freestanding object | `build/proof/freestanding_probe.o` | | |
| Freestanding ELF | `build/proof/freestanding_probe.elf` | | |
| ELF header | `build/proof/readelf-header.txt` | | |
| ELF sections | `build/proof/readelf-sections.txt` | | |
| Disassembly | `build/proof/objdump-disassembly.txt` | | |
| Undefined symbol report | `build/proof/nm-undefined.txt` | | |
| Reproducibility hash | `build/repro/sha256-run1.txt`, `build/repro/sha256-run2.txt` | | |

## Acceptance criteria M1

| Kriteria | Lulus/Gagal | Bukti |
|---|---|---|
| Repository berada di filesystem Linux WSL | | |
| Semua tool wajib tersedia | | |
| `make meta` berhasil | | |
| `make check` berhasil | | |
| `make proof` berhasil | | |
| `make qemu-probe` berhasil | | |
| `make repro` berhasil | | |
| `make test` berhasil dari clean checkout | | |
| `nm-undefined.txt` kosong | | |
| Hasil `readelf` menunjukkan ELF64 x86_64 | | |

## Known limitations

Tuliskan keterbatasan yang masih ada. Contoh: belum ada cross GCC `x86_64-elf-gcc`, belum ada CI, belum ada hardware test, belum ada boot image.

## Risiko dan mitigasi

Tuliskan minimal tiga risiko teknis M1 dan mitigasinya.

## Readiness decision

Pilih salah satu:

- [ ] Belum siap lanjut M2.
- [ ] Siap lanjut M2 dengan catatan.
- [ ] Siap lanjut M2.

Alasan keputusan:

MD
```

### Langkah 17 - Jalankan clean checkout rehearsal

Sebelum commit, lakukan simulasi minimal dengan membersihkan build lalu menjalankan ulang test. Tujuannya adalah membuktikan bahwa keberhasilan tidak bergantung pada artefak lama.

```bash
make distclean
make test
```

Jika `make test` gagal setelah `make distclean`, berarti ada dependensi tersembunyi atau script belum lengkap. Perbaiki sebelum melanjutkan.

### Langkah 18 - Commit hasil M1

Setelah semua evidence dibuat dan dokumen readiness diisi, commit perubahan source dan dokumen. Jangan commit direktori `build/` kecuali dosen meminta arsip evidence secara eksplisit. Evidence dapat dilampirkan pada laporan praktikum.

```bash
git status
git add Makefile .gitignore docs tools tests
git commit -m "M1: add reproducible toolchain readiness baseline"
git status
git rev-parse HEAD
```

Catat commit hash ke laporan praktikum.

---

## 10. Checkpoint Buildable

| Checkpoint | Perintah | Output wajib | Lanjut jika |
|---|---|---|---|
| CP1 | `wsl --list --verbose` | Distro VERSION 2 | WSL 2 aktif |
| CP2 | `./tools/scripts/check_toolchain.sh` | semua tool OK | tidak ada ERROR |
| CP3 | `make meta` | `build/meta/toolchain-versions.txt` | file terisi |
| CP4 | `make proof` | object dan ELF proof | `nm-undefined.txt` kosong |
| CP5 | `make qemu-probe` | q35 dan OVMF terdeteksi | tidak ada ERROR |
| CP6 | `make repro` | hash run1 dan run2 sama | diff kosong |
| CP7 | `make test` | `OK: M1 test suite passed` | siap isi readiness |
| CP8 | `git commit` | commit hash | siap laporan |

---

## 11. Tugas Implementasi Mahasiswa

### Tugas wajib

1. Menyiapkan WSL 2 dan repository pada filesystem Linux WSL.
2. Memasang paket toolchain M1.
3. Membuat seluruh script M1 sesuai panduan.
4. Membuat Makefile minimum M1.
5. Menjalankan `make test` dari clean state.
6. Mengisi `docs/readiness/M1-toolchain.md`.
7. Membuat laporan praktikum memakai template laporan standar.
8. Melakukan commit Git dengan pesan yang ditentukan.

### Tugas pengayaan

1. Menambahkan dukungan deteksi `x86_64-elf-gcc` bila tersedia.
2. Menambahkan `llvm-readelf` sebagai fallback bila GNU `readelf` tidak tersedia.
3. Menambahkan GitHub Actions atau CI lokal untuk target `make test`.
4. Menambahkan script `tools/scripts/archive_evidence.sh` untuk membundel evidence M1 ke file `.tar.gz`.

### Tantangan riset

1. Bandingkan output proof ELF dari Clang/LLD dan GCC/Binutils cross toolchain.
2. Jelaskan perbedaan target triple `x86_64-unknown-elf`, `x86_64-elf`, dan `x86_64-linux-gnu`.
3. Uji apakah flag tertentu menyebabkan perubahan hash build dan jelaskan sumber nondeterminism.

---

## 12. Perintah Uji Ringkas

Gunakan urutan berikut sebagai uji akhir sebelum laporan dikumpulkan.

```bash
make distclean
make meta
make check
make proof
make qemu-probe
make repro
make test
git status
git rev-parse HEAD
```

Setiap perintah harus disalin ke laporan bersama ringkasan output. Output penuh dapat diletakkan di lampiran.

---

## 13. Bukti yang Harus Dikumpulkan

| Jenis bukti | Minimum isi |
|---|---|
| Screenshot PowerShell | `wsl --list --verbose` menunjukkan WSL 2 |
| Screenshot terminal WSL | `make test` berhasil |
| `toolchain-versions.txt` | versi toolchain lengkap |
| `host-readiness.txt` | CPU, memori, filesystem repository |
| `qemu-capabilities.txt` | QEMU, q35, OVMF |
| `readelf-header.txt` | ELF64 x86_64 |
| `objdump-disassembly.txt` | disassembly object proof |
| `nm-undefined.txt` | kosong |
| `sha256-run1.txt` dan `sha256-run2.txt` | hash identik |
| Git commit hash | commit M1 |
| Readiness review | keputusan siap/tidak siap M2 |

---

## 14. Pertanyaan Analisis

Jawab pertanyaan berikut pada laporan praktikum:

1. Mengapa repository MCSOS sebaiknya ditempatkan di filesystem Linux WSL, bukan di `/mnt/c`?
2. Apa perbedaan `x86_64-unknown-elf` dengan `x86_64-linux-gnu` dalam konteks kernel freestanding?
3. Mengapa flag `-ffreestanding`, `-nostdlib`, dan `-mno-red-zone` penting untuk kernel x86_64?
4. Apa risiko jika `nm -u` pada ELF proof menampilkan `__stack_chk_fail`, `memcpy`, atau symbol libc lain?
5. Mengapa M1 belum boleh disebut sebagai bukti bahwa MCSOS dapat boot?
6. Apa saja bukti minimum agar lingkungan dinyatakan siap lanjut M2?
7. Jika hash build pertama dan kedua berbeda, bagaimana langkah diagnosis yang sistematis?
8. Mengapa QEMU dan OVMF diperiksa pada M1 padahal boot image baru dibuat pada M2?
9. Bagaimana threat model toolchain dapat mempengaruhi keamanan OS pada tahap lanjut?
10. Apakah hasil emulator dapat dijadikan bukti kesiapan hardware? Jelaskan batasannya.

---

## 15. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | WSL 2, toolchain, script, Makefile, proof compile, QEMU probe, dan reproducibility check berjalan benar. |
| Kualitas desain dan invariants | 20 | Struktur repository, `.gitignore`, invariants, threat model, dan readiness review konsisten. |
| Pengujian dan bukti | 20 | Evidence lengkap, dapat dibaca, dan cocok dengan perintah yang dijalankan. |
| Debugging dan failure analysis | 10 | Mahasiswa mampu menjelaskan error, penyebab, dan langkah perbaikan. |
| Keamanan dan robustness | 10 | Tidak ada hidden dependency, path aman, undefined symbol kosong, risiko supply-chain dicatat. |
| Dokumentasi dan laporan | 10 | Laporan rapi, mengikuti template, berisi commit hash, output uji, analisis, dan referensi. |

---

## 16. Failure Modes dan Diagnosis

| Failure mode | Gejala | Penyebab umum | Diagnosis | Perbaikan |
|---|---|---|---|---|
| Repository di `/mnt/c` | `check_toolchain.sh` gagal | repo dibuat dari File Explorer/Windows drive | `pwd` | pindahkan ke `~/src/mcsos` |
| Tool tidak ditemukan | command not found | paket belum dipasang | `command -v nama_tool` | `sudo apt install ...` |
| OVMF tidak ditemukan | `qemu_probe.sh` gagal | paket `ovmf` belum ada atau path berbeda | `dpkg -L ovmf` | install ovmf atau update script path |
| Undefined symbol | `nm-undefined.txt` tidak kosong | compiler menghasilkan dependency runtime | cek `nm -u` dan flags | matikan stack protector, supply runtime, atau ubah kode |
| ELF bukan x86_64 | `readelf` menunjukkan machine salah | target triple salah | `readelf -hW` | gunakan `--target=x86_64-unknown-elf` |
| Red zone aktif | tidak terlihat langsung pada M1 | flag hilang | audit CFLAGS | tambahkan `-mno-red-zone` |
| Repro check gagal | hash berbeda | timestamp, path debug, build-id, metadata nondeterministic | bandingkan `sha256-diff.txt`, `readelf -n` | catat atau hilangkan sumber nondeterminism |
| `make test` hanya sukses sekali | `distclean` lalu gagal | dependensi generated tidak dibuat ulang | jalankan target satu per satu | perbaiki dependency Makefile |
| QEMU tidak jalan di WSL | error display/accelerator | display GUI tidak tersedia, KVM tidak aktif | gunakan headless/probe TCG | pakai `-display none`, fallback TCG |

---

## 17. Prosedur Rollback

Jika M1 gagal secara sistematis, lakukan rollback berikut:

1. Simpan error log ke `docs/reports/M1-failure-notes.md`.
2. Jalankan `make distclean`.
3. Periksa repository path dengan `pwd`.
4. Jalankan ulang `sudo apt update` dan instal ulang paket yang hilang.
5. Jalankan `./tools/scripts/check_toolchain.sh` sebelum target lain.
6. Jika error berasal dari script yang baru dimodifikasi, gunakan `git diff` untuk melihat perubahan.
7. Jika perubahan tidak dapat diperbaiki, kembalikan file ke commit terakhir yang stabil.
8. Jangan mengubah acceptance criteria agar terlihat lulus. Catat kegagalan dan penyebabnya.

---

## 18. Kriteria Lulus Praktikum

Mahasiswa atau kelompok dinyatakan lulus M1 bila memenuhi seluruh kriteria minimum berikut:

1. Repository berada pada filesystem Linux WSL.
2. Semua tool wajib terpasang dan terdeteksi oleh `check_toolchain.sh`.
3. `make meta`, `make check`, `make proof`, `make qemu-probe`, `make repro`, dan `make test` berhasil.
4. `freestanding_probe.o` dan `freestanding_probe.elf` terbentuk.
5. `readelf` membuktikan target ELF64 x86_64.
6. `nm-undefined.txt` kosong.
7. Hash reproducibility run pertama dan kedua identik atau nondeterminism dijelaskan secara valid.
8. Evidence disimpan dan dilampirkan pada laporan.
9. `docs/readiness/M1-toolchain.md` diisi dengan keputusan readiness yang jujur.
10. Git commit dibuat dan commit hash dicantumkan pada laporan.
11. Mahasiswa dapat menjelaskan minimal tiga failure mode M1 dan mitigasinya.

Untuk pengayaan, mahasiswa dapat memperoleh nilai tambahan sesuai kebijakan dosen bila berhasil menambahkan CI atau membandingkan Clang/LLD dengan GCC/Binutils cross toolchain secara terukur.

---

## 19. Template Laporan Praktikum M1

Gunakan template laporan standar `os_template_laporan_praktikum.md`. Untuk M1, minimal isi bagian berikut:

1. Sampul: judul praktikum M1, nama mahasiswa/kelompok, NIM, kelas, dosen, program studi.
2. Tujuan: tuliskan target teknis M1.
3. Dasar teori ringkas: hosted vs freestanding, target triple, ELF, red zone, reproducibility.
4. Lingkungan: Windows build, WSL version, distro Linux, CPU, RAM, path repository, commit hash.
5. Desain: struktur repository, Makefile target, script M1, invariants.
6. Langkah kerja: salin perintah penting dan jelaskan hasilnya.
7. Hasil uji: tabel pass/fail untuk `make meta`, `make check`, `make proof`, `make qemu-probe`, `make repro`, `make test`.
8. Analisis: jawab pertanyaan analisis M1.
9. Keamanan dan reliability: jelaskan threat model toolchain dan mitigasi.
10. Kesimpulan: nyatakan readiness M1 secara terukur.
11. Lampiran: output `toolchain-versions.txt`, `readelf`, `objdump`, `nm`, hash, screenshot, dan Git commit.

---

## 20. Readiness Review Akhir M1

Gunakan klasifikasi berikut:

| Status | Arti | Syarat |
|---|---|---|
| Belum siap lanjut M2 | Lingkungan belum dapat dipercaya | Ada target wajib gagal atau evidence tidak lengkap |
| Siap lanjut M2 dengan catatan | Mayoritas target lulus, ada risiko kecil terdokumentasi | Risiko tidak menghalangi boot image M2 |
| Siap lanjut M2 | Semua target wajib lulus dan evidence lengkap | `make test` lulus dari clean state |

M1 hanya dapat menghasilkan status maksimum **siap lanjut M2**. Tidak ada klaim `siap produksi`, `tanpa error`, atau `siap pakai umum` pada tahap ini.

---

## 21. Referensi

[1] Microsoft, "Install WSL," Microsoft Learn, 2025. [Online]. Available: https://learn.microsoft.com/windows/wsl/install  
[2] Microsoft, "Advanced settings configuration in WSL," Microsoft Learn, 2025. [Online]. Available: https://learn.microsoft.com/windows/wsl/wsl-config  
[3] QEMU Project, "Invocation," QEMU System Emulation User's Guide, 2026. [Online]. Available: https://www.qemu.org/docs/master/system/invocation.html  
[4] QEMU Project, "GDB usage," QEMU System Emulation User's Guide, 2026. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html  
[5] Free Software Foundation, "x86 Options," GCC Online Documentation. [Online]. Available: https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html  
[6] Free Software Foundation, "Options for Linking," GCC Online Documentation. [Online]. Available: https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html  
[7] LLVM Project, "Cross-compilation using Clang," Clang Documentation. [Online]. Available: https://clang.llvm.org/docs/CrossCompilation.html  
[8] Kitware, "CMAKE_SYSTEM_NAME," CMake Documentation. [Online]. Available: https://cmake.org/cmake/help/latest/variable/CMAKE_SYSTEM_NAME.html  
[9] GNU Project, "Parallel Execution," GNU Make Manual. [Online]. Available: https://www.gnu.org/software/make/manual/html_node/Parallel.html  
[10] GNU Project, "GNU Binutils," GNU Binutils Documentation. [Online]. Available: https://www.gnu.org/software/binutils/  
[11] Ninja Build, "The Ninja build system," Ninja Manual. [Online]. Available: https://ninja-build.org/manual  

---

## 22. Lampiran A - Checklist Pengumpulan

| Item | Ya/Tidak | Catatan |
|---|---|---|
| Repository di `~/src/mcsos` atau path Linux WSL lain | | |
| `.wslconfig` dicatat | | |
| `make test` lulus | | |
| Evidence `build/meta` tersedia | | |
| Evidence `build/proof` tersedia | | |
| Evidence `build/repro` tersedia | | |
| Readiness review diisi | | |
| Laporan memakai template standar | | |
| Commit Git dibuat | | |
| Failure modes dianalisis | | |

