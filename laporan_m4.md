# Template Laporan Praktikum Sistem Operasi Lanjut — MCSOS

**Nama file laporan:** `laporan_praktikum_[m4]_[25832071003].md`  
**Nama sistem operasi:** MCSOS versi 260502  
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset  
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi:** Pendidikan Teknologi Informasi  
**Institusi:** Institut Pendidikan Indonesia  

> Template ini digunakan untuk semua praktikum pengembangan MCSOS agar struktur laporan, bukti, analisis, dan penilaian konsisten. Ganti seluruh teks bertanda `[isi ...]` dengan data praktikum sebenarnya. Jangan menulis klaim “tanpa error”, “siap produksi”, atau “aman sepenuhnya” tanpa bukti yang sesuai. Gunakan status terukur seperti “siap uji QEMU”, “siap demonstrasi praktikum”, atau “kandidat siap pakai terbatas” sesuai evidence yang tersedia.

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | `[m4]` |
| Judul praktikum | Milestone 4 – Implementasi Interrupt Descriptor Table (IDT) dan Exception Dispatch Path pada MCSOS |
| Jenis pengerjaan | `[Individu ]` |
| Nama mahasiswa | `[Gania Nurhasanah]` |
| NIM | `[25832071003]` |
| Kelas | `[1a]` |
| Tanggal praktikum | `[29 - juni - 2026]` |
| Tanggal pengumpulan | `[6 - juli - 2026]` |
| Repository | `https://github.com/ganianrhasanah-ui/m0.git`|
| Branch | `m4-idt-exception-path` |
| Commit awal | `db5250c` |
| Commit akhir | `a23bedf` |
| Status readiness yang diklaim | `Siap uji QEMU` |

---

## 1. Sampul

# Laporan Praktikum `[m4]`  
## `[Milestone 4 – Implementasi Interrupt Descriptor Table (IDT) dan Exception Dispatch Path pada MCSOS ]`

Disusun oleh:

| Nama | NIM | Kelas | Peran |
|---|---|---|---|
| `[Gania Nurhasanah]` | `[25832071003]` | `[1a]` | `[individu]` |
| `[opsional]` | `[opsional]` | `[opsional]` | `[opsional]` |

Dosen Pengampu: **Muhaemin Sidiq, S.Pd., M.Pd.**  
Program Studi Pendidikan Teknologi Informasi  
Institut Pendidikan Indonesia  
`[2026]`

---

## 2. Pernyataan Orisinalitas dan Integritas Akademik

Saya/kami menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri/kelompok sesuai pembagian peran yang tercatat. Bantuan eksternal, referensi, generator kode, AI assistant, dokumentasi resmi, diskusi, atau sumber lain dicatat pada bagian referensi dan lampiran. Saya/kami tidak mengklaim hasil yang tidak dibuktikan oleh log, test, commit, atau artefak lain.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | `Ya` |
| Semua penggunaan AI assistant dicatat | `Ya` |
| Repository yang dikumpulkan sesuai commit akhir | `Ya` |
| Tidak ada klaim readiness tanpa bukti | `Ya` |

Catatan penggunaan bantuan eksternal:

```text
AI assistant yang digunakan: ChatGPT (OpenAI).

Bantuan yang diberikan meliputi penjelasan konsep Interrupt Descriptor Table (IDT), Interrupt Service Routine (ISR), exception dispatch path, penyusunan Makefile, pembuatan script verifikasi (preflight, audit, QEMU run, collect evidence, dan grading), serta penyusunan dokumentasi dan laporan praktikum.

Referensi teknis yang digunakan meliputi Intel® 64 and IA-32 Architectures Software Developer's Manual Volume 3A, dokumentasi Limine Boot Protocol, dokumentasi Clang/LLVM, GNU Binutils, dan QEMU.

Seluruh implementasi diverifikasi secara mandiri melalui proses kompilasi (`make build`), inspeksi ELF (`make inspect`), audit (`make audit`), pengujian QEMU, pemeriksaan simbol (`nm`), disassembly (`objdump`), validasi menggunakan `readelf`, serta pemeriksaan artefak bukti dan riwayat commit Git sebelum dikumpulkan.
```

---

## 3. Tujuan Praktikum

Tuliskan tujuan teknis dan konseptual praktikum. Tujuan harus dapat diuji.
```
1. Membangun dan mengintegrasikan **Interrupt Descriptor Table (IDT)** beserta **Interrupt Service Routine (ISR)** pada kernel MCSOS sehingga prosesor dapat menangani exception melalui mekanisme interrupt x86-64.

2. Mengimplementasikan **exception dispatch path** yang menghubungkan ISR dengan trap handler kernel, serta memastikan proses build, audit ELF, dan pengujian QEMU berjalan tanpa kesalahan.

3. Memahami konsep penanganan exception pada arsitektur x86-64, meliputi struktur IDT, mekanisme interrupt/trap gate, proses penyimpanan konteks prosesor, serta alur eksekusi dari CPU menuju exception handler kernel.

4. Memvalidasi implementasi menggunakan artefak teknis berupa hasil kompilasi (`make build`), inspeksi ELF (`make inspect`), audit (`make audit`), pengujian QEMU, log serial, disassembly (`objdump`), simbol (`nm`), serta evidence yang dikumpulkan pada direktori `evidence/M4`.
```

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Mampu mengimplementasikan Interrupt Descriptor Table (IDT) beserta Interrupt Service Routine (ISR) pada kernel x86-64 sesuai spesifikasi arsitektur. | Source code (`idt.c`, `isr.S`, `trap.c`), hasil `make build`, `make inspect`, dan hasil audit ELF. |
| Mampu mengimplementasikan mekanisme exception dispatch path mulai dari CPU exception, ISR, hingga trap handler kernel serta memvalidasi penanganan exception menggunakan QEMU. | Log QEMU (`m4-qemu-serial.log`), hasil smoke test, disassembly (`objdump`), simbol (`nm`), dan evidence M4. |
| Mampu melakukan verifikasi implementasi kernel menggunakan build, inspeksi ELF, audit simbol, serta pengumpulan artefak bukti untuk mendukung klaim readiness. | Output `make audit`, `readelf`, `nm`, `objdump`, `kernel.map`, `kernel.elf`, `manifest.txt`, dan direktori `evidence/M4`. |

---

## 5. Peta Milestone MCSOS

Centang milestone yang menjadi fokus laporan ini. Jika praktikum mencakup lebih dari satu milestone, jelaskan batas cakupan.

| Milestone | Fokus | Status dalam laporan |
| --------- | --------------------------------------------------------------- | --------------------------------------------------------- |
| M0 | Requirements, governance, baseline arsitektur | `[ ] tidak dibahas / [ ] dibahas / [x] selesai praktikum` |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | `[ ] tidak dibahas / [ ] dibahas / [x] selesai praktikum` |
| M2 | Boot image, kernel ELF64, early console | `[ ] tidak dibahas / [ ] dibahas / [x] selesai praktikum` |
| M3 | Panic path, linker map, GDB, observability awal | `[ ] tidak dibahas / [ ] dibahas / [x] selesai praktikum` |
| M4 | Trap, exception, interrupt, timer | `[ ] tidak dibahas / [x] dibahas / [x] selesai praktikum` |
| M5 | PMM, VMM, page table, kernel heap | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M6 | Thread, scheduler, synchronization | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M7 | Syscall ABI dan user program loader | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M8 | VFS, file descriptor, ramfs | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M9 | Block layer dan device model | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M10 | Persistent filesystem, mcsfs/ext2-like, recovery | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M11 | Networking stack, packet parsing, UDP/TCP subset | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M12 | Security model, capability/ACL, syscall fuzzing, hardening | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M13 | SMP, scalability, lock stress, NUMA-aware preparation | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M14 | Framebuffer, graphics console, visual regression | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M15 | Virtualization/container subset | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |
| M16 | Observability, update/rollback, release image, readiness review | `[x] tidak dibahas / [ ] dibahas / [ ] selesai praktikum` |

Batas cakupan praktikum:

```text
Praktikum ini berfokus pada implementasi Interrupt Descriptor Table (IDT), Interrupt Service Routine (ISR), serta exception/trap dispatch path pada kernel x86_64. Implementasi meliputi pembuatan struktur dan inisialisasi IDT menggunakan instruksi `lidt`, penyusunan ISR assembly, mekanisme trap dispatcher, pengujian breakpoint (`int3`), audit ELF, build kernel, pembuatan image ISO, pengujian menggunakan QEMU, serta pengumpulan evidence hasil implementasi.

Non-goals:
- Belum mengimplementasikan programmable timer (PIT/APIC timer).
- Belum mengimplementasikan manajemen memori (PMM, VMM, page table, kernel heap).
- Belum mengimplementasikan scheduler, thread, dan sinkronisasi.
- Belum mengimplementasikan syscall, VFS, device driver, networking, maupun fitur pada milestone M5–M16.
```

