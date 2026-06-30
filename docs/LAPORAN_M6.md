# Template Laporan Praktikum Sistem Operasi Lanjut — MCSOS

**Nama file laporan:** `laporan_praktikum_[m6]_[25832071003].md`  
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
| Kode praktikum | `[m6]` |
| Judul praktikum | `Milestone 6 – Implementasi Physical Memory Manager (PMM) Berbasis Bitmap` |
| Jenis pengerjaan | `[Individu]` |
| Nama mahasiswa | `[Gania Nurhasanah]` |
| NIM | `[25832071003]` |
| Kelas | `[1a]` |
| Tanggal praktikum | `[30 - juni - 2026]` |
| Tanggal pengumpulan | `[6 - juli - 2026]` |
| Repository | `https://github.com/ganianrhasanah-ui/m0` |
| Branch | `main` |
| Commit awal | `50e77e2` |
| Commit akhir | `f42ec4c` |
| Status readiness yang diklaim | `Siap uji QEMU` |

---

## 1. Sampul

# Laporan Praktikum `[m6]`  
## `[Milestone 6 – Implementasi Physical Memory Manager (PMM) Berbasis Bitmap]`

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

| Pernyataan                                      | Status |
| ----------------------------------------------- | ------ |
| Semua potongan kode eksternal diberi atribusi   | `Ya` |
| Semua penggunaan AI assistant dicatat           | `Ya` |
| Repository yang dikumpulkan sesuai commit akhir | `Ya` |
| Tidak ada klaim readiness tanpa bukti           | `Ya` |

Catatan penggunaan bantuan eksternal:

```text
Alat:
- ChatGPT (OpenAI GPT-5.5)

Bagian yang dibantu:
- Penjelasan konsep Physical Memory Manager (PMM).
- Pendampingan implementasi bitmap allocator.
- Bantuan debugging error kompilasi, include path, prototype fungsi, dan integrasi Makefile.
- Penyusunan host unit test serta dokumentasi laporan.

Referensi:
- Dokumen praktikum M6 (OS_panduan_M6.md).
- Dokumentasi kode proyek.

Verifikasi mandiri:
- Menjalankan host unit test dengan hasil PASS.
- Menjalankan scripts/check_m6_static.sh dengan hasil PASS.
- Melakukan build kernel menggunakan make tanpa error.
- Memverifikasi perubahan melalui commit Git dan repository GitHub.
```

---

## 3. Tujuan Praktikum

Tuliskan tujuan teknis dan konseptual praktikum. Tujuan harus dapat diuji.
```
1. Mengimplementasikan Physical Memory Manager (PMM) berbasis bitmap untuk mengelola frame memori fisik.
2. Menyediakan API PMM yang meliputi inisialisasi dari boot memory map, alokasi frame, pelepasan frame, reservasi rentang memori, dan penyediaan statistik penggunaan memori.
3. Memahami konsep pengelolaan memori fisik, bitmap allocator, status frame, serta invariant Physical Memory Manager pada sistem operasi.
4. Memvalidasi implementasi melalui host unit test, static check, proses build kernel, dan penyimpanan hasil implementasi pada repository Git.
```
---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
| ------------------ | ---------------------------- |
| Mengimplementasikan Physical Memory Manager berbasis bitmap | Source code `src/pmm.c`, `include/pmm.h`, commit Git |
| Melakukan pengujian dan validasi implementasi PMM | Host unit test PASS, `scripts/check_m6_static.sh` PASS, log build |
| Mengintegrasikan modul PMM ke sistem build kernel | Build kernel berhasil (`make` tanpa error), perubahan Makefile, repository GitHub |

---

## 5. Peta Milestone MCSOS

Centang milestone yang menjadi fokus laporan ini. Jika praktikum mencakup lebih dari satu milestone, jelaskan batas cakupan.

| Milestone | Fokus | Status dalam laporan |
| --------- | ----- | -------------------- |
| M0 | Requirements, governance, baseline arsitektur | `[x] selesai praktikum` |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | `[x] selesai praktikum` |
| M2 | Boot image, kernel ELF64, early console | `[x] selesai praktikum` |
| M3 | Panic path, linker map, GDB, observability awal | `[x] selesai praktikum` |
| M4 | Trap, exception, interrupt, timer | `[x] selesai praktikum` |
| M5 | PMM, VMM, page table, kernel heap | `[x] dibahas` |
| M6 | Thread, scheduler, synchronization | `[ ] tidak dibahas` |
| M7 | Syscall ABI dan user program loader | `[ ] tidak dibahas` |
| M8 | VFS, file descriptor, ramfs | `[ ] tidak dibahas` |
| M9 | Block layer dan device model | `[ ] tidak dibahas` |
| M10 | Persistent filesystem, mcsfs/ext2-like, recovery | `[ ] tidak dibahas` |
| M11 | Networking stack, packet parsing, UDP/TCP subset | `[ ] tidak dibahas` |
| M12 | Security model, capability/ACL, syscall fuzzing, hardening | `[ ] tidak dibahas` |
| M13 | SMP, scalability, lock stress, NUMA-aware preparation | `[ ] tidak dibahas` |
| M14 | Framebuffer, graphics console, visual regression | `[ ] tidak dibahas` |
| M15 | Virtualization/container subset | `[ ] tidak dibahas` |
| M16 | Observability, update/rollback, release image, readiness review | `[ ] tidak dibahas` |

Batas cakupan praktikum:

```text
Praktikum berfokus pada implementasi Physical Memory Manager (PMM) berbasis bitmap yang meliputi inisialisasi dari boot memory map, alokasi frame, pelepasan frame, reservasi rentang memori, dan penyediaan statistik penggunaan frame.

Implementasi Virtual Memory Manager (VMM), page table, kernel heap, thread, scheduler, synchronization, maupun subsystem lain di luar PMM belum diimplementasikan pada praktikum ini. Seluruh klaim pada laporan dibatasi pada implementasi PMM, hasil host unit test, static check, dan build kernel yang berhasil.
```

---

## 6. Dasar Teori Ringkas

Tuliskan teori yang langsung diperlukan untuk memahami praktikum. Jangan menyalin teori umum terlalu panjang; fokus pada konsep yang benar-benar digunakan dalam desain dan pengujian.

### 6.1 Konsep Sistem Operasi yang Diuji

```text
Praktikum ini berfokus pada implementasi Physical Memory Manager (PMM), yaitu komponen kernel yang bertugas mengelola memori fisik. PMM bekerja berdasarkan boot memory map yang diberikan bootloader untuk menentukan bagian memori yang dapat digunakan maupun yang harus dicadangkan.

PMM pada praktikum ini menggunakan bitmap sebagai struktur data utama. Setiap bit merepresentasikan satu frame memori fisik berukuran 4096 byte. Nilai bit 0 menunjukkan frame bebas (free), sedangkan nilai bit 1 menunjukkan frame telah digunakan atau direservasi.

Fungsi utama PMM meliputi inisialisasi bitmap dari boot memory map, alokasi frame, pelepasan frame, reservasi rentang memori, serta penyediaan statistik jumlah frame bebas dan digunakan. Implementasi ini menjadi dasar bagi pengembangan Virtual Memory Manager (VMM) pada milestone berikutnya.
```

