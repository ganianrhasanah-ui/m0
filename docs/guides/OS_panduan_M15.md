# OS_panduan_M15.md

# Panduan Praktikum M15 - Filesystem Persistent Minimal MCSFS1, On-Disk Superblock/Inode/Directory, dan Fsck-Lite pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M15  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: siap uji QEMU untuk filesystem persistent minimal berbasis block device layer M14. Status ini bukan bukti filesystem crash-consistent penuh, bukan bukti kompatibilitas POSIX penuh, bukan bukti aman terhadap power-loss pada perangkat nyata, dan bukan bukti siap produksi.

---

## 1. Ringkasan Praktikum

Praktikum M15 melanjutkan M14. Sampai M14, MCSOS telah memiliki block device layer, RAM block driver, dan buffer cache minimal. Kelemahan utama M14 adalah storage belum mempunyai format filesystem persistent yang dapat memetakan nama file, inode, dan blok data. M15 memperkenalkan **MCSFS1**, yaitu filesystem pendidikan yang sangat kecil, root-only, menggunakan superblock, inode bitmap, block bitmap, inode table, root directory block, dan blok data langsung.

MCSFS1 sengaja tidak meniru seluruh kompleksitas ext2/ext4. Akan tetapi, desainnya mengambil gagasan inti yang sama: filesystem persistent membutuhkan superblock untuk konfigurasi, inode untuk metadata objek, directory entry untuk pemetaan nama ke inode, dan bitmap untuk alokasi resource. Dokumentasi Linux ext2 menjelaskan bahwa ext2 memakai konsep block, inode, directory, block bitmap, inode bitmap, inode table, dan superblock sebagai metadata utama [2]. Dokumentasi Linux VFS juga menjelaskan bahwa VFS memberi abstraksi filesystem ke userspace dan memungkinkan berbagai implementasi filesystem berdampingan; objek pentingnya meliputi superblock, inode, dentry, dan file [1]. M15 memakai konsep tersebut dalam versi jauh lebih kecil agar dapat dibangun dan diuji secara bertahap.

M15 belum memperkenalkan journaling, ordered mode, copy-on-write, fsync POSIX penuh, multi-directory, permission model, hard link, symbolic link, page cache, atau recovery setelah power-loss arbitrer. Fokusnya adalah memastikan mahasiswa memahami struktur on-disk, mounting, pembuatan file, pembacaan file, penulisan file, penghapusan file, dan fsck-lite berbasis invariant. Linux buffer-head documentation menunjukkan pentingnya dirty buffer, read block, refcount, dirty marking, dan flush untuk metadata/filesystem blocks [3]; M15 menyederhanakan gagasan tersebut menjadi flush eksplisit pada operasi metadata dan data.

QEMU tetap menjadi target runtime untuk smoke test kernel, sedangkan host unit test dipakai untuk memverifikasi algoritma filesystem tanpa bergantung pada boot path. QEMU gdbstub dapat digunakan dengan `-s -S` agar guest berhenti sampai GDB tersambung, sehingga debugging low-level dapat memeriksa register, memori, dan breakpoint [4]. Kompilasi freestanding memakai Clang `-ffreestanding`, yang menyatakan environment kompilasi freestanding [5]. Audit object memakai GNU Binutils seperti `nm`, `readelf`, dan `objdump` untuk memeriksa simbol, header ELF, dan disassembly [6].

Keberhasilan M15 tidak boleh ditulis sebagai "filesystem sudah aman". Kriteria minimum M15 adalah: readiness M0-M14 terdokumentasi, MCSFS1 dapat dikompilasi sebagai host test dan freestanding object x86_64, host unit test lulus, linked relocatable object tidak memiliki undefined symbol, ELF header tervalidasi sebagai ELF64 relocatable x86-64, disassembly dan checksum tersimpan, serta integrasi QEMU dapat diuji ulang pada WSL 2 mahasiswa.

---

## 2. Assumptions, Scope, and Target Matrix

| Aspek | Keputusan M15 |
|---|---|
| Architecture | x86_64 long mode |
| Host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Kernel model | Monolithic teaching kernel |
| Bahasa | C17 freestanding untuk kernel; C17 hosted untuk host unit test |
| Toolchain | Clang target `x86_64-elf`, GNU `ld`, `nm`, `readelf`, `objdump`, `sha256sum`, `make` |
| Target triple | `x86_64-elf` |
| ABI | Kernel-internal filesystem ABI; belum stable public ABI |
| Boot artifact | Menggunakan image/ISO hasil M2-M14; M15 menambah object filesystem yang dapat ditautkan ke kernel |
| Subsystem utama | MCSFS1 persistent filesystem minimal, root directory, inode table, bitmap allocation, fsck-lite |
| Storage model | Block device M14; host test memakai RAM-backed block device 128 block x 512 byte |
| Filesystem impact | Menggantikan RAMFS volatil sebagai latihan persistent storage minimal; RAMFS tetap dapat dipakai sebagai fallback |
| Crash model | Clean shutdown atau explicit flush. Power-loss arbitrer belum dijamin. |
| Concurrency | Single-core educational baseline; operasi MCSFS1 diasumsikan dilindungi oleh VFS/filesystem lock eksternal bila dipakai bersamaan |
| Security posture | Validasi nama, range, ukuran file, dan metadata internal; belum DAC/ACL/capability penuh |

---

## 2A. Goals and Non-goals

**Goals M15** adalah menyediakan format filesystem persistent minimal bernama MCSFS1, menghubungkannya secara konseptual dengan VFS M13 dan block layer M14, menulis operasi `format`, `mount`, `fsck`, `create`, `write`, `read`, dan `unlink`, serta membuktikan operasi tersebut melalui host unit test dan freestanding object audit.

**Non-goals M15** adalah kompatibilitas ext2/ext4, POSIX penuh, directory bertingkat, permission DAC, ACL, hard link, symbolic link, journaling, crash recovery penuh, fsync POSIX penuh, quota, xattr, mmap, page cache, writeback daemon, driver disk nyata, virtio-blk, AHCI, NVMe, DMA, dan production readiness.

---

## 2B. Assumptions and Target

Assumptions and target M15 adalah: filesystem role berupa teaching filesystem; interface target berupa VFS-like internal API; storage model berupa block device M14; crash model terbatas pada clean shutdown/flush eksplisit; compatibility target berupa custom MCSFS1, bukan ext2; dan evidence baseline berupa host unit test, ELF audit, checksum, dan QEMU smoke test. Asumsi ini wajib dicatat dalam laporan karena klaim M15 hanya valid pada cakupan tersebut.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M15, mahasiswa mampu:

1. Menjelaskan hubungan VFS, block device, buffer cache, dan filesystem persistent.
2. Mendesain superblock, inode bitmap, block bitmap, inode table, root directory, dan direct data block.
3. Menjelaskan invariant filesystem: magic/version valid, root inode valid, bitmap konsisten, LBA dalam range, file size tidak melewati direct block limit, dan directory entry menunjuk inode aktif.
4. Mengimplementasikan `format`, `mount`, `fsck`, `create`, `write`, `read`, dan `unlink` pada filesystem root-only.
5. Menguji operasi filesystem dengan RAM-backed block device pada host test.
6. Mengompilasi source filesystem menjadi object freestanding x86_64 tanpa dependensi libc tersembunyi.
7. Menghasilkan bukti audit `nm`, `readelf`, `objdump`, `sha256sum`, host test, dan QEMU smoke log.
8. Menganalisis failure modes seperti corrupt superblock, bitmap mismatch, out-of-range LBA, duplicate name, stale metadata, dan no-space condition.

---

## 4. Prasyarat Teori

Mahasiswa harus menguasai materi berikut sebelum mengerjakan M15.

1. **VFS**: lapisan abstraksi yang memisahkan syscall/file descriptor dari implementasi filesystem konkret.
2. **Block device**: perangkat atau abstraksi yang membaca/menulis data dalam unit blok tetap.
3. **Superblock**: metadata global filesystem, berisi magic number, versi, block size, jumlah blok, lokasi bitmap, lokasi inode table, dan lokasi root directory.
4. **Inode**: metadata objek filesystem. Pada M15, inode menyimpan mode, link count, ukuran file, dan direct block array.
5. **Directory entry**: pemetaan nama file ke nomor inode. Pada M15, hanya root directory yang didukung.
6. **Bitmap allocator**: struktur bit untuk menandai inode dan block yang bebas atau terpakai.
7. **Direct block**: pointer langsung dari inode ke block data. M15 belum memakai indirect block.
8. **Fsck-lite**: pemeriksaan konsistensi minimum, bukan repair penuh.
9. **Crash consistency**: kemampuan mempertahankan invariant setelah crash. M15 hanya mendokumentasikan risiko dan belum menjamin pemulihan penuh.
10. **Freestanding C**: source kernel tidak boleh memakai `malloc`, `printf`, hosted libc, atau runtime yang tidak tersedia di kernel.

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M15 |
|---|---|
| `osdev-general` | Readiness gate, roadmap, acceptance criteria, dan integrasi tahap M0-M14 ke M15. |
| `osdev-01-computer-foundation` | State machine, invariant bitmap, batas file size, dan proof obligation. |
| `osdev-02-low-level-programming` | C freestanding, object ELF, pointer ownership, overflow check, dan audit undefined symbol. |
| `osdev-03-computer-and-hardware-architecture` | Model block device, LBA, block size, dan batas menuju driver hardware nyata. |
| `osdev-04-kernel-development` | Integrasi VFS, syscall file I/O, error path, dan observability kernel. |
| `osdev-05-filesystem-development` | Desain filesystem persistent, inode, directory, bitmap, fsck, dan crash-risk analysis. |
| `osdev-07-os-security` | Validasi input, nama file, boundary, dan risiko metadata corruption. |
| `osdev-08-device-driver-development` | Hubungan filesystem dengan block driver M14 dan fault taxonomy I/O. |
| `osdev-12-toolchain-devenv` | Makefile, freestanding compile, audit `nm`/`readelf`/`objdump`, checksum, dan reproducibility. |
| `osdev-14-cross-science` | Verification matrix, risk register, failure mode analysis, dan evidence baseline. |

---

## 6. Alat dan Versi yang Harus Dicatat

Mahasiswa wajib mencatat versi alat aktual dari host masing-masing. Jalankan perintah berikut dari WSL 2 pada root repository MCSOS.

Perintah ini mengumpulkan identitas host dan toolchain. Outputnya menjadi bukti bahwa praktikum dibangun pada lingkungan yang dapat diaudit.

```bash
mkdir -p artifacts/m15
{ uname -a; lsb_release -a 2>/dev/null || cat /etc/os-release; } | tee artifacts/m15/host_info.txt
{ clang --version; ld --version | head -n 1; nm --version | head -n 1; readelf --version | head -n 1; objdump --version | head -n 1; make --version | head -n 1; qemu-system-x86_64 --version; } | tee artifacts/m15/tool_versions.txt
```

Indikator hasil yang benar adalah file `artifacts/m15/host_info.txt` dan `artifacts/m15/tool_versions.txt` terbuat, semua tool utama ditemukan, dan `clang` dapat dipanggil dari WSL. Jika `qemu-system-x86_64` tidak ditemukan, M15 host unit test masih dapat dijalankan, tetapi QEMU smoke test harus ditunda sampai paket QEMU dan OVMF tersedia.

---