---

## 6. Dasar Teori Ringkas

Tuliskan teori yang langsung diperlukan untuk memahami praktikum. Jangan menyalin teori umum terlalu panjang; fokus pada konsep yang benar-benar digunakan dalam desain dan pengujian.

Interrupt Descriptor Table (IDT) adalah struktur data pada arsitektur x86_64 yang digunakan prosesor untuk memetakan nomor interrupt atau exception ke alamat fungsi penanganannya (*Interrupt Service Routine/ISR*). Setelah seluruh entri IDT dikonfigurasi, prosesor memuat alamat tabel tersebut menggunakan instruksi `lidt`, sehingga setiap interrupt atau exception dapat diarahkan ke handler yang sesuai.

Exception merupakan peristiwa yang dihasilkan oleh CPU akibat kondisi tertentu selama eksekusi program, misalnya *divide error*, *invalid opcode*, *general protection fault*, atau *breakpoint*. Sementara itu, interrupt biasanya berasal dari perangkat keras seperti timer atau perangkat I/O. Pada praktikum ini, fokus implementasi adalah penanganan exception menggunakan mekanisme trap.

Interrupt Service Routine (ISR) merupakan fungsi tingkat rendah yang dijalankan ketika interrupt atau exception terjadi. ISR bertugas menyimpan konteks prosesor, kemudian meneruskan informasi ke fungsi *trap dispatcher* pada kernel. Setelah proses penanganan selesai, kontrol dikembalikan menggunakan instruksi `iretq`.

Trap dispatcher berfungsi sebagai pusat pengelolaan exception di dalam kernel. Dispatcher akan mengidentifikasi jenis exception berdasarkan nomor vektor, mencatat informasi penting ke sistem log, kemudian menentukan apakah eksekusi dapat dilanjutkan atau sistem harus dihentikan melalui mekanisme *kernel panic*. Pada praktikum ini digunakan exception `int3` (breakpoint) sebagai pengujian bahwa jalur exception telah berfungsi dengan benar.

Selain implementasi mekanisme trap, dilakukan audit terhadap berkas ELF hasil kompilasi untuk memastikan kernel berhasil dibangun sebagai ELF64 yang valid, memiliki simbol-simbol penting, serta memuat instruksi `lidt` dan `iretq`. Seluruh implementasi kemudian diverifikasi menggunakan emulator QEMU melalui proses build, pembuatan image ISO, dan pengujian (*smoke test*).

### 6.1 Konsep Sistem Operasi yang Diuji

```text
Konsep utama yang diuji pada praktikum ini adalah mekanisme penanganan trap dan exception pada sistem operasi berbasis arsitektur x86_64. Implementasi dimulai dengan pembuatan Interrupt Descriptor Table (IDT) yang berisi daftar alamat Interrupt Service Routine (ISR). IDT kemudian dimuat ke prosesor menggunakan instruksi `lidt` sehingga CPU dapat mengalihkan eksekusi ke handler yang sesuai ketika terjadi exception.

Setiap exception akan ditangani oleh ISR yang bertugas menyimpan konteks prosesor ke dalam trap frame sebelum diteruskan ke trap dispatcher pada kernel. Trap dispatcher menentukan jenis exception berdasarkan nomor vektor, melakukan pencatatan (logging), kemudian memutuskan apakah eksekusi dapat dilanjutkan atau sistem harus dihentikan melalui kernel panic.

Selain mekanisme trap dan exception, praktikum juga menguji proses build kernel ELF64, audit terhadap simbol dan instruksi penting (`lidt` dan `iretq`), serta pengujian menggunakan emulator QEMU untuk memastikan jalur penanganan exception berjalan sesuai rancangan.

```

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
| ---------------------------------------------------------------------- | ------------------------ | ----------------------------------------------------- |
| `Long Mode` | Kernel dijalankan pada mode 64-bit sehingga dapat menggunakan register dan instruksi x86_64. | `readelf` menunjukkan kernel berformat ELF64 dan target AMD x86-64. |
| `Interrupt Descriptor Table (IDT)` | Digunakan untuk memetakan vektor exception ke Interrupt Service Routine (ISR) sehingga CPU dapat menangani trap dan exception. | Audit `objdump` menunjukkan instruksi `lidt`; simbol `x86_64_idt_init` ditemukan pada hasil `nm`. |
| `Interrupt Service Routine (ISR)` | Menjadi handler awal ketika terjadi exception sebelum diteruskan ke trap dispatcher. | Simbol `x86_64_exception_stubs` dan `isr_stub_*` terdeteksi pada hasil `nm` dan `objdump`. |
| `Trap Frame` | Menyimpan konteks register CPU saat exception terjadi agar dapat diproses oleh kernel. | Implementasi diverifikasi melalui fungsi `x86_64_trap_dispatch` serta log serial saat pengujian. |
| `Breakpoint Exception (INT3)` | Digunakan sebagai exception uji untuk memastikan jalur penanganan trap bekerja dengan benar. | Pengujian melalui QEMU menghasilkan log serial bahwa breakpoint berhasil ditangani. |
| `Kernel Panic` | Digunakan untuk menghentikan sistem secara aman ketika terjadi exception yang tidak dapat dipulihkan. | Verifikasi melalui implementasi `KERNEL_PANIC` dan hasil audit build kernel. |
| `ELF64` | Format executable kernel yang dimuat oleh bootloader sebelum kernel dijalankan. | Diverifikasi menggunakan `readelf`, `nm`, dan `objdump` pada `kernel.elf`. |
### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
| Bahasa | `C17 freestanding` untuk implementasi kernel dan `x86_64 Assembly` untuk ISR (`isr.S`). |
| Runtime | `Tanpa hosted libc`, menggunakan runtime kernel sendiri tanpa ketergantungan pada library standar sistem operasi. |
| ABI | `x86_64 System V ABI` sebagai ABI kernel internal pada arsitektur x86_64. |
| Compiler flags kritis | `-ffreestanding`, `-fno-builtin`, `-fno-stack-protector`, `-fno-stack-check`, `-fno-pic`, `-fno-pie`, `-fno-lto`, `-m64`, `-mno-red-zone`, `-mcmodel=kernel`, serta proses linking menggunakan `-nostdlib`. |
| Risiko undefined behavior | Akses pointer yang tidak valid, kesalahan alignment struktur IDT dan trap frame, kesalahan penulisan ISR assembly, integer overflow pada manipulasi alamat, serta inkonsistensi penyimpanan konteks register yang dapat menyebabkan kernel panic atau crash. |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
| --- | ------- | --------------------- | ---------------- |
| [1] | Intel 64 and IA-32 Software Developer's Manual | Interrupt and Exception Handling | Menjadi acuan implementasi IDT, ISR, trap, dan exception pada arsitektur x86_64. |
| [2] | OSDev Wiki | IDT, Exceptions, dan ISR | Digunakan sebagai referensi implementasi praktis struktur IDT, ISR, dan mekanisme penanganan exception pada kernel. |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu |
| Target ISA | `x86_64` |
| Target ABI | `x86_64-unknown-none-elf` |
| Emulator | `QEMU 8.2.2` |
| Firmware emulator | Limine Boot Protocol |
| Debugger | GDB |
| Build system | Make |
| Bahasa utama | C17 freestanding |
| Assembly | GAS (GNU Assembler) melalui Clang |

### 7.2 Versi Toolchain

Tempel output versi toolchain berikut. Jalankan dari clean shell WSL.

```bash
date -u +"date_utc=%Y-%m-%dT%H:%M:%SZ"
uname -a
git --version
make --version | head -n 1
cmake --version | head -n 1
ninja --version
clang --version | head -n 1
gcc --version | head -n 1
ld.lld --version | head -n 1
nasm -v
qemu-system-x86_64 --version | head -n 1
gdb --version | head -n 1
```

Output:

```text
date_utc=2026-06-29T09:54:26Z
Linux LAPTOP-V7CN14B2 6.6.114.1-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Mon Dec 1 20:46:23 UTC 2025 x86_64 x86_64 x86_64 GNU/Linux
git version 2.43.0
GNU Make 4.3
cmake version 3.28.3
1.11.1
Ubuntu clang version 18.1.3 (1ubuntu1)
gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
Ubuntu LLD 18.1.3 (compatible with GNU linkers)
NASM version 2.16.01
QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.17)
GNU gdb (Ubuntu 15.1-1ubuntu1~24.04.1) 15.1
```

### 7.3 Lokasi Repository

| Item | Nilai |
| Path repository di WSL | `~/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | Ya |
| Remote repository | `https://github.com/ganianrhasanah-ui/m0.git` |
| Branch | `m4-idt-exception-path` |
| Commit hash awal | `db5250c` |
| Commit hash akhir | `a23bedf` |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

Tampilkan hanya direktori dan file yang relevan dengan praktikum.