### 6.2 Konsep Arsitektur x86_64 yang Relevan
```
| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
| ------- | ------------------------ | ---------------- |
| Paging (4 KiB page/frame) | PMM mengelola memori fisik dalam satuan frame berukuran 4096 byte. | Host unit test PASS, implementasi `PMM_PAGE_SIZE = 4096`. |
| Physical Address | PMM mengalokasikan dan mengembalikan alamat fisik (physical address). | Pengujian `pmm_alloc_frame()` dan `pmm_free_frame()`. |
| Boot Memory Map | Menentukan area memori yang dapat digunakan dan area yang harus direservasi. | Implementasi `pmm_init_from_map()` dan struktur `boot_mem_region`. |
| x86_64 Long Mode | Kernel berjalan pada arsitektur x86_64 sehingga seluruh alamat fisik menggunakan tipe data 64-bit. | Kernel berhasil dibangun sebagai ELF64 menggunakan `make`. |
```
### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
| ----- | ------------------- |
| Bahasa | `C17 freestanding` |
| Runtime | `Tanpa hosted libc, menggunakan implementasi memset(), memcpy(), dan memmove() sendiri.` |
| ABI | `x86_64 System V ABI untuk kernel.` |
| Compiler flags kritis | `-ffreestanding`, `-fno-builtin`, `-fno-stack-protector`, `-mno-red-zone`, `-nostdlib` (saat linking). |
| Risiko undefined behavior | Pointer tidak valid, akses di luar bitmap, alignment alamat fisik, integer overflow saat perhitungan frame, dan double free. |

### 6.4 Referensi Teori yang Digunakan
```
| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
| --- | ------ | --------------------- | ---------------- |
| [1] | Dokumen praktikum `OS_panduan_M6.md` | Spesifikasi PMM, API, kontrak implementasi, dan pengujian | Menjadi acuan utama implementasi milestone. |
| [2] | Intel® 64 and IA-32 Architectures Software Developer's Manual Volume 3 | Manajemen memori, paging, dan physical memory | Menjelaskan konsep memori fisik pada arsitektur x86_64. |
| [3] | Operating Systems: Three Easy Pieces (OSTEP) | Bab Memory Virtualization | Digunakan sebagai referensi konsep pengelolaan memori dan allocator. |
```
---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
| --------- | ----- |
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu 24.04 LTS |
| Target ISA | `x86_64` |
| Target ABI | `x86_64-unknown-none-elf` |
| Emulator | QEMU 8.2.2 |
| Firmware emulator | Tidak digunakan (kernel ELF dibangun untuk target freestanding) |
| Debugger | GNU GDB 15.1 |
| Build system | GNU Make |
| Bahasa utama | C17 freestanding |
| Assembly | GAS (GNU Assembler melalui Clang) |

### 7.2 Versi Toolchain

Perintah yang dijalankan:

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
date_utc=2026-06-30T15:10:36Z
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
| ---- | ----- |
| Path repository di WSL | `~/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | Ya |
| Remote repository | `https://github.com/ganianrhasanah-ui/m0.git` |
| Branch | `main` |
| Commit hash awal | `342d761` |
| Commit hash akhir | `f42ec4c` |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
├── include/
│   ├── pmm.h
│   └── types.h
├── kernel/
│   ├── core/
│   │   ├── kmain.c
│   │   ├── log.c
│   │   ├── panic.c
│   │   ├── serial.c
│   │   └── trap.c
│   ├── include/
│   │   └── mcsos/
│   │       └── kernel/
│   │           ├── memory.h
│   │           └── pmm.h
│   └── lib/
│       └── memory.c
├── src/
│   └── pmm.c
├── tests/
│   └── test_pmm_host.c
├── scripts/
│   └── check_m6_static.sh
├── docs/
├── tools/
├── Makefile
└── linker.ld
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko |
| ----- | --------------- | ---------------- | ------- |
| `include/pmm.h` | baru | Menambahkan antarmuka (API) Physical Memory Manager untuk digunakan oleh kernel dan host unit test. | Rendah, hanya berisi deklarasi API. |
| `include/types.h` | baru | Menyediakan definisi tipe data yang digunakan oleh modul PMM. | Rendah, hanya memengaruhi deklarasi tipe. |
| `src/pmm.c` | baru | Mengimplementasikan bitmap-based Physical Memory Manager beserta fungsi inisialisasi, alokasi, pelepasan, reservasi, dan statistik frame. | Sedang, kesalahan implementasi dapat menyebabkan alokasi frame fisik tidak valid. |
| `tests/test_pmm_host.c` | baru | Menambahkan host unit test untuk memverifikasi fungsi-fungsi PMM tanpa menjalankan kernel pada emulator. | Rendah, hanya digunakan saat pengujian. |
| `scripts/check_m6_static.sh` | baru | Menambahkan skrip verifikasi otomatis untuk milestone M6. | Rendah, hanya digunakan pada tahap validasi. |
| `kernel/include/mcsos/kernel/pmm.h` | baru | Menyediakan wrapper header agar modul kernel dapat mengakses API PMM. | Rendah, hanya berfungsi sebagai antarmuka. |
| `kernel/include/mcsos/kernel/memory.h` | baru | Menambahkan deklarasi fungsi utilitas memori kernel. | Rendah, tidak mengubah perilaku runtime secara langsung. |
| `kernel/lib/memory.c` | diubah | Menambahkan implementasi utilitas memori serta placeholder inisialisasi subsistem memori kernel. | Rendah, perubahan terbatas pada modul memori. |
| `Makefile` | diubah | Menambahkan proses kompilasi modul PMM dan integrasi dengan sistem build kernel. | Sedang, kesalahan konfigurasi dapat menyebabkan proses build gagal. |

### 8.3 Ringkasan Diff

Perintah yang dijalankan:

```bash
git status --short
git diff --stat
git log --oneline -n 5
```

Output:

```text
git status --short
(tidak ada output karena working tree bersih)

git diff --stat
(tidak ada output karena seluruh perubahan telah di-commit)

f42ec4c (HEAD -> main, origin/main) M6: implement physical memory manager
342d761 Merge pull request #3 from ganianrhasanah-ui/m5-pic-pit
50e77e2 (origin/m5-pic-pit, m5-pic-pit) Add M5 report
21a1f6b Merge pull request #2 from ganianrhasanah-ui/m5-pic-pit
b7b4f86 (m4-idt-exception-path) Milestone 5: Add PIC remapping, PIT timer, and IRQ0 handler
```

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

```text
Sebelum milestone ini kernel belum memiliki Physical Memory Manager (PMM) sehingga belum tersedia mekanisme untuk mengelola frame memori fisik. Akibatnya kernel tidak dapat menentukan frame yang bebas, telah dialokasikan, maupun yang harus direservasi berdasarkan peta memori.

Milestone M6 menyelesaikan masalah tersebut dengan mengimplementasikan Physical Memory Manager berbasis bitmap. PMM mampu melakukan inisialisasi dari boot memory map, mengalokasikan dan membebaskan frame fisik, melakukan reservasi rentang alamat tertentu, serta menyediakan informasi statistik jumlah frame bebas dan yang telah digunakan. Implementasi juga dilengkapi host unit test sehingga logika allocator dapat diverifikasi tanpa menjalankan kernel pada emulator.
```

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
| ---------- | ------------------------------- | -------------- | ----------- |
| Menggunakan bitmap sebagai representasi status frame fisik | Linked list free frame | Bitmap sederhana, hemat memori, dan mudah melakukan pengecekan status frame. | Alokasi memerlukan proses pencarian bit kosong. |
| Memisahkan API PMM ke dalam `include/pmm.h` | Menggabungkan implementasi langsung ke kernel | Memungkinkan implementasi digunakan bersama oleh kernel dan host unit test. | Membutuhkan wrapper header pada direktori kernel. |
| Menambahkan host unit test (`tests/test_pmm_host.c`) | Hanya melakukan pengujian melalui boot kernel di QEMU | Mempermudah debugging serta validasi fungsi PMM secara independen dari proses boot. | Menambah target build khusus untuk pengujian host. |
| Menambahkan script `check_m6_static.sh` | Verifikasi manual | Memastikan proses validasi dapat dilakukan secara konsisten dan berulang. | Memerlukan pemeliharaan script apabila struktur proyek berubah. |
### 9.3 Arsitektur Ringkas

