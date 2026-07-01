# Praktikum M9 — Kernel Thread, Scheduler, dan Context Switch x86_64 pada MCSOS

---

## 19.1 Sampul

**Judul Praktikum**

Praktikum M9 — Kernel Thread, Scheduler, dan Context Switch x86_64 pada MCSOS

**Nama Mahasiswa** : Gania Nurhasanah

**NIM** :(25832071003)

**Kelas** : 1a

**Kelompok** : Tidak berlaku

**Dosen Pengampu** : Muhaemin Sidiq, S.Pd., M.Pd.

**Program Studi** : Pendidikan Teknologi Informasi

**Institut** : Institut Pendidikan Indonesia

---

# 19.2 Tujuan

Praktikum M9 bertujuan mengimplementasikan kernel thread scheduler sederhana berbasis cooperative scheduling pada sistem operasi MCSOS. Scheduler yang dikembangkan menggunakan algoritma Round Robin berbasis FIFO sehingga beberapa thread kernel dapat dijalankan secara bergantian melalui mekanisme context switch.

Secara teknis praktikum ini bertujuan untuk:

1. Mengimplementasikan Thread Control Block (TCB) sesuai kontrak desain.
2. Mengimplementasikan scheduler kernel single-core.
3. Mengimplementasikan FIFO ready queue.
4. Mengimplementasikan cooperative scheduler melalui fungsi `mcsos_sched_yield()`.
5. Mengimplementasikan context switch x86_64 menggunakan assembly.
6. Melakukan host unit test terhadap scheduler.
7. Melakukan audit ELF menggunakan `nm`, `readelf`, dan `objdump`.
8. Memastikan object freestanding dapat dikompilasi tanpa unresolved symbol.
9. Menyiapkan integrasi scheduler dengan kernel MCSOS sebagai dasar implementasi multitasking pada milestone berikutnya.

Secara konseptual praktikum ini juga bertujuan memahami hubungan antara:

- Thread Control Block
- Kernel Stack
- Scheduler
- Context Switch
- Cooperative Scheduling
- ABI x86_64
- Runqueue Management

sebagai fondasi menuju scheduler preemptive pada milestone selanjutnya.

---

# 19.3 Dasar Teori Ringkas

## Thread Control Block (TCB)

Thread Control Block merupakan struktur data utama yang menyimpan seluruh informasi sebuah thread kernel. Informasi tersebut meliputi state thread, pointer stack, context CPU, queue pointer, magic number validasi, dan informasi scheduler lainnya.

TCB menjadi identitas setiap thread yang dikelola scheduler.

---

## Kernel Stack

Setiap thread kernel memiliki stack sendiri.

Kernel stack digunakan untuk:

- pemanggilan fungsi
- penyimpanan local variable
- penyimpanan register
- trap frame
- context switch

Setiap stack harus memiliki alignment 16-byte agar sesuai dengan ABI x86_64.

---

## CPU Context

CPU context merupakan kumpulan register yang harus dipertahankan ketika scheduler melakukan perpindahan thread.

Pada implementasi M9 register yang disimpan meliputi register callee-saved sesuai ABI x86_64 yaitu:

- RBX
- RBP
- R12
- R13
- R14
- R15
- Stack Pointer (RSP)
- Return Address (RIP)

Dengan penyimpanan register tersebut maka eksekusi thread dapat dilanjutkan kembali tanpa kehilangan state sebelumnya.

---

## Scheduler State Machine

Thread dapat berada pada beberapa state utama yaitu:

- READY
- RUNNING
- BLOCKED
- TERMINATED

Pada cooperative scheduler perpindahan state dilakukan secara eksplisit melalui fungsi scheduler dan bukan melalui interrupt timer.

---

## Round Robin FIFO

Scheduler M9 menggunakan algoritma FIFO Round Robin.

Alur dasarnya adalah:

1. Thread aktif memanggil `yield()`.
2. Scheduler memasukkan thread lama ke belakang runqueue.
3. Scheduler memilih thread paling depan.
4. Context switch dilakukan.
5. Thread baru mulai berjalan.

Pendekatan ini sederhana, deterministik, dan sesuai untuk kernel thread awal.

---

## ABI x86_64

ABI menentukan register mana yang harus dipertahankan selama pemanggilan fungsi.