## 7. Pemeriksaan Kesiapan Hasil Praktikum M0-M14

Sebelum menulis source M15, lakukan pemeriksaan readiness berlapis. Tujuannya bukan hanya memastikan file ada, tetapi memastikan artefak yang menjadi prasyarat M15 dapat dipercaya.

### 7.1 Checklist readiness ringkas

| Tahap | Artefak minimum | Pemeriksaan | Jika gagal |
|---|---|---|---|
| M0 | Repository, ADR, risk register, verification matrix | `git status`, direktori `docs/`, `artifacts/m0/` | Pulihkan struktur repo dan commit baseline. |
| M1 | Toolchain proof object | `artifacts/m1/tool_versions.txt`, `readelf` proof | Install Clang/Binutils dan ulangi proof compile. |
| M2 | Boot image/ISO awal | ISO/kernel ELF tersedia | Rebuild Limine/boot image sesuai panduan M2. |
| M3 | Panic/logging path | Serial log panic terbaca | Perbaiki early console dan panic path. |
| M4 | IDT/trap | `lidt`, `iretq`, exception stub tervalidasi | Audit assembly stub dan gate descriptor. |
| M5 | PIC/PIT/IRQ0 | Timer tick log deterministik | Periksa PIC remap, PIT divisor, EOI, `sti`. |
| M6 | PMM bitmap | Host unit test PMM lulus | Periksa bitmap boundary dan reserved frame. |
| M7 | VMM awal | Page-table invariant test lulus | Periksa HHDM, PTE flags, CR3 handoff. |
| M8 | Kernel heap | Allocator test lulus | Periksa free-list coalescing dan alignment. |
| M9 | Kernel thread/scheduler | Context switch object audit lulus | Periksa callee-saved register dan stack alignment. |
| M10 | Syscall ABI awal | Syscall dispatcher test lulus | Periksa trap frame dan user buffer validation. |
| M11 | ELF user loader | ELF validation test lulus | Periksa `PT_LOAD`, alignment, W^X, dan address range. |
| M12 | Synchronization | Lock test lulus | Periksa atomic acquire/release dan lock-order validator. |
| M13 | VFS/RAMFS/FD table | File I/O host test lulus | Periksa fd table, refcount, read/write offset. |
| M14 | Block layer/RAM block/buffer cache | Block host test dan freestanding audit lulus | Periksa LBA range, dirty flag, flush, object audit. |

### 7.2 Script preflight M15

Buat script preflight untuk mengumpulkan bukti kesiapan. Perintah ini tidak memperbaiki source secara otomatis; ia mengumpulkan bukti agar kegagalan mudah dilacak.

```bash
mkdir -p scripts artifacts/m15
cat > scripts/m15_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p artifacts/m15
{
  echo "== git =="
  git status --short || true
  git rev-parse --short HEAD || true
  echo "== toolchain =="
  clang --version | head -n 1
  ld --version | head -n 1
  nm --version | head -n 1
  readelf --version | head -n 1
  objdump --version | head -n 1
  make --version | head -n 1
  echo "== prior artifacts =="
  for d in m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12 m13 m14; do
    if [ -d "artifacts/$d" ]; then
      echo "artifacts/$d: present"
    else
      echo "artifacts/$d: missing"
    fi
  done
} | tee artifacts/m15/preflight.txt
EOF
chmod +x scripts/m15_preflight.sh
./scripts/m15_preflight.sh
```

Indikator hasil benar adalah `artifacts/m15/preflight.txt` berisi versi toolchain dan status artefak. Jika direktori artefak lama belum ada karena mahasiswa memulai dari repository bersih, catat pada laporan dan ulangi minimal build/test dari tahap prasyarat yang digunakan oleh M15.

---

## 8. Diagnosis dan Solusi Kendala dari M0-M14

| Gejala | Kemungkinan penyebab | Pemeriksaan | Solusi konservatif |
|---|---|---|---|
| `clang: command not found` | Toolchain WSL belum lengkap | `which clang` | Install paket `clang lld binutils make`. |
| `qemu-system-x86_64: command not found` | QEMU belum tersedia | `which qemu-system-x86_64` | Install `qemu-system-x86 ovmf`. |
| `-target x86_64-elf` gagal | Memakai `cc`, bukan `clang` | `make CC=clang` | Jalankan target dengan `make CC=clang`. |
| Undefined symbol muncul pada `nm -u` | Source freestanding memanggil libc atau helper compiler | `nm -u artifacts/m15/mcsfs1.rel.o` | Hilangkan `memcpy`, `memset`, `printf`, `malloc`; pakai helper lokal. |
| Host test gagal pada read/write | Bitmap atau direct block rusak | Jalankan test dengan `printf` tambahan di host-only path | Periksa alokasi inode, alokasi block, ukuran file, dan LBA. |
| Fsck-lite gagal setelah format | Layout block tidak konsisten | Dump superblock dan bitmaps | Pastikan reserved block, root inode, dan root dir block ditandai used. |
| Duplicate file tidak terdeteksi | Directory lookup salah | Test `create` dua kali | Periksa pembandingan nama dan panjang nama. |
| File besar salah baca | Direct block loop salah | Tulis file >512 byte | Periksa `blocks_needed`, `written`, `copied`, dan zero-fill partial block. |
| QEMU boot regression | Object M15 belum tertaut benar | Lihat serial log dan linker map | Rebuild kernel dengan simbol M15 dan rollback ke commit M14 jika perlu. |

---

## 9. Konsep Inti MCSFS1

MCSFS1 memakai format sangat kecil:

| LBA | Isi | Keterangan |
|---:|---|---|
| 0 | Superblock | Magic, version, block size, block count, lokasi metadata. |
| 1 | Inode bitmap | Bit inode aktif. Inode 1 adalah root. |
| 2 | Block bitmap | Bit block aktif. Block 0-7 reserved. |
| 3-6 | Inode table | 32 inode; setiap inode menyimpan mode, links, size, direct blocks. |
| 7 | Root directory block | Maksimal 16 directory entry. |
| 8..N | Data blocks | Data file regular. |

M15 memakai block size 512 byte, maksimal 32 inode, maksimal 8 direct blocks per file, dan root directory tunggal. Batas ukuran file per inode adalah `8 * 512 = 4096` byte. Batas ini sengaja kecil agar invariant dapat diperiksa secara manual.

---

## 10. Invariant M15

Invariant berikut wajib dipertahankan oleh semua fungsi MCSFS1.

| ID | Invariant | Alasan | Bukti minimum |
|---|---|---|---|
| I15-01 | `super.magic == MCSFS1_MAGIC` dan `super.version == 1` | Mount tidak boleh menerima format asing. | Host test corrupt-super gagal dengan `MCSFS1_ERR_CORRUPT`. |
| I15-02 | `block_size == 512` dan `block_count == dev->block_count` | Driver dan filesystem harus sepakat tentang ukuran dan range. | `mcsfs1_mount` memvalidasi superblock. |
| I15-03 | Root inode adalah inode 1, bertipe directory, direct[0] menunjuk block 7 | Root directory adalah anchor namespace. | `mcsfs1_fsck` memeriksa root inode. |
| I15-04 | Semua block metadata 0-7 ditandai used pada block bitmap | Metadata tidak boleh dialokasikan untuk data file. | `mcsfs1_format` dan `mcsfs1_fsck`. |
| I15-05 | Directory entry aktif harus menunjuk inode aktif | Nama file tidak boleh menunjuk inode bebas. | `mcsfs1_fsck`. |
| I15-06 | File inode bertipe file dan size tidak melebihi 4096 byte | Direct-only filesystem tidak mendukung ukuran lebih besar. | `mcsfs1_write` dan `mcsfs1_fsck`. |
| I15-07 | Semua direct block file harus berada pada range data block dan bitnya used | Mencegah pembacaan metadata sebagai data. | `mcsfs1_fsck`. |
| I15-08 | Nama file tidak boleh kosong, tidak boleh memuat `/`, dan maksimal 27 byte | M15 hanya mendukung root-only flat namespace. | `valid_name`. |
| I15-09 | Operasi metadata yang berhasil harus melakukan flush eksplisit | Mengurangi risiko stale metadata pada clean shutdown. | Host test `flush_count > 0`. |
| I15-10 | Source freestanding tidak boleh memakai hosted libc | Kernel belum memiliki libc. | `nm -u` kosong pada linked relocatable object. |

---

## 11. State Machine Operasi File

State file MCSFS1 dapat diringkas sebagai berikut.

| State | Makna | Transisi masuk | Transisi keluar |
|---|---|---|---|
| `Absent` | Tidak ada directory entry | Setelah format atau unlink | `create(name)` |
| `CreatedEmpty` | Directory entry dan inode ada, size 0 | `create(name)` | `write(name, len)` atau `unlink(name)` |
| `Written` | Inode memiliki size > 0 dan direct data blocks | `write(name, len > 0)` | `write(name, len lain)` atau `unlink(name)` |
| `Deleted` | Directory entry dihapus, inode dan block dibebaskan | `unlink(name)` | `create(name)` memakai nama sama atau lain |
| `Corrupt` | Metadata tidak memenuhi invariant | Fault injection/corrupt write | `fsck` mendeteksi; repair penuh belum tersedia |

Progress property minimum: jika `format` berhasil dan device tidak gagal, maka `mount` harus berhasil; jika `create` berhasil, `read` terhadap file tersebut harus menemukan inode; jika `write` berhasil, `read` harus mengembalikan byte yang sama sampai file dihapus atau metadata dirusak.

---

## 12. Struktur Repository yang Diharapkan

Setelah M15, struktur tambahan repository adalah sebagai berikut.

```text
mcsos/
├── fs/
│   └── mcsfs1/
│       ├── mcsfs1.h
│       └── mcsfs1.c
├── tests/
│   └── m15/
│       └── test_mcsfs1.c
├── artifacts/
│   └── m15/
│       ├── host_info.txt
│       ├── tool_versions.txt
│       ├── preflight.txt
│       ├── host_test.txt
│       ├── nm_undefined.txt
│       ├── readelf_header.txt
│       ├── objdump.txt
│       └── SHA256SUMS.txt
└── Makefile
```

Jika repository sudah memiliki struktur `kernel/fs/` atau `kernel/storage/`, penempatan file boleh disesuaikan, tetapi laporan harus menjelaskan path aktual dan alasan integrasinya.

---

## 13. Langkah Kerja Praktikum M15

### 13.1 Membuat branch kerja

Perintah ini membuat branch khusus M15. Tujuannya agar perubahan filesystem dapat direview dan rollback ke M14 dengan aman.

```bash
git switch -c praktikum-m15-mcsfs1
mkdir -p fs/mcsfs1 tests/m15 artifacts/m15
```

Indikator hasil benar adalah branch aktif bernama `praktikum-m15-mcsfs1` dan direktori `fs/mcsfs1`, `tests/m15`, serta `artifacts/m15` tersedia.

### 13.2 Menambahkan header `mcsfs1.h`

Header ini mendefinisikan konstanta format, error code, block-device interface, mount object, dan API filesystem. Header sengaja kecil agar dapat dipakai dari host test dan kernel freestanding.

