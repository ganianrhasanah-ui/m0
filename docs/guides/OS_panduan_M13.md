# OS_panduan_M13.md

# Panduan Praktikum M13 - VFS Minimal, File Descriptor Table, RAMFS, dan Syscall File I/O Awal pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M13  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: siap uji QEMU untuk VFS/FD/RAMFS awal, bukan siap produksi, bukan bukti filesystem aman terhadap crash, dan bukan bukti kompatibilitas POSIX penuh.

---

## 1. Ringkasan Praktikum

Praktikum M13 melanjutkan fondasi M0 sampai M12. Sampai M12, MCSOS telah memiliki kerangka toolchain, boot image, early console, panic path, IDT/trap, timer interrupt, physical memory manager, virtual memory manager awal, kernel heap, kernel thread, syscall ABI awal, loader ELF64 user program, dan primitive sinkronisasi awal. M13 mulai memperkenalkan lapisan filesystem yang dapat dipanggil dari jalur syscall, tetapi masih pada level konservatif: VFS minimal, file descriptor table per process, RAMFS in-memory, dan operasi file I/O dasar.

M13 tidak membuat filesystem persistent dan tidak mengklaim crash consistency. Fokusnya adalah membuat kontrak objek `vnode`, objek `file`, tabel file descriptor, path lookup absolut sederhana, operasi `open`, `read`, `write`, `lseek`, `close`, dan `dup` pada RAMFS. Desain ini sengaja kecil agar setiap invariant, lifetime, error path, dan acceptance evidence dapat diperiksa. Modelnya dipengaruhi oleh konsep VFS sebagai abstraksi kernel yang menyediakan antarmuka filesystem ke program user dan memungkinkan implementasi filesystem berbeda hidup bersama [1]. Konsep file object juga diselaraskan secara konseptual dengan gagasan open file description, sedangkan perilaku file descriptor awal mengacu pada prinsip umum `open` dan `close` dalam antarmuka POSIX-like [2], [3].

Keberhasilan M13 tidak boleh ditulis sebagai "MCSOS telah memiliki filesystem lengkap". Kriteria minimum M13 adalah: readiness M0-M12 terdokumentasi, source VFS/RAMFS/FD dapat dikompilasi sebagai host test dan object freestanding x86_64, host unit test lulus, `nm -u` pada linked relocatable object kosong, `readelf` menunjukkan ELF64 relocatable object, `objdump` dapat diaudit, checksum artefak tersimpan, dan integrasi QEMU dapat diuji ulang pada WSL 2 mahasiswa.

---

## 2. Assumptions, Scope, and Target Matrix

| Aspek | Keputusan M13 |
|---|---|
| Architecture | x86_64 long mode |
| Host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Kernel model | Monolithic teaching kernel |
| Bahasa | C17 freestanding untuk kernel; C17 hosted untuk host unit test |
| Toolchain | Clang target `x86_64-elf`, GNU `ld`, `nm`, `readelf`, `objdump`, `sha256sum`, `make` |
| Target triple | `x86_64-elf` |
| ABI | Kernel-internal C ABI; syscall wrapper M10/M13 masih pendidikan |
| Boot artifact | Menggunakan image/ISO hasil M2-M12; M13 menambah object VFS yang dapat ditautkan ke kernel |
| Subsystem utama | VFS minimal, file descriptor table, RAMFS in-memory, syscall file I/O wrapper |
| Storage model | RAMFS volatil; belum memakai block device |
| Crash model | Tidak ada durability guarantee; state hilang setelah reboot |
| POSIX compatibility | POSIX-like subset konseptual untuk `open`, `read`, `write`, `lseek`, `close` |
| CI / pipeline | Host unit test, freestanding object build, ELF audit, checksum artifact |

### 2.1 Goals

1. Membuat model VFS minimal yang memisahkan nama file, vnode, open file object, dan file descriptor.
2. Membuat RAMFS volatil yang dapat melakukan path lookup absolut sederhana dan membuat file baru dengan `MCS_O_CREAT`.
3. Membuat file descriptor table per process yang membatasi jumlah open file, mengembalikan error deterministik, dan membersihkan descriptor saat `close`.
4. Menyediakan wrapper syscall file I/O awal yang dapat dihubungkan dengan dispatcher M10.
5. Menulis host unit test yang menguji read path, write path, create path, lseek, close, invalid fd, missing path, relative path rejection, dan limit file descriptor.
6. Mengompilasi source sebagai object freestanding x86_64 tanpa runtime libc tersembunyi.
7. Menghasilkan bukti build, test, audit, dan checksum.

### 2.2 Non-Goals

M13 tidak membangun ext2, journaling, page cache, block cache, mount namespace, permission model lengkap, ACL, xattr, quota, encryption, symlink, hardlink, directory listing, rename atomicity, fsync semantics, crash recovery, fsck, mmap, pipe, socket, device node, atau persistent storage. Semua fitur tersebut menjadi target M14 dan modul lanjutan.

---

## 3. Peta Skill yang Digunakan

| Skill | Penggunaan dalam M13 |
|---|---|
| osdev-general | Readiness gate, staged delivery, integrasi M0-M12, acceptance criteria |
| osdev-01-computer-foundation | Invariant, state machine, lifetime, error semantics |
| osdev-02-low-level-programming | C17 freestanding, ABI, object audit, undefined behavior boundary |
| osdev-03-computer-and-hardware-architecture | x86_64 target, QEMU, emulator-vs-hardware boundary |
| osdev-04-kernel-development | process object, syscall wrapper, user pointer boundary, fd table |
| osdev-05-filesystem-development | VFS object model, RAMFS, file operation contract, failure modes |
| osdev-07-os-security | validasi user pointer konseptual, permission placeholder, privilege boundary |
| osdev-12-toolchain-devenv | Makefile, target triple, readelf, objdump, nm, checksum, reproducibility |
| osdev-14-cross-science | verification matrix, risk register, experiment design, evidence threshold |

---

## 4. Capaian Pembelajaran

Setelah menyelesaikan M13, mahasiswa mampu menjelaskan file descriptor, open file object, vnode, RAMFS, syscall file I/O, negative errno, object lifetime, error path, artifact audit, dan readiness review yang berbasis bukti.

---

## 5. Prasyarat Teori

| Materi | Kebutuhan dalam M13 |
|---|---|
| VFS | Memahami lapisan abstraksi filesystem kernel |
| File descriptor | Memahami integer handle per process yang menunjuk ke open file object |
| Open file object | Memahami offset, flag, dan hubungan ke vnode |
| RAMFS | Memahami filesystem volatil in-memory |
| Syscall ABI | Memahami wrapper `sys_open`, `sys_read`, `sys_write`, `sys_close` dari M10 |
| Memory safety | Memahami batas buffer, pointer NULL, integer size, dan ownership |
| Error handling | Memahami negative errno style pada kernel internal |
| Testing | Memahami host unit test, freestanding compile, `nm`, `readelf`, `objdump`, checksum |

---

## 6. Pemeriksaan Kesiapan Praktikum Sebelumnya M0-M12

Jalankan pemeriksaan ini dari root repository MCSOS. Tujuannya memastikan M13 tidak dimulai di atas baseline yang rusak. Jika salah satu pemeriksaan gagal, perbaiki terlebih dahulu sebelum menambahkan source M13.

