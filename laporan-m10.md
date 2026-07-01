# Laporan Praktikum M10 — ABI System Call Awal dan Dispatcher Syscall MCSOS

## 1. Sampul

- **Judul Praktikum**: ABI System Call Awal dan Dispatcher Syscall MCSOS
- **Nama Mahasiswa**: Gania Nur Hasanah
- **NIM**: [Isi NIM]
- **Kelas**: [Isi Kelas]
- **Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.
- **Program Studi**: Pendidikan Teknologi Informasi
- **Institusi**: Institut Pendidikan Indonesia
- **Tanggal**: 2 Juli 2026
- **Repository**: https://github.com/ganianrhasanah-ui/m0
- **Commit Hash**: `b88c0d9`

---

# 2. Tujuan

Praktikum M10 bertujuan mengimplementasikan fondasi mekanisme system call pada kernel MCSOS. Target yang dicapai meliputi:

- Membuat ABI (Application Binary Interface) sederhana untuk system call.
- Mengimplementasikan dispatcher syscall di sisi kernel.
- Menambahkan syscall table dan callback operasi kernel.
- Mengimplementasikan validasi dasar terhadap user region.
- Menambahkan syscall entry stub berbasis assembly x86_64.
- Menyediakan host unit test untuk dispatcher syscall.
- Melakukan audit object hasil kompilasi menggunakan `nm`, `readelf`, dan `objdump`.

---

# 3. Dasar Teori Ringkas

## System Call

System call merupakan mekanisme agar program user dapat meminta layanan kernel secara aman. Semua akses terhadap perangkat keras, scheduler, memori, maupun resource kernel dilakukan melalui syscall.

## ABI (Application Binary Interface)

ABI menentukan bagaimana parameter syscall dikirim menggunakan register CPU, bagaimana nilai balik dikembalikan, serta bagaimana kernel dan user saling berinteraksi.

## IDT Vector

Interrupt Descriptor Table (IDT) berisi daftar interrupt dan exception handler. Pada M10 dipersiapkan penggunaan vector **0x80** sebagai syscall entry.

## Trap Frame

Trap frame merupakan kumpulan register CPU yang disimpan ketika interrupt atau exception terjadi sehingga eksekusi dapat dilanjutkan setelah handler selesai.

## Pointer Validation

Kernel tidak boleh mempercayai pointer dari user. Pointer harus divalidasi agar tidak menunjuk ke alamat kernel maupun alamat di luar region yang diizinkan.

## User Copy

User copy merupakan mekanisme penyalinan data antara user space dan kernel space dengan validasi alamat untuk mencegah kernel membaca alamat yang tidak valid.

## Error Convention

Setiap syscall mengembalikan nilai:

- nilai positif atau nol → sukses
- nilai negatif → error

Pendekatan ini menyerupai konvensi Linux.

---

# 4. Lingkungan

| Komponen | Keterangan |
|-----------|------------|
| OS Host | Windows 11 |
| WSL | Ubuntu 24.04 LTS |
| Compiler | Clang |
| Linker | LLD (ld.lld) |
| Binutils | readelf, objdump, nm |
| QEMU | Belum digunakan pada M10 |
| GDB | Belum digunakan |
| Target | x86_64-unknown-none-elf |
| Bootloader | Limine |
| Commit M9 | `59aabcf` |
| Commit M10 | `b88c0d9` |

---

# 5. Desain

## ABI Register

ABI syscall menggunakan register x86_64 sebagai berikut.

| Register | Fungsi |
|-----------|---------|
| RAX | Nomor syscall / return value |
| RDI | Argumen 1 |
| RSI | Argumen 2 |
| RDX | Argumen 3 |
| R10 | Argumen 4 |
| R8 | Argumen 5 |
| R9 | Argumen 6 |

---

## Syscall Table

Dispatcher M10 menyediakan beberapa operasi awal:

- Ping
- Get Tick
- Yield
- Exit
- Write Serial

Dispatcher akan memilih callback berdasarkan nomor syscall.

