# OS_panduan_M5.md

# Panduan Praktikum M5 — External Interrupt, Legacy PIC Remap, dan PIT Timer Tick pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M5  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: *siap uji QEMU untuk external interrupt awal*, bukan siap produksi.

---

## 1. Ringkasan Praktikum

Praktikum M5 memperluas hasil M4 dari penanganan exception dan IDT menjadi jalur external interrupt yang dapat menghasilkan tick timer deterministik. Mahasiswa mengimplementasikan tiga bagian inti: remapping legacy Intel 8259A Programmable Interrupt Controller atau PIC agar IRQ tidak berbenturan dengan exception CPU, konfigurasi Intel 8254/8253 Programmable Interval Timer atau PIT pada channel 0, dan dispatcher trap yang membedakan exception CPU dari IRQ hardware. Intel 8259A memakai rangkaian Initialization Command Word dan Operation Command Word untuk mode 8086 serta End-of-Interrupt, sedangkan Intel 8254 menyediakan tiga counter 16-bit dan enam mode yang dapat diprogram perangkat lunak [2], [3]. Pada tahap ini legacy PIC/PIT dipakai sebagai jalur pendidikan awal sebelum APIC/IOAPIC/HPET/LAPIC timer diperkenalkan.

Praktikum ini tidak membuktikan sistem operasi bebas kesalahan. Target yang valid adalah kernel MCSOS dapat dibangun dari clean checkout, IDT masih valid, IRQ0 masuk ke stub vektor 32 setelah PIC diremap, PIT menghasilkan tick pada interval yang dapat diamati di serial log QEMU, dan panic path tetap dapat dibaca jika terjadi exception.

---

## 2. Asumsi Target dan Batasan

| Aspek | Keputusan M5 |
|---|---|
| Arsitektur | x86_64 long mode |
| Lingkungan host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Boot path | Melanjutkan M2/M3/M4; direkomendasikan Limine/UEFI atau pipeline ISO yang sudah lulus M2 |
| Toolchain | Clang/LLD atau GCC/binutils freestanding; contoh tervalidasi memakai Clang + LLD |
| Bahasa | C17 freestanding + assembly AT&T minimal |
| Kernel | Monolitik pendidikan, single-core awal, belum SMP |
| Scope perangkat | Legacy PIC 8259A dan PIT 8254/8253 pada port I/O klasik |
| Out of scope | APIC, IOAPIC, HPET, LAPIC timer, preemptive scheduler, user mode, SMP, dan power management |

Catatan batasan: QEMU menyediakan model PC yang dapat mengemulasikan perangkat legacy seperti PIC/PIT, tetapi keberhasilan QEMU tidak otomatis membuktikan kesiapan hardware fisik. Dokumentasi QEMU menegaskan pilihan machine dan perangkat emulator harus dikonfigurasi secara eksplisit melalui opsi sistem [4].


---

## 2A. Goals dan Non-Goals

### Goals

1. Menghasilkan jalur external interrupt awal yang terukur untuk MCSOS x86_64.
2. Menyediakan remap legacy PIC ke vector `0x20..0x2F` dan membuka IRQ0 secara terkendali.
3. Menghasilkan tick timer awal dari PIT channel 0 yang dapat diamati melalui serial log.
4. Mempertahankan panic path dan exception dispatcher M4 agar regression mudah didiagnosis.
5. Menyediakan bukti build, audit ELF, audit symbol, audit disassembly, dan QEMU smoke test.

### Non-Goals

1. Tidak mengimplementasikan scheduler preemptive final.
2. Tidak mengganti legacy PIC/PIT dengan APIC, IOAPIC, HPET, atau LAPIC timer pada M5.
3. Tidak mendukung SMP, user mode, syscall ABI, atau interrupt affinity.
4. Tidak menyatakan sistem siap produksi atau siap hardware umum.

## 2B. Architecture and Design Overview

Arsitektur M5 terdiri atas empat komponen: IDT dan stub assembly sebagai entry interrupt, driver PIC untuk routing IRQ legacy, driver PIT untuk sumber tick periodik, dan dispatcher C untuk memisahkan exception dari IRQ. Jalur data utamanya adalah `PIT -> PIC -> IDT[32] -> isr_stub_32 -> isr_common_stub -> x86_64_trap_dispatch -> timer_on_irq0 -> pic_send_eoi`.

## 2C. Interfaces, ABI, dan API Boundary

Boundary ABI M5 terletak di stub assembly `isr_common_stub`. Stub menyimpan register umum, membuat pointer `struct trap_frame`, memanggil fungsi C `x86_64_trap_dispatch(struct trap_frame *)`, memulihkan register, membuang pasangan `vector/error_code`, lalu kembali dengan `iretq`. API internal yang boleh dipakai modul lain adalah `idt_init()`, `pic_remap()`, `pic_mask_all()`, `pic_unmask_irq()`, `pic_send_eoi()`, `pit_configure_hz()`, `timer_ticks()`, dan `timer_on_irq0()`.

## 2D. Security and Threat Model Ringkas

Threat model M5 mencakup fault akibat interrupt masuk sebelum IDT valid, interrupt storm akibat IRQ tidak dimask, EOI hilang, trap frame rusak, dan penggunaan dependency host yang tidak tersedia di kernel freestanding. Mitigasi minimum adalah `cli` selama konfigurasi, mask semua IRQ sebagai default, unmask hanya IRQ0, fail-closed untuk exception fatal, dan audit `nm -u` agar tidak ada pemanggilan libc host.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M5, mahasiswa harus mampu:

1. Menjelaskan perbedaan exception CPU, software interrupt, dan external hardware interrupt.
2. Menjelaskan alasan vektor IRQ legacy perlu diremap dari rentang historis ke rentang aman `0x20..0x2F` agar tidak tumpang tindih dengan exception CPU `0..31`.
3. Mengimplementasikan akses port I/O `inb`/`outb` secara eksplisit dengan constraint assembly yang aman untuk kernel freestanding.
4. Menginisialisasi PIC master dan slave memakai ICW1–ICW4 dengan mode 8086, cascade master/slave, masking, unmasking IRQ0, dan EOI.
5. Mengonfigurasi PIT channel 0 ke frekuensi 100 Hz menggunakan command word `0x36` dan divisor dari basis frekuensi historis 1,193,182 Hz.
6. Memperluas trap dispatcher M4 agar IRQ0 tidak diperlakukan sebagai fatal exception.
7. Menghasilkan bukti build, audit ELF, audit symbol, audit disassembly, serial log QEMU, dan analisis failure mode.
8. Menyusun rollback jika jalur interrupt menyebabkan hang, interrupt storm, triple fault, atau log tidak keluar.

---

## 4. Prasyarat Teori

Mahasiswa harus memahami M0–M4 sebelum mulai. M0 menyiapkan tata kelola proyek dan lingkungan. M1 memverifikasi toolchain dan reproducibility. M2 membangun bootable image awal. M3 menyediakan early console, logging, panic path, dan halt path. M4 menyediakan IDT, exception stubs, trap frame, dan dispatcher exception.

Secara konseptual, M5 membutuhkan pemahaman berikut:

| Konsep | Makna Praktis di M5 | Bukti Minimal |
|---|---|---|
| IDT gate | Setiap interrupt vector menunjuk ke stub assembly | `readelf`, `nm`, `objdump` menunjukkan stub dan `lidt` |
| Trap frame | Register disimpan sebelum C handler dipanggil | Layout `struct trap_frame` konsisten dengan urutan `pushq` |
| PIC remap | IRQ0 berpindah ke vector `0x20` | Log menunjukkan timer IRQ tidak masuk vector exception |
| PIT divisor | `divisor = 1193182 / hz` | Konfigurasi 100 Hz menghasilkan log tick periodik |
| EOI | PIC diberi tahu interrupt selesai | Tidak terjadi satu tick lalu berhenti akibat ISR tidak dibersihkan |
| `sti`/`cli` | Interrupt diaktifkan hanya setelah IDT, PIC, dan PIT siap | Tidak ada interrupt sebelum handler siap |

Intel SDM menjadi rujukan primer untuk perilaku interrupt/exception, IDT, `IRETQ`, dan interrupt flag pada x86_64 [1].

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M5 |
|---|---|
| `osdev-general` | Readiness gate, urutan milestone, integrasi M0–M5 |
| `osdev-01-computer-foundation` | Invariant state machine interrupt, liveness tick, safety proof obligation |
| `osdev-02-low-level-programming` | Assembly stub, ABI, stack alignment, `iretq`, port I/O, red-zone policy |
| `osdev-03-computer-and-hardware-architecture` | x86_64 interrupt model, PIC/PIT, privilege, port I/O |
| `osdev-04-kernel-development` | Trap dispatcher, panic path, logging, observability kernel |
| `osdev-07-os-security` | Fail-closed sebelum `sti`, validasi boundary handler, tidak menerima IRQ tak dikenal sebagai normal |
| `osdev-08-device-driver-development` | PIC/PIT sebagai driver perangkat awal berbasis port I/O |
| `osdev-10-boot-firmware` | Integrasi boot image M2/M3/M4 dan serial log awal |
| `osdev-12-toolchain-devenv` | Build freestanding, linker script, audit ELF/disassembly, QEMU/GDB workflow |
| `osdev-14-cross-science` | Verification matrix, risk register, failure mode, evidence-based readiness |

---

## 6. Alat dan Versi yang Disarankan

Gunakan versi yang tersedia di WSL 2, tetapi catat versi aktual pada laporan.

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

QEMU menyediakan gdbstub; opsi `-s -S` membuat QEMU membuka port GDB 1234 dan menahan guest sampai debugger melanjutkan eksekusi [5]. Linker script dipakai untuk mengontrol layout section dan entry point; dokumentasi GNU ld menjelaskan bahwa `SECTIONS` mengatur pemetaan section input ke output dan `ENTRY(symbol)` menetapkan entry point [6]. LLD mendukung target ELF dan kompatibilitas besar dengan opsi/linker script GNU ld [7].

---

## 7. Struktur Repository yang Diharapkan

Struktur berikut bersifat kompatibel dengan M5. Jika repository M4 sudah memakai nama berbeda, lakukan mapping nama dengan konsisten dan dokumentasikan di laporan.

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
│   └── serial.c
├── scripts/
│   └── check_m5_static.sh
└── build/
    └── ... hasil build, map, symbol, disassembly, log
