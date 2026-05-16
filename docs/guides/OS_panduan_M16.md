# OS_panduan_M16.md

# Panduan Praktikum M16 - Crash Consistency, Write-Ahead Journal, Recovery, dan Fault-Injection Test untuk MCSFS1J pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M16  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: siap uji QEMU untuk mekanisme crash-consistency filesystem pendidikan berbasis write-ahead journal. Status ini bukan bukti filesystem aman terhadap semua power-loss nyata, bukan bukti durability POSIX penuh, bukan bukti kompatibilitas ext4, dan bukan bukti siap produksi.

---

## 1. Ringkasan Praktikum

Praktikum M16 melanjutkan M15. Sampai M15, MCSOS telah memiliki filesystem persistent minimal bernama MCSFS1 yang memakai superblock, inode bitmap, block bitmap, inode table, root directory, direct block, dan fsck-lite. Keterbatasan utama M15 adalah operasi metadata dan data masih bergantung pada clean shutdown atau flush eksplisit. Apabila sistem berhenti di tengah pembaruan beberapa blok, filesystem dapat berada pada keadaan antara: bitmap sudah berubah tetapi inode belum berubah, directory entry menunjuk inode yang belum lengkap, atau blok data sudah dialokasikan tetapi tidak dapat dijangkau oleh directory.

M16 memperkenalkan **MCSFS1J**, yaitu penyempurnaan MCSFS1 dengan **write-ahead journal** sederhana. Prinsipnya adalah: sebelum blok metadata/data kritis ditulis ke lokasi utama, salinan blok target ditulis lebih dahulu ke area journal bersama descriptor dan checksum. Setelah semua payload journal tersedia, kernel menulis commit record. Pada mount berikutnya, recovery memeriksa commit record, descriptor, checksum, dan target LBA. Jika transaksi valid, replay menyalin payload journal ke lokasi utama secara idempotent lalu mengosongkan journal.

Rancangan ini mengikuti pola konseptual journaling yang dipakai filesystem nyata, tetapi dibuat jauh lebih kecil untuk praktikum. Dokumentasi Linux JBD2 menjelaskan bahwa journaling layer mengelola state transaksi outstanding dan proses penulisan log untuk filesystem [1]. Dokumentasi ext4 menyatakan bahwa journal melindungi filesystem dari inkonsistensi metadata ketika terjadi crash, dan transaksi yang sudah memiliki commit record dapat direplay sampai commit record terakhir [2]. Dokumentasi ext4 juga membedakan mode writeback, ordered, dan journal; M16 tidak mengimplementasikan seluruh mode ext4, tetapi memakai pendekatan metadata/data journal minimal yang dapat diuji pada host [3].

M16 tidak menggantikan fsck-lite. Journal dan fsck memiliki fungsi berbeda. Journal mempercepat recovery setelah crash pada transaksi yang sudah commit, sedangkan fsck-lite tetap diperlukan untuk mendeteksi korupsi metadata, descriptor journal rusak, bitmap mismatch, stale inode, dan directory entry invalid. Karena itu, M16 mensyaratkan dua bukti: replay journal harus berhasil pada skenario crash yang terkontrol, dan fsck-lite harus lulus setelah recovery.

Workflow debugging tetap mengikuti pola M0-M15. QEMU gdbstub dapat dijalankan dengan `-s -S` agar guest berhenti menunggu GDB, lalu GDB dapat memeriksa register dan memori guest [4]. Kompilasi freestanding memakai Clang dengan target `x86_64-elf` dan flag freestanding; Clang menyediakan dokumentasi command-line reference untuk opsi driver [5]. Audit object memakai GNU Binutils seperti `nm`, `readelf`, dan `objdump`, yang memang termasuk utilitas utama untuk inspeksi object dan ELF [6]. GNU Make dipakai untuk orkestrasi build karena manual resminya mendefinisikan makefile sebagai file yang menjelaskan relasi file dan perintah untuk memperbarui artefak [7].

Keberhasilan M16 tidak boleh dilaporkan sebagai "filesystem sudah tanpa error". Rumusan yang valid adalah: **mekanisme journal MCSFS1J siap uji QEMU dan host fault-injection terbatas**, dengan bukti host unit test, freestanding object audit, checksum, dan rencana integrasi kernel.

---

## 2. Assumptions, Scope, and Target Matrix

| Aspek | Keputusan M16 |
|---|---|
| Architecture | x86_64 long mode |
| Host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Kernel model | Monolithic teaching kernel |
| Bahasa | C17 freestanding untuk kernel; C17 hosted untuk host unit test |
| Toolchain | Clang target `x86_64-elf`, GNU `make`, `nm`, `readelf`, `objdump`, `sha256sum` |
| Target triple | `x86_64-elf` |
| Subsystem utama | Crash-consistency filesystem, write-ahead journal, replay, checksum, fault injection |
| Basis sebelumnya | M13 VFS minimal, M14 block device layer/buffer cache, M15 MCSFS1 persistent filesystem |
| Storage model | RAM-backed block device untuk host test; block device M14 untuk integrasi kernel |
| Crash model | Crash setelah commit record dan sebelum home-location write harus dapat direplay. Crash sebelum commit record tidak dijanjikan menjadi durable. |
| Concurrency model | Single-core educational baseline. Locking eksternal filesystem/VFS diperlukan bila dipakai bersama scheduler/thread M9-M12. |
| Security posture | Validasi magic, version, count, target LBA, checksum, dan fail-closed saat journal corrupt. Belum ada MAC/DAC/capability penuh. |
| Status readiness | Siap uji QEMU dan siap host fault-injection terbatas, bukan siap produksi. |

---

## 2A. Goals and Non-goals

**Goals M16** adalah memperkenalkan write-ahead journal sederhana, menyediakan commit record, descriptor, payload checksum, replay idempotent, recovery pada mount, fail-closed saat journal rusak, fault-injection test pada host, dan audit object freestanding x86_64.

**Non-goals M16** adalah kompatibilitas ext4/JBD2, delayed allocation, ordered mode penuh, full-data journaling POSIX, fsync POSIX lengkap, multi-transaction concurrency, checkpoint daemon, writeback cache kompleks, barrier/FUA perangkat nyata, disk scheduler, AHCI/NVMe, journaling directory bertingkat, snapshot, copy-on-write, encryption, quota, xattr, dan production readiness.

---

## 2B. Assumptions and Target

Assumptions and target M16 adalah: filesystem role berupa teaching filesystem; interface target berupa internal VFS-like API; storage model berupa block device M14; crash model terbatas pada transaksi tunggal yang commit record-nya sudah tertulis; target test utama berupa host unit test dan QEMU smoke test; dan evidence baseline berupa host test, freestanding object audit, checksum, serta log QEMU/GDB. Asumsi ini wajib dicatat pada laporan karena klaim kebenaran M16 hanya berlaku dalam batas tersebut.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M16, mahasiswa mampu:

1. Menjelaskan perbedaan clean shutdown, fsck-only recovery, journaling, commit record, checkpoint, replay, dan idempotence.
2. Mendesain format journal sederhana yang memuat header, descriptor, payload block, target LBA, checksum, dan transaction sequence.
3. Menjelaskan mengapa write-ahead journal harus menulis payload journal sebelum commit record.
4. Mengimplementasikan replay journal saat mount sebelum filesystem digunakan.
5. Menguji skenario crash setelah commit record tetapi sebelum home-location write.
6. Menguji skenario corrupt journal agar recovery gagal secara terkendali, bukan menulis ke target yang tidak valid.
7. Mengompilasi source menjadi host binary dan freestanding x86_64 object tanpa undefined symbol.
8. Menghasilkan bukti `make`, host unit test, `nm -u`, `readelf -h`, `objdump -dr`, `sha256sum`, dan rencana QEMU smoke test.
9. Menganalisis failure modes: torn journal, descriptor corrupt, checksum mismatch, stale journal, partial transaction, no-space, dan layout mismatch.
10. Menentukan batas readiness: siap uji QEMU, belum siap production storage.

---

## 4. Prasyarat Teori

Mahasiswa wajib menguasai materi berikut sebelum mengerjakan M16.

| Area | Prasyarat minimal | Bukti penguasaan |
|---|---|---|
| M13 VFS | File descriptor, VFS object, RAMFS/VFS call path | Menjelaskan jalur `open/read/write/close` internal |
| M14 block layer | LBA, block size, buffer cache, read/write block | Menunjukkan log block read/write dan checksum object |
| M15 MCSFS1 | Superblock, inode, bitmap, root directory, fsck-lite | Menjelaskan invariant `inode bitmap == inode.used` |
| Crash consistency | Atomicity, durability, ordering, replay, idempotence | Menjelaskan kasus crash sebelum/sesudah commit record |
| Low-level C | Freestanding C, fixed-width integer, alignment, no hidden libc | Audit `nm -u` kosong pada object kernel |
| Debugging | QEMU serial log, GDB remote, readelf/objdump | Menunjukkan `readelf` dan `objdump` artefak |

---

## 5. Peta Skill yang Digunakan

| Skill | Fungsi dalam M16 |
|---|---|
| `@osdev-general` | Readiness gate, integrasi milestone, rollback, acceptance criteria |
| `@osdev-01-computer-foundation` | State machine journal, invariant, idempotence, failure model |
| `@osdev-02-low-level-programming` | C17 freestanding, ELF object, no hidden libc, object audit |
| `@osdev-03-computer-and-hardware-architecture` | LBA, storage ordering, emulator vs hardware boundary |
| `@osdev-04-kernel-development` | Mount-time recovery, panic/fail-closed, locking boundary |
| `@osdev-05-filesystem-development` | VFS, inode, directory, journal, fsck, crash consistency |
| `@osdev-07-os-security` | Fail-closed pada journal corrupt, input validation, integrity check |
| `@osdev-08-device-driver-development` | Interaksi block device, write path, storage error propagation |
| `@osdev-12-toolchain-devenv` | Makefile, Clang target, nm/readelf/objdump/sha256sum, reproducibility |
| `@osdev-14-cross-science` | Verification matrix, risk register, fault injection, evidence synthesis |

---

## 6. Alat dan Versi

Gunakan versi aktual yang tersedia di WSL 2. Jangan menyalin versi contoh di bawah sebagai bukti; mahasiswa wajib menjalankan command di lingkungan masing-masing.

```bash
uname -a
lsb_release -a || cat /etc/os-release
clang --version
make --version
qemu-system-x86_64 --version
nm --version | head -n 1
readelf --version | head -n 1
objdump --version | head -n 1
sha256sum --version | head -n 1
git --version
```

Indikator hasil: semua command mencetak versi, bukan `command not found`. Jika `qemu-system-x86_64` tidak tersedia, instal paket QEMU sesuai panduan M0/M1. Jika `clang` tidak mengenali `-target x86_64-elf`, gunakan paket LLVM/Clang yang lebih lengkap atau fallback cross toolchain sesuai catatan dosen.

---

## 7. Repository Awal dan Struktur Direktori

Gunakan repository hasil M15. Buat branch baru agar rollback mudah.

```bash
cd ~/mcsos
mkdir -p kernel/fs/mcsfs1j tests/m16 scripts build/m16 logs/m16 evidence/m16
git checkout -b praktikum-m16-journal-recovery
```

Struktur target M16:

```text
mcsos/
├── kernel/
│   └── fs/
│       └── mcsfs1j/
│           └── m16_mcsfs_journal.c
├── tests/
│   └── m16/
│       └── Makefile
├── scripts/
│   ├── m16_preflight.sh
│   └── m16_grade.sh
├── build/
│   └── m16/
├── logs/
│   └── m16/
└── evidence/
    └── m16/
```

