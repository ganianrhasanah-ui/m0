# OS_panduan_M0.md

# Panduan Praktikum M0 — Baseline Requirements, Governance, dan Lingkungan Pengembangan Reproducible MCSOS 260502

**Mata kuliah:** Sistem Operasi Lanjut / Praktikum Sistem Operasi  
**Proyek:** MCSOS versi 260502  
**Target awal:** x86_64, QEMU, UEFI/OVMF, kernel monolitik pendidikan bertahap  
**Host pengembangan:** Windows 11 x64 dengan WSL 2 Linux environment  
**Bahasa implementasi tahap awal:** freestanding C17 dengan assembly x86_64 minimal  
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi:** Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia  
**Status readiness akhir praktikum:** *siap untuk memasuki M1 apabila seluruh bukti lingkungan, repository, dokumen baseline, dan pemeriksaan toolchain tersedia*. Praktikum M0 tidak mengklaim kernel siap boot, tidak mengklaim sistem operasi siap pakai, dan tidak mengklaim bebas cacat.

---

## 0. Ringkasan Praktikum

Praktikum M0 adalah fondasi administratif, teknis, dan rekayasa sistem untuk seluruh rangkaian pengembangan MCSOS. Pada tahap ini mahasiswa belum menulis kernel fungsional. Fokus M0 adalah memastikan bahwa setiap mahasiswa atau kelompok memiliki lingkungan pengembangan yang dapat direproduksi, struktur repository yang seragam, baseline requirements yang jelas, threat model awal, risk register, verification matrix, serta mekanisme pengumpulan bukti teknis yang akan digunakan pada M1 sampai milestone lanjutan.

M0 sengaja ditempatkan sebelum praktikum boot, linker, kernel entry, interrupt, memory manager, scheduler, filesystem, networking, dan security karena kesalahan lingkungan pengembangan sering tampak seperti kesalahan kernel. Compiler yang salah, linker host yang tidak sesuai, file repository yang berada di filesystem Windows dengan perilaku permission berbeda, QEMU yang tidak tercatat versinya, atau konfigurasi WSL yang tidak terdokumentasi dapat membuat hasil praktikum tidak dapat direproduksi. Karena itu, M0 memperlakukan toolchain dan proses dokumentasi sebagai bagian dari trusted development base.

Target akhir M0 bukan sistem operasi yang dapat boot. Target akhirnya adalah *baseline proyek yang dapat diaudit*: repository bersih, versi tool tercatat, perintah validasi berjalan, struktur dokumen tersedia, dan laporan praktikum memuat bukti yang cukup untuk melanjutkan ke M1.

---

## 1. Identitas Praktikum

| Komponen | Keterangan |
|---|---|
| Kode praktikum | M0 |
| Judul praktikum | Baseline Requirements, Governance, dan Lingkungan Pengembangan Reproducible |
| Sistem operasi proyek | MCSOS 260502 |
| Arsitektur target | x86_64 / AMD64 / Intel 64 |
| Host utama | Windows 11 x64 |
| Lingkungan build | WSL 2, direkomendasikan Ubuntu 24.04 LTS atau Ubuntu LTS yang distandarkan kelas |
| Emulator utama | QEMU system emulator untuk x86_64 |
| Firmware emulator | OVMF untuk jalur UEFI |
| Model awal kernel | Monolithic educational kernel dengan batas modul internal dan readiness gates |
| Mode kerja | Individu atau kelompok |
| Artefak utama | Repository awal, metadata toolchain, dokumen baseline, risk register, verification matrix, laporan praktikum |

---

## 2. Capaian Pembelajaran Praktikum

Setelah menyelesaikan praktikum M0, mahasiswa mampu:

1. Menjelaskan mengapa pengembangan sistem operasi memerlukan lingkungan build yang terisolasi, terdokumentasi, dan dapat direproduksi.
2. Menginstal dan memverifikasi WSL 2 pada Windows 11 x64 sesuai prosedur resmi Microsoft [1], [2].
3. Menyiapkan distribusi Linux WSL untuk pengembangan OS dengan toolchain, emulator, debugger, assembler, static analysis, dan utilitas image dasar.
4. Membuat struktur repository awal MCSOS yang konsisten dengan roadmap pengembangan bertahap.
5. Membuat dokumen baseline requirements, non-goals, assumptions, threat model awal, risk register, dan verification matrix.
6. Membuat script validasi lingkungan yang mencatat versi toolchain dan mendeteksi kesalahan konfigurasi umum.
7. Memahami bahwa bukti teknis berupa log, commit hash, versi tool, checksum, dan hasil pemeriksaan object file adalah bagian dari penilaian praktikum.
8. Membedakan status *siap uji lingkungan*, *siap uji QEMU*, *siap demonstrasi praktikum*, dan klaim yang tidak boleh digunakan seperti “tanpa error” atau “siap produksi”.

---

## 3. Prasyarat Teori

Sebelum mengerjakan M0, mahasiswa harus meninjau konsep berikut.

| Konsep | Alasan diperlukan pada M0 |
|---|---|
| Arsitektur komputer x86_64 | Menentukan target object, ABI, dan emulator yang benar. |
| Perbedaan host dan target | Host adalah Windows/WSL; target adalah bare-metal x86_64 tanpa OS host. |
| Cross-compilation | Kernel tidak boleh dikompilasi seolah-olah program Linux atau Windows biasa. |
| ELF object dan symbol metadata | Setiap object awal harus dapat diperiksa dengan `readelf`, `objdump`, atau tool setara. |
| Version control | Semua perubahan praktikum harus terlacak melalui Git. |
| Reproducible build | Hasil build harus dapat diulang dari clean checkout dengan tool yang sama. |
| Threat model awal | Sejak M0, proyek harus memiliki batas trust dan risiko eksplisit. |
| Verification matrix | Setiap requirement harus mempunyai bukti validasi yang dapat diperiksa. |

---

## 4. Peta Skill yang Digunakan

| Skill | Peran dalam M0 | Artefak yang dihasilkan |
|---|---|---|
| `osdev-general` | Menetapkan roadmap, gates, dan struktur milestone | `docs/readiness/gates.md`, `docs/architecture/overview.md` |
| `osdev-01-computer-foundation` | Menentukan invariants, state model, dan verification matrix | `docs/architecture/invariants.md`, `docs/testing/verification_matrix.md` |
| `osdev-02-low-level-programming` | Menetapkan target triple, freestanding assumptions, ABI, object inspection | `docs/toolchain/abi_baseline.md`, object smoke test |
| `osdev-03-computer-and-hardware-architecture` | Menetapkan target x86_64, UEFI/OVMF, QEMU, CPU feature policy | `docs/architecture/target_platform.md` |
| `osdev-04-kernel-development` | Menentukan batas kernel awal, panic path, observability obligation | `docs/architecture/kernel_baseline.md` |
| `osdev-05-filesystem-development` | Menetapkan rencana VFS dan filesystem masa depan, bukan implementasi M0 | `docs/architecture/fs_scope.md` |
| `osdev-06-networking-stack` | Menetapkan rencana networking masa depan, packet safety, dan pcap evidence | `docs/architecture/net_scope.md` |
| `osdev-07-os-security` | Menyusun threat model, trust boundary, dan fail-closed policy awal | `docs/security/threat_model.md` |
| `osdev-08-device-driver-development` | Menetapkan driver model masa depan dan boundary MMIO/DMA | `docs/architecture/driver_scope.md` |
| `osdev-09-virtualization-and-containerization` | Menetapkan QEMU sebagai platform uji dan scope virtualization/container tahap lanjut | `docs/architecture/virt_scope.md` |
| `osdev-10-boot-firmware` | Menetapkan UEFI/OVMF dan Limine sebagai jalur boot awal yang direkomendasikan | `docs/architecture/boot_baseline.md` |
| `osdev-11-graphics-display` | Menetapkan framebuffer/early console sebagai target lanjutan | `docs/architecture/display_scope.md` |
| `osdev-12-toolchain-devenv` | Menyusun BOM toolchain, script validasi, QEMU/GDB workflow | `build/meta/toolchain-versions.txt`, `tools/check_env.sh` |
| `osdev-13-enterprise-features` | Menetapkan observability, update/rollback, incident log, long-haul test sebagai arah lanjutan | `docs/operations/observability_baseline.md` |
| `osdev-14-cross-science` | Menyusun requirements, risk register, reliability assumptions, governance | `docs/governance/risk_register.md` |

---

## 5. Aturan Kerja Individu dan Kelompok

Praktikum dapat dikerjakan secara individu atau kelompok. Jika dikerjakan secara kelompok, semua anggota wajib memahami seluruh isi repository, bukan hanya file yang dikerjakan masing-masing. Sistem operasi memiliki coupling lintas subsistem; kegagalan pada toolchain, linker, boot, memory, atau driver dapat memengaruhi seluruh proyek.