```bash
git status --short
git branch --show-current
git log --oneline -5
ls -la
find . -maxdepth 3 -type f | sort | sed -n '1,160p'
```

Indikator lulus: working tree terkendali, branch praktikum jelas, commit M12 ada, dan struktur `kernel/`, `include/`, `tests/`, `scripts/`, `build/`, serta Makefile praktikum sebelumnya tersedia.

### 6.1 Checklist Readiness M0-M12

| Tahap | Bukti minimum sebelum M13 | Perbaikan jika gagal |
|---|---|---|
| M0 | WSL 2, Git, shell, direktori kerja, metadata toolchain | Ulangi setup WSL 2 dan dokumentasikan versi host |
| M1 | Toolchain audit, proof compile, reproducibility check | Pasang ulang `clang`, `lld`, `binutils`, `make`, `qemu-system-x86` |
| M2 | Kernel ELF/ISO boot path siap uji QEMU | Periksa Limine/OVMF path, linker script, entry symbol |
| M3 | Panic path dan kernel log dapat dibaca | Periksa serial console dan halt loop |
| M4 | IDT exception path berjalan untuk `int3` | Audit `lidt`, trap stub, stack alignment, `iretq` |
| M5 | IRQ0 timer tick dan PIC/PIT path terdeteksi | Periksa `sti`, EOI, remap PIC, PIT divisor |
| M6 | PMM bitmap allocator lulus host test | Periksa frame alignment, reserved region, double free |
| M7 | VMM page table awal lulus host test | Periksa PML4/PDPT/PD/PT allocation dan permission bit |
| M8 | Kernel heap first-fit lulus host test | Periksa alignment, split, coalesce, free-list invariant |
| M9 | Kernel thread dan scheduler kooperatif lulus host test | Periksa TCB state, stack setup, context switch ABI |
| M10 | Syscall dispatcher dan validation path lulus host test | Periksa syscall number, arg boundary, invalid pointer result |
| M11 | ELF64 loader awal lulus host test | Periksa `PT_LOAD`, alignment, W^X, user region |
| M12 | Spinlock, mutex kooperatif, lock-order validator lulus host test | Periksa atomic acquire/release, owner check, lock order |

### 6.2 Saran Solusi Kendala Umum dari M0-M12

1. Jika `clang -target x86_64-elf` tidak mengenali target, gunakan paket Clang dari distro WSL 2 terbaru atau LLVM resmi. Catat versi dengan `clang --version`.
2. Jika object freestanding memunculkan simbol `memcpy`, `memset`, `__stack_chk_fail`, atau helper runtime lain, periksa flag `-ffreestanding`, `-fno-builtin`, `-fno-stack-protector`, dan pastikan loop copy manual tersedia.
3. Jika QEMU boot hang tanpa log serial, jalankan QEMU dengan `-serial stdio` dan pastikan kernel tidak masuk triple fault sebelum early console.
4. Jika host unit test gagal pada allocator M8, jangan lanjut ke M13 karena RAMFS akan bergantung pada ownership dan batas kapasitas memori.
5. Jika syscall M10 belum memiliki validasi user pointer, M13 boleh memakai validasi placeholder, tetapi laporan wajib menandai ini sebagai risk item.
6. Jika M12 lock-order validator gagal, jangan menambahkan global VFS lock dahulu; gunakan single-thread host test sampai primitive sinkronisasi stabil.

---

## 7. Architecture and Design M13

### 7.1 Arsitektur Ringkas

```text
user program / test harness
        |
        v
sys_open / sys_read / sys_write / sys_lseek / sys_close
        |
        v
process.fd_table[fd] -> mcs_file { flags, offset, vnode, ramfs }
        |
        v
mcs_vnode { id, parent, type, name, size, data_offset, data_capacity }
        |
        v
mcs_ramfs { static vnode array, static data arena }
```

### 7.2 Object Model

| Object | Owner | Lifetime | Keterangan |
|---|---|---|---|
| `mcs_ramfs_t` | Kernel filesystem instance | Seumur mount/boot praktikum | Memiliki semua vnode dan data file |
| `mcs_vnode_t` | RAMFS | Stabil setelah dibuat | Merepresentasikan directory atau file |
| `mcs_file_t` | Process fd table | Dari open sampai close | Menyimpan flag, offset, pointer vnode, pointer fs |
| `mcs_fd_table_t` | Process | Seumur process | Array descriptor terbatas |
| User buffer | Caller syscall | Valid selama syscall | Pada M13 divalidasi minimal NULL/len; copyin/copyout penuh target M14+ |

### 7.3 Interface and ABI / API Surface

M13 menyediakan API kernel internal: `mcs_ramfs_init`, `mcs_ramfs_seed_file`, `mcs_ramfs_lookup`, `mcs_ramfs_create_file`, `mcs_fd_table_init`, `mcs_vfs_open`, `mcs_vfs_read`, `mcs_vfs_write`, `mcs_vfs_lseek`, `mcs_vfs_close`, `mcs_vfs_dup`, `mcs_sys_open`, `mcs_sys_read`, `mcs_sys_write`, `mcs_sys_lseek`, dan `mcs_sys_close`. Nomor syscall final harus mengikuti table M10.

---

## 8. Filesystem Contract, Semantics, Operation, and Error Behavior

### 8.1 Contract

Path harus absolut, file yang tidak ditemukan menghasilkan `MCS_ENOENT` kecuali `MCS_O_CREAT`, descriptor invalid menghasilkan `MCS_EBADF`, table penuh menghasilkan `MCS_ENFILE`, arena data penuh menghasilkan `MCS_ENOSPC`, `read` dan `write` memperbarui offset, `lseek` menolak offset negatif, dan `close` membuat descriptor dapat digunakan ulang.

### 8.2 POSIX-like Boundary

M13 hanya meniru subset kecil perilaku file descriptor. Dalam antarmuka POSIX-like, `open` menghasilkan file descriptor yang digunakan untuk operasi I/O berikutnya, sementara `close` membebaskan descriptor tersebut [2], [3]. Namun M13 belum mengimplementasikan semua flag POSIX, permission mode, `EINTR`, `O_CLOEXEC`, `fork`, record lock, dan delayed write error.

### 8.3 Security Metadata Placeholder

M13 belum mengimplementasikan permission, ACL, xattr, quota, encryption, credential, capability, atau namespace policy. Semua operasi diasumsikan berjalan pada kernel test context. Ini adalah risiko privilege boundary yang harus dicatat dalam laporan.

---

## 9. On-Disk and In-Memory Design

### 9.1 In-Memory Design

M13 memakai RAMFS statik: `nodes[MCS_MAX_NODES]` sebagai tabel vnode, `data[MCS_RAMFS_DATA_BYTES]` sebagai arena konten file, `data_offset` dan `data_capacity` pada vnode untuk menunjuk rentang data file, `parent` untuk relasi directory sederhana, dan root vnode pada index 0.

### 9.2 On-Disk Design Status

M13 tidak memiliki on-disk format. Istilah superblock, inode, directory, dan allocation digunakan hanya sebagai pembanding konseptual untuk modul filesystem berikutnya. RAMFS M13 tidak memiliki superblock persistent, tidak memiliki inode number persistent, tidak memiliki directory block persistent, tidak memiliki block allocation bitmap persistent, dan tidak memiliki journal.