---

## 8. Pemeriksaan Kesiapan M0-M15

Sebelum menulis source M16, jalankan pemeriksaan berikut. Pemeriksaan ini tidak membuktikan kebenaran seluruh OS, tetapi mencegah M16 dikerjakan di atas baseline yang rusak.

### 8.1 Checklist Artefak Wajib

| Tahap | Artefak wajib | Command pemeriksaan | Jika gagal |
|---|---|---|---|
| M0 | WSL 2, struktur repo, ADR, risk register | `test -d docs && test -d scripts` | Pulihkan struktur repo dari M0 |
| M1 | Toolchain dan proof compile | `clang --version && make --version` | Instal ulang toolchain WSL |
| M2 | Boot image/ISO awal | `test -d build || mkdir -p build` | Rebuild M2 sebelum lanjut |
| M3 | Panic/logging path | `grep -R "panic" -n kernel || true` | Pastikan panic path tidak hilang |
| M4 | IDT/trap baseline | `grep -R "idt\|trap" -n kernel || true` | Pulihkan trap handler |
| M5 | Timer/IRQ baseline | `grep -R "pit\|irq\|timer" -n kernel || true` | Rebuild timer baseline |
| M6 | PMM bitmap allocator | `grep -R "pmm" -n kernel || true` | Perbaiki frame allocator dahulu |
| M7 | VMM page table baseline | `grep -R "vmm\|page" -n kernel || true` | Jangan lanjut jika page mapping rusak |
| M8 | Kernel heap | `grep -R "kheap\|kmalloc" -n kernel || true` | Perbaiki allocator |
| M9 | Thread/scheduler | `grep -R "sched\|thread" -n kernel || true` | Pastikan cooperative scheduler stabil |
| M10 | Syscall ABI | `grep -R "syscall" -n kernel || true` | Kunci ABI sebelum file syscall |
| M11 | ELF loader | `grep -R "elf" -n kernel || true` | Pisahkan bug loader dari bug filesystem |
| M12 | Locking primitives | `grep -R "spinlock\|mutex" -n kernel || true` | Gunakan lock eksternal saat integrasi FS |
| M13 | VFS/file descriptor | `grep -R "vfs\|fd" -n kernel || true` | Pulihkan interface VFS |
| M14 | Block device/buffer cache | `grep -R "block" -n kernel || true` | Perbaiki block device sebelum FS journal |
| M15 | MCSFS1 persistent FS | `grep -R "mcsfs" -n kernel || true` | M16 dapat memakai source mandiri, tetapi konsep M15 wajib dipahami |

### 8.2 Script Preflight M16

Buat script berikut untuk mengumpulkan status lingkungan dan artefak.

```bash
cat > scripts/m16_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs/m16 evidence/m16 build/m16
{
  echo "== M16 preflight =="
  date -Iseconds
  echo "== host =="
  uname -a
  lsb_release -a 2>/dev/null || cat /etc/os-release
  echo "== tools =="
  clang --version | head -n 1
  make --version | head -n 1
  nm --version | head -n 1
  readelf --version | head -n 1
  objdump --version | head -n 1
  sha256sum --version | head -n 1
  qemu-system-x86_64 --version | head -n 1 || true
  echo "== git =="
  git status --short
  git rev-parse --short HEAD || true
  echo "== subsystem probes =="
  find kernel -maxdepth 4 -type f | sort | sed -n '1,120p'
} | tee logs/m16/preflight.log
EOF
chmod +x scripts/m16_preflight.sh
./scripts/m16_preflight.sh
```

Indikator hasil: `logs/m16/preflight.log` terbentuk dan memuat versi toolchain, status Git, serta daftar file kernel. Jika script berhenti pada tool tertentu, selesaikan dependency tersebut sebelum menulis source M16.

---

## 9. Desain Teknis MCSFS1J

### 9.1 Layout On-Disk Pendidikan

| LBA | Fungsi |
|---:|---|
| 0 | Superblock MCSFS1J |
| 1 | Journal header / commit record |
| 2..17 | Journal descriptor dan payload blocks, dua block per record |
| 18 | Inode bitmap |
| 19 | Block bitmap |
| 20..23 | Inode table, 16 inode x 128 byte |
| 24 | Root directory block |
| 25..127 | Data blocks |

### 9.2 Invariant Utama

1. Superblock harus memiliki `magic`, `version`, `block_size`, dan layout LBA yang valid.
2. Journal header dianggap kosong jika `magic == 0` dan `state == EMPTY`.
3. Journal header dianggap replayable hanya jika `magic`, `version`, `state == COMMITTED`, `count <= max_records`, dan checksum header valid.
4. Setiap descriptor journal harus memiliki `magic` valid, target LBA berada dalam range device, dan checksum payload cocok.
5. Replay harus idempotent: menyalin payload yang sama ke target yang sama berulang kali menghasilkan state yang sama.
6. Recovery harus fail-closed saat journal corrupt; tidak boleh menulis payload ke target yang tidak tervalidasi.
7. Root inode harus aktif, bertipe directory, dan menunjuk root directory LBA.
8. Reserved blocks sampai `DATA_START_LBA - 1` harus ditandai aktif pada block bitmap.
9. Directory entry aktif harus menunjuk inode aktif.
10. File inode aktif harus menunjuk data block yang aktif pada block bitmap.

### 9.3 State Machine Journal

| State | Makna | Tindakan recovery |
|---|---|---|
| EMPTY | Tidak ada transaksi pending | Mount lanjut |
| Payload/descriptor written tanpa commit | Tidak durable | Diabaikan; journal akan dibersihkan oleh transaksi berikutnya |
| COMMITTED valid | Transaksi durable tetapi mungkin belum sampai home location | Replay semua payload ke target, lalu clear journal |
| COMMITTED corrupt | Ada bukti transaksi tetapi integritas gagal | Return `M16_E_CORRUPT`, mount ditolak |

### 9.4 Kontrak Write Ordering

M16 memakai urutan konservatif:

1. Clear journal lama.
2. Tulis descriptor dan payload journal untuk semua record.
3. Tulis commit record sebagai blok terakhir yang membuat transaksi replayable.
4. Salin payload ke lokasi utama.
5. Clear journal.

Urutan ini adalah kontrak pendidikan, bukan bukti flush/barrier perangkat nyata. Pada perangkat nyata, storage write cache, FUA, cache flush, DMA ordering, dan driver error path harus ditangani pada praktikum driver/storage lanjutan.

---

## 10. Instruksi Implementasi Langkah demi Langkah

### Langkah 1 - Salin source M16

Tujuan langkah ini adalah membuat implementasi mandiri yang dapat diuji pada host dan dikompilasi sebagai object freestanding. Source ini sengaja tidak memakai `malloc`, `printf`, `memcpy`, atau `memset` pada path freestanding. Fungsi host test hanya aktif saat macro `MCSOS_M16_HOST_TEST` didefinisikan.