```bash
cat > fs/mcsfs1/mcsfs1.h <<'EOF'
#ifndef MCSFS1_H
#define MCSFS1_H

#include <stdint.h>
#include <stddef.h>

#define MCSFS1_BLOCK_SIZE 512u
#define MCSFS1_MAGIC 0x31465343u
#define MCSFS1_VERSION 1u
#define MCSFS1_MAX_INODES 32u
#define MCSFS1_DIRECT_BLOCKS 8u
#define MCSFS1_MAX_NAME 27u
#define MCSFS1_ROOT_INO 1u
#define MCSFS1_MODE_FREE 0u
#define MCSFS1_MODE_FILE 1u
#define MCSFS1_MODE_DIR 2u
#define MCSFS1_ERR_OK 0
#define MCSFS1_ERR_INVAL -1
#define MCSFS1_ERR_IO -2
#define MCSFS1_ERR_NOSPC -3
#define MCSFS1_ERR_EXIST -4
#define MCSFS1_ERR_NOENT -5
#define MCSFS1_ERR_NAMETOOLONG -6
#define MCSFS1_ERR_CORRUPT -7
#define MCSFS1_ERR_ISDIR -8
#define MCSFS1_ERR_RANGE -9

struct mcsfs1_blkdev {
    void *ctx;
    uint32_t block_count;
    int (*read)(void *ctx, uint32_t lba, void *buf512);
    int (*write)(void *ctx, uint32_t lba, const void *buf512);
    int (*flush)(void *ctx);
};

struct mcsfs1_mount {
    struct mcsfs1_blkdev *dev;
    uint32_t block_count;
    uint32_t data_start;
};

int mcsfs1_format(struct mcsfs1_blkdev *dev);
int mcsfs1_mount(struct mcsfs1_mount *mnt, struct mcsfs1_blkdev *dev);
int mcsfs1_fsck(struct mcsfs1_blkdev *dev);
int mcsfs1_create(struct mcsfs1_mount *mnt, const char *name);
int mcsfs1_write(struct mcsfs1_mount *mnt, const char *name, const uint8_t *buf, uint32_t len);
int mcsfs1_read(struct mcsfs1_mount *mnt, const char *name, uint8_t *buf, uint32_t cap, uint32_t *out_len);
int mcsfs1_unlink(struct mcsfs1_mount *mnt, const char *name);

#endif

EOF
```

### 13.3 Menambahkan implementasi `mcsfs1.c`

Implementasi ini tidak memakai hosted libc. Helper `mcsfs_memset`, `mcsfs_memcpy`, `mcsfs_memcmp`, dan `mcsfs_strlen_bound` disediakan secara lokal untuk menjaga object tetap freestanding. Fungsi `mcsfs1_format` menginisialisasi superblock, bitmap, root inode, dan root directory. Fungsi `mcsfs1_fsck` memeriksa konsistensi minimum.