### 9.3 Consistency and Recovery

M13 tidak memiliki fsync, journal, checkpoint, recovery, fsck, atau crash consistency. Crash model M13 adalah volatil: semua isi RAMFS hilang saat reboot.

---

## 10. Invariants and Correctness

| Invariant | Pernyataan | Test obligation |
|---|---|---|
| Root vnode | `nodes[0]` selalu directory root `/` | Test lookup `/` dan path absolut |
| Vnode ownership | Semua vnode dimiliki oleh satu `mcs_ramfs_t` | Tidak ada `malloc`; pointer stabil |
| File data bound | `size <= data_capacity` | Test write dan `ENOSPC` |
| FD bound | `0 <= fd < MCS_MAX_OPEN_FILES` | Test invalid fd dan fd exhaustion |
| FD lifetime | Descriptor valid dari open sampai close | Test read setelah close menghasilkan `EBADF` |
| Offset monotonic | read/write menaikkan offset sebesar byte sukses | Test partial read dan lseek |
| Error deterministic | Input invalid menghasilkan negative status tetap | Test relative path, missing file, invalid whence |
| No hidden libc | Object freestanding tidak memanggil runtime libc | `nm -u build/m13/vfs.o` kosong |
| Section audit | Object adalah ELF64 relocatable | `readelf -h build/m13/vfs.o` |

---

## 11. Concurrency, Lock, Reference, Lifetime, and Deadlock Boundary

M13 sengaja belum menambahkan global VFS lock agar mahasiswa memahami object model terlebih dahulu. Pada integrasi kernel nyata, perubahan `fs->node_count`, `fs->data_used`, `file->offset`, `node->size`, dan traversal path lookup saat ada create/unlink concurrent harus dilindungi lock dari M12. Lock order yang disarankan untuk M14+ adalah `process.fd_table_lock -> ramfs.global_lock -> vnode.lock`. Risiko deadlock, race, missed wakeup, reference leak, dan use-after-close harus tetap dicatat dalam laporan.

---

## 12. Toolchain Bill of Materials and Reproducibility Controls

| Komponen | Perintah validasi |
|---|---|
| Shell | `bash --version` |
| Compiler host | `cc --version` |
| Compiler target | `clang --version` |
| Linker | `ld --version` |
| ELF inspection | `readelf --version` |
| Disassembly | `objdump --version` |
| Symbol audit | `nm --version` |
| Build system | `make --version` |
| Checksum | `sha256sum --version` |
| Emulator | `qemu-system-x86_64 --version` |
| Debugger | `gdb --version` |

### 12.1 Build and Link Design

M13 memakai Makefile kecil yang membangun host test dan object freestanding. Object kernel ditautkan dengan `ld -r -m elf_x86_64` menjadi `vfs.o` agar dependency antar source terselesaikan sebelum `nm -u` diperiksa.

---

## 13. Implementation Plan dengan Checkpoint Buildable

### Checkpoint 13.0 - Buat branch kerja

```bash
git checkout -b praktikum-m13-vfs-ramfs
mkdir -p include kernel/vfs tests build/m13
```

### Checkpoint 13.1 - Tambahkan header VFS

### `include/mcs_vfs.h`

```c
#ifndef MCS_VFS_H
#define MCS_VFS_H

#include <stddef.h>
#include <stdint.h>

#define MCS_MAX_NAME 32u
#define MCS_MAX_PATH 128u
#define MCS_MAX_NODES 64u
#define MCS_MAX_OPEN_FILES 16u
#define MCS_RAMFS_DATA_BYTES 8192u

#define MCS_O_RDONLY 0x0001u
#define MCS_O_WRONLY 0x0002u
#define MCS_O_RDWR   0x0004u
#define MCS_O_CREAT  0x0100u
#define MCS_O_TRUNC  0x0200u
#define MCS_O_APPEND 0x0400u

#define MCS_SEEK_SET 0
#define MCS_SEEK_CUR 1
#define MCS_SEEK_END 2

typedef long mcs_ssize_t;

typedef enum mcs_vnode_type {
    MCS_VNODE_DIR = 1,
    MCS_VNODE_FILE = 2
} mcs_vnode_type_t;

typedef enum mcs_vfs_status {
    MCS_OK = 0,
    MCS_ENOENT = -2,
    MCS_EBADF = -9,
    MCS_EACCES = -13,
    MCS_EEXIST = -17,
    MCS_ENOTDIR = -20,
    MCS_EISDIR = -21,
    MCS_EINVAL = -22,
    MCS_ENFILE = -23,
    MCS_ENOSPC = -28,
    MCS_ENAMETOOLONG = -36
} mcs_vfs_status_t;

typedef struct mcs_vnode {
    uint32_t used;
    uint32_t id;
    uint32_t parent;
    mcs_vnode_type_t type;
    char name[MCS_MAX_NAME];
    size_t size;
    size_t data_offset;
    size_t data_capacity;
} mcs_vnode_t;

typedef struct mcs_ramfs {
    mcs_vnode_t nodes[MCS_MAX_NODES];
    size_t node_count;
    uint8_t data[MCS_RAMFS_DATA_BYTES];
    size_t data_used;
} mcs_ramfs_t;

typedef struct mcs_file {
    uint32_t used;
    uint32_t flags;
    size_t offset;
    mcs_vnode_t *node;
    mcs_ramfs_t *fs;
} mcs_file_t;

typedef struct mcs_fd_table {
    mcs_file_t files[MCS_MAX_OPEN_FILES];
} mcs_fd_table_t;

typedef struct mcs_process {
    uint32_t pid;
    mcs_fd_table_t fd_table;
} mcs_process_t;

void mcs_ramfs_init(mcs_ramfs_t *fs);
int mcs_ramfs_seed_file(mcs_ramfs_t *fs, const char *path, const uint8_t *data, size_t len);
int mcs_ramfs_lookup(mcs_ramfs_t *fs, const char *path, mcs_vnode_t **out_node);
int mcs_ramfs_create_file(mcs_ramfs_t *fs, const char *path, mcs_vnode_t **out_node);

void mcs_fd_table_init(mcs_fd_table_t *table);
int mcs_vfs_open(mcs_fd_table_t *table, mcs_ramfs_t *fs, const char *path, uint32_t flags);
mcs_ssize_t mcs_vfs_read(mcs_fd_table_t *table, int fd, void *buf, size_t len);
mcs_ssize_t mcs_vfs_write(mcs_fd_table_t *table, int fd, const void *buf, size_t len);
int mcs_vfs_lseek(mcs_fd_table_t *table, int fd, long offset, int whence);
int mcs_vfs_close(mcs_fd_table_t *table, int fd);
int mcs_vfs_dup(mcs_fd_table_t *table, int fd);

int mcs_sys_open(mcs_process_t *proc, mcs_ramfs_t *fs, const char *user_path, uint32_t flags);
mcs_ssize_t mcs_sys_read(mcs_process_t *proc, int fd, void *user_buf, size_t len);
mcs_ssize_t mcs_sys_write(mcs_process_t *proc, int fd, const void *user_buf, size_t len);
int mcs_sys_close(mcs_process_t *proc, int fd);
int mcs_sys_lseek(mcs_process_t *proc, int fd, long offset, int whence);

#endif
```