```

Jika M4 sudah mempunyai `serial.c`, `panic.c`, `idt.c`, dan `interrupts.S`, jangan menyalin buta. Lakukan merge bertahap: tambah fitur PIC/PIT dan perluas stub vector sampai 47, bukan membuat dua dispatcher yang saling bertabrakan.

---

## 8. Pemeriksaan Kesiapan Hasil M0–M4

Bagian ini wajib dilakukan sebelum menulis kode M5. Tujuannya bukan mengulang praktikum lama, tetapi memastikan M5 tidak dibangun di atas artefak yang sudah rusak.

### 8.1 Pemeriksaan M0 — Repository, governance, dan baseline lingkungan

Jalankan perintah berikut dari root repository. Perintah ini memastikan repository berada dalam kontrol Git, struktur dokumen minimal tersedia, dan mahasiswa tidak bekerja di direktori acak yang tidak dapat direproduksi.

```bash
git status --short
git rev-parse --show-toplevel
test -f README.md || echo "PERINGATAN: README.md belum tersedia"
test -d docs || echo "PERINGATAN: direktori docs belum tersedia"
```

Indikator lulus: `git rev-parse` mengembalikan root repository, `git status --short` tidak menunjukkan perubahan tidak disengaja, dan dokumen tata kelola minimal tersedia.

Saran perbaikan:

| Gejala | Penyebab Mungkin | Solusi |
|---|---|---|
| `fatal: not a git repository` | Mahasiswa berada di folder salah | Pindah ke folder `mcsos` atau clone ulang repository |
| Banyak file tak dikenal | Build artefact ikut masuk Git | Tambahkan `build/`, `*.o`, `*.elf`, `*.iso`, `*.map`, `*.log` ke `.gitignore` |
| Tidak ada README/docs | M0 belum tuntas | Lengkapi dokumen baseline sebelum M5 |

### 8.2 Pemeriksaan M1 — Toolchain dan audit build

M5 membutuhkan compiler, assembler, linker, dan tool audit. Jalankan:

```bash
command -v clang
command -v ld.lld
command -v make
command -v readelf
command -v objdump || command -v llvm-objdump
command -v nm
```

Indikator lulus: semua tool inti tersedia. Jika memakai GCC/binutils, pastikan target dan flag tetap freestanding serta tidak menarik libc host.

Saran perbaikan:

| Gejala | Solusi |
|---|---|
| `clang: command not found` | `sudo apt update && sudo apt install -y clang lld` |
| `ld.lld: command not found` | `sudo apt install -y lld` |
| `llvm-objdump` tidak ada | Gunakan `objdump` dan sesuaikan variabel `OBJDUMP` di Makefile |
| Build menarik `libc` | Tambahkan `-ffreestanding -nostdlib -fno-stack-protector -mno-red-zone` dan audit `nm -u` |

### 8.3 Pemeriksaan M2 — Boot image dan serial path

M5 bergantung pada jalur boot dan serial log dari M2. Jalankan target build/ISO dari M2, misalnya:

```bash
make clean
make all
make iso || true
ls -lah build || true
```

Indikator lulus: kernel ELF atau ISO tersedia, serial log dari M2/M3 dapat dibaca di QEMU.

Saran perbaikan:

| Gejala | Penyebab Mungkin | Solusi |
|---|---|---|
| ISO tidak dibuat | Limine/xorriso belum tersedia | Ulangi panduan M2 untuk `limine`, `xorriso`, dan struktur `iso_root` |
| QEMU boot tetapi tidak ada log | Serial COM1 belum diinisialisasi atau `-serial stdio` tidak dipakai | Pastikan `serial_init()` dipanggil sebelum log dan jalankan QEMU dengan `-serial stdio` |
| Kernel tidak ditemukan bootloader | Path kernel di konfigurasi boot salah | Cocokkan nama file kernel ELF dengan konfigurasi Limine/bootloader |

### 8.4 Pemeriksaan M3 — Panic path, halt path, dan logging

Jalankan varian panic dari M3 jika masih tersedia.

```bash
make panic || true
make run-panic || true
```

Indikator lulus: panic log terbaca, lalu CPU masuk loop halt tanpa reboot tidak terkendali.

Saran perbaikan:

| Gejala | Solusi |
|---|---|
| Panic tidak mencetak pesan | Pastikan serial sudah siap sebelum `kernel_panic()` |
| QEMU langsung reboot | Tambahkan `-no-reboot -no-shutdown`; periksa triple fault akibat IDT/stack rusak |
| Halt memakan CPU 100% | Gunakan instruksi `hlt` dalam loop dengan interrupt policy yang jelas |

### 8.5 Pemeriksaan M4 — IDT dan exception stub

M5 tidak boleh dimulai jika IDT M4 belum stabil. Jalankan:

```bash
make clean
make all
nm -n build/*.elf | grep -E "idt_init|x86_64_trap_dispatch|isr_stub_3|isr_stub_14" || true
objdump -d build/*.elf | grep -E "lidt|iretq" || true
```

Indikator lulus: symbol IDT dan stub exception tersedia, disassembly memuat `lidt` dan `iretq`, dan varian `int3` dari M4 menghasilkan log breakpoint.

Saran perbaikan:

| Gejala | Penyebab Mungkin | Solusi |
|---|---|---|
| `iretq` tidak ditemukan | Stub assembly belum dipakai atau optimizer menghapus jalur | Pastikan `interrupts.S` masuk `OBJS` dan symbol stub direferensikan tabel IDT |
| `lidt` tidak ditemukan | `idt_init()` belum memuat IDT | Audit `idt_init()` dan pastikan dipanggil sebelum interrupt/test |
| Page fault/triple fault saat `int3` | Layout trap frame tidak cocok dengan urutan push/pop | Samakan urutan `pushq` assembly dengan `struct trap_frame` |
| Error-code exception salah | Stub error-code/no-error-code tertukar | Vector 8,10,11,12,13,14,17,21,29,30 harus diperlakukan sebagai error-code exception |

---

## 9. Desain M5

### 9.1 State machine interrupt M5

```text
BOOT_EARLY
  -> SERIAL_READY
  -> IDT_READY
  -> PIC_REMAP_MASKED
  -> PIT_CONFIGURED
  -> IRQ0_UNMASKED
  -> INTERRUPTS_ENABLED
  -> TICKING
  -> READY_FOR_QEMU_SMOKE_TEST
```

Transisi hanya boleh maju jika precondition terpenuhi. `sti` hanya boleh dipanggil pada state `IRQ0_UNMASKED`, bukan sebelumnya.

### 9.2 Invariants

| Invariant | Rationale | Cara Uji |
|---|---|---|
| IDT dimuat sebelum `sti` | Jika interrupt datang tanpa gate valid, sistem dapat triple fault | Disassembly dan urutan log |
| PIC diremap ke `0x20/0x28` | Menghindari konflik vector IRQ dengan exception CPU | Audit `pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET)` |
| IRQ selain IRQ0 tetap masked | Mengurangi interrupt noise pada tahap awal | `pic_read_master_mask()` menampilkan bit selain IRQ0 masih masked |
| PIT dikonfigurasi sebelum IRQ0 diharapkan | Tick tidak muncul jika PIT belum mengeluarkan IRQ | Log `[MCSOS:TIMER] ticks=...` |
| EOI dikirim setelah IRQ ditangani | Tanpa EOI, PIC dapat berhenti mengirim interrupt berikutnya | Tick berlanjut lebih dari satu periode |
| Handler exception tetap fail-closed | Exception tidak boleh dianggap sukses diam-diam | Exception selain breakpoint memanggil panic |
| Tidak ada libc host | Kernel freestanding tidak boleh bergantung pada runtime host | `nm -u` kosong |
| Assembly stub menjaga register umum | C handler tidak merusak konteks interrupt | Audit push/pop dan uji breakpoint/timer |

### 9.3 Arsitektur ringkas

```text
PIT channel 0 --IRQ0--> PIC master IR0 --vector 0x20--> IDT[32]
      |                                                   |
      v                                                   v
  pit_configure_hz()                              isr_stub_32
                                                          |
                                                          v
                                               isr_common_stub
                                                          |
                                                          v
                                          x86_64_trap_dispatch()
                                                          |
                                                          v
                                      timer_on_irq0(); pic_send_eoi(0)
```

---

## 10. Instruksi Implementasi Langkah demi Langkah

### Langkah 1 — Buat branch M5

Perintah ini memisahkan pekerjaan M5 dari hasil M4 sehingga rollback dapat dilakukan tanpa kehilangan baseline.

```bash
git status --short
git checkout -b praktikum/m5-timer-irq
```

Indikator lulus: `git branch --show-current` menampilkan `praktikum/m5-timer-irq`.

### Langkah 2 — Bersihkan build lama dan verifikasi M4

Perintah ini memastikan tidak ada artefak lama yang menyamarkan error M5.

```bash
make clean
make all
make grade || true
```

Jika `make all` gagal pada tahap ini, hentikan M5 dan perbaiki M4. Jangan menambal M5 di atas build M4 yang rusak.

### Langkah 3 — Tambahkan akses port I/O dan CPU control

Tambahkan `include/io.h`. File ini menjadi boundary low-level untuk `outb`, `inb`, `cli`, `sti`, `hlt`, dan pembacaan `CS`. Jangan memakai fungsi I/O biasa karena port I/O adalah instruksi CPU, bukan akses memori normal.

### Langkah 4 — Tambahkan driver PIC

Tambahkan `include/pic.h` dan `src/pic.c`. PIC harus diremap sebelum interrupt diaktifkan. Mask semua IRQ terlebih dahulu, lalu unmask hanya IRQ0.

Kontrak implementasi:

1. `pic_remap()` menyimpan mask lama, mengirim ICW1–ICW4 ke master dan slave, lalu memulihkan mask.
2. `pic_mask_all()` menutup semua IRQ sebagai safe default.
3. `pic_unmask_irq(0)` membuka hanya timer IRQ.
4. `pic_send_eoi(irq)` mengirim EOI ke slave jika IRQ >= 8, lalu ke master.

### Langkah 5 — Tambahkan driver PIT

Tambahkan `include/pit.h` dan `src/pit.c`. M5 memakai 100 Hz sebagai frekuensi praktikum karena mudah diamati tanpa membanjiri serial log.

Kontrak implementasi:

1. `pit_configure_hz(100)` menghitung divisor dari `1193182 / 100`.
2. Command word `0x36` berarti channel 0, akses low byte/high byte, mode 3, binary counting.
3. `timer_on_irq0()` menaikkan `g_ticks` dan mencetak log setiap 100 tick.
4. `g_ticks` bersifat `volatile` karena dimodifikasi di jalur interrupt.

### Langkah 6 — Perluas IDT sampai vector 47

M4 minimal biasanya memuat vector 0–31. M5 harus menambah vector 32–47 untuk IRQ PIC. Stub IRQ tidak memiliki hardware error code, sehingga gunakan varian no-error-code.

### Langkah 7 — Perbarui dispatcher trap

Dispatcher harus memisahkan tiga kelas kejadian:

1. `vector 32..47`: hardware IRQ dari PIC.
2. `vector 3`: breakpoint test yang non-fatal.
3. vector lain: exception fatal yang masuk panic.

### Langkah 8 — Atur urutan boot di `kmain`

Urutan yang benar adalah:

```text
cli -> serial_init -> idt_init -> pic_remap -> pic_mask_all -> pic_unmask_irq0 -> pit_configure -> sti -> hlt loop
```

Jangan memanggil `sti` sebelum `idt_init`, `pic_remap`, dan `pit_configure_hz` selesai.

### Langkah 9 — Build dan audit statis

Jalankan:

```bash
make clean
make grade
```

Artefak wajib:

```text
build/mcsos-m5.elf
build/mcsos-m5.map
build/readelf-header.txt
build/readelf-sections.txt
build/readelf-program-headers.txt
build/symbols.txt
build/undefined.txt
build/disassembly.txt
```

### Langkah 10 — Jalankan QEMU smoke test

Jika pipeline ISO dari M2/M3/M4 sudah tersedia, jalankan target QEMU. Contoh umum:

```bash
qemu-system-x86_64 \
  -M q35 \
  -m 512M \
  -cdrom build/mcsos.iso \
  -serial stdio \
  -no-reboot \
  -no-shutdown
```

Indikator lulus serial log minimal:

```text
[MCSOS:M5] boot: external interrupt bring-up start
[MCSOS:M5] idt: loaded
[MCSOS:M5] pic: remapped; mask master=...
[MCSOS:M5] pit: configured 100Hz
[MCSOS:M5] sti: enabling interrupts
[MCSOS:TIMER] ticks=100
[MCSOS:TIMER] ticks=200
```

Jika log timer tidak muncul, lihat bagian failure modes.

### Langkah 11 — Jalankan debug GDB bila timer tidak bekerja

Gunakan QEMU gdbstub. QEMU mendukung debug guest melalui remote GDB; opsi `-s -S` membuka port 1234 dan menghentikan guest sampai GDB melanjutkan eksekusi [5].

Terminal 1:

```bash
qemu-system-x86_64 \
  -M q35 \
  -m 512M \
  -cdrom build/mcsos.iso \
  -serial stdio \
  -no-reboot \
  -no-shutdown \
  -s -S
```

Terminal 2:

```bash
gdb build/mcsos-m5.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) break idt_init
(gdb) break pic_remap
(gdb) break pit_configure_hz
(gdb) break x86_64_trap_dispatch
(gdb) continue
```

---

## 11. Source Code M5 yang Telah Diperiksa Secara Statis

Source berikut telah diuji di lingkungan validasi lokal dengan kompilasi `clang -target x86_64-elf`, link `ld.lld -nostdlib`, audit `readelf`, `nm -u`, dan `llvm-objdump`. Hasil validasi menunjukkan `M5 static grade: PASS`. Validasi runtime QEMU/OVMF tetap wajib dilakukan di WSL 2 mahasiswa karena bergantung pada paket QEMU, OVMF, Limine/ISO, dan konfigurasi host setempat.

### `linker.ld`

```ld
ENTRY(_start)

SECTIONS
{
    . = 2M;

    .text : ALIGN(4K) {
        *(.text .text.*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K) {
        *(.data .data.*)
    }

    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(16);
        __stack_bottom = .;
        . += 64K;
        __stack_top = .;
    }
}
```
### `include/types.h`

```c
#ifndef MCSOS_TYPES_H
#define MCSOS_TYPES_H
#include <stddef.h>
#include <stdint.h>
#endif
```
### `include/io.h`

```c
#ifndef MCSOS_IO_H
#define MCSOS_IO_H
#include "types.h"

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void cpu_cli(void) {
    __asm__ volatile ("cli" ::: "memory");
}

static inline void cpu_sti(void) {
    __asm__ volatile ("sti" ::: "memory");
}

static inline void cpu_hlt(void) {
    __asm__ volatile ("hlt" ::: "memory");
}

static inline uint16_t x86_64_read_cs(void) {
    uint16_t value;
    __asm__ volatile ("movw %%cs, %0" : "=rm"(value));
    return value;
}

#endif
```
### `include/serial.h`

```c
#ifndef MCSOS_SERIAL_H
#define MCSOS_SERIAL_H
#include "types.h"

void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *s);
void serial_write_hex64(uint64_t value);
void serial_write_dec64(uint64_t value);

#endif
```
### `src/serial.c`

```c
#include "io.h"
#include "serial.h"

#define COM1 0x3F8u

void serial_init(void) {
    outb(COM1 + 1u, 0x00u);
    outb(COM1 + 3u, 0x80u);
    outb(COM1 + 0u, 0x03u);
    outb(COM1 + 1u, 0x00u);
    outb(COM1 + 3u, 0x03u);
    outb(COM1 + 2u, 0xC7u);
    outb(COM1 + 4u, 0x0Bu);
}

static int serial_transmit_empty(void) {
    return (inb(COM1 + 5u) & 0x20u) != 0;
}

void serial_write_char(char c) {
    while (!serial_transmit_empty()) {
        __asm__ volatile ("pause");
    }
    outb(COM1, (uint8_t)c);
}

void serial_write_string(const char *s) {
    while (*s != '\0') {
        if (*s == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*s);
        ++s;
    }
}

void serial_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    serial_write_string("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_write_char(digits[(value >> (unsigned)i) & 0xFu]);
    }
}

void serial_write_dec64(uint64_t value) {
    char buf[21];
    size_t i = 0;
    if (value == 0) {
        serial_write_char('0');
        return;
    }
    while (value != 0 && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (i != 0) {
        serial_write_char(buf[--i]);
    }
}
```
### `include/panic.h`

```c
#ifndef MCSOS_PANIC_H
#define MCSOS_PANIC_H
#include "types.h"

_Noreturn void halt_forever(void);
_Noreturn void kernel_panic(const char *reason, uint64_t code);

#endif
```
### `src/panic.c`

```c
#include "io.h"
#include "panic.h"
#include "serial.h"

_Noreturn void halt_forever(void) {
    cpu_cli();
    for (;;) {
        cpu_hlt();
    }
}

_Noreturn void kernel_panic(const char *reason, uint64_t code) {
    cpu_cli();
    serial_write_string("\n[MCSOS:PANIC] ");
    serial_write_string(reason);
    serial_write_string(" code=");
    serial_write_hex64(code);
    serial_write_string("\n");
    for (;;) {
        cpu_hlt();
    }
}
```
### `include/pic.h`

```c
#ifndef MCSOS_PIC_H
#define MCSOS_PIC_H
#include "types.h"

#define PIC_MASTER_OFFSET 0x20u
#define PIC_SLAVE_OFFSET  0x28u

void pic_remap(uint8_t master_offset, uint8_t slave_offset);
void pic_mask_all(void);
void pic_unmask_irq(uint8_t irq);
void pic_send_eoi(uint8_t irq);
uint8_t pic_read_master_mask(void);
uint8_t pic_read_slave_mask(void);

#endif
```
### `src/pic.c`

```c
#include "io.h"
#include "pic.h"

#define PIC1_COMMAND 0x20u
#define PIC1_DATA    0x21u
#define PIC2_COMMAND 0xA0u
#define PIC2_DATA    0xA1u
#define PIC_EOI      0x20u

void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11u);
    io_wait();
    outb(PIC2_COMMAND, 0x11u);
    io_wait();

    outb(PIC1_DATA, master_offset);
    io_wait();
    outb(PIC2_DATA, slave_offset);
    io_wait();

    outb(PIC1_DATA, 0x04u);
    io_wait();
    outb(PIC2_DATA, 0x02u);
    io_wait();

    outb(PIC1_DATA, 0x01u);
    io_wait();
    outb(PIC2_DATA, 0x01u);
    io_wait();

    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

void pic_mask_all(void) {
    outb(PIC1_DATA, 0xFFu);
    outb(PIC2_DATA, 0xFFu);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t line;
    if (irq < 8u) {
        port = PIC1_DATA;
        line = irq;
    } else {
        port = PIC2_DATA;
        line = (uint8_t)(irq - 8u);
    }
    uint8_t mask = inb(port);
    mask = (uint8_t)(mask & (uint8_t)~(1u << line));
    outb(port, mask);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8u) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

uint8_t pic_read_master_mask(void) {
    return inb(PIC1_DATA);
}

uint8_t pic_read_slave_mask(void) {
    return inb(PIC2_DATA);
}
```
### `include/pit.h`

```c
#ifndef MCSOS_PIT_H
#define MCSOS_PIT_H
#include "types.h"

#define PIT_BASE_FREQUENCY_HZ 1193182u

void pit_configure_hz(uint32_t hz);
uint64_t timer_ticks(void);
void timer_on_irq0(void);

#endif
```
### `src/pit.c`

```c
#include "io.h"
#include "pit.h"
#include "serial.h"

#define PIT_CHANNEL0 0x40u
#define PIT_COMMAND  0x43u

static volatile uint64_t g_ticks = 0;

void pit_configure_hz(uint32_t hz) {
    if (hz == 0u) {
        hz = 100u;
    }
    uint32_t divisor = PIT_BASE_FREQUENCY_HZ / hz;
    if (divisor == 0u) {
        divisor = 1u;
    }
    if (divisor > 0xFFFFu) {
        divisor = 0xFFFFu;
    }

    outb(PIT_COMMAND, 0x36u);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFFu));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8u) & 0xFFu));
}

uint64_t timer_ticks(void) {
    return g_ticks;
}

void timer_on_irq0(void) {
    ++g_ticks;
    if ((g_ticks % 100u) == 0u) {
        serial_write_string("[MCSOS:TIMER] ticks=");
        serial_write_dec64(g_ticks);
        serial_write_string("\n");
    }
}
```
### `include/idt.h`

```c
#ifndef MCSOS_IDT_H
#define MCSOS_IDT_H
#include "types.h"

struct trap_frame {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

void idt_init(void);
void x86_64_trap_dispatch(struct trap_frame *frame);

#endif
```
### `src/idt.c`

```c
#include "idt.h"
#include "io.h"
#include "panic.h"
#include "pic.h"
#include "pit.h"
#include "serial.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern void (*const isr_stub_table[48])(void);

static struct idt_entry g_idt[256];

static void idt_set_gate(uint8_t vector, void (*handler)(void), uint16_t selector, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;
    g_idt[vector].offset_low = (uint16_t)(addr & 0xFFFFu);
    g_idt[vector].selector = selector;
    g_idt[vector].ist = 0;
    g_idt[vector].type_attr = type_attr;
    g_idt[vector].offset_mid = (uint16_t)((addr >> 16u) & 0xFFFFu);
    g_idt[vector].offset_high = (uint32_t)((addr >> 32u) & 0xFFFFFFFFu);
    g_idt[vector].zero = 0;
}

static void lidt(const struct idt_pointer *ptr) {
    __asm__ volatile ("lidt (%0)" :: "r"(ptr) : "memory");
}

void idt_init(void) {
    const uint16_t cs = x86_64_read_cs();
    for (uint8_t i = 0; i < 48u; ++i) {
        idt_set_gate(i, isr_stub_table[i], cs, 0x8Eu);
    }
    struct idt_pointer ptr = {
        .limit = (uint16_t)(sizeof(g_idt) - 1u),
        .base = (uint64_t)&g_idt[0],
    };
    lidt(&ptr);
}

void x86_64_trap_dispatch(struct trap_frame *frame) {
    if (frame == (struct trap_frame *)0) {
        kernel_panic("null trap frame", 0);
    }

    if (frame->vector >= PIC_MASTER_OFFSET && frame->vector < (PIC_SLAVE_OFFSET + 8u)) {
        uint8_t irq = (uint8_t)(frame->vector - PIC_MASTER_OFFSET);
        if (irq == 0u) {
            timer_on_irq0();
        } else {
            serial_write_string("[MCSOS:IRQ] unexpected irq=");
            serial_write_dec64(irq);
            serial_write_string("\n");
        }
        pic_send_eoi(irq);
        return;
    }

    if (frame->vector == 3u) {
        serial_write_string("[MCSOS:TRAP] breakpoint rip=");
        serial_write_hex64(frame->rip);
        serial_write_string("\n");
        return;
    }

    serial_write_string("[MCSOS:EXCEPTION] vector=");
    serial_write_dec64(frame->vector);
    serial_write_string(" error=");
    serial_write_hex64(frame->error_code);
    serial_write_string(" rip=");
    serial_write_hex64(frame->rip);
    serial_write_string("\n");
    kernel_panic("unhandled CPU exception", frame->vector);
}
```
### `src/interrupts.S`

```asm
.text
.global isr_common_stub
.type isr_common_stub, @function
isr_common_stub:
    cld
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax
    movq %rsp, %rdi
    call x86_64_trap_dispatch
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rbp
    popq %rdi
    popq %rsi
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    addq $16, %rsp
    iretq
.size isr_common_stub, . - isr_common_stub

.macro ISR_NOERR n
.global isr_stub_\n
.type isr_stub_\n, @function
isr_stub_\n:
    pushq $0
    pushq $\n
    jmp isr_common_stub
.size isr_stub_\n, . - isr_stub_\n
.endm

.macro ISR_ERR n
.global isr_stub_\n
.type isr_stub_\n, @function
isr_stub_\n:
    pushq $\n
    jmp isr_common_stub
.size isr_stub_\n, . - isr_stub_\n
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

.section .rodata
.global isr_stub_table
.type isr_stub_table, @object
.align 8
isr_stub_table:
    .quad isr_stub_0
    .quad isr_stub_1
    .quad isr_stub_2
    .quad isr_stub_3
    .quad isr_stub_4
    .quad isr_stub_5
    .quad isr_stub_6
    .quad isr_stub_7
    .quad isr_stub_8
    .quad isr_stub_9
    .quad isr_stub_10
    .quad isr_stub_11
    .quad isr_stub_12
    .quad isr_stub_13
    .quad isr_stub_14
    .quad isr_stub_15
    .quad isr_stub_16
    .quad isr_stub_17
    .quad isr_stub_18
    .quad isr_stub_19
    .quad isr_stub_20
    .quad isr_stub_21
    .quad isr_stub_22
    .quad isr_stub_23
    .quad isr_stub_24
    .quad isr_stub_25
    .quad isr_stub_26
    .quad isr_stub_27
    .quad isr_stub_28
    .quad isr_stub_29
    .quad isr_stub_30
    .quad isr_stub_31
    .quad isr_stub_32
    .quad isr_stub_33
    .quad isr_stub_34
    .quad isr_stub_35
    .quad isr_stub_36
    .quad isr_stub_37
    .quad isr_stub_38
    .quad isr_stub_39
    .quad isr_stub_40
    .quad isr_stub_41
    .quad isr_stub_42
    .quad isr_stub_43
    .quad isr_stub_44
    .quad isr_stub_45
    .quad isr_stub_46
    .quad isr_stub_47
.size isr_stub_table, . - isr_stub_table
```
### `src/boot.S`

```asm
.text
.global _start
.type _start, @function
.extern kmain
.extern __stack_top
_start:
    cli
    leaq __stack_top(%rip), %rsp
    andq $-16, %rsp
    call kmain
1:
    hlt
    jmp 1b
.size _start, . - _start
```
### `src/kernel.c`

```c
#include "idt.h"
#include "io.h"
#include "panic.h"
#include "pic.h"
#include "pit.h"
#include "serial.h"

void kmain(void) {
    cpu_cli();
    serial_init();
    serial_write_string("[MCSOS:M5] boot: external interrupt bring-up start\n");

    idt_init();
    serial_write_string("[MCSOS:M5] idt: loaded\n");

    pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);
    pic_mask_all();
    pic_unmask_irq(0);
    serial_write_string("[MCSOS:M5] pic: remapped; mask master=");
    serial_write_hex64(pic_read_master_mask());
    serial_write_string(" slave=");
    serial_write_hex64(pic_read_slave_mask());
    serial_write_string("\n");

    pit_configure_hz(100u);
    serial_write_string("[MCSOS:M5] pit: configured 100Hz\n");
    serial_write_string("[MCSOS:M5] sti: enabling interrupts\n");
    cpu_sti();

#if defined(MCSOS_TEST_BREAKPOINT)
    __asm__ volatile ("int3");
#endif

    for (;;) {
        cpu_hlt();
    }
}
```
### `Makefile`

```make
TARGET := x86_64-elf
CC := clang
LD := ld.lld
OBJDUMP := llvm-objdump
READELF := readelf
NM := nm

CFLAGS := -target $(TARGET) -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -O2 -Wall -Wextra -Werror -Iinclude
ASFLAGS := -target $(TARGET) -ffreestanding -fno-pic -mno-red-zone -Wall -Wextra -Werror -Iinclude
LDFLAGS := -nostdlib -T linker.ld -z max-page-size=0x1000

BUILD := build
OBJS := $(BUILD)/boot.o $(BUILD)/interrupts.o $(BUILD)/serial.o $(BUILD)/panic.o $(BUILD)/pic.o $(BUILD)/pit.o $(BUILD)/idt.o $(BUILD)/kernel.o
KERNEL := $(BUILD)/mcsos-m5.elf

.PHONY: all clean audit breakpoint
all: $(KERNEL) audit

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -Map=$(BUILD)/mcsos-m5.map -o $@

breakpoint: CFLAGS += -DMCSOS_TEST_BREAKPOINT
breakpoint: clean all

audit: $(KERNEL)
	$(READELF) -h $(KERNEL) > $(BUILD)/readelf-header.txt
	$(READELF) -S $(KERNEL) > $(BUILD)/readelf-sections.txt
	$(READELF) -l $(KERNEL) > $(BUILD)/readelf-program-headers.txt
	$(NM) -n $(KERNEL) > $(BUILD)/symbols.txt
	$(NM) -u $(KERNEL) > $(BUILD)/undefined.txt
	$(OBJDUMP) -d $(KERNEL) > $(BUILD)/disassembly.txt
	test ! -s $(BUILD)/undefined.txt
	grep -q "lidt" $(BUILD)/disassembly.txt
	grep -q "iretq" $(BUILD)/disassembly.txt
	grep -q "outb" $(BUILD)/disassembly.txt
	grep -q "sti" $(BUILD)/disassembly.txt
	grep -q "hlt" $(BUILD)/disassembly.txt

grade: all
	grep -q "isr_stub_32" $(BUILD)/symbols.txt
	grep -q "pic_remap" $(BUILD)/symbols.txt
	grep -q "pit_configure_hz" $(BUILD)/symbols.txt
	grep -q "timer_on_irq0" $(BUILD)/symbols.txt
	grep -q "x86_64_trap_dispatch" $(BUILD)/symbols.txt
	@echo "M5 static grade: PASS"

clean:
	rm -rf $(BUILD)
```
### `scripts/check_m5_static.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail
make clean
make grade
printf '[M5] static build and audit passed.\n'
```

---

## 12. Perintah Validasi Statis

Jalankan dari root repository:

```bash
chmod +x scripts/check_m5_static.sh
./scripts/check_m5_static.sh
```

Validasi ini memeriksa:

1. Semua source C dan assembly dapat dikompilasi dengan `-Werror`.
2. Kernel ELF dapat dilink tanpa libc host.
3. `nm -u` kosong.
4. Disassembly mengandung `lidt`, `iretq`, `outb`, `sti`, dan `hlt`.
5. Symbol penting tersedia: `isr_stub_32`, `pic_remap`, `pit_configure_hz`, `timer_on_irq0`, dan `x86_64_trap_dispatch`.

Contoh log validasi lokal:

```text
clang -target x86_64-elf -ffreestanding -fno-pic -mno-red-zone -Wall -Wextra -Werror -Iinclude -c src/boot.S -o build/boot.o
clang -target x86_64-elf -ffreestanding -fno-pic -mno-red-zone -Wall -Wextra -Werror -Iinclude -c src/interrupts.S -o build/interrupts.o
clang -target x86_64-elf -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -O2 -Wall -Wextra -Werror -Iinclude -c src/serial.c -o build/serial.o
clang -target x86_64-elf -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -O2 -Wall -Wextra -Werror -Iinclude -c src/panic.c -o build/panic.o
clang -target x86_64-elf -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -O2 -Wall -Wextra -Werror -Iinclude -c src/pic.c -o build/pic.o
clang -target x86_64-elf -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -O2 -Wall -Wextra -Werror -Iinclude -c src/pit.c -o build/pit.o
clang -target x86_64-elf -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -O2 -Wall -Wextra -Werror -Iinclude -c src/idt.c -o build/idt.o
clang -target x86_64-elf -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -O2 -Wall -Wextra -Werror -Iinclude -c src/kernel.c -o build/kernel.o
ld.lld -nostdlib -T linker.ld -z max-page-size=0x1000 build/boot.o build/interrupts.o build/serial.o build/panic.o build/pic.o build/pit.o build/idt.o build/kernel.o -Map=build/mcsos-m5.map -o build/mcsos-m5.elf
readelf -h build/mcsos-m5.elf > build/readelf-header.txt
readelf -S build/mcsos-m5.elf > build/readelf-sections.txt
readelf -l build/mcsos-m5.elf > build/readelf-program-headers.txt
nm -n build/mcsos-m5.elf > build/symbols.txt
nm -u build/mcsos-m5.elf > build/undefined.txt
llvm-objdump -d build/mcsos-m5.elf > build/disassembly.txt
test ! -s build/undefined.txt
grep -q "lidt" build/disassembly.txt
grep -q "iretq" build/disassembly.txt
grep -q "outb" build/disassembly.txt
grep -q "sti" build/disassembly.txt
grep -q "hlt" build/disassembly.txt
grep -q "isr_stub_32" build/symbols.txt
grep -q "pic_remap" build/symbols.txt
grep -q "pit_configure_hz" build/symbols.txt
grep -q "timer_on_irq0" build/symbols.txt
grep -q "x86_64_trap_dispatch" build/symbols.txt
M5 static grade: PASS
[M5] static build and audit passed.
```

---

## 13. Checkpoint Buildable

| Checkpoint | Artefak | Perintah | Kriteria Lulus |
|---|---|---|---|
| M5-C1 | Port I/O dan CPU control | `make clean && make all` | Tidak ada warning/error |
| M5-C2 | PIC driver | `grep -q pic_remap build/symbols.txt` | Symbol PIC tersedia |
| M5-C3 | PIT driver | `grep -q pit_configure_hz build/symbols.txt` | Symbol PIT tersedia |
| M5-C4 | IRQ vector 32 | `grep -q isr_stub_32 build/symbols.txt` | Stub IRQ0 tersedia |
| M5-C5 | No unresolved symbol | `test ! -s build/undefined.txt` | Tidak ada dependency host |
| M5-C6 | Instruction audit | `grep -E "lidt|iretq|sti|outb" build/disassembly.txt` | Instruksi kritis ada |
| M5-C7 | QEMU boot | `make run` atau QEMU manual | Log M5 muncul |
| M5-C8 | Timer tick | QEMU serial | `ticks=100`, `ticks=200` muncul |

---

## 14. Failure Modes dan Solusi Perbaikan

| Gejala | Kemungkinan Penyebab | Diagnosis | Solusi |
|---|---|---|---|
| Build gagal pada `interrupts.S` | Macro assembly tidak cocok assembler | Lihat error baris macro | Pastikan ekstensi `.S`, bukan `.s`, dan gunakan Clang/GAS syntax AT&T |
| `nm -u` tidak kosong | Ada pemanggilan runtime host | `cat build/undefined.txt` | Hilangkan `printf`, `memcpy` host, stack protector, atau builtins yang belum disediakan |
| QEMU reboot tiba-tiba | Triple fault akibat IDT/stack/iret rusak | Jalankan `-no-reboot -no-shutdown -d int` | Audit `trap_frame`, push/pop, `addq $16,%rsp`, dan selector CS |
| Log berhenti setelah `sti` | Interrupt masuk handler rusak | GDB breakpoint di `x86_64_trap_dispatch` | Pastikan IDT[32] menunjuk `isr_stub_32` dan stub IRQ tidak error-code |
| Tidak ada tick | IRQ0 masih masked, PIT belum aktif, atau PIC belum remap | Cetak mask PIC dan audit `pit_configure_hz` | Urutan: `pic_remap`, `pic_mask_all`, `pic_unmask_irq(0)`, `pit_configure_hz`, baru `sti` |
| Tick hanya sekali | EOI tidak dikirim | Breakpoint setelah `timer_on_irq0` | Panggil `pic_send_eoi(0)` untuk IRQ0 |
| Timer terlalu cepat/lambat | Divisor salah | Cetak divisor | Gunakan `1193182 / hz`, clamp `1..65535` |
| Breakpoint `int3` menjadi panic | Dispatcher tidak mengecualikan vector 3 | Cek `frame->vector == 3` | Return setelah log breakpoint |
| Exception page fault salah error code | Stub error-code salah | Audit vector 14 | Gunakan `ISR_ERR` untuk vector error-code |
| Sistem hang sebelum log | Serial belum inisialisasi atau stack salah | GDB break `_start`, `kmain` | Set stack 16-byte aligned dan panggil `serial_init()` awal |
| `lidt` tidak ditemukan | `idt_init` terhapus/tidak dipanggil | `objdump -d` | Pastikan `idt_init()` dipanggil dan file `idt.c` masuk OBJS |
| `sti` ditemukan sebelum PIC/PIT | Urutan boot salah | Audit `kernel.c` dan disassembly | Panggil `cpu_sti()` paling akhir setelah konfigurasi interrupt |

---

## 15. Prosedur Rollback

Jika M5 menyebabkan boot regression, lakukan rollback bertahap.

1. Simpan bukti error:

```bash
mkdir -p evidence/m5-failure
cp -a build/*.txt build/*.map build/*.elf evidence/m5-failure/ 2>/dev/null || true
```

2. Nonaktifkan `cpu_sti()` sementara untuk memastikan panic/logging M4 masih sehat:

```c
/* cpu_sti(); */
```

3. Rebuild dan jalankan. Jika log M5 muncul sampai `pit: configured`, maka kerusakan berada di jalur IRQ runtime, bukan boot awal.

4. Mask semua IRQ dan jangan unmask IRQ0. Jika sistem stabil, masalah berada pada PIT/PIC/IRQ0 handler.

5. Kembalikan branch M4 bila perlu:

```bash
git status --short
git restore include src Makefile linker.ld
git checkout main
```

6. Jika perubahan M5 sudah terlanjur dicommit, gunakan revert, bukan reset paksa, kecuali instruktur mengizinkan:

```bash
git log --oneline -5
git revert <commit_m5>
```

---

## 16. Tugas Implementasi Mahasiswa

### Tugas wajib

1. Tambahkan driver port I/O, PIC, dan PIT sesuai source M5.
2. Perluas IDT M4 sampai vector 47.
3. Pastikan IRQ0 masuk `timer_on_irq0()` dan tick counter bertambah.
4. Pastikan EOI dikirim setelah IRQ0.
5. Pastikan exception fatal tetap memanggil panic.
6. Simpan bukti build, audit, QEMU log, dan analisis failure mode.

### Tugas pengayaan

1. Tambahkan pembacaan IRR/ISR PIC melalui OCW3 untuk diagnosis interrupt pending/in-service.
2. Tambahkan `timer_wait_ticks(uint64_t delta)` untuk busy wait terukur di tahap awal, dengan peringatan bahwa ini bukan scheduler sleep final.
3. Tambahkan counter untuk unexpected IRQ dan cetak setiap N kejadian.
4. Tambahkan opsi build `MCSOS_TEST_BREAKPOINT` untuk membuktikan exception dan IRQ tetap koeksis.

### Tantangan riset

1. Bandingkan legacy PIT/PIC dengan LAPIC timer dan IOAPIC dalam rancangan M6/M7.
2. Rancang state machine transisi dari interrupt single-core ke preemptive scheduler.
3. Buat model formal sederhana: `masked`, `pending`, `in_service`, `eoi_sent`, dan `ticks`.

---

## 17. Perintah Uji Lengkap

### 17.1 Uji statis

```bash
make clean
make grade
```

### 17.2 Audit ELF

```bash
readelf -h build/mcsos-m5.elf
readelf -S build/mcsos-m5.elf
readelf -l build/mcsos-m5.elf
nm -n build/mcsos-m5.elf | grep -E "isr_stub_32|pic_remap|pit_configure_hz|timer_on_irq0|x86_64_trap_dispatch"
nm -u build/mcsos-m5.elf
objdump -d build/mcsos-m5.elf | grep -E "lidt|iretq|outb|sti|hlt"
```

### 17.3 Uji runtime QEMU

```bash
qemu-system-x86_64 \
  -M q35 \
  -m 512M \
  -cdrom build/mcsos.iso \
  -serial stdio \
  -no-reboot \
  -no-shutdown