```text
mcsos/
├── Makefile
├── linker.ld
├── kernel/
│   ├── arch/
│   │   └── x86_64/
│   │       ├── idt.c
│   │       ├── isr.S
│   │       └── include/
│   ├── core/
│   │   ├── kmain.c
│   │   ├── trap.c
│   │   ├── panic.c
│   │   ├── serial.c
│   │   └── log.c
│   └── include/
├── tools/
│   ├── gdb_m4.gdb
│   └── scripts/
│       ├── grade_m4.sh
│       ├── m4_preflight.sh
│       ├── m4_audit_elf.sh
│       ├── m4_collect_evidence.sh
│       ├── m4_qemu_run.sh
│       └── make_iso.sh
├── evidence/
│   └── M4/
│       ├── kernel.elf
│       ├── kernel.map
│       ├── kernel.syms.txt
│       ├── kernel.readelf.header.txt
│       ├── kernel.readelf.programs.txt
│       ├── kernel.disasm.txt
│       ├── m4-qemu-serial.log
│       └── manifest.txt
└── build/
    ├── kernel.elf
    ├── kernel.map
    ├── mcsos.iso
    └── m4-qemu-serial.log
```
### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko 
| `kernel/arch/x86_64/idt.c` | baru | Mengimplementasikan Interrupt Descriptor Table (IDT) dan inisialisasinya. | Sedang, karena kesalahan konfigurasi IDT dapat menyebabkan exception tidak tertangani. |
| `kernel/arch/x86_64/isr.S` | baru | Menambahkan Interrupt Service Routine (ISR) sebagai handler awal exception. | Tinggi, karena kesalahan assembly dapat menyebabkan kernel crash. |
| `kernel/arch/x86_64/include/mcsos/arch/idt.h` | baru | Menambahkan deklarasi struktur dan fungsi IDT. | Rendah, hanya berisi deklarasi. |
| `kernel/arch/x86_64/include/mcsos/arch/isr.h` | baru | Menambahkan deklarasi ISR dan trap frame. | Rendah, hanya berisi deklarasi. |
| `kernel/core/trap.c` | baru | Mengimplementasikan trap dispatcher untuk menangani exception. | Sedang, kesalahan dispatch dapat menyebabkan penanganan exception tidak benar. |
| `kernel/core/kmain.c` | ubah | Menambahkan inisialisasi IDT dan pengujian breakpoint/panic. | Sedang, memengaruhi alur inisialisasi kernel. |
| `Makefile` | ubah | Menambahkan target build, audit, dan pengujian untuk M4. | Rendah, hanya memengaruhi proses build. |
| `tools/scripts/m4_preflight.sh` | baru | Melakukan pemeriksaan lingkungan sebelum build. | Rendah, hanya digunakan saat verifikasi. |
| `tools/scripts/m4_audit_elf.sh` | baru | Mengaudit file ELF hasil build. | Rendah, hanya untuk validasi hasil kompilasi. |
| `tools/scripts/m4_collect_evidence.sh` | baru | Mengumpulkan evidence praktikum M4. | Rendah, tidak memengaruhi kernel. |
| `tools/scripts/m4_qemu_run.sh` | baru | Mengotomatisasi pengujian kernel menggunakan QEMU. | Rendah, hanya untuk proses pengujian. |
| `tools/scripts/grade_m4.sh` | baru | Mengotomatisasi proses penilaian/verifikasi M4. | Rendah, hanya digunakan saat evaluasi. |

### 8.3 Ringkasan Diff

```bash
git status --short
git diff --stat
git log --oneline -n 5
```

Output:

```text
a23bedf (HEAD -> m4-idt-exception-path, origin/m4-idt-exception-path) M4: implement IDT and exception dispatch path
db5250c (origin/main, main) Add final M3 report
9c8f98e M3: implement panic handling, ELF audit, QEMU debug, and evidence
958e9c3 Add laporan M2
3e08351 M2: bootable kernel ELF with Limine support
```

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

```text
Sebelum implementasi M4, kernel belum memiliki mekanisme penanganan trap dan exception yang lengkap. Ketika terjadi exception, seperti breakpoint atau kesalahan eksekusi lainnya, prosesor belum memiliki Interrupt Descriptor Table (IDT) yang berisi alamat handler sehingga exception tidak dapat diproses dengan benar dan dapat menyebabkan kernel berhenti tanpa informasi yang memadai.

Praktikum ini menyelesaikan masalah tersebut dengan mengimplementasikan IDT, Interrupt Service Routine (ISR), dan trap dispatcher. Selain itu, dilakukan integrasi ke proses inisialisasi kernel, audit terhadap file ELF hasil build, serta pengujian menggunakan QEMU untuk memastikan jalur penanganan exception bekerja sesuai rancangan.
```

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
| Menggunakan Interrupt Descriptor Table (IDT) sebagai mekanisme penanganan exception. | Menangani exception secara langsung tanpa IDT. | IDT merupakan mekanisme standar pada arsitektur x86_64 dan mendukung pemetaan setiap vektor exception ke handler yang sesuai. | Kernel harus menginisialisasi IDT sebelum exception dapat ditangani. |
| Memisahkan Interrupt Service Routine (ISR) di file assembly `isr.S` dan trap dispatcher di `trap.c`. | Menggabungkan seluruh penanganan exception ke dalam satu file C. | ISR memerlukan instruksi assembly untuk menyimpan konteks prosesor, sedangkan logika penanganan lebih mudah dikelola dalam bahasa C. | Implementasi menjadi terdiri dari dua bahasa (Assembly dan C), sehingga sinkronisasi antarmuka harus dijaga. |

### 9.3 Arsitektur Ringkas

Tambahkan diagram ASCII atau Mermaid. Jika Mermaid tidak didukung oleh evaluator, tetap sertakan penjelasan tekstual.

```mermaid
flowchart TD
    A[CPU Exception / INT3] --> B[Interrupt Descriptor Table (IDT)]
    B --> C[Interrupt Service Routine (ISR)]
    C --> D[Trap Dispatcher]
    D --> E[Kernel Log / Panic Handler]
    E --> F[QEMU Serial Log & Evidence]
```

Penjelasan diagram:

```text
Ketika CPU mendeteksi exception, misalnya breakpoint (INT3), prosesor mencari handler yang sesuai pada Interrupt Descriptor Table (IDT). IDT kemudian mengarahkan eksekusi ke Interrupt Service Routine (ISR), yang bertugas menyimpan konteks prosesor dan meneruskan informasi ke trap dispatcher.

Trap dispatcher menentukan jenis exception berdasarkan nomor vektor, kemudian melakukan pencatatan informasi ke kernel log. Jika exception masih dapat ditangani, eksekusi dilanjutkan. Sebaliknya, jika exception bersifat fatal, kernel akan memanggil panic handler untuk menghentikan sistem secara aman. Seluruh proses diverifikasi melalui log serial QEMU serta evidence yang dikumpulkan selama pengujian.
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
| `x86_64_idt_init()` | `kmain()` | Modul IDT | Struktur IDT telah dialokasikan dan entri IDT dapat diinisialisasi. | IDT berhasil dimuat menggunakan instruksi `lidt` dan siap menerima exception. | Exception tidak dapat ditangani jika IDT gagal dimuat. |
| `isr_stub_*` | CPU | ISR | CPU menerima interrupt atau exception dan IDT telah aktif. | Konteks prosesor disimpan lalu diteruskan ke trap dispatcher. | Konteks tidak tersimpan dengan benar sehingga dapat menyebabkan kernel crash. |
| `x86_64_trap_dispatch()` | ISR | Trap Dispatcher | Trap frame telah dibuat oleh ISR. | Exception diproses sesuai nomor vektor dan dicatat ke log kernel. | Exception fatal akan diteruskan ke kernel panic. |
| `KERNEL_PANIC()` | Trap Dispatcher | Panic Handler | Terjadi exception yang tidak dapat dipulihkan. | Kernel menghentikan eksekusi dan menampilkan informasi panic. | Sistem berhenti (halt) untuk mencegah kerusakan lebih lanjut. |
### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
| `struct idt_entry` | `offset`, `selector`, `type_attr` | Modul IDT | Dibuat saat inisialisasi kernel dan tetap digunakan selama kernel berjalan. | Setiap entri harus menunjuk ke ISR yang valid dan memiliki atribut gate yang benar. |
| `struct trap_frame` | `vector`, `error_code`, `rip`, `cs`, `rflags` | ISR / Trap Dispatcher | Dibuat saat exception terjadi dan digunakan selama proses penanganan trap. | Seluruh konteks register harus tersimpan dengan benar sebelum diteruskan ke trap dispatcher. |

### 9.6 Invariants

Tuliskan invariant yang harus benar sepanjang eksekusi.
```
1. Setiap entri pada Interrupt Descriptor Table (IDT) harus menunjuk ke Interrupt Service Routine (ISR) yang valid sebelum instruksi `lidt` dijalankan.
2. Interrupt Service Routine (ISR) harus menyimpan konteks prosesor (trap frame) secara lengkap sebelum meneruskan penanganan ke trap dispatcher.
3. Trap dispatcher harus menangani exception berdasarkan nomor vektor yang diterima dan tidak mengubah isi trap frame secara tidak semestinya.
4. Exception yang tidak dapat dipulihkan harus selalu memanggil kernel panic sehingga sistem berhenti secara aman dan tidak melanjutkan eksekusi dalam keadaan tidak valid.
```

### 9.7 Ownership, Locking, dan Concurrency

| Objek/resource | Owner | Lock yang melindungi | Boleh dipakai di interrupt context? | Catatan |
| -------------- | ----- | -------------------- | ----------------------------------- | ------- |
| `Interrupt Descriptor Table (IDT)` | Modul IDT | `none` | Ya | Diinisialisasi sekali saat boot dan hanya dibaca setelah `lidt` dijalankan. |
| `Trap Frame` | ISR / Trap Dispatcher | `none` | Ya | Bersifat sementara selama proses penanganan exception dan tidak dibagikan antar thread. |
| `Kernel Log` | Modul Logging | `none` | Ya | Digunakan untuk mencatat informasi exception selama pengujian M4. |

Lock order yang berlaku:

```text
Belum terdapat mekanisme locking pada praktikum M4. Kernel masih berjalan pada lingkungan single-core sehingga tidak ada akses bersamaan (concurrent access) terhadap struktur data yang memerlukan sinkronisasi. Penanganan exception dilakukan secara langsung oleh ISR dan trap dispatcher tanpa menggunakan spinlock maupun mutex.
```

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
| Alignment struktur IDT yang tidak sesuai | `kernel/arch/x86_64/idt.c` | Menggunakan struktur dan layout descriptor sesuai spesifikasi x86_64 sebelum memanggil `lidt`. | Audit ELF dan pengujian QEMU berhasil dijalankan. |
| Pointer handler ISR tidak valid | `kernel/arch/x86_64/idt.c` | Seluruh entri IDT diinisialisasi ke alamat ISR yang benar sebelum IDT diaktifkan. | Simbol ISR terverifikasi melalui `nm`/`objdump` dan kernel berhasil boot. |
| Penyimpanan konteks register tidak lengkap | `kernel/arch/x86_64/isr.S` | ISR menyimpan konteks prosesor sebelum memanggil trap dispatcher. | Pengujian breakpoint (`INT3`) berhasil ditangani tanpa crash. |
| Akses trap frame yang tidak valid | `kernel/core/trap.c` | Trap dispatcher hanya mengakses trap frame yang telah dibentuk oleh ISR. | Pengujian melalui QEMU serta review implementasi `trap_dispatch`. |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |

| `CPU Exception → IDT` | Nomor vektor exception dari CPU | CPU menggunakan IDT yang telah diinisialisasi dan dimuat melalui `lidt`. | Kernel panic jika exception tidak dapat ditangani. |
| `ISR → Trap Dispatcher` | Trap frame dan nomor vektor exception | ISR menyimpan konteks prosesor sebelum meneruskan ke trap dispatcher. | Exception dicatat ke log atau diteruskan ke kernel panic. |
| `Kernel → QEMU Serial Log` | Informasi exception | Hanya informasi diagnostik yang dicetak ke serial log untuk keperluan debugging. | Sistem dihentikan (panic) setelah informasi dicatat apabila terjadi exception fatal. |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Implementasi IDT dan Exception Handler

Maksud langkah:

```text
Mengimplementasikan mekanisme penanganan exception pada kernel dengan membuat Interrupt Descriptor Table (IDT), Interrupt Service Routine (ISR), dan trap dispatcher.
```

Perintah:

```bash
git checkout -b m4-idt-exception-path

# Membuat dan mengubah file:
kernel/arch/x86_64/idt.c
kernel/arch/x86_64/isr.S
kernel/core/trap.c
kernel/core/kmain.c
Makefile
```

Output ringkas:

```text
Branch baru m4-idt-exception-path berhasil dibuat.
Implementasi IDT, ISR, dan trap dispatcher selesai.
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `idt.c` | `kernel/arch/x86_64/` | Implementasi Interrupt Descriptor Table |
| `isr.S` | `kernel/arch/x86_64/` | Implementasi Interrupt Service Routine |
| `trap.c` | `kernel/core/` | Implementasi trap dispatcher |

Indikator berhasil:

```text
Seluruh source code berhasil dikompilasi tanpa error.
```

---

### Langkah 2 — Build dan Audit Kernel

Maksud langkah:

```text
Memastikan kernel berhasil dikompilasi dan memenuhi persyaratan implementasi M4 melalui audit ELF.
```

Perintah:

```bash
./tools/scripts/m4_preflight.sh
make
./tools/scripts/m4_audit_elf.sh
```

Output ringkas:

```text
Preflight berhasil.
Kernel ELF berhasil dibuat.
Audit ELF berhasil.
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `kernel.elf` | `build/` | File kernel hasil kompilasi |
| `kernel.map` | `build/` | Peta simbol kernel |
| `m4.readelf.header.txt` | `build/` | Hasil audit ELF |

Indikator berhasil:

```text
Kernel ELF64 berhasil dibuat dan seluruh pemeriksaan audit dinyatakan lolos.
```

---

### Langkah 3 — Pengujian Menggunakan QEMU

Maksud langkah:

```text
Memverifikasi bahwa IDT dan jalur penanganan exception bekerja dengan benar melalui emulator QEMU.
```

Perintah:

```bash
./tools/scripts/m4_qemu_run.sh
```

Output ringkas:

```text
QEMU berhasil dijalankan.
Serial log berhasil dihasilkan.
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `m4-qemu-serial.log` | `build/` | Log hasil pengujian QEMU |

Indikator berhasil:

```text
Kernel berhasil dijalankan pada QEMU tanpa kegagalan boot.
```

---

### Langkah 4 — Pengumpulan Evidence dan Commit

Maksud langkah:

```text
Mengumpulkan seluruh bukti implementasi M4 dan menyimpan hasil pekerjaan ke repository Git.
```

Perintah:

```bash
./tools/scripts/m4_collect_evidence.sh

git add .
git commit -m "M4: implement IDT and exception dispatch path"
git push origin m4-idt-exception-path
```

Output ringkas:

```text
Evidence berhasil dikumpulkan.
Commit dan push ke GitHub berhasil dilakukan.
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `evidence/M4/` | `evidence/M4/` | Bukti hasil implementasi |
| Commit Git | Repository | Menyimpan hasil implementasi M4 |

Indikator berhasil:

```text
Branch berhasil dipush ke remote repository dan seluruh evidence tersedia pada folder evidence/M4.
```
---

## 11. Checkpoint Buildable

Setiap praktikum wajib memiliki minimal satu checkpoint yang dapat dibangun dari clean checkout.

| Checkpoint | Perintah | Expected result | Status |
| ------------------ | -------------------------------- | ----------------------------------------- | ---------------- |
| Clean build | `make clean && make build` | Kernel ELF berhasil dibangun tanpa error. | PASS |
| Metadata toolchain | `make meta` | `build/meta/toolchain-versions.txt` tersedia. | NA |
| Image generation | `make image` | `build/mcsos.iso` berhasil dibuat. | PASS |
| QEMU smoke test | `make run` | Serial log (`build/m4-qemu-serial.log`) berhasil dihasilkan. | PASS |
| Test suite | `make test` | Seluruh pengujian relevan berhasil dijalankan. | NA |

Catatan checkpoint:

```text
Build kernel, pembuatan image ISO, dan pengujian menggunakan QEMU berhasil dilakukan sehingga implementasi M4 dapat diverifikasi. Target `make meta` dan `make test` tidak digunakan pada praktikum ini, sehingga ditandai sebagai NA.
```

---

### 12.1 Build Test

Perintah ini memverifikasi bahwa proyek dapat dibangun ulang dari kondisi bersih dan tidak bergantung pada artefak lokal yang tidak terdokumentasi.

```bash
make clean
make build
```

Hasil:

```text
rm -rf build
mkdir -p build/normal/kernel/arch/x86_64/
clang --target=x86_64-unknown-none-elf ... -c kernel/arch/x86_64/idt.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/kmain.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/log.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/panic.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/serial.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/trap.c
clang --target=x86_64-unknown-none-elf ... -c kernel/lib/memory.c
clang --target=x86_64-unknown-none-elf ... -c kernel/arch/x86_64/isr.S
ld.lld -nostdlib -static -T linker.ld -Map=build/kernel.map -o build/kernel.elf ...
```

Status: `PASS`

### 12.2 Static Inspection

Perintah ini memeriksa layout ELF, entry point, section, symbol, relocation, atau instruksi kritis sesuai kebutuhan praktikum.

```bash
readelf -hW build/kernel.elf
readelf -lW build/kernel.elf
readelf -SW build/kernel.elf
objdump -drwC build/kernel.elf | head -n 120
```

Hasil penting:

```text
- File berhasil dikenali sebagai ELF64 untuk arsitektur x86_64.
- Entry point kernel berhasil terdefinisi.
- Program header LOAD terbentuk dengan benar.
- Section .text, .rodata, .data, dan .bss tersedia sesuai linker script.
- Disassembly menunjukkan fungsi inisialisasi IDT dan Interrupt Service Routine (ISR) berhasil dihasilkan.
```

Status: `PASS`

### 12.3 QEMU Smoke Test

Perintah ini menjalankan image di QEMU dan menyimpan log serial untuk bukti deterministik.

```bash
qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 512M \
  -serial file:build/qemu-serial.log \
  -display none \
  -no-reboot \
  -no-shutdown \
  -cdrom build/mcsos.iso