```bash
mkdir -p kernel/fs/mcsfs1j tests/m16
cat > kernel/fs/mcsfs1j/m16_mcsfs_journal.c <<'EOF'
/*
 * MCSOS M16 - MCSFS1J crash-consistency teaching journal
 * Target: host unit test and x86_64-elf freestanding object.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define M16_BLOCK_SIZE 512u
#define M16_MAX_BLOCKS 128u
#define M16_MAX_INODES 16u
#define M16_DIRECT_BLOCKS 4u
#define M16_MAX_NAME 32u
#define M16_MAGIC 0x4d43534631564a31ULL /* "MCSF1VJ1"-like */
#define M16_JMAGIC 0x4d43534a524e4c31ULL /* "MCSJRNL1"-like */
#define M16_VERSION 1u
#define M16_J_EMPTY 0u
#define M16_J_COMMITTED 2u
#define M16_JOURNAL_MAX_RECORDS 8u
#define M16_JOURNAL_START 1u
#define M16_JOURNAL_BLOCKS (1u + (2u * M16_JOURNAL_MAX_RECORDS))
#define M16_INODE_BITMAP_LBA (M16_JOURNAL_START + M16_JOURNAL_BLOCKS)
#define M16_BLOCK_BITMAP_LBA (M16_INODE_BITMAP_LBA + 1u)
#define M16_INODE_TABLE_LBA (M16_BLOCK_BITMAP_LBA + 1u)
#define M16_INODE_TABLE_BLOCKS 4u
#define M16_ROOT_DIR_LBA (M16_INODE_TABLE_LBA + M16_INODE_TABLE_BLOCKS)
#define M16_DATA_START_LBA (M16_ROOT_DIR_LBA + 1u)

#define M16_E_OK 0
#define M16_E_INVAL -1
#define M16_E_IO -2
#define M16_E_NOSPC -3
#define M16_E_EXISTS -4
#define M16_E_NOENT -5
#define M16_E_CORRUPT -6
#define M16_E_TOOLONG -7

struct m16_blockdev {
    uint8_t blocks[M16_MAX_BLOCKS][M16_BLOCK_SIZE];
    uint32_t total_blocks;
    uint64_t writes;
    int fail_after; /* negative disables fault injection */
};

struct m16_super {
    uint64_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t journal_start;
    uint32_t journal_blocks;
    uint32_t inode_bitmap_lba;
    uint32_t block_bitmap_lba;
    uint32_t inode_table_lba;
    uint32_t inode_table_blocks;
    uint32_t root_dir_lba;
    uint32_t data_start_lba;
    uint32_t clean_generation;
    uint32_t reserved[114];
};

struct m16_inode {
    uint32_t used;
    uint32_t kind; /* 1=file, 2=dir */
    uint32_t size;
    uint32_t direct[M16_DIRECT_BLOCKS];
    uint32_t reserved[25];
};

struct m16_dirent {
    uint32_t used;
    uint32_t ino;
    char name[M16_MAX_NAME];
};

struct m16_journal_header {
    uint64_t magic;
    uint32_t version;
    uint32_t state;
    uint32_t seq;
    uint32_t count;
    uint32_t header_checksum;
    uint32_t reserved[121];
};

struct m16_journal_desc {
    uint64_t magic;
    uint32_t target_lba;
    uint32_t payload_checksum;
    uint32_t reserved[124];
};

struct m16_jrec {
    uint32_t target_lba;
    uint8_t payload[M16_BLOCK_SIZE];
};

struct m16_tx {
    uint32_t count;
    struct m16_jrec rec[M16_JOURNAL_MAX_RECORDS];
};

_Static_assert(sizeof(struct m16_super) == M16_BLOCK_SIZE, "m16_super must occupy one block");
_Static_assert(sizeof(struct m16_inode) == 128u, "m16_inode must be 128 bytes");

static void m16_zero(void *ptr, size_t n) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < n; i++) {
        p[i] = 0;
    }
}

static void m16_copy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static size_t m16_strlen_bounded(const char *s, size_t max) {
    size_t n = 0;
    while (n < max && s[n] != '\0') {
        n++;
    }
    return n;
}

static int m16_streq(const char *a, const char *b) {
    for (size_t i = 0; i < M16_MAX_NAME; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == '\0') {
            return 1;
        }
    }
    return 1;
}

static uint32_t m16_checksum(const void *ptr, size_t n) {
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)p[i];
        h *= 16777619u;
    }
    return h;
}

static int m16_valid_lba(const struct m16_blockdev *dev, uint32_t lba) {
    return dev != NULL && lba < dev->total_blocks && lba < M16_MAX_BLOCKS;
}

static int m16_read_block(struct m16_blockdev *dev, uint32_t lba, void *out) {
    if (dev == NULL || out == NULL || !m16_valid_lba(dev, lba)) {
        return M16_E_INVAL;
    }
    m16_copy(out, dev->blocks[lba], M16_BLOCK_SIZE);
    return M16_E_OK;
}

static int m16_write_block(struct m16_blockdev *dev, uint32_t lba, const void *in) {
    if (dev == NULL || in == NULL || !m16_valid_lba(dev, lba)) {
        return M16_E_INVAL;
    }
    if (dev->fail_after == 0) {
        return M16_E_IO;
    }
    if (dev->fail_after > 0) {
        dev->fail_after--;
    }
    m16_copy(dev->blocks[lba], in, M16_BLOCK_SIZE);
    dev->writes++;
    return M16_E_OK;
}

void m16_dev_init(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return;
    }
    m16_zero(dev, sizeof(*dev));
    dev->total_blocks = M16_MAX_BLOCKS;
    dev->fail_after = -1;
}

static void m16_bitmap_set(uint8_t *bm, uint32_t bit) {
    bm[bit / 8u] = (uint8_t)(bm[bit / 8u] | (uint8_t)(1u << (bit % 8u)));
}

static int m16_bitmap_get(const uint8_t *bm, uint32_t bit) {
    return (bm[bit / 8u] & (uint8_t)(1u << (bit % 8u))) != 0u;
}

static int m16_load_inode_table(struct m16_blockdev *dev, struct m16_inode *inodes) {
    uint8_t *raw = (uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        int rc = m16_read_block(dev, M16_INODE_TABLE_LBA + i, raw + ((size_t)i * M16_BLOCK_SIZE));
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return M16_E_OK;
}

static int m16_store_inode_table(struct m16_tx *tx, const struct m16_inode *inodes) {
    if (tx == NULL || inodes == NULL || tx->count + M16_INODE_TABLE_BLOCKS > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_NOSPC;
    }
    const uint8_t *raw = (const uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        tx->rec[tx->count].target_lba = M16_INODE_TABLE_LBA + i;
        m16_copy(tx->rec[tx->count].payload, raw + ((size_t)i * M16_BLOCK_SIZE), M16_BLOCK_SIZE);
        tx->count++;
    }
    return M16_E_OK;
}

static int m16_tx_add(struct m16_tx *tx, uint32_t lba, const void *payload) {
    if (tx == NULL || payload == NULL || tx->count >= M16_JOURNAL_MAX_RECORDS) {
        return M16_E_NOSPC;
    }
    tx->rec[tx->count].target_lba = lba;
    m16_copy(tx->rec[tx->count].payload, payload, M16_BLOCK_SIZE);
    tx->count++;
    return M16_E_OK;
}

static uint32_t m16_header_checksum(struct m16_journal_header *h) {
    uint32_t saved = h->header_checksum;
    h->header_checksum = 0;
    uint32_t sum = m16_checksum(h, sizeof(*h));
    h->header_checksum = saved;
    return sum;
}

static int m16_journal_clear(struct m16_blockdev *dev) {
    struct m16_journal_header h;
    m16_zero(&h, sizeof(h));
    return m16_write_block(dev, M16_JOURNAL_START, &h);
}

static int m16_journal_commit(struct m16_blockdev *dev, const struct m16_tx *tx, uint32_t seq, int stop_after_commit_record) {
    if (dev == NULL || tx == NULL || tx->count > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_INVAL;
    }
    int rc = m16_journal_clear(dev);
    if (rc != M16_E_OK) {
        return rc;
    }
    for (uint32_t i = 0; i < tx->count; i++) {
        uint32_t desc_lba = M16_JOURNAL_START + 1u + (i * 2u);
        uint32_t data_lba = desc_lba + 1u;
        struct m16_journal_desc d;
        m16_zero(&d, sizeof(d));
        d.magic = M16_JMAGIC;
        d.target_lba = tx->rec[i].target_lba;
        d.payload_checksum = m16_checksum(tx->rec[i].payload, M16_BLOCK_SIZE);
        rc = m16_write_block(dev, desc_lba, &d);
        if (rc != M16_E_OK) {
            return rc;
        }
        rc = m16_write_block(dev, data_lba, tx->rec[i].payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    struct m16_journal_header h;
    m16_zero(&h, sizeof(h));
    h.magic = M16_JMAGIC;
    h.version = M16_VERSION;
    h.state = M16_J_COMMITTED;
    h.seq = seq;
    h.count = tx->count;
    h.header_checksum = m16_header_checksum(&h);
    rc = m16_write_block(dev, M16_JOURNAL_START, &h);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (stop_after_commit_record != 0) {
        return M16_E_OK;
    }
    for (uint32_t i = 0; i < tx->count; i++) {
        rc = m16_write_block(dev, tx->rec[i].target_lba, tx->rec[i].payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return m16_journal_clear(dev);
}

int m16_journal_recover(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    struct m16_journal_header h;
    int rc = m16_read_block(dev, M16_JOURNAL_START, &h);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (h.magic == 0u && h.state == M16_J_EMPTY) {
        return M16_E_OK;
    }
    if (h.magic != M16_JMAGIC || h.version != M16_VERSION || h.state != M16_J_COMMITTED || h.count > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_CORRUPT;
    }
    if (m16_header_checksum(&h) != h.header_checksum) {
        return M16_E_CORRUPT;
    }
    uint8_t payload[M16_BLOCK_SIZE];
    for (uint32_t i = 0; i < h.count; i++) {
        uint32_t desc_lba = M16_JOURNAL_START + 1u + (i * 2u);
        uint32_t data_lba = desc_lba + 1u;
        struct m16_journal_desc d;
        rc = m16_read_block(dev, desc_lba, &d);
        if (rc != M16_E_OK) {
            return rc;
        }
        if (d.magic != M16_JMAGIC || !m16_valid_lba(dev, d.target_lba)) {
            return M16_E_CORRUPT;
        }
        rc = m16_read_block(dev, data_lba, payload);
        if (rc != M16_E_OK) {
            return rc;
        }
        if (m16_checksum(payload, M16_BLOCK_SIZE) != d.payload_checksum) {
            return M16_E_CORRUPT;
        }
        rc = m16_write_block(dev, d.target_lba, payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return m16_journal_clear(dev);
}

int m16_format(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    m16_zero(dev->blocks, sizeof(dev->blocks));
    struct m16_super sb;
    m16_zero(&sb, sizeof(sb));
    sb.magic = M16_MAGIC;
    sb.version = M16_VERSION;
    sb.block_size = M16_BLOCK_SIZE;
    sb.total_blocks = dev->total_blocks;
    sb.journal_start = M16_JOURNAL_START;
    sb.journal_blocks = M16_JOURNAL_BLOCKS;
    sb.inode_bitmap_lba = M16_INODE_BITMAP_LBA;
    sb.block_bitmap_lba = M16_BLOCK_BITMAP_LBA;
    sb.inode_table_lba = M16_INODE_TABLE_LBA;
    sb.inode_table_blocks = M16_INODE_TABLE_BLOCKS;
    sb.root_dir_lba = M16_ROOT_DIR_LBA;
    sb.data_start_lba = M16_DATA_START_LBA;
    sb.clean_generation = 1u;
    int rc = m16_write_block(dev, 0u, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    m16_zero(ib, sizeof(ib));
    m16_zero(bb, sizeof(bb));
    m16_bitmap_set(ib, 0u); /* root inode */
    for (uint32_t i = 0; i < M16_DATA_START_LBA; i++) {
        m16_bitmap_set(bb, i);
    }
    rc = m16_write_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_write_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    struct m16_inode inodes[M16_MAX_INODES];
    m16_zero(inodes, sizeof(inodes));
    inodes[0].used = 1u;
    inodes[0].kind = 2u;
    inodes[0].size = 0u;
    inodes[0].direct[0] = M16_ROOT_DIR_LBA;
    uint8_t *raw = (uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        rc = m16_write_block(dev, M16_INODE_TABLE_LBA + i, raw + ((size_t)i * M16_BLOCK_SIZE));
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    uint8_t root[M16_BLOCK_SIZE];
    m16_zero(root, sizeof(root));
    rc = m16_write_block(dev, M16_ROOT_DIR_LBA, root);
    if (rc != M16_E_OK) {
        return rc;
    }
    return m16_journal_clear(dev);
}

int m16_mount(struct m16_blockdev *dev, struct m16_super *sb) {
    if (dev == NULL || sb == NULL) {
        return M16_E_INVAL;
    }
    int rc = m16_journal_recover(dev);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, 0u, sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (sb->magic != M16_MAGIC || sb->version != M16_VERSION || sb->block_size != M16_BLOCK_SIZE) {
        return M16_E_CORRUPT;
    }
    if (sb->data_start_lba != M16_DATA_START_LBA || sb->root_dir_lba != M16_ROOT_DIR_LBA) {
        return M16_E_CORRUPT;
    }
    return M16_E_OK;
}

static int m16_find_free_inode(const uint8_t *ib) {
    for (uint32_t i = 1; i < M16_MAX_INODES; i++) {
        if (!m16_bitmap_get(ib, i)) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

static int m16_find_free_block(const uint8_t *bb) {
    for (uint32_t i = M16_DATA_START_LBA; i < M16_MAX_BLOCKS; i++) {
        if (!m16_bitmap_get(bb, i)) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

static int m16_find_dirent(struct m16_dirent *dir, const char *name) {
    uint32_t n = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < n; i++) {
        if (dir[i].used != 0u && m16_streq(dir[i].name, name)) {
            return (int)i;
        }
    }
    return M16_E_NOENT;
}

static int m16_find_free_dirent(struct m16_dirent *dir) {
    uint32_t n = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < n; i++) {
        if (dir[i].used == 0u) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

int m16_write_file_ex(struct m16_blockdev *dev, const char *name, const uint8_t *data, uint32_t size, int stop_after_commit_record) {
    if (dev == NULL || name == NULL || data == NULL) {
        return M16_E_INVAL;
    }
    size_t name_len = m16_strlen_bounded(name, M16_MAX_NAME);
    if (name_len == 0u || name_len >= M16_MAX_NAME) {
        return M16_E_TOOLONG;
    }
    if (size > M16_BLOCK_SIZE) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_read_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (m16_find_dirent(dir, name) >= 0) {
        return M16_E_EXISTS;
    }
    int ino = m16_find_free_inode(ib);
    if (ino < 0) {
        return ino;
    }
    int data_block = m16_find_free_block(bb);
    if (data_block < 0) {
        return data_block;
    }
    int slot = m16_find_free_dirent(dir);
    if (slot < 0) {
        return slot;
    }
    uint8_t data_blk[M16_BLOCK_SIZE];
    m16_zero(data_blk, sizeof(data_blk));
    m16_copy(data_blk, data, size);
    m16_bitmap_set(ib, (uint32_t)ino);
    m16_bitmap_set(bb, (uint32_t)data_block);
    inodes[ino].used = 1u;
    inodes[ino].kind = 1u;
    inodes[ino].size = size;
    inodes[ino].direct[0] = (uint32_t)data_block;
    dir[slot].used = 1u;
    dir[slot].ino = (uint32_t)ino;
    m16_zero(dir[slot].name, M16_MAX_NAME);
    m16_copy(dir[slot].name, name, name_len);
    struct m16_tx tx;
    m16_zero(&tx, sizeof(tx));
    rc = m16_tx_add(&tx, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_tx_add(&tx, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_store_inode_table(&tx, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    /* M16 educational simplification: inode table is 4 records, so root/data are committed in separate transactions. */
    rc = m16_journal_commit(dev, &tx, sb.clean_generation + 1u, 0);
    if (rc != M16_E_OK) {
        return rc;
    }
    m16_zero(&tx, sizeof(tx));
    rc = m16_tx_add(&tx, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_tx_add(&tx, (uint32_t)data_block, data_blk);
    if (rc != M16_E_OK) {
        return rc;
    }
    return m16_journal_commit(dev, &tx, sb.clean_generation + 2u, stop_after_commit_record);
}

int m16_write_file(struct m16_blockdev *dev, const char *name, const uint8_t *data, uint32_t size) {
    return m16_write_file_ex(dev, name, data, size, 0);
}

int m16_read_file(struct m16_blockdev *dev, const char *name, uint8_t *out, uint32_t out_cap, uint32_t *out_size) {
    if (dev == NULL || name == NULL || out == NULL || out_size == NULL) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    int slot = m16_find_dirent(dir, name);
    if (slot < 0) {
        return slot;
    }
    uint32_t ino = dir[slot].ino;
    if (ino >= M16_MAX_INODES || inodes[ino].used == 0u || inodes[ino].kind != 1u) {
        return M16_E_CORRUPT;
    }
    if (inodes[ino].size > out_cap || inodes[ino].direct[0] >= dev->total_blocks) {
        return M16_E_INVAL;
    }
    uint8_t blk[M16_BLOCK_SIZE];
    rc = m16_read_block(dev, inodes[ino].direct[0], blk);
    if (rc != M16_E_OK) {
        return rc;
    }
    m16_copy(out, blk, inodes[ino].size);
    *out_size = inodes[ino].size;
    return M16_E_OK;
}

int m16_fsck(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_read_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (!m16_bitmap_get(ib, 0u) || inodes[0].used != 1u || inodes[0].kind != 2u || inodes[0].direct[0] != M16_ROOT_DIR_LBA) {
        return M16_E_CORRUPT;
    }
    for (uint32_t b = 0; b < M16_DATA_START_LBA; b++) {
        if (!m16_bitmap_get(bb, b)) {
            return M16_E_CORRUPT;
        }
    }
    uint32_t dir_count = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < dir_count; i++) {
        if (dir[i].used != 0u) {
            if (dir[i].ino >= M16_MAX_INODES || !m16_bitmap_get(ib, dir[i].ino)) {
                return M16_E_CORRUPT;
            }
            struct m16_inode *inode = &inodes[dir[i].ino];
            if (inode->used == 0u || inode->kind != 1u || inode->size > M16_BLOCK_SIZE) {
                return M16_E_CORRUPT;
            }
            if (inode->direct[0] < M16_DATA_START_LBA || inode->direct[0] >= dev->total_blocks || !m16_bitmap_get(bb, inode->direct[0])) {
                return M16_E_CORRUPT;
            }
        }
    }
    return M16_E_OK;
}

#ifdef MCSOS_M16_HOST_TEST
#include <stdio.h>

static int m16_expect(int cond, const char *msg) {
    if (!cond) {
        printf("FAIL: %s\n", msg);
        return 1;
    }
    return 0;
}

int main(void) {
    int fails = 0;
    struct m16_blockdev dev;
    uint8_t out[64];
    uint32_t out_size = 0;
    const uint8_t hello[] = { 'h', 'e', 'l', 'l', 'o', '-', 'm', '1', '6' };
    const uint8_t crashy[] = { 'c', 'r', 'a', 's', 'h', '-', 'r', 'e', 'p', 'l', 'a', 'y' };

    m16_dev_init(&dev);
    fails += m16_expect(m16_format(&dev) == M16_E_OK, "format");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after format");
    fails += m16_expect(m16_write_file(&dev, "hello.txt", hello, (uint32_t)sizeof(hello)) == M16_E_OK, "write hello");
    fails += m16_expect(m16_read_file(&dev, "hello.txt", out, sizeof(out), &out_size) == M16_E_OK, "read hello");
    fails += m16_expect(out_size == sizeof(hello), "hello size");
    fails += m16_expect(out[0] == 'h' && out[8] == '6', "hello content");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after hello");

    /* Simulate power loss after the second transaction commit record but before home-location writes. */
    fails += m16_expect(m16_write_file_ex(&dev, "crash.txt", crashy, (uint32_t)sizeof(crashy), 1) == M16_E_OK, "write crash transaction until commit record");
    fails += m16_expect(m16_journal_recover(&dev) == M16_E_OK, "journal replay after committed crash");
    fails += m16_expect(m16_read_file(&dev, "crash.txt", out, sizeof(out), &out_size) == M16_E_OK, "read crash after replay");
    fails += m16_expect(out_size == sizeof(crashy), "crash size after replay");
    fails += m16_expect(out[0] == 'c' && out[11] == 'y', "crash content after replay");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after replay");

    /* Corrupt committed journal descriptor: recovery must fail closed instead of applying unknown target. */
    m16_dev_init(&dev);
    fails += m16_expect(m16_format(&dev) == M16_E_OK, "format for corrupt test");
    fails += m16_expect(m16_write_file_ex(&dev, "bad.txt", crashy, (uint32_t)sizeof(crashy), 1) == M16_E_OK, "commit bad transaction");
    dev.blocks[M16_JOURNAL_START + 1u][0] ^= 0x7fu;
    fails += m16_expect(m16_journal_recover(&dev) == M16_E_CORRUPT, "corrupt descriptor rejected");

    if (fails == 0) {
        printf("M16 host tests PASS\n");
    }
    return fails == 0 ? 0 : 1;
}
#endif

EOF
```