### 5.1 Praktikum individu

Mahasiswa individu bertanggung jawab atas seluruh artefak M0, meliputi setup WSL, repository, metadata toolchain, dokumen governance, validasi, commit, dan laporan.

### 5.2 Praktikum kelompok

Kelompok direkomendasikan berisi 3–5 mahasiswa. Pembagian peran minimal:

| Peran | Tanggung jawab |
|---|---|
| Koordinator teknis | Menjaga konsistensi repository, branch, dan readiness checklist. |
| Toolchain engineer | Menyiapkan WSL, paket, QEMU, OVMF, compiler, assembler, debugger, dan script validasi. |
| Documentation engineer | Menyusun baseline requirements, ADR, risk register, dan laporan. |
| Verification engineer | Menyusun verification matrix, menjalankan check script, dan mengumpulkan bukti. |
| Security reviewer | Menyusun threat model awal dan memeriksa fail-closed policy. |

Jika kelompok hanya berisi 2 anggota, peran dapat digabung, tetapi pembagian kontribusi tetap harus dicatat di laporan.

---

## 6. Target Praktikum

Pada akhir M0, mahasiswa harus menghasilkan artefak berikut.

| No | Artefak | Lokasi wajib | Bukti minimum |
|---|---|---|---|
| 1 | Repository awal MCSOS | `~/src/mcsos` di filesystem Linux WSL | Output `pwd`, `git status`, dan tree repository |
| 2 | Metadata toolchain | `build/meta/toolchain-versions.txt` | Isi file versi tool |
| 3 | Script validasi lingkungan | `tools/check_env.sh` | Output `bash tools/check_env.sh` |
| 4 | Baseline requirements | `docs/requirements/system_requirements.md` | Minimal 10 requirement dengan test/evidence mapping |
| 5 | Non-goals dan assumptions | `docs/requirements/assumptions_and_nongoals.md` | Daftar asumsi dan batasan tahap awal |
| 6 | ADR boot/toolchain | `docs/adr/ADR-0001-toolchain-and-boot-baseline.md` | Keputusan WSL 2, QEMU, OVMF, Limine, LLVM/GCC policy |
| 7 | Invariants awal | `docs/architecture/invariants.md` | Invariants boot, toolchain, repository, evidence |
| 8 | Threat model awal | `docs/security/threat_model.md` | Assets, actors, trust boundary, mitigasi awal |
| 9 | Risk register | `docs/governance/risk_register.md` | Risiko, dampak, probabilitas, mitigasi, owner |
| 10 | Verification matrix | `docs/testing/verification_matrix.md` | Requirement → command/evidence mapping |
| 11 | Object smoke test | `build/smoke/freestanding.o` | `readelf -h` menunjukkan ELF64 relocatable untuk x86-64 |
| 12 | Laporan praktikum | `docs/reports/M0-laporan.md` atau file laporan yang ditentukan dosen | Mengikuti template laporan praktikum standar |

---

## 7. Alat dan Versi Minimum

Versi final yang dipakai boleh mengikuti paket distribusi Linux WSL yang digunakan kelas. Semua versi aktual wajib dicatat melalui `make meta` atau `tools/check_env.sh`.

| Alat | Peran | Versi minimum konservatif | Catatan |
|---|---|---:|---|
| Windows 11 x64 | Host administratif | Windows 11 | WSL install resmi didukung pada Windows 11 [1]. |
| WSL 2 | Linux build environment | WSL 2 | `.wslconfig` berlaku untuk distro WSL 2 [2]. |
| Ubuntu LTS / Debian stable | Build userspace | Ubuntu 24.04 LTS direkomendasikan | Distribusi boleh distandarkan dosen. |
| Git | Version control | Versi paket distro | Wajib untuk commit evidence. |
| Clang/LLVM | Compiler alternatif utama | Versi paket distro | Clang mendukung cross-compilation melalui opsi `-target` [7]. |
| LLD | Linker LLVM | Versi paket distro | Digunakan pada fase kernel berikutnya. |
| GNU binutils | `readelf`, `objdump`, `ld`, `objcopy` | Versi paket distro | Wajib untuk inspeksi object/ELF. |
| NASM | Assembly x86_64 | Versi paket distro | Digunakan untuk assembly Intel syntax. |
| QEMU system x86 | Emulator x86_64 | Versi paket distro | QEMU menyediakan `qemu-system-x86_64` dan opsi mesin seperti `-machine` [3]. |
| OVMF | UEFI firmware QEMU | Versi paket distro | Digunakan untuk jalur UEFI pada M1/M2. |
| GDB / gdb-multiarch | Debugger | Versi paket distro | QEMU gdbstub memakai `-s -S` untuk debugging awal [4]. |
| Make/CMake/Ninja | Build orchestration | Versi paket distro | M0 membuat target `meta`, `check`, `smoke`. |
| Python 3 | Script utilitas | Versi paket distro | Untuk validasi dan tooling tambahan. |
| shellcheck/cppcheck/clang-tidy | Analisis awal | Versi paket distro | Minimal dipasang untuk readiness tahap lanjut. |

---

## 8. Konsep Inti M0

### 8.1 Host, build environment, dan target

Host adalah Windows 11 x64. Build environment adalah Linux di WSL 2. Target adalah bare-metal x86_64 yang akan dijalankan di QEMU. Tiga istilah ini tidak boleh dicampur.

Contoh kesalahan umum: mahasiswa menjalankan compiler host Linux biasa lalu menghasilkan executable Linux, bukan object freestanding untuk kernel. Pada M0, kesalahan ini dicegah dengan membuat smoke test object dan memeriksa header ELF.

### 8.2 Reproducibility

Reproducibility berarti prosedur praktikum dapat diulang dari clean checkout dengan hasil yang dapat diaudit. Pada M0, reproducibility belum harus byte-for-byte sempurna, tetapi setiap nondeterminism harus dicatat, misalnya timestamp build, path absolut debug, versi paket distro, atau UUID image.

### 8.3 Evidence-first engineering

Setiap klaim harus disertai bukti. Contoh:

| Klaim | Bukti yang diterima |
|---|---|
| WSL 2 sudah aktif | Output `wsl --list --verbose` |
| Repository berada di filesystem Linux WSL | Output `pwd` menunjukkan `/home/<user>/src/mcsos`, bukan `/mnt/c/...` |
| QEMU tersedia | Output `qemu-system-x86_64 --version` |
| Object freestanding berhasil dibuat | Output `readelf -h build/smoke/freestanding.o` |
| Git siap digunakan | Output `git status`, `git log --oneline -n 3` |
| Dokumen baseline tersedia | Output `find docs -maxdepth 3 -type f | sort` |

### 8.4 Readiness terminology

Gunakan istilah berikut secara ketat.

| Istilah | Arti |
|---|---|
| Siap uji lingkungan | Tool, repository, dan dokumen baseline dapat diverifikasi. Ini target M0. |
| Siap uji QEMU | Kernel/image dapat dijalankan di QEMU dengan log deterministik. Ini bukan target M0. |
| Siap demonstrasi praktikum | Fitur praktikum tertentu bekerja dengan bukti build/test/log. |
| Kandidat siap pakai terbatas | Hanya boleh dipakai jika ada build, test, security, dokumentasi, rollback, dan known limitations. |
| Tidak boleh digunakan | “Tanpa error”, “siap produksi”, “aman sepenuhnya”, “stabil penuh”. |

---

## 9. Arsitektur Ringkas M0

M0 membangun fondasi berikut.

```text
Windows 11 x64 host
  |
  |-- PowerShell Administrator
  |     |-- install WSL 2
  |     |-- configure global WSL resource policy
  |
  |-- WSL 2 Linux distro
        |
        |-- ~/src/mcsos repository
        |     |-- docs/requirements
        |     |-- docs/architecture
        |     |-- docs/security
        |     |-- docs/testing
        |     |-- docs/governance
        |     |-- tools/check_env.sh
        |     |-- Makefile
        |     |-- smoke/freestanding.c
        |
        |-- build/meta/toolchain-versions.txt
        |-- build/smoke/freestanding.o
```

Pada M0 belum ada kernel final, belum ada linker script final, belum ada boot image final, dan belum ada klaim boot. Semua yang dibuat adalah landasan untuk M1.

---

## 10. Persiapan Windows 11 x64

### 10.1 Verifikasi edisi dan build Windows

Langkah ini memastikan Windows memenuhi prasyarat WSL modern. Jalankan PowerShell biasa atau Windows Terminal.

```powershell
winver
```

Catat edisi dan build Windows pada laporan. Jika menggunakan komputer laboratorium, sertakan screenshot jendela `winver` atau tulis versinya secara manual.

### 10.2 Aktifkan virtualisasi di firmware