### Checkpoint 13.2 - Implementasikan RAMFS

### `kernel/vfs/ramfs.c`

```c
#include "mcs_vfs.h"

static size_t mcs_strlen(const char *s) {
    size_t n = 0;
    if (!s) {
        return 0;
    }
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int mcs_streq_n(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return b[n] == '\0';
}

static void mcs_copy_bytes(uint8_t *dst, const uint8_t *src, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

static void mcs_copy_name(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < MCS_MAX_NAME; i++) {
        dst[i] = '\0';
    }
    for (i = 0; i < n && i + 1u < MCS_MAX_NAME; i++) {
        dst[i] = src[i];
    }
}

static int mcs_find_child(mcs_ramfs_t *fs, uint32_t parent, const char *name, size_t name_len, mcs_vnode_t **out) {
    size_t i;
    if (!fs || !name || !out || name_len == 0u || name_len >= MCS_MAX_NAME) {
        return MCS_EINVAL;
    }
    for (i = 0; i < fs->node_count; i++) {
        if (fs->nodes[i].used && fs->nodes[i].parent == parent && mcs_streq_n(name, fs->nodes[i].name, name_len)) {
            *out = &fs->nodes[i];
            return MCS_OK;
        }
    }
    return MCS_ENOENT;
}

static int mcs_split_parent_leaf(mcs_ramfs_t *fs, const char *path, mcs_vnode_t **parent, const char **leaf, size_t *leaf_len) {
    const char *seg;
    const char *next;
    mcs_vnode_t *cur;
    size_t seg_len;
    int rc;
    if (!fs || !path || !parent || !leaf || !leaf_len) {
        return MCS_EINVAL;
    }
    if (path[0] != '/') {
        return MCS_EINVAL;
    }
    if (path[1] == '\0') {
        return MCS_EINVAL;
    }
    if (mcs_strlen(path) >= MCS_MAX_PATH) {
        return MCS_ENAMETOOLONG;
    }
    cur = &fs->nodes[0];
    seg = path + 1;
    for (;;) {
        next = seg;
        while (*next != '/' && *next != '\0') {
            next++;
        }
        seg_len = (size_t)(next - seg);
        if (seg_len == 0u || seg_len >= MCS_MAX_NAME) {
            return MCS_EINVAL;
        }
        if (*next == '\0') {
            *parent = cur;
            *leaf = seg;
            *leaf_len = seg_len;
            return MCS_OK;
        }
        rc = mcs_find_child(fs, cur->id, seg, seg_len, &cur);
        if (rc != MCS_OK) {
            return rc;
        }
        if (cur->type != MCS_VNODE_DIR) {
            return MCS_ENOTDIR;
        }
        seg = next + 1;
    }
}

static int mcs_alloc_node(mcs_ramfs_t *fs, uint32_t parent, mcs_vnode_type_t type, const char *name, size_t name_len, size_t capacity, mcs_vnode_t **out) {
    mcs_vnode_t *node;
    if (!fs || !name || !out || name_len == 0u || name_len >= MCS_MAX_NAME) {
        return MCS_EINVAL;
    }
    if (fs->node_count >= MCS_MAX_NODES) {
        return MCS_ENOSPC;
    }
    if (type == MCS_VNODE_FILE && fs->data_used + capacity > MCS_RAMFS_DATA_BYTES) {
        return MCS_ENOSPC;
    }
    node = &fs->nodes[fs->node_count];
    node->used = 1u;
    node->id = (uint32_t)fs->node_count;
    node->parent = parent;
    node->type = type;
    mcs_copy_name(node->name, name, name_len);
    node->size = 0u;
    node->data_offset = 0u;
    node->data_capacity = 0u;
    if (type == MCS_VNODE_FILE) {
        node->data_offset = fs->data_used;
        node->data_capacity = capacity;
        fs->data_used += capacity;
    }
    fs->node_count++;
    *out = node;
    return MCS_OK;
}

void mcs_ramfs_init(mcs_ramfs_t *fs) {
    size_t i;
    if (!fs) {
        return;
    }
    for (i = 0; i < MCS_MAX_NODES; i++) {
        fs->nodes[i].used = 0u;
        fs->nodes[i].id = 0u;
        fs->nodes[i].parent = 0u;
        fs->nodes[i].type = MCS_VNODE_FILE;
        fs->nodes[i].name[0] = '\0';
        fs->nodes[i].size = 0u;
        fs->nodes[i].data_offset = 0u;
        fs->nodes[i].data_capacity = 0u;
    }
    for (i = 0; i < MCS_RAMFS_DATA_BYTES; i++) {
        fs->data[i] = 0u;
    }
    fs->node_count = 1u;
    fs->data_used = 0u;
    fs->nodes[0].used = 1u;
    fs->nodes[0].id = 0u;
    fs->nodes[0].parent = 0u;
    fs->nodes[0].type = MCS_VNODE_DIR;
    fs->nodes[0].name[0] = '/';
    fs->nodes[0].name[1] = '\0';
}

int mcs_ramfs_lookup(mcs_ramfs_t *fs, const char *path, mcs_vnode_t **out_node) {
    const char *seg;
    const char *next;
    mcs_vnode_t *cur;
    size_t seg_len;
    int rc;
    if (!fs || !path || !out_node) {
        return MCS_EINVAL;
    }
    if (path[0] != '/') {
        return MCS_EINVAL;
    }
    if (mcs_strlen(path) >= MCS_MAX_PATH) {
        return MCS_ENAMETOOLONG;
    }
    if (path[1] == '\0') {
        *out_node = &fs->nodes[0];
        return MCS_OK;
    }
    cur = &fs->nodes[0];
    seg = path + 1;
    while (*seg != '\0') {
        next = seg;
        while (*next != '/' && *next != '\0') {
            next++;
        }
        seg_len = (size_t)(next - seg);
        if (seg_len == 0u || seg_len >= MCS_MAX_NAME) {
            return MCS_EINVAL;
        }
        rc = mcs_find_child(fs, cur->id, seg, seg_len, &cur);
        if (rc != MCS_OK) {
            return rc;
        }
        if (*next == '\0') {
            *out_node = cur;
            return MCS_OK;
        }
        if (cur->type != MCS_VNODE_DIR) {
            return MCS_ENOTDIR;
        }
        seg = next + 1;
    }
    return MCS_EINVAL;
}

int mcs_ramfs_create_file(mcs_ramfs_t *fs, const char *path, mcs_vnode_t **out_node) {
    mcs_vnode_t *parent;
    mcs_vnode_t *existing;
    const char *leaf;
    size_t leaf_len;
    int rc;
    if (!fs || !path || !out_node) {
        return MCS_EINVAL;
    }
    rc = mcs_ramfs_lookup(fs, path, &existing);
    if (rc == MCS_OK) {
        if (existing->type != MCS_VNODE_FILE) {
            return MCS_EISDIR;
        }
        *out_node = existing;
        return MCS_OK;
    }
    rc = mcs_split_parent_leaf(fs, path, &parent, &leaf, &leaf_len);
    if (rc != MCS_OK) {
        return rc;
    }
    if (parent->type != MCS_VNODE_DIR) {
        return MCS_ENOTDIR;
    }
    return mcs_alloc_node(fs, parent->id, MCS_VNODE_FILE, leaf, leaf_len, 256u, out_node);
}

int mcs_ramfs_seed_file(mcs_ramfs_t *fs, const char *path, const uint8_t *data, size_t len) {
    mcs_vnode_t *node;
    int rc;
    if (!fs || !path || (!data && len != 0u)) {
        return MCS_EINVAL;
    }
    rc = mcs_ramfs_create_file(fs, path, &node);
    if (rc != MCS_OK) {
        return rc;
    }
    if (len > node->data_capacity) {
        return MCS_ENOSPC;
    }
    mcs_copy_bytes(&fs->data[node->data_offset], data, len);
    node->size = len;
    return MCS_OK;
}
```


