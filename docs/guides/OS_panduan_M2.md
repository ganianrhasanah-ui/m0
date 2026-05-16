# OS_panduan_M2.md

# Panduan Praktikum M2 — Boot Image, Kernel ELF64, Early Serial Console, dan Readiness Gate M2 MCSOS 260502

**Mata kuliah:** Sistem Operasi Lanjut / Praktikum Sistem Operasi  
**Proyek:** MCSOS versi 260502  
**Kode praktikum:** M2  
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi:** Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia  
**Target arsitektur:** x86_64 / AMD64 / Intel 64  
**Host pengembangan:** Windows 11 x64  
**Lingkungan build:** WSL 2 Linux environment, direkomendasikan Ubuntu LTS atau Debian stable  
**Emulator utama:** QEMU system emulator untuk x86_64  
**Firmware emulator:** OVMF untuk jalur UEFI  
**Bootloader referensi:** Limine binary release branch  
**Model kernel awal:** kernel monolitik pendidikan dengan boundary modular dan POSIX-like subset  
**Bahasa utama:** freestanding C17 dengan assembly x86_64 minimal melalui inline assembly terkendali  
**Status keluaran:** *siap uji QEMU untuk jalur boot M2 apabila seluruh bukti build, image, inspeksi ELF, log serial, dan readiness review tersedia*. Status ini bukan “tanpa error”, bukan “siap produksi”, dan bukan “siap bring-up hardware umum”.

---

## 0. Ringkasan Praktikum

Praktikum M2 membangun artefak boot pertama MCSOS 260502. Pada M0 mahasiswa menyiapkan baseline requirements, governance, risk register, verification matrix, dan struktur repository. Pada M1 mahasiswa memvalidasi toolchain, lingkungan WSL 2, kompilasi freestanding object, inspeksi ELF, serta reproducibility dasar. Pada M2 mahasiswa mulai menghasilkan kernel ELF64 yang dapat dimuat oleh bootloader, membuat image bootable, menjalankan image di QEMU/OVMF, dan memperoleh bukti log serial bahwa kontrol eksekusi telah masuk ke kernel MCSOS secara terukur.

Fokus M2 bukan membuat sistem operasi lengkap. Fokusnya adalah membuktikan jalur paling awal berikut: source freestanding C dan inline assembly kecil dapat dikompilasi menjadi `kernel.elf`, `kernel.elf` memiliki entry point dan program header yang dapat diperiksa, image bootable dapat dibuat secara deterministik, QEMU dapat menjalankan image dengan firmware OVMF, dan serial console mencetak marker MCSOS M2. Keberhasilan M2 hanya boleh diklaim sebagai **siap uji QEMU tahap boot awal**, bukan sebagai bukti kernel stabil, aman, atau lengkap.

Panduan ini juga memuat pemeriksaan kesiapan hasil M0 dan M1. Pemeriksaan tersebut wajib dilakukan sebelum menulis source M2 karena kesalahan paling umum pada tahap boot bukan berasal dari kode kernel, tetapi dari lingkungan tidak siap: repository berada di `/mnt/c`, toolchain tidak benar, `ld` bukan `ld.lld`, OVMF tidak ditemukan, QEMU tidak tersedia, newline Windows merusak script, permission executable hilang, Git tidak bersih, atau object M1 tidak benar-benar target x86_64 ELF.

---

## 1. Capaian Pembelajaran

Setelah menyelesaikan M2, mahasiswa mampu:

1. Menjelaskan hubungan antara firmware, bootloader, kernel ELF64, linker script, entry point, dan emulator.
2. Memeriksa kembali readiness M0 dan M1 sebelum mengeksekusi milestone boot.
3. Membuat source kernel freestanding C17 yang tidak bergantung pada hosted libc.
4. Membuat accessor port I/O x86_64 yang terbatas untuk UART 16550 COM1.
5. Menginisialisasi serial console awal dan mencetak marker boot deterministik.
6. Membuat linker script untuk higher-half kernel ELF64 tahap awal.
7. Menghasilkan `kernel.elf`, `kernel.map`, `readelf` evidence, `objdump` evidence, dan `nm` evidence.
8. Mengambil Limine binary release secara terkontrol dan membuat ISO bootable untuk QEMU.
9. Menjalankan QEMU/OVMF secara headless dan menyimpan log serial ke file.
10. Mengklasifikasikan failure modes M2: build failure, linker failure, image failure, firmware failure, bootloader failure, serial failure, hang, reboot loop, dan triple-fault-like behavior.
11. Menyusun readiness review M2 berdasarkan bukti, bukan berdasarkan klaim subjektif.

---

## 2. Prasyarat Teori

| Konsep | Penjelasan operasional | Kewajiban bukti pada M2 |
|---|---|---|
| Firmware UEFI/OVMF | Firmware virtual yang menyiapkan platform awal sebelum bootloader berjalan. | Path OVMF, versi paket, dan QEMU command disimpan. |
| Bootloader Limine | Bootloader modern yang dapat memuat kernel ELF64 dan menyediakan konfigurasi boot. | Branch/tag Limine dicatat, `limine.conf` disimpan. |
| ELF64 executable | Format executable kernel yang dimuat bootloader. | `readelf -hW`, `readelf -lW`, dan `kernel.map`. |
| Higher-half kernel | Kernel ditempatkan pada alamat virtual tinggi, misalnya `0xffffffff80000000`. | Entry point dan symbol boundary diperiksa. |
| Freestanding C | C tanpa asumsi hosted libc, startup object, atau `main`. | Flags `-ffreestanding`, `-nostdlib`, `-mno-red-zone`. |
| x86_64 port I/O | Akses I/O port untuk UART 16550 COM1 memakai instruksi `inb`/`outb`. | Source `io.h`, `serial.c`, disassembly bukti `out`/`in`. |
| Serial console | Kanal observability paling awal untuk boot. | `build/qemu-serial.log` berisi marker M2. |
| Linker script | Mengontrol entry point, section, segment, alignment, dan map file. | `linker.ld`, `kernel.map`, PHDR inspection. |
| Reproducibility | Build dapat diulang dari clean checkout. | `make distclean && make all inspect image` berhasil. |

---

## 3. Peta Skill yang Digunakan

| Skill | Peran pada M2 | Artefak wajib |
|---|---|---|
| `osdev-general` | Gate M2, roadmap, acceptance criteria, readiness review | `docs/readiness/M2-boot-image.md` |
| `osdev-01-computer-foundation` | Invariants boot path, state transition, proof obligation | `docs/architecture/invariants.md` diperbarui |
| `osdev-02-low-level-programming` | Freestanding C, inline assembly, ABI, linker, ELF, red zone | `kernel.elf`, `kernel.map`, disassembly |
| `osdev-03-computer-and-hardware-architecture` | x86_64, port I/O, UART 16550, QEMU q35, OVMF | QEMU command dan serial log |
| `osdev-04-kernel-development` | Early kernel entry, halt loop, panic/observability planning | `kernel/core/kmain.c`, boot marker |
| `osdev-05-filesystem-development` | Belum mengimplementasi FS; hanya image layout dan generated artifact policy | `iso_root/` generated, tidak dikomit |
| `osdev-06-networking-stack` | Non-goal M2; network device QEMU tidak diaktifkan agar boot path deterministik | catatan non-goal |
| `osdev-07-os-security` | Trust boundary bootloader, supply-chain, fail-closed behavior | checksum/revision Limine, threat model update |
| `osdev-08-device-driver-development` | Serial driver awal sebagai driver minimal port I/O | `serial_init`, `serial_write`, disassembly |
| `osdev-09-virtualization-and-containerization` | QEMU/OVMF sebagai platform uji virtual | QEMU run log |
| `osdev-10-boot-firmware` | Boot image, Limine config, OVMF, handoff assumptions | `configs/limine/limine.conf`, ISO |
| `osdev-11-graphics-display` | Non-goal GUI; display dinonaktifkan, serial menjadi output utama | `-display none`, serial log |
| `osdev-12-toolchain-devenv` | Build system, inspection, reproducibility, QEMU/GDB workflow | Makefile, scripts, metadata |
| `osdev-13-enterprise-features` | Observability awal, log retention, rollback | readiness review dan rollback plan |
| `osdev-14-cross-science` | Requirements traceability, risk register, verification matrix | update verification matrix |

---

## 4. Asumsi Target M2

Panduan ini menggunakan asumsi konservatif berikut.

| Komponen | Asumsi |
|---|---|
| Arsitektur | x86_64 long mode, target awal QEMU. |
| Host | Windows 11 x64. |
| Lingkungan build | WSL 2 Linux filesystem, bukan `/mnt/c`. |
| Compiler | Clang dengan target `x86_64-unknown-none-elf`. |
| Linker | `ld.lld` dengan linker script eksplisit. |
| Emulator | `qemu-system-x86_64`. |
| Firmware | OVMF. |
| Bootloader | Limine binary release branch. |
| Output | `build/kernel.elf`, `build/mcsos.iso`, `build/qemu-serial.log`. |
| Display | Headless; output validasi utama melalui serial. |
| Userspace | Belum ada. |
| Memory manager | Belum ada. |
| Interrupt handler | Belum ada. Kernel memasuki controlled halt loop. |
| Klaim readiness | Hanya siap uji QEMU tahap M2 apabila evidence lengkap. |

Jika kelas memilih GCC cross-compiler `x86_64-elf-gcc`, perintah kompilasi dapat disesuaikan dengan mengganti `CC`, `LD`, dan flags target. Namun panduan utama menggunakan Clang/LLD karena Clang dapat menargetkan `x86_64-unknown-none-elf` secara eksplisit dan LLD mendukung linking ELF dengan linker script.