WSL 2 membutuhkan virtualisasi hardware aktif. Istilah yang umum muncul pada BIOS/UEFI: Intel VT-x, Intel Virtualization Technology, AMD-V, atau SVM Mode. Jika WSL gagal berjalan dengan pesan virtualisasi tidak aktif, masuk ke firmware setup perangkat dan aktifkan opsi tersebut. Dokumentasikan tindakan ini pada laporan jika dilakukan.

### 10.3 Instal WSL

Jalankan PowerShell sebagai Administrator. Perintah berikut mengikuti pola instalasi resmi WSL: `wsl --install` mengaktifkan fitur yang diperlukan dan memasang distribusi Linux default jika belum tersedia [1].

```powershell
wsl --install
```

Restart Windows jika diminta. Setelah restart, buka Windows Terminal dan jalankan:

```powershell
wsl --status
wsl --list --verbose
```

Indikator hasil yang diharapkan:

1. `wsl --status` menampilkan status WSL tanpa error fatal.
2. `wsl --list --verbose` menampilkan distro Linux dengan `VERSION` bernilai `2`.
3. Jika distro masih WSL 1, konversi dengan perintah pada langkah berikut.

### 10.4 Pilih distribusi Linux

Jika kelas menetapkan Ubuntu, gunakan Ubuntu. Untuk melihat distribusi yang tersedia:

```powershell
wsl --list --online
```

Untuk memasang Ubuntu:

```powershell
wsl --install -d Ubuntu
```

Jika tersedia pilihan Ubuntu versi tertentu dan dosen menetapkan Ubuntu 24.04, gunakan distribusi tersebut. Jika tidak tersedia, gunakan Ubuntu LTS yang tersedia di mesin laboratorium dan catat versinya.

### 10.5 Pastikan distro memakai WSL 2

```powershell
wsl --set-default-version 2
wsl --list --verbose
```

Jika nama distro misalnya `Ubuntu`, dan version masih `1`, jalankan:

```powershell
wsl --set-version Ubuntu 2
```

### 10.6 Konfigurasi `.wslconfig`

File `.wslconfig` adalah konfigurasi global WSL 2 yang ditempatkan di `%UserProfile%\.wslconfig`; Microsoft membedakan `.wslconfig` sebagai konfigurasi global WSL 2 dan `/etc/wsl.conf` sebagai konfigurasi per-distribusi [2]. Buat file ini di Windows, bukan di dalam filesystem Linux.

Buka Notepad dari PowerShell:

```powershell
notepad $env:USERPROFILE\.wslconfig
```

Isi konservatif untuk mesin dengan RAM 16 GB dan 8 logical CPU:

```ini
[wsl2]
memory=8GB
processors=4
swap=4GB
localhostForwarding=true

[experimental]
autoMemoryReclaim=gradual
```

Jika mesin memiliki RAM 32 GB atau lebih, dosen dapat menetapkan konfigurasi lebih besar, misalnya `memory=16GB` dan `processors=8`. Jangan memakai seluruh RAM host karena QEMU, editor, browser, dan Windows tetap memerlukan memori.

Terapkan konfigurasi:

```powershell
wsl --shutdown
wsl --status
```

Buka kembali distro Ubuntu setelah `wsl --shutdown`.

### 10.7 Catatan penting lokasi repository

Repository MCSOS wajib ditempatkan di filesystem Linux WSL, misalnya:

```text
/home/<username>/src/mcsos
```

Jangan menempatkan repository utama di:

```text
/mnt/c/Users/<username>/...
```

Alasan teknis: filesystem Windows dapat memiliki perbedaan case sensitivity, permission bit, executable bit, newline conversion, dan performa I/O. Perbedaan ini dapat menghasilkan bug praktikum yang tidak berasal dari kode OS.

---

## 11. Persiapan Linux WSL

### 11.1 Perbarui indeks paket

Buka terminal Ubuntu WSL. Perintah ini memperbarui daftar paket dan paket terpasang. Tujuannya adalah memastikan package manager mengetahui versi paket terbaru dari repository yang aktif.

```bash
sudo apt update
sudo apt upgrade -y
```

Jika jaringan laboratorium menggunakan proxy, catat konfigurasi proxy yang dipakai. Jangan menghapus log error; log tersebut dapat membantu diagnosis.

### 11.2 Pasang paket dasar pengembangan

Perintah berikut memasang build tools, compiler, linker, assembler, emulator, debugger, firmware UEFI untuk QEMU, utilitas filesystem image, static analysis, dan dokumentasi dasar. Ubuntu menyediakan paket `qemu-system-x86` untuk full system emulation x86 [9]. QEMU sendiri mendokumentasikan pemanggilan `qemu-system-x86_64` beserta opsi seperti `-machine` [3].

```bash
sudo apt install -y \
  build-essential git make cmake ninja-build pkg-config \
  clang lld llvm binutils nasm \
  qemu-system-x86 qemu-utils ovmf \
  gdb gdb-multiarch \
  xorriso mtools dosfstools parted gdisk \
  python3 python3-pip python3-venv \
  shellcheck cppcheck clang-tidy \
  curl wget ca-certificates unzip tree file xxd
```

Indikator hasil:

1. Perintah selesai tanpa error `Unable to locate package`.
2. `qemu-system-x86_64 --version` dapat dijalankan.
3. `clang --version`, `ld.lld --version`, `nasm -v`, dan `gdb --version` menghasilkan output.

### 11.3 Opsional: paket dokumentasi tambahan

Jika praktikum membutuhkan generasi dokumentasi atau diagram, pasang paket berikut.

```bash
sudo apt install -y doxygen graphviz pandoc
```

### 11.4 Verifikasi tool penting

Perintah berikut hanya memeriksa keberadaan tool. Hasilnya belum membuktikan toolchain benar untuk kernel, tetapi cukup untuk memastikan lingkungan dasar tersedia.

```bash
for tool in git make cmake ninja clang ld.lld llvm-readelf llvm-objdump readelf objdump nasm qemu-system-x86_64 gdb python3 shellcheck cppcheck; do
  printf "%-24s" "$tool"
  command -v "$tool" || true
done
```

Jika ada baris yang kosong setelah nama tool, paket terkait belum tersedia atau PATH belum benar.

---

## 12. Konfigurasi Git dan Identitas Commit

### 12.1 Atur identitas Git

Git dipakai untuk traceability. Setiap laporan harus mencantumkan commit hash. Jalankan:

```bash
git config --global user.name "Nama Mahasiswa atau Nama Kelompok"
git config --global user.email "email_aktif@example.com"
git config --global init.defaultBranch main
git config --global core.autocrlf input
git config --global pull.rebase false
```

Penjelasan:

1. `user.name` dan `user.email` menjadi identitas commit.
2. `init.defaultBranch main` menyeragamkan branch awal.
3. `core.autocrlf input` mengurangi risiko perubahan line ending saat bekerja lintas Windows/Linux.

Verifikasi:

```bash
git config --global --list | sort
```

### 12.2 Kebijakan branch praktikum

Gunakan kebijakan minimal berikut.

```text
main                  : baseline stabil yang sudah dinilai atau siap dikumpulkan
dev                   : integrasi pekerjaan aktif
m0/<nama-atau-kelompok>: branch khusus praktikum M0
```

Untuk praktikum kelompok, semua anggota harus melakukan commit dengan identitas masing-masing atau menyatakan kontribusi di laporan.

---

## 13. Membuat Repository Awal MCSOS

### 13.1 Buat direktori kerja

Perintah ini membuat repository di filesystem Linux WSL. Tujuannya adalah menghindari masalah permission dan path yang dapat terjadi jika repository ditempatkan di `/mnt/c`.

```bash
mkdir -p ~/src
cd ~/src
mkdir -p mcsos
cd mcsos
git init
```

Verifikasi lokasi:

```bash
pwd
```

Output harus berbentuk:

```text
/home/<username>/src/mcsos
```

Jika output berada di `/mnt/c/...`, pindahkan repository ke filesystem Linux WSL sebelum melanjutkan.

### 13.2 Buat struktur direktori baseline

Perintah berikut membuat struktur minimal untuk M0 sampai M1. Direktori belum berisi kernel final; struktur ini adalah kontrak awal agar dokumen, tools, dan bukti berada di lokasi yang seragam.

```bash
mkdir -p \
  docs/adr \
  docs/architecture \
  docs/requirements \
  docs/security \
  docs/testing \
  docs/governance \
  docs/operations \
  docs/reports \
  tools \
  smoke \
  build/meta \
  build/smoke
```

Tampilkan struktur:

```bash
tree -a -L 3
```

### 13.3 Buat `.gitignore`

File `.gitignore` mencegah artefak generated masuk ke repository. Namun, laporan dapat melampirkan ringkasan output atau file evidence tertentu jika dosen mengizinkan.

```bash
cat > .gitignore <<'EOF'
# Build artifacts
build/
*.o
*.elf
*.bin
*.iso
*.img
*.map
*.log

# Editor and OS noise
.vscode/
.idea/
.DS_Store
Thumbs.db

# Python cache
__pycache__/
*.pyc

# Temporary files
*.tmp
*.swp
EOF
```