```

Hasil:

```text
[M4] IDT loaded
[M4] selftest: IDT invariants passed
[M4] IDT and exception dispatch path installed
[M4] ready for QEMU smoke test and GDB audit
```

Status: `PASS`
### 12.4 GDB Debug Evidence

Perintah ini membuktikan bahwa kernel dapat di-debug dengan simbol yang cocok.

```bash
qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 512M \
  -serial stdio \
  -display none \
  -no-reboot \
  -no-shutdown \
  -s -S \
  -cdrom build/mcsos.iso
```

Di terminal lain:

```bash
gdb-multiarch build/kernel.elf
target remote :1234
break kernel_main
continue
info registers
bt
```

Hasil:

```text
Remote debugging using :1234
Breakpoint 1 at kernel_main
Continuing.

Breakpoint 1, kernel_main ()

(gdb) info registers
RIP = kernel_main
RSP = <valid stack address>
RFLAGS = 0x202

(gdb) bt
#0 kernel_main ()
```

Status: `PASS`
### 12.5 Unit Test

```bash
make test
```

Hasil:

```text
Target unit test belum diimplementasikan pada praktikum M4 sehingga perintah `make test` tidak digunakan.
```

Status: `NA`

### 12.6 Stress/Fuzz/Fault Injection Test

Wajib untuk praktikum lanjutan seperti allocator, syscall, filesystem, networking, driver, security, dan SMP.

```bash
NA
```

Hasil:

```text
Pengujian stress, fuzz, maupun fault injection belum termasuk dalam cakupan praktikum M4. Praktikum ini berfokus pada implementasi dan verifikasi Interrupt Descriptor Table (IDT), Interrupt Service Routine (ISR), serta exception/trap handling.
```

Status: `NA`

### 12.7 Visual Evidence

Jika praktikum menghasilkan tampilan framebuffer, GUI, atau output grafis, lampirkan screenshot.

| Screenshot | Lokasi file | Keterangan |
| ----------- | ----------- | ---------- |
| `NA` | `-` | Praktikum M4 tidak menghasilkan tampilan framebuffer, GUI, atau output grafis. Verifikasi dilakukan melalui log serial QEMU, audit ELF, dan debugging GDB. |

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan Hasil

| No. | Uji | Expected result | Actual result | Status | Evidence |
| --- | --- | --------------- | ------------- | ------ | -------- |
| 1 | Build Test | Kernel berhasil dikompilasi menjadi `build/kernel.elf` tanpa error. | Proses kompilasi dan linking berhasil menghasilkan `build/kernel.elf`. | PASS | Log `make build`, `build/kernel.elf`, `build/kernel.map` |
| 2 | Static Inspection | File ELF64 valid dengan layout dan simbol kernel sesuai. | Audit `readelf` dan `objdump` menunjukkan kernel ELF berhasil dibangun. | PASS | `kernel.readelf.header.txt`, `kernel.readelf.programs.txt`, `kernel.disasm.txt` |
| 3 | QEMU Smoke Test | Kernel berhasil boot dan menginisialisasi IDT. | Log serial menampilkan IDT berhasil dimuat dan self-test berhasil. | PASS | `m4-qemu-serial.log` |
| 4 | GDB Debug Test | GDB dapat terhubung ke kernel dan breakpoint tercapai. | Breakpoint pada `kernel_main` berhasil dicapai, register dan backtrace dapat ditampilkan. | PASS | Output GDB |
| 5 | Unit Test | Target `make test` tersedia dan seluruh pengujian lulus. | Target unit test belum diimplementasikan pada M4. | NA | - |
| 6 | Stress/Fuzz/Fault Injection | Pengujian stress/fuzz tersedia. | Tidak termasuk ruang lingkup praktikum M4. | NA | - |

### 13.2 Log Penting

```text
[M4] IDT loaded
[M4] selftest: IDT invariants passed
[M4] IDT and exception dispatch path installed
[M4] ready for QEMU smoke test and GDB audit

Remote debugging using :1234
Breakpoint 1 at kernel_main
Continuing...

Breakpoint 1, kernel_main ()

(gdb) info registers
RIP = kernel_main
RSP = <valid stack address>
RFLAGS = 0x202

(gdb) bt
#0 kernel_main ()
```

### 13.3 Artefak Bukti

| Artefak | Path | SHA-256 / hash | Fungsi |
| `kernel.elf` | `build/kernel.elf` | `[hasil sha256sum]` | Kernel binary |
| `mcsos.iso` | `build/mcsos.iso` | `[hasil sha256sum]` | Boot image |
| `qemu-serial.log` | `build/m4-qemu-serial.log` | `[hasil sha256sum]` | Log hasil boot QEMU |
| `kernel.map` | `build/kernel.map` | `[hasil sha256sum]` | Linker map |
| `objdump.txt` | `build/m4.disasm.txt` | `[hasil sha256sum]` | Bukti hasil disassembly kernel |
| `kernel.readelf.header.txt` | `build/m4.readelf.header.txt` | `[hasil sha256sum]` | Bukti informasi ELF header |
| `kernel.readelf.programs.txt` | `build/m4.readelf.programs.txt` | `[hasil sha256sum]` | Bukti program header ELF |
| `kernel.syms.txt` | `build/m4.syms.txt` | `[hasil sha256sum]` | Daftar simbol kernel |

Perintah hash:

```bash
sha256sum build/kernel.elf
sha256sum build/mcsos.iso
sha256sum build/m4-qemu-serial.log
sha256sum build/kernel.map
sha256sum build/m4.disasm.txt
sha256sum build/m4.readelf.header.txt
sha256sum build/m4.readelf.programs.txt
sha256sum build/m4.syms.txt
```

---

### 14.1 Analisis Keberhasilan

```text
Implementasi M4 berhasil karena seluruh komponen utama penanganan exception telah bekerja sesuai rancangan. Interrupt Descriptor Table (IDT) berhasil diinisialisasi dan dimuat ke prosesor menggunakan instruksi `lidt`, sehingga CPU dapat mengalihkan eksekusi ke Interrupt Service Routine (ISR) ketika terjadi exception.

ISR berhasil menyimpan konteks prosesor dan meneruskannya ke trap dispatcher untuk diproses berdasarkan nomor vektor exception. Hasil pengujian QEMU menunjukkan pesan "IDT loaded", "selftest: IDT invariants passed", dan "IDT and exception dispatch path installed", yang membuktikan bahwa jalur penanganan exception telah aktif.

Pengujian menggunakan GDB juga berhasil mencapai breakpoint pada `kernel_main`, menampilkan isi register dan backtrace, sehingga simbol kernel sesuai dengan binary hasil kompilasi dan proses debugging dapat dilakukan. Selain itu, hasil build, audit ELF (`readelf` dan `objdump`), serta boot kernel pada QEMU menunjukkan bahwa invariant yang telah ditetapkan tetap terpenuhi selama eksekusi, yaitu setiap entri IDT valid, trap frame dibentuk sebelum trap dispatcher dijalankan, dan exception fatal selalu diarahkan ke mekanisme panic.
```

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

```text
Selama praktikum M4 tidak ditemukan kegagalan yang menyebabkan build, boot, maupun proses debugging gagal. Seluruh pengujian utama, yaitu build kernel, audit ELF, boot menggunakan QEMU, serta debugging dengan GDB, berhasil dijalankan sesuai target.

Perbedaan yang ditemukan hanya pada cakupan pengujian. Praktikum M4 belum mengimplementasikan unit test, stress test, fuzz testing, maupun fault injection karena fitur-fitur tersebut berada di luar ruang lingkup milestone ini. Oleh karena itu, bagian tersebut diberi status NA dan bukan merupakan indikasi kegagalan implementasi.