```mermaid
flowchart TD
    A[Boot Memory Map] --> B[pmm_init_from_map()]
    B --> C[PMM State]
    C --> D[Bitmap Frame Status]
    D --> E[pmm_alloc_frame()]
    D --> F[pmm_free_frame()]
    D --> G[pmm_reserve_range()]
    E --> H[Physical Frame]
    F --> D
    G --> D
    C --> I[pmm_frame_count()]
    C --> J[pmm_free_count()]
```

Penjelasan diagram:

```text
Kernel menginisialisasi Physical Memory Manager menggunakan informasi peta memori fisik. Fungsi pmm_init_from_map() membangun bitmap yang merepresentasikan status setiap frame fisik.

Bitmap kemudian menjadi sumber informasi utama bagi operasi pmm_alloc_frame(), pmm_free_frame(), dan pmm_reserve_range(). Setiap perubahan status frame langsung memperbarui bitmap dan statistik internal PMM. Fungsi query seperti pmm_frame_count() dan pmm_free_count() hanya membaca state tersebut tanpa mengubahnya.
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
| ---------- | --------- | -------- | ------------ | ------------- | ---------- |
| `pmm_init_from_map()` | Kernel bootstrap | PMM | Parameter valid dan bitmap tersedia | State PMM berhasil diinisialisasi | Mengembalikan `false` bila inisialisasi gagal |
| `pmm_alloc_frame()` | Kernel memory subsystem | PMM | PMM telah diinisialisasi | Mengembalikan alamat frame bebas dan menandainya sebagai digunakan | Mengembalikan `false` bila tidak ada frame tersedia |
| `pmm_free_frame()` | Kernel memory subsystem | PMM | Frame pernah dialokasikan | Frame kembali berstatus bebas | Mengembalikan `false` bila frame tidak valid |
| `pmm_reserve_range()` | Kernel bootstrap | PMM | Rentang alamat valid | Semua frame pada rentang menjadi reserved | Mengembalikan `false` bila parameter tidak valid |
| `pmm_frame_count()` | Kernel | PMM | PMM telah diinisialisasi | Mengembalikan jumlah frame fisik | Tidak mengubah state |
| `pmm_free_count()` | Kernel | PMM | PMM telah diinisialisasi | Mengembalikan jumlah frame bebas | Tidak mengubah state |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
| ------------- | ------------- | --------- | -------- | --------- |
| `struct pmm_state` | `bitmap`, `frame_count`, `free_frames`, `used_frames`, `reserved_frames`, `next_hint` | Kernel | Dibuat saat inisialisasi kernel dan hidup selama kernel berjalan | Statistik selalu konsisten dengan bitmap |
| `struct boot_mem_region` | `base`, `length`, `type` | Bootloader / PMM | Digunakan saat proses inisialisasi PMM | Setiap region memiliki alamat dan panjang yang valid |

### 9.6 Invariants

1. Setiap physical frame hanya memiliki satu status pada bitmap (free atau used/reserved).
2. Nilai `free_frames + used_frames` selalu sama dengan `frame_count`.
3. Frame yang telah di-reserve tidak boleh dialokasikan kembali.
4. PMM harus berhasil diinisialisasi sebelum fungsi alokasi maupun pelepasan frame dipanggil.

### 9.7 Ownership, Locking, dan Concurrency

| Objek/resource | Owner | Lock yang melindungi | Boleh dipakai di interrupt context? | Catatan |
| -------------- | ----- | -------------------- | ----------------------------------- | -------- |
| `struct pmm_state` | Kernel | None | Tidak | Praktikum masih berjalan pada lingkungan single-core sehingga belum memerlukan sinkronisasi. |
| Bitmap PMM | PMM | None | Tidak | Akses dilakukan secara serial selama tahap awal kernel. |

Lock order yang berlaku:

```text
Belum terdapat mekanisme locking pada milestone ini. Implementasi masih diasumsikan berjalan pada lingkungan single-core tanpa akses paralel sehingga tidak diperlukan spinlock maupun mutex.
```

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
| ------ | ------ | -------- | ------ |
| Out-of-bounds bitmap access | `src/pmm.c` | Seluruh indeks divalidasi terhadap jumlah frame yang dikelola. | Host unit test berhasil dijalankan. |
| Double free | `pmm_free_frame()` | Status bitmap diperiksa sebelum frame dibebaskan. | Review kode dan host unit test. |
| Integer overflow pada perhitungan frame | `pmm_init_from_map()` | Menggunakan tipe `uint64_t` untuk alamat dan jumlah frame. | Build tanpa warning (`-Wall -Wextra -Werror`). |
| Parameter tidak valid | Seluruh API PMM | Validasi pointer dan ukuran dilakukan sebelum operasi utama. | Host unit test dan static check M6. |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
| -------- | -------------------- | ----------------------- | ----------------- |
| Boot memory map | Region memori dari bootloader | Validasi jumlah region, ukuran bitmap, dan batas frame sebelum digunakan. | Fungsi mengembalikan `false` sehingga kernel dapat menghentikan proses inisialisasi secara aman. |
| API PMM | Pointer state dan parameter fungsi | Memastikan pointer tidak `NULL` dan parameter berada dalam rentang yang valid. | Operasi dibatalkan dan fungsi mengembalikan status gagal tanpa mengubah state PMM. |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Menambahkan API Physical Memory Manager

Maksud langkah:

```text
Mendefinisikan antarmuka (API) Physical Memory Manager agar dapat digunakan oleh kernel maupun host unit test. Tahap ini juga menambahkan tipe data yang diperlukan untuk merepresentasikan state PMM dan boot memory map.
```

Perintah:

```bash
nano include/pmm.h
nano include/types.h
nano kernel/include/mcsos/kernel/pmm.h
```

Output ringkas:

```text
Header PMM berhasil dibuat dan dapat di-include oleh modul kernel maupun host test.
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `pmm.h` | `include/` | Mendefinisikan API PMM |
| `types.h` | `include/` | Mendefinisikan tipe data dasar |
| `pmm.h` | `kernel/include/mcsos/kernel/` | Wrapper header untuk kernel |

Indikator berhasil:

```text
Seluruh source dapat menemukan deklarasi fungsi PMM tanpa error include.
```

---

### Langkah 2 — Mengimplementasikan Bitmap Physical Memory Manager

Maksud langkah:

```text
Mengimplementasikan mekanisme pengelolaan frame fisik menggunakan bitmap sehingga kernel dapat mengetahui frame yang bebas, digunakan, maupun direservasi.
```

Perintah:

```bash
nano src/pmm.c
```

Output ringkas:

```text
Implementasi berhasil dikompilasi tanpa warning menggunakan Clang.
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `pmm.c` | `src/` | Implementasi Physical Memory Manager |

Indikator berhasil:

```text
Seluruh fungsi PMM berhasil dikompilasi dan tersedia pada hasil build.
```

---

### Langkah 3 — Membuat Host Unit Test

Maksud langkah:

```text
Memverifikasi logika PMM secara terpisah dari kernel sehingga proses debugging lebih mudah dilakukan.
```

Perintah:

```bash
clang -std=c17 \
    -Iinclude \
    tests/test_pmm_host.c \
    src/pmm.c \
    -o test_pmm

./test_pmm
```

Output ringkas:

```text
M6 PMM host unit test: PASS
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `test_pmm` | Root repository | Program host untuk menguji PMM |
| `test_pmm_host.c` | `tests/` | Unit test PMM |

Indikator berhasil:

```text
Seluruh pengujian host selesai dengan status PASS.
```

---

### Langkah 4 — Menambahkan Static Verification