```bash
cat > fs/mcsfs1/mcsfs1.c <<'EOF'
#include "mcsfs1.h"

#define MCSFS1_SB_LBA 0u
#define MCSFS1_INODE_BMAP_LBA 1u
#define MCSFS1_BLOCK_BMAP_LBA 2u
#define MCSFS1_INODE_TABLE_LBA 3u
#define MCSFS1_INODE_TABLE_BLOCKS 4u
#define MCSFS1_ROOT_DIR_LBA 7u
#define MCSFS1_DATA_START_LBA 8u
#define MCSFS1_MIN_BLOCKS 16u
#define MCSFS1_DIRENT_COUNT 16u

struct mcsfs1_super_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t inode_count;
    uint32_t inode_bmap_lba;
    uint32_t block_bmap_lba;
    uint32_t inode_table_lba;
    uint32_t inode_table_blocks;
    uint32_t root_ino;
    uint32_t root_dir_lba;
    uint32_t data_start_lba;
    uint32_t clean;
    uint32_t reserved[115];
};

struct mcsfs1_inode_disk {
    uint16_t mode;
    uint16_t links;
    uint32_t size;
    uint32_t direct[MCSFS1_DIRECT_BLOCKS];
    uint32_t reserved[5];
};

struct mcsfs1_dirent_disk {
    uint32_t ino;
    uint8_t type;
    char name[MCSFS1_MAX_NAME];
};

static void *mcsfs_memset(void *dst, int c, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = (uint8_t)c;
    }
    return dst;
}

static void *mcsfs_memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

static int mcsfs_memcmp(const void *a, const void *b, uint32_t n) {
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    for (uint32_t i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return (int)x[i] - (int)y[i];
        }
    }
    return 0;
}

static uint32_t mcsfs_strlen_bound(const char *s, uint32_t max_plus_one) {
    uint32_t n = 0;
    if (s == 0) {
        return max_plus_one;
    }
    while (n < max_plus_one && s[n] != '\0') {
        n++;
    }
    return n;
}

static int valid_name(const char *name, uint32_t *len_out) {
    uint32_t n = mcsfs_strlen_bound(name, MCSFS1_MAX_NAME + 1u);
    if (n == 0u) {
        return MCSFS1_ERR_INVAL;
    }
    if (n > MCSFS1_MAX_NAME) {
        return MCSFS1_ERR_NAMETOOLONG;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (name[i] == '/') {
            return MCSFS1_ERR_INVAL;
        }
    }
    *len_out = n;
    return MCSFS1_ERR_OK;
}

static int dev_read(struct mcsfs1_blkdev *dev, uint32_t lba, void *buf) {
    if (dev == 0 || dev->read == 0 || buf == 0 || lba >= dev->block_count) {
        return MCSFS1_ERR_INVAL;
    }
    return dev->read(dev->ctx, lba, buf) == 0 ? MCSFS1_ERR_OK : MCSFS1_ERR_IO;
}

static int dev_write(struct mcsfs1_blkdev *dev, uint32_t lba, const void *buf) {
    if (dev == 0 || dev->write == 0 || buf == 0 || lba >= dev->block_count) {
        return MCSFS1_ERR_INVAL;
    }
    return dev->write(dev->ctx, lba, buf) == 0 ? MCSFS1_ERR_OK : MCSFS1_ERR_IO;
}

static int dev_flush(struct mcsfs1_blkdev *dev) {
    if (dev == 0 || dev->flush == 0) {
        return MCSFS1_ERR_INVAL;
    }
    return dev->flush(dev->ctx) == 0 ? MCSFS1_ERR_OK : MCSFS1_ERR_IO;
}

static void bit_set(uint8_t *b, uint32_t bit) {
    b[bit / 8u] = (uint8_t)(b[bit / 8u] | (uint8_t)(1u << (bit % 8u)));
}

static void bit_clear(uint8_t *b, uint32_t bit) {
    b[bit / 8u] = (uint8_t)(b[bit / 8u] & (uint8_t)~(uint8_t)(1u << (bit % 8u)));
}

static int bit_test(const uint8_t *b, uint32_t bit) {
    return (b[bit / 8u] & (uint8_t)(1u << (bit % 8u))) != 0u;
}

static int load_super(struct mcsfs1_blkdev *dev, struct mcsfs1_super_disk *sb) {
    int rc = dev_read(dev, MCSFS1_SB_LBA, sb);
    if (rc != 0) {
        return rc;
    }
    if (sb->magic != MCSFS1_MAGIC || sb->version != MCSFS1_VERSION || sb->block_size != MCSFS1_BLOCK_SIZE) {
        return MCSFS1_ERR_CORRUPT;
    }
    if (sb->block_count != dev->block_count || sb->inode_count != MCSFS1_MAX_INODES) {
        return MCSFS1_ERR_CORRUPT;
    }
    if (sb->inode_bmap_lba != MCSFS1_INODE_BMAP_LBA || sb->block_bmap_lba != MCSFS1_BLOCK_BMAP_LBA || sb->inode_table_lba != MCSFS1_INODE_TABLE_LBA) {
        return MCSFS1_ERR_CORRUPT;
    }
    if (sb->root_ino != MCSFS1_ROOT_INO || sb->root_dir_lba != MCSFS1_ROOT_DIR_LBA || sb->data_start_lba != MCSFS1_DATA_START_LBA) {
        return MCSFS1_ERR_CORRUPT;
    }
    if (sb->data_start_lba >= sb->block_count) {
        return MCSFS1_ERR_CORRUPT;
    }
    return MCSFS1_ERR_OK;
}

static int read_inode(struct mcsfs1_blkdev *dev, uint32_t ino, struct mcsfs1_inode_disk *inode) {
    if (ino == 0u || ino > MCSFS1_MAX_INODES || inode == 0) {
        return MCSFS1_ERR_INVAL;
    }
    uint8_t block[MCSFS1_BLOCK_SIZE];
    uint32_t index = ino - 1u;
    uint32_t per_block = MCSFS1_BLOCK_SIZE / (uint32_t)sizeof(struct mcsfs1_inode_disk);
    uint32_t lba = MCSFS1_INODE_TABLE_LBA + (index / per_block);
    uint32_t off = (index % per_block) * (uint32_t)sizeof(struct mcsfs1_inode_disk);
    if (lba >= MCSFS1_DATA_START_LBA) {
        return MCSFS1_ERR_CORRUPT;
    }
    int rc = dev_read(dev, lba, block);
    if (rc != 0) {
        return rc;
    }
    mcsfs_memcpy(inode, block + off, (uint32_t)sizeof(*inode));
    return MCSFS1_ERR_OK;
}

static int write_inode(struct mcsfs1_blkdev *dev, uint32_t ino, const struct mcsfs1_inode_disk *inode) {
    if (ino == 0u || ino > MCSFS1_MAX_INODES || inode == 0) {
        return MCSFS1_ERR_INVAL;
    }
    uint8_t block[MCSFS1_BLOCK_SIZE];
    uint32_t index = ino - 1u;
    uint32_t per_block = MCSFS1_BLOCK_SIZE / (uint32_t)sizeof(struct mcsfs1_inode_disk);
    uint32_t lba = MCSFS1_INODE_TABLE_LBA + (index / per_block);
    uint32_t off = (index % per_block) * (uint32_t)sizeof(struct mcsfs1_inode_disk);
    if (lba >= MCSFS1_DATA_START_LBA) {
        return MCSFS1_ERR_CORRUPT;
    }
    int rc = dev_read(dev, lba, block);
    if (rc != 0) {
        return rc;
    }
    mcsfs_memcpy(block + off, inode, (uint32_t)sizeof(*inode));
    return dev_write(dev, lba, block);
}

static int load_bmaps(struct mcsfs1_blkdev *dev, uint8_t *ib, uint8_t *bb) {
    int rc = dev_read(dev, MCSFS1_INODE_BMAP_LBA, ib);
    if (rc != 0) {
        return rc;
    }
    return dev_read(dev, MCSFS1_BLOCK_BMAP_LBA, bb);
}

static int store_bmaps(struct mcsfs1_blkdev *dev, const uint8_t *ib, const uint8_t *bb) {
    int rc = dev_write(dev, MCSFS1_INODE_BMAP_LBA, ib);
    if (rc != 0) {
        return rc;
    }
    return dev_write(dev, MCSFS1_BLOCK_BMAP_LBA, bb);
}

static int find_dirent(struct mcsfs1_blkdev *dev, const char *name, uint32_t *slot_out, uint32_t *ino_out) {
    uint8_t block[MCSFS1_BLOCK_SIZE];
    uint32_t name_len = 0;
    int rc = valid_name(name, &name_len);
    if (rc != 0) {
        return rc;
    }
    rc = dev_read(dev, MCSFS1_ROOT_DIR_LBA, block);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_dirent_disk *de = (struct mcsfs1_dirent_disk *)block;
    for (uint32_t i = 0; i < MCSFS1_DIRENT_COUNT; i++) {
        if (de[i].ino != 0u && mcsfs_strlen_bound(de[i].name, MCSFS1_MAX_NAME + 1u) == name_len && mcsfs_memcmp(de[i].name, name, name_len) == 0) {
            if (slot_out != 0) {
                *slot_out = i;
            }
            if (ino_out != 0) {
                *ino_out = de[i].ino;
            }
            return MCSFS1_ERR_OK;
        }
    }
    return MCSFS1_ERR_NOENT;
}

static int alloc_inode_block(struct mcsfs1_blkdev *dev, uint32_t *ino_out, uint32_t *data_lba_out) {
    uint8_t ib[MCSFS1_BLOCK_SIZE];
    uint8_t bb[MCSFS1_BLOCK_SIZE];
    int rc = load_bmaps(dev, ib, bb);
    if (rc != 0) {
        return rc;
    }
    uint32_t ino = 0;
    for (uint32_t i = 2u; i <= MCSFS1_MAX_INODES; i++) {
        if (!bit_test(ib, i)) {
            ino = i;
            break;
        }
    }
    if (ino == 0u) {
        return MCSFS1_ERR_NOSPC;
    }
    uint32_t lba = 0;
    for (uint32_t b = MCSFS1_DATA_START_LBA; b < dev->block_count; b++) {
        if (!bit_test(bb, b)) {
            lba = b;
            break;
        }
    }
    if (lba == 0u) {
        return MCSFS1_ERR_NOSPC;
    }
    bit_set(ib, ino);
    bit_set(bb, lba);
    rc = store_bmaps(dev, ib, bb);
    if (rc != 0) {
        return rc;
    }
    *ino_out = ino;
    *data_lba_out = lba;
    return MCSFS1_ERR_OK;
}

static int alloc_data_block(struct mcsfs1_blkdev *dev, uint32_t *data_lba_out) {
    uint8_t ib[MCSFS1_BLOCK_SIZE];
    uint8_t bb[MCSFS1_BLOCK_SIZE];
    int rc = load_bmaps(dev, ib, bb);
    if (rc != 0) {
        return rc;
    }
    for (uint32_t b = MCSFS1_DATA_START_LBA; b < dev->block_count; b++) {
        if (!bit_test(bb, b)) {
            bit_set(bb, b);
            rc = store_bmaps(dev, ib, bb);
            if (rc != 0) {
                return rc;
            }
            *data_lba_out = b;
            return MCSFS1_ERR_OK;
        }
    }
    return MCSFS1_ERR_NOSPC;
}

static int free_inode_and_blocks(struct mcsfs1_blkdev *dev, uint32_t ino, const struct mcsfs1_inode_disk *inode) {
    uint8_t ib[MCSFS1_BLOCK_SIZE];
    uint8_t bb[MCSFS1_BLOCK_SIZE];
    int rc = load_bmaps(dev, ib, bb);
    if (rc != 0) {
        return rc;
    }
    bit_clear(ib, ino);
    for (uint32_t i = 0; i < MCSFS1_DIRECT_BLOCKS; i++) {
        if (inode->direct[i] != 0u && inode->direct[i] < dev->block_count) {
            bit_clear(bb, inode->direct[i]);
        }
    }
    return store_bmaps(dev, ib, bb);
}

int mcsfs1_format(struct mcsfs1_blkdev *dev) {
    if (dev == 0 || dev->block_count < MCSFS1_MIN_BLOCKS || dev->block_count > (MCSFS1_BLOCK_SIZE * 8u)) {
        return MCSFS1_ERR_INVAL;
    }
    uint8_t zero[MCSFS1_BLOCK_SIZE];
    mcsfs_memset(zero, 0, MCSFS1_BLOCK_SIZE);
    for (uint32_t lba = 0; lba < dev->block_count; lba++) {
        int rc0 = dev_write(dev, lba, zero);
        if (rc0 != 0) {
            return rc0;
        }
    }

    struct mcsfs1_super_disk sb;
    mcsfs_memset(&sb, 0, (uint32_t)sizeof(sb));
    sb.magic = MCSFS1_MAGIC;
    sb.version = MCSFS1_VERSION;
    sb.block_size = MCSFS1_BLOCK_SIZE;
    sb.block_count = dev->block_count;
    sb.inode_count = MCSFS1_MAX_INODES;
    sb.inode_bmap_lba = MCSFS1_INODE_BMAP_LBA;
    sb.block_bmap_lba = MCSFS1_BLOCK_BMAP_LBA;
    sb.inode_table_lba = MCSFS1_INODE_TABLE_LBA;
    sb.inode_table_blocks = MCSFS1_INODE_TABLE_BLOCKS;
    sb.root_ino = MCSFS1_ROOT_INO;
    sb.root_dir_lba = MCSFS1_ROOT_DIR_LBA;
    sb.data_start_lba = MCSFS1_DATA_START_LBA;
    sb.clean = 1u;
    int rc = dev_write(dev, MCSFS1_SB_LBA, &sb);
    if (rc != 0) {
        return rc;
    }

    uint8_t ib[MCSFS1_BLOCK_SIZE];
    uint8_t bb[MCSFS1_BLOCK_SIZE];
    mcsfs_memset(ib, 0, MCSFS1_BLOCK_SIZE);
    mcsfs_memset(bb, 0, MCSFS1_BLOCK_SIZE);
    bit_set(ib, 0u);
    bit_set(ib, MCSFS1_ROOT_INO);
    for (uint32_t b = 0; b < MCSFS1_DATA_START_LBA; b++) {
        bit_set(bb, b);
    }
    bit_set(bb, MCSFS1_ROOT_DIR_LBA);
    rc = store_bmaps(dev, ib, bb);
    if (rc != 0) {
        return rc;
    }

    struct mcsfs1_inode_disk root;
    mcsfs_memset(&root, 0, (uint32_t)sizeof(root));
    root.mode = MCSFS1_MODE_DIR;
    root.links = 1u;
    root.size = MCSFS1_BLOCK_SIZE;
    root.direct[0] = MCSFS1_ROOT_DIR_LBA;
    rc = write_inode(dev, MCSFS1_ROOT_INO, &root);
    if (rc != 0) {
        return rc;
    }
    return dev_flush(dev);
}

int mcsfs1_mount(struct mcsfs1_mount *mnt, struct mcsfs1_blkdev *dev) {
    if (mnt == 0 || dev == 0) {
        return MCSFS1_ERR_INVAL;
    }
    struct mcsfs1_super_disk sb;
    int rc = load_super(dev, &sb);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_inode_disk root;
    rc = read_inode(dev, MCSFS1_ROOT_INO, &root);
    if (rc != 0) {
        return rc;
    }
    if (root.mode != MCSFS1_MODE_DIR || root.direct[0] != MCSFS1_ROOT_DIR_LBA) {
        return MCSFS1_ERR_CORRUPT;
    }
    mnt->dev = dev;
    mnt->block_count = sb.block_count;
    mnt->data_start = sb.data_start_lba;
    return MCSFS1_ERR_OK;
}

int mcsfs1_create(struct mcsfs1_mount *mnt, const char *name) {
    if (mnt == 0 || mnt->dev == 0) {
        return MCSFS1_ERR_INVAL;
    }
    uint32_t name_len = 0;
    int rc = valid_name(name, &name_len);
    if (rc != 0) {
        return rc;
    }
    if (find_dirent(mnt->dev, name, 0, 0) == 0) {
        return MCSFS1_ERR_EXIST;
    }
    uint8_t dir_block[MCSFS1_BLOCK_SIZE];
    rc = dev_read(mnt->dev, MCSFS1_ROOT_DIR_LBA, dir_block);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_dirent_disk *de = (struct mcsfs1_dirent_disk *)dir_block;
    uint32_t free_slot = MCSFS1_DIRENT_COUNT;
    for (uint32_t i = 0; i < MCSFS1_DIRENT_COUNT; i++) {
        if (de[i].ino == 0u) {
            free_slot = i;
            break;
        }
    }
    if (free_slot == MCSFS1_DIRENT_COUNT) {
        return MCSFS1_ERR_NOSPC;
    }
    uint32_t ino = 0;
    uint32_t first_data = 0;
    rc = alloc_inode_block(mnt->dev, &ino, &first_data);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_inode_disk inode;
    mcsfs_memset(&inode, 0, (uint32_t)sizeof(inode));
    inode.mode = MCSFS1_MODE_FILE;
    inode.links = 1u;
    inode.size = 0u;
    inode.direct[0] = first_data;
    rc = write_inode(mnt->dev, ino, &inode);
    if (rc != 0) {
        return rc;
    }
    de[free_slot].ino = ino;
    de[free_slot].type = MCSFS1_MODE_FILE;
    mcsfs_memset(de[free_slot].name, 0, MCSFS1_MAX_NAME);
    mcsfs_memcpy(de[free_slot].name, name, name_len);
    rc = dev_write(mnt->dev, MCSFS1_ROOT_DIR_LBA, dir_block);
    if (rc != 0) {
        return rc;
    }
    return dev_flush(mnt->dev);
}

int mcsfs1_write(struct mcsfs1_mount *mnt, const char *name, const uint8_t *buf, uint32_t len) {
    if (mnt == 0 || mnt->dev == 0 || (buf == 0 && len != 0u)) {
        return MCSFS1_ERR_INVAL;
    }
    if (len > MCSFS1_DIRECT_BLOCKS * MCSFS1_BLOCK_SIZE) {
        return MCSFS1_ERR_RANGE;
    }
    uint32_t ino = 0;
    int rc = find_dirent(mnt->dev, name, 0, &ino);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_inode_disk inode;
    rc = read_inode(mnt->dev, ino, &inode);
    if (rc != 0) {
        return rc;
    }
    if (inode.mode != MCSFS1_MODE_FILE) {
        return MCSFS1_ERR_ISDIR;
    }
    uint32_t blocks_needed = (len + MCSFS1_BLOCK_SIZE - 1u) / MCSFS1_BLOCK_SIZE;
    if (blocks_needed == 0u) {
        blocks_needed = 1u;
    }
    for (uint32_t i = 0; i < blocks_needed; i++) {
        if (inode.direct[i] == 0u) {
            rc = alloc_data_block(mnt->dev, &inode.direct[i]);
            if (rc != 0) {
                return rc;
            }
        }
    }
    uint8_t block[MCSFS1_BLOCK_SIZE];
    uint32_t written = 0;
    for (uint32_t i = 0; i < blocks_needed; i++) {
        mcsfs_memset(block, 0, MCSFS1_BLOCK_SIZE);
        uint32_t remain = len - written;
        uint32_t chunk = remain > MCSFS1_BLOCK_SIZE ? MCSFS1_BLOCK_SIZE : remain;
        if (chunk != 0u) {
            mcsfs_memcpy(block, buf + written, chunk);
        }
        rc = dev_write(mnt->dev, inode.direct[i], block);
        if (rc != 0) {
            return rc;
        }
        written += chunk;
    }
    inode.size = len;
    rc = write_inode(mnt->dev, ino, &inode);
    if (rc != 0) {
        return rc;
    }
    return dev_flush(mnt->dev);
}

int mcsfs1_read(struct mcsfs1_mount *mnt, const char *name, uint8_t *buf, uint32_t cap, uint32_t *out_len) {
    if (mnt == 0 || mnt->dev == 0 || buf == 0 || out_len == 0) {
        return MCSFS1_ERR_INVAL;
    }
    uint32_t ino = 0;
    int rc = find_dirent(mnt->dev, name, 0, &ino);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_inode_disk inode;
    rc = read_inode(mnt->dev, ino, &inode);
    if (rc != 0) {
        return rc;
    }
    if (inode.mode != MCSFS1_MODE_FILE) {
        return MCSFS1_ERR_ISDIR;
    }
    if (cap < inode.size) {
        return MCSFS1_ERR_RANGE;
    }
    uint32_t blocks_needed = (inode.size + MCSFS1_BLOCK_SIZE - 1u) / MCSFS1_BLOCK_SIZE;
    uint32_t copied = 0;
    uint8_t block[MCSFS1_BLOCK_SIZE];
    for (uint32_t i = 0; i < blocks_needed; i++) {
        if (inode.direct[i] == 0u || inode.direct[i] >= mnt->block_count) {
            return MCSFS1_ERR_CORRUPT;
        }
        rc = dev_read(mnt->dev, inode.direct[i], block);
        if (rc != 0) {
            return rc;
        }
        uint32_t remain = inode.size - copied;
        uint32_t chunk = remain > MCSFS1_BLOCK_SIZE ? MCSFS1_BLOCK_SIZE : remain;
        mcsfs_memcpy(buf + copied, block, chunk);
        copied += chunk;
    }
    *out_len = inode.size;
    return MCSFS1_ERR_OK;
}

int mcsfs1_unlink(struct mcsfs1_mount *mnt, const char *name) {
    if (mnt == 0 || mnt->dev == 0) {
        return MCSFS1_ERR_INVAL;
    }
    uint32_t slot = 0;
    uint32_t ino = 0;
    int rc = find_dirent(mnt->dev, name, &slot, &ino);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_inode_disk inode;
    rc = read_inode(mnt->dev, ino, &inode);
    if (rc != 0) {
        return rc;
    }
    if (inode.mode != MCSFS1_MODE_FILE) {
        return MCSFS1_ERR_ISDIR;
    }
    uint8_t dir_block[MCSFS1_BLOCK_SIZE];
    rc = dev_read(mnt->dev, MCSFS1_ROOT_DIR_LBA, dir_block);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_dirent_disk *de = (struct mcsfs1_dirent_disk *)dir_block;
    mcsfs_memset(&de[slot], 0, (uint32_t)sizeof(de[slot]));
    rc = dev_write(mnt->dev, MCSFS1_ROOT_DIR_LBA, dir_block);
    if (rc != 0) {
        return rc;
    }
    rc = free_inode_and_blocks(mnt->dev, ino, &inode);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_inode_disk zero_inode;
    mcsfs_memset(&zero_inode, 0, (uint32_t)sizeof(zero_inode));
    rc = write_inode(mnt->dev, ino, &zero_inode);
    if (rc != 0) {
        return rc;
    }
    return dev_flush(mnt->dev);
}

int mcsfs1_fsck(struct mcsfs1_blkdev *dev) {
    if (dev == 0) {
        return MCSFS1_ERR_INVAL;
    }
    struct mcsfs1_super_disk sb;
    int rc = load_super(dev, &sb);
    if (rc != 0) {
        return rc;
    }
    uint8_t ib[MCSFS1_BLOCK_SIZE];
    uint8_t bb[MCSFS1_BLOCK_SIZE];
    rc = load_bmaps(dev, ib, bb);
    if (rc != 0) {
        return rc;
    }
    if (!bit_test(ib, MCSFS1_ROOT_INO) || !bit_test(bb, MCSFS1_ROOT_DIR_LBA)) {
        return MCSFS1_ERR_CORRUPT;
    }
    for (uint32_t b = 0; b < MCSFS1_DATA_START_LBA; b++) {
        if (!bit_test(bb, b)) {
            return MCSFS1_ERR_CORRUPT;
        }
    }
    struct mcsfs1_inode_disk root;
    rc = read_inode(dev, MCSFS1_ROOT_INO, &root);
    if (rc != 0) {
        return rc;
    }
    if (root.mode != MCSFS1_MODE_DIR || root.direct[0] != MCSFS1_ROOT_DIR_LBA) {
        return MCSFS1_ERR_CORRUPT;
    }
    uint8_t dir_block[MCSFS1_BLOCK_SIZE];
    rc = dev_read(dev, MCSFS1_ROOT_DIR_LBA, dir_block);
    if (rc != 0) {
        return rc;
    }
    struct mcsfs1_dirent_disk *de = (struct mcsfs1_dirent_disk *)dir_block;
    for (uint32_t i = 0; i < MCSFS1_DIRENT_COUNT; i++) {
        if (de[i].ino == 0u) {
            continue;
        }
        if (de[i].ino > MCSFS1_MAX_INODES || de[i].type != MCSFS1_MODE_FILE || !bit_test(ib, de[i].ino)) {
            return MCSFS1_ERR_CORRUPT;
        }
        struct mcsfs1_inode_disk inode;
        rc = read_inode(dev, de[i].ino, &inode);
        if (rc != 0) {
            return rc;
        }
        if (inode.mode != MCSFS1_MODE_FILE || inode.size > MCSFS1_DIRECT_BLOCKS * MCSFS1_BLOCK_SIZE) {
            return MCSFS1_ERR_CORRUPT;
        }
        uint32_t needed = (inode.size + MCSFS1_BLOCK_SIZE - 1u) / MCSFS1_BLOCK_SIZE;
        if (needed == 0u) {
            needed = 1u;
        }
        for (uint32_t j = 0; j < needed; j++) {
            uint32_t lba = inode.direct[j];
            if (lba < MCSFS1_DATA_START_LBA || lba >= dev->block_count || !bit_test(bb, lba)) {
                return MCSFS1_ERR_CORRUPT;
            }
        }
    }
    return MCSFS1_ERR_OK;
}

EOF
```