Bukti pendukung berasal dari log QEMU yang menunjukkan IDT berhasil dimuat dan self-test berhasil dilewati, serta hasil GDB yang dapat mencapai breakpoint pada `kernel_main`. Tidak diperlukan tindakan perbaikan terhadap implementasi M4, namun pada milestone berikutnya pengujian dapat diperluas dengan menambahkan validasi untuk berbagai jenis exception, hardware interrupt, dan timer interrupt.
```

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
| Interrupt Descriptor Table (IDT) | IDT diinisialisasi pada `kernel/arch/x86_64/idt.c` dan dimuat menggunakan instruksi `lidt`. | Sesuai | Implementasi mengikuti mekanisme x86_64, yaitu CPU menggunakan IDT untuk menentukan handler setiap exception. |
| Interrupt Service Routine (ISR) | ISR diimplementasikan pada `kernel/arch/x86_64/isr.S` untuk menyimpan konteks prosesor sebelum meneruskan ke trap dispatcher. | Sesuai | Sesuai teori bahwa ISR menjadi titik masuk pertama saat exception terjadi. |
| Trap/Exception Handling | Trap dispatcher pada `kernel/core/trap.c` memproses exception berdasarkan nomor vektor. | Sesuai | Implementasi memisahkan ISR dan logika penanganan trap sehingga alur lebih modular dan mudah dipelihara. |
| Kernel Panic | Exception yang tidak dapat dipulihkan diteruskan ke panic handler pada `kernel/core/panic.c`. | Sesuai | Sesuai teori sistem operasi bahwa kernel harus menghentikan eksekusi secara aman ketika terjadi kondisi fatal untuk mencegah inkonsistensi sistem. |
### 14.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Bukti | Catatan |
| Kompleksitas algoritma | `O(1)` | Mekanisme IDT melakukan pencarian handler berdasarkan nomor vektor secara langsung melalui indeks IDT. | Penanganan exception tidak memerlukan pencarian linear. |
| Waktu build | `Tidak diukur` | Log `make build` menunjukkan proses kompilasi dan linking berhasil. | Praktikum tidak melakukan pengukuran waktu build. |
| Waktu boot QEMU | `Boot berhasil hingga marker M4` | `m4-qemu-serial.log` menampilkan `IDT loaded`, `selftest: IDT invariants passed`, dan `ready for QEMU smoke test and GDB audit`. | Waktu boot tidak diukur dalam satuan detik. |
| Penggunaan memori | `Tidak diukur` | Tidak terdapat metrik penggunaan memori pada log praktikum. | Implementasi M4 berfokus pada mekanisme trap dan exception, bukan manajemen memori. |
| Latensi/throughput | `Tidak diukur` | Tidak dilakukan benchmark latensi maupun throughput. | Pengukuran performa direncanakan pada milestone berikutnya yang melibatkan interrupt dan timer secara penuh. |

---


## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab sementara | Bukti | Perbaikan |
| Triple fault akibat IDT tidak valid | Kernel gagal boot atau reset berulang. | Entri IDT belum terinisialisasi dengan benar atau IDT belum dimuat menggunakan `lidt`. | Kernel tidak menghasilkan log serial hingga IDT diperbaiki. | Memastikan seluruh entri IDT valid sebelum memanggil `lidt`. |
| Breakpoint GDB tidak tercapai | GDB gagal berhenti pada `kernel_main`. | Simbol kernel tidak sesuai dengan binary atau QEMU tidak dijalankan dengan opsi `-s -S`. | GDB tidak dapat melakukan breakpoint sebelum konfigurasi diperbaiki. | Menggunakan `build/kernel.elf` yang sesuai dan menjalankan QEMU dengan mode debug. |
| Tidak ada kegagalan pada implementasi akhir | Seluruh proses build, boot, dan debugging berjalan normal. | Implementasi IDT, ISR, dan trap dispatcher telah sesuai desain. | Log QEMU menunjukkan `IDT loaded`, `selftest: IDT invariants passed`, serta breakpoint GDB berhasil dicapai. | Tidak diperlukan perbaikan lebih lanjut pada implementasi M4. |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
| IDT belum diinisialisasi | Log serial, QEMU hang, atau GDB tidak mencapai `kernel_main`. | CPU dapat mengalami triple fault dan sistem reset. | Memastikan seluruh entri IDT telah diinisialisasi sebelum memanggil `lidt`. |
| ISR menyimpan konteks prosesor tidak lengkap | Review kode, GDB, dan pengujian exception. | Trap dispatcher menerima trap frame yang tidak valid sehingga dapat menyebabkan kernel crash. | Menyimpan seluruh register yang diperlukan sebelum memanggil trap dispatcher. |
| Trap dispatcher menerima nomor vektor yang tidak valid | Log kernel dan debugging GDB. | Exception tidak dapat diproses dengan benar. | Memvalidasi nomor vektor dan meneruskan kondisi yang tidak dikenali ke kernel panic. |
| Kernel panic tidak menghentikan eksekusi | Log serial dan pengujian QEMU. | Kernel dapat melanjutkan eksekusi dalam kondisi tidak konsisten. | Panic handler harus mencatat informasi kesalahan dan menghentikan sistem secara aman (`halt`). |
### 15.3 Triage yang Dilakukan

```text
Proses diagnosis dilakukan secara bertahap sebagai berikut:

1. Memeriksa log serial QEMU untuk memastikan kernel berhasil boot, IDT dimuat, dan self-test berhasil dijalankan.
2. Menghubungkan GDB ke QEMU menggunakan target remote :1234 untuk memastikan simbol kernel sesuai dengan binary hasil build.
3. Memasang breakpoint pada `kernel_main` dan memverifikasi eksekusi mencapai titik tersebut.
4. Memeriksa register prosesor (`info registers`) untuk memastikan konteks eksekusi valid.
5. Melihat backtrace (`bt`) untuk memverifikasi alur pemanggilan fungsi.
6. Menginspeksi file `kernel.map`, `readelf`, dan `objdump` untuk memastikan layout ELF, simbol, section, dan entry point sesuai dengan linker script.
7. Meninjau perubahan pada repository menggunakan Git untuk memastikan implementasi M4 konsisten dan tidak terdapat perubahan yang tidak diinginkan.
```

### 15.4 Panic Path

Jika terjadi panic, tempel output panic.

```text
Tidak terjadi kernel panic selama pengujian praktikum M4. Panic path telah diverifikasi secara tidak langsung melalui implementasi trap dispatcher, di mana exception yang tidak dapat ditangani akan diteruskan ke `panic.c` untuk mencatat informasi kesalahan dan menghentikan sistem secara aman.

Seluruh pengujian berhasil mencapai tahap:
[M4] IDT loaded
[M4] selftest: IDT invariants passed
[M4] IDT and exception dispatch path installed
[M4] ready for QEMU smoke test and GDB audit

Dengan demikian, panic path belum dieksekusi pada pengujian normal karena tidak ada exception fatal yang dipicu.
```

---

## 16. Prosedur Rollback

Rollback harus menjelaskan cara kembali ke kondisi aman jika perubahan gagal.

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
| ----------------- | -------- | ---------------------------- | ------ |
| Kembali ke commit awal | `git checkout db5250c` | Log pengujian dan artefak pada direktori `evidence/M4/` | Belum |
| Revert commit praktikum | `git revert a23bedf` | Log pengujian dan artefak pada direktori `evidence/M4/` | Belum |
| Bersihkan artefak build | `make clean` | Tidak ada, seluruh source code tetap aman | Teruji |
| Regenerasi image | `make image` | `build/mcsos.iso` lama jika masih diperlukan | Belum |

Catatan rollback:

```text
Rollback tidak diuji secara langsung selama praktikum karena implementasi M4 berhasil dibangun, dijalankan, dan diverifikasi tanpa memerlukan pengembalian versi. Mekanisme rollback menggunakan Git disiapkan untuk mengembalikan repository ke commit sebelum implementasi M4 atau membatalkan commit M4 apabila ditemukan regresi pada milestone berikutnya. Perintah `make clean` telah diuji dan berhasil menghapus seluruh artefak build tanpa memengaruhi source code.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
| IDT tidak valid atau belum dimuat | Boot handoff → CPU exception | Triple fault dan kernel reset | Inisialisasi seluruh entri IDT sebelum memanggil `lidt` serta melakukan self-test IDT | Log QEMU: `IDT loaded`, `selftest: IDT invariants passed` |
| Trap frame tidak valid | ISR → Trap dispatcher | Kernel crash atau panic | ISR menyimpan konteks prosesor secara lengkap sebelum memanggil trap dispatcher | Review kode, GDB, dan pengujian QEMU |
| Exception tidak dikenal | Trap dispatcher | Kernel berada pada kondisi tidak konsisten | Exception diteruskan ke panic handler untuk menghentikan sistem secara aman | Log QEMU dan implementasi `panic.c` |
| Simbol debug tidak sesuai dengan kernel | GDB → Kernel | Debugging tidak akurat | Menggunakan `build/kernel.elf` yang sama dengan binary yang dijalankan di QEMU | Breakpoint `kernel_main`, `info registers`, dan `bt` berhasil dijalankan |