Context switch hanya menyimpan register callee-saved sehingga implementasi menjadi lebih efisien dibandingkan menyimpan seluruh register CPU.

---

## Cooperative Scheduler

Scheduler cooperative hanya berpindah thread ketika thread aktif secara sukarela memanggil scheduler.

Keuntungan:

- implementasi sederhana
- tidak memerlukan interrupt ownership
- mudah diverifikasi

Kelemahan:

- satu thread dapat memonopoli CPU apabila tidak pernah memanggil `yield()`.

Scheduler preemptive akan diperkenalkan pada milestone berikutnya menggunakan timer interrupt.

---

# 19.4 Lingkungan Praktikum

| Item | Versi / Nilai |
|------|---------------|
| Sistem Operasi | Windows 11 |
| WSL | Ubuntu 24.04 LTS |
| Kernel WSL | Linux WSL2 |
| Compiler | Clang LLVM |
| Linker | LLD |
| QEMU | qemu-system-x86_64 |
| Debugger | GNU GDB 15.1 |
| Target Arsitektur | x86_64 |
| Repository | mcsos |
| Branch | main |
| Commit Hash | 43481d8 |

---

# 19.5 Desain

## Desain Scheduler

Scheduler menggunakan model single-core cooperative scheduler.

Komponen utama terdiri atas:

- Scheduler
- Ready Queue
- Current Thread
- Idle Thread

Ready queue menggunakan linked list FIFO.

Thread yang sedang RUNNING tidak berada di dalam runqueue.

---

## Struktur Thread

Setiap thread memiliki:

- Context
- Stack Pointer
- Magic Number
- Thread State
- Queue Pointer

Thread dipersiapkan menggunakan fungsi:

```
mcsos_thread_prepare()
```

yang melakukan validasi alignment stack serta inisialisasi context awal.

---

## Invariant Scheduler

Invariant yang dijaga selama implementasi adalah:

- Thread RUNNING tidak berada pada ready queue.
- Thread READY hanya muncul satu kali pada ready queue.
- runnable_count selalu sesuai jumlah thread READY.
- Queue tidak boleh membentuk cycle.
- Idle thread selalu valid.
- Context switch tidak melakukan alokasi heap.

---

## Diagram State Thread

```text
NEW
 |
 V
READY
 |
 V
RUNNING
 | \
 |  \
 |   \
 |    \
 |     V
 |   BLOCKED
 |      |
 |      V
 +---- READY

RUNNING
 |
 V
TERMINATED
```

---

## Alur Scheduler

```text
Thread Running
      |
      V
mcsos_sched_yield()
      |
      V
enqueue(current)
      |
      V
pick_next()
      |
      V
mcsos_context_switch()
      |
      V
Next Thread Running
```

Context switch dilakukan melalui file assembly:

```
kernel/arch/x86_64/context_switch.S
```

---

# 19.6 Langkah Kerja

## Persiapan

Repository dibersihkan terlebih dahulu kemudian dilakukan build ulang.

```bash
make clean
make
```

Build berhasil menghasilkan:

- kernel.elf
- kernel.map
- kernel.disasm.txt
- kernel.readelf.header.txt
- kernel.syms.txt

---

## Implementasi Scheduler

File yang dibuat:

```
kernel/sched/mcsos_thread.c
```

Berisi implementasi:

- scheduler init
- enqueue
- pick next
- yield
- tick
- validate
- ready count
- thread prepare

---

## Implementasi Context Switch

File assembly:

```
kernel/arch/x86_64/context_switch.S
```

Context switch menyimpan register callee-saved sesuai ABI x86_64 kemudian memulihkan register thread tujuan sebelum melakukan transfer eksekusi.

---

## Host Unit Test

Scheduler diuji menggunakan:

```bash
make m9-host-test
```

Hasil:

```
M9 scheduler host unit test PASS
```

---

## Freestanding Build

Scheduler kemudian dikompilasi sebagai object kernel menggunakan:

```bash
make m9-freestanding
```

Object berhasil dibentuk:

```
build/m9/m9_scheduler_combined.o
```

---

## Audit Object

Audit dilakukan menggunakan:

```bash
make m9-audit
```

Audit menghasilkan:

- nm
- readelf
- objdump
- sha256sum