### Checkpoint 13.3 - Implementasikan FD table dan operasi VFS

### `kernel/vfs/fd.c`

```c
#include "mcs_vfs.h"

static size_t mcs_min_size(size_t a, size_t b) {
    return a < b ? a : b;
}

static void mcs_copy_to_user(void *dst, const uint8_t *src, size_t n) {
    size_t i;
    uint8_t *d = (uint8_t *)dst;
    for (i = 0; i < n; i++) {
        d[i] = src[i];
    }
}

static void mcs_copy_from_user(uint8_t *dst, const void *src, size_t n) {
    size_t i;
    const uint8_t *s = (const uint8_t *)src;
    for (i = 0; i < n; i++) {
        dst[i] = s[i];
    }
}

static int mcs_can_read(uint32_t flags) {
    return (flags & MCS_O_RDONLY) != 0u || (flags & MCS_O_RDWR) != 0u;
}

static int mcs_can_write(uint32_t flags) {
    return (flags & MCS_O_WRONLY) != 0u || (flags & MCS_O_RDWR) != 0u;
}

void mcs_fd_table_init(mcs_fd_table_t *table) {
    size_t i;
    if (!table) {
        return;
    }
    for (i = 0; i < MCS_MAX_OPEN_FILES; i++) {
        table->files[i].used = 0u;
        table->files[i].flags = 0u;
        table->files[i].offset = 0u;
        table->files[i].node = (mcs_vnode_t *)0;
        table->files[i].fs = (mcs_ramfs_t *)0;
    }
}

static mcs_file_t *mcs_fd_get(mcs_fd_table_t *table, int fd) {
    if (!table || fd < 0 || (size_t)fd >= MCS_MAX_OPEN_FILES) {
        return (mcs_file_t *)0;
    }
    if (!table->files[fd].used) {
        return (mcs_file_t *)0;
    }
    return &table->files[fd];
}

static int mcs_fd_alloc(mcs_fd_table_t *table) {
    size_t i;
    if (!table) {
        return MCS_EINVAL;
    }
    for (i = 0; i < MCS_MAX_OPEN_FILES; i++) {
        if (!table->files[i].used) {
            table->files[i].used = 1u;
            return (int)i;
        }
    }
    return MCS_ENFILE;
}

int mcs_vfs_open(mcs_fd_table_t *table, mcs_ramfs_t *fs, const char *path, uint32_t flags) {
    mcs_vnode_t *node;
    int fd;
    int rc;
    if (!table || !fs || !path) {
        return MCS_EINVAL;
    }
    if ((flags & (MCS_O_RDONLY | MCS_O_WRONLY | MCS_O_RDWR)) == 0u) {
        flags |= MCS_O_RDONLY;
    }
    rc = mcs_ramfs_lookup(fs, path, &node);
    if (rc != MCS_OK) {
        if ((flags & MCS_O_CREAT) == 0u) {
            return rc;
        }
        rc = mcs_ramfs_create_file(fs, path, &node);
        if (rc != MCS_OK) {
            return rc;
        }
    }
    if (node->type == MCS_VNODE_DIR && mcs_can_write(flags)) {
        return MCS_EISDIR;
    }
    fd = mcs_fd_alloc(table);
    if (fd < 0) {
        return fd;
    }
    table->files[fd].flags = flags;
    table->files[fd].node = node;
    table->files[fd].fs = fs;
    table->files[fd].offset = ((flags & MCS_O_APPEND) != 0u) ? node->size : 0u;
    if ((flags & MCS_O_TRUNC) != 0u) {
        if (!mcs_can_write(flags)) {
            table->files[fd].used = 0u;
            table->files[fd].node = (mcs_vnode_t *)0;
            table->files[fd].fs = (mcs_ramfs_t *)0;
            return MCS_EACCES;
        }
        node->size = 0u;
        table->files[fd].offset = 0u;
    }
    return fd;
}

mcs_ssize_t mcs_vfs_read(mcs_fd_table_t *table, int fd, void *buf, size_t len) {
    mcs_file_t *file;
    size_t remain;
    size_t n;
    if (!buf && len != 0u) {
        return MCS_EINVAL;
    }
    file = mcs_fd_get(table, fd);
    if (!file) {
        return MCS_EBADF;
    }
    if (!mcs_can_read(file->flags)) {
        return MCS_EACCES;
    }
    if (!file->node || !file->fs) {
        return MCS_EINVAL;
    }
    if (file->node->type == MCS_VNODE_DIR) {
        return MCS_EISDIR;
    }
    if (file->offset >= file->node->size) {
        return 0;
    }
    remain = file->node->size - file->offset;
    n = mcs_min_size(len, remain);
    mcs_copy_to_user(buf, &file->fs->data[file->node->data_offset + file->offset], n);
    file->offset += n;
    return (mcs_ssize_t)n;
}

mcs_ssize_t mcs_vfs_write(mcs_fd_table_t *table, int fd, const void *buf, size_t len) {
    mcs_file_t *file;
    size_t n;
    if (!buf && len != 0u) {
        return MCS_EINVAL;
    }
    file = mcs_fd_get(table, fd);
    if (!file) {
        return MCS_EBADF;
    }
    if (!mcs_can_write(file->flags)) {
        return MCS_EACCES;
    }
    if (!file->node || !file->fs) {
        return MCS_EINVAL;
    }
    if (file->node->type == MCS_VNODE_DIR) {
        return MCS_EISDIR;
    }
    if ((file->flags & MCS_O_APPEND) != 0u) {
        file->offset = file->node->size;
    }
    if (file->offset > file->node->data_capacity) {
        return MCS_EINVAL;
    }
    n = mcs_min_size(len, file->node->data_capacity - file->offset);
    if (n < len) {
        return MCS_ENOSPC;
    }
    mcs_copy_from_user(&file->fs->data[file->node->data_offset + file->offset], buf, n);
    file->offset += n;
    if (file->offset > file->node->size) {
        file->node->size = file->offset;
    }
    return (mcs_ssize_t)n;
}

int mcs_vfs_lseek(mcs_fd_table_t *table, int fd, long offset, int whence) {
    mcs_file_t *file;
    long base;
    long next;
    file = mcs_fd_get(table, fd);
    if (!file) {
        return MCS_EBADF;
    }
    if (!file->node || file->node->type == MCS_VNODE_DIR) {
        return MCS_EISDIR;
    }
    if (whence == MCS_SEEK_SET) {
        base = 0;
    } else if (whence == MCS_SEEK_CUR) {
        base = (long)file->offset;
    } else if (whence == MCS_SEEK_END) {
        base = (long)file->node->size;
    } else {
        return MCS_EINVAL;
    }
    next = base + offset;
    if (next < 0) {
        return MCS_EINVAL;
    }
    file->offset = (size_t)next;
    return (int)file->offset;
}

int mcs_vfs_close(mcs_fd_table_t *table, int fd) {
    mcs_file_t *file = mcs_fd_get(table, fd);
    if (!file) {
        return MCS_EBADF;
    }
    file->used = 0u;
    file->flags = 0u;
    file->offset = 0u;
    file->node = (mcs_vnode_t *)0;
    file->fs = (mcs_ramfs_t *)0;
    return MCS_OK;
}

int mcs_vfs_dup(mcs_fd_table_t *table, int fd) {
    mcs_file_t *file;
    int newfd;
    file = mcs_fd_get(table, fd);
    if (!file) {
        return MCS_EBADF;
    }
    newfd = mcs_fd_alloc(table);
    if (newfd < 0) {
        return newfd;
    }
    table->files[newfd].flags = file->flags;
    table->files[newfd].offset = file->offset;
    table->files[newfd].node = file->node;
    table->files[newfd].fs = file->fs;
    return newfd;
}

int mcs_sys_open(mcs_process_t *proc, mcs_ramfs_t *fs, const char *user_path, uint32_t flags) {
    if (!proc || !fs || !user_path) {
        return MCS_EINVAL;
    }
    return mcs_vfs_open(&proc->fd_table, fs, user_path, flags);
}

mcs_ssize_t mcs_sys_read(mcs_process_t *proc, int fd, void *user_buf, size_t len) {
    if (!proc || (!user_buf && len != 0u)) {
        return MCS_EINVAL;
    }
    return mcs_vfs_read(&proc->fd_table, fd, user_buf, len);
}

mcs_ssize_t mcs_sys_write(mcs_process_t *proc, int fd, const void *user_buf, size_t len) {
    if (!proc || (!user_buf && len != 0u)) {
        return MCS_EINVAL;
    }
    return mcs_vfs_write(&proc->fd_table, fd, user_buf, len);
}

int mcs_sys_close(mcs_process_t *proc, int fd) {
    if (!proc) {
        return MCS_EINVAL;
    }
    return mcs_vfs_close(&proc->fd_table, fd);
}

int mcs_sys_lseek(mcs_process_t *proc, int fd, long offset, int whence) {
    if (!proc) {
        return MCS_EINVAL;
    }
    return mcs_vfs_lseek(&proc->fd_table, fd, offset, whence);
}
```