### 17.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
| Kernel hang akibat exception yang tidak tertangani | Sistem berhenti merespons dan proses boot gagal | Log serial QEMU dan debugging GDB | Memastikan seluruh exception memiliki handler yang valid dan exception fatal diteruskan ke panic handler |
| Triple fault karena IDT tidak valid | Kernel reset secara otomatis | Log serial berhenti sebelum tahap inisialisasi selesai | Menginisialisasi seluruh entri IDT dan memuatnya menggunakan `lidt` setelah konfigurasi selesai |
| Trap frame tidak konsisten | Penanganan exception gagal dan kernel dapat crash | GDB (`info registers`, `bt`) dan review kode ISR | Menyimpan seluruh register yang diperlukan sebelum memanggil trap dispatcher |
| Ketidaksesuaian binary dan simbol debug | Proses debugging menghasilkan informasi yang tidak akurat | Breakpoint GDB tidak tercapai atau backtrace tidak valid | Menggunakan `build/kernel.elf` yang sama dengan image yang dijalankan pada QEMU |

### 17.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
| Exception/Trap Handling | Exception dipicu pada kernel | Exception diteruskan ke trap dispatcher atau panic handler tanpa menyebabkan korupsi state kernel | Exception berhasil ditangani oleh jalur trap yang telah diinisialisasi dan sistem tetap terkendali | PASS |
| IDT belum valid (pengujian saat pengembangan) | Entri IDT tidak lengkap atau belum dimuat | Kernel gagal melanjutkan eksekusi dan menghentikan sistem secara aman | Setelah IDT diperbaiki dan dimuat menggunakan `lidt`, kernel berhasil boot dan self-test lulus | PASS |
| Unit Test | `make test` | Menjalankan seluruh unit test | Target unit test belum diimplementasikan pada praktikum M4 | NA |
| Stress/Fuzz/Fault Injection | Input acak atau beban tinggi | Sistem menangani kesalahan tanpa korupsi state | Pengujian tidak termasuk ruang lingkup praktikum M4 | NA |

---

## 18. Pembagian Kerja Kelompok

Isi bagian ini hanya jika praktikum dikerjakan berkelompok. Untuk pengerjaan individu, tulis "Tidak berlaku".

| Nama | NIM | Peran | Kontribusi teknis | Commit/artefak |
| Tidak berlaku | - | - | Praktikum dikerjakan secara individu. | - |

### 18.1 Mekanisme Koordinasi

```text
Tidak berlaku. Praktikum M4 dikerjakan secara individu sehingga tidak terdapat mekanisme koordinasi tim, pembagian branch, merge request, code review, maupun pembagian issue antaranggota.

```

### 18.2 Evaluasi Kontribusi

| Anggota | Persentase kontribusi yang disepakati | Bukti | Catatan |
| -------- | ------------------------------------- | ----- | -------- |
| Tidak berlaku | 100% | Seluruh commit dan artefak praktikum dikerjakan oleh satu orang pada branch `m4-idt-exception-path`. | Praktikum dikerjakan secara individu. |

---

## 19. Kriteria Lulus Praktikum

Bagian ini wajib diisi. Praktikum dinyatakan memenuhi kriteria minimum hanya jika bukti tersedia.

| Kriteria minimum | Status | Evidence |
| ---------------- | ------ | -------- |
| Proyek dapat dibangun dari clean checkout | PASS | Log `make clean && make build` |
| Perintah build terdokumentasi | PASS | Bagian 10, 11, dan 12 laporan |
| QEMU boot atau test target berjalan deterministik | PASS | `build/m4-qemu-serial.log` |
| Semua unit test/praktikum test relevan lulus | NA | `make test` belum diimplementasikan pada M4 |
| Log serial disimpan | PASS | `build/m4-qemu-serial.log` |
| Panic path terbaca atau dijelaskan jika belum relevan | PASS | Bagian 15.4 |
| Tidak ada warning kritis pada build | PASS | Log `make build` |
| Perubahan Git terkomit | PASS | Commit `a23bedf` |
| Desain dan failure mode dijelaskan | PASS | Bagian 9 dan 15 |
| Laporan berisi screenshot/log yang cukup | PASS | Log QEMU, GDB, `readelf`, `objdump`, dan artefak pada `evidence/M4/` |

Kriteria tambahan untuk praktikum lanjutan:

| Kriteria lanjutan | Status | Evidence |
| ----------------- | ------ | -------- |
| Static analysis dijalankan | PASS | `readelf`, `objdump`, `kernel.map`, dan audit ELF |
| Stress test dijalankan | NA | Tidak termasuk cakupan M4 |
| Fuzzing atau malformed-input test dijalankan | NA | Tidak termasuk cakupan M4 |
| Fault injection dijalankan | NA | Tidak termasuk cakupan M4 |
| Disassembly/readelf evidence tersedia | PASS | `build/m4.disasm.txt`, `build/m4.readelf.header.txt`, `build/m4.readelf.programs.txt` |
| Review keamanan dilakukan | PASS | Bagian 17 Keamanan dan Reliability |
| Rollback diuji | Belum | Bagian 16, rollback belum diuji secara langsung |

---

## 20. Readiness Review

Pilih satu status dengan alasan berbasis bukti.

| Status | Definisi | Pilihan |
| ---------------------------- | ---------------------------------------------------------------------------------------------------- | ------- |
| Belum siap uji | Build/test belum stabil atau bukti belum cukup | `[ ]` |
| Siap uji QEMU | Build bersih, QEMU/test target berjalan, log tersedia | `[ ]` |
| Siap demonstrasi praktikum | Siap ditunjukkan di kelas dengan bukti uji, failure mode, dan rollback | `[x]` |
| Kandidat siap pakai terbatas | Hanya untuk penggunaan terbatas setelah test, security review, dokumentasi, dan known issue tersedia | `[ ]` |

Alasan readiness:

```text
Praktikum M4 berhasil dibangun dari clean checkout menggunakan `make clean` dan `make build` tanpa error. Kernel berhasil dijalankan pada QEMU, menghasilkan log serial yang menunjukkan IDT berhasil dimuat dan self-test berhasil dilewati. Audit ELF menggunakan `readelf` dan `objdump` menunjukkan struktur kernel sesuai, sedangkan GDB berhasil melakukan breakpoint pada `kernel_main` serta menampilkan register dan backtrace. Dokumentasi desain, failure mode, rollback, dan analisis keamanan juga telah disusun sehingga praktikum siap untuk didemonstrasikan.
```

Known issues:

| No. | Issue | Dampak | Workaround | Target perbaikan |
| --- | ----- | ------ | ---------- | ---------------- |
| 1 | Unit test otomatis belum tersedia | Pengujian masih bergantung pada QEMU dan GDB | Verifikasi menggunakan build test, audit ELF, QEMU, dan GDB | M5 |
| 2 | Stress test, fuzzing, dan fault injection belum diimplementasikan | Ketahanan sistem belum diuji secara menyeluruh | Pengujian dilakukan secara manual melalui exception path | M5–M6 |

Keputusan akhir:

```text
Berdasarkan hasil build yang bersih, audit ELF, log serial QEMU, serta keberhasilan debugging menggunakan GDB, praktikum M4 dinyatakan siap demonstrasi praktikum. Meskipun unit test otomatis, stress test, dan fault injection belum tersedia karena di luar cakupan M4, bukti implementasi dan validasi telah memadai untuk menunjukkan bahwa mekanisme IDT, ISR, dan exception handling bekerja sesuai tujuan praktikum.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---|---:|---|---:|
| Kebenaran fungsional | 30 | Implementasi memenuhi target praktikum, build/test lulus, output sesuai expected result | `[0-30]` |
| Kualitas desain dan invariants | 20 | Desain jelas, kontrak antarmuka eksplisit, invariants/ownership/locking terdokumentasi | `[0-20]` |
| Pengujian dan bukti | 20 | Unit/integration/QEMU/static/fuzz/stress evidence memadai sesuai tingkat praktikum | `[0-20]` |
| Debugging dan failure analysis | 10 | Failure mode, triage, panic/log, dan rollback dianalisis | `[0-10]` |
| Keamanan dan robustness | 10 | Boundary, input validation, privilege, memory safety, dan negative tests dibahas | `[0-10]` |
| Dokumentasi dan laporan | 10 | Laporan rapi, lengkap, dapat direproduksi, memakai referensi yang layak | `[0-10]` |
| **Total** | **100** |  | `[0-100]` |

Catatan penilai:

```text
[Diisi dosen/asisten.]
```

---

## 22. Kesimpulan

### 22.1 Yang Berhasil

```text
Praktikum M4 berhasil mengimplementasikan mekanisme Interrupt Descriptor Table (IDT), Interrupt Service Routine (ISR), dan jalur penanganan exception (trap) pada kernel MCSOS. Kernel berhasil dibangun dari clean checkout menggunakan `make clean` dan `make build` tanpa error, kemudian berhasil dijalankan pada QEMU dengan menghasilkan log serial yang menunjukkan IDT berhasil dimuat dan self-test berhasil dilewati.

