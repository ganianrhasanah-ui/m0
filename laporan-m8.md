# Laporan Praktikum M8 — Kernel Heap Awal dan Allocator Dinamis MCSOS

## 1. Sampul

- **Judul Praktikum** : Praktikum M8 — Kernel Heap Awal dan Allocator Dinamis
- **Nama Mahasiswa** : Gania Nurhasanah
- **NIM** : [25832071003]
- **Kelas** : [Isi Kelas]
- **Dosen** : Muhaemin Sidiq, S.Pd., M.Pd.
- **Program Studi** : Pendidikan Teknologi Informasi
- **Institusi** : Institut Pendidikan Indonesia
- **Tanggal** : 1 Juli 2026

---

# 2. Tujuan

Praktikum M8 bertujuan mengimplementasikan kernel heap allocator sederhana pada sistem operasi MCSOS menggunakan pendekatan free-list. Selain itu, praktikum ini bertujuan memahami hubungan antara Physical Memory Manager (PMM), Virtual Memory Manager (VMM), dan kernel heap, mengimplementasikan API allocator, melakukan pengujian host, serta memastikan implementasi memenuhi lingkungan freestanding tanpa ketergantungan terhadap pustaka standar C.

---

# 3. Dasar Teori Ringkas

Kernel heap merupakan mekanisme alokasi memori dinamis yang digunakan oleh kernel untuk menyediakan ruang bagi objek dengan ukuran yang tidak diketahui pada saat kompilasi. Kernel heap berada di atas Physical Memory Manager (PMM) dan Virtual Memory Manager (VMM). PMM bertanggung jawab mengelola frame fisik, sedangkan VMM bertanggung jawab memetakan alamat virtual ke alamat fisik. Kernel heap memanfaatkan ruang memori yang telah disediakan kedua subsistem tersebut.

Allocator yang digunakan pada praktikum ini menerapkan algoritma **first-fit**, yaitu memilih blok bebas pertama yang cukup besar untuk memenuhi permintaan alokasi. Jika ukuran blok lebih besar daripada kebutuhan, allocator melakukan **split** sehingga sisa ruang tetap dapat digunakan. Ketika blok dibebaskan, allocator melakukan **coalesce** untuk menggabungkan blok-blok bebas yang bersebelahan sehingga fragmentasi eksternal dapat dikurangi.

Alignment payload diterapkan agar alamat hasil alokasi memenuhi batas alignment tertentu sehingga akses memori tetap efisien. Implementasi juga mendeteksi **double free**, yaitu pembebasan blok yang telah dibebaskan sebelumnya. Seluruh implementasi dibuat pada lingkungan **freestanding C**, sehingga tidak bergantung pada fungsi pustaka standar seperti `malloc`, `free`, maupun `printf`.

---

# 4. Lingkungan

| Komponen | Keterangan |
|----------|------------|
| Sistem Operasi | Windows 11 |
| WSL | Ubuntu |
| Compiler | clang |
| Linker | ld.lld |
| Build System | GNU Make |
| Emulator | QEMU (belum digunakan pada pengujian M8) |
| Debugger | GDB |
| Commit Hash | `5cc26a6` |

---

# 5. Desain

## Hubungan PMM, VMM, dan Kernel Heap

```
+----------------------+
| Kernel Subsystem     |
+----------+-----------+
           |
           v
+----------------------+
| Kernel Heap (kmem)   |
+----------+-----------+
           |
           v
+----------------------+
| Virtual Memory (VMM) |
+----------+-----------+
           |
           v
+----------------------+
| Physical Memory PMM  |
+----------------------+
```

## Struktur `kmem_block`

Setiap blok heap terdiri atas metadata yang menyimpan ukuran blok, status bebas atau terpakai, serta pointer menuju blok berikutnya.

## Invariant Allocator

- Semua blok berada di dalam arena heap.
- Payload selalu aligned.
- Metadata antar blok tidak saling tumpang tindih.
- Double free ditolak.
- Pointer yang berada di luar arena tidak diterima.

## Error Path

Allocator mengembalikan nilai gagal apabila:

- ukuran alokasi tidak valid,
- pointer berada di luar arena,
- terjadi double free,
- arena tidak cukup besar.

## Batasan IRQ/SMP

Pada milestone ini allocator hanya digunakan pada konteks kernel biasa dan belum mendukung sinkronisasi terhadap interrupt maupun sistem multiprosesor (SMP).

---

# 6. Langkah Kerja

Langkah implementasi yang dilakukan adalah sebagai berikut.

1. Menambahkan header `include/mcsos/kmem.h`.
2. Mengimplementasikan allocator pada `kernel/mm/kmem.c`.
3. Menambahkan host unit test `tests/test_kmem.c`.
4. Menambahkan script audit `scripts/check_m8_kmem.sh`.
5. Memodifikasi `Makefile` agar mendukung target pengujian M8.
6. Mengintegrasikan `m8_heap_bootstrap()` pada `kernel/core/kmain.c`.
7. Menjalankan host unit test.
8. Melakukan audit object menggunakan `make m8-audit`.
9. Melakukan commit dan push ke GitHub.