Seluruh artefak berhasil dibuat.

---

## Integrasi Kernel

Scheduler berhasil terhubung ke kernel sehingga symbol berikut muncul pada binary:

- mcsos_scheduler_init
- mcsos_sched_yield
- mcsos_sched_tick
- mcsos_context_switch
- mcsos_thread_prepare

Symbol diverifikasi menggunakan:

```bash
nm build/kernel.elf | grep mcsos
```

Seluruh symbol ditemukan pada kernel ELF.
---

# 19.7 Hasil Uji

Pengujian dilakukan menggunakan host unit test, kompilasi freestanding, audit ELF, audit symbol, audit disassembly, serta integrasi scheduler ke dalam kernel MCSOS. Seluruh pengujian dilakukan pada lingkungan WSL2 menggunakan toolchain LLVM/Clang.

| Uji | Perintah | Hasil | Bukti |
|------|----------|-------|--------|
| Host Unit Test | `make m9-host-test` | **PASS** | `build/m9/test_scheduler.log` menampilkan `M9 scheduler host unit test PASS` |
| Freestanding Compile | `make m9-freestanding` | **PASS** | `build/m9/m9_scheduler_combined.o` berhasil dibuat |
| Undefined Symbol Audit | `nm -u build/m9/m9_scheduler_combined.o` | **PASS** | Tidak terdapat unresolved symbol |
| ELF Audit | `readelf -h build/m9/m9_scheduler_combined.o` | **PASS** | ELF64 Relocatable x86_64 |
| Disassembly Audit | `objdump -d build/m9/m9_scheduler_combined.o` | **PASS** | Symbol `mcsos_context_switch` ditemukan |
| SHA-256 Audit | `sha256sum` | **PASS** | Hash artefak berhasil dibuat |
| Kernel Integration | `make` | **PASS** | Symbol scheduler muncul pada `kernel.elf` |
| QEMU Smoke Test | QEMU Serial | **BELUM DILAKUKAN** | Repository belum menghasilkan bootable ISO |
| GDB Context Switch | GDB | **BELUM DILAKUKAN** | Tidak dapat dilakukan karena belum tersedia image boot |

---

## Hasil Host Unit Test

Host unit test berhasil dijalankan menggunakan target Makefile berikut.

```bash
make m9-host-test
```

Output:

```text
M9 scheduler host unit test PASS
```

Hasil ini menunjukkan bahwa:

- scheduler berhasil diinisialisasi
- enqueue berjalan benar
- dequeue berjalan benar
- FIFO scheduler bekerja sesuai desain
- invariant scheduler tetap terjaga

---

## Hasil Freestanding Build

Scheduler kemudian dikompilasi sebagai object freestanding.

Perintah:

```bash
make m9-freestanding
```

Output berhasil menghasilkan object:

```
build/m9/m9_scheduler_combined.o
```

Object tersebut selanjutnya digunakan pada proses audit.

---

## Audit Undefined Symbol

Perintah:

```bash
nm -u build/m9/m9_scheduler_combined.o
```

Hasil:

```
Tidak terdapat unresolved symbol.
```

Hal ini menunjukkan seluruh dependency scheduler telah terpenuhi.

---

## Audit ELF

Perintah:

```bash
readelf -h build/m9/m9_scheduler_combined.o
```

Ringkasan hasil:

| Properti | Nilai |
|-----------|-------|
| Class | ELF64 |
| Type | REL (Relocatable) |
| Machine | Advanced Micro Devices X86-64 |
| ABI | System V |

Audit menunjukkan bahwa object scheduler sesuai dengan target arsitektur MCSOS.

---

## Audit Disassembly

Audit dilakukan menggunakan:

```bash
objdump -d build/m9/m9_scheduler_combined.o
```

Hasil menunjukkan symbol:

```
mcsos_context_switch
```

beserta instruksi:

- jmp
- ret
- hlt

yang sesuai dengan implementasi scheduler.

---

## SHA-256 Artefak

Checksum artefak berhasil dibuat.

```
e3a4a12942237e6eadc8b632535324df345e7e7f6665fb49b062a13d3369c0ac
build/m9/m9_host_test

ee820d4eca8430330fcbc986822484d8cc6b40ef766dc4b91b8cf49b09db6788
build/m9/m9_scheduler_combined.o
```