### Langkah 2 - Buat Makefile M16

Makefile ini membangun dua target. Target pertama adalah host unit test agar algoritma journal dapat diuji tanpa boot kernel. Target kedua adalah freestanding object `x86_64-elf` untuk memastikan source dapat masuk jalur build kernel.

```bash
cat > tests/m16/Makefile <<'EOF'
CLANG ?= clang
TARGET_TRIPLE ?= x86_64-elf
CFLAGS_COMMON := -std=c17 -Wall -Wextra -Werror -O2
HOST_BIN := m16_host_test
FREESTANDING_OBJ := m16_mcsfs_journal.o

.PHONY: all host freestanding audit clean
all: host freestanding audit

host: $(HOST_BIN)
	./$(HOST_BIN)

$(HOST_BIN): m16_mcsfs_journal.c
	$(CLANG) $(CFLAGS_COMMON) -DMCSOS_M16_HOST_TEST $< -o $@

freestanding: $(FREESTANDING_OBJ)

$(FREESTANDING_OBJ): m16_mcsfs_journal.c
	$(CLANG) $(CFLAGS_COMMON) -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -target $(TARGET_TRIPLE) -c $< -o $@

audit: $(FREESTANDING_OBJ)
	nm -u $(FREESTANDING_OBJ) > nm_undefined.txt
	readelf -h $(FREESTANDING_OBJ) > readelf_header.txt
	objdump -dr $(FREESTANDING_OBJ) > objdump_disasm.txt
	sha256sum $(FREESTANDING_OBJ) > sha256sum.txt
	test ! -s nm_undefined.txt
	grep -q 'ELF64' readelf_header.txt
	grep -q 'Advanced Micro Devices X86-64' readelf_header.txt

clean:
	rm -f $(HOST_BIN) $(FREESTANDING_OBJ) nm_undefined.txt readelf_header.txt objdump_disasm.txt sha256sum.txt

EOF
```

Jika struktur repository berbeda, sesuaikan path source pada Makefile. Jangan mengubah flag freestanding tanpa mencatat alasan teknis pada laporan.

### Langkah 3 - Jalankan host unit test

Host unit test memverifikasi format, fsck, write/read normal, crash setelah commit record, journal replay, dan corrupt descriptor rejection.

```bash
cd tests/m16
make clean host
cd ../..
```

Indikator hasil yang benar:

```text
M16 host tests PASS
```

Jika gagal pada `write hello`, periksa layout journal dan `M16_JOURNAL_MAX_RECORDS`. Jika gagal pada `read crash after replay`, periksa apakah commit record ditulis setelah descriptor dan payload. Jika gagal pada `corrupt descriptor rejected`, recovery tidak fail-closed dan harus diperbaiki sebelum integrasi kernel.

### Langkah 4 - Jalankan freestanding object audit

Audit ini memastikan object dapat dikompilasi untuk target x86_64 freestanding dan tidak memiliki undefined symbol.

```bash
cd tests/m16
make clean all
cp m16_mcsfs_journal.o ../../build/m16/
cp nm_undefined.txt readelf_header.txt objdump_disasm.txt sha256sum.txt ../../evidence/m16/
cd ../..
```

Indikator hasil:

```text
M16 host tests PASS
nm_undefined.txt berukuran 0 byte
readelf_header.txt memuat ELF64, REL, Advanced Micro Devices X86-64
sha256sum.txt memuat checksum object
```

### Langkah 5 - Integrasi konseptual ke kernel

Pada tahap integrasi, jangan langsung mengganti seluruh MCSFS1 M15. Tambahkan adapter kecil agar VFS M13 dapat memilih antara backend MCSFS1 lama dan MCSFS1J baru.

Contoh integrasi konservatif:

```c
/* kernel/fs/mcsfs1j/mcsfs1j_adapter.h */
#pragma once
#include <stdint.h>

int m16_format(void *block_device_context);
int m16_mount(void *block_device_context, void *out_super);
int m16_fsck(void *block_device_context);
int m16_write_file(void *block_device_context, const char *name, const uint8_t *data, uint32_t size);
int m16_read_file(void *block_device_context, const char *name, uint8_t *out, uint32_t cap, uint32_t *out_size);
```

Catatan: source praktikum memakai `struct m16_blockdev` RAM-backed untuk host test. Kernel MCSOS harus menyediakan wrapper `m16_read_block` dan `m16_write_block` yang memanggil block layer M14. Jangan menghubungkan source host test langsung ke device driver nyata tanpa review ownership, lock, dan error path.

### Langkah 6 - Tambahkan command QEMU smoke test

Tujuan QEMU smoke test adalah memastikan integrasi M16 tidak merusak boot path. Smoke test tidak membuktikan crash consistency penuh; bukti crash consistency utama M16 tetap berasal dari host fault-injection test.

```bash
mkdir -p logs/m16
make clean all 2>&1 | tee logs/m16/build_kernel.log
qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -serial file:logs/m16/qemu_serial.log \
  -display none \
  -no-reboot \
  -no-shutdown \
  -cdrom build/mcsos.iso
```

Indikator hasil: `logs/m16/qemu_serial.log` memuat boot banner MCSOS, subsystem filesystem M16 init log, hasil fsck/replay, dan tidak langsung triple fault. Jika QEMU tidak menemukan ISO, kembali ke panduan M2-M3 untuk memastikan image build tersedia.

### Langkah 7 - Debug dengan GDB bila QEMU berhenti

```bash
qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -serial stdio \
  -display none \
  -s -S \
  -cdrom build/mcsos.iso
```

Pada terminal lain:

```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break m16_journal_recover
(gdb) break m16_fsck
(gdb) continue
```

Gunakan breakpoint `m16_journal_recover` untuk memastikan recovery berjalan sebelum filesystem diekspos ke VFS. Jika symbol tidak ditemukan, pastikan object M16 ditautkan ke kernel dan file `kernel.elf` memuat debug symbols.

---

## 11. Source Code Lengkap M16

File: `kernel/fs/mcsfs1j/m16_mcsfs_journal.c`