---

## 4A. Goals dan Non-goals M2

### 4A.1 Goals

1. Menghasilkan kernel ELF64 x86_64 freestanding yang dapat diinspeksi.
2. Menghasilkan boot image MCSOS M2 yang dapat dijalankan pada QEMU/OVMF.
3. Menyediakan early serial console sebagai kanal observability pertama.
4. Membuktikan jalur firmware -> bootloader -> kernel entry -> serial output -> controlled halt loop.
5. Menyediakan evidence build dan runtime yang dapat direproduksi oleh dosen atau asisten.

### 4A.2 Non-goals

1. Tidak mengimplementasikan memory manager.
2. Tidak mengimplementasikan IDT, interrupt handler, timer, atau panic subsystem penuh.
3. Tidak mengimplementasikan framebuffer, filesystem, driver block, network stack, scheduler, syscall, atau userspace.
4. Tidak melakukan hardware bring-up fisik.
5. Tidak mengklaim kernel aman, stabil, bebas cacat, atau siap produksi.

---

## 4B. Architecture / Design M2

Arsitektur M2 adalah boot path paling kecil yang dapat diuji. Alur kendalinya adalah `OVMF -> Limine -> kernel.elf -> kmain -> serial_init -> serial_write -> halt_forever`. Setiap komponen memiliki tanggung jawab sempit. OVMF menyediakan lingkungan UEFI di QEMU. Limine memuat kernel ELF64. Kernel M2 tidak melakukan parsing boot info; ia hanya menginisialisasi UART COM1 dan menulis marker boot. Pendekatan ini membuat ruang kegagalan kecil dan membantu mahasiswa membedakan kesalahan firmware, bootloader, linker, dan kode kernel.

```text
Windows 11 x64
  -> WSL 2 Linux filesystem
    -> Clang/LLD build
      -> kernel.elf + kernel.map
        -> Limine ISO image
          -> QEMU q35 + OVMF
            -> kmain()
              -> COM1 serial log
                -> controlled halt loop
```

---

## 4C. Interfaces dan ABI/API Boundary M2

M2 memiliki tiga interface utama. Pertama, interface build: source C dikompilasi dengan target `x86_64-unknown-none-elf`, mode freestanding, tanpa hosted startup object, dan dilink dengan `ld.lld`. Kedua, interface boot: `kernel.elf` diekspos kepada Limine melalui `configs/limine/limine.conf`. Ketiga, interface observability: output kernel diekspos melalui UART COM1 yang diarahkan QEMU ke `build/qemu-serial.log`.

ABI internal M2 mengikuti batas konservatif berikut: fungsi C menggunakan x86_64 System V calling convention, red zone dinonaktifkan, SIMD/FPU tidak digunakan, dan kernel tidak bergantung pada `main`, libc, exception runtime, unwinder, atau dynamic loader. Semua perubahan ABI boot pada milestone berikutnya harus dicatat dalam ADR.

---

## 4D. Security dan Threat Model M2

Threat model M2 masih terbatas pada jalur pengembangan dan boot awal. Aset utama adalah source code, toolchain, `kernel.elf`, image ISO, konfigurasi Limine, dan log evidence. Ancaman yang relevan meliputi dependency bootloader yang tidak terverifikasi, path repository yang tidak deterministik, script shell yang rusak karena CRLF, QEMU command yang tidak terdokumentasi, dan klaim readiness yang melebihi bukti. Mitigasi M2 adalah pencatatan revision Limine, checksum ISO, preflight script, generated artifact policy, Git commit evidence, dan status readiness terbatas.

M2 belum memiliki user/kernel isolation, credential, capability, MAC/DAC, secure boot, measured boot, atau update integrity. Bagian tersebut menjadi target milestone lanjut dan tidak boleh diklaim sudah tersedia pada M2.

---

## 4E. Testing, Validation, dan Verification M2

Validasi M2 terdiri atas lima lapis. Lapis pertama adalah preflight M0/M1 untuk memastikan lingkungan dan baseline tidak rusak. Lapis kedua adalah build validation melalui `make build`. Lapis ketiga adalah ELF inspection melalui `readelf`, `objdump`, dan `nm`. Lapis keempat adalah image validation melalui `make image` dan checksum ISO. Lapis kelima adalah runtime validation melalui QEMU/OVMF dan marker serial. Kelima lapis harus lulus sebelum status M2 dapat dinyatakan siap uji QEMU tahap boot awal.


---

## 4F. Assumptions and Target untuk Boot/Firmware

Assumptions and target M2 adalah arsitektur x86_64, firmware OVMF, boot path Limine, dan image ISO bootable. Kernel image adalah `build/kernel.elf` dengan format ELF64. Emulator target adalah QEMU `q35`; hardware fisik tidak menjadi target M2 dan hanya boleh diuji setelah milestone observability yang lebih kuat.

## 4G. Boot-chain Design

Boot-chain design M2 terdiri dari stage firmware, stage bootloader, stage kernel load, dan stage kernel entry. Stage firmware adalah OVMF pada QEMU. Stage bootloader adalah Limine yang membaca `limine.conf`. Stage kernel load memuat `kernel.elf` dari ISO. Stage kernel entry memanggil `kmain` pada alamat higher-half. Handoff pada M2 sengaja minimal: kernel belum menerima atau memvalidasi memory map, sehingga memory manager belum boleh diaktifkan.

## 4H. Handoff Contract

Handoff contract M2: CPU sudah berada pada mode yang dapat mengeksekusi kernel x86_64 sesuai kontrak bootloader; stack awal disediakan oleh bootloader; register umum tidak boleh diasumsikan berisi boot info karena M2 tidak memakai parameter boot; memory map belum dipakai oleh kernel; serial port COM1 diakses secara eksplisit melalui port I/O. Jika milestone berikutnya memakai boot info, kontrak CPU, stack, register, memory map, dan lifetime data boot harus ditulis ulang dalam `docs/architecture/boot_handoff.md`.

## 4I. Implementation Plan Ringkas

Implementation plan M2 berbentuk checkpoint: preflight M0/M1, build source kernel, link `kernel.elf`, inspect ELF, fetch/load Limine, build image ISO, run QEMU, validasi serial log, dan update readiness review. Setiap checkpoint menghasilkan artefak yang dapat diperiksa sebelum kernel dipromosikan ke gate berikutnya.

## 4J. Validation Plan Boot/Firmware

Validation plan M2 memakai QEMU dan OVMF sebagai target utama. Pengujian hardware fisik dinyatakan out of scope, tetapi rencana hardware dicatat sebagai future work. Negative test minimal mencakup OVMF hilang, ISO hilang, serial log kosong, entry point salah, dan artifact inspection gagal. Fuzz testing belum dijalankan pada M2 karena parser boot info belum diaktifkan; rencana fuzz untuk struktur handoff dan konfigurasi boot ditempatkan pada M3/M4.

## 4K. Failure Modes, Diagnostic, Recovery, dan Rollback

Failure modes M2 harus dikaitkan dengan diagnostic evidence: compiler output, linker output, `readelf`, `objdump`, `kernel.map`, QEMU command, dan serial log. Recovery dilakukan dengan memperbaiki checkpoint terdekat, bukan mengubah seluruh desain. Rollback dilakukan melalui branch `repair/M2-boot`, penyimpanan log kegagalan, dan kembali ke commit M1 yang lulus bila preflight gagal.

## 4L. Acceptance Criteria Boot/Firmware

Acceptance criteria M2 dianggap complete hanya jika evidence matrix lengkap: `kernel.elf`, `kernel.map`, hasil inspeksi ELF, ISO checksum, QEMU serial log, dan readiness review. Log serial harus memuat marker M2. Acceptance tidak boleh diberikan hanya karena `make build` berhasil.

## 4M. Secure Boot dan Measured Boot Scope Note

Secure boot belum diimplementasikan pada M2. Signature, key, revocation, dan tamper response hanya dicatat sebagai kebutuhan security milestone lanjutan. Measured boot juga belum diimplementasikan; TPM, PCR, event log, dan attestation belum menjadi acceptance criteria M2. Pernyataan ini sengaja dibuat agar tidak ada klaim keamanan boot yang melebihi bukti.

## 4N. Reproducibility dan Clean Rebuild

Reproducibility M2 minimum dibuktikan dengan clean rebuild: `make distclean && make check-src && make build && make inspect && make image`. Build disebut deterministic secara praktikum hanya jika perintah tersebut dapat diulang pada repository yang sama dan menghasilkan artifact inspection serta serial marker yang konsisten. Byte-for-byte reproducible ISO belum menjadi syarat wajib M2 karena timestamp, toolchain path, dan bootloader artifact dapat memengaruhi image; nondeterminism tersebut harus dicatat bila ditemukan.

---

## 5. Struktur Repository Setelah M2

Setelah M2, repository minimal harus memiliki struktur berikut.

```text
mcsos/
  README.md
  LICENSE
  Makefile
  linker.ld
  .gitignore
  configs/
    limine/
      limine.conf
  docs/
    architecture/
      boot_handoff.md
      invariants.md
    readiness/
      M2-boot-image.md
    security/
      threat_model.md
    testing/
      verification_matrix.md
  kernel/
    arch/
      x86_64/
        include/
          mcsos/
            arch/
              io.h
    core/
      kmain.c
      serial.c
    lib/
      memory.c
  tools/
    scripts/
      m2_preflight.sh
      fetch_limine.sh
      make_iso.sh
      run_qemu.sh
      run_qemu_debug.sh
      inspect_kernel.sh
      grade_m2.sh
  third_party/
    limine/                 # generated atau vendored sesuai policy kelas
  build/                    # generated, tidak dikomit
```