### 13.4 Menambahkan host unit test

Host unit test menggunakan RAM-backed block device. Test ini memverifikasi format, mount, fsck empty, create, duplicate create, write/read file kecil, write/read file multi-block, range error, missing file, unlink, fsck after unlink, dan corrupt-super detection.

```bash
cat > tests/m15/test_mcsfs1.c <<'EOF'
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../fs/mcsfs1/mcsfs1.h"

#define RAMBLK_BLOCKS 128u
static uint8_t disk[RAMBLK_BLOCKS][MCSFS1_BLOCK_SIZE];
static unsigned flush_count;

static int ram_read(void *ctx, uint32_t lba, void *buf512) {
    (void)ctx;
    if (lba >= RAMBLK_BLOCKS) return -1;
    memcpy(buf512, disk[lba], MCSFS1_BLOCK_SIZE);
    return 0;
}

static int ram_write(void *ctx, uint32_t lba, const void *buf512) {
    (void)ctx;
    if (lba >= RAMBLK_BLOCKS) return -1;
    memcpy(disk[lba], buf512, MCSFS1_BLOCK_SIZE);
    return 0;
}

static int ram_flush(void *ctx) {
    (void)ctx;
    flush_count++;
    return 0;
}

static int expect_int(const char *label, int got, int want) {
    if (got != want) {
        printf("FAIL %s got=%d want=%d\n", label, got, want);
        return 1;
    }
    return 0;
}

int main(void) {
    struct mcsfs1_blkdev dev = {0};
    struct mcsfs1_mount mnt = {0};
    uint8_t out[4096];
    uint32_t out_len = 0;
    int fails = 0;

    dev.block_count = RAMBLK_BLOCKS;
    dev.read = ram_read;
    dev.write = ram_write;
    dev.flush = ram_flush;

    fails += expect_int("format", mcsfs1_format(&dev), MCSFS1_ERR_OK);
    fails += expect_int("mount", mcsfs1_mount(&mnt, &dev), MCSFS1_ERR_OK);
    fails += expect_int("fsck-empty", mcsfs1_fsck(&dev), MCSFS1_ERR_OK);
    fails += expect_int("create-alpha", mcsfs1_create(&mnt, "alpha.txt"), MCSFS1_ERR_OK);
    fails += expect_int("create-duplicate", mcsfs1_create(&mnt, "alpha.txt"), MCSFS1_ERR_EXIST);

    const char msg[] = "MCSOS M15 persistent file payload";
    fails += expect_int("write-alpha", mcsfs1_write(&mnt, "alpha.txt", (const uint8_t *)msg, (uint32_t)strlen(msg)), MCSFS1_ERR_OK);
    memset(out, 0, sizeof(out));
    fails += expect_int("read-alpha", mcsfs1_read(&mnt, "alpha.txt", out, sizeof(out), &out_len), MCSFS1_ERR_OK);
    if (out_len != strlen(msg) || memcmp(out, msg, strlen(msg)) != 0) {
        printf("FAIL read-data len=%u\n", out_len);
        fails++;
    }

    uint8_t big[1400];
    for (unsigned i = 0; i < sizeof(big); i++) big[i] = (uint8_t)(i & 0xffu);
    fails += expect_int("write-big", mcsfs1_write(&mnt, "alpha.txt", big, sizeof(big)), MCSFS1_ERR_OK);
    memset(out, 0, sizeof(out));
    fails += expect_int("read-big", mcsfs1_read(&mnt, "alpha.txt", out, sizeof(out), &out_len), MCSFS1_ERR_OK);
    if (out_len != sizeof(big) || memcmp(out, big, sizeof(big)) != 0) {
        printf("FAIL read-big-data len=%u\n", out_len);
        fails++;
    }

    fails += expect_int("read-small-cap", mcsfs1_read(&mnt, "alpha.txt", out, 8, &out_len), MCSFS1_ERR_RANGE);
    fails += expect_int("missing", mcsfs1_read(&mnt, "missing", out, sizeof(out), &out_len), MCSFS1_ERR_NOENT);
    fails += expect_int("fsck-populated", mcsfs1_fsck(&dev), MCSFS1_ERR_OK);
    fails += expect_int("unlink", mcsfs1_unlink(&mnt, "alpha.txt"), MCSFS1_ERR_OK);
    fails += expect_int("read-after-unlink", mcsfs1_read(&mnt, "alpha.txt", out, sizeof(out), &out_len), MCSFS1_ERR_NOENT);
    fails += expect_int("fsck-after-unlink", mcsfs1_fsck(&dev), MCSFS1_ERR_OK);

    disk[0][0] ^= 0x55u;
    fails += expect_int("corrupt-super", mcsfs1_fsck(&dev), MCSFS1_ERR_CORRUPT);

    if (flush_count == 0) {
        printf("FAIL flush-count zero\n");
        fails++;
    }

    if (fails != 0) {
        printf("M15 host test failed: %d failures\n", fails);
        return 1;
    }
    printf("M15 host test passed: flush_count=%u\n", flush_count);
    return 0;
}

EOF
```

### 13.5 Menambahkan target Makefile M15

Makefile berikut menyediakan target `m15-all`. Target ini membangun host unit test, membangun object freestanding x86_64, membuat linked relocatable object, menjalankan audit `nm`, `readelf`, `objdump`, dan membuat checksum.

Jika repository sudah memiliki Makefile besar, gabungkan target ini tanpa merusak target M0-M14. Gunakan `CC=clang` karena opsi `-target x86_64-elf` adalah opsi driver Clang, bukan selalu tersedia pada `cc` default.