---

## Error Code

Dispatcher menggunakan nilai negatif sebagai kode error, misalnya:

- syscall tidak dikenal
- pointer tidak valid
- callback belum tersedia

---

## User Region

Kernel menyimpan rentang alamat user menggunakan struktur:

```
base  = 0x0000000000400000
limit = 0x0000800000000000
```

Rentang ini masih bersifat simulasi karena user mode belum tersedia.

---

## Callback Kernel

Kernel menyediakan callback:

- get_ticks()
- yield_current()
- exit_current()
- write_serial()

Callback tersebut dihubungkan melalui struktur `mcsos_syscall_ops_t`.

---

## Diagram Alur Syscall

```
User Program
      │
      ▼
 syscall ABI
      │
      ▼
syscall entry stub
      │
      ▼
dispatcher
      │
      ▼
callback kernel
      │
      ▼
return value
```

---

## Invariants

Implementasi mempertahankan beberapa invariant berikut.

- Dispatcher tidak menerima callback NULL.
- User region harus memiliki batas base dan limit yang valid.
- Dispatcher selalu mengembalikan nilai bertipe `int64_t`.
- Callback kernel hanya dipanggil jika telah diinisialisasi.

---

## Batasan

Pada M10 belum tersedia:

- User mode
- Ring 3
- Validasi page table
- Context switch user
- Scheduler preemptive
- User stack

---

# 6. Langkah Kerja

## Langkah 1

Membuat header syscall baru.

File:

```
include/mcsos/syscall.h
```

Header berisi:

- nomor syscall
- ABI
- deklarasi dispatcher
- struktur callback
- user region

---

## Langkah 2

Membuat implementasi dispatcher.

File:

```
kernel/syscall/syscall.c
```

Dispatcher bertugas:

- memeriksa nomor syscall
- memanggil callback yang sesuai
- mengembalikan error bila syscall tidak tersedia

---

## Langkah 3

Membuat syscall entry assembly.

File:

```
kernel/syscall/syscall_entry.S
```

Stub assembly digunakan sebagai fondasi interrupt syscall berbasis x86_64.

---

## Langkah 4

Menambahkan callback kernel.

File:

```
kernel/core/kmain.c
```

Kernel mendaftarkan callback melalui:

```
mcsos_syscall_init(&ops);
```

serta menginisialisasi user region menggunakan:

```
mcsos_syscall_set_user_region(...)
```

---

## Langkah 5

Menambahkan syscall smoke test langsung ke dispatcher.

Fungsi:

```
m10_syscall_smoke_direct()
```

Smoke test memanggil dispatcher secara langsung tanpa melewati interrupt.

---

## Langkah 6

Menambahkan host unit test.

File:

```
tests/test_syscall_host.c
```

Host test digunakan untuk memverifikasi seluruh dispatcher tanpa memerlukan QEMU.

---

## Langkah 7

Melakukan audit object hasil kompilasi menggunakan:

```
make m10-audit
```

Audit menghasilkan:

- nm
- readelf
- objdump
- checksum SHA256

yang digunakan untuk memastikan object freestanding telah terbentuk dengan benar.
# 7. Hasil Uji

| Uji | Perintah | Hasil | Bukti |
|------|----------|--------|--------|
| Host Test | `make m10-host-test` | Berhasil | `M10 syscall host tests passed` |
| Freestanding Compile | `make m10-freestanding` | Berhasil | Object `syscall.o` dan `syscall_entry.o` terbentuk |
| nm Audit | `make m10-audit` | Berhasil | `nm -u` tidak menampilkan unresolved symbol |
| readelf Audit | `readelf -h build/m10/m10_syscall_combined.o` | Berhasil | ELF64 Relocatable x86_64 |
| objdump Audit | `objdump -dr build/m10/m10_syscall_combined.o` | Berhasil | Symbol dispatcher dan syscall stub terlihat |
| Kernel Build | `make` | Berhasil | Kernel ELF berhasil dibuat tanpa warning |
| QEMU Smoke Test | Belum dilakukan | Belum tersedia | Tidak ada log serial |
| GDB Debug | Belum dilakukan | Tidak diperlukan | Host test telah lulus |