### Checkpoint 13.4 - Tambahkan hook transitional untuk test/integrasi

### `kernel/vfs/sys_vfs.c`

```c
#include "mcs_vfs.h"

mcs_ramfs_t *mcs_active_ramfs_for_test = (mcs_ramfs_t *)0;

void mcs_vfs_set_active_ramfs_for_test(mcs_ramfs_t *fs) {
    mcs_active_ramfs_for_test = fs;
}
```


### Checkpoint 13.5 - Tambahkan host unit test

### `tests/m13_vfs_host_test.c`

```c
#include "mcs_vfs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void mcs_vfs_set_active_ramfs_for_test(mcs_ramfs_t *fs);

static void test_basic_read(void) {
    mcs_ramfs_t fs;
    mcs_process_t proc;
    char buf[32];
    int fd;
    mcs_ssize_t n;
    mcs_ramfs_init(&fs);
    assert(mcs_ramfs_seed_file(&fs, "/hello.txt", (const uint8_t *)"hello-mcsos", 11) == MCS_OK);
    proc.pid = 1;
    mcs_fd_table_init(&proc.fd_table);
    mcs_vfs_set_active_ramfs_for_test(&fs);
    fd = mcs_sys_open(&proc, &fs, "/hello.txt", MCS_O_RDONLY);
    assert(fd >= 0);
    memset(buf, 0, sizeof(buf));
    n = mcs_sys_read(&proc, fd, buf, 5);
    assert(n == 5);
    assert(memcmp(buf, "hello", 5) == 0);
    assert(mcs_sys_lseek(&proc, fd, 1, MCS_SEEK_SET) == 1);
    memset(buf, 0, sizeof(buf));
    n = mcs_sys_read(&proc, fd, buf, 4);
    assert(n == 4);
    assert(memcmp(buf, "ello", 4) == 0);
    assert(mcs_sys_close(&proc, fd) == MCS_OK);
    assert(mcs_sys_read(&proc, fd, buf, 1) == MCS_EBADF);
}

static void test_create_write_read(void) {
    mcs_ramfs_t fs;
    mcs_process_t proc;
    char buf[64];
    int fd;
    mcs_ssize_t n;
    mcs_ramfs_init(&fs);
    proc.pid = 2;
    mcs_fd_table_init(&proc.fd_table);
    mcs_vfs_set_active_ramfs_for_test(&fs);
    fd = mcs_sys_open(&proc, &fs, "/log.txt", MCS_O_CREAT | MCS_O_RDWR | MCS_O_TRUNC);
    assert(fd >= 0);
    n = mcs_sys_write(&proc, fd, "abc123", 6);
    assert(n == 6);
    assert(mcs_sys_lseek(&proc, fd, 0, MCS_SEEK_SET) == 0);
    memset(buf, 0, sizeof(buf));
    n = mcs_sys_read(&proc, fd, buf, sizeof(buf));
    assert(n == 6);
    assert(strcmp(buf, "abc123") == 0);
    assert(mcs_sys_close(&proc, fd) == MCS_OK);
}

static void test_errors_and_fd_limit(void) {
    mcs_ramfs_t fs;
    mcs_process_t proc;
    int fds[MCS_MAX_OPEN_FILES];
    size_t i;
    mcs_ramfs_init(&fs);
    assert(mcs_ramfs_seed_file(&fs, "/x", (const uint8_t *)"x", 1) == MCS_OK);
    proc.pid = 3;
    mcs_fd_table_init(&proc.fd_table);
    mcs_vfs_set_active_ramfs_for_test(&fs);
    assert(mcs_sys_open(&proc, &fs, "relative", MCS_O_RDONLY) == MCS_EINVAL);
    assert(mcs_sys_open(&proc, &fs, "/missing", MCS_O_RDONLY) == MCS_ENOENT);
    for (i = 0; i < MCS_MAX_OPEN_FILES; i++) {
        fds[i] = mcs_sys_open(&proc, &fs, "/x", MCS_O_RDONLY);
        assert(fds[i] == (int)i);
    }
    assert(mcs_sys_open(&proc, &fs, "/x", MCS_O_RDONLY) == MCS_ENFILE);
    assert(mcs_sys_close(&proc, fds[0]) == MCS_OK);
    assert(mcs_sys_open(&proc, &fs, "/x", MCS_O_RDONLY) == 0);
}

int main(void) {
    test_basic_read();
    test_create_write_read();
    test_errors_and_fd_limit();
    puts("M13 VFS/FD/RAMFS host tests: PASS");
    return 0;
}
```


### Checkpoint 13.6 - Tambahkan Makefile M13

### `Makefile.m13`