```c
/*
 * MCSOS M16 - MCSFS1J crash-consistency teaching journal
 * Target: host unit test and x86_64-elf freestanding object.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define M16_BLOCK_SIZE 512u
#define M16_MAX_BLOCKS 128u
#define M16_MAX_INODES 16u
#define M16_DIRECT_BLOCKS 4u
#define M16_MAX_NAME 32u
#define M16_MAGIC 0x4d43534631564a31ULL /* "MCSF1VJ1"-like */
#define M16_JMAGIC 0x4d43534a524e4c31ULL /* "MCSJRNL1"-like */
#define M16_VERSION 1u
#define M16_J_EMPTY 0u
#define M16_J_COMMITTED 2u
#define M16_JOURNAL_MAX_RECORDS 8u
#define M16_JOURNAL_START 1u
#define M16_JOURNAL_BLOCKS (1u + (2u * M16_JOURNAL_MAX_RECORDS))
#define M16_INODE_BITMAP_LBA (M16_JOURNAL_START + M16_JOURNAL_BLOCKS)
#define M16_BLOCK_BITMAP_LBA (M16_INODE_BITMAP_LBA + 1u)
#define M16_INODE_TABLE_LBA (M16_BLOCK_BITMAP_LBA + 1u)
#define M16_INODE_TABLE_BLOCKS 4u
#define M16_ROOT_DIR_LBA (M16_INODE_TABLE_LBA + M16_INODE_TABLE_BLOCKS)
#define M16_DATA_START_LBA (M16_ROOT_DIR_LBA + 1u)

#define M16_E_OK 0
#define M16_E_INVAL -1
#define M16_E_IO -2
#define M16_E_NOSPC -3
#define M16_E_EXISTS -4
#define M16_E_NOENT -5
#define M16_E_CORRUPT -6
#define M16_E_TOOLONG -7

struct m16_blockdev {
    uint8_t blocks[M16_MAX_BLOCKS][M16_BLOCK_SIZE];
    uint32_t total_blocks;
    uint64_t writes;
    int fail_after; /* negative disables fault injection */
};

struct m16_super {
    uint64_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t journal_start;
    uint32_t journal_blocks;
    uint32_t inode_bitmap_lba;
    uint32_t block_bitmap_lba;
    uint32_t inode_table_lba;
    uint32_t inode_table_blocks;
    uint32_t root_dir_lba;
    uint32_t data_start_lba;
    uint32_t clean_generation;
    uint32_t reserved[114];
};

struct m16_inode {
    uint32_t used;
    uint32_t kind; /* 1=file, 2=dir */
    uint32_t size;
    uint32_t direct[M16_DIRECT_BLOCKS];
    uint32_t reserved[25];
};

struct m16_dirent {
    uint32_t used;
    uint32_t ino;
    char name[M16_MAX_NAME];
};

struct m16_journal_header {
    uint64_t magic;
    uint32_t version;
    uint32_t state;
    uint32_t seq;
    uint32_t count;
    uint32_t header_checksum;
    uint32_t reserved[121];
};

struct m16_journal_desc {
    uint64_t magic;
    uint32_t target_lba;
    uint32_t payload_checksum;
    uint32_t reserved[124];
};

struct m16_jrec {
    uint32_t target_lba;
    uint8_t payload[M16_BLOCK_SIZE];
};

struct m16_tx {
    uint32_t count;
    struct m16_jrec rec[M16_JOURNAL_MAX_RECORDS];
};

_Static_assert(sizeof(struct m16_super) == M16_BLOCK_SIZE, "m16_super must occupy one block");
_Static_assert(sizeof(struct m16_inode) == 128u, "m16_inode must be 128 bytes");

static void m16_zero(void *ptr, size_t n) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < n; i++) {
        p[i] = 0;
    }
}

static void m16_copy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static size_t m16_strlen_bounded(const char *s, size_t max) {
    size_t n = 0;
    while (n < max && s[n] != '\0') {
        n++;
    }
    return n;
}

static int m16_streq(const char *a, const char *b) {
    for (size_t i = 0; i < M16_MAX_NAME; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == '\0') {
            return 1;
        }
    }
    return 1;
}

static uint32_t m16_checksum(const void *ptr, size_t n) {
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)p[i];
        h *= 16777619u;
    }
    return h;
}

static int m16_valid_lba(const struct m16_blockdev *dev, uint32_t lba) {
    return dev != NULL && lba < dev->total_blocks && lba < M16_MAX_BLOCKS;
}

static int m16_read_block(struct m16_blockdev *dev, uint32_t lba, void *out) {
    if (dev == NULL || out == NULL || !m16_valid_lba(dev, lba)) {
        return M16_E_INVAL;
    }
    m16_copy(out, dev->blocks[lba], M16_BLOCK_SIZE);
    return M16_E_OK;
}

static int m16_write_block(struct m16_blockdev *dev, uint32_t lba, const void *in) {
    if (dev == NULL || in == NULL || !m16_valid_lba(dev, lba)) {
        return M16_E_INVAL;
    }
    if (dev->fail_after == 0) {
        return M16_E_IO;
    }
    if (dev->fail_after > 0) {
        dev->fail_after--;
    }
    m16_copy(dev->blocks[lba], in, M16_BLOCK_SIZE);
    dev->writes++;
    return M16_E_OK;
}

void m16_dev_init(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return;
    }
    m16_zero(dev, sizeof(*dev));
    dev->total_blocks = M16_MAX_BLOCKS;
    dev->fail_after = -1;
}

static void m16_bitmap_set(uint8_t *bm, uint32_t bit) {
    bm[bit / 8u] = (uint8_t)(bm[bit / 8u] | (uint8_t)(1u << (bit % 8u)));
}

static int m16_bitmap_get(const uint8_t *bm, uint32_t bit) {
    return (bm[bit / 8u] & (uint8_t)(1u << (bit % 8u))) != 0u;
}

static int m16_load_inode_table(struct m16_blockdev *dev, struct m16_inode *inodes) {
    uint8_t *raw = (uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        int rc = m16_read_block(dev, M16_INODE_TABLE_LBA + i, raw + ((size_t)i * M16_BLOCK_SIZE));
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return M16_E_OK;
}

static int m16_store_inode_table(struct m16_tx *tx, const struct m16_inode *inodes) {
    if (tx == NULL || inodes == NULL || tx->count + M16_INODE_TABLE_BLOCKS > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_NOSPC;
    }
    const uint8_t *raw = (const uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        tx->rec[tx->count].target_lba = M16_INODE_TABLE_LBA + i;
        m16_copy(tx->rec[tx->count].payload, raw + ((size_t)i * M16_BLOCK_SIZE), M16_BLOCK_SIZE);
        tx->count++;
    }
    return M16_E_OK;
}

static int m16_tx_add(struct m16_tx *tx, uint32_t lba, const void *payload) {
    if (tx == NULL || payload == NULL || tx->count >= M16_JOURNAL_MAX_RECORDS) {
        return M16_E_NOSPC;
    }
    tx->rec[tx->count].target_lba = lba;
    m16_copy(tx->rec[tx->count].payload, payload, M16_BLOCK_SIZE);
    tx->count++;
    return M16_E_OK;
}

static uint32_t m16_header_checksum(struct m16_journal_header *h) {
    uint32_t saved = h->header_checksum;
    h->header_checksum = 0;
    uint32_t sum = m16_checksum(h, sizeof(*h));
    h->header_checksum = saved;
    return sum;
}

static int m16_journal_clear(struct m16_blockdev *dev) {
    struct m16_journal_header h;
    m16_zero(&h, sizeof(h));
    return m16_write_block(dev, M16_JOURNAL_START, &h);
}

static int m16_journal_commit(struct m16_blockdev *dev, const struct m16_tx *tx, uint32_t seq, int stop_after_commit_record) {
    if (dev == NULL || tx == NULL || tx->count > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_INVAL;
    }
    int rc = m16_journal_clear(dev);
    if (rc != M16_E_OK) {
        return rc;
    }
    for (uint32_t i = 0; i < tx->count; i++) {
        uint32_t desc_lba = M16_JOURNAL_START + 1u + (i * 2u);
        uint32_t data_lba = desc_lba + 1u;
        struct m16_journal_desc d;
        m16_zero(&d, sizeof(d));
        d.magic = M16_JMAGIC;
        d.target_lba = tx->rec[i].target_lba;
        d.payload_checksum = m16_checksum(tx->rec[i].payload, M16_BLOCK_SIZE);
        rc = m16_write_block(dev, desc_lba, &d);
        if (rc != M16_E_OK) {
            return rc;
        }
        rc = m16_write_block(dev, data_lba, tx->rec[i].payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    struct m16_journal_header h;
    m16_zero(&h, sizeof(h));
    h.magic = M16_JMAGIC;
    h.version = M16_VERSION;
    h.state = M16_J_COMMITTED;
    h.seq = seq;
    h.count = tx->count;
    h.header_checksum = m16_header_checksum(&h);
    rc = m16_write_block(dev, M16_JOURNAL_START, &h);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (stop_after_commit_record != 0) {
        return M16_E_OK;
    }
    for (uint32_t i = 0; i < tx->count; i++) {
        rc = m16_write_block(dev, tx->rec[i].target_lba, tx->rec[i].payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return m16_journal_clear(dev);
}

int m16_journal_recover(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    struct m16_journal_header h;
    int rc = m16_read_block(dev, M16_JOURNAL_START, &h);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (h.magic == 0u && h.state == M16_J_EMPTY) {
        return M16_E_OK;
    }
    if (h.magic != M16_JMAGIC || h.version != M16_VERSION || h.state != M16_J_COMMITTED || h.count > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_CORRUPT;
    }
    if (m16_header_checksum(&h) != h.header_checksum) {
        return M16_E_CORRUPT;
    }
    uint8_t payload[M16_BLOCK_SIZE];
    for (uint32_t i = 0; i < h.count; i++) {
        uint32_t desc_lba = M16_JOURNAL_START + 1u + (i * 2u);
        uint32_t data_lba = desc_lba + 1u;
        struct m16_journal_desc d;
        rc = m16_read_block(dev, desc_lba, &d);
        if (rc != M16_E_OK) {
            return rc;
        }
        if (d.magic != M16_JMAGIC || !m16_valid_lba(dev, d.target_lba)) {
            return M16_E_CORRUPT;
        }
        rc = m16_read_block(dev, data_lba, payload);
        if (rc != M16_E_OK) {
            return rc;
        }
        if (m16_checksum(payload, M16_BLOCK_SIZE) != d.payload_checksum) {
            return M16_E_CORRUPT;
        }
        rc = m16_write_block(dev, d.target_lba, payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return m16_journal_clear(dev);
}

int m16_format(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    m16_zero(dev->blocks, sizeof(dev->blocks));
    struct m16_super sb;
    m16_zero(&sb, sizeof(sb));
    sb.magic = M16_MAGIC;
    sb.version = M16_VERSION;
    sb.block_size = M16_BLOCK_SIZE;
    sb.total_blocks = dev->total_blocks;
    sb.journal_start = M16_JOURNAL_START;
    sb.journal_blocks = M16_JOURNAL_BLOCKS;
    sb.inode_bitmap_lba = M16_INODE_BITMAP_LBA;
    sb.block_bitmap_lba = M16_BLOCK_BITMAP_LBA;
    sb.inode_table_lba = M16_INODE_TABLE_LBA;
    sb.inode_table_blocks = M16_INODE_TABLE_BLOCKS;
    sb.root_dir_lba = M16_ROOT_DIR_LBA;
    sb.data_start_lba = M16_DATA_START_LBA;
    sb.clean_generation = 1u;
    int rc = m16_write_block(dev, 0u, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    m16_zero(ib, sizeof(ib));
    m16_zero(bb, sizeof(bb));
    m16_bitmap_set(ib, 0u); /* root inode */
    for (uint32_t i = 0; i < M16_DATA_START_LBA; i++) {
        m16_bitmap_set(bb, i);
    }
    rc = m16_write_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_write_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    struct m16_inode inodes[M16_MAX_INODES];
    m16_zero(inodes, sizeof(inodes));
    inodes[0].used = 1u;
    inodes[0].kind = 2u;
    inodes[0].size = 0u;
    inodes[0].direct[0] = M16_ROOT_DIR_LBA;
    uint8_t *raw = (uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        rc = m16_write_block(dev, M16_INODE_TABLE_LBA + i, raw + ((size_t)i * M16_BLOCK_SIZE));
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    uint8_t root[M16_BLOCK_SIZE];
    m16_zero(root, sizeof(root));
    rc = m16_write_block(dev, M16_ROOT_DIR_LBA, root);
    if (rc != M16_E_OK) {
        return rc;
    }
    return m16_journal_clear(dev);
}

int m16_mount(struct m16_blockdev *dev, struct m16_super *sb) {
    if (dev == NULL || sb == NULL) {
        return M16_E_INVAL;
    }
    int rc = m16_journal_recover(dev);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, 0u, sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (sb->magic != M16_MAGIC || sb->version != M16_VERSION || sb->block_size != M16_BLOCK_SIZE) {
        return M16_E_CORRUPT;
    }
    if (sb->data_start_lba != M16_DATA_START_LBA || sb->root_dir_lba != M16_ROOT_DIR_LBA) {
        return M16_E_CORRUPT;
    }
    return M16_E_OK;
}

static int m16_find_free_inode(const uint8_t *ib) {
    for (uint32_t i = 1; i < M16_MAX_INODES; i++) {
        if (!m16_bitmap_get(ib, i)) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

static int m16_find_free_block(const uint8_t *bb) {
    for (uint32_t i = M16_DATA_START_LBA; i < M16_MAX_BLOCKS; i++) {
        if (!m16_bitmap_get(bb, i)) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

static int m16_find_dirent(struct m16_dirent *dir, const char *name) {
    uint32_t n = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < n; i++) {
        if (dir[i].used != 0u && m16_streq(dir[i].name, name)) {
            return (int)i;
        }
    }
    return M16_E_NOENT;
}

static int m16_find_free_dirent(struct m16_dirent *dir) {
    uint32_t n = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < n; i++) {
        if (dir[i].used == 0u) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

int m16_write_file_ex(struct m16_blockdev *dev, const char *name, const uint8_t *data, uint32_t size, int stop_after_commit_record) {
    if (dev == NULL || name == NULL || data == NULL) {
        return M16_E_INVAL;
    }
    size_t name_len = m16_strlen_bounded(name, M16_MAX_NAME);
    if (name_len == 0u || name_len >= M16_MAX_NAME) {
        return M16_E_TOOLONG;
    }
    if (size > M16_BLOCK_SIZE) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_read_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (m16_find_dirent(dir, name) >= 0) {
        return M16_E_EXISTS;
    }
    int ino = m16_find_free_inode(ib);
    if (ino < 0) {
        return ino;
    }
    int data_block = m16_find_free_block(bb);
    if (data_block < 0) {
        return data_block;
    }
    int slot = m16_find_free_dirent(dir);
    if (slot < 0) {
        return slot;
    }
    uint8_t data_blk[M16_BLOCK_SIZE];
    m16_zero(data_blk, sizeof(data_blk));
    m16_copy(data_blk, data, size);
    m16_bitmap_set(ib, (uint32_t)ino);
    m16_bitmap_set(bb, (uint32_t)data_block);
    inodes[ino].used = 1u;
    inodes[ino].kind = 1u;
    inodes[ino].size = size;
    inodes[ino].direct[0] = (uint32_t)data_block;
    dir[slot].used = 1u;
    dir[slot].ino = (uint32_t)ino;
    m16_zero(dir[slot].name, M16_MAX_NAME);
    m16_copy(dir[slot].name, name, name_len);
    struct m16_tx tx;
    m16_zero(&tx, sizeof(tx));
    rc = m16_tx_add(&tx, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_tx_add(&tx, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_store_inode_table(&tx, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    /* M16 educational simplification: inode table is 4 records, so root/data are committed in separate transactions. */
    rc = m16_journal_commit(dev, &tx, sb.clean_generation + 1u, 0);
    if (rc != M16_E_OK) {
        return rc;
    }
    m16_zero(&tx, sizeof(tx));
    rc = m16_tx_add(&tx, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_tx_add(&tx, (uint32_t)data_block, data_blk);
    if (rc != M16_E_OK) {
        return rc;
    }
    return m16_journal_commit(dev, &tx, sb.clean_generation + 2u, stop_after_commit_record);
}

int m16_write_file(struct m16_blockdev *dev, const char *name, const uint8_t *data, uint32_t size) {
    return m16_write_file_ex(dev, name, data, size, 0);
}

int m16_read_file(struct m16_blockdev *dev, const char *name, uint8_t *out, uint32_t out_cap, uint32_t *out_size) {
    if (dev == NULL || name == NULL || out == NULL || out_size == NULL) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    int slot = m16_find_dirent(dir, name);
    if (slot < 0) {
        return slot;
    }
    uint32_t ino = dir[slot].ino;
    if (ino >= M16_MAX_INODES || inodes[ino].used == 0u || inodes[ino].kind != 1u) {
        return M16_E_CORRUPT;
    }
    if (inodes[ino].size > out_cap || inodes[ino].direct[0] >= dev->total_blocks) {
        return M16_E_INVAL;
    }
    uint8_t blk[M16_BLOCK_SIZE];
    rc = m16_read_block(dev, inodes[ino].direct[0], blk);
    if (rc != M16_E_OK) {
        return rc;
    }
    m16_copy(out, blk, inodes[ino].size);
    *out_size = inodes[ino].size;
    return M16_E_OK;
}

int m16_fsck(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_read_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (!m16_bitmap_get(ib, 0u) || inodes[0].used != 1u || inodes[0].kind != 2u || inodes[0].direct[0] != M16_ROOT_DIR_LBA) {
        return M16_E_CORRUPT;
    }
    for (uint32_t b = 0; b < M16_DATA_START_LBA; b++) {
        if (!m16_bitmap_get(bb, b)) {
            return M16_E_CORRUPT;
        }
    }
    uint32_t dir_count = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < dir_count; i++) {
        if (dir[i].used != 0u) {
            if (dir[i].ino >= M16_MAX_INODES || !m16_bitmap_get(ib, dir[i].ino)) {
                return M16_E_CORRUPT;
            }
            struct m16_inode *inode = &inodes[dir[i].ino];
            if (inode->used == 0u || inode->kind != 1u || inode->size > M16_BLOCK_SIZE) {
                return M16_E_CORRUPT;
            }
            if (inode->direct[0] < M16_DATA_START_LBA || inode->direct[0] >= dev->total_blocks || !m16_bitmap_get(bb, inode->direct[0])) {
                return M16_E_CORRUPT;
            }
        }
    }
    return M16_E_OK;
}

#ifdef MCSOS_M16_HOST_TEST
#include <stdio.h>

static int m16_expect(int cond, const char *msg) {
    if (!cond) {
        printf("FAIL: %s\n", msg);
        return 1;
    }
    return 0;
}

int main(void) {
    int fails = 0;
    struct m16_blockdev dev;
    uint8_t out[64];
    uint32_t out_size = 0;
    const uint8_t hello[] = { 'h', 'e', 'l', 'l', 'o', '-', 'm', '1', '6' };
    const uint8_t crashy[] = { 'c', 'r', 'a', 's', 'h', '-', 'r', 'e', 'p', 'l', 'a', 'y' };

    m16_dev_init(&dev);
    fails += m16_expect(m16_format(&dev) == M16_E_OK, "format");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after format");
    fails += m16_expect(m16_write_file(&dev, "hello.txt", hello, (uint32_t)sizeof(hello)) == M16_E_OK, "write hello");
    fails += m16_expect(m16_read_file(&dev, "hello.txt", out, sizeof(out), &out_size) == M16_E_OK, "read hello");
    fails += m16_expect(out_size == sizeof(hello), "hello size");
    fails += m16_expect(out[0] == 'h' && out[8] == '6', "hello content");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after hello");

    /* Simulate power loss after the second transaction commit record but before home-location writes. */
    fails += m16_expect(m16_write_file_ex(&dev, "crash.txt", crashy, (uint32_t)sizeof(crashy), 1) == M16_E_OK, "write crash transaction until commit record");
    fails += m16_expect(m16_journal_recover(&dev) == M16_E_OK, "journal replay after committed crash");
    fails += m16_expect(m16_read_file(&dev, "crash.txt", out, sizeof(out), &out_size) == M16_E_OK, "read crash after replay");
    fails += m16_expect(out_size == sizeof(crashy), "crash size after replay");
    fails += m16_expect(out[0] == 'c' && out[11] == 'y', "crash content after replay");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after replay");

    /* Corrupt committed journal descriptor: recovery must fail closed instead of applying unknown target. */
    m16_dev_init(&dev);
    fails += m16_expect(m16_format(&dev) == M16_E_OK, "format for corrupt test");
    fails += m16_expect(m16_write_file_ex(&dev, "bad.txt", crashy, (uint32_t)sizeof(crashy), 1) == M16_E_OK, "commit bad transaction");
    dev.blocks[M16_JOURNAL_START + 1u][0] ^= 0x7fu;
    fails += m16_expect(m16_journal_recover(&dev) == M16_E_CORRUPT, "corrupt descriptor rejected");

    if (fails == 0) {
        printf("M16 host tests PASS\n");
    }
    return fails == 0 ? 0 : 1;
}
#endif

```