```makefile
CC ?= clang
HOST_CFLAGS := -std=c17 -Wall -Wextra -Werror -O2 -g
FREESTANDING_CFLAGS := -target x86_64-elf -std=c17 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra -Werror -O2 -g

.PHONY: m15-all clean
m15-all: artifacts/m15/test_mcsfs1 artifacts/m15/mcsfs1.o artifacts/m15/mcsfs1.rel.o
	./artifacts/m15/test_mcsfs1 | tee artifacts/m15/host_test.txt
	nm -u artifacts/m15/mcsfs1.rel.o | tee artifacts/m15/nm_undefined.txt
	test ! -s artifacts/m15/nm_undefined.txt
	readelf -h artifacts/m15/mcsfs1.rel.o | tee artifacts/m15/readelf_header.txt
	objdump -dr artifacts/m15/mcsfs1.rel.o | tee artifacts/m15/objdump.txt >/dev/null
	sha256sum artifacts/m15/* | tee artifacts/m15/SHA256SUMS.txt

artifacts/m15/test_mcsfs1: tests/m15/test_mcsfs1.c fs/mcsfs1/mcsfs1.c fs/mcsfs1/mcsfs1.h
	mkdir -p artifacts/m15
	$(CC) $(HOST_CFLAGS) -I. tests/m15/test_mcsfs1.c fs/mcsfs1/mcsfs1.c -o $@

artifacts/m15/mcsfs1.o: fs/mcsfs1/mcsfs1.c fs/mcsfs1/mcsfs1.h
	mkdir -p artifacts/m15
	$(CC) $(FREESTANDING_CFLAGS) -I. -c fs/mcsfs1/mcsfs1.c -o $@

artifacts/m15/mcsfs1.rel.o: artifacts/m15/mcsfs1.o
	ld -r $< -o $@

clean:
	rm -rf artifacts/m15

```

### 13.6 Menjalankan build dan test M15

Perintah ini menjalankan seluruh test dan audit M15.

```bash
make CC=clang m15-all
```

Indikator hasil benar adalah:

1. `M15 host test passed` muncul.
2. `artifacts/m15/nm_undefined.txt` kosong.
3. `readelf` menunjukkan `Class: ELF64`, `Type: REL`, dan `Machine: Advanced Micro Devices X86-64`.
4. `artifacts/m15/objdump.txt` tersedia.
5. `artifacts/m15/SHA256SUMS.txt` tersedia.

### 13.7 Membersihkan dan membangun ulang dari kondisi bersih

Perintah ini memastikan target tidak bergantung pada artefak lama.

```bash
make clean
make CC=clang m15-all
```

Indikator hasil benar adalah hasil test tetap sama setelah `make clean`. Jika hasil berbeda, periksa dependensi tersembunyi, file generated yang belum dinyatakan, atau environment variable yang tidak dicatat.

---

## 14. Bukti Validasi Lokal Source M15

Source code yang disediakan pada dokumen ini telah diperiksa secara lokal dalam lingkungan eksekusi terisolasi dengan target `make CC=clang m15-all`. Bukti berikut adalah cakupan validasi lokal, bukan pengganti validasi ulang di WSL 2 mahasiswa.

### 14.1 Host unit test

```text
M15 host test passed: flush_count=5
```

### 14.2 Header ELF relocatable object

```text
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00 
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0x0
  Start of program headers:          0 (bytes into file)
  Start of section headers:          42680 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           64 (bytes)
  Number of section headers:         26
  Section header string table index: 25
```

### 14.3 Checksum artefak

```text
51398b24103c7f24b278a4e19012702cd40ff7a1bba5227b1bce55e48cd96017  artifacts/m15/host_test.txt
f20835f162ad3dae2f3a4baacb7eeb3d3e9d2777a10f5644697132641258f8d7  artifacts/m15/mcsfs1.o
4073460988ff3bc43e774be8420540aeedc0b969cefe38cc78970be8a6c1221b  artifacts/m15/mcsfs1.rel.o
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  artifacts/m15/nm_undefined.txt
105754f2a192a0daac4e91c7675cd2a333849e17ee945df6e7ef2327ea6c3fe7  artifacts/m15/objdump.txt
574125e18f473e0436642c2306d708a1cd39fb5a8d2c1f677b85b8db75570f59  artifacts/m15/readelf_header.txt
d7e00dc7cd77656c39e6adba5d3061e02c44765c83d2a719f602d52e2fd07567  artifacts/m15/test_mcsfs1
```

Interpretasi bukti: host unit test lulus, object freestanding berhasil dibuat, linked relocatable object bertipe ELF64 x86-64, `nm -u` kosong, dan checksum artefak tersimpan. Validasi runtime QEMU/OVMF tetap wajib dijalankan ulang karena bergantung pada paket QEMU, OVMF, bootloader, linker script, image recipe, dan konfigurasi host mahasiswa.

---

## 15. Integrasi Konseptual dengan VFS M13 dan Block Layer M14

M15 sengaja memisahkan tiga lapisan:

1. **VFS M13** menangani file descriptor, offset, dan syscall-level file I/O.
2. **MCSFS1 M15** menangani namespace root-only, inode, directory entry, dan alokasi block.
3. **Block layer M14** menangani operasi read/write/flush berbasis LBA.

Integrasi idealnya tidak membuat VFS mengetahui layout on-disk. VFS cukup memanggil operasi filesystem seperti `create`, `read`, `write`, dan `unlink`. MCSFS1 menerjemahkan operasi tersebut menjadi pembacaan/penulisan block. Block layer meneruskan request ke RAM block driver atau driver block lain.

Untuk praktikum ini, integrasi kernel penuh boleh dilakukan sebagai tugas pengayaan. Tugas wajib adalah host test dan freestanding object audit. Integrasi VFS penuh menjadi aman setelah lock ownership, mount table, dan error propagation didokumentasikan.

---

## 16. Workflow QEMU Smoke Test

Setelah source M15 ditautkan ke kernel MCSOS, jalankan QEMU smoke test seperti pola M2-M14. Perintah aktual dapat berbeda sesuai bootloader dan image recipe repository. Contoh konservatif:

```bash
mkdir -p artifacts/m15
qemu-system-x86_64   -machine q35   -m 256M   -serial file:artifacts/m15/qemu_serial.log   -display none   -no-reboot   -no-shutdown   -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd   -cdrom build/mcsos.iso
```

Indikator hasil benar adalah kernel mencapai log boot M15 atau minimal tidak mengalami regression sebelum subsystem storage/filesystem diinisialisasi. Simpan `artifacts/m15/qemu_serial.log` sebagai bukti. Jika QEMU boot gagal, rollback ke commit M14 dan bandingkan linker map serta log serial.

---

## 17. Workflow GDB untuk Debugging

QEMU gdbstub memungkinkan debugging guest seperti low-level target. Jalankan QEMU dengan `-s -S` agar guest berhenti sampai GDB tersambung [4].

```bash
qemu-system-x86_64   -machine q35   -m 256M   -serial stdio   -display none   -s -S   -cdrom build/mcsos.iso
```

Pada terminal lain:

```bash
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) break mcsfs1_format
(gdb) break mcsfs1_mount
(gdb) break mcsfs1_fsck
(gdb) continue
```

Gunakan breakpoint ini hanya jika simbol M15 benar-benar ditautkan ke kernel image. Jika GDB melaporkan simbol tidak ditemukan, periksa apakah object M15 sudah masuk `kernel.elf` melalui `nm build/kernel.elf | grep mcsfs1`.

---

## 18. Fault Injection yang Wajib Dicoba

Fault injection dilakukan pada host test atau test tambahan, bukan pada data penting. Tujuannya memastikan fsck-lite dapat mendeteksi kelas korupsi minimum.

| Fault | Cara simulasi | Ekspektasi |
|---|---|---|
| Superblock magic rusak | Ubah byte pada block 0 | `mcsfs1_fsck` mengembalikan `MCSFS1_ERR_CORRUPT`. |
| Root inode salah mode | Ubah mode root menjadi file | `mcsfs1_mount` atau `mcsfs1_fsck` gagal. |
| Directory entry menunjuk inode bebas | Hapus bit inode tetapi biarkan dirent | `mcsfs1_fsck` gagal. |
| Direct block keluar range | Ubah direct block ke `block_count + 1` | `mcsfs1_fsck` gagal. |
| Nama terlalu panjang | Panggil `mcsfs1_create` dengan nama >27 byte | Mengembalikan `MCSFS1_ERR_NAMETOOLONG`. |
| File terlalu besar | `mcsfs1_write` dengan len >4096 | Mengembalikan `MCSFS1_ERR_RANGE`. |
| Directory penuh | Buat lebih dari 16 file | Mengembalikan `MCSFS1_ERR_NOSPC`. |

---

## 19. Failure Modes dan Prosedur Perbaikan

| Failure mode | Gejala | Root cause yang mungkin | Perbaikan |
|---|---|---|---|
| Wrong magic/version | Mount gagal | Block 0 bukan MCSFS1 atau write superblock gagal | Jalankan ulang `mcsfs1_format`; audit `dev_write`. |
| Bitmap metadata tidak reserved | Fsck gagal atau data menimpa metadata | Block 0-7 tidak ditandai used | Periksa loop reserved block pada `mcsfs1_format`. |
| Root directory hilang | Mount gagal | Root inode atau root block rusak | Periksa `root.direct[0]` dan bit block 7. |
| Duplicate name bisa dibuat | Dua dirent untuk nama sama | Lookup salah | Periksa `find_dirent` dan pembandingan panjang nama. |
| Data multi-block salah | File >512 byte rusak | Chunk loop salah | Audit `written`, `copied`, `blocks_needed`. |
| Undefined symbol pada object | `nm -u` tidak kosong | Memanggil libc/helper compiler | Hilangkan hosted calls dan compile dengan freestanding flags. |
| Stack terlalu besar | Kernel stack early boot habis | Array 512 byte lokal dipakai pada path dalam | Batasi nested call atau pindahkan buffer ke caller/static setelah allocator siap. |
| Flush tidak terpanggil | Data/metadata stale pada model writeback | Lupa `dev_flush` | Pastikan operasi metadata sukses memanggil flush. |
| Fsck tidak mendeteksi korupsi | False negative | Invariant belum lengkap | Tambahkan check bitmap, inode mode, LBA, dan file size. |
| QEMU regression | Boot berhenti sebelum log M15 | Linker script/order/object salah | Periksa linker map, serial log, dan rollback ke commit M14. |

---

## 20. Prosedur Rollback

Rollback harus menjaga repository dapat kembali ke state M14.

```bash
git status --short
git diff > artifacts/m15/m15_failed_attempt.diff
git restore fs/mcsfs1 tests/m15 Makefile
git clean -fd artifacts/m15
git switch main
```

Jika perubahan Makefile sudah tercampur dengan target lama, jangan langsung `git restore Makefile` tanpa menyimpan diff. Pisahkan perubahan target M15 dan pastikan target M0-M14 masih dapat berjalan.

---

## 21. Checkpoint Buildable