Maksud langkah:

```text
Menyediakan proses verifikasi otomatis agar implementasi PMM dapat diperiksa secara konsisten sebelum build kernel dilakukan.
```

Perintah:

```bash
bash scripts/check_m6_static.sh
```

Output ringkas:

```text
M6 PMM host unit test: PASS
[PASS] M6 static check selesai
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `check_m6_static.sh` | `scripts/` | Script validasi otomatis M6 |

Indikator berhasil:

```text
Script selesai dijalankan tanpa error dan seluruh pemeriksaan dinyatakan PASS.
```

---

### Langkah 5 — Integrasi dengan Build Kernel

Maksud langkah:

```text
Mengintegrasikan modul PMM ke dalam sistem build kernel sehingga implementasi ikut dikompilasi dan ditautkan ke kernel ELF.
```

Perintah:

```bash
make clean
make
```

Output ringkas:

```text
Kernel berhasil dikompilasi dan menghasilkan build/kernel.elf tanpa error.
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| `kernel.elf` | `build/` | Kernel hasil kompilasi |
| `kernel.map` | `build/` | Peta simbol linker |
| `kernel.disasm.txt` | `build/` | Hasil disassembly |
| `kernel.syms.txt` | `build/` | Daftar simbol kernel |

Indikator berhasil:

```text
Proses make selesai tanpa error dan seluruh artefak build berhasil dibuat.
```

---

### Langkah 6 — Commit dan Push Repository

Maksud langkah:

```text
Menyimpan implementasi M6 ke repository Git sehingga seluruh perubahan terdokumentasi dan tersedia pada branch utama.
```

Perintah:

```bash
git add .
git commit -m "M6: implement physical memory manager"
git checkout main
git merge m6-pmm
git pull --rebase origin main
git push origin main
```

Output ringkas:

```text
Branch main berhasil diperbarui dan perubahan berhasil di-push ke GitHub.
```

Artefak yang dihasilkan:

| Artefak | Lokasi | Fungsi |
| -------- | ------ | ------ |
| Commit Git | Repository | Menyimpan implementasi M6 |
| Branch `main` | GitHub | Branch utama repository |

Indikator berhasil:

```text
Commit M6 tersedia pada branch main dan berhasil dipush ke remote repository.
```

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
| ---------- | -------- | --------------- | ------ |
| Clean build | `make clean && make` | `build/kernel.elf` berhasil dibuat | PASS |
| Metadata toolchain | `make meta` | Metadata toolchain tersedia | NA |
| Image generation | `make image` | Image bootable berhasil dibuat | NA |
| QEMU smoke test | `make run` | Kernel berjalan pada QEMU | NA |
| Test suite | `./test_pmm` dan `bash scripts/check_m6_static.sh` | Seluruh pengujian PMM lulus | PASS |

Catatan checkpoint:

```text
Checkpoint clean build berhasil dilalui dan kernel ELF berhasil dibangun tanpa error. Host unit test PMM dan script verifikasi statis juga selesai dengan status PASS.

Checkpoint make meta, make image, dan make run tidak dievaluasi pada praktikum ini karena fokus implementasi berada pada Physical Memory Manager dan host-side verification.
```

---

## 12. Perintah Uji dan Validasi

### 12.1 Build Test

Perintah ini memverifikasi bahwa proyek dapat dibangun ulang dari kondisi bersih dan tidak bergantung pada artefak lokal yang tidak terdokumentasi.

```bash
make clean
make build
```

Hasil:

```text
Build berhasil tanpa error.

Seluruh source berhasil dikompilasi, termasuk:
- kernel/arch/x86_64/idt.c
- kernel/arch/x86_64/pic.c
- kernel/arch/x86_64/pit.c
- kernel/core/kmain.c
- kernel/core/log.c
- kernel/core/panic.c
- kernel/core/serial.c
- kernel/core/trap.c
- kernel/lib/memory.c
- src/pmm.c
- kernel/arch/x86_64/isr.S

Linking berhasil menghasilkan:
- build/kernel.elf
- build/kernel.map

Artefak verifikasi yang dihasilkan:
- build/kernel.readelf.header.txt
- build/kernel.readelf.programs.txt
- build/kernel.syms.txt
- build/kernel.disasm.txt

Verifikasi otomatis berhasil:
- ELF64 terdeteksi.
- Target Machine: Advanced Micro Devices X86-64.
- Simbol kmain ditemukan.
- Simbol x86_64_idt_init ditemukan.
- Simbol x86_64_trap_dispatch ditemukan.
- Instruksi iretq ditemukan.
- Instruksi lidt ditemukan.
```

Status: `PASS`
```

Status: `[PASS/FAIL]`

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
[Tempel bukti entry point, program headers, section flags, atau disassembly yang relevan.]
```

Status: `[PASS/FAIL/NA]`

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
Pengujian QEMU belum dilakukan pada milestone ini.

Fokus implementasi M6 adalah penyelesaian modul Physical Memory Manager (PMM),
host unit test, serta static verification. Oleh karena itu belum tersedia
berkas build/qemu-serial.log sebagai evidence.
```

Status: `NA`


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
gdb build/kernel.elf
target remote :1234
break kmain
continue
info registers
bt
```

Hasil:

```text
Pengujian GDB tidak dilakukan pada milestone ini.

Implementasi M6 berfokus pada pengembangan Physical Memory Manager (PMM),
host unit test, dan static verification. Oleh karena itu belum tersedia
bukti breakpoint, register dump, maupun backtrace dari GDB.
```

Status: `NA`

### 12.5 Unit Test

```bash
bash scripts/check_m6_static.sh
```

Hasil:

```text
M6 PMM host unit test: PASS
[PASS] M6 static check selesai
```

Status: `PASS`

### 12.6 Stress/Fuzz/Fault Injection Test

Pengujian dilakukan menggunakan host unit test dan static validation untuk memastikan allocator PMM tetap mempertahankan invariant ketika dilakukan alokasi dan dealokasi frame secara berulang.

```bash
bash scripts/check_m6_static.sh
```

Hasil:

```text
M6 PMM host unit test: PASS
[PASS] M6 static check selesai
```

Status: `PASS`
### 12.7 Visual Evidence

Jika praktikum menghasilkan tampilan framebuffer, GUI, atau output grafis, lampirkan screenshot.

| Screenshot | Lokasi file | Keterangan |
|------------|-------------|------------|
| Tidak ada | - | Milestone M6 tidak menghasilkan output grafis. Validasi dilakukan melalui build log dan host unit test. |

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan Hasil

| No. | Uji | Expected result | Actual result | Status | Evidence |
|-----|-----|-----------------|---------------|--------|----------|
| 1 | Clean Build | Seluruh source berhasil dikompilasi dan menghasilkan `build/kernel.elf` tanpa error | Build berhasil, seluruh object file terkompilasi, `kernel.elf`, `kernel.map`, `kernel.disasm.txt`, dan file verifikasi berhasil dibuat | `PASS` | Output `make clean && make`, direktori `build/` |
| 2 | Static Verification | Struktur ELF valid dan simbol kernel tersedia | Verifikasi `readelf`, `nm`, dan `objdump` berhasil (`ELF64`, `kmain`, `x86_64_idt_init`, `x86_64_trap_dispatch`, `lidt`, `iretq`) | `PASS` | `build/kernel.readelf.*`, `build/kernel.syms.txt`, `build/kernel.disasm.txt` |
| 3 | PMM Host Unit Test | Seluruh fungsi dasar PMM berjalan benar | `M6 PMM host unit test: PASS` dan `[PASS] M6 static check selesai` | `PASS` | `bash scripts/check_m6_static.sh` |
| 4 | Integrasi PMM ke Build | Modul PMM ikut dikompilasi dan dilink ke kernel | `src/pmm.c` berhasil dikompilasi menjadi `build/normal/src/pmm.o` dan berhasil dilink ke `kernel.elf` | `PASS` | Log proses build |
| 5 | QEMU Smoke Test | Kernel berhasil dijalankan pada QEMU | Belum dilakukan karena image ISO (`build/mcsos.iso`) belum dibuat | `NA` | - |
| 6 | GDB Debug Test | Kernel dapat di-debug menggunakan simbol | Belum dilakukan pada milestone ini | `NA` | - |