---

# 8. Analisis

Implementasi M10 berhasil menambahkan fondasi mekanisme system call pada kernel MCSOS. Dispatcher syscall telah dapat menerima nomor syscall, memeriksa callback yang tersedia, kemudian mengembalikan nilai sesuai ABI yang telah ditentukan.

Host unit test berhasil dijalankan dan seluruh pengujian dispatcher dinyatakan lulus. Hal ini menunjukkan bahwa implementasi dispatcher telah bekerja dengan benar tanpa bergantung pada lingkungan kernel sebenarnya.

Kompilasi freestanding juga berhasil dilakukan menggunakan target `x86_64-unknown-none-elf`, sehingga source dapat digunakan sebagai bagian dari kernel tanpa memerlukan library standar.

Audit object menggunakan `nm`, `readelf`, dan `objdump` menunjukkan bahwa object hasil kompilasi valid serta tidak memiliki unresolved symbol pada object gabungan.

Pada tahap ini syscall masih menggunakan callback sederhana (stub) untuk scheduler sehingga belum melakukan context switch sesungguhnya. Integrasi penuh dengan interrupt `int 0x80` dan user mode akan dilakukan pada milestone berikutnya.

---

# 9. Keamanan dan Reliability

Walaupun implementasi masih sederhana, beberapa mekanisme keamanan dasar telah diterapkan.

## Validasi Pointer User

Kernel menyimpan batas user region sehingga pointer dapat diperiksa sebelum digunakan. Pada M10 validasi masih menggunakan rentang alamat sederhana dan belum memanfaatkan page table.

## Invalid Syscall Number

Dispatcher akan mengembalikan error apabila nomor syscall tidak dikenali sehingga tidak terjadi eksekusi fungsi yang tidak valid.

## Overflow Address

Pemeriksaan dilakukan menggunakan batas bawah (base) dan batas atas (limit) untuk mencegah pointer keluar dari area user.

## Scheduler Reentrancy

Fungsi `yield_current()` masih berupa stub sehingga belum terjadi perpindahan thread yang sebenarnya. Hal ini menghindari kondisi race selama tahap awal implementasi.

## Lock Order

Belum terdapat mekanisme locking pada dispatcher sehingga deadlock belum menjadi isu pada M10.

## Reliability

Dispatcher hanya akan memanggil callback yang telah diinisialisasi melalui `mcsos_syscall_init()`. Callback yang belum tersedia akan menghasilkan error sehingga kernel tetap stabil.

---

# 10. Failure Modes dan Rollback

Selama implementasi ditemukan beberapa kegagalan.

## Error Kompilasi

Beberapa kali terjadi error akibat:

- fungsi berada di dalam `kmain()`
- variabel `ops` digunakan sebelum dideklarasikan
- penggunaan placeholder `...`
- deklarasi fungsi yang salah

Semua masalah berhasil diperbaiki dengan menyusun ulang urutan deklarasi.

---

## Rollback

Apabila implementasi menyebabkan kernel gagal boot, prosedur rollback yang digunakan adalah:

1. Menghapus smoke test sementara.
2. Mengembalikan perubahan pada integrasi IDT.
3. Menjalankan host test secara terpisah.
4. Mengembalikan repository ke commit M9 apabila diperlukan menggunakan Git.

Selama pengerjaan praktikum rollback penuh tidak diperlukan karena seluruh error dapat diperbaiki sebelum tahap build akhir.

---

# 11. Readiness Review

**Status:** **Siap Uji QEMU**

Berdasarkan hasil pengujian yang telah dilakukan:

| Area | Status |
|------|--------|
| Toolchain | Siap |
| Dispatcher | Siap |
| Freestanding Object | Siap |
| Host Test | Lulus |
| Entry `int 0x80` | Fondasi tersedia |
| User Pointer | Validasi dasar |
| Scheduler Callback | Stub |
| Ring 3 | Belum tersedia |
| SMP | Belum tersedia |
| Release | Belum siap |