```makefile
CC ?= cc
CLANG ?= clang
OBJDUMP ?= objdump
READELF ?= readelf
NM ?= nm
LD ?= ld
SHA256SUM ?= sha256sum

BUILD := build/m13
INCLUDES := -Iinclude
HOST_CFLAGS := -std=c17 -Wall -Wextra -Werror -O2 $(INCLUDES)
FREESTANDING_CFLAGS := -target x86_64-elf -std=c17 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra -Werror -O2 $(INCLUDES)
VFS_SRCS := kernel/vfs/ramfs.c kernel/vfs/fd.c kernel/vfs/sys_vfs.c

.PHONY: m13-all m13-host-test m13-objects m13-audit clean

m13-all: m13-host-test m13-objects m13-audit

m13-host-test: $(BUILD)/m13_vfs_host_test
	./$(BUILD)/m13_vfs_host_test | tee $(BUILD)/host-test.log

$(BUILD)/m13_vfs_host_test: tests/m13_vfs_host_test.c $(VFS_SRCS) include/mcs_vfs.h
	mkdir -p $(BUILD)
	$(CC) $(HOST_CFLAGS) tests/m13_vfs_host_test.c $(VFS_SRCS) -o $@

m13-objects: $(BUILD)/ramfs.o $(BUILD)/fd.o $(BUILD)/sys_vfs.o

$(BUILD)/ramfs.o: kernel/vfs/ramfs.c include/mcs_vfs.h
	mkdir -p $(BUILD)
	$(CLANG) $(FREESTANDING_CFLAGS) -c $< -o $@

$(BUILD)/fd.o: kernel/vfs/fd.c include/mcs_vfs.h
	mkdir -p $(BUILD)
	$(CLANG) $(FREESTANDING_CFLAGS) -c $< -o $@

$(BUILD)/sys_vfs.o: kernel/vfs/sys_vfs.c include/mcs_vfs.h
	mkdir -p $(BUILD)
	$(CLANG) $(FREESTANDING_CFLAGS) -c $< -o $@

m13-audit: m13-objects
	$(LD) -r -m elf_x86_64 $(BUILD)/ramfs.o $(BUILD)/fd.o $(BUILD)/sys_vfs.o -o $(BUILD)/vfs.o
	$(NM) -u $(BUILD)/vfs.o > $(BUILD)/nm-undefined.txt
	$(READELF) -h $(BUILD)/vfs.o > $(BUILD)/readelf-vfs.txt
	$(OBJDUMP) -dr $(BUILD)/vfs.o > $(BUILD)/objdump-vfs.txt
	$(SHA256SUM) $(BUILD)/ramfs.o $(BUILD)/fd.o $(BUILD)/sys_vfs.o $(BUILD)/vfs.o $(BUILD)/m13_vfs_host_test > $(BUILD)/sha256sums.txt
	test ! -s $(BUILD)/nm-undefined.txt

clean:
	rm -rf $(BUILD)
```


---

## 14. Perintah Uji, Validation Plan, and Evidence

```bash
make -f Makefile.m13 clean
make -f Makefile.m13 m13-all
```

Indikator lulus: host test PASS, object `ramfs.o`, `fd.o`, `sys_vfs.o`, dan `vfs.o` terbentuk, `nm-undefined.txt` kosong, `readelf` menunjukkan ELF64 relocatable object, `objdump` tersedia, dan checksum tersimpan.

### 14.1 Validation Plan Khusus Filesystem

Validation plan M13 mencakup unit test host, smoke test QEMU setelah integrasi, negative test untuk path relatif dan fd invalid, fuzz ringan berbasis variasi pathname pada modul lanjutan, crash note untuk RAMFS volatil, serta fsck note bahwa M13 belum memiliki fsck karena tidak ada on-disk metadata. Untuk compatibility, M13 hanya POSIX-like subset; mount table, version negotiation, dan feature flags belum tersedia. Untuk performance, catat benchmark awal berupa latency operasi `open/read/write/close` pada host test dan throughput baca/tulis RAMFS sebagai baseline non-final.

### 14.1 Bukti hasil build lokal sumber panduan

### `host-test.log`

```text
M13 VFS/FD/RAMFS host tests: PASS
```


`nm -u build/m13/vfs.o`:

### `nm-undefined.txt`

```text

```


Header ELF linked relocatable object:

### `readelf-vfs.txt`

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
  Start of section headers:          7544 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           64 (bytes)
  Number of section headers:         11
  Section header string table index: 10
```


Checksum artefak:

### `sha256sums.txt`

```text
b5234d7cc2606ff336f05c03050cc0262df78a663af38ae02cc5e591bbbcb234  build/m13/ramfs.o
ed845b33608139c75700260e0a5fec5d90a8815704271f685ec90fc4bbf43d33  build/m13/fd.o
ca60fde4073bd0b8bf0f7945335e16e8741cc12ec402fbf7fcf330662baa61e3  build/m13/sys_vfs.o
8d45bac2d241ec3002f8475a7ac70467dd31c7fc3833fb7bbab0364d77992ba6  build/m13/vfs.o
e56679fa77db45b047cc8fd03d47a56d898e1669317d33249b8c1948f6de2dd7  build/m13/m13_vfs_host_test
```


### 14.2 QEMU smoke workflow setelah integrasi ke kernel

```bash
make clean
make all
make iso
qemu-system-x86_64 \
  -machine q35 \
  -m 256M \
  -cdrom build/mcsos.iso \
  -serial stdio \
  -no-reboot \
  -no-shutdown