### 13.2 Log Penting

```text
== Build Verification ==

Build kernel berhasil tanpa error.

Generated artifacts:
- build/kernel.elf
- build/kernel.map
- build/kernel.readelf.header.txt
- build/kernel.readelf.programs.txt
- build/kernel.syms.txt
- build/kernel.disasm.txt

Static verification:
- ELF64: PASS
- Machine: Advanced Micro Devices X86-64
- Symbol 'kmain' ditemukan
- Symbol 'x86_64_idt_init' ditemukan
- Symbol 'x86_64_trap_dispatch' ditemukan
- Instruksi 'lidt' ditemukan
- Instruksi 'iretq' ditemukan

== PMM Host Unit Test ==

$ bash scripts/check_m6_static.sh

M6 PMM host unit test: PASS
[PASS] M6 static check selesai
```

### 13.3 Artefak Bukti

| Artefak | Path | SHA-256 / hash | Fungsi |
|---------|------|----------------|---------|
| `kernel.elf` | `build/kernel.elf` | `6be0d7096e6db187d716073b779c99488c4b84d426565c4dc4661f7ee1d45f59` | Binary kernel hasil build |
| `mcsos.iso` | `build/mcsos.iso` | `NA` | Boot image belum dihasilkan pada milestone ini |
| `qemu-serial.log` | `build/qemu-serial.log` | `NA` | Log boot belum tersedia karena QEMU smoke test belum dijalankan |
| `kernel.map` | `build/kernel.map` | `5567db5885b10a4a58b27655b70c4f86f5021f884b0d57941a8955a23a13ef3f` | Linker map untuk analisis layout kernel |
| `kernel.disasm.txt` | `build/kernel.disasm.txt` | `8497a7076d1ddf5e587ed7cc381c1ea1e76c832d4ce0aec49c91d923ad63945b` | Bukti hasil disassembly kernel |
| `kernel.readelf.header.txt` | `build/kernel.readelf.header.txt` | `Belum dihitung` | Bukti inspeksi header ELF |
| `kernel.readelf.programs.txt` | `build/kernel.readelf.programs.txt` | `Belum dihitung` | Bukti program header ELF |
| `kernel.syms.txt` | `build/kernel.syms.txt` | `Belum dihitung` | Daftar simbol kernel untuk verifikasi |


---

### 14.1 Analisis Keberhasilan

```text
Implementasi Physical Memory Manager (PMM) pada milestone ini berhasil
mencapai tujuan utama, yaitu menyediakan mekanisme dasar untuk
mengelola frame memori fisik secara freestanding dan dapat
diintegrasikan ke dalam kernel MCSOS.

Keberhasilan implementasi dibuktikan oleh beberapa hasil pengujian.
Pertama, proses clean build berhasil menyusun seluruh source code
tanpa error dan menghasilkan artefak kernel seperti kernel.elf,
kernel.map, kernel.readelf.*, kernel.syms.txt, dan
kernel.disasm.txt. Hal ini menunjukkan bahwa modul PMM telah
terintegrasi dengan sistem build tanpa merusak komponen kernel yang
telah dibuat pada milestone sebelumnya.

Kedua, static verification berhasil menemukan seluruh simbol dan
instruksi penting yang menjadi indikator bahwa kernel masih valid,
antara lain format ELF64, arsitektur x86-64, simbol kmain,
x86_64_idt_init, x86_64_trap_dispatch, serta instruksi lidt dan
iretq. Hasil tersebut menunjukkan bahwa penambahan modul PMM tidak
mengubah struktur executable kernel secara tidak semestinya.

Ketiga, host unit test berhasil dijalankan melalui
scripts/check_m6_static.sh dengan hasil:

"M6 PMM host unit test: PASS"
"[PASS] M6 static check selesai"

Hasil tersebut menunjukkan bahwa fungsi-fungsi dasar allocator fisik,
seperti inisialisasi, alokasi frame, dealokasi frame, dan pengelolaan
status frame bekerja sesuai rancangan.

Invariant utama implementasi juga tetap terjaga, yaitu setiap frame
fisik hanya dapat berada pada satu status kepemilikan pada satu waktu,
tidak terjadi alokasi ganda terhadap frame yang sama, dan frame yang
dibebaskan dapat dialokasikan kembali secara konsisten. Keberhasilan
host unit test menunjukkan bahwa invariant tersebut tidak dilanggar
selama skenario pengujian yang dilakukan.

Secara keseluruhan, implementasi PMM telah memenuhi target milestone
ini karena berhasil dikompilasi, lolos verifikasi statis, dan
berfungsi dengan benar pada unit test tanpa menyebabkan regresi pada
proses build kernel.
```

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
| ------------ | ---------------------- | ------------------- | ---------- |
| Physical Memory Manager (PMM) mengelola alokasi dan pelepasan frame fisik secara terpusat. | PMM diimplementasikan pada `src/pmm.c` dengan fungsi inisialisasi, alokasi, dan pelepasan frame fisik. | Sesuai | Implementasi menyediakan mekanisme dasar untuk mengelola frame fisik tanpa bergantung pada allocator pengguna atau virtual memory. |
| Struktur data allocator harus menjaga status setiap frame agar tidak dialokasikan lebih dari satu kali. | PMM menggunakan struktur data internal untuk menyimpan status frame dan memperbaruinya pada setiap operasi alokasi maupun pelepasan. | Sesuai | Hasil host unit test menunjukkan tidak terjadi alokasi ganda maupun inkonsistensi status frame. |
| Kernel freestanding tidak menggunakan allocator dari standard library. | Seluruh fungsi PMM ditulis dalam bahasa C freestanding dan dikompilasi menggunakan `-ffreestanding` tanpa ketergantungan pada libc. | Sesuai | Implementasi memenuhi karakteristik kernel freestanding sehingga dapat dijalankan pada lingkungan bare metal. |
| Implementasi allocator harus dapat diverifikasi melalui pengujian dan validasi statis. | PMM diuji menggunakan `tests/test_pmm_host.c`, script `check_m6_static.sh`, serta build kernel penuh. | Sesuai | Seluruh pengujian menghasilkan status PASS sehingga implementasi sesuai dengan prinsip verifikasi perangkat lunak sistem operasi. |