---

# 7. Hasil Uji

| Uji | Perintah | Hasil | Bukti |
|-----|----------|--------|--------|
| Host Unit Test | `make m8-kmem-host-test` | **PASS** | `M8 kmem host tests: PASS` |
| Freestanding Compile | `make m8-audit` | **PASS** | Object berhasil dikompilasi |
| Unresolved Symbol | `nm -u` | **Kosong** | `build/m8/nm_u.txt` |
| ELF Audit | `readelf -h` | **ELF64 x86-64** | `build/m8/readelf_h.txt` |
| QEMU Log | Belum dilakukan | - | Integrasi QEMU belum tersedia pada Makefile |

---

# 8. Analisis

Implementasi allocator berhasil memenuhi seluruh pengujian utama pada lingkungan host. Host unit test menunjukkan seluruh fungsi allocator bekerja dengan benar, termasuk proses inisialisasi, alokasi, pembebasan, serta validasi struktur heap.

Audit menggunakan `nm -u` menunjukkan tidak terdapat unresolved symbol sehingga implementasi memenuhi lingkungan freestanding. Audit `readelf` memperlihatkan object bertipe ELF64 untuk arsitektur x86-64 sesuai target kernel.

Integrasi awal allocator telah dilakukan melalui `m8_heap_bootstrap()` pada `kmain()`. Akan tetapi, pengujian boot menggunakan QEMU belum dilakukan karena proyek belum menyediakan target pembuatan image bootable maupun target `make run` pada Makefile.

---

# 9. Keamanan dan Reliability

Implementasi memperhatikan beberapa aspek keamanan.

- Double free dideteksi dan ditolak.
- Pointer di luar arena heap tidak diterima.
- Fragmentasi dikurangi melalui mekanisme coalesce.
- Metadata divalidasi sebelum digunakan.
- Tidak terdapat ketergantungan terhadap libc.
- Allocator belum digunakan pada interrupt handler sehingga belum memerlukan mekanisme sinkronisasi.

---

# 10. Failure Modes dan Rollback

Failure mode yang dianalisis meliputi:

- header tidak aligned,
- double free,
- pointer di luar arena,
- metadata corruption,
- fragmentasi,
- unresolved symbol,
- page fault saat bootstrap heap,
- penggunaan allocator dari interrupt handler.

Apabila integrasi menyebabkan kernel gagal boot, prosedur rollback dilakukan dengan:

1. Menonaktifkan pemanggilan `m8_heap_bootstrap()`.
2. Memastikan kembali M7 berjalan normal.
3. Menjalankan host unit test.
4. Menjalankan audit object.
5. Menggunakan `git diff` untuk memeriksa perubahan.
6. Melakukan rollback penuh apabila diperlukan.

---

# 11. Kesimpulan

Praktikum M8 berhasil mengimplementasikan kernel heap allocator sederhana berbasis free-list. Seluruh host unit test berhasil dilewati, audit freestanding menunjukkan tidak terdapat unresolved symbol, serta object berhasil dikompilasi sebagai ELF64 x86-64. Implementasi telah diintegrasikan ke kernel melalui fungsi bootstrap sehingga siap menjadi dasar pengembangan allocator pada milestone berikutnya.

---

# 12. Readiness Review

**Status:** ✅ **Siap demonstrasi praktikum terbatas**

Alasan:

- Build kernel berhasil.
- Host unit test PASS.
- Audit freestanding PASS.
- `nm -u` kosong.
- Object ELF64 sesuai target.
- Perubahan telah di-commit (`5cc26a6`) dan di-push ke branch `main` pada GitHub.

---

# 13. Lampiran

## File yang Ditambahkan

- `include/mcsos/kmem.h`
- `kernel/mm/kmem.c`
- `tests/test_kmem.c`
- `scripts/check_m8_kmem.sh`

## File yang Dimodifikasi

- `Makefile`
- `kernel/core/kmain.c`

## Commit

```
5cc26a6
M8: implement kernel heap allocator
```

## Hasil Host Test

```
make m8-kmem-host-test

M8 kmem host tests: PASS
```

## Hasil Audit

```
make m8-audit
```

Berhasil menghasilkan:

- `nm_u.txt`
- `readelf_h.txt`
- `kmem.objdump.txt`

## Referensi

[1] Andrew S. Tanenbaum and Herbert Bos, *Modern Operating Systems*, 4th Edition, Pearson, 2015.

[2] Remzi H. Arpaci-Dusseau and Andrea C. Arpaci-Dusseau, *Operating Systems: Three Easy Pieces*, 2018.

[3] Intel Corporation, *Intel® 64 and IA-32 Architectures Software Developer's Manual*, Vol. 3, 2023.

[4] The ELF Specification, *Tool Interface Standard (TIS) Executable and Linking Format (ELF)*.

[5] Dokumentasi Praktikum Sistem Operasi MCSOS Milestone 8.