### 13.4 Buat README awal

README harus menyatakan target, batasan, cara validasi, dan status readiness. Jangan menulis klaim “sistem operasi siap pakai”.

```bash
cat > README.md <<'EOF'
# MCSOS 260502

MCSOS 260502 adalah proyek sistem operasi pendidikan bertahap untuk target x86_64 dengan host pengembangan Windows 11 x64 melalui WSL 2.

Status saat ini: M0 — baseline requirements, governance, dan lingkungan pengembangan reproducible.

Target awal:

- Arsitektur: x86_64
- Emulator: QEMU system x86_64
- Firmware emulator: OVMF / UEFI
- Bahasa kernel awal: freestanding C17 dan assembly x86_64 minimal
- Kernel model awal: monolithic educational kernel dengan boundary modular internal

Perintah awal:

```bash
make meta
make check
make smoke
```

Dokumen utama:

- `docs/requirements/system_requirements.md`
- `docs/requirements/assumptions_and_nongoals.md`
- `docs/adr/ADR-0001-toolchain-and-boot-baseline.md`
- `docs/security/threat_model.md`
- `docs/governance/risk_register.md`
- `docs/testing/verification_matrix.md`

Catatan readiness: keberhasilan M0 hanya berarti lingkungan dan baseline proyek siap diperiksa. M0 tidak membuktikan kernel dapat boot.
EOF
```

---

## 14. Membuat Script Validasi Lingkungan

### 14.1 Buat `tools/check_env.sh`

Script ini memeriksa tool wajib, mencatat versi, memperingatkan jika repository berada di `/mnt/c`, dan membuat metadata. Script ini tidak membuktikan kernel benar; script ini hanya memverifikasi baseline lingkungan.

```bash
cat > tools/check_env.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
META_DIR="$ROOT_DIR/build/meta"
mkdir -p "$META_DIR"

fail=0

say() { printf '[M0] %s\n' "$*"; }
check_tool() {
  local tool="$1"
  if command -v "$tool" >/dev/null 2>&1; then
    printf '[OK]   %-24s %s\n' "$tool" "$(command -v "$tool")"
  else
    printf '[FAIL] %-24s not found\n' "$tool"
    fail=1
  fi
}

say "Repository root: $ROOT_DIR"
case "$ROOT_DIR" in
  /mnt/c/*|/mnt/d/*|/mnt/e/*)
    printf '[WARN] Repository appears to be on a Windows-mounted filesystem. Move it to ~/src/mcsos for this practicum.\n'
    ;;
  *)
    printf '[OK] Repository is not under /mnt/<drive>.\n'
    ;;
esac

say "Checking required tools"
for tool in git make clang ld.lld llvm-readelf llvm-objdump readelf objdump nasm qemu-system-x86_64 gdb python3 shellcheck cppcheck; do
  check_tool "$tool"
done

say "Writing toolchain metadata"
{
  echo "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "root_dir=$ROOT_DIR"
  echo "uname=$(uname -a)"
  echo "wsl_distro=${WSL_DISTRO_NAME:-unknown}"
  echo "shell=$SHELL"
  echo
  echo "## Tool versions"
  git --version || true
  make --version | head -n 1 || true
  clang --version | head -n 1 || true
  ld.lld --version | head -n 1 || true
  llvm-readelf --version | head -n 1 || true
  llvm-objdump --version | head -n 1 || true
  readelf --version | head -n 1 || true
  objdump --version | head -n 1 || true
  nasm -v || true
  qemu-system-x86_64 --version | head -n 1 || true
  gdb --version | head -n 1 || true
  python3 --version || true
  shellcheck --version | head -n 2 || true
  cppcheck --version || true
} > "$META_DIR/toolchain-versions.txt"

say "Metadata written to build/meta/toolchain-versions.txt"

if [ "$fail" -ne 0 ]; then
  say "Environment check failed. Install missing tools before continuing."
  exit 1
fi

say "Environment check completed. This means the M0 environment is checkable, not that the OS can boot."
EOF

chmod +x tools/check_env.sh
```

### 14.2 Jalankan script validasi

```bash
bash tools/check_env.sh
cat build/meta/toolchain-versions.txt
```

Bukti yang harus dimasukkan ke laporan:

1. Output lengkap `bash tools/check_env.sh`.
2. Isi `build/meta/toolchain-versions.txt`.
3. Jika ada warning, tuliskan penyebab dan tindakan perbaikan.

---

## 15. Membuat Smoke Test Freestanding Object

### 15.1 Tujuan smoke test

Smoke test ini memastikan compiler dapat menghasilkan object ELF64 untuk target x86_64 tanpa bergantung pada hosted libc. Ini bukan kernel, bukan bootloader, dan bukan image boot. Ini hanya validasi awal bahwa toolchain dapat menghasilkan object untuk tahap berikutnya.

Clang mendukung cross-compilation dengan opsi `-target <triple>`; dokumentasi Clang menjelaskan bahwa target triple perlu ditetapkan agar compiler tidak mengasumsikan target host [7]. Untuk M0, gunakan target konservatif `x86_64-unknown-none` atau target yang distandarkan dosen.

### 15.2 Buat file smoke test

```bash
cat > smoke/freestanding.c <<'EOF'
#include <stdint.h>
#include <stddef.h>

#define MCSOS_M0_MAGIC 0x4D435330u /* "MCS0" */

struct m0_smoke_record {
    uint32_t magic;
    uint32_t version;
    uintptr_t pointer_width;
    size_t size_width;
};

__attribute__((used))
const struct m0_smoke_record m0_smoke_record = {
    .magic = MCSOS_M0_MAGIC,
    .version = 260502u,
    .pointer_width = sizeof(void *),
    .size_width = sizeof(size_t),
};

int m0_smoke_add(int a, int b) {
    return a + b;
}
EOF
```

### 15.3 Kompilasi object

Perintah ini memakai mode freestanding, tidak memakai hosted include selain header compiler dasar, dan menghasilkan object relocatable. Flag `-ffreestanding` memberi tahu compiler bahwa program berjalan pada lingkungan freestanding, bukan hosted OS biasa.

```bash
clang \
  --target=x86_64-unknown-none \
  -ffreestanding \
  -fno-stack-protector \
  -fno-pic \
  -mno-red-zone \
  -mno-mmx -mno-sse -mno-sse2 \
  -Wall -Wextra -Werror \
  -std=c17 \
  -c smoke/freestanding.c \
  -o build/smoke/freestanding.o
```

### 15.4 Inspeksi object

```bash
readelf -h build/smoke/freestanding.o | tee build/smoke/readelf-header.txt
objdump -drwC build/smoke/freestanding.o | tee build/smoke/objdump.txt
file build/smoke/freestanding.o | tee build/smoke/file.txt
```

Indikator hasil yang diharapkan:

| Pemeriksaan | Hasil yang diharapkan |
|---|---|
| `Class` | `ELF64` |
| `Machine` | `Advanced Micro Devices X86-64` atau ekuivalen x86-64 |
| `Type` | `REL (Relocatable file)` |
| `file` | Menyebut `ELF 64-bit LSB relocatable, x86-64` atau ekuivalen |

Jika output menunjukkan executable Linux, PE/COFF Windows, atau arsitektur selain x86-64, smoke test gagal.

---

## 16. Membuat Makefile M0

Makefile ini menyeragamkan perintah praktikum. Pada M0 belum ada target `run` untuk boot kernel. Target `qemu-version` hanya memeriksa emulator.

```bash
cat > Makefile <<'EOF'
.PHONY: meta check smoke qemu-version clean distclean tree

BUILD_DIR := build
SMOKE_DIR := smoke

meta:
	@bash tools/check_env.sh

check:
	@bash tools/check_env.sh
	@shellcheck tools/check_env.sh

smoke:
	@mkdir -p $(BUILD_DIR)/smoke
	clang --target=x86_64-unknown-none \
	  -ffreestanding \
	  -fno-stack-protector \
	  -fno-pic \
	  -mno-red-zone \
	  -mno-mmx -mno-sse -mno-sse2 \
	  -Wall -Wextra -Werror \
	  -std=c17 \
	  -c $(SMOKE_DIR)/freestanding.c \
	  -o $(BUILD_DIR)/smoke/freestanding.o
	readelf -h $(BUILD_DIR)/smoke/freestanding.o | tee $(BUILD_DIR)/smoke/readelf-header.txt
	objdump -drwC $(BUILD_DIR)/smoke/freestanding.o | tee $(BUILD_DIR)/smoke/objdump.txt >/dev/null
	file $(BUILD_DIR)/smoke/freestanding.o | tee $(BUILD_DIR)/smoke/file.txt

qemu-version:
	@qemu-system-x86_64 --version
	@echo "QEMU exists. M0 does not boot a kernel image."

tree:
	@tree -a -L 3

clean:
	rm -rf $(BUILD_DIR)/smoke

# distclean intentionally removes all generated build metadata.
distclean:
	rm -rf $(BUILD_DIR)
EOF
```