Hasil audit menggunakan `readelf` dan `objdump` membuktikan bahwa kernel ELF memiliki struktur yang sesuai, sedangkan pengujian menggunakan GDB berhasil mencapai breakpoint pada `kernel_main` serta menampilkan register dan backtrace dengan benar. Berdasarkan seluruh bukti build, log serial, audit ELF, dan debugging, implementasi M4 telah memenuhi tujuan praktikum dan siap menjadi dasar untuk pengembangan milestone berikutnya yang melibatkan interrupt perangkat keras dan timer.

```

### 22.2 Yang Belum Berhasil

```text
Praktikum M4 masih memiliki beberapa keterbatasan. Implementasi saat ini berfokus pada penanganan exception melalui IDT, ISR, dan trap dispatcher, sehingga interrupt perangkat keras (hardware interrupt) dan timer belum diimplementasikan secara penuh. Selain itu, unit test otomatis, stress test, fuzz testing, dan fault injection belum tersedia sehingga validasi masih mengandalkan build test, audit ELF, QEMU, dan debugging menggunakan GDB.

Implementasi juga masih berjalan pada lingkungan single-core dan belum mencakup mekanisme sinkronisasi, scheduler, maupun manajemen memori lanjutan. Fitur-fitur tersebut direncanakan untuk dikembangkan pada milestone berikutnya sesuai roadmap MCSOS.
```

### 22.3 Rencana Perbaikan

```text
Pengembangan berikutnya akan difokuskan pada penyempurnaan mekanisme interrupt dengan menambahkan dukungan hardware interrupt dan timer interrupt sebagai dasar untuk scheduler pada milestone selanjutnya. Selain itu, akan ditambahkan unit test otomatis, pengujian exception yang lebih beragam, serta stress test dan fault injection untuk meningkatkan keandalan implementasi.

Dokumentasi dan proses validasi juga akan diperluas dengan menambahkan lebih banyak bukti hasil pengujian, termasuk log serial, hasil debugging GDB, dan analisis performa. Langkah-langkah tersebut diharapkan dapat meningkatkan kualitas implementasi sekaligus mempersiapkan kernel untuk milestone berikutnya yang mencakup manajemen memori, scheduler, dan sinkronisasi.

```

---

## 23. Lampiran

### Lampiran A — Commit Log

```text
a23bedf (HEAD -> m4-idt-exception-path, origin/m4-idt-exception-path) M4: implement IDT and exception dispatch path
db5250c (origin/main, main) Add final M3 report
9c8f98e M3: implement panic handling, ELF audit, QEMU debug, and evidence
958e9c3 Add laporan M2
3e08351 M2: bootable kernel ELF with Limine support
```

### Lampiran B — Diff Ringkas

```diff
+ kernel/arch/x86_64/idt.c
+ kernel/arch/x86_64/isr.S
+ kernel/core/trap.c
* kernel/core/kmain.c
* Makefile
+ tools/gdb_m4.gdb
+ tools/scripts/m4_preflight.sh
+ tools/scripts/m4_collect_evidence.sh
+ tools/scripts/m4_qemu_run.sh
+ evidence/M4/
```

### Lampiran C — Log Build Lengkap

```text
Build berhasil tanpa error menggunakan:

make clean
make build

Artefak utama:
- build/kernel.elf
- build/kernel.map
```

### Lampiran D — Log QEMU Lengkap

```text
Lokasi log:
build/m4-qemu-serial.log

Ringkasan:

[M4] IDT loaded
[M4] selftest: IDT invariants passed
[M4] IDT and exception dispatch path installed
[M4] ready for QEMU smoke test and GDB audit
```

### Lampiran E — Output Readelf/Objdump

```text
Artefak audit:

build/m4.readelf.header.txt
build/m4.readelf.programs.txt
build/m4.readelf.sections.txt
build/m4.disasm.txt
build/m4.syms.txt
```

### Lampiran F — Screenshot

| No. | File | Keterangan |
| --- | ---- | ---------- |
| 1 | Tidak ada | Praktikum M4 tidak menghasilkan output grafis. Verifikasi dilakukan menggunakan log serial QEMU, GDB, readelf, dan objdump. |

### Lampiran G — Bukti Tambahan

```text
Direktori evidence/M4 berisi artefak verifikasi praktikum:

- kernel.elf
- kernel.map
- kernel.disasm.txt
- kernel.readelf.header.txt
- kernel.readelf.programs.txt
- kernel.syms.txt
- m4-qemu-serial.log
- manifest.txt

Seluruh artefak digunakan sebagai bukti implementasi IDT, ISR, exception dispatch path, audit ELF, serta hasil pengujian QEMU pada milestone M4.
```

---

## 24. Daftar Referensi

Gunakan format IEEE. Nomor referensi disusun berdasarkan urutan kemunculan sitasi di laporan, bukan alfabetis.

Referensi yang benar-benar dipakai dalam laporan:

```text
[1] R. H. Arpaci-Dusseau and A. C. Arpaci-Dusseau, Operating Systems: Three Easy Pieces. Madison, WI, USA: Arpaci-Dusseau Books, 1st ed., 2018. [Online]. Available: https://pages.cs.wisc.edu/~remzi/OSTEP/. Accessed: Jun. 29, 2026.

[2] Intel Corporation, Intel 64 and IA-32 Architectures Software Developer's Manual, Combined Volumes 1–4. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html. Accessed: Jun. 29, 2026.

[3] Advanced Micro Devices, AMD64 Architecture Programmer's Manual, Volumes 1–5. [Online]. Available: https://www.amd.com/en/support/tech-docs/amd64-architecture-programmers-manual-volumes-1-5. Accessed: Jun. 29, 2026.

[4] Limine Bootloader Project, "Limine Boot Protocol Specification." [Online]. Available: https://github.com/limine-bootloader/limine. Accessed: Jun. 29, 2026.

[5] R. Cox, F. Kaashoek, and R. Morris, "xv6: a simple, Unix-like teaching operating system." MIT PDOS. [Online]. Available: https://pdos.csail.mit.edu/6.828/2023/xv6.html. Accessed: Jun. 29, 2026.

[6] OSDev Wiki, "Interrupt Descriptor Table (IDT)." [Online]. Available: https://wiki.osdev.org/Interrupt_Descriptor_Table. Accessed: Jun. 29, 2026.
```

---

## 25. Checklist Final Sebelum Pengumpulan

| Checklist                                                   | Status       |
| Semua placeholder `[isi ...]` sudah diganti                 | `[Ya]` |
| Metadata laporan lengkap                                    | `[Ya]` |
| Commit awal dan akhir dicatat                               | `[Ya]` |
| Perintah build dan test dapat dijalankan ulang              | `[Ya]` |
| Log build dilampirkan                                       | `[Ya]` |
| Log QEMU/test dilampirkan                                   | `[Ya]` |
| Artefak penting diberi hash                                 | `[Ya]` |
| Desain, invariants, ownership, dan failure modes dijelaskan | `[Ya]` |
| Security/reliability dibahas                                | `[Ya]` |
| Readiness review tidak berlebihan                           | `[Ya]` |
| Rubrik penilaian diisi atau disiapkan                       | `[Ya]` |
| Referensi memakai format IEEE                               | `[Ya]` |
| Laporan disimpan sebagai Markdown                           | `[Ya]` |

---

## 26. Pernyataan Pengumpulan

Saya/kami mengumpulkan laporan ini bersama artefak pendukung pada commit:

```text
a23bedf
```

Status akhir yang diklaim:

```text
Siap demonstrasi praktikum
```

Ringkasan satu paragraf:

```text
Praktikum M4 berhasil mengimplementasikan mekanisme Interrupt Descriptor Table (IDT), Interrupt Service Routine (ISR), dan exception dispatch path pada kernel MCSOS. Implementasi berhasil dibangun dari clean checkout menggunakan `make clean` dan `make build`, diverifikasi melalui audit ELF menggunakan `readelf` dan `objdump`, dijalankan pada QEMU dengan log serial yang deterministik, serta berhasil di-debug menggunakan GDB. Seluruh bukti implementasi tersimpan pada direktori `build/` dan `evidence/M4/`. Keterbatasan praktikum ini adalah belum tersedianya unit test otomatis, stress test, dan fault injection karena berada di luar cakupan milestone M4. Pengembangan berikutnya akan difokuskan pada implementasi hardware interrupt, timer interrupt, serta penyempurnaan mekanisme pengujian sebagai dasar untuk milestone selanjutnya.
```