Checksum digunakan untuk memastikan artefak yang diuji identik dengan artefak yang dilampirkan.

---

## Integrasi Kernel

Setelah build kernel dilakukan kembali, scheduler berhasil terintegrasi.

Verifikasi menggunakan:

```bash
nm build/kernel.elf | grep mcsos
```

menunjukkan symbol:

```
mcsos_scheduler_init
mcsos_sched_enqueue
mcsos_sched_pick_next
mcsos_sched_tick
mcsos_sched_yield
mcsos_sched_validate
mcsos_context_switch
mcsos_thread_prepare
mcsos_thread_block_current
mcsos_thread_mark_ready
```

Seluruh symbol scheduler berhasil ditemukan pada kernel ELF.

---

## QEMU Smoke Test

Pada repository praktikum ini belum tersedia proses pembuatan bootable ISO sehingga pengujian runtime menggunakan QEMU belum dapat dilakukan.

Akibatnya evidence berikut belum tersedia:

- qemu_m9.log
- scheduler runtime log
- thread switching runtime

Bagian ini akan menjadi tahap validasi ketika pipeline boot image telah tersedia.

---

## Debug Menggunakan GDB

Percobaan menggunakan GDB telah dilakukan.

Debugger berhasil membaca file:

```
build/kernel.elf
```

Namun binary dibangun tanpa debug symbol (`No debugging symbols found`) dan repository belum menghasilkan image bootable sehingga breakpoint terhadap proses context switch belum dapat diverifikasi pada runtime.

---

# 19.8 Analisis

Implementasi scheduler berhasil memenuhi tujuan utama milestone M9 yaitu menyediakan scheduler cooperative sederhana untuk kernel thread. Host unit test menunjukkan seluruh operasi dasar scheduler berjalan sesuai rancangan, sedangkan audit object memastikan implementasi memenuhi persyaratan freestanding x86_64.

Ready queue berhasil diimplementasikan menggunakan struktur FIFO sehingga thread dipilih berdasarkan urutan kedatangan. Pendekatan ini menghasilkan perilaku yang sederhana, mudah diverifikasi, dan cocok digunakan sebagai dasar scheduler kernel awal.

Implementasi context switch menggunakan assembly x86_64 hanya menyimpan register callee-saved sesuai ABI System V. Pendekatan tersebut lebih efisien dibandingkan menyimpan seluruh register CPU dan sesuai dengan kontrak ABI.

Selama proses implementasi ditemukan beberapa kendala.

Pertama, repository belum menghasilkan bootable ISO sehingga QEMU tidak dapat dijalankan untuk melakukan validasi runtime scheduler. Akibatnya pengujian hanya dapat dilakukan sampai tahap build dan audit object.

Kedua, kernel dibangun tanpa debug symbol sehingga proses debugging menggunakan GDB terbatas pada pemeriksaan symbol tanpa dapat melakukan source level debugging.

Walaupun demikian, seluruh bukti build menunjukkan bahwa implementasi scheduler telah berhasil diintegrasikan ke kernel dan seluruh symbol penting muncul pada kernel ELF.

Batasan implementasi M9 adalah:

- hanya mendukung single-core
- menggunakan cooperative scheduling
- belum mendukung timer preemption
- belum mendukung user process
- belum mendukung SMP
- belum mendukung virtual memory per-process

Dengan demikian implementasi ini layak dijadikan dasar untuk pengembangan scheduler preemptive pada milestone berikutnya.

---

# 19.9 Keamanan dan Reliability

Implementasi scheduler masih berada sepenuhnya di dalam kernel sehingga ancaman utama berasal dari bug internal kernel.

Risiko pertama adalah stack corruption apabila dua thread menggunakan stack yang saling overlap atau alignment stack tidak sesuai ABI. Risiko ini dapat menyebabkan page fault maupun triple fault ketika context switch dilakukan.

Risiko kedua adalah context corruption apabila register callee-saved tidak dipulihkan dengan benar. Kondisi tersebut dapat menyebabkan eksekusi kembali ke alamat yang salah atau kerusakan data lokal thread.

Risiko ketiga adalah double enqueue, yaitu satu thread muncul lebih dari satu kali pada ready queue. Masalah ini dapat menyebabkan scheduler kehilangan invariant FIFO dan menghasilkan siklus (cycle) pada linked list.