`build/`, `iso_root/`, dan hasil clone Limine dapat diperlakukan sebagai generated artifact jika kelas tidak mengharuskan vendoring. Apabila koneksi internet laboratorium tidak stabil, dosen dapat menyediakan snapshot Limine resmi dan checksum-nya, lalu mahasiswa menaruhnya pada `third_party/limine/`.

---

## 6. Kriteria Lulus Praktikum M2

Mahasiswa atau kelompok dinyatakan lulus M2 apabila seluruh kriteria berikut terpenuhi.

1. Repository berada di filesystem Linux WSL dan bukan di `/mnt/c`.
2. Bukti M0 dan M1 tersedia: dokumen baseline, metadata toolchain, proof object, dan readiness M1.
3. `make distclean && make all inspect` berhasil dari clean checkout.
4. `kernel.elf` adalah ELF64 x86_64 executable dengan entry point `0xffffffff80000000` atau alamat higher-half yang disetujui dosen.
5. `kernel.map`, `readelf-header.txt`, `readelf-program-headers.txt`, `objdump-disassembly.txt`, dan `nm-symbols.txt` tersedia.
6. Image bootable `build/mcsos.iso` berhasil dibuat.
7. QEMU/OVMF berjalan headless dan menulis `build/qemu-serial.log`.
8. Serial log memuat minimal tiga marker:
   - `MCSOS 260502 M2 boot path entered`
   - `[M2] early serial online`
   - `[M2] kernel reached controlled halt loop`
9. Tidak ada warning kompilasi karena `-Werror` aktif.
10. Semua script shell melewati pemeriksaan minimal `bash -n` dan, jika tersedia, `shellcheck`.
11. Perubahan Git dikomit dengan pesan yang jelas.
12. Laporan praktikum memuat screenshot/log yang cukup, analisis failure modes, dan readiness review.

---

## 7. Pemeriksaan Kesiapan M0 dan M1 Sebelum M2

Bagian ini wajib dijalankan sebelum menyalin atau menulis source M2. Tujuannya adalah mencegah mahasiswa memperbaiki bug yang salah. Jika preflight gagal, hentikan M2 dan perbaiki M0/M1 terlebih dahulu.

### 7.1 Pemeriksaan lokasi repository

Repository harus berada pada filesystem Linux WSL. Jalankan perintah berikut dari root repository.

```bash
pwd
case "$(pwd)" in
  /mnt/c/*|/mnt/d/*|/mnt/e/*)
    echo "ERROR: repository berada di filesystem Windows. Pindahkan ke ~/src/mcsos."
    exit 1
    ;;
  *)
    echo "OK: repository berada di filesystem Linux WSL."
    ;;
esac
```

Indikator benar: output menyatakan `OK`. Jika output menyatakan error, pindahkan repository.

```bash
mkdir -p ~/src
cp -a /mnt/c/path/ke/mcsos ~/src/mcsos
cd ~/src/mcsos
```

Jangan memakai `mv` lintas filesystem jika belum yakin semua permission dan symlink tersalin dengan benar. Setelah salin selesai, jalankan kembali preflight.

### 7.2 Pemeriksaan Git dan kebersihan branch

Perintah berikut memastikan repository sudah menjadi Git repository dan mahasiswa bekerja pada branch praktikum yang jelas.

```bash
git rev-parse --show-toplevel
git status --short
git branch --show-current
git log --oneline -5
```

Indikator benar: `git rev-parse` menampilkan root repository, branch bukan kosong, dan perubahan yang belum dikomit dapat dijelaskan. Jika banyak file generated masuk status Git, perbaiki `.gitignore`.

Tambahkan `.gitignore` minimum berikut jika belum ada.

```gitignore
build/
iso_root/
*.iso
*.img
*.elf
*.map
*.o
*.d
third_party/limine/
```

Catatan: jika dosen meminta Limine divendor untuk kelas offline, hapus `third_party/limine/` dari `.gitignore` dan simpan checksum resmi pada `docs/security/supply_chain.md`.

### 7.3 Pemeriksaan artefak M0

Jalankan pemeriksaan dokumen baseline.

```bash
test -f docs/architecture/overview.md
test -f docs/architecture/invariants.md
test -f docs/security/threat_model.md
test -f docs/testing/verification_matrix.md
test -f docs/readiness/gates.md
```

Jika salah satu gagal, perbaiki M0. Minimal, dokumen tersebut harus menyatakan target x86_64, Windows 11 x64, WSL 2, QEMU/OVMF, kernel monolitik pendidikan, bahasa C freestanding, non-goals, readiness gate, threat model awal, dan verification matrix.

### 7.4 Pemeriksaan artefak M1

Jalankan target M1 yang tersedia.

```bash
make meta
make check
make proof
make inspect-proof
make repro-check
```

Jika nama target berbeda karena implementasi M1 kelas, jalankan target ekuivalen yang menghasilkan:

| Artefak | Lokasi umum | Pemeriksaan |
|---|---|---|
| Toolchain metadata | `build/meta/toolchain-versions.txt` | Ada dan memuat versi compiler/linker/QEMU/GDB. |
| Host readiness | `build/meta/host-readiness.txt` | Ada dan memuat WSL/distro/kernel. |
| Freestanding object | `build/proof/freestanding_probe.o` | ELF64 relocatable x86_64. |
| Proof executable | `build/proof/freestanding_probe.elf` | Jika M1 memilikinya, harus bisa diinspeksi. |
| QEMU capability | `build/meta/qemu-capabilities.txt` | Ada jika target M1 mengharuskannya. |
| Readiness M1 | `docs/readiness/M1-toolchain.md` | Ada dan berisi pass/fail. |

Pemeriksaan manual object M1:

```bash
readelf -hW build/proof/freestanding_probe.o | tee build/proof/check-m1-readelf.txt
objdump -drwC build/proof/freestanding_probe.o | tee build/proof/check-m1-objdump.txt >/dev/null
grep -q "Class:.*ELF64" build/proof/check-m1-readelf.txt
grep -q "Machine:.*Advanced Micro Devices X86-64" build/proof/check-m1-readelf.txt
```

Jika `grep` gagal, object M1 bukan target x86_64 ELF yang benar. Perbaiki target triple dan flags M1 sebelum lanjut.

### 7.5 Pemeriksaan tool wajib M2

M2 membutuhkan tool tambahan untuk ISO dan boot. Jalankan:

```bash
for tool in git make clang ld.lld readelf objdump nm qemu-system-x86_64 xorriso python3; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf "OK: %s -> %s\n" "$tool" "$(command -v "$tool")"
  else
    printf "MISSING: %s\n" "$tool"
  fi
done
```

Jika ada `MISSING`, pasang paket yang sesuai.

```bash
sudo apt update
sudo apt install -y clang lld llvm binutils make git qemu-system-x86 qemu-utils ovmf xorriso mtools dosfstools python3
```

Catatan: nama paket dapat sedikit berbeda antar distribusi. Simpan output instalasi dan versi tool pada laporan.

### 7.6 Pemeriksaan OVMF

Jalankan:

```bash
find /usr/share -type f \( -name 'OVMF_CODE*.fd' -o -name 'OVMF_VARS*.fd' \) 2>/dev/null | sort
```

Indikator benar: terdapat file `OVMF_CODE*.fd` dan idealnya `OVMF_VARS*.fd`. Jika tidak ada:

```bash
sudo apt update
sudo apt install -y ovmf
```

Jika tetap tidak ditemukan, catat distribusi dan path paket OVMF, lalu konsultasikan dengan dosen. Jangan mengganti firmware tanpa mencatat ADR karena boot behavior dapat berubah.

---

## 8. Script Preflight M2

Buat file `tools/scripts/m2_preflight.sh`. Script ini mengotomasi pemeriksaan M0/M1/M2 sebelum build boot. Tujuannya adalah membuat kegagalan eksplisit dan mudah diperbaiki.