Jalankan:

```bash
make meta
make check
make smoke
make qemu-version
```

Simpan output ke laporan. Jika `make smoke` gagal karena flag target tidak didukung pada versi Clang tertentu, catat versi Clang dan gunakan fallback `x86_64-elf-gcc` jika kelas telah menyediakan cross-compiler GCC.

---

## 17. Opsional Lanjutan: Cross-Compiler GCC `x86_64-elf`

Bagian ini opsional pada M0 kecuali dosen menetapkannya wajib. Membangun GCC dari source memerlukan waktu dan koneksi stabil. Dokumentasi GCC menyatakan pembangunan GCC memiliki prasyarat tool dan paket tertentu [5], dan pembangunan cross compiler tidak umumnya memakai 3-stage bootstrap seperti native compiler [6].

### 17.1 Kapan opsi ini digunakan

Gunakan bagian ini jika:

1. Kelas menginginkan toolchain GNU `x86_64-elf-gcc` sejak awal.
2. Mesin laboratorium memiliki waktu dan sumber daya memadai.
3. Dosen ingin membandingkan LLVM/Clang dan GCC cross-toolchain.

Jika tidak, cukup gunakan Clang smoke test pada M0 dan tunda GCC cross compiler ke praktikum toolchain khusus.

### 17.2 Dependensi build GCC

```bash
sudo apt install -y \
  build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo libisl-dev
```

### 17.3 Variabel environment

```bash
export TARGET=x86_64-elf
export PREFIX="$HOME/opt/cross"
export PATH="$PREFIX/bin:$PATH"
mkdir -p "$HOME/src/toolchain-src"
cd "$HOME/src/toolchain-src"
```

### 17.4 Catatan keamanan dan reproducibility

Sebelum memakai source archive, catat URL, versi, checksum, dan tanggal unduh. Jangan memakai source tidak jelas. Jika dosen menyediakan mirror internal, gunakan mirror tersebut.

---

## 18. Menyiapkan QEMU dan GDB Workflow

### 18.1 Tujuan pada M0

M0 belum menjalankan kernel MCSOS. Namun, QEMU dan GDB harus tersedia karena M1/M2 akan membutuhkan boot smoke test dan debug early kernel. Dokumentasi QEMU menyatakan `qemu-system-x86_64` dipanggil dengan opsi sistem seperti `-machine` [3]. Dokumentasi QEMU juga menyatakan debugging via gdbstub dapat memakai `-s -S`, dengan `-s` membuka port TCP 1234 dan `-S` menghentikan guest sampai GDB melanjutkan eksekusi [4].

### 18.2 Verifikasi QEMU

```bash
qemu-system-x86_64 --version
qemu-system-x86_64 -machine help | head -n 30
```

### 18.3 Verifikasi OVMF path

```bash
find /usr/share -iname 'OVMF_CODE*.fd' -o -iname 'OVMF_VARS*.fd' | sort
```

Catat path aktual pada laporan. Path dapat berbeda antar distribusi.

### 18.4 Buat catatan QEMU baseline

```bash
cat > docs/architecture/qemu_baseline.md <<'EOF'
# QEMU Baseline MCSOS 260502

Target awal MCSOS menggunakan QEMU system emulator untuk x86_64.

Baseline M0:

- M0 hanya memverifikasi keberadaan QEMU dan OVMF.
- M0 belum menjalankan kernel image.
- Jalur UEFI/OVMF akan digunakan pada milestone boot berikutnya.

Command template untuk M1/M2, belum wajib berhasil pada M0:

```bash
qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 512M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -serial file:build/qemu-serial.log \
  -display none \
  -no-reboot \
  -no-shutdown
```

Debug template untuk M1/M2:

```bash
qemu-system-x86_64 -s -S ...
gdb -ex "target remote localhost:1234"
```
EOF
```

---

## 19. Menyusun Dokumen Requirements dan Governance

### 19.1 System requirements

Buat file requirements. Gunakan requirement yang dapat diverifikasi. Hindari requirement kabur seperti “kernel harus cepat” tanpa ukuran.

```bash
cat > docs/requirements/system_requirements.md <<'EOF'
# System Requirements MCSOS 260502 — Baseline M0

## Scope

Dokumen ini menetapkan requirement awal untuk proyek MCSOS 260502. Requirement pada M0 berfokus pada lingkungan, governance, dan evidence. Requirement runtime kernel akan diperinci pada milestone berikutnya.

| ID | Requirement | Rationale | Verification evidence |
|---|---|---|---|
| REQ-M0-001 | Repository MCSOS harus berada di filesystem Linux WSL, bukan `/mnt/c`. | Mengurangi risiko permission, case, dan I/O mismatch. | Output `pwd` dan `tools/check_env.sh`. |
| REQ-M0-002 | Semua tool wajib harus terdeteksi oleh script validasi. | Build lanjutan tidak boleh bergantung pada tool manual tak tercatat. | Output `bash tools/check_env.sh`. |
| REQ-M0-003 | Versi toolchain harus dicatat pada `build/meta/toolchain-versions.txt`. | Traceability dan reproducibility. | Isi file metadata. |
| REQ-M0-004 | Repository harus memiliki struktur `docs`, `tools`, `smoke`, dan `build`. | Menyeragamkan artefak praktikum. | Output `tree -a -L 3`. |
| REQ-M0-005 | Smoke test harus menghasilkan object ELF64 x86-64 relocatable. | Validasi awal target toolchain. | Output `readelf -h`. |
| REQ-M0-006 | Proyek harus memiliki assumptions dan non-goals. | Mencegah scope creep dan klaim readiness berlebih. | `docs/requirements/assumptions_and_nongoals.md`. |
| REQ-M0-007 | Proyek harus memiliki ADR awal untuk toolchain dan boot baseline. | Keputusan teknis harus dapat dilacak. | `docs/adr/ADR-0001-toolchain-and-boot-baseline.md`. |
| REQ-M0-008 | Proyek harus memiliki threat model awal. | Security from phase 0. | `docs/security/threat_model.md`. |
| REQ-M0-009 | Proyek harus memiliki risk register. | Risiko teknis dan operasional harus dikelola. | `docs/governance/risk_register.md`. |
| REQ-M0-010 | Proyek harus memiliki verification matrix. | Setiap requirement harus punya bukti validasi. | `docs/testing/verification_matrix.md`. |
| REQ-M0-011 | Semua perubahan M0 harus dikomit ke Git. | Traceability penilaian. | `git log --oneline`. |
| REQ-M0-012 | Laporan M0 harus memuat log, command, screenshot seperlunya, commit hash, dan analisis failure mode. | Evidence-first assessment. | `docs/reports/M0-laporan.md`. |
EOF
```

### 19.2 Assumptions and non-goals

```bash
cat > docs/requirements/assumptions_and_nongoals.md <<'EOF'
# Assumptions and Non-Goals MCSOS 260502 — M0

## Assumptions

1. Target arsitektur awal adalah x86_64 long mode.
2. Host pengembangan adalah Windows 11 x64.
3. Build dilakukan di WSL 2 Linux environment.
4. Repository utama berada di filesystem Linux WSL.
5. Emulator utama untuk milestone awal adalah QEMU system x86_64.
6. Firmware emulator untuk jalur boot awal adalah OVMF/UEFI.
7. Bootloader awal yang direkomendasikan untuk milestone boot adalah Limine atau bootloader setara yang memiliki handoff terdokumentasi.
8. Bahasa kernel awal adalah freestanding C17 dengan assembly minimal.
9. Compatibility target awal adalah POSIX-like subset, bukan Linux ABI penuh.
10. Setiap milestone harus menghasilkan bukti: log, command output, image, checksum, map file, disassembly, trace, atau laporan.

## Non-goals M0

1. M0 tidak membuat kernel bootable.
2. M0 tidak mengimplementasikan bootloader.
3. M0 tidak membuat linker script final.
4. M0 tidak mengimplementasikan interrupt, paging, scheduler, syscall, VFS, driver, networking, graphics, atau security enforcement.
5. M0 tidak mengklaim MCSOS siap produksi.
6. M0 tidak mengklaim semua mesin x86_64 akan kompatibel.
7. M0 tidak mengharuskan hardware bring-up.
8. M0 tidak mengharuskan byte-for-byte reproducible build; nondeterminism cukup dicatat.
EOF
```

### 19.3 ADR-0001 toolchain dan boot baseline