---

## 12. Makefile Lengkap M16

File: `tests/m16/Makefile`

```makefile
CLANG ?= clang
TARGET_TRIPLE ?= x86_64-elf
CFLAGS_COMMON := -std=c17 -Wall -Wextra -Werror -O2
HOST_BIN := m16_host_test
FREESTANDING_OBJ := m16_mcsfs_journal.o

.PHONY: all host freestanding audit clean
all: host freestanding audit

host: $(HOST_BIN)
	./$(HOST_BIN)

$(HOST_BIN): m16_mcsfs_journal.c
	$(CLANG) $(CFLAGS_COMMON) -DMCSOS_M16_HOST_TEST $< -o $@

freestanding: $(FREESTANDING_OBJ)

$(FREESTANDING_OBJ): m16_mcsfs_journal.c
	$(CLANG) $(CFLAGS_COMMON) -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -target $(TARGET_TRIPLE) -c $< -o $@

audit: $(FREESTANDING_OBJ)
	nm -u $(FREESTANDING_OBJ) > nm_undefined.txt
	readelf -h $(FREESTANDING_OBJ) > readelf_header.txt
	objdump -dr $(FREESTANDING_OBJ) > objdump_disasm.txt
	sha256sum $(FREESTANDING_OBJ) > sha256sum.txt
	test ! -s nm_undefined.txt
	grep -q 'ELF64' readelf_header.txt
	grep -q 'Advanced Micro Devices X86-64' readelf_header.txt

clean:
	rm -f $(HOST_BIN) $(FREESTANDING_OBJ) nm_undefined.txt readelf_header.txt objdump_disasm.txt sha256sum.txt

```

---

## 13. Hasil Pemeriksaan Source Code Lokal

Source code inti M16 telah diperiksa pada lingkungan verifikasi lokal dengan command berikut:

```bash
make -C /mnt/data/m16_verify clean all
```

Ringkasan hasil:

```text
M16 host tests PASS
nm_undefined.txt: 0 byte
  Class:                             ELF64
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
ffea8903d1cd470761afbf9e1ae5114721001216d753c2411ee2152a91c7bf84  m16_mcsfs_journal.o
```

Interpretasi hasil:

1. Host unit test lulus untuk format, fsck, write/read normal, crash setelah commit record, replay, dan corrupt descriptor rejection.
2. Object freestanding berhasil dibuat dengan target `x86_64-elf`.
3. `nm -u` kosong, sehingga object tidak membawa undefined symbol ke linker kernel pada unit ini.
4. `readelf -h` menunjukkan ELF64 relocatable untuk x86-64.
5. `sha256sum` menyediakan fingerprint artefak object untuk laporan.

Batasan hasil: pemeriksaan ini tidak menggantikan QEMU smoke test di WSL 2 mahasiswa, tidak membuktikan flush ordering perangkat nyata, dan tidak membuktikan race freedom saat filesystem dipanggil paralel oleh banyak thread.

---

## 14. Checkpoint Buildable

| Checkpoint | Command | Artefak | Kriteria pass |
|---|---|---|---|
| C1 Preflight | `./scripts/m16_preflight.sh` | `logs/m16/preflight.log` | Toolchain dan status repo tercatat |
| C2 Host test | `make -C tests/m16 clean host` | `tests/m16/m16_host_test` | Output `M16 host tests PASS` |
| C3 Freestanding object | `make -C tests/m16 freestanding` | `tests/m16/m16_mcsfs_journal.o` | Object terbentuk |
| C4 Undefined symbol audit | `make -C tests/m16 audit` | `nm_undefined.txt` | File kosong |
| C5 ELF audit | `readelf -h tests/m16/m16_mcsfs_journal.o` | `readelf_header.txt` | ELF64, REL, x86-64 |
| C6 Disassembly audit | `objdump -dr tests/m16/m16_mcsfs_journal.o` | `objdump_disasm.txt` | Fungsi M16 terlihat |
| C7 Checksum | `sha256sum tests/m16/m16_mcsfs_journal.o` | `sha256sum.txt` | Checksum tersimpan |
| C8 QEMU smoke | QEMU command M16 | `qemu_serial.log` | Boot tidak regresi |
| C9 Git commit | `git commit` | Commit hash | Commit berisi source, log, evidence |

---

## 15. Tugas Implementasi Mahasiswa

### Tugas Wajib

1. Menyalin source M16 dan Makefile ke repository masing-masing.
2. Menjalankan preflight, host test, freestanding audit, dan checksum.
3. Menjelaskan state machine journal pada laporan.
4. Menunjukkan bukti crash setelah commit record dapat direplay.
5. Menunjukkan bukti corrupt descriptor ditolak.
6. Mengintegrasikan minimal satu log kernel: `m16 journal: empty`, `m16 journal: replayed`, atau `m16 journal: corrupt`.
7. Menyimpan semua artefak ke `evidence/m16/` dan `logs/m16/`.
8. Membuat commit Git dengan pesan yang terukur.

### Tugas Pengayaan

1. Tambahkan transaction sequence monotonic yang diperbarui pada superblock.
2. Tambahkan journal statistics: jumlah replay, jumlah corrupt journal, jumlah transaction commit.
3. Tambahkan negative test untuk checksum payload rusak.
4. Tambahkan `unlink` berbasis journal dengan bitmap clear yang aman.
5. Tambahkan wrapper block layer M14 agar host source dapat dipakai pada kernel dengan adapter.

### Tantangan Riset

1. Rancang ordered mode sederhana: data block harus sampai home location sebelum metadata commit.
2. Bandingkan write amplification antara full block journaling dan metadata-only journaling.
3. Rancang formal state machine kecil untuk journal replay dan buktikan idempotence.
4. Rancang test matrix powercut berbasis QEMU snapshot atau disk image copy.

---

## 16. Perintah Uji dan Bukti yang Wajib Dikumpulkan

```bash
./scripts/m16_preflight.sh
make -C tests/m16 clean all | tee logs/m16/m16_make_all.log
cp tests/m16/nm_undefined.txt evidence/m16/
cp tests/m16/readelf_header.txt evidence/m16/
cp tests/m16/objdump_disasm.txt evidence/m16/
cp tests/m16/sha256sum.txt evidence/m16/
git status --short | tee logs/m16/git_status_after_m16.log
git diff --stat | tee logs/m16/git_diff_stat_m16.log
```

Jika QEMU sudah tersedia:

```bash
make clean all 2>&1 | tee logs/m16/kernel_build.log
qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -serial file:logs/m16/qemu_serial.log \
  -display none \
  -no-reboot \
  -no-shutdown \
  -cdrom build/mcsos.iso
```

Artefak wajib:

| Artefak | Lokasi |
|---|---|
| Preflight log | `logs/m16/preflight.log` |
| Host test log | `logs/m16/m16_make_all.log` |
| Undefined symbol audit | `evidence/m16/nm_undefined.txt` |
| ELF header | `evidence/m16/readelf_header.txt` |
| Disassembly | `evidence/m16/objdump_disasm.txt` |
| Checksum | `evidence/m16/sha256sum.txt` |
| QEMU serial log | `logs/m16/qemu_serial.log` |
| Git diff/stat | `logs/m16/git_diff_stat_m16.log` |
| Commit hash | dicatat di laporan |

---

## 17. Failure Modes dan Solusi Perbaikan

| Gejala | Kemungkinan penyebab | Perbaikan |
|---|---|---|
| `M16 host tests` gagal pada `format` | Layout superblock tidak tepat 512 byte atau write block gagal | Periksa `_Static_assert`, `M16_BLOCK_SIZE`, dan fault injection |
| Gagal pada `fsck after format` | Root inode, bitmap reserved, atau root directory tidak konsisten | Audit `m16_format`, root inode, dan block bitmap |
| Gagal pada `write hello` | Journal record kurang, inode table terlalu besar, no-space | Periksa `M16_JOURNAL_MAX_RECORDS`, inode table, dan tx count |
| Gagal pada `read hello` | Root directory tidak ditulis, inode direct block salah | Periksa transaction kedua: root dir dan data block |
| Replay tidak memulihkan `crash.txt` | Commit record tidak ditulis terakhir atau recovery tidak membaca journal valid | Audit `m16_journal_commit` dan `m16_journal_recover` |
| Corrupt descriptor tidak ditolak | Recovery tidak memvalidasi magic/checksum/target LBA | Tambahkan validasi descriptor dan fail-closed return |
| `nm -u` tidak kosong | Ada hidden libc call atau symbol eksternal | Hindari `memcpy`, `memset`, `printf` pada path freestanding |
| `readelf` bukan x86-64 | Target triple salah | Gunakan `-target x86_64-elf` dan audit compiler |
| QEMU boot regresi | Object belum ditautkan benar atau init order salah | Kembalikan integrasi, uji source host dahulu, lalu integrasikan bertahap |
| Journal corrupt saat mount kernel | Layout disk lama M15 tidak cocok dengan M16 | Format ulang image praktikum atau buat migration path eksplisit |

---

## 18. Prosedur Rollback

Rollback dilakukan bila M16 menyebabkan build kernel gagal, QEMU tidak boot, atau recovery corrupt. Jangan menghapus bukti gagal; simpan log sebagai bahan analisis.

```bash
git status --short
git diff > logs/m16/rollback_diff_before_reset.patch
git restore kernel/fs/mcsfs1j tests/m16 scripts/m16_preflight.sh scripts/m16_grade.sh || true
git status --short
```

Jika sudah commit:

```bash
git log --oneline -5
git revert <commit_m16>
```

Jika hanya ingin menonaktifkan integrasi kernel tetapi mempertahankan source dan test:

```bash
# Contoh: hapus object M16 dari daftar object kernel, tetapi biarkan tests/m16 tetap ada.
git restore build.mk Makefile kernel/Makefile 2>/dev/null || true
make -C tests/m16 clean all
```

---

## 19. Verification Matrix

| Requirement | Evidence | Pass criterion |
|---|---|---|
| M16-R1 source dapat diuji pada host | `m16_make_all.log` | `M16 host tests PASS` |
| M16-R2 journal replay bekerja | Host test crash-after-commit | `read crash after replay` pass |
| M16-R3 corrupt journal fail-closed | Host test corrupt descriptor | Return `M16_E_CORRUPT` |
| M16-R4 freestanding object valid | `m16_mcsfs_journal.o` | Object terbentuk |
| M16-R5 tidak ada undefined symbol | `nm_undefined.txt` | 0 byte |
| M16-R6 object x86-64 | `readelf_header.txt` | ELF64, REL, x86-64 |
| M16-R7 disassembly tersedia | `objdump_disasm.txt` | Fungsi M16 tampak |
| M16-R8 artefak fingerprinted | `sha256sum.txt` | SHA-256 tercatat |
| M16-R9 QEMU tidak regresi | `qemu_serial.log` | Boot mencapai log M16 atau kernel prompt |
| M16-R10 laporan lengkap | `laporan_m16.md/pdf` | Memuat bukti, analisis, failure modes, rollback |

---

## 20. Kriteria Lulus Praktikum

Praktikum M16 dinyatakan lulus minimum jika seluruh syarat berikut terpenuhi.

1. Repository dapat dibangun dari clean checkout.
2. `scripts/m16_preflight.sh` berjalan dan menghasilkan log.
3. `make -C tests/m16 clean all` lulus.
4. Host unit test menampilkan `M16 host tests PASS`.
5. `nm_undefined.txt` kosong.
6. `readelf_header.txt` menunjukkan ELF64 relocatable x86-64.
7. `objdump_disasm.txt` dan `sha256sum.txt` disimpan.
8. Mahasiswa dapat menjelaskan state machine journal dan urutan write-ahead.
9. Mahasiswa dapat menjelaskan mengapa crash sebelum commit record tidak harus durable.
10. Mahasiswa dapat menjelaskan mengapa recovery harus fail-closed saat descriptor/checksum rusak.
11. QEMU smoke test dijalankan bila boot image M2-M15 tersedia.
12. Semua perubahan Git dikomit.
13. Laporan menyertakan log, screenshot/serial output, analisis failure mode, dan readiness review.

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | Format, journal commit, replay, read/write, fsck, corrupt descriptor rejection berjalan sesuai kontrak |
| Kualitas desain dan invariants | 20 | Layout, state machine, checksum, target LBA validation, idempotence, fail-closed behavior dijelaskan |
| Pengujian dan bukti | 20 | Host test, freestanding audit, nm/readelf/objdump/checksum, QEMU log, commit hash lengkap |
| Debugging/failure analysis | 10 | Analisis bug, root cause, failure mode, dan tindakan perbaikan konkret |
| Keamanan dan robustness | 10 | Validasi input, checksum, range check, no hidden libc, fail-closed, batas crash model jelas |
| Dokumentasi/laporan | 10 | Laporan mengikuti template, command dan output lengkap, referensi IEEE, readiness review objektif |

---

## 22. Template Laporan Praktikum M16

Gunakan template umum `os_template_laporan_praktikum.md`. Isi minimum M16:

1. **Sampul**: judul praktikum, nama mahasiswa, NIM, kelas, dosen Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.
2. **Tujuan**: tuliskan capaian teknis journal, recovery, fsck, dan fault injection.
3. **Dasar teori ringkas**: jelaskan write-ahead journal, commit record, replay, idempotence, checksum, dan fail-closed.
4. **Lingkungan**: OS host, WSL distro, versi Clang, Make, QEMU, Binutils, target architecture, commit hash.
5. **Desain**: layout LBA, journal header, descriptor, payload, state machine, invariant, batasan.
6. **Langkah kerja**: command, file yang dibuat, dan alasan teknis setiap command.
7. **Hasil uji**: output `make`, host test, `nm`, `readelf`, `objdump`, `sha256sum`, QEMU serial log.
8. **Analisis**: penyebab keberhasilan, bug yang ditemukan, crash scenario, dan perbandingan dengan teori.
9. **Keamanan dan reliability**: risiko corrupt journal, stale metadata, partial transaction, storage reorder, dan mitigasi.
10. **Kesimpulan**: apa yang berhasil, apa yang belum, dan rencana M17.
11. **Lampiran**: source penting, diff ringkas, log penuh, screenshot, dan referensi.

---


---

## Architecture and Design

Arsitektur M16 memisahkan empat komponen: block device M14 sebagai penyedia operasi `read_block` dan `write_block`; MCSFS1J sebagai filesystem persistent kecil; journal manager sebagai lapisan recovery; dan VFS M13 sebagai interface operasi file. System decomposition ini sengaja menjaga journal manager tetap kecil agar mahasiswa dapat membuktikan invariant transaksi sebelum integrasi ke scheduler, syscall, dan driver nyata.

Interface internal M16 menggunakan API `m16_format`, `m16_mount`, `m16_fsck`, `m16_write_file`, dan `m16_read_file`. ABI publik userspace belum distabilkan; syscall file I/O M10-M13 harus tetap dianggap wrapper sementara. Desain ini menghindari perubahan syscall ABI sampai filesystem contract, journal recovery, dan failure behavior terbukti melalui test.

## Filesystem Contract

Filesystem contract M16 adalah sebagai berikut. Dalam istilah operation semantics, setiap operation mempunyai error behavior yang eksplisit. Operasi `format` membuat superblock, bitmap, inode table, root directory, dan journal kosong. Operasi `mount` selalu menjalankan recovery sebelum mengembalikan superblock. Operasi `write_file` membuat file root-only baru dan mengembalikan `M16_E_EXISTS` jika nama sudah ada. Operasi `read_file` mengembalikan isi file bila directory entry, inode, direct block, dan output capacity valid. Operasi `fsck` memverifikasi root inode, reserved block bitmap, directory entry, inode liveness, dan data block reachability.

Semantics error mengikuti prinsip fail-closed: input invalid menghasilkan `M16_E_INVAL`, no-space menghasilkan `M16_E_NOSPC`, nama duplikat menghasilkan `M16_E_EXISTS`, objek tidak ditemukan menghasilkan `M16_E_NOENT`, dan metadata/journal corrupt menghasilkan `M16_E_CORRUPT`. Compatibility target adalah custom MCSFS1J, bukan POSIX penuh dan bukan ext4; mount version dan feature flag harus ditolak bila tidak dikenal. Security metadata seperti permission, ACL, xattr, quota, dan encryption belum diimplementasikan; bagian tersebut menjadi target praktikum keamanan dan filesystem lanjutan.

## Concurrency and Ordering

Concurrency M16 dibatasi pada single-core educational baseline. Bila dipakai dalam kernel dengan thread dan scheduler M9-M12, semua operasi filesystem harus dilindungi oleh lock eksternal pada level VFS atau superblock. Lock tersebut harus menjaga reference lifetime superblock, inode, directory block, journal transaction, dan buffer cache. Deadlock yang harus dihindari adalah urutan lock yang berubah antara VFS lock, filesystem lock, buffer-cache lock, dan block-device lock.

Ordering yang diwajibkan adalah descriptor/payload journal terlebih dahulu, commit record setelah payload lengkap, home-location write setelah commit, lalu journal clear. Pada hardware nyata, ordering ini belum cukup tanpa flush/FUA/barrier dan validasi write cache. Karena M16 berjalan pada RAM-backed block test, durability fisik tidak diklaim.

## Security and Threat Model

Threat model M16 mencakup metadata rusak akibat bug kernel, journal descriptor rusak, target LBA di luar range, payload checksum mismatch, nama file terlalu panjang, duplicate name, dan image lama M15 yang dipakai sebagai M16. Attacker yang diasumsikan adalah local unprivileged program yang kelak memanggil syscall file I/O; boundary kernel/user belum menjadi target utama M16, tetapi validasi input nama, panjang data, pointer output, dan range block wajib dipertahankan.

Mitigasi M16 adalah magic/version check, checksum header, checksum payload, target LBA validation, transaction count bound, fail-closed recovery, dan fsck setelah replay. Tidak ada klaim confidentiality, ACL, MAC, capability, encryption, secure boot, atau anti-rollback pada tahap ini.

## Validation Plan

Validation plan M16 terdiri dari unit test host, crash fault injection, corrupt journal negative test, fsck invariant test, freestanding object compile, `nm -u`, `readelf -h`, `objdump -dr`, checksum, QEMU smoke test, dan regression log. Fuzzing belum menjadi target wajib, tetapi dapat dilakukan dengan membuat corpus berupa journal header, descriptor, payload, superblock, bitmap, inode table, dan directory block yang dimutasi secara acak lalu memanggil `m16_mount` dan `m16_fsck`.

Crash testing minimum meliputi crash setelah commit record sebelum home-location write dan corrupt descriptor. Fuzz extension yang disarankan: randomize `count`, `target_lba`, `payload_checksum`, `magic`, dan `state`. Fsck harus tetap mendeteksi corruption tanpa unchecked panic.

## Reproducibility and CI/Supply-Chain Controls

Build M16 harus reproducible sejauh tahap praktikum: clean rebuild dari source yang sama harus menghasilkan host test yang lulus dan freestanding object yang dapat diaudit. Artefak `sha256sum.txt` menjadi kontrol checksum. CI pipeline yang disarankan menjalankan `make -C tests/m16 clean all`, menyimpan `nm_undefined.txt`, `readelf_header.txt`, `objdump_disasm.txt`, `sha256sum.txt`, dan QEMU serial log sebagai artifact.

Supply-chain controls minimum: catat versi compiler, make, binutils, QEMU, commit hash, dan checksum object. SBOM formal dan signature belum wajib pada M16, tetapi provenance build harus cukup untuk mengulangi test dari clean checkout.

## Assumptions and Scope

M16 adalah praktikum filesystem reliability untuk OS pendidikan MCSOS versi 260502. Scope terbatas pada x86_64, WSL 2, QEMU, kernel monolitik pendidikan, C17 freestanding, block-device abstraction M14, dan filesystem root-only. Requirements utama adalah journal replay, fail-closed recovery, fsck-lite, object audit, dan evidence trail. Interface yang dinilai adalah API internal, bukan syscall POSIX final.

## Cross-Science Map

| Domain | Transfer ke M16 |
|---|---|
| Systems engineering | Requirements, traceability, interface, verification matrix, rollback gate |
| Mathematics/formal reasoning | State machine journal, invariant, idempotence, target LBA validity |
| Statistics/performance | Benchmark awal dapat mengukur latency write, throughput file kecil, confidence dari pengulangan host test |
| Reliability/safety | Hazard partial transaction, fault injection, recovery, availability mount setelah crash |
| Control/physics/hardware | Timer/logging QEMU, block write ordering, DMA/MMIO belum in-scope, power-loss nyata belum diklaim |
| Human/governance | Documentation, operator/developer workflow, compliance evidence praktikum, support log, ethics klaim readiness |

## Models and Invariants

Model M16 adalah state machine disk kecil: `clean`, `journal_payload_only`, `journal_committed`, `replayed`, dan `corrupt`. Invariant keselamatan: recovery hanya boleh menulis payload jika header dan semua descriptor valid; target LBA harus in-range; checksum harus cocok; fsck harus menolak directory entry yang menunjuk inode tidak aktif. Invariant liveness terbatas: transaksi committed yang valid harus dapat direplay sampai journal clear, kecuali block device mengembalikan error.

## Implementation Transfer

Transfer implementasi ke kernel dilakukan melalui adapter block layer M14. VFS M13 memanggil operasi filesystem setelah `m16_mount` menyelesaikan recovery. Locking dari M12 harus membungkus operasi filesystem bila scheduler M9 menjalankan lebih dari satu thread. Debugging memakai QEMU/GDB dari M1-M4, sedangkan evidence toolchain memakai `nm`, `readelf`, dan `objdump` dari M1/M12.

## Failure Modes and Mitigations

Failure modes utama mencakup corruption dan membutuhkan diagnostic log untuk recovery: commit record torn, descriptor corrupt, payload checksum mismatch, target LBA out-of-range, stale journal, duplicate name, inode bitmap mismatch, block bitmap mismatch, root directory corrupt, no-space, hidden libc call, wrong target triple, QEMU boot regression, dan lock-order deadlock saat integrasi. Mitigasi: checksum, bound check, fail-closed, fsck after replay, rollback Git, host test sebelum integrasi kernel, dan log serial untuk triage.

## Acceptance Criteria

Acceptance criteria M16: semua requirement pada verification matrix memiliki evidence; host test pass; freestanding object terbentuk; `nm -u` kosong; `readelf` menunjukkan ELF64 x86-64 relocatable; checksum tersimpan; QEMU smoke test dicoba bila image tersedia; failure modes dianalisis; rollback path terdokumentasi; dan laporan menyatakan readiness hanya sebagai siap uji QEMU/fault-injection terbatas.

## 23. Readiness Review

| Aspek | Status M16 | Catatan |
|---|---|---|
| Build host | Lulus pada verifikasi lokal | Wajib diulang di WSL mahasiswa |
| Freestanding object | Lulus pada verifikasi lokal | `nm -u` kosong dan ELF64 x86-64 |
| Journal replay | Lulus pada host unit test | Crash model terbatas dan terkendali |
| Corrupt journal handling | Lulus pada host unit test | Fail-closed pada descriptor corrupt |
| QEMU smoke | Siap dijalankan | Bergantung image M2-M15 mahasiswa |
| Hardware real storage | Belum siap | Belum ada flush/FUA/DMA/driver evidence |
| SMP/concurrency | Belum siap | Perlu lock eksternal dari M12 dan stress test |
| Security | Baseline validation | Belum DAC/MAC/capability penuh |
| Production readiness | Tidak berlaku | Praktikum pendidikan |

**Keputusan readiness**: hasil M16 adalah **siap uji QEMU dan host fault-injection terbatas**. Hasil ini belum boleh disebut siap produksi atau bebas error.

---

## 24. Referensi

[1] The Linux Kernel Documentation, "The Linux Journalling API," Linux Kernel Documentation, accessed May 2026. [Online]. Available: https://www.kernel.org/doc/html/v5.17/filesystems/journalling.html

[2] The Linux Kernel Documentation, "3.6. Journal (jbd2)," Linux Kernel Documentation, accessed May 2026. [Online]. Available: https://www.kernel.org/doc/html/latest/filesystems/ext4/journal.html

[3] The Linux Kernel Documentation, "Ext4 Data Mode," Linux Kernel Documentation, accessed May 2026. [Online]. Available: https://www.kernel.org/doc/html/v4.19/filesystems/ext4/ext4.html

[4] QEMU Project, "GDB usage," QEMU System Emulation Documentation, accessed May 2026. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html

[5] LLVM Project, "Clang command line argument reference," Clang Documentation, accessed May 2026. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html

[6] Free Software Foundation, "GNU Binutils," GNU Project, accessed May 2026. [Online]. Available: https://www.gnu.org/software/binutils/binutils.html

[7] Free Software Foundation, "GNU make," GNU Make Manual, accessed May 2026. [Online]. Available: https://www.gnu.org/software/make/manual/make.html