```

### 17.4 Uji GDB

```bash
qemu-system-x86_64 \
  -M q35 \
  -m 512M \
  -cdrom build/mcsos.iso \
  -serial stdio \
  -no-reboot \
  -no-shutdown \
  -s -S
```

```gdb
target remote :1234
break kmain
break idt_init
break pic_remap
break pit_configure_hz
break x86_64_trap_dispatch
continue
```

---

## 18. Bukti yang Wajib Dikumpulkan

| Bukti | File/format | Minimum isi |
|---|---|---|
| Commit Git | Hash commit | Perubahan M5 terpisah dari M4 |
| Build log | `evidence/m5-build.log` | Perintah `make clean && make grade` |
| ELF header | `readelf-header.txt` | ELF64 x86_64, entry valid |
| Section/program header | `readelf-sections.txt`, `readelf-program-headers.txt` | Layout `.text`, `.rodata`, `.data`, `.bss` jelas |
| Symbol table | `symbols.txt` | Symbol PIC/PIT/IRQ/stub terlihat |
| Undefined symbol | `undefined.txt` | Kosong |
| Disassembly | `disassembly.txt` | `lidt`, `iretq`, `outb`, `sti`, `hlt` terlihat |
| QEMU serial log | `m5-qemu.log` | Log M5 dan ticks periodik |
| Screenshot | PNG/JPG | Terminal QEMU/GDB/build |
| Analisis failure | Markdown/PDF laporan | Minimal 3 failure mode dan mitigasi |

---

## 19. Pertanyaan Analisis

1. Mengapa IRQ legacy tidak boleh tetap berada pada vector historis yang dapat bertabrakan dengan exception CPU?
2. Mengapa `sti` harus dipanggil setelah IDT, PIC, dan PIT siap?
3. Apa konsekuensi jika handler IRQ0 lupa mengirim EOI?
4. Apa perbedaan exception dengan error code dan exception tanpa error code pada layout stack?
5. Mengapa `volatile` dipakai untuk `g_ticks`, tetapi tidak cukup untuk sinkronisasi SMP jangka panjang?
6. Mengapa PIT/PIC masih dipakai pada M5 meskipun APIC/HPET/LAPIC timer lebih relevan untuk sistem modern?
7. Bagaimana cara membuktikan bahwa kernel tidak menarik dependency libc host?
8. Bagaimana QEMU gdbstub membantu membedakan hang sebelum `sti` dan hang setelah interrupt aktif?
9. Risiko keamanan apa yang muncul jika semua IRQ dibuka sebelum driver tersedia?
10. Bagaimana M5 menjadi fondasi untuk scheduler tick atau clocksource pada praktikum berikutnya?

---

## 20. Kriteria Lulus Praktikum

Mahasiswa dinyatakan lulus M5 jika memenuhi seluruh kriteria minimum berikut:

1. Repository dapat dibangun dari clean checkout.
2. Perintah build terdokumentasi dan dapat diulang.
3. Kernel ELF M5 berhasil dikompilasi dan dilink tanpa warning kritis.
4. `nm -u` kosong atau seluruh symbol eksternal dapat dijelaskan sebagai bagian runtime kernel yang memang tersedia.
5. Disassembly menunjukkan `lidt`, `iretq`, `outb`, `sti`, dan `hlt`.
6. IDT mencakup vector 0–47.
7. PIC diremap ke `0x20` dan `0x28`.
8. IRQ0 dibuka, IRQ lain tetap masked pada baseline M5.
9. PIT channel 0 dikonfigurasi 100 Hz atau frekuensi lain yang dijelaskan.
10. Serial log QEMU menunjukkan tick timer periodik.
11. Panic path tetap terbaca untuk exception fatal.
12. Mahasiswa menyerahkan log serial, screenshot, output `make grade`, dan analisis failure mode.
13. Perubahan Git sudah dicommit dengan pesan yang jelas.
14. Laporan memakai template praktikum seragam.

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator |
|---|---:|---|
| Kebenaran fungsional | 30 | Build lulus, IDT 0–47, PIC remap, PIT 100 Hz, IRQ0 tick, EOI benar |
| Kualitas desain dan invariants | 20 | Urutan `cli`/`sti` benar, mask policy jelas, trap frame konsisten, fail-closed |
| Pengujian dan bukti | 20 | Build log, QEMU log, GDB/audit ELF/disassembly, symbol table, `nm -u` |
| Debugging dan failure analysis | 10 | Mampu menjelaskan minimal 3 bug potensial dan diagnosisnya |
| Keamanan dan robustness | 10 | IRQ tidak dibuka sembarangan, exception fatal tetap panic, tidak ada dependency host |
| Dokumentasi/laporan | 10 | Laporan rapi, referensi IEEE, screenshot/log cukup, commit hash dicantumkan |

---

## 22. Template Laporan Praktikum M5

Gunakan template laporan praktikum seragam yang sudah dibuat sebelumnya. Untuk M5, isi minimal berikut wajib ada:

1. **Sampul**: judul praktikum, nama mahasiswa, NIM, kelas, dosen Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.
2. **Tujuan**: jelaskan target PIC/PIT/timer tick.
3. **Dasar teori ringkas**: exception vs IRQ, IDT, PIC remap, PIT divisor, EOI.
4. **Lingkungan**: Windows 11, WSL 2, distro, versi compiler, linker, QEMU, GDB, commit hash.
5. **Desain**: diagram alur IRQ0, struktur `trap_frame`, tabel vector.
6. **Langkah kerja**: file yang diubah, alasan teknis, perintah build.
7. **Hasil uji**: build log, QEMU serial log, `readelf`, `nm`, `objdump`, screenshot.
8. **Analisis**: mengapa tick muncul, bug yang ditemukan, solusi.
9. **Keamanan dan reliability**: mask IRQ, fail-closed exception, risiko interrupt storm.
10. **Kesimpulan**: status readiness M5 dan batasannya.
11. **Lampiran**: potongan kode penting, diff ringkas, log penuh, referensi.

---

## 23. Readiness Review M5

| Area | Status yang Diharapkan | Bukti |
|---|---|---|
| Build reproducibility | Siap uji statis | `make clean && make grade` lulus |
| Interrupt entry | Siap uji QEMU | `lidt`, `iretq`, `isr_stub_32` terbukti |
| PIC/PIT | Siap uji QEMU | Symbol dan log konfigurasi tersedia |
| Runtime tick | Siap demonstrasi praktikum jika QEMU log menunjukkan tick | `ticks=100`, `ticks=200` |
| Security baseline | Kandidat terbatas untuk tahap awal | IRQ selain IRQ0 tetap masked |
| Hardware fisik | Belum siap | Belum ada APIC/IOAPIC/HPET/hardware matrix |
| Scheduler integration | Belum siap | Tick belum dipakai preemption |

Kesimpulan readiness: jika seluruh kriteria terpenuhi, hasil M5 hanya boleh disebut **siap uji QEMU untuk external interrupt dan PIT timer awal**. Hasil ini belum siap digunakan sebagai timer production-grade, belum siap SMP, belum siap hardware fisik umum, dan belum siap menjadi scheduler preemption final.

---

## References

[1] Intel Corporation, *Intel 64 and IA-32 Architectures Software Developer's Manual*. Accessed: May 3, 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

[2] Intel Corporation, *8259A Programmable Interrupt Controller Datasheet*. Accessed: May 3, 2026. [Online]. Available: https://www.alldatasheet.com/datasheet-pdf/pdf/66107/INTEL/8259A.html

[3] Intel Corporation, *8254 Programmable Interval Timer Datasheet*. Accessed: May 3, 2026. [Online]. Available: https://www.alldatasheet.com/datasheet-pdf/pdf/66099/INTEL/8254.html

[4] QEMU Project, “Invocation,” *QEMU Documentation*. Accessed: May 3, 2026. [Online]. Available: https://www.qemu.org/docs/master/system/invocation.html

[5] QEMU Project, “GDB usage,” *QEMU Documentation*. Accessed: May 3, 2026. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html

[6] GNU Binutils, “LD: Linker Scripts,” *GNU Binutils Documentation*. Accessed: May 3, 2026. [Online]. Available: https://sourceware.org/binutils/docs/ld/Scripts.html

[7] LLVM Project, “LLD - The LLVM Linker,” *LLVM Documentation*. Accessed: May 3, 2026. [Online]. Available: https://lld.llvm.org/

[8] Microsoft, “Install WSL,” *Microsoft Learn*. Accessed: May 3, 2026. [Online]. Available: https://learn.microsoft.com/windows/wsl/install