```bash
mkdir -p tools/scripts
cat > tools/scripts/m2_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT"
mkdir -p build/meta
REPORT="build/meta/m2-preflight.txt"
: > "$REPORT"

log() {
  printf '%s\n' "$*" | tee -a "$REPORT"
}

fail() {
  log "ERROR: $*"
  exit 1
}

need_cmd() {
  if command -v "$1" >/dev/null 2>&1; then
    log "OK command: $1 -> $(command -v "$1")"
  else
    fail "command tidak ditemukan: $1"
  fi
}

log "== M2 preflight MCSOS 260502 =="
log "root=$ROOT"
log "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"

case "$ROOT" in
  /mnt/c/*|/mnt/d/*|/mnt/e/*)
    fail "repository berada di filesystem Windows. Pindahkan ke filesystem Linux WSL, misalnya ~/src/mcsos."
    ;;
  *)
    log "OK filesystem: repository bukan /mnt/c, /mnt/d, atau /mnt/e"
    ;;
esac

need_cmd git
need_cmd make
need_cmd clang
need_cmd ld.lld
need_cmd readelf
need_cmd objdump
need_cmd nm
need_cmd qemu-system-x86_64
need_cmd xorriso
need_cmd python3

for f in \
  docs/architecture/overview.md \
  docs/architecture/invariants.md \
  docs/security/threat_model.md \
  docs/testing/verification_matrix.md; do
  if [ -f "$f" ]; then
    log "OK M0 file: $f"
  else
    fail "artefak M0 belum ada: $f"
  fi
done

if [ -f build/meta/toolchain-versions.txt ]; then
  log "OK M1 metadata: build/meta/toolchain-versions.txt"
else
  log "WARN: build/meta/toolchain-versions.txt belum ada; menjalankan make meta jika tersedia"
  if make -n meta >/dev/null 2>&1; then
    make meta
  else
    fail "target make meta tidak tersedia dan metadata M1 belum ada"
  fi
fi

if [ -f build/proof/freestanding_probe.o ]; then
  readelf -hW build/proof/freestanding_probe.o > build/meta/m2-check-m1-object-readelf.txt
  grep -q 'Class:.*ELF64' build/meta/m2-check-m1-object-readelf.txt || fail "object M1 bukan ELF64"
  grep -q 'Machine:.*Advanced Micro Devices X86-64' build/meta/m2-check-m1-object-readelf.txt || fail "object M1 bukan x86_64"
  log "OK M1 proof object: ELF64 x86_64"
else
  log "WARN: build/proof/freestanding_probe.o tidak ditemukan. Pastikan M1 sudah dinilai atau jalankan ulang target proof M1."
fi

if find /usr/share -type f \( -name 'OVMF_CODE*.fd' -o -name 'OVMF_VARS*.fd' \) 2>/dev/null | grep -q OVMF; then
  find /usr/share -type f \( -name 'OVMF_CODE*.fd' -o -name 'OVMF_VARS*.fd' \) 2>/dev/null | sort | tee -a "$REPORT"
else
  fail "OVMF tidak ditemukan pada /usr/share. Pasang paket ovmf."
fi

log "OK: preflight M2 selesai"
EOF
chmod +x tools/scripts/m2_preflight.sh
```

Jalankan:

```bash
bash -n tools/scripts/m2_preflight.sh
./tools/scripts/m2_preflight.sh
```

Jika tersedia, jalankan juga:

```bash
shellcheck tools/scripts/m2_preflight.sh
```

Indikator benar: `build/meta/m2-preflight.txt` berisi `OK: preflight M2 selesai`.

---

## 9. Source Code M2 yang Harus Dibuat

Source M2 di bawah ini sengaja kecil. Ia hanya melakukan port I/O serial, mencetak marker, lalu masuk ke halt loop. Tidak ada allocator, interrupt handler, scheduler, framebuffer, atau userspace. Pembatasan ini penting agar M2 mudah diuji dan kegagalan mudah dilokalisasi.

### 9.1 Header port I/O x86_64

Buat file `kernel/arch/x86_64/include/mcsos/arch/io.h`.

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
    outb(0x80, 0);
}

#endif
EOF
```

Kontrak teknis:

1. `outb` dan `inb` hanya dipakai untuk port I/O, bukan MMIO.
2. Clobber `memory` mencegah compiler menggeser akses memori di sekitar operasi I/O secara agresif.
3. Ini belum cukup untuk semua device driver; M2 hanya menggunakannya untuk UART COM1.
4. Tidak boleh dipakai dari userspace karena belum ada privilege boundary.

### 9.2 Driver serial awal

Buat file `kernel/core/serial.c`.

```bash
mkdir -p kernel/core
cat > kernel/core/serial.c <<'EOF'
#include <stdint.h>
#include <stddef.h>
#include <mcsos/arch/io.h>

#define COM1_PORT 0x3F8u

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
    if (c == '\n') {
        serial_putc('\r');
    }
    while (!serial_transmit_empty()) { }
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

Kontrak teknis:

1. Driver ini hanya untuk early boot dan debugging awal.
2. Driver melakukan busy-wait; belum aman untuk sistem preemptive/SMP.
3. Tidak ada locking karena M2 belum mengaktifkan multitasking.
4. Jika serial tidak muncul di log, diagnosis pertama adalah QEMU command, bukan scheduler atau memory manager.

### 9.3 Runtime memori minimal

Clang dalam mode freestanding tetap dapat menghasilkan panggilan ke `memcpy`, `memmove`, atau `memset` pada kondisi tertentu. Karena kernel belum memiliki libc, M2 menyediakan implementasi minimal.

Buat file `kernel/lib/memory.c`.

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

Kontrak teknis:

1. Fungsi ini belum dioptimalkan; targetnya correctness sederhana.
2. `memmove` menangani overlap.
3. Tidak ada dependency pada libc.
4. Pada milestone lanjut, fungsi ini dapat diganti dengan implementasi arsitektur-spesifik setelah ada test dan benchmark.

### 9.4 Kernel entry C

Buat file `kernel/core/kmain.c`.

```bash
cat > kernel/core/kmain.c <<'EOF'
void serial_init(void);
void serial_write(const char *s);

__attribute__((noreturn)) static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt" : : : "memory");
    }
}

void kmain(void) {
    serial_init();
    serial_write("MCSOS 260502 M2 boot path entered\n");
    serial_write("[M2] early serial online\n");
    serial_write("[M2] kernel reached controlled halt loop\n");
    halt_forever();
}
EOF
```

Kontrak teknis:

1. `kmain` adalah entry point yang ditentukan di linker script.
2. Tidak menggunakan parameter boot info pada M2 agar path awal tetap kecil.
3. `halt_forever` mencegah kernel kembali ke bootloader atau menjalankan memori tidak valid.
4. `cli; hlt` adalah pilihan konservatif untuk tahap ini. Pada M3/M4 akan diganti dengan interrupt/trap path yang benar.

---

## 10. Linker Script M2

Buat `linker.ld`.

```bash
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

Kontrak teknis:

1. `OUTPUT_FORMAT(elf64-x86-64)` memastikan output ELF64 x86_64.
2. `ENTRY(kmain)` menetapkan entry point eksplisit.
3. Alamat `0xffffffff80000000` menempatkan kernel pada higher-half region awal.
4. PHDR memisahkan permission konseptual text, rodata, dan data. Pada M2, hasil aktual bergantung pada section yang tidak kosong; jika `.data` kosong, PHDR data dapat tidak muncul sebagai segment aktif.
5. Jangan mengubah alamat entry tanpa memperbarui acceptance criteria dan readiness review.

---

## 11. Makefile M2

Buat atau ganti `Makefile` dengan versi berikut. Makefile ini memakai `.RECIPEPREFIX := >` agar mahasiswa tidak gagal karena spasi/tab pada recipe. GNU Make tetap diperlukan.

```bash
cat > Makefile <<'EOF'
.RECIPEPREFIX := >
SHELL := /usr/bin/env bash

ARCH := x86_64
BUILD_DIR := build
KERNEL := $(BUILD_DIR)/kernel.elf
MAP := $(BUILD_DIR)/kernel.map
CC := clang
LD := ld.lld
OBJDUMP := objdump
READELF := readelf
NM := nm

CFLAGS := --target=x86_64-unknown-none-elf -std=c17 -ffreestanding -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -fno-lto -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror -Ikernel/arch/x86_64/include
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T linker.ld -Map=$(MAP)
SRC_C := $(shell find kernel -name '*.c' | LC_ALL=C sort)
OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_C))

.PHONY: all build inspect image run debug check-prev check-src check-scripts grade clean distclean

all: build

check-prev:
>./tools/scripts/m2_preflight.sh

check-src:
>$(CC) --version | head -n 1
>$(LD) --version | head -n 1
>test -f linker.ld
>test -d kernel/core
>test -d kernel/lib
>test -d kernel/arch/x86_64/include