```bash
cat > docs/adr/ADR-0001-toolchain-and-boot-baseline.md <<'EOF'
# ADR-0001 — Toolchain dan Boot Baseline MCSOS 260502

## Status

Accepted for M0 baseline.

## Context

MCSOS dikembangkan pada host Windows 11 x64, tetapi targetnya adalah bare-metal x86_64. Program kernel tidak boleh bergantung pada ABI Windows atau Linux host. Lingkungan build harus dapat direproduksi oleh mahasiswa, asisten, dan dosen.

## Decision

1. Build environment utama adalah WSL 2 Linux environment.
2. Repository utama ditempatkan di filesystem Linux WSL, bukan `/mnt/c`.
3. Toolchain awal M0 memakai paket distro: Clang/LLVM, LLD, binutils, NASM, Make, CMake, Ninja, Python 3.
4. Smoke test M0 memakai `clang --target=x86_64-unknown-none` untuk menghasilkan object freestanding.
5. Emulator utama untuk milestone berikutnya adalah QEMU system x86_64.
6. Firmware emulator adalah OVMF untuk jalur UEFI.
7. Bootloader awal yang direkomendasikan untuk milestone boot adalah Limine karena mendukung x86-64 dan protokol boot modern; keputusan final tetap harus divalidasi pada M1/M2.
8. GCC `x86_64-elf` dari source bersifat opsional pada M0 kecuali ditetapkan wajib oleh dosen.

## Consequences

Keuntungan:

- Setup lebih seragam di Windows 11.
- Toolchain Linux tersedia melalui package manager.
- QEMU/GDB workflow selaras dengan praktik OS development.
- Struktur evidence dapat direproduksi.

Trade-off:

- WSL 2 memiliki boundary VM yang harus dikonfigurasi.
- Akselerasi KVM di WSL dapat bergantung pada konfigurasi host; TCG harus diterima sebagai baseline lambat tetapi deterministik.
- Versi paket distro dapat berbeda antar mesin; karena itu metadata versi wajib dicatat.

## Review Trigger

ADR ini harus ditinjau ulang jika:

1. Target arsitektur berubah dari x86_64.
2. Distro WSL distandarkan ulang.
3. Bootloader diganti dari Limine ke GRUB/custom UEFI loader.
4. Toolchain utama diganti dari LLVM ke GCC-only atau sebaliknya.
5. CI resmi proyek diperkenalkan.
EOF
```

---

## 20. Menyusun Invariants Awal

Invariants adalah aturan yang tidak boleh dilanggar. Pada M0, invariants lebih banyak terkait proses, environment, dan evidence.

```bash
cat > docs/architecture/invariants.md <<'EOF'
# Invariants MCSOS 260502 — Baseline M0

## Repository invariants

1. Repository utama berada di filesystem Linux WSL.
2. Semua generated artifact berada di `build/` atau lokasi generated yang terdokumentasi.
3. Source, dokumen, dan script validasi dikomit ke Git.
4. File generated besar seperti image, object, ISO, dan log penuh tidak dikomit kecuali diminta sebagai fixture.

## Toolchain invariants

1. Setiap praktikum mencatat versi tool pada `build/meta/toolchain-versions.txt` atau file metadata setara.
2. Compiler target harus dinyatakan eksplisit; kernel tidak boleh diam-diam memakai ABI host.
3. Object smoke test harus diperiksa dengan `readelf`, `objdump`, atau tool setara.
4. Flag freestanding dan red-zone policy harus terdokumentasi sebelum kode kernel nyata dibuat.

## Documentation invariants

1. Requirement harus memiliki metode verifikasi.
2. Risiko harus memiliki mitigasi atau trigger review.
3. Threat model harus ada sejak M0 dan diperbarui ketika subsistem baru ditambahkan.
4. Readiness label harus berbasis bukti.

## Evidence invariants

1. Klaim “berhasil” harus memiliki command output, log, checksum, screenshot, commit, atau artefak yang dapat diperiksa.
2. Error tidak boleh dihapus dari laporan; error harus diklasifikasi dan dianalisis.
3. Setiap rollback harus didokumentasikan.
EOF
```

---

## 21. Menyusun Threat Model Awal

Threat model M0 bersifat awal dan akan diperbarui pada milestone security. Fokusnya adalah supply-chain, kesalahan toolchain, trust boundary, dan praktik kerja.

```bash
cat > docs/security/threat_model.md <<'EOF'
# Threat Model Awal MCSOS 260502 — M0

## Assets

| Asset | Alasan dilindungi |
|---|---|
| Source code repository | Menentukan perilaku kernel dan tools. |
| Toolchain | Compiler/linker yang salah dapat menghasilkan artefak salah. |
| Build scripts | Script dapat menyisipkan flag berbahaya atau target salah. |
| Documentation baseline | Menjadi sumber requirement dan acceptance criteria. |
| Generated artifacts | Image/log/map dapat menjadi bukti penilaian. |
| Signing keys masa depan | Belum dibuat pada M0, tetapi harus direncanakan. |

## Actors

| Actor | Capability |
|---|---|
| Mahasiswa | Mengubah repository dan menjalankan build. |
| Anggota kelompok | Mengubah branch dan dokumen. |
| Dosen/asisten | Melakukan review dan penilaian. |
| Dependency eksternal | Menyediakan paket, source, dan tools. |
| Malicious local process | Dapat memodifikasi file jika permission buruk. |

## Trust boundaries

1. Windows host ↔ WSL Linux environment.
2. Repository source ↔ generated build output.
3. Package manager ↔ toolchain lokal.
4. Script praktikum ↔ shell pengguna.
5. QEMU guest masa depan ↔ host environment.

## Initial threats and mitigations

| Threat | Dampak | Mitigasi M0 |
|---|---|---|
| Repository ditempatkan di `/mnt/c` dan permission/line ending berubah | Build tidak reproducible | Check script memberi warning; repository dipindah ke `~/src/mcsos`. |
| Compiler host dipakai tanpa target eksplisit | Object salah ABI | Smoke test memakai `--target` dan `readelf`. |
| Tool versi tidak tercatat | Hasil tidak dapat diaudit | `build/meta/toolchain-versions.txt`. |
| Script dari internet dieksekusi tanpa review | Supply-chain compromise | Gunakan package manager resmi atau source resmi; catat URL dan checksum untuk source manual. |
| Klaim readiness berlebihan | Penilaian tidak valid | Gunakan readiness label berbasis bukti. |
| Anggota kelompok tidak memahami keseluruhan baseline | Integrasi gagal | Laporan mencantumkan peran dan review lintas anggota. |

## Out of scope M0

1. Enforcement MAC/RBAC/capability.
2. Secure Boot penuh.
3. TPM measured boot.
4. Kernel exploit mitigation.
5. Syscall fuzzing.

Semua item out-of-scope akan masuk milestone berikutnya setelah boot, memory, syscall, dan userspace baseline tersedia.
EOF
```

---

## 22. Menyusun Risk Register

```bash
cat > docs/governance/risk_register.md <<'EOF'
# Risk Register MCSOS 260502 — M0

| ID | Risiko | Probabilitas | Dampak | Mitigasi | Owner | Trigger review |
|---|---|---:|---:|---|---|---|
| R-M0-001 | WSL tidak aktif atau memakai WSL 1 | Medium | High | Verifikasi `wsl --list --verbose`; konversi ke WSL 2 | Toolchain engineer | `VERSION` bukan 2 |
| R-M0-002 | Repository berada di `/mnt/c` | High | Medium | Pindahkan ke `~/src/mcsos`; check script warning | Koordinator | `pwd` menunjukkan `/mnt/c` |
| R-M0-003 | QEMU tidak tersedia | Medium | High | Pasang `qemu-system-x86`; catat versi | Toolchain engineer | `command -v qemu-system-x86_64` gagal |
| R-M0-004 | OVMF path berbeda | Medium | Medium | Cari dengan `find /usr/share`; jangan hardcode tanpa verifikasi | Toolchain engineer | `OVMF_CODE.fd` tidak ditemukan |
| R-M0-005 | Compiler menghasilkan target host | Medium | High | Pakai `--target`; inspeksi `readelf` | Verification engineer | `Machine` bukan x86-64 |
| R-M0-006 | Dokumen requirement tidak testable | Medium | Medium | Verification matrix wajib | Documentation engineer | Requirement tanpa evidence |
| R-M0-007 | Kelompok tidak sinkron branch | Medium | Medium | Kebijakan branch dan pull sebelum commit | Koordinator | Konflik merge berulang |
| R-M0-008 | Mahasiswa menghapus log error | Medium | Medium | Laporan wajib mencantumkan failure mode | Semua | Error tidak tercatat |
| R-M0-009 | Build bergantung pada package version tidak tercatat | Medium | High | `make meta` sebelum submit | Verification engineer | Metadata kosong |
| R-M0-010 | Scope M0 melebar menjadi implementasi kernel | Medium | Medium | Ikuti non-goals; tunda kernel ke M1/M2 | Koordinator | Ada kode kernel fungsional tanpa kontrak |
EOF
```

---