```

### 14.3 Debug Workflow QEMU/GDB

```bash
qemu-system-x86_64 -machine q35 -m 256M -cdrom build/mcsos.iso -serial stdio -S -s
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break mcs_vfs_open
(gdb) break mcs_vfs_read
(gdb) break mcs_vfs_write
(gdb) continue
```

---

## 15. Integration Transfer ke MCSOS Kernel

Tambahkan source M13 ke build kernel, tambahkan satu instance `mcs_ramfs_t kernel_ramfs`, panggil `mcs_ramfs_init`, seed file demo, tambahkan field `mcs_fd_table_t fd_table` ke process/TCB, panggil `mcs_fd_table_init` saat process dibuat, hubungkan syscall M10 ke `mcs_sys_open`, `mcs_sys_read`, `mcs_sys_write`, `mcs_sys_lseek`, dan `mcs_sys_close`, lalu uji melalui kernel self-test sebelum mengizinkan user path penuh.

---

## 16. Failure Modes, Diagnostics, Risk, and Rollback

| Failure mode | Gejala | Diagnosis | Solusi |
|---|---|---|---|
| Path relatif diterima | Test relative path tidak gagal | Periksa `path[0] != '/'` | Kembalikan `MCS_EINVAL` |
| Descriptor bocor | FD table penuh setelah close | Periksa `mcs_vfs_close` | Reset `used`, `node`, `fs`, `offset`, `flags` |
| Read selalu kosong | Offset/size salah | Periksa `file->offset` dan `node->size` | Audit update offset pada write dan read |
| Write melewati buffer | Panic atau memory corruption | Periksa `data_capacity` | Tolak dengan `MCS_ENOSPC` |
| Missing file tidak error | Lookup mengembalikan node salah | Periksa parent/id/name compare | Perbaiki `mcs_find_child` |
| `nm -u` tidak kosong | Hidden dependency/runtime helper | Buka `nm-undefined.txt` | Hindari libc, pakai loop copy manual |
| `readelf` bukan ELF64 | Target triple salah | Periksa compile flags | Gunakan `-target x86_64-elf` |
| QEMU panic setelah integrasi | ABI mismatch atau syscall pointer invalid | Gunakan GDB break pada `mcs_sys_*` | Uji kernel self-test sebelum user path |
| Race pada create/read | State berubah concurrent | Belum ada lock M13 | Lindungi dengan lock M12 pada M14+ |
| Data hilang setelah reboot | RAMFS volatil | Bukan bug M13 | Persistent FS dibahas modul lanjutan |

### 16.1 Rollback Procedure

```bash
git status --short
git diff -- include/mcs_vfs.h kernel/vfs tests Makefile.m13 > build/m13/m13-rollback-diff.patch
git restore include/mcs_vfs.h kernel/vfs tests/m13_vfs_host_test.c Makefile.m13
make clean
```

---

## 17. Security and Reliability Review

| Asset | Threat | Mitigasi M13 | Residual risk |
|---|---|---|---|
| Kernel memory | User pointer invalid | NULL/len check minimal | Belum ada full usercopy/page permission check |
| FD table | Use-after-close | `EBADF` setelah close | Belum ada refcount sharing antar process |
| RAMFS data | Write overflow | Capacity check | Belum ada per-file quota |
| Namespace | Path traversal | Hanya path absolut sederhana | Belum ada `..`, symlink, mount namespace |
| Privilege | Semua file bisa dibuka | Tidak ada permission | Credential/capability M14+ |
| Durability | Reboot kehilangan data | Non-goal eksplisit | Persistent FS dan fsck modul lanjutan |

Eksperimen reliability minimum: ulangi clean build, bandingkan checksum, jalankan host test untuk ukuran read berbeda, jalankan test fd exhaustion, jalankan QEMU smoke setelah integrasi, dan catat compiler version, linker version, commit hash, serta checksum.

---

## 18. CI Matrix and Supply Chain Evidence

```bash
set -e
make -f Makefile.m13 clean
make -f Makefile.m13 m13-all
sha256sum build/m13/* > build/m13/ci-artifacts.sha256
```

Matrix CI minimal: host-test-gcc, host-test-clang, freestanding-clang, qemu-smoke, audit-artifact. SBOM/provenance M13 minimal adalah daftar versi compiler, linker, binutils, QEMU, GDB, commit hash, dan checksum source/output. Signature artefak belum wajib pada M13, tetapi checksum wajib.

---

## 19. Acceptance Criteria / Kriteria Lulus Praktikum

M13 lulus jika repository dapat dibangun dari clean checkout; readiness M0-M12 tersedia; source VFS/RAMFS/FD dan Makefile tersedia; `make -f Makefile.m13 m13-all` lulus; host test menampilkan PASS; `nm-undefined.txt` kosong; `readelf` menunjukkan ELF64 relocatable object; `objdump` dan checksum tersimpan; QEMU smoke test dijalankan atau alasan teknisnya dicatat; laporan memuat object lifetime, error path, failure modes, dan analisis mengapa M13 belum crash-consistent serta belum permission-safe.

---

## 20. Rubrik Penilaian 100 Poin

| Komponen | Poin | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | API VFS/FD/RAMFS berjalan, host test lulus, error path deterministik |
| Kualitas desain dan invariants | 20 | Object lifetime, ownership, offset, fd bound, capacity bound jelas |
| Pengujian dan bukti | 20 | Host test, freestanding compile, `nm`, `readelf`, `objdump`, checksum, QEMU smoke evidence |
| Debugging/failure analysis | 10 | Failure modes, diagnostic commands, rollback patch/log lengkap |
| Keamanan dan robustness | 10 | User pointer risk, permission gap, capacity checks, fd validation, threat model dicatat |
| Dokumentasi/laporan | 10 | Laporan sesuai template, referensi IEEE, screenshot/log memadai |

---

## 21. Pertanyaan Analisis

1. Apa perbedaan file descriptor, open file object, vnode, inode, dan pathname?
2. Mengapa `close(fd)` harus membuat descriptor dapat digunakan ulang?
3. Mengapa `read` dan `write` memperbarui offset pada open file object, bukan pada vnode?
4. Mengapa RAMFS M13 tidak memiliki crash consistency?
5. Risiko apa yang muncul jika dua thread melakukan `write` ke file yang sama tanpa lock?
6. Mengapa path relatif ditolak pada M13?
7. Apa perbedaan `MCS_ENFILE` pada FD table penuh dan `MCS_ENOSPC` pada RAMFS penuh?
8. Mengapa `nm -u` harus kosong pada linked relocatable `vfs.o`?
9. Apa risiko keamanan dari syscall yang menerima user pointer tanpa copyin/copyout penuh?
10. Bagaimana Anda akan memperluas M13 menjadi VFS dengan mount table pada M14?

---

## 22. Template Laporan Praktikum M13

Sampul memuat judul praktikum, nama mahasiswa atau kelompok, NIM, kelas, dosen Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia. Laporan harus berisi tujuan, dasar teori ringkas, lingkungan, desain, langkah kerja, hasil uji, analisis, keamanan dan reliability, kesimpulan, lampiran, dan referensi IEEE.

---

## 23. Readiness Review

| Gate | Status minimum M13 | Bukti |
|---|---|---|
| Toolchain/devenv | Siap uji | `make`, Clang, `ld`, `nm`, `readelf`, `objdump`, checksum |
| Kernel integration | Kandidat integrasi terbatas | Source VFS/FD/RAMFS tersedia |
| Filesystem | Siap uji RAMFS volatil | Host test read/write/create/lseek/close lulus |
| Security | Belum siap | Permission dan copyin/copyout penuh belum ada |
| Crash consistency | Tidak siap | Tidak ada fsync, journal, recovery, fsck |
| QEMU | Wajib diuji ulang | Bergantung ISO/OVMF/QEMU lokal mahasiswa |

Kesimpulan readiness: hasil M13 hanya dapat disebut **siap uji QEMU untuk VFS/FD/RAMFS awal** setelah host test, object audit, dan integrasi QEMU menghasilkan bukti. Hasil ini belum siap demonstrasi praktikum jika tidak ada log build/test. Hasil ini belum kandidat penggunaan terbatas karena belum memiliki permission model, crash consistency, dan persistent storage.

---

## 24. References

[1] Linux Kernel Documentation, "Overview of the Linux Virtual File System," docs.kernel.org, accessed May 2026. [Online]. Available: https://docs.kernel.org/filesystems/vfs.html

[2] The Open Group, "open - open a file," The Open Group Base Specifications Issue 7/IEEE Std 1003.1, 2018 edition, accessed May 2026. [Online]. Available: https://pubs.opengroup.org/onlinepubs/9699919799/functions/open.html

[3] GNU C Library Manual, "Opening and Closing Files," Free Software Foundation, accessed May 2026. [Online]. Available: https://www.gnu.org/software/libc/manual/html_node/Opening-and-Closing-Files.html

[4] Intel Corporation, "Intel 64 and IA-32 Architectures Software Developer's Manual," accessed May 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

[5] QEMU Project, "GDB usage," QEMU documentation, accessed May 2026. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html

[6] Clang/LLVM Project, "Clang command line argument reference," accessed May 2026. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html

[7] GNU Binutils, "readelf, objdump, nm," GNU documentation, accessed May 2026. [Online]. Available: https://sourceware.org/binutils/docs/