Risiko berikutnya adalah lost wakeup apabila transisi state READY dan BLOCKED tidak dilakukan secara atomik. Pada M9 risiko ini masih dibatasi karena scheduler menggunakan cooperative scheduling.

Scheduler juga belum dirancang untuk dipanggil dari interrupt secara langsung sehingga race condition antara interrupt dan scheduler belum menjadi target implementasi.

Selain itu, M9 belum memiliki boundary antara kernel dan user mode sehingga belum dapat memberikan perlindungan terhadap akses memori pengguna ataupun privilege escalation. Seluruh thread masih berjalan pada privilege kernel.

---

# 19.10 Kesimpulan

Praktikum M9 berhasil mengimplementasikan kernel scheduler cooperative berbasis FIFO Round Robin pada sistem operasi MCSOS.

Thread Control Block, scheduler, ready queue, thread preparation, cooperative yield, serta context switch assembly berhasil dibangun dan diuji menggunakan host unit test. Audit menggunakan nm, readelf, objdump, serta SHA-256 juga menunjukkan bahwa object yang dihasilkan memenuhi target ELF64 x86_64 dan tidak memiliki unresolved symbol.

Integrasi scheduler ke kernel berhasil ditunjukkan melalui munculnya seluruh symbol scheduler pada kernel ELF.

Pengujian runtime menggunakan QEMU dan debugging penuh menggunakan GDB belum dapat dilakukan karena repository belum menghasilkan bootable image serta binary belum dibangun menggunakan debug symbol.

Secara keseluruhan implementasi M9 dapat dinyatakan **siap sebagai dasar pengembangan scheduler kernel single-core berbasis cooperative scheduling**, namun belum dapat diklaim sebagai scheduler produksi ataupun scheduler preemptive.

Milestone berikutnya akan memperluas implementasi menuju mekanisme sinkronisasi, timer preemption, serta pengelolaan proses yang lebih kompleks.

---

# 19.11 Lampiran

## Artefak Build

- build/kernel.elf
- build/kernel.map
- build/kernel.syms.txt
- build/kernel.readelf.header.txt
- build/kernel.disasm.txt
- build/m9/m9_scheduler_combined.o
- build/m9/test_scheduler.log
- build/m9/readelf_header.log
- build/m9/nm_undefined.log
- build/m9/objdump_key.log
- build/m9/sha256.log

---

## Potongan Symbol Kernel

```
mcsos_scheduler_init
mcsos_sched_enqueue
mcsos_sched_pick_next
mcsos_sched_tick
mcsos_sched_yield
mcsos_context_switch
mcsos_thread_prepare
```

---

## Readiness Review

| Kriteria | Status | Bukti |
|----------|--------|--------|
| Host Test | Lulus | test_scheduler.log |
| Freestanding Build | Lulus | object berhasil dibuat |
| Symbol Audit | Lulus | nm -u kosong |
| ELF Audit | Lulus | ELF64 x86_64 |
| Disassembly Audit | Lulus | symbol context switch ditemukan |
| Integrasi Kernel | Lulus | symbol scheduler muncul pada kernel ELF |
| QEMU Runtime | Belum Diverifikasi | bootable ISO belum tersedia |
| GDB Runtime | Belum Diverifikasi | image boot belum tersedia |
| Security Boundary | Terbatas | kernel only |
| SMP Readiness | Tidak termasuk ruang lingkup | single-core |
| Production Readiness | Belum | scheduler pembelajaran |

### Kesimpulan Readiness

Implementasi M9 dinyatakan siap sebagai dasar scheduler kernel single-core berbasis cooperative scheduling. Scheduler telah memenuhi seluruh pengujian build dan audit object, namun validasi runtime menggunakan QEMU masih memerlukan pipeline bootable image pada repository.

---

# 21. Referensi

1. Intel Corporation. *Intel® 64 and IA-32 Architectures Software Developer Manuals.*

2. x86 psABIs. *System V AMD64 ABI.*

3. QEMU Project. *System Emulation Documentation.*

4. LLVM Project. *Clang Command Line Reference.*

5. GNU Project. *GNU Linker Documentation.*

6. Linux Kernel Documentation. *Completely Fair Scheduler (CFS).*