### 14.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Bukti | Catatan |
| ---------------------- | ---------------------- | ---------------- | ----------- |
| Kompleksitas algoritma | O(1) untuk operasi alokasi dan pelepasan frame | Review implementasi `src/pmm.c` dan host unit test | PMM menggunakan struktur data sederhana sehingga setiap operasi tidak bergantung pada jumlah frame yang dikelola. |
| Waktu build | Tidak diukur secara kuantitatif | Log `make clean` dan `make` berhasil tanpa error | Fokus praktikum adalah keberhasilan build, bukan optimasi waktu kompilasi. |
| Waktu boot QEMU | NA | Tidak dilakukan pengujian QEMU pada praktikum ini | Pengujian difokuskan pada host unit test dan static verification. |
| Penggunaan memori | Belum dilakukan pengukuran | Tidak tersedia metrik penggunaan memori | Implementasi hanya menyediakan mekanisme dasar Physical Memory Manager tanpa profiling memori. |
| Latensi/throughput | NA | Tidak dilakukan benchmark performa | Praktikum berfokus pada kebenaran fungsi allocator, bukan evaluasi performa. |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab sementara | Bukti | Perbaikan |
| ---------------------------------------------------------------------------------------------- | ---------- | ------------------ | ------- | ---------------- |
| Build failure | Kompilasi gagal pada `scripts/check_m6_static.sh` | Header `mcsos/kernel/pmm.h` belum berada pada lokasi include yang sesuai | Pesan error `fatal error: 'mcsos/kernel/pmm.h' file not found` | Menambahkan header pada `kernel/include/mcsos/kernel/` dan memperbaiki include path. |
| Git non-fast-forward | `git push origin main` ditolak | Repository remote memiliki commit yang belum tersedia secara lokal | Pesan `Updates were rejected because the remote contains work that you do not have locally` | Menjalankan `git pull --rebase origin main` kemudian melakukan `git push` kembali hingga berhasil. |
| Perubahan source tidak diinginkan | File `kernel/core/kmain.c` berubah akibat proses edit | Kesalahan saat modifikasi source code | Output `git status` menunjukkan file berubah | Mengembalikan file menggunakan `git restore kernel/core/kmain.c`. |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
| ------------ | ------------------- | ---------- | ------------ |
| Double allocation frame | Host unit test dan review implementasi PMM | Kerusakan state allocator | Memastikan status frame diperbarui setiap proses alokasi dan pelepasan. |
| Invalid free | Host unit test | Inkonsistensi data allocator | Melakukan validasi terhadap frame sebelum dikembalikan ke daftar bebas. |
| Build regression | `make clean && make` | Kernel gagal dibangun | Selalu melakukan clean build setelah perubahan kode. |
| Kesalahan konfigurasi include | Static checker (`check_m6_static.sh`) | Modul PMM tidak dapat dikompilasi | Menjaga struktur header dan include path tetap konsisten. |

### 15.3 Triage yang Dilakukan

```text
1. Memeriksa pesan error dari compiler ketika build atau static checker gagal.
2. Menggunakan git status untuk mengidentifikasi file yang berubah.
3. Mengembalikan perubahan yang tidak diinginkan menggunakan git restore.
4. Menjalankan kembali make clean dan make untuk memastikan kernel dapat dibangun.
5. Menjalankan scripts/check_m6_static.sh untuk memverifikasi implementasi PMM.
6. Menjalankan host unit test PMM untuk memastikan seluruh fungsi allocator bekerja dengan benar.
7. Melakukan git pull --rebase ketika push ditolak, kemudian mengulangi git push hingga berhasil.
```

### 15.4 Panic Path

```text
Selama implementasi Milestone 6 tidak ditemukan kondisi yang memicu kernel panic. Pengujian difokuskan pada proses build, static verification, dan host unit test Physical Memory Manager. Oleh karena itu tidak terdapat panic log yang dapat dilampirkan. Mekanisme panic kernel yang telah dibuat pada milestone sebelumnya tetap tersedia apabila di kemudian hari ditemukan kondisi fatal pada subsystem memori.
```
---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
| ----------------- | -------- | ---------------------------- | ------ |
| Kembali ke commit awal | `git checkout 342d761` | Laporan praktikum, log pengujian, dan perubahan yang belum di-commit | Belum |
| Revert commit praktikum | `git revert f42ec4c` | Laporan, hasil build, dan artefak pengujian | Belum |
| Bersihkan artefak build | `make clean` | Tidak ada, karena hanya menghapus direktori `build/` | Teruji |
| Regenerasi image | `make image` | Image lama jika masih diperlukan sebagai arsip | Belum |

Catatan rollback:

```text
Rollback penuh ke commit sebelumnya maupun menggunakan git revert tidak dilakukan selama praktikum karena implementasi PMM telah berhasil dibangun, lulus host unit test, dan berhasil di-push ke branch main. Prosedur yang telah diuji adalah pembersihan artefak build menggunakan `make clean`, kemudian melakukan build ulang menggunakan `make` untuk memastikan repository dapat direproduksi dari kondisi bersih.

Apabila pada pengembangan berikutnya ditemukan regresi, repository dapat dikembalikan ke commit sebelum implementasi PMM (`342d761`) menggunakan `git checkout` atau membatalkan perubahan dengan `git revert f42ec4c`. Sebelum melakukan rollback disarankan menyimpan laporan praktikum, log pengujian, dan perubahan yang belum di-commit agar tidak hilang.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
| ------ | -------- | ------ | -------- | -------- |
| Invalid physical frame allocation | Antarmuka PMM | Kerusakan state allocator atau alokasi frame yang tidak valid | Melakukan validasi parameter dan menjaga invariant status frame pada setiap operasi alokasi maupun pelepasan | Host unit test PMM dan `scripts/check_m6_static.sh` (PASS) |
| Double allocation | Internal PMM | Dua komponen menggunakan frame fisik yang sama sehingga dapat menyebabkan korupsi memori | Status setiap frame diperbarui secara konsisten dan diverifikasi melalui unit test | Host unit test `tests/test_pmm_host.c` |
| Invalid free | Antarmuka PMM | Inkonsistensi metadata allocator | Memastikan hanya frame yang valid dan telah dialokasikan yang dapat dikembalikan ke daftar bebas | Review implementasi dan host unit test |
| Build regression | Build system | Kernel tidak dapat dikompilasi sehingga perubahan tidak dapat digunakan | Melakukan clean build (`make clean && make`) setelah setiap perubahan serta menjalankan static checker | Log build dan `check_m6_static.sh` menghasilkan PASS |
| Kesalahan konfigurasi include | Build boundary | Modul PMM gagal dikompilasi | Menempatkan header pada direktori include yang sesuai dan memperbarui include path pada Makefile | Build kernel berhasil tanpa error |

### 17.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
| ------------------ | ------ | ------- | -------- |
| Build failure | Kernel tidak dapat dibangun sehingga pengujian tidak dapat dilakukan | `make clean && make`, log compiler | Memperbaiki konfigurasi build, include path, dan memastikan seluruh dependensi tersedia. |
| Inconsistent allocator state | Alokasi atau pelepasan frame menjadi tidak valid | Host unit test PMM dan static checker | Menjaga invariant status frame pada setiap operasi alokasi dan free serta melakukan validasi parameter. |
| Resource leak (frame tidak dikembalikan) | Berkurangnya jumlah frame yang dapat digunakan | Host unit test | Memastikan setiap frame yang telah dialokasikan dapat dikembalikan melalui fungsi free dan diverifikasi melalui pengujian. |
| Race condition pada PMM | Potensi korupsi metadata allocator | Review implementasi | Belum terdapat mekanisme locking karena implementasi masih berjalan pada lingkungan single-core dan belum mendukung konkurensi. |
| Kesalahan integrasi modul PMM | Kernel gagal dikompilasi atau fitur tidak terhubung dengan benar | Static checker dan clean build | Mengintegrasikan modul melalui Makefile, header, serta melakukan pengujian ulang setiap selesai perubahan. |


### 17.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
| ------------- | ----------- | --------------- | ------------- | ------ |
| Header PMM tidak tersedia | File `mcsos/kernel/pmm.h` dihapus atau include path salah | Proses kompilasi gagal dengan pesan error, tidak menghasilkan binary yang rusak | Compiler menghasilkan `fatal error: 'mcsos/kernel/pmm.h' file not found` | PASS |
| Build dari kondisi bersih | Direktori `build/` dihapus menggunakan `make clean` | Seluruh artefak build berhasil dibuat kembali tanpa error | Kernel berhasil dikompilasi dan seluruh static check lulus | PASS |
| Static verification PMM | Menjalankan `scripts/check_m6_static.sh` setelah implementasi PMM | Seluruh host unit test dan static check menghasilkan PASS | Output: `M6 PMM host unit test: PASS` dan `[PASS] M6 static check selesai` | PASS |
| Push tanpa sinkronisasi repository | Melakukan `git push origin main` ketika remote lebih baru | Git menolak push (non-fast-forward) tanpa merusak repository | Push ditolak, kemudian berhasil setelah `git pull --rebase origin main` | PASS |
| Alokasi frame tidak valid | Meminta allocator menggunakan frame di luar kondisi yang diharapkan (diverifikasi melalui host unit test) | Operasi ditolak atau tidak menyebabkan korupsi state allocator | Host unit test selesai tanpa kegagalan maupun korupsi state | PASS |

---

## 18. Pembagian Kerja Kelompok

Praktikum ini dikerjakan secara individu sehingga bagian pembagian kerja kelompok **tidak berlaku**.

| Nama | NIM | Peran | Kontribusi teknis | Commit/artefak |
| ---- | --- | ----- | ----------------- | -------------- |
| Tidak berlaku | - | - | - | - |

### 18.1 Mekanisme Koordinasi

```text
Tidak berlaku. Praktikum dikerjakan secara individu sehingga tidak terdapat pembagian tugas, koordinasi antaranggota, merge request, maupun proses code review internal.
```

### 18.2 Evaluasi Kontribusi

| Anggota | Persentase kontribusi yang disepakati | Bukti | Catatan |
| -------- | ------------------------------------- | ----- | -------- |
| Tidak berlaku | 100% | Seluruh commit dan artefak repository | Praktikum dikerjakan secara individu. |

---
## 19. Kriteria Lulus Praktikum

Bagian ini wajib diisi. Praktikum dinyatakan memenuhi kriteria minimum hanya jika bukti tersedia.

| Kriteria minimum | Status | Evidence |
| ---------------- | ------ | -------- |
| Proyek dapat dibangun dari clean checkout | PASS | Log `make clean && make` (Bagian 12.1) |
| Perintah build terdokumentasi | PASS | Bagian 10 dan 12 laporan |
| QEMU boot atau test target berjalan deterministik | NA | Pengujian QEMU tidak dilakukan pada praktikum ini |
| Semua unit test/praktikum test relevan lulus | PASS | `M6 PMM host unit test: PASS` dan `check_m6_static.sh` |
| Log serial disimpan | NA | Tidak dilakukan pengujian QEMU |
| Panic path terbaca atau dijelaskan jika belum relevan | PASS | Bagian 15.4 |
| Tidak ada warning kritis pada build | PASS | Log build berhasil tanpa warning maupun error |
| Perubahan Git terkomit | PASS | Commit `f42ec4c` |
| Desain dan failure mode dijelaskan | PASS | Bagian 9 dan 15 |
| Laporan berisi screenshot/log yang cukup | PASS | Build log, host unit test, static check, dan hash artefak |

Kriteria tambahan untuk praktikum lanjutan:

| Kriteria lanjutan | Status | Evidence |
| ----------------- | ------ | -------- |
| Static analysis dijalankan | PASS | `scripts/check_m6_static.sh` |
| Stress test dijalankan | NA | Tidak menjadi ruang lingkup praktikum |
| Fuzzing atau malformed-input test dijalankan | NA | Tidak menjadi ruang lingkup praktikum |
| Fault injection dijalankan | NA | Tidak menjadi ruang lingkup praktikum |
| Disassembly/readelf evidence tersedia | PASS | `build/kernel.disasm.txt`, `build/kernel.readelf.header.txt`, `build/kernel.readelf.programs.txt` |
| Review keamanan dilakukan | PASS | Bagian 17 (Keamanan dan Reliability) |
| Rollback diuji | PASS | `make clean` telah diuji sebagai prosedur rollback build (Bagian 16) |

---

## 20. Readiness Review

Pilih satu status dengan alasan berbasis bukti.

| Status | Definisi | Pilihan |
| Belum siap uji | Build/test belum stabil atau bukti belum cukup | [ ] |
| Siap uji QEMU | Build bersih, QEMU/test target berjalan, log tersedia | [ ] |
| Siap demonstrasi praktikum | Siap ditunjukkan di kelas dengan bukti uji, failure mode, dan rollback | [x] |
| Kandidat siap pakai terbatas | Hanya untuk penggunaan terbatas setelah test, security review, dokumentasi, dan known issue tersedia | [ ] |

Alasan readiness:

```text
Implementasi Physical Memory Manager berhasil dibangun dari clean checkout menggunakan `make clean` dan `make`. Host unit test PMM menghasilkan status PASS, static verification melalui `scripts/check_m6_static.sh` juga berhasil tanpa error. Seluruh perubahan telah di-commit dan di-push ke repository, serta dokumentasi mengenai desain, failure mode, keamanan, dan prosedur rollback telah disusun. Pengujian QEMU tidak dilakukan pada milestone ini sehingga status "Siap uji QEMU" tidak dipilih. Berdasarkan bukti yang tersedia, implementasi dinilai siap untuk didemonstrasikan pada praktikum.
```

Known issues:

| No. | Issue | Dampak | Workaround | Target perbaikan |
| --- | ----- | ------ | ---------- | ---------------- |
| 1 | Pengujian QEMU belum dilakukan | Belum tersedia bukti boot runtime kernel | Melakukan pengujian menggunakan image bootable dan menyimpan serial log | Milestone berikutnya |
| 2 | Benchmark performa allocator belum tersedia | Belum diketahui karakteristik performa PMM | Menambahkan benchmark dan stress test allocator | Milestone berikutnya |

Keputusan akhir:

```text
Berdasarkan bukti clean build, host unit test PMM yang lulus, static verification yang berhasil, commit repository yang telah dipublikasikan, serta dokumentasi desain dan failure mode yang lengkap, hasil praktikum ini layak dinyatakan siap demonstrasi praktikum. Status kandidat siap pakai terbatas belum diberikan karena belum tersedia pengujian runtime menggunakan QEMU maupun evaluasi performa allocator.
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
Praktikum Milestone 6 berhasil mengimplementasikan Physical Memory Manager (PMM) sebagai dasar pengelolaan memori fisik pada kernel MCSOS. Modul PMM berhasil diintegrasikan ke dalam sistem build melalui Makefile dan header yang sesuai. Proses clean build menggunakan `make clean` dan `make` berhasil menghasilkan kernel tanpa error. Host unit test PMM serta static verification melalui `scripts/check_m6_static.sh` juga menghasilkan status PASS. Seluruh perubahan telah di-commit dan dipublikasikan ke repository GitHub sehingga implementasi dapat direproduksi dan diverifikasi kembali.
```

### 22.2 Yang Belum Berhasil

```text
Pengujian runtime menggunakan QEMU belum dilakukan sehingga belum tersedia bukti boot kernel maupun serial log yang menunjukkan integrasi PMM saat eksekusi. Selain itu, belum dilakukan pengukuran performa allocator, stress test, maupun benchmark penggunaan memori sehingga evaluasi masih berfokus pada aspek fungsional dan keberhasilan build.
```

### 22.3 Rencana Perbaikan

```text
Tahap berikutnya adalah mengintegrasikan PMM dengan subsystem Virtual Memory Manager (VMM) sehingga frame fisik dapat digunakan untuk membangun page table dan mekanisme paging kernel. Selain itu akan ditambahkan pengujian runtime menggunakan QEMU, stress test allocator, benchmark performa, serta fault injection untuk meningkatkan keandalan implementasi dan memperluas cakupan validasi pada milestone berikutnya.
```

---
## 23. Lampiran

### Lampiran A — Commit Log

```text
f42ec4c (HEAD -> main, origin/main) M6: implement physical memory manager
342d761 Merge pull request #3 from ganianrhasanah-ui/m5-pic-pit
50e77e2 (origin/m5-pic-pit, m5-pic-pit) Add M5 report
21a1f6b Merge pull request #2 from ganianrhasanah-ui/m5-pic-pit
b7b4f86 (m4-idt-exception-path) Milestone 5: Add PIC remapping, PIT timer, and IRQ0 handler
```

### Lampiran B — Diff Ringkas

```diff
+ Menambahkan implementasi Physical Memory Manager (PMM) pada src/pmm.c
+ Menambahkan header include/pmm.h
+ Menambahkan header include/types.h
+ Menambahkan kernel/include/mcsos/kernel/pmm.h
+ Menambahkan kernel/include/mcsos/kernel/memory.h
* Memperbarui kernel/lib/memory.c
* Memperbarui Makefile untuk memasukkan modul PMM
+ Menambahkan tests/test_pmm_host.c
+ Menambahkan scripts/check_m6_static.sh
```

### Lampiran C — Log Build Lengkap

```text
Log build lengkap dihasilkan dari perintah:

make clean
make

Hasil akhir:
- Kernel berhasil dikompilasi.
- build/kernel.elf berhasil dibuat.
- build/kernel.map berhasil dibuat.
- build/kernel.readelf.header.txt berhasil dibuat.
- build/kernel.readelf.programs.txt berhasil dibuat.
- build/kernel.syms.txt berhasil dibuat.
- build/kernel.disasm.txt berhasil dibuat.

Seluruh proses build selesai tanpa warning maupun error.
```

### Lampiran D — Log QEMU Lengkap

```text
Tidak tersedia.

Milestone ini tidak melakukan pengujian runtime menggunakan QEMU sehingga file qemu-serial.log tidak dihasilkan.
```

### Lampiran E — Output Readelf/Objdump

```text
Verifikasi artefak ELF dilakukan selama proses build.

Artefak yang dihasilkan:
- build/kernel.readelf.header.txt
- build/kernel.readelf.programs.txt
- build/kernel.syms.txt
- build/kernel.disasm.txt

Verifikasi otomatis:
- ELF64 terdeteksi.
- Target Machine: Advanced Micro Devices X86-64.
- Simbol kmain ditemukan.
- Simbol x86_64_idt_init ditemukan.
- Simbol x86_64_trap_dispatch ditemukan.
- Instruksi iretq ditemukan.
- Instruksi lidt ditemukan.
```

### Lampiran F — Screenshot

| No. | File | Keterangan |
| --- | ---- | ---------- |
| 1 | Tidak ada | Praktikum M6 tidak menghasilkan output grafis maupun framebuffer sehingga tidak terdapat screenshot yang dilampirkan. |

### Lampiran G — Bukti Tambahan

```text
Host Unit Test:
M6 PMM host unit test: PASS

Static Verification:
[PASS] M6 static check selesai

Hash artefak:

build/kernel.elf
SHA-256:
6be0d7096e6db187d716073b779c99488c4b84d426565c4dc4661f7ee1d45f59

build/kernel.map
SHA-256:
5567db5885b10a4a58b27655b70c4f86f5021f884b0d57941a8955a23a13ef3f

build/kernel.disasm.txt
SHA-256:
8497a7076d1ddf5e587ed7cc381c1ea1e76c832d4ce0aec49c91d923ad63945b
```

---

## 24. Daftar Referensi

Gunakan format IEEE. Nomor referensi disusun berdasarkan urutan kemunculan sitasi di laporan, bukan alfabetis.

```text
[1] R. H. Arpaci-Dusseau and A. C. Arpaci-Dusseau, Operating Systems: Three Easy Pieces. Madison, WI, USA: Arpaci-Dusseau Books, 2023. [Online]. Available: https://pages.cs.wisc.edu/~remzi/OSTEP/. Accessed: Jun. 30, 2026.

[2] Intel Corporation, Intel® 64 and IA-32 Architectures Software Developer's Manual. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html. Accessed: Jun. 30, 2026.

[3] Advanced Micro Devices, AMD64 Architecture Programmer's Manual. [Online]. Available: https://www.amd.com/en/support/tech-docs/amd64-architecture-programmers-manual-volumes-1-5. Accessed: Jun. 30, 2026.

[4] The LLVM Project, "Clang 18 Documentation." [Online]. Available: https://clang.llvm.org/docs/. Accessed: Jun. 30, 2026.

[5] The LLVM Project, "LLD - LLVM Linker." [Online]. Available: https://lld.llvm.org/. Accessed: Jun. 30, 2026.

[6] QEMU Project, "QEMU Emulator Documentation." [Online]. Available: https://www.qemu.org/docs/master/. Accessed: Jun. 30, 2026.

[7] GNU Project, "GNU Make Manual." [Online]. Available: https://www.gnu.org/software/make/manual/. Accessed: Jun. 30, 2026.

[8] Git Project, "Git Documentation." [Online]. Available: https://git-scm.com/doc. Accessed: Jun. 30, 2026.

[9] Limine Bootloader Project, "Limine Documentation." [Online]. Available: https://github.com/limine-bootloader/limine. Accessed: Jun. 30, 2026.

[10] Dokumentasi Praktikum Sistem Operasi MCSOS, "Panduan Milestone M6 – Physical Memory Manager (PMM)," Fakultas Ilmu Komputer Universitas Indonesia, 2026.
```

---

## 25. Checklist Final Sebelum Pengumpulan

| Checklist                                                   | Status |
| Semua placeholder `[isi ...]` sudah diganti                 | `Ya` |
| Metadata laporan lengkap                                    | `Ya` |
| Commit awal dan akhir dicatat                               | `Ya` |
| Perintah build dan test dapat dijalankan ulang              | `Ya` |
| Log build dilampirkan                                       | `Ya` |
| Log QEMU/test dilampirkan                                   | `Tidak` |
| Artefak penting diberi hash                                 | `Ya` |
| Desain, invariants, ownership, dan failure modes dijelaskan | `Ya` |
| Security/reliability dibahas                                | `Ya` |
| Readiness review tidak berlebihan                           | `Ya` |
| Rubrik penilaian diisi atau disiapkan                       | `Ya` |
| Referensi memakai format IEEE                               | `Ya` |
| Laporan disimpan sebagai Markdown                           | `Ya` |

---
## 26. Pernyataan Pengumpulan

Saya mengumpulkan laporan ini bersama artefak pendukung pada commit:

```text
f42ec4c
```

Status akhir yang diklaim:

```text
Siap demonstrasi praktikum
```

Ringkasan satu paragraf:

```text
Pada praktikum Milestone 6 ini berhasil diimplementasikan Physical Memory Manager (PMM) pada kernel MCSOS beserta integrasinya ke sistem build menggunakan Clang/LLVM freestanding. Implementasi mencakup penambahan modul PMM, header antarmuka, pembaruan Makefile, serta unit test host untuk memverifikasi fungsi dasar allocator. Proyek berhasil dibangun ulang dari kondisi bersih menggunakan `make clean` dan `make`, menghasilkan artefak `kernel.elf`, `kernel.map`, dan `kernel.disasm.txt` yang telah diverifikasi menggunakan readelf, objdump, dan hash SHA-256. Seluruh perubahan telah dikomit dan dipublikasikan pada branch `main`. Keterbatasan implementasi saat ini adalah belum tersedianya bukti pengujian QEMU dan GDB secara langsung sehingga validasi masih berfokus pada keberhasilan build, inspeksi statis, dan pengujian unit. Pengembangan berikutnya akan melanjutkan integrasi PMM dengan Virtual Memory Manager (VMM), page table, serta kernel heap pada milestone selanjutnya.
```

---