check-scripts:
>for s in tools/scripts/*.sh; do bash -n "$$s"; done
>if command -v shellcheck >/dev/null 2>&1; then shellcheck tools/scripts/*.sh; else echo "WARN: shellcheck tidak tersedia"; fi

build: $(KERNEL)

$(BUILD_DIR)/%.o: %.c
>mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJ) linker.ld
>mkdir -p $(BUILD_DIR)
>$(LD) $(LDFLAGS) -o $@ $(OBJ)

inspect: $(KERNEL)
>./tools/scripts/inspect_kernel.sh

image: $(KERNEL)
>./tools/scripts/make_iso.sh

run: image
>./tools/scripts/run_qemu.sh

debug: image
>./tools/scripts/run_qemu_debug.sh

grade: check-src check-scripts build inspect image run
>./tools/scripts/grade_m2.sh

clean:
>rm -rf $(BUILD_DIR)/kernel $(BUILD_DIR)/*.elf $(BUILD_DIR)/*.map $(BUILD_DIR)/inspect

distclean:
>rm -rf $(BUILD_DIR) iso_root
EOF
```

Jalankan validasi Makefile:

```bash
make distclean
make check-src
make build
make inspect
```

Indikator benar: tidak ada warning, `build/kernel.elf` terbentuk, dan folder `build/inspect/` berisi hasil inspeksi.

---

## 12. Script Inspeksi Kernel

Buat `tools/scripts/inspect_kernel.sh`.

```bash
cat > tools/scripts/inspect_kernel.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

KERNEL="build/kernel.elf"
OUT="build/inspect"
mkdir -p "$OUT"

test -f "$KERNEL"

readelf -hW "$KERNEL" | tee "$OUT/readelf-header.txt"
readelf -lW "$KERNEL" | tee "$OUT/readelf-program-headers.txt"
readelf -SW "$KERNEL" | tee "$OUT/readelf-sections.txt" >/dev/null
objdump -drwC "$KERNEL" | tee "$OUT/objdump-disassembly.txt" >/dev/null
nm -n "$KERNEL" | tee "$OUT/nm-symbols.txt" >/dev/null

 grep -q 'Class:.*ELF64' "$OUT/readelf-header.txt"
 grep -q 'Machine:.*Advanced Micro Devices X86-64' "$OUT/readelf-header.txt"
 grep -q 'Entry point address:.*0xffffffff80000000' "$OUT/readelf-header.txt"
 grep -q 'kmain' "$OUT/nm-symbols.txt"
 grep -q 'serial_init' "$OUT/nm-symbols.txt"
 grep -q 'serial_write' "$OUT/nm-symbols.txt"

echo "OK: kernel ELF inspection passed"
EOF
chmod +x tools/scripts/inspect_kernel.sh
```

Catatan: spasi sebelum `grep` tidak mengubah perilaku shell, tetapi tetap boleh dihapus. Jika entry point diubah melalui ADR dosen, perbarui nilai `0xffffffff80000000` pada script dan acceptance criteria.

---

## 13. Konfigurasi Limine

Buat `configs/limine/limine.conf`.

```bash
mkdir -p configs/limine
cat > configs/limine/limine.conf <<'EOF'
timeout: 0
serial: yes

/MCSOS 260502 M2
    protocol: limine
    path: boot():/boot/kernel.elf
    cmdline: mcsos.version=260502 mcsos.milestone=M2 console=serial
EOF
```

Penjelasan:

1. `timeout: 0` membuat boot langsung memilih entry pertama agar pengujian QEMU deterministik.
2. `serial: yes` mengaktifkan serial untuk bootloader jika didukung pada mode terkait.
3. `protocol: limine` menyatakan kernel dimuat melalui Limine protocol.
4. `path: boot():/boot/kernel.elf` menunjuk kernel di image ISO.
5. `cmdline` belum dipakai oleh kernel M2, tetapi dicatat untuk milestone berikutnya.

---

## 14. Script Fetch Limine

Buat `tools/scripts/fetch_limine.sh`.

```bash
cat > tools/scripts/fetch_limine.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

LIMINE_DIR="third_party/limine"
LIMINE_BRANCH="${LIMINE_BRANCH:-v11.x-binary}"
LIMINE_URL="${LIMINE_URL:-https://github.com/Limine-Bootloader/Limine.git}"

mkdir -p third_party build/meta

if [ -d "$LIMINE_DIR/.git" ]; then
  git -C "$LIMINE_DIR" fetch --depth=1 origin "$LIMINE_BRANCH"
  git -C "$LIMINE_DIR" checkout "$LIMINE_BRANCH"
else
  rm -rf "$LIMINE_DIR"
  git clone "$LIMINE_URL" --branch="$LIMINE_BRANCH" --depth=1 "$LIMINE_DIR"
fi

make -C "$LIMINE_DIR"

git -C "$LIMINE_DIR" rev-parse HEAD | tee build/meta/limine-revision.txt
printf 'branch=%s\nurl=%s\n' "$LIMINE_BRANCH" "$LIMINE_URL" | tee -a build/meta/limine-revision.txt

test -f "$LIMINE_DIR/limine-bios.sys"
test -f "$LIMINE_DIR/limine-bios-cd.bin"
test -f "$LIMINE_DIR/limine-uefi-cd.bin"
test -f "$LIMINE_DIR/BOOTX64.EFI"
test -x "$LIMINE_DIR/limine" || test -f "$LIMINE_DIR/limine"

echo "OK: Limine ready in $LIMINE_DIR"
EOF
chmod +x tools/scripts/fetch_limine.sh
```

Jalankan:

```bash
bash -n tools/scripts/fetch_limine.sh
./tools/scripts/fetch_limine.sh
```

Jika jaringan laboratorium memblokir GitHub, solusi konservatif:

1. Dosen menyediakan arsip Limine resmi yang telah diverifikasi checksum-nya.
2. Ekstrak ke `third_party/limine`.
3. Simpan checksum arsip pada `docs/security/supply_chain.md`.
4. Jalankan `make -C third_party/limine`.

Jangan mengganti bootloader atau branch tanpa ADR karena hasil boot dan konfigurasi dapat berubah.

---

## 15. Script Pembuatan ISO Bootable

Buat `tools/scripts/make_iso.sh`.

```bash
cat > tools/scripts/make_iso.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

KERNEL="build/kernel.elf"
ISO="build/mcsos.iso"
ISO_ROOT="iso_root"
LIMINE_DIR="third_party/limine"

if [ ! -f "$KERNEL" ]; then
  echo "ERROR: $KERNEL tidak ditemukan. Jalankan make build." >&2
  exit 1
fi

if [ ! -d "$LIMINE_DIR" ]; then
  ./tools/scripts/fetch_limine.sh
fi

mkdir -p "$ISO_ROOT/boot/limine" "$ISO_ROOT/EFI/BOOT" build
cp -v "$KERNEL" "$ISO_ROOT/boot/kernel.elf"
cp -v configs/limine/limine.conf "$ISO_ROOT/boot/limine/limine.conf"
cp -v "$LIMINE_DIR/limine-bios.sys" "$ISO_ROOT/boot/limine/"
cp -v "$LIMINE_DIR/limine-bios-cd.bin" "$ISO_ROOT/boot/limine/"
cp -v "$LIMINE_DIR/limine-uefi-cd.bin" "$ISO_ROOT/boot/limine/"
cp -v "$LIMINE_DIR/BOOTX64.EFI" "$ISO_ROOT/EFI/BOOT/BOOTX64.EFI"
if [ -f "$LIMINE_DIR/BOOTIA32.EFI" ]; then
  cp -v "$LIMINE_DIR/BOOTIA32.EFI" "$ISO_ROOT/EFI/BOOT/BOOTIA32.EFI"
fi

xorriso -as mkisofs \
  -R -r -J \
  -b boot/limine/limine-bios-cd.bin \
  -no-emul-boot -boot-load-size 4 -boot-info-table \
  --efi-boot boot/limine/limine-uefi-cd.bin \
  -efi-boot-part --efi-boot-image --protective-msdos-label \
  "$ISO_ROOT" -o "$ISO"

"$LIMINE_DIR/limine" bios-install "$ISO"
sha256sum "$ISO" | tee build/mcsos.iso.sha256

echo "OK: ISO dibuat pada $ISO"
EOF
chmod +x tools/scripts/make_iso.sh
```

Jalankan:

```bash
bash -n tools/scripts/make_iso.sh
make image
ls -lh build/mcsos.iso build/mcsos.iso.sha256
```

Indikator benar: `build/mcsos.iso` ada dan checksum SHA-256 tercatat.

---

## 16. Script QEMU Run Headless

Buat `tools/scripts/run_qemu.sh`.

```bash
cat > tools/scripts/run_qemu.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

ISO="build/mcsos.iso"
LOG="build/qemu-serial.log"
OVMF_CODE=""
OVMF_VARS_TEMPLATE=""
OVMF_VARS="build/OVMF_VARS.fd"

find_first() {
  for f in "$@"; do
    if [ -f "$f" ]; then
      printf '%s\n' "$f"
      return 0
    fi
  done
  return 1
}

OVMF_CODE="$(find_first \
  /usr/share/OVMF/OVMF_CODE_4M.fd \
  /usr/share/OVMF/OVMF_CODE.fd \
  /usr/share/edk2/ovmf/OVMF_CODE.fd \
  /usr/share/qemu/OVMF_CODE.fd || true)"

OVMF_VARS_TEMPLATE="$(find_first \
  /usr/share/OVMF/OVMF_VARS_4M.fd \
  /usr/share/OVMF/OVMF_VARS.fd \
  /usr/share/edk2/ovmf/OVMF_VARS.fd \
  /usr/share/qemu/OVMF_VARS.fd || true)"

if [ ! -f "$ISO" ]; then
  echo "ERROR: $ISO tidak ditemukan. Jalankan make image." >&2
  exit 1
fi

if [ -z "$OVMF_CODE" ]; then
  echo "ERROR: OVMF_CODE tidak ditemukan. Pasang paket ovmf." >&2
  exit 1
fi

rm -f "$LOG"
mkdir -p build

QEMU_ARGS=(
  -machine q35
  -cpu qemu64
  -m 512M
  -serial "file:$LOG"
  -display none
  -monitor none
  -no-reboot
  -no-shutdown
  -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE"
  -cdrom "$ISO"
)

if [ -n "$OVMF_VARS_TEMPLATE" ]; then
  cp "$OVMF_VARS_TEMPLATE" "$OVMF_VARS"
  QEMU_ARGS+=( -drive "if=pflash,format=raw,file=$OVMF_VARS" )
fi

timeout 10s qemu-system-x86_64 "${QEMU_ARGS[@]}" || status=$?
status="${status:-0}"
if [ "$status" != "0" ] && [ "$status" != "124" ]; then
  echo "ERROR: QEMU keluar dengan status $status" >&2
  exit "$status"
fi

if [ ! -s "$LOG" ]; then
  echo "ERROR: serial log kosong: $LOG" >&2
  exit 1
fi

grep -q 'MCSOS 260502 M2 boot path entered' "$LOG"
grep -q '\[M2\] early serial online' "$LOG"
grep -q '\[M2\] kernel reached controlled halt loop' "$LOG"

echo "OK: QEMU serial log valid: $LOG"
EOF
chmod +x tools/scripts/run_qemu.sh
```

Jalankan:

```bash
bash -n tools/scripts/run_qemu.sh
make run
cat build/qemu-serial.log
```

Jika QEMU berhenti karena `timeout 10s`, itu normal pada M2 karena kernel sengaja masuk halt loop. Yang penting adalah log serial berisi marker.

---

## 17. Script QEMU Debug dengan GDB Stub

Buat `tools/scripts/run_qemu_debug.sh`.

```bash
cat > tools/scripts/run_qemu_debug.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

ISO="build/mcsos.iso"
LOG="build/qemu-debug-serial.log"
OVMF_CODE=""

find_first() {
  for f in "$@"; do
    if [ -f "$f" ]; then
      printf '%s\n' "$f"
      return 0
    fi
  done
  return 1
}

OVMF_CODE="$(find_first \
  /usr/share/OVMF/OVMF_CODE_4M.fd \
  /usr/share/OVMF/OVMF_CODE.fd \
  /usr/share/edk2/ovmf/OVMF_CODE.fd \
  /usr/share/qemu/OVMF_CODE.fd || true)"

test -f "$ISO"
test -n "$OVMF_CODE"

rm -f "$LOG"
qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 512M \
  -serial "file:$LOG" \
  -display none \
  -monitor stdio \
  -no-reboot \
  -no-shutdown \
  -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" \
  -cdrom "$ISO" \
  -s -S
EOF
chmod +x tools/scripts/run_qemu_debug.sh
```

Cara menggunakan:

Terminal 1:

```bash
make debug
```

Terminal 2:

```bash
gdb build/kernel.elf
```

Di dalam GDB:

```gdb
set architecture i386:x86-64:intel
target remote localhost:1234
break kmain
continue
info registers
x/16i $rip
```

Jika GDB dapat breakpoint di `kmain`, simpan screenshot atau transcript sebagai evidence tambahan. Debug mode bukan syarat minimal M2 apabila QEMU run biasa sudah lulus, tetapi sangat dianjurkan untuk mahasiswa yang mengalami boot hang.

---

## 18. Script Grading Lokal M2

Buat `tools/scripts/grade_m2.sh`.

```bash
cat > tools/scripts/grade_m2.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

required_files=(
  build/kernel.elf
  build/kernel.map
  build/inspect/readelf-header.txt
  build/inspect/readelf-program-headers.txt
  build/inspect/objdump-disassembly.txt
  build/inspect/nm-symbols.txt
  build/mcsos.iso
  build/mcsos.iso.sha256
  build/qemu-serial.log
)

for f in "${required_files[@]}"; do
  if [ ! -s "$f" ]; then
    echo "ERROR: artefak tidak ada atau kosong: $f" >&2
    exit 1
  fi
  echo "OK artifact: $f"
done

grep -q 'Class:.*ELF64' build/inspect/readelf-header.txt
grep -q 'Machine:.*Advanced Micro Devices X86-64' build/inspect/readelf-header.txt
grep -q 'Entry point address:.*0xffffffff80000000' build/inspect/readelf-header.txt
grep -q 'MCSOS 260502 M2 boot path entered' build/qemu-serial.log
grep -q '\[M2\] early serial online' build/qemu-serial.log
grep -q '\[M2\] kernel reached controlled halt loop' build/qemu-serial.log

echo "OK: M2 local grading checks passed"
EOF
chmod +x tools/scripts/grade_m2.sh
```

Jalankan:

```bash
bash -n tools/scripts/grade_m2.sh
make grade
```

Indikator benar: `OK: M2 local grading checks passed`.

---

## 19. Urutan Kerja Langkah demi Langkah

### Langkah 1 — Pastikan repository bersih dan siap

Tujuan langkah ini adalah memastikan mahasiswa tidak membangun M2 di atas hasil M0/M1 yang rusak.

```bash
cd ~/src/mcsos
git status --short
./tools/scripts/m2_preflight.sh
```

Jika `m2_preflight.sh` belum ada, buat sesuai bagian sebelumnya. Jika Git menampilkan banyak file generated, perbaiki `.gitignore` lalu commit perubahan source dan dokumen yang relevan.

### Langkah 2 — Buat source tree M2

Tujuan langkah ini adalah menambahkan source minimal: I/O port, serial driver, runtime memory, dan kernel entry.

```bash
mkdir -p kernel/arch/x86_64/include/mcsos/arch kernel/core kernel/lib configs/limine tools/scripts
```

Salin seluruh source dari bagian 9 sampai 18. Setelah itu jalankan pemeriksaan syntax script.

```bash
make check-scripts
```

Jika `make check-scripts` gagal karena `shellcheck` belum tersedia, pesan `WARN` dapat diterima selama `bash -n` lulus. Namun untuk penilaian penuh, pasang `shellcheck`.

```bash
sudo apt install -y shellcheck
make check-scripts
```

### Langkah 3 — Build kernel ELF64

Tujuan langkah ini adalah membuktikan bahwa source M2 dapat dikompilasi dan dilink sebagai kernel ELF64 freestanding.

```bash
make distclean
make check-src
make build
```

Indikator benar: file `build/kernel.elf` dan `build/kernel.map` terbentuk. Jika gagal, baca error compiler pertama, bukan error berantai.

### Langkah 4 — Inspeksi kernel ELF

Tujuan langkah ini adalah memastikan kernel bukan executable Linux/Windows biasa dan entry point sesuai desain.

```bash
make inspect
```

Periksa:

```bash
cat build/inspect/readelf-header.txt
cat build/inspect/readelf-program-headers.txt
head -n 40 build/inspect/nm-symbols.txt
```

Indikator benar:

1. `Class: ELF64`.
2. `Machine: Advanced Micro Devices X86-64`.
3. `Entry point address: 0xffffffff80000000`.
4. Symbol `kmain`, `serial_init`, dan `serial_write` muncul.

### Langkah 5 — Ambil Limine

Tujuan langkah ini adalah menyiapkan bootloader referensi.

```bash
./tools/scripts/fetch_limine.sh
cat build/meta/limine-revision.txt
```

Jika gagal karena jaringan, gunakan paket Limine yang disediakan dosen. Jangan melanjutkan dengan bootloader lain tanpa ADR.

### Langkah 6 — Buat image ISO

Tujuan langkah ini adalah membungkus kernel ELF64 dan konfigurasi Limine ke image bootable.

```bash
make image
sha256sum -c build/mcsos.iso.sha256
```

Indikator benar: `build/mcsos.iso` ada dan checksum valid.

### Langkah 7 — Jalankan QEMU/OVMF

Tujuan langkah ini adalah membuktikan jalur boot masuk ke kernel.

```bash
make run
cat build/qemu-serial.log
```

Indikator benar: log memuat marker M2. Jika QEMU berjalan sampai timeout, hal itu normal karena kernel sengaja berhenti di halt loop.

### Langkah 8 — Jalankan grading lokal

Tujuan langkah ini adalah menyatukan semua pemeriksaan build, image, dan log serial.

```bash
make grade
```

Jika lulus, buat dokumen readiness M2.

### Langkah 9 — Commit hasil

```bash
git status --short
git add Makefile linker.ld configs/limine/limine.conf kernel tools docs .gitignore
git commit -m "M2: add bootable kernel ELF and early serial console"
git rev-parse HEAD | tee build/meta/m2-commit.txt
```

Jangan commit `build/` kecuali dosen secara eksplisit meminta artefak release. Untuk laporan, salin log dan bukti penting sebagai lampiran atau unggahan terpisah.

---

## 20. Failure Modes dan Solusi Perbaikan

### 20.1 Repository berada di `/mnt/c`

Gejala:

```text
ERROR: repository berada di filesystem Windows
```

Penyebab: repository dikerjakan di filesystem Windows. Ini dapat menyebabkan masalah permission, executable bit, symlink, newline, dan performa.

Perbaikan:

```bash
mkdir -p ~/src
cp -a /mnt/c/path/ke/mcsos ~/src/mcsos
cd ~/src/mcsos
./tools/scripts/m2_preflight.sh
```

### 20.2 `clang` tidak mengenali target atau memakai compiler host salah

Gejala:

```text
clang: error: unknown argument
cc: error: unrecognized command-line option '--target=x86_64-unknown-none-elf'
```

Penyebab: Makefile memakai `cc` atau compiler host bukan Clang.

Perbaikan: pastikan Makefile memakai:

```makefile
CC := clang
LD := ld.lld
```

Lalu jalankan:

```bash
make distclean
make check-src
make build
```

### 20.3 `ld.lld` tidak ditemukan

Gejala:

```text
ld.lld: command not found
```

Perbaikan:

```bash
sudo apt update
sudo apt install -y lld llvm
command -v ld.lld
ld.lld --version
```

### 20.4 Undefined symbol `memcpy`, `memset`, atau `memmove`

Gejala:

```text
undefined symbol: memcpy
undefined symbol: memset
```

Penyebab: compiler menghasilkan panggilan runtime memori, tetapi kernel belum menyediakan libc.

Perbaikan: pastikan `kernel/lib/memory.c` ada dan masuk daftar source Makefile.

```bash
find kernel -name '*.c' | sort
make distclean
make build
```

### 20.5 Entry point tidak sesuai

Gejala:

```text
grep Entry point failed
```

Penyebab: `ENTRY(kmain)` hilang, `kmain` tidak diekspor, atau linker script berubah.

Perbaikan:

```bash
grep -n 'ENTRY' linker.ld
nm -n build/kernel.elf | grep kmain
readelf -hW build/kernel.elf | grep 'Entry point'
```

Pastikan `kmain` tidak `static` dan linker script memuat `ENTRY(kmain)`.

### 20.6 Limine gagal di-clone

Gejala:

```text
fatal: unable to access
```

Penyebab: jaringan, proxy, DNS, atau GitHub tidak dapat diakses.

Perbaikan:

1. Gunakan jaringan yang diizinkan.
2. Gunakan arsip Limine yang disediakan dosen.
3. Catat checksum dan sumber arsip.
4. Jangan mengganti branch tanpa ADR.

### 20.7 `xorriso` tidak ditemukan

Gejala:

```text
xorriso: command not found
```

Perbaikan:

```bash
sudo apt update
sudo apt install -y xorriso
```

### 20.8 OVMF tidak ditemukan

Gejala:

```text
ERROR: OVMF_CODE tidak ditemukan
```

Perbaikan:

```bash
sudo apt update
sudo apt install -y ovmf
find /usr/share -type f \( -name 'OVMF_CODE*.fd' -o -name 'OVMF_VARS*.fd' \) 2>/dev/null | sort
```

Jika distribusi memakai path berbeda, perbarui script `run_qemu.sh` dengan path yang ditemukan dan catat dalam laporan.

### 20.9 QEMU log kosong

Gejala:

```text
ERROR: serial log kosong
```

Kemungkinan penyebab:

1. QEMU tidak benar-benar boot dari ISO.
2. OVMF gagal memuat Limine.
3. Limine config tidak ditemukan.
4. Kernel tidak masuk `kmain`.
5. Serial COM1 tidak diarahkan ke file.

Urutan diagnosis:

```bash
ls -lh build/mcsos.iso
find iso_root -maxdepth 4 -type f | sort
cat configs/limine/limine.conf
qemu-system-x86_64 --version
./tools/scripts/run_qemu_debug.sh
```

Gunakan monitor QEMU/GDB jika perlu. Jangan langsung mengubah kode kernel sebelum memastikan image layout benar.

### 20.10 QEMU reboot loop

Gejala: QEMU berulang-ulang boot, log tidak stabil, atau `-no-reboot` menghentikan VM.

Kemungkinan penyebab:

1. Triple fault karena entry point salah.
2. Bootloader memuat kernel tidak sesuai format.
3. Linker script menghasilkan segment tidak dapat dimuat.
4. CPU menjalankan instruksi illegal.

Perbaikan awal:

```bash
make inspect
readelf -lW build/kernel.elf
objdump -drwC build/kernel.elf | head -n 80
make debug
```

Pasang breakpoint `kmain`. Jika breakpoint tidak pernah tercapai, fokus pada bootloader/image/linker. Jika tercapai tetapi log kosong, fokus pada serial driver atau QEMU serial option.

### 20.11 Permission denied pada script

Gejala:

```text
Permission denied
```

Perbaikan:

```bash
chmod +x tools/scripts/*.sh
git update-index --chmod=+x tools/scripts/*.sh
git status --short
```

### 20.12 CRLF Windows merusak script

Gejala:

```text
/usr/bin/env: ‘bash\r’: No such file or directory
```

Perbaikan:

```bash
sudo apt install -y dos2unix
dos2unix tools/scripts/*.sh Makefile linker.ld configs/limine/limine.conf
```

Tambahkan `.gitattributes`:

```gitattributes
*.sh text eol=lf
Makefile text eol=lf
*.ld text eol=lf
*.c text eol=lf
*.h text eol=lf
*.conf text eol=lf
```

---

## 21. Checkpoint Buildable

| Checkpoint | Perintah | Artefak | Lulus jika |
|---|---|---|---|
| CP-M2.1 Preflight | `./tools/scripts/m2_preflight.sh` | `build/meta/m2-preflight.txt` | Semua pemeriksaan wajib OK. |
| CP-M2.2 Source syntax | `make check-scripts` | output shell lint | `bash -n` lulus. |
| CP-M2.3 Build ELF | `make build` | `build/kernel.elf`, `build/kernel.map` | Tidak ada warning/error. |
| CP-M2.4 Inspect ELF | `make inspect` | `build/inspect/*` | ELF64 x86_64 dan entry benar. |
| CP-M2.5 Fetch bootloader | `./tools/scripts/fetch_limine.sh` | `build/meta/limine-revision.txt` | Limine tersedia. |
| CP-M2.6 Build image | `make image` | `build/mcsos.iso` | ISO dan checksum terbentuk. |
| CP-M2.7 QEMU run | `make run` | `build/qemu-serial.log` | Marker M2 muncul. |
| CP-M2.8 Local grade | `make grade` | pass/fail output | Semua artefak valid. |

---

## 22. Bukti yang Harus Dikumpulkan

Simpan bukti berikut pada laporan praktikum.

| Bukti | Lokasi | Cara menampilkan |
|---|---|---|
| Commit hash | `build/meta/m2-commit.txt` | `cat build/meta/m2-commit.txt` |
| Toolchain versions | `build/meta/toolchain-versions.txt` | dari M1 atau `make meta` |
| Preflight M2 | `build/meta/m2-preflight.txt` | `cat build/meta/m2-preflight.txt` |
| Kernel ELF header | `build/inspect/readelf-header.txt` | `cat` atau screenshot |
| Program headers | `build/inspect/readelf-program-headers.txt` | `cat` atau screenshot |
| Symbol list | `build/inspect/nm-symbols.txt` | `grep kmain` |
| Disassembly | `build/inspect/objdump-disassembly.txt` | cuplikan fungsi `kmain`, `serial_init`, `outb`, `inb` |
| Kernel map | `build/kernel.map` | cuplikan symbol boundary |
| ISO checksum | `build/mcsos.iso.sha256` | `cat` |
| QEMU serial log | `build/qemu-serial.log` | `cat` |
| Git status | output `git status --short` | harus bersih setelah commit |
| Readiness review | `docs/readiness/M2-boot-image.md` | isi dokumen |

---

## 23. Template Readiness Review M2

Buat file `docs/readiness/M2-boot-image.md`.

```bash
mkdir -p docs/readiness
cat > docs/readiness/M2-boot-image.md <<'EOF'
# Readiness Review M2 - Boot Image dan Early Serial Console

## Identitas

- Proyek: MCSOS 260502
- Praktikum: M2
- Target: x86_64, QEMU, OVMF, Limine
- Nama/Kelompok:
- Commit hash:
- Tanggal:

## Ringkasan Status

Status yang diajukan: siap uji QEMU tahap M2 / belum siap uji QEMU tahap M2.

Alasan ringkas:

## Evidence Matrix

| Evidence | Lokasi | Status | Catatan |
|---|---|---|---|
| Preflight M2 | `build/meta/m2-preflight.txt` | PASS/FAIL | |
| Kernel ELF | `build/kernel.elf` | PASS/FAIL | |
| Kernel map | `build/kernel.map` | PASS/FAIL | |
| readelf header | `build/inspect/readelf-header.txt` | PASS/FAIL | |
| readelf PHDR | `build/inspect/readelf-program-headers.txt` | PASS/FAIL | |
| objdump | `build/inspect/objdump-disassembly.txt` | PASS/FAIL | |
| ISO | `build/mcsos.iso` | PASS/FAIL | |
| ISO checksum | `build/mcsos.iso.sha256` | PASS/FAIL | |
| Serial log | `build/qemu-serial.log` | PASS/FAIL | |
| Git commit | `build/meta/m2-commit.txt` | PASS/FAIL | |

## Invariants yang Diperiksa

1. Kernel adalah ELF64 x86_64.
2. Entry point sesuai linker script.
3. Kernel tidak memakai hosted libc.
4. Source dikompilasi dengan `-ffreestanding` dan `-mno-red-zone`.
5. Serial console tersedia sebelum subsistem kompleks.
6. Kernel tidak kembali setelah `kmain`.
7. Output QEMU disimpan sebagai log file.

## Failure Modes yang Diuji atau Dianalisis

| Failure mode | Pernah terjadi? | Diagnosis | Perbaikan |
|---|---|---|---|
| Toolchain salah | Ya/Tidak | | |
| OVMF tidak ditemukan | Ya/Tidak | | |
| Limine gagal fetch | Ya/Tidak | | |
| ISO gagal dibuat | Ya/Tidak | | |
| QEMU log kosong | Ya/Tidak | | |
| Entry point salah | Ya/Tidak | | |
| Reboot loop | Ya/Tidak | | |
| CRLF script | Ya/Tidak | | |

## Keputusan Readiness

- [ ] Lulus M2: siap uji QEMU tahap M2.
- [ ] Belum lulus M2: perlu perbaikan.

## Catatan Reviewer

EOF
```

Isi file tersebut setelah `make grade` selesai.

---

## 24. Pertanyaan Analisis

Jawab pertanyaan berikut pada laporan.

1. Mengapa M2 tidak boleh menggunakan `printf` dari libc host?
2. Apa fungsi `-ffreestanding`, `-nostdlib`, dan `-mno-red-zone` pada kernel awal?
3. Mengapa serial console lebih diutamakan daripada framebuffer pada M2?
4. Mengapa `kernel.elf` harus diperiksa dengan `readelf` dan `objdump`, bukan hanya dilihat dari keberhasilan `make build`?
5. Apa risiko jika repository dikerjakan di `/mnt/c` pada WSL?
6. Jelaskan perbedaan kegagalan build, kegagalan link, kegagalan image, dan kegagalan runtime QEMU.
7. Mengapa QEMU timeout tidak otomatis berarti gagal pada M2?
8. Jika serial log kosong, urutan diagnosis apa yang paling rasional?
9. Mengapa `kmain` tidak boleh kembali?
10. Apa bukti minimum yang diperlukan agar M2 dapat dinyatakan siap uji QEMU tahap boot awal?

---

## 25. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator penilaian |
|---|---:|---|
| Kebenaran fungsional | 30 | `kernel.elf` terbentuk, ISO terbentuk, QEMU/OVMF berjalan, serial log memuat marker M2. |
| Kualitas desain dan invariants | 20 | Entry contract, linker layout, freestanding assumptions, serial driver boundary, dan halt behavior dijelaskan. |
| Pengujian dan bukti | 20 | `readelf`, `objdump`, `nm`, `kernel.map`, checksum ISO, preflight, dan serial log lengkap. |
| Debugging dan failure analysis | 10 | Failure modes dianalisis dengan diagnosis dan solusi yang tepat. |
| Keamanan dan robustness | 10 | Supply-chain Limine, generated artifact policy, fail-closed behavior, dan tidak ada klaim berlebihan. |
| Dokumentasi dan laporan | 10 | Laporan mengikuti template, jelas, menyertakan commit hash, bukti, screenshot/log, dan readiness review. |

Pengurangan nilai wajib:

1. Mengklaim OS “tanpa error” atau “siap produksi”: pengurangan signifikan.
2. Tidak menyertakan log serial: M2 tidak lulus fungsional.
3. Tidak menyertakan inspeksi ELF: M2 tidak lulus evidence gate.
4. Repository di `/mnt/c` tanpa justifikasi dan bukti mitigasi: pengurangan readiness.
5. File generated besar dikomit tanpa instruksi dosen: pengurangan dokumentasi/governance.

---

## 26. Prosedur Rollback

Jika M2 gagal dan perbaikan tidak jelas, lakukan rollback terkontrol.

1. Simpan log kegagalan:

```bash
mkdir -p build/failure/M2
cp -a build/meta build/inspect build/*.log build/failure/M2/ 2>/dev/null || true
git diff > build/failure/M2/worktree.diff || true
```

2. Kembali ke commit M1 terakhir yang lulus:

```bash
git log --oneline --decorate -10
git switch -c repair/M2-boot
```

3. Reset hanya file M2 yang dicurigai, bukan seluruh repository tanpa analisis:

```bash
git checkout HEAD -- Makefile linker.ld kernel tools/scripts configs/limine
```

4. Jalankan preflight ulang:

```bash
./tools/scripts/m2_preflight.sh
```

5. Terapkan ulang source M2 secara bertahap: `io.h`, `serial.c`, `memory.c`, `kmain.c`, `linker.ld`, `Makefile`, script image, script QEMU.

6. Setelah lulus, commit dengan pesan perbaikan:

```bash
git add .
git commit -m "M2: repair boot image and serial console path"
```

---

## 27. Batasan M2

M2 belum memiliki:

1. Boot info parsing.
2. Memory map validation.
3. Physical memory manager.
4. Virtual memory manager milik kernel.
5. IDT/GDT/TSS milik kernel.
6. Interrupt/trap handler.
7. Panic path penuh.
8. Scheduler.
9. Syscall ABI.
10. Userspace.
11. Filesystem.
12. Network stack.
13. Security policy beyond early fail-closed design.
14. Hardware bring-up evidence.

Semua fitur tersebut masuk milestone berikutnya. Jangan menambahkan fitur-fitur tersebut ke M2 kecuali sebagai pengayaan yang dipisahkan jelas dari tugas wajib.

---

## 28. Tugas Wajib, Pengayaan, dan Tantangan Riset

### 28.1 Tugas wajib

1. Menjalankan preflight M0/M1/M2.
2. Membuat source M2 sesuai panduan.
3. Build `kernel.elf`.
4. Inspeksi ELF.
5. Membuat ISO bootable.
6. Menjalankan QEMU/OVMF.
7. Mengumpulkan serial log.
8. Menyusun readiness review dan laporan.

### 28.2 Pengayaan

1. Menambahkan `run_qemu_debug.sh` dan transcript GDB breakpoint di `kmain`.
2. Menambahkan `serial_write_hex64` untuk mencetak alamat `kmain` dan `__kernel_start`.
3. Menambahkan pemeriksaan `.comment` untuk memastikan LLD dipakai.
4. Menambahkan CI lokal sederhana yang menjalankan `make grade`.

### 28.3 Tantangan riset

1. Bandingkan hasil PHDR antara `ld.lld` dan GNU `ld`.
2. Uji perbedaan QEMU `q35` dan `pc` pada boot M2.
3. Buat ADR tentang pilihan Limine dibanding GRUB/Multiboot2 untuk MCSOS.
4. Buat boot log classifier yang membedakan firmware failure, bootloader failure, dan kernel entry failure.

---

## 29. Template Laporan Praktikum M2

Gunakan template laporan praktikum umum `os_template_laporan_praktikum.md`, lalu isi bagian khusus M2 berikut.

### 29.1 Sampul

- Judul: Praktikum M2 — Boot Image, Kernel ELF64, dan Early Serial Console MCSOS 260502
- Nama mahasiswa/NIM atau nama kelompok/anggota
- Kelas
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia

### 29.2 Lingkungan

Isi tabel:

| Komponen | Versi/output | Bukti |
|---|---|---|
| Windows | | |
| WSL | | |
| Distro | | |
| Kernel Linux WSL | | |
| Clang | | |
| LLD | | |
| Binutils | | |
| QEMU | | |
| OVMF path | | |
| Limine branch/revision | | |
| Git commit | | |

### 29.3 Desain

Jelaskan:

1. Alur boot firmware -> Limine -> `kernel.elf` -> `kmain` -> serial -> halt loop.
2. Struktur source M2.
3. Linker layout.
4. Invariants M2.
5. Batasan M2.

### 29.4 Langkah kerja

Tuliskan perintah yang dijalankan dan alasan teknisnya. Jangan hanya menempel output.

### 29.5 Hasil uji

Sertakan:

1. Output `make build`.
2. Output `make inspect`.
3. `readelf-header.txt`.
4. `readelf-program-headers.txt`.
5. Cuplikan `objdump`.
6. `mcsos.iso.sha256`.
7. `qemu-serial.log`.
8. Output `make grade`.

### 29.6 Analisis

Jawab pertanyaan analisis pada bagian 24.

### 29.7 Readiness Review

Sertakan isi `docs/readiness/M2-boot-image.md`.

---

## 30. Readiness Review Akhir M2

Hasil M2 hanya dapat diberi salah satu status berikut.

| Status | Kriteria |
|---|---|
| Belum siap uji QEMU | Build, image, atau serial log gagal; evidence tidak lengkap. |
| Siap uji QEMU tahap M2 | Build ELF, image, QEMU/OVMF, dan serial marker lulus dengan evidence lengkap. |
| Siap demonstrasi praktikum terbatas | Selain lulus M2, mahasiswa mampu menjelaskan desain, failure modes, rollback, dan debug GDB. |

M2 tidak boleh diberi status “siap produksi”, “tanpa error”, atau “siap hardware umum”. Bring-up perangkat keras baru hanya boleh dibahas setelah observability, panic path, interrupt/trap handling, memory manager, dan hardware matrix tersedia pada milestone berikutnya.

---

## 31. Referensi

[1] Microsoft, “Install WSL,” *Microsoft Learn*. Accessed: 2026-05-02. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/install  
[2] Microsoft, “Basic commands for WSL,” *Microsoft Learn*. Accessed: 2026-05-02. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/basic-commands  
[3] QEMU Project, “Invocation,” *QEMU documentation*. Accessed: 2026-05-02. [Online]. Available: https://www.qemu.org/docs/master/system/invocation.html  
[4] Limine Bootloader Project, “Limine,” *GitHub repository README*. Accessed: 2026-05-02. [Online]. Available: https://github.com/limine-bootloader/limine  
[5] Limine Bootloader Project, “Limine configuration file,” *CONFIG.md*. Accessed: 2026-05-02. [Online]. Available: https://github.com/limine-bootloader/limine/blob/v11.x/CONFIG.md  
[6] OSDev Wiki, “Limine Bare Bones,” *OSDev Wiki*. Accessed: 2026-05-02. [Online]. Available: https://wiki.osdev.org/Limine_Bare_Bones  
[7] OSDev Wiki, “Higher Half Kernel,” *OSDev Wiki*. Accessed: 2026-05-02. [Online]. Available: https://wiki.osdev.org/Higher_Half_Kernel  
[8] LLVM Project, “Clang Compiler User’s Manual — Freestanding Builds,” *Clang documentation*. Accessed: 2026-05-02. [Online]. Available: https://clang.llvm.org/docs/UsersManual.html  
[9] GNU Project, “C Dialect Options,” *Using the GNU Compiler Collection*. Accessed: 2026-05-02. [Online]. Available: https://gcc.gnu.org/onlinedocs/gcc-13.1.0/gcc/C-Dialect-Options.html  
[10] LLVM Project, “LLD — The LLVM Linker,” *LLD documentation*. Accessed: 2026-05-02. [Online]. Available: https://lld.llvm.org/  
[11] LLVM Project, “Linker Script implementation notes and policy,” *LLD documentation*. Accessed: 2026-05-02. [Online]. Available: https://lld.llvm.org/ELF/linker_script.html  
[12] GNU Project, “GNU make manual,” *GNU Make documentation*. Accessed: 2026-05-02. [Online]. Available: https://www.gnu.org/software/make/manual/make.html  
[13] GNU Project, “GNU Binutils,” *GNU Binutils documentation*. Accessed: 2026-05-02. [Online]. Available: https://www.gnu.org/software/binutils/binutils.html  
[14] GNU Project, “readelf,” *GNU Binary Utilities*. Accessed: 2026-05-02. [Online]. Available: https://www.sourceware.org/binutils/docs/binutils/readelf.html  
[15] GNU Project, “objdump,” *GNU Binary Utilities*. Accessed: 2026-05-02. [Online]. Available: https://www.sourceware.org/binutils/docs/binutils/objdump.html