## 23. Menyusun Verification Matrix

```bash
cat > docs/testing/verification_matrix.md <<'EOF'
# Verification Matrix MCSOS 260502 — M0

| Requirement | Verification command | Expected evidence | Pass/Fail |
|---|---|---|---|
| REQ-M0-001 | `pwd` | Path berada di `/home/.../src/mcsos` | TBD |
| REQ-M0-002 | `bash tools/check_env.sh` | Semua tool wajib `[OK]` atau warning terdokumentasi | TBD |
| REQ-M0-003 | `cat build/meta/toolchain-versions.txt` | Versi tool tercatat | TBD |
| REQ-M0-004 | `tree -a -L 3` | Struktur docs/tools/smoke/build tersedia | TBD |
| REQ-M0-005 | `make smoke` | Object ELF64 x86-64 relocatable | TBD |
| REQ-M0-006 | `test -s docs/requirements/assumptions_and_nongoals.md` | File ada dan tidak kosong | TBD |
| REQ-M0-007 | `test -s docs/adr/ADR-0001-toolchain-and-boot-baseline.md` | File ada dan tidak kosong | TBD |
| REQ-M0-008 | `test -s docs/security/threat_model.md` | File ada dan tidak kosong | TBD |
| REQ-M0-009 | `test -s docs/governance/risk_register.md` | File ada dan tidak kosong | TBD |
| REQ-M0-010 | `test -s docs/testing/verification_matrix.md` | File ada dan tidak kosong | TBD |
| REQ-M0-011 | `git log --oneline -n 3` | Minimal satu commit M0 | TBD |
| REQ-M0-012 | `test -s docs/reports/M0-laporan.md` | Laporan tersedia | TBD |
EOF
```

---

## 24. Menyiapkan Laporan Praktikum M0

Buat file laporan awal. Mahasiswa harus melengkapi isi setelah menjalankan seluruh langkah.

```bash
cat > docs/reports/M0-laporan.md <<'EOF'
# Laporan Praktikum M0 — Baseline Requirements, Governance, dan Lingkungan Pengembangan

## 1. Sampul

- Judul praktikum: Praktikum M0 — Baseline Requirements, Governance, dan Lingkungan Pengembangan Reproducible MCSOS 260502
- Nama mahasiswa / kelompok:
- NIM:
- Kelas:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi: Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia
- Tanggal:

## 2. Tujuan

Tuliskan capaian teknis dan konseptual M0.

## 3. Dasar teori ringkas

Jelaskan host vs target, WSL 2, cross-compilation, ELF object, QEMU, OVMF, Git, reproducibility, dan evidence-first engineering.

## 4. Lingkungan

| Komponen | Versi / output |
|---|---|
| Windows | |
| WSL distro | |
| Kernel Linux WSL | |
| Git | |
| Clang | |
| LLD | |
| binutils/readelf | |
| NASM | |
| QEMU | |
| GDB | |
| Python | |

Lampirkan isi `build/meta/toolchain-versions.txt`.

## 5. Desain baseline

Jelaskan struktur repository, dokumen baseline, assumptions, non-goals, dan threat model awal.

## 6. Langkah kerja

Tuliskan perintah yang dijalankan, alasan teknis, dan hasilnya.

## 7. Hasil uji

| Pengujian | Command | Hasil | Pass/Fail |
|---|---|---|---|
| WSL version | `wsl --list --verbose` | | |
| Tool check | `bash tools/check_env.sh` | | |
| Metadata | `cat build/meta/toolchain-versions.txt` | | |
| Smoke object | `make smoke` | | |
| ELF header | `readelf -h build/smoke/freestanding.o` | | |
| Git status | `git status` | | |

## 8. Analisis

Jelaskan kendala, error, penyebab, perbaikan, dan bukti bahwa perbaikan berhasil.

## 9. Keamanan dan reliability

Jelaskan risiko supply-chain, toolchain mismatch, repository path, permission, log integrity, dan mitigasi yang diterapkan.

## 10. Failure modes dan rollback

| Failure mode | Gejala | Diagnosis | Rollback/perbaikan |
|---|---|---|---|
| WSL bukan versi 2 | | | |
| Tool tidak ditemukan | | | |
| Repository di `/mnt/c` | | | |
| Smoke object salah target | | | |
| OVMF tidak ditemukan | | | |

## 11. Kesimpulan

Nyatakan apakah M0 hanya siap uji lingkungan, belum siap boot, dan apa syarat masuk M1.

## 12. Lampiran

- Output `tools/check_env.sh`
- Isi `build/meta/toolchain-versions.txt`
- Output `readelf -h`
- Output `objdump` ringkas
- Screenshot relevan
- Commit hash

## 13. Referensi

Gunakan format IEEE sesuai panduan praktikum.
EOF
```

---

## 25. Commit Baseline M0

Sebelum commit, jalankan pemeriksaan.

```bash
make check
make smoke
make tree
```

Periksa status Git:

```bash
git status --short
```

Tambahkan file yang perlu dikomit. Karena `build/` diabaikan, metadata dan output build tidak akan masuk commit. Jika dosen meminta metadata dikumpulkan, lampirkan di laporan atau arsip pengumpulan.

```bash
git add README.md Makefile .gitignore tools smoke docs
git commit -m "M0: initialize reproducible OS development baseline"
```

Catat commit hash:

```bash
git rev-parse HEAD
git log --oneline -n 3
```

---

## 26. Checkpoint Buildable M0

M0 dinyatakan memenuhi checkpoint jika seluruh perintah berikut berhasil atau failure-nya terdokumentasi dan diperbaiki.

```bash
make meta
make check
make smoke
make qemu-version
git status --short
git log --oneline -n 3
```

Kriteria pass:

1. `make meta` menghasilkan `build/meta/toolchain-versions.txt`.
2. `make check` berhasil dan `shellcheck` tidak melaporkan error kritis pada `tools/check_env.sh`.
3. `make smoke` menghasilkan `build/smoke/freestanding.o`.
4. `readelf` menunjukkan object ELF64 x86-64 relocatable.
5. QEMU tersedia dan versinya tercatat.
6. Git memiliki commit M0.
7. Dokumen baseline tersedia.

---

## 27. Tugas Implementasi Mahasiswa

### 27.1 Tugas wajib

1. Instal atau verifikasi WSL 2 pada Windows 11 x64.
2. Pasang paket Linux WSL yang diperlukan.
3. Buat repository `~/src/mcsos`.
4. Buat struktur direktori baseline.
5. Buat `tools/check_env.sh`.
6. Buat smoke test freestanding object.
7. Buat Makefile M0.
8. Buat dokumen requirements, assumptions/non-goals, ADR, invariants, threat model, risk register, verification matrix.
9. Jalankan semua command validasi.
10. Buat laporan M0.
11. Commit semua file source dan dokumen baseline.

### 27.2 Tugas pengayaan

1. Tambahkan script `tools/collect_evidence.sh` yang mengumpulkan metadata, output smoke test, dan ringkasan Git ke satu direktori `build/evidence/M0`.
2. Tambahkan target Makefile `evidence` untuk menjalankan semua validasi dan menyimpan output.
3. Tambahkan pre-commit hook lokal yang menjalankan `shellcheck tools/check_env.sh`.
4. Buat diagram dependency M0 dalam format Mermaid di `docs/architecture/m0_dependency_graph.md`.
5. Bangun cross-compiler GCC `x86_64-elf` dari source dan catat checksum source archive.

### 27.3 Tantangan riset

1. Bandingkan output object dari Clang `--target=x86_64-unknown-none` dan GCC `x86_64-elf-gcc` untuk source yang sama.
2. Analisis pengaruh `-mno-red-zone` terhadap kode prolog/epilog menggunakan `objdump`.
3. Rancang policy reproducible build untuk fase kernel ELF, termasuk kontrol timestamp dan debug path.
4. Susun threat model supply-chain untuk toolchain dan bootloader.

---

## 28. Perintah Uji Lengkap

Gunakan bagian ini sebagai daftar perintah uji final sebelum pengumpulan.

```bash
# Pastikan berada di root repository
cd ~/src/mcsos

# Metadata dan validasi environment
make meta
make check

# Smoke test freestanding object
make smoke

# Emulator availability check
make qemu-version

# Bukti object
readelf -h build/smoke/freestanding.o
file build/smoke/freestanding.o

# Bukti repository
pwd
tree -a -L 3
git status --short
git log --oneline -n 3
git rev-parse HEAD
```

---

## 29. Bukti yang Wajib Dikumpulkan