Implementasi M10 telah memenuhi syarat untuk pengujian dispatcher kernel-side, namun belum mendukung user mode maupun syscall dari Ring 3.

---

# 12. Kesimpulan

Praktikum M10 berhasil mengimplementasikan ABI syscall awal pada kernel MCSOS.

Beberapa capaian utama adalah:

- implementasi dispatcher syscall
- implementasi callback kernel
- implementasi syscall entry stub
- host unit test berhasil
- kompilasi freestanding berhasil
- audit object berhasil
- kernel berhasil dibangun tanpa warning dengan `-Wall -Wextra -Werror`

Fitur yang belum tersedia meliputi user mode, syscall dari Ring 3, validasi page table, serta scheduler penuh. Semua fitur tersebut akan menjadi pengembangan pada milestone berikutnya.

---

# 13. Lampiran

## File yang Ditambahkan

```
include/mcsos/syscall.h
kernel/syscall/syscall.c
kernel/syscall/syscall_entry.S
tests/test_syscall_host.c
```

---

## File yang Dimodifikasi

```
Makefile
kernel/core/kmain.c
kernel/arch/x86_64/idt.c
```

---

## Log Host Test

```
$ make m10-host-test

M10 syscall host tests passed
```

---

## Log Audit

```
$ make m10-audit

nm -u build/m10/m10_syscall_combined.o
(readelf)
(objdump)

SHA256:
b7e7b969cd2bc87f3ff99ee3d50badbe6a8093eb88bba834b162a40a15aa8b4b
74442a9b7126463de5bd81c2b95c07176e154c4e392d757f8758a2ce290654d4
```

---

## Screenshot

Tambahkan screenshot berikut sebelum pengumpulan:

- Host test berhasil
- Build kernel berhasil
- Output `make m10-audit`
- `git log`
- Repository GitHub
- (Opsional) QEMU apabila telah dijalankan

---

# 22. Readiness Review

| Area | Status | Bukti |
|------|--------|-------|
| Toolchain | Siap | Build berhasil |
| Dispatcher | Siap | Host test lulus |
| Freestanding Object | Siap | `readelf`, `nm`, `objdump` |
| Entry `int 0x80` | Siap tahap awal | Stub assembly tersedia |
| User Pointer | Belum security complete | Validasi range sederhana |
| Scheduler Syscall | Siap terbatas | Callback stub |
| Ring 3 | Belum siap | Di luar scope M10 |
| SMP | Belum siap | Di luar scope M10 |
| Release | Belum siap | Menunggu M11 |

**Keputusan Readiness:** Implementasi M10 dinyatakan **siap uji QEMU untuk dispatcher syscall kernel-side** namun belum siap untuk penggunaan user mode maupun produksi.

---

# 23. Rencana Lanjutan M11

Pengembangan berikutnya pada M11 meliputi:

- implementasi Ring 3
- GDT user segment
- TSS kernel stack
- user page table
- syscall dari user mode
- return-to-user
- user process pertama
- validasi pointer berbasis page table
- pengujian privilege boundary antara user dan kernel

---

# Referensi

1. Intel Corporation. *Intel® 64 and IA-32 Architectures Software Developer Manuals*. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

2. x86 psABIs Project. *x86-64 psABI*. https://gitlab.com/x86-psABIs/x86-64-ABI

3. QEMU Project. *GDB Usage*. https://qemu-project.gitlab.io/qemu/system/gdb.html

4. LLVM Project. *Clang Command Line Reference*. https://clang.llvm.org/docs/ClangCommandLineReference.html

5. Linux Kernel Documentation. *Adding a New System Call*. https://www.kernel.org/doc/html/latest/process/adding-syscalls.html

6. Linux Kernel Documentation. *Lock Types and Their Rules*. https://www.kernel.org/doc/html/latest/locking/locktypes.html