| Checkpoint | Perintah | Evidence |
|---|---|---|
| CP15-1 Preflight | `./scripts/m15_preflight.sh` | `artifacts/m15/preflight.txt` |
| CP15-2 Host compile | `make CC=clang artifacts/m15/test_mcsfs1` | Binary host test terbentuk |
| CP15-3 Host test | `./artifacts/m15/test_mcsfs1` | `M15 host test passed` |
| CP15-4 Freestanding object | `make CC=clang artifacts/m15/mcsfs1.o` | Object x86_64 terbentuk |
| CP15-5 Relocatable link | `make CC=clang artifacts/m15/mcsfs1.rel.o` | `mcsfs1.rel.o` terbentuk |
| CP15-6 Undefined symbol audit | `nm -u artifacts/m15/mcsfs1.rel.o` | Output kosong |
| CP15-7 ELF audit | `readelf -h artifacts/m15/mcsfs1.rel.o` | ELF64 REL x86-64 |
| CP15-8 Disassembly audit | `objdump -dr artifacts/m15/mcsfs1.rel.o` | `artifacts/m15/objdump.txt` |
| CP15-9 Checksum | `sha256sum artifacts/m15/*` | `SHA256SUMS.txt` |
| CP15-10 QEMU smoke | QEMU command repository | `qemu_serial.log` |

---

## 22. Tugas Implementasi

### 22.1 Tugas wajib

1. Implementasikan source MCSFS1 sesuai dokumen ini.
2. Jalankan `make CC=clang m15-all` dari clean checkout.
3. Simpan seluruh artifact M15.
4. Tambahkan minimal satu fault injection tambahan selain corrupt-super.
5. Jelaskan invariant yang dibuktikan oleh host unit test.
6. Jelaskan batasan crash consistency MCSFS1.
7. Jelaskan bagaimana MCSFS1 akan dihubungkan ke VFS M13.
8. Buat commit Git dengan pesan `M15: add MCSFS1 minimal persistent filesystem`.

### 22.2 Tugas pengayaan

1. Tambahkan operasi `stat` untuk membaca ukuran file.
2. Tambahkan test directory penuh.
3. Tambahkan fsck check bahwa tidak ada dua dirent menunjuk inode sama.
4. Tambahkan block leak detection sederhana.
5. Tambahkan mount flag read-only bila fsck gagal.
6. Integrasikan MCSFS1 sebagai mount backend ops pada VFS M13.

### 22.3 Tantangan riset

1. Rancang mini-journal metadata-only untuk `create` dan `unlink`.
2. Rancang recovery idempotent untuk operasi setengah selesai.
3. Rancang fsck repair untuk orphan inode dan lost block.
4. Bandingkan MCSFS1 dengan ext2 pada aspek layout dan crash model.
5. Usulkan strategi fuzzing image parser MCSFS1.

---

## 23. Pertanyaan Analisis

1. Mengapa superblock harus memiliki magic number dan version?
2. Mengapa metadata block harus ditandai used pada block bitmap?
3. Apa risiko jika directory entry ditulis sebelum inode selesai ditulis?
4. Apa risiko jika bitmap ditulis sebelum data block selesai ditulis?
5. Mengapa M15 belum boleh disebut crash-consistent?
6. Apa perbedaan fsck detection dan fsck repair?
7. Mengapa source freestanding tidak boleh memakai `printf` atau `malloc`?
8. Mengapa `nm -u` harus kosong untuk linked relocatable object M15?
9. Bagaimana desain MCSFS1 berubah jika mendukung subdirectory?
10. Bagaimana desain MCSFS1 berubah jika mendukung file lebih besar dari 4096 byte?
11. Apa akibat security jika nama file tidak divalidasi?
12. Bagaimana MCSFS1 harus berinteraksi dengan lock M12 jika ada multi-threaded file I/O?

---

## 24. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | Format, mount, create, write, read, unlink, dan fsck-lite berjalan sesuai kontrak. |
| Kualitas desain dan invariants | 20 | Layout, inode, bitmap, root directory, error code, dan batasan desain terdokumentasi. |
| Pengujian dan bukti | 20 | Host test, freestanding compile, `nm`, `readelf`, `objdump`, checksum, QEMU log, dan clean rebuild tersedia. |
| Debugging/failure analysis | 10 | Failure modes, root cause, dan solusi perbaikan dijelaskan dengan bukti. |
| Keamanan dan robustness | 10 | Validasi nama, range, size, LBA, corrupt metadata, dan trust boundary dibahas. |
| Dokumentasi/laporan | 10 | Laporan lengkap, rapi, memakai template, menyertakan commit hash dan referensi. |

Nilai maksimum hanya diberikan jika mahasiswa menunjukkan artefak nyata, bukan hanya screenshot parsial. Jika QEMU tidak dapat dijalankan karena environment, mahasiswa wajib menjelaskan penyebab, melampirkan host test dan object audit lengkap, serta menyusun rencana validasi ulang.

---

## 25. Kriteria Lulus Praktikum

Kriteria minimum M15:

1. Proyek dapat dibangun dari clean checkout.
2. Perintah build terdokumentasi.
3. `make CC=clang m15-all` berhasil pada host WSL 2 atau environment setara.
4. Host unit test MCSFS1 lulus.
5. Freestanding object x86_64 berhasil dibuat.
6. `nm -u artifacts/m15/mcsfs1.rel.o` kosong.
7. `readelf -h` menunjukkan ELF64 relocatable x86-64.
8. `objdump` disimpan sebagai evidence.
9. Checksum artefak disimpan.
10. QEMU smoke test dijalankan ulang atau alasan teknis kegagalan environment dicatat secara jujur.
11. Failure mode dan rollback dijelaskan.
12. Perubahan Git dikomit.
13. Laporan memakai template standar dan mencantumkan log/screenshot yang cukup.

---

## 26. Template Laporan Praktikum M15

Gunakan struktur berikut pada laporan.

```markdown
# Laporan Praktikum M15 - MCSFS1 Persistent Filesystem Minimal

## 1. Sampul
- Judul praktikum:
- Nama mahasiswa / kelompok:
- NIM:
- Kelas:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi Pendidikan Teknologi Informasi
- Institut Pendidikan Indonesia

## 2. Tujuan
Tuliskan capaian teknis dan konseptual M15.

## 3. Dasar Teori Ringkas
Jelaskan VFS, block device, superblock, inode, directory entry, bitmap, direct block, dan fsck-lite.

## 4. Lingkungan
- OS host:
- WSL distro:
- Compiler:
- Linker:
- QEMU:
- GDB:
- Target architecture:
- Commit hash:

## 5. Desain
- Diagram layout block MCSFS1.
- Struktur superblock.
- Struktur inode.
- Struktur directory entry.
- Invariant I15-01 sampai I15-10.
- Batasan desain.

## 6. Langkah Kerja
Tuliskan perintah, perubahan file, dan alasan teknis.

## 7. Hasil Uji
Sertakan:
- `artifacts/m15/preflight.txt`
- `artifacts/m15/host_test.txt`
- `artifacts/m15/nm_undefined.txt`
- `artifacts/m15/readelf_header.txt`
- `artifacts/m15/objdump.txt`
- `artifacts/m15/SHA256SUMS.txt`
- `artifacts/m15/qemu_serial.log` bila tersedia

## 8. Analisis
Jelaskan keberhasilan, bug yang ditemukan, failure modes, dan perbandingan dengan teori.

## 9. Keamanan dan Reliability
Jelaskan risiko metadata corruption, stale data, power-loss, path traversal, range violation, dan mitigasi.

## 10. Kesimpulan
Tuliskan apa yang berhasil, apa yang belum, dan rencana perbaikan.

## 11. Lampiran
Sertakan potongan kode penting, diff ringkas, log penuh, dan referensi IEEE.
```

---

## 27. Verification Matrix

| Requirement | Evidence | Status yang diharapkan |
|---|---|---|
| M15-REQ-01 Format filesystem | Host test `format` | Pass |
| M15-REQ-02 Mount filesystem | Host test `mount` | Pass |
| M15-REQ-03 Fsck empty | Host test `fsck-empty` | Pass |
| M15-REQ-04 Create file | Host test `create-alpha` | Pass |
| M15-REQ-05 Duplicate detection | Host test `create-duplicate` | Pass |
| M15-REQ-06 Write/read small file | Host test `write-alpha`, `read-alpha` | Pass |
| M15-REQ-07 Write/read multi-block file | Host test `write-big`, `read-big` | Pass |
| M15-REQ-08 Range validation | Host test `read-small-cap` | Pass |
| M15-REQ-09 Missing file error | Host test `missing` | Pass |
| M15-REQ-10 Unlink | Host test `unlink`, `read-after-unlink` | Pass |
| M15-REQ-11 Corrupt superblock detection | Host test `corrupt-super` | Pass |
| M15-REQ-12 Freestanding object | `mcsfs1.o` | Pass |
| M15-REQ-13 Undefined symbol audit | `nm_undefined.txt` empty | Pass |
| M15-REQ-14 ELF audit | `readelf_header.txt` | Pass |
| M15-REQ-15 Disassembly audit | `objdump.txt` | Pass |
| M15-REQ-16 Checksum | `SHA256SUMS.txt` | Pass |
| M15-REQ-17 QEMU smoke | `qemu_serial.log` | Pass or documented environment limitation |


---

## Architecture and Design

Architecture M15 terdiri atas empat boundary eksplisit. Boundary pertama adalah VFS M13 yang membawa semantics operasi file seperti open, read, write, close, dan unlink dalam bentuk POSIX-like subset. Boundary kedua adalah MCSFS1 sebagai filesystem implementation yang memelihara superblock, inode table, directory entry, bitmap allocation, dan error propagation. Boundary ketiga adalah block layer M14 yang menerima operation read/write/flush berbasis LBA. Boundary keempat adalah block driver RAM-backed pada host unit test atau driver block kernel pada integrasi lanjutan.

Design MCSFS1 bersifat root-only. Semua pathname yang diterima M15 adalah nama file tunggal, bukan path bertingkat. Keputusan ini mengurangi surface area parser, mencegah traversal kompleks, dan membuat invariant directory mudah diuji. Format on-disk tidak memakai struktur compiler-dependent antarplatform sebagai kontrak final; pada tahap pendidikan ini struktur C dipakai sebagai scaffolding dan wajib disertai batasan bahwa kompatibilitas lintas compiler/endianness belum menjadi goal.

System decomposition M15 adalah: `mcsfs1_format` membangun layout; `mcsfs1_mount` memvalidasi superblock dan root inode; `mcsfs1_create` mengalokasikan inode dan block awal; `mcsfs1_write` mengalokasikan direct block dan menulis payload; `mcsfs1_read` membaca payload sesuai inode size; `mcsfs1_unlink` menghapus directory entry serta membebaskan inode/block; `mcsfs1_fsck` memeriksa invariant minimum. Setiap operation mengembalikan error code negatif sehingga caller kernel tidak bergantung pada `errno` hosted libc.

## Filesystem Contract

Contract MCSFS1 menetapkan semantics minimum berikut. `format` menghapus seluruh block device uji dan menulis metadata baru. `mount` hanya boleh berhasil jika superblock, root inode, dan root directory memenuhi invariant. `create` gagal dengan `MCSFS1_ERR_EXIST` bila nama sudah ada, gagal dengan `MCSFS1_ERR_NAMETOOLONG` bila nama melebihi batas, dan gagal dengan `MCSFS1_ERR_NOSPC` bila directory, inode, atau block habis. `write` mengganti isi file dari offset nol sampai `len`, bukan append. `read` membaca seluruh file dan gagal dengan `MCSFS1_ERR_RANGE` bila buffer caller terlalu kecil. `unlink` menghapus file regular root-only dan membebaskan resource.