| Bukti | Format | Lokasi laporan |
|---|---|---|
| Output `wsl --list --verbose` | Teks/screenshot | Bagian lingkungan |
| Output `pwd` | Teks | Bagian repository |
| Output `make meta` | Teks | Hasil uji |
| Isi `toolchain-versions.txt` | Teks/lampiran | Lampiran |
| Output `make smoke` | Teks | Hasil uji |
| Output `readelf -h` | Teks | Hasil uji |
| Output `file build/smoke/freestanding.o` | Teks | Hasil uji |
| Struktur direktori `tree -a -L 3` | Teks | Desain baseline |
| Commit hash | Teks | Kesimpulan/lampiran |
| Analisis error jika ada | Narasi | Analisis dan failure modes |

---

## 30. Pertanyaan Analisis

Jawab pertanyaan berikut pada laporan.

1. Mengapa repository utama MCSOS tidak direkomendasikan berada di `/mnt/c`?
2. Apa perbedaan host, build environment, dan target pada praktikum ini?
3. Mengapa kernel tidak boleh diam-diam memakai compiler/linker host tanpa target eksplisit?
4. Apa arti `ELF64 relocatable` pada output smoke test?
5. Mengapa `-mno-red-zone` penting untuk kernel x86_64 pada fase berikutnya?
6. Mengapa M0 belum boleh diklaim “siap uji QEMU” walaupun QEMU sudah terpasang?
7. Apa fungsi `build/meta/toolchain-versions.txt` dalam reproducibility?
8. Apa risiko jika versi toolchain tidak dicatat?
9. Bagaimana threat model M0 membantu pengembangan security pada milestone berikutnya?
10. Requirement mana yang paling sulit diverifikasi dan mengapa?
11. Apa rollback yang Anda lakukan jika smoke test menghasilkan object untuk target yang salah?
12. Bagaimana pembagian peran kelompok memengaruhi kualitas integrasi repository?

---

## 31. Failure Modes dan Prosedur Rollback

| Failure mode | Gejala | Penyebab umum | Diagnosis | Rollback/perbaikan |
|---|---|---|---|---|
| WSL belum terpasang | `wsl` tidak dikenali | Fitur belum aktif | Jalankan PowerShell Admin | Instal ulang WSL dengan `wsl --install` |
| Distro memakai WSL 1 | `VERSION` bernilai 1 | Default WSL salah | `wsl --list --verbose` | `wsl --set-version Ubuntu 2` |
| Repository di `/mnt/c` | Check script warning | Dibuat dari Windows path | `pwd` | Pindahkan ke `~/src/mcsos` |
| Paket tidak ditemukan | `command not found` | Paket belum terinstal | `command -v <tool>` | `sudo apt install ...` |
| QEMU tidak tersedia | `qemu-system-x86_64` gagal | Paket `qemu-system-x86` belum ada | `apt policy qemu-system-x86` | Instal paket QEMU |
| OVMF tidak ditemukan | `OVMF_CODE.fd` kosong | Path distro berbeda | `find /usr/share -iname 'OVMF_CODE*.fd'` | Catat path aktual atau instal `ovmf` |
| Smoke compile gagal | Clang error | Flag tidak kompatibel atau typo | Baca output compiler | Perbaiki flag, catat versi Clang |
| Object salah arsitektur | `Machine` bukan x86-64 | Target triple salah | `readelf -h` | Pakai `--target=x86_64-unknown-none` atau cross compiler benar |
| Git commit gagal | Identitas Git kosong | `user.name`/`user.email` belum diatur | `git config --global --list` | Set identitas Git |
| Shell script gagal permission | `Permission denied` | `chmod +x` belum dilakukan | `ls -l tools/check_env.sh` | `chmod +x tools/check_env.sh` |

---

## 32. Kriteria Lulus Praktikum

M0 lulus jika memenuhi kriteria minimum berikut.

1. Repository berada di filesystem Linux WSL dan dapat diperiksa dari clean shell.
2. Semua paket wajib tersedia atau kekurangan paket terdokumentasi dengan alasan yang dapat diterima.
3. `tools/check_env.sh` berjalan dan menghasilkan metadata versi toolchain.
4. `make smoke` menghasilkan object ELF64 x86-64 relocatable.
5. Struktur repository baseline sesuai panduan.
6. Dokumen requirements, assumptions/non-goals, ADR, invariants, threat model, risk register, dan verification matrix tersedia.
7. Laporan memuat command, output, log, screenshot seperlunya, commit hash, dan analisis failure modes.
8. Tidak ada klaim readiness berlebihan.
9. Perubahan Git sudah dikomit.
10. Mahasiswa atau kelompok dapat menjelaskan batas M0 dan syarat masuk M1.

Status maksimum yang boleh diberikan setelah M0 adalah:

```text
Siap uji lingkungan dan siap masuk M1 apabila seluruh acceptance evidence tersedia.
```

Status yang tidak boleh diberikan:

```text
Sistem operasi siap boot.
Sistem operasi tanpa error.
Sistem operasi siap produksi.
```

---

## 33. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | WSL 2 berjalan, tool wajib tersedia, repository benar, script validasi berjalan, smoke object benar. |
| Kualitas desain dan invariants | 20 | Requirements testable, ADR jelas, assumptions/non-goals konservatif, invariants dapat diperiksa. |
| Pengujian dan bukti | 20 | Metadata lengkap, output command lengkap, `readelf`/`objdump` tersedia, commit hash jelas. |
| Debugging dan failure analysis | 10 | Error diklasifikasi, penyebab dianalisis, rollback tepat, tidak menghapus bukti kegagalan. |
| Keamanan dan robustness | 10 | Threat model awal ada, supply-chain risk dipertimbangkan, fail-closed terminology dipakai. |
| Dokumentasi dan laporan | 10 | Laporan mengikuti template, bahasa jelas, tabel lengkap, lampiran cukup, referensi IEEE. |

Pengurangan nilai yang umum:

| Pelanggaran | Pengurangan maksimum |
|---|---:|
| Repository berada di `/mnt/c` tanpa analisis | -15 |
| Tidak ada metadata toolchain | -15 |
| Tidak ada smoke test object | -20 |
| Tidak ada commit Git | -15 |
| Dokumen threat model/risk register kosong | -10 |
| Klaim “siap produksi” atau “tanpa error” | -10 |
| Output error dihapus dari laporan | -10 |
| Laporan tidak mengikuti template | -10 |

---

## 34. Readiness Review M0

Gunakan tabel berikut pada akhir laporan.

| Area | Evidence | Status | Catatan |
|---|---|---|---|
| WSL 2 | `wsl --list --verbose` | Pass/Fail | |
| Repository location | `pwd` | Pass/Fail | |
| Toolchain availability | `tools/check_env.sh` | Pass/Fail | |
| Metadata versioning | `toolchain-versions.txt` | Pass/Fail | |
| Freestanding smoke object | `make smoke`, `readelf -h` | Pass/Fail | |
| Documentation baseline | `docs/...` | Pass/Fail | |
| Threat model | `docs/security/threat_model.md` | Pass/Fail | |
| Risk register | `docs/governance/risk_register.md` | Pass/Fail | |
| Verification matrix | `docs/testing/verification_matrix.md` | Pass/Fail | |
| Git traceability | `git log --oneline` | Pass/Fail | |

Kesimpulan readiness:

```text
M0 dinilai [lulus/belum lulus] sebagai baseline lingkungan dan governance.
Status readiness: [siap uji lingkungan / belum siap uji lingkungan].
M0 belum membuktikan kernel bootable dan belum memenuhi status siap uji QEMU.
Syarat masuk M1: seluruh failure pada tabel readiness ditutup atau diberi waiver tertulis oleh dosen/asisten.
```

---

## 35. Referensi

[1] Microsoft, “How to install Linux on Windows with WSL,” *Microsoft Learn*. Accessed: May 2, 2026. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/install

[2] Microsoft, “Advanced settings configuration in WSL,” *Microsoft Learn*. Accessed: May 2, 2026. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/wsl-config

[3] QEMU Project, “Invocation,” *QEMU Documentation*. Accessed: May 2, 2026. [Online]. Available: https://www.qemu.org/docs/master/system/invocation.html

[4] QEMU Project, “GDB usage,” *QEMU Documentation*. Accessed: May 2, 2026. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html

[5] GNU Project, “Prerequisites for GCC,” *Installing GCC*. Accessed: May 2, 2026. [Online]. Available: https://gcc.gnu.org/install/prerequisites.html

[6] GNU Project, “Installing GCC: Building,” *Installing GCC*. Accessed: May 2, 2026. [Online]. Available: https://gcc.gnu.org/install/build.html

[7] LLVM Project, “Cross-compilation using Clang,” *Clang Documentation*. Accessed: May 2, 2026. [Online]. Available: https://clang.llvm.org/docs/CrossCompilation.html

[8] Limine Project, “Limine,” *Limine Bootloader*. Accessed: May 2, 2026. [Online]. Available: https://limine-bootloader.org/

[9] Ubuntu, “Package Search Results — qemu-system-x86,” *Ubuntu Packages*. Accessed: May 2, 2026. [Online]. Available: https://packages.ubuntu.com/qemu-system-x86