Error semantics wajib stabil selama M15. Caller tidak boleh menebak keadaan internal; caller hanya membaca return code. Operation yang gagal tidak dijamin atomic penuh karena M15 belum memiliki journal. Oleh sebab itu, setelah fault injection atau kegagalan I/O, caller wajib menjalankan `mcsfs1_fsck` sebelum mount read-write berikutnya.

## Implementation Plan

Implementation plan M15 dilakukan dalam checkpoint kecil. Pertama, tambahkan header dan API tanpa integrasi kernel. Kedua, implementasikan helper freestanding agar object tidak memanggil libc. Ketiga, implementasikan superblock dan bitmap format. Keempat, implementasikan inode table dan root directory lookup. Kelima, implementasikan create/write/read/unlink. Keenam, implementasikan fsck-lite. Ketujuh, jalankan host unit test. Kedelapan, jalankan freestanding compile dan ELF audit. Kesembilan, integrasikan secara opsional ke VFS M13 melalui operation table dengan lock eksternal. Kesepuluh, jalankan QEMU smoke test dan dokumentasikan serial log.

Rollback point ditetapkan setelah setiap checkpoint. Jika host unit test gagal, jangan lanjut ke integrasi kernel. Jika object freestanding gagal, hilangkan seluruh dependensi hosted sebelum mencoba QEMU. Jika QEMU gagal setelah integrasi, rollback ke commit M14 dan bandingkan linker map.

## Security and Threat Model

Threat model M15 terbatas pada bug internal kernel, metadata image yang rusak, nama file invalid, ukuran file di luar batas, LBA di luar range, dan caller internal yang salah memakai API. M15 belum mempertahankan boundary terhadap pengguna jahat karena user/kernel copy, credential, permission, ACL, xattr, quota, encryption, dan capability model belum menjadi bagian implementasi M15.

Security property minimum adalah fail-closed pada mount bila magic/version/layout tidak cocok, validasi nama untuk menolak string kosong dan karakter `/`, validasi range LBA, validasi ukuran file maksimum, serta fsck-lite untuk mendeteksi korupsi metadata dasar. Privilege enforcement, DAC, MAC, capability, dan audit event security akan menjadi tahap lanjutan setelah identity dan permission model tersedia.


## Compatibility and Feature Versioning

Compatibility M15 dibatasi pada custom image MCSFS1. Format tidak dimaksudkan kompatibel dengan POSIX filesystem penuh, ext2, FAT, atau filesystem lain. Field `version` pada superblock adalah feature gate awal: mount harus menolak version yang tidak dikenali sampai migration policy tersedia. Feature flag belum diimplementasikan, tetapi laporan harus menyebut bahwa perubahan layout masa depan wajib menaikkan version atau menambah feature compatibility bits agar image lama tidak dimount secara keliru.

## Concurrency, Reference, and Lifetime Model

Concurrency M15 diasumsikan single-core atau dilindungi lock eksternal dari VFS/filesystem layer. Tidak ada internal mutex pada source M15 karena dokumen ini memprioritaskan struktur on-disk dan invariant persistence terlebih dahulu. Jika digunakan pada kernel multi-threaded, caller wajib memegang filesystem-wide lock selama create/write/unlink dan minimal shared/read lock selama read/fsck.

Reference dan lifetime rule: `struct mcsfs1_mount` tidak memiliki block device; ia hanya menyimpan pointer pinjaman ke `struct mcsfs1_blkdev`. Block device harus hidup lebih lama daripada mount object. Buffer lokal 512 byte hidup hanya selama stack frame fungsi. Directory entry dan inode on-disk tidak boleh dipakai sebagai pointer jangka panjang; setiap operasi harus membaca ulang metadata atau memakai cache dengan invalidation rule yang jelas. Deadlock utama yang harus dihindari pada integrasi lanjutan adalah urutan lock VFS -> filesystem -> buffer cache -> block device; urutan sebaliknya tidak boleh dilakukan.

## Validation Plan

Validation plan M15 terdiri atas unit test host, freestanding build, static object audit, fault injection, QEMU smoke test, dan dokumentasi evidence. Unit test host memverifikasi operation semantics tanpa boot dependency. Freestanding build memverifikasi bahwa kode dapat menjadi object kernel. `nm`, `readelf`, dan `objdump` memverifikasi simbol, format ELF, dan disassembly. Fault injection memverifikasi deteksi korupsi superblock dan dapat diperluas ke bitmap/inode corruption. QEMU smoke test memastikan penambahan object tidak menyebabkan boot regression.

Fuzzing tahap awal dapat dilakukan dengan membuat mutasi random pada block 0-7 lalu menjalankan `mcsfs1_fsck` untuk memastikan tidak terjadi crash host test. Crash testing penuh belum diwajibkan pada M15 karena belum ada journal atau write-order protocol. Benchmark awal dapat mencatat latency host test, throughput read/write RAM-backed block, dan waktu fsck untuk image 128 block; data benchmark ini hanya baseline, bukan klaim performa kernel.

## Failure Modes and Mitigations

Failure modes and mitigations M15 mencakup corruption, diagnostic, recovery, dan rollback. Superblock corruption didiagnosis oleh magic/version mismatch dan dimitigasi dengan menolak mount. Bitmap corruption didiagnosis oleh fsck-lite dan dimitigasi dengan read-only fallback atau reformat pada image latihan. Directory corruption didiagnosis bila dirent menunjuk inode bebas atau inode invalid. Data corruption pada payload belum memiliki checksum sehingga hanya dapat dideteksi bila metadata ikut rusak. Recovery otomatis belum tersedia; prosedur recovery M15 adalah backup image, jalankan fsck-lite, catat fault, dan reformat media latihan bila invariant tidak dapat dipulihkan.

Mitigasi engineering adalah menjaga operation ordering, memanggil flush eksplisit setelah metadata update, menyimpan serial log, menjaga checksum artefak build, dan membuat rollback commit. Untuk integrasi lanjutan, tambahkan journal metadata-only atau copy-on-write update agar create/unlink dapat dipulihkan secara idempotent.

## Acceptance Criteria

Acceptance criteria M15 adalah evidence-based gate. Praktikum lulus bila host unit test lulus, freestanding object x86_64 terbentuk, undefined symbol audit kosong, ELF audit benar, disassembly tersimpan, checksum tersedia, preflight tercatat, failure modes dianalisis, dan QEMU smoke test dijalankan atau keterbatasan environment dicatat. Acceptance tidak mencakup production readiness, data safety pada media nyata, POSIX full compliance, atau crash consistency penuh.

## Assumptions and Scope

Assumptions and scope lintas-disiplin M15 adalah OS pendidikan, target x86_64, environment QEMU/WSL 2, storage latihan berbasis RAM block device, risk class utama berupa correctness dan data integrity, serta evidence baseline berupa host test dan object audit. Scope tidak mencakup hardware storage nyata, DMA, IOMMU, thermal/power management, compliance formal, atau fleet operations.

## Cross-Science Map

| Domain | Transfer ke M15 |
|---|---|
| Systems engineering | Requirements, interface, traceability, verification matrix, dan gate readiness. |
| Mathematics/formal reasoning | Invariant, model state, bitmap allocation, dan batas ukuran file. |
| Statistics/performance | Benchmark latency/throughput host test, confidence melalui clean rebuild berulang, dan baseline fsck time. |
| Reliability/safety | Hazard metadata corruption, fault injection, recovery manual, dan availability terbatas pada image latihan. |
| Control/physics/hardware | Timer/QEMU smoke, block device abstraction, DMA/MMIO belum masuk scope, dan power-loss sebagai hazard terdokumentasi. |
| Human/governance | Documentation, operator rollback, support evidence, compliance akademik, dan ethics penggunaan data latihan non-nyata. |

## Models and Invariants

Model M15 memakai state machine file `Absent -> CreatedEmpty -> Written -> Deleted` dan state `Corrupt` untuk metadata yang melanggar invariant. Invariant matematika utama adalah bijeksi parsial antara directory entry aktif dan inode aktif, serta relasi subset antara direct block file dan block bitmap used. Complexity bound operasi lookup adalah O(16) karena root directory memiliki 16 slot. Complexity bound alokasi inode adalah O(32). Complexity bound alokasi block adalah O(number_of_blocks). Batas ini kecil dan dapat diterima untuk praktikum.

## Implementation Transfer

Implementation transfer ke subsystem lain adalah sebagai berikut. Ke VFS M13, MCSFS1 menyediakan backend operation untuk create/read/write/unlink. Ke block layer M14, MCSFS1 memakai read/write/flush berbasis LBA. Ke synchronization M12, integrasi kernel harus menambah filesystem-wide lock. Ke security tahap lanjut, MCSFS1 harus menerima credential dan permission check. Ke observability tahap lanjut, operasi MCSFS1 harus menambah counter mount, fsck_fail, read, write, unlink, nospc, dan corrupt_detected.

---

## 28. Readiness Review

| Area | Evidence minimum | M15 status |
|---|---|---|
| Build | Clean build, host test, freestanding object | Siap uji bila `make CC=clang m15-all` lulus |
| Functional | Format, mount, create, write, read, unlink, fsck-lite | Siap uji host |
| Debuggability | `readelf`, `objdump`, `nm`, GDB workflow | Siap audit object |
| Runtime | QEMU smoke log | Harus diuji ulang di WSL 2 mahasiswa |
| Security | Validasi nama/range/size | Baseline, belum access-control penuh |
| Reliability | Flush eksplisit dan fsck-lite | Belum crash-consistency penuh |
| Release | Dokumentasi dan rollback | Siap demonstrasi praktikum terbatas bila evidence lengkap |

Kesimpulan readiness: hasil M15 hanya boleh dinyatakan **siap uji QEMU untuk filesystem persistent minimal MCSFS1** atau **siap demonstrasi praktikum terbatas** jika seluruh evidence minimum tersedia. Hasil M15 belum boleh dinyatakan aman untuk data nyata, belum siap produksi, dan belum membuktikan crash consistency terhadap power-loss arbitrer.

---

## References

[1] Linux Kernel Documentation, "Overview of the Linux Virtual File System," The Linux Kernel documentation. [Online]. Available: https://docs.kernel.org/filesystems/vfs.html. Accessed: 2026-05-03.

[2] Linux Kernel Documentation, "The Second Extended Filesystem," The Linux Kernel documentation. [Online]. Available: https://www.kernel.org/doc/html/v6.6/filesystems/ext2.html. Accessed: 2026-05-03.

[3] Linux Kernel Documentation, "Buffer Heads," The Linux Kernel documentation. [Online]. Available: https://docs.kernel.org/filesystems/buffer.html. Accessed: 2026-05-03.

[4] QEMU Project, "GDB usage," QEMU documentation. [Online]. Available: https://qemu-project.gitlab.io/qemu/system/gdb.html. Accessed: 2026-05-03.

[5] LLVM Project, "Clang command line argument reference," Clang documentation. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html. Accessed: 2026-05-03.

[6] GNU Project, "GNU Binary Utilities," GNU Binutils documentation. [Online]. Available: https://www.sourceware.org/binutils/docs/binutils.html. Accessed: 2026-05-03.
