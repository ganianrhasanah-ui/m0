# OS_panduan_M14.md

# Panduan Praktikum M14 - Block Device Layer, RAM Block Driver, Buffer Cache Minimal, dan Jalur Persiapan Filesystem Persistent pada MCSOS

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Tahap**: M14  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target utama**: x86_64, QEMU, Windows 11 x64 dengan WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, dan subset POSIX-like jangka panjang.  
**Status keluaran yang diperbolehkan setelah praktikum**: siap uji QEMU untuk block device layer, RAM block driver, dan buffer cache minimal. Status ini bukan bukti storage persistent aman terhadap power-loss, bukan bukti driver hardware siap, dan bukan bukti filesystem crash-consistent.

---

## 1. Ringkasan Praktikum

Praktikum M14 melanjutkan M13. Sampai M13, MCSOS telah memiliki VFS minimal, file descriptor table, RAMFS volatil, dan syscall file I/O awal. Kelemahan utama M13 adalah seluruh filesystem masih berada di memori dan belum memiliki abstraksi storage berbasis blok. M14 memperkenalkan lapisan block device agar filesystem berikutnya dapat membaca dan menulis media berbasis blok secara terukur.

Fokus M14 adalah tiga komponen kecil tetapi fundamental: `block device registry`, `ramblk` sebagai block driver volatil yang meniru perangkat blok, dan `buffer cache` satu-blok-per-entry dengan dirty flag serta operasi flush eksplisit. Desain ini sengaja belum memakai driver SATA/NVMe/virtio-blk agar mahasiswa dapat memverifikasi invariant storage tanpa langsung memasuki kompleksitas PCIe, DMA, interrupt completion, atau virtqueue. Model konseptualnya diselaraskan dengan prinsip umum block layer yang memisahkan request block I/O dari perangkat fisik; Linux modern menggunakan block subsystem dan mekanisme multi-queue untuk perangkat berperforma tinggi, sedangkan M14 hanya mengambil gagasan pemisahan antarmuka dan driver dalam bentuk yang jauh lebih kecil [1], [2]. Sebagai pembanding edukatif, `null_blk` pada Linux juga menunjukkan nilai perangkat blok sintetis untuk menguji lapisan block I/O tanpa bergantung pada media fisik [3].

QEMU pada M14 digunakan sebagai target runtime untuk memastikan kernel masih dapat diboot dengan artefak baru; dokumentasi QEMU menyatakan bahwa `qemu-system-x86_64` menerima opsi mesin, memori, disk image, dan drive, termasuk penentuan `format=raw` agar format disk tidak ditebak secara ambigu [4]. Untuk debugging, QEMU gdbstub dapat dipakai dengan `-s -S` agar guest berhenti menunggu koneksi GDB pada port default 1234 [5]. Toolchain tetap memakai Clang `-ffreestanding`, yang mendeklarasikan kompilasi dalam environment freestanding [6], serta `nm`, `readelf`, dan `objdump` dari GNU Binutils untuk audit object ELF [7].

Keberhasilan M14 tidak boleh ditulis sebagai "MCSOS sudah memiliki driver storage lengkap". Kriteria minimum M14 adalah: readiness M0-M13 terdokumentasi, block device layer dapat dikompilasi sebagai host test dan freestanding object x86_64, host unit test lulus, linked relocatable object tidak memiliki undefined symbol, ELF header tervalidasi sebagai ELF64 relocatable x86-64, disassembly dapat diaudit, checksum artefak tersimpan, dan integrasi QEMU dapat diuji ulang pada WSL 2 mahasiswa.

---

## 2. Assumptions, Scope, and Target Matrix

| Aspek | Keputusan M14 |
|---|---|
| Architecture | x86_64 long mode |
| Host | Windows 11 x64 + WSL 2 Ubuntu/Debian-like |
| Emulator | QEMU system emulation x86_64 |
| Kernel model | Monolithic teaching kernel |
| Bahasa | C17 freestanding untuk kernel; C17 hosted untuk host unit test |
| Toolchain | Clang target `x86_64-elf`, GNU `ld`, `nm`, `readelf`, `objdump`, `sha256sum`, `make` |
| Target triple | `x86_64-elf` |
| ABI | Kernel-internal C ABI; belum ada stable driver ABI publik |
| Boot artifact | Menggunakan image/ISO hasil M2-M13; M14 menambah object block layer yang dapat ditautkan ke kernel |
| Subsystem utama | Block device registry, RAM block driver, buffer cache write-back minimal |
| Storage model | RAM-backed block device volatil; belum persistent ke disk QEMU |
| Filesystem impact | Menyiapkan jalur untuk filesystem persistent pada M15+ |
| Concurrency | Single-core educational baseline; belum SMP-safe; sinkronisasi eksplisit akan dikembangkan setelah lock discipline matang |
| Security posture | Validasi pointer/range internal; belum ada device isolation, DMA protection, capability check, atau user/kernel copy penuh |


---

## 2A. Goals and Non-goals

**Goals M14** adalah menyediakan block device layer kecil yang dapat diuji, RAM block driver volatil, buffer cache minimal, audit object freestanding, dan jalur integrasi kernel yang tidak merusak VFS/RAMFS M13. Target praktikum adalah kemampuan membaca, menulis, dan flush blok secara deterministik dengan bukti host test, ELF audit, dan QEMU smoke test.

**Non-goals M14** adalah driver disk hardware nyata, driver virtio-blk, AHCI, NVMe, DMA, MSI/MSI-X, interrupt completion, filesystem persistent, journal, fsck penuh, crash consistency, POSIX full compliance, user ABI storage publik, security boundary untuk pengguna, dan produksi. Semua non-goals tersebut sengaja ditunda agar kontrak block layer dapat divalidasi terlebih dahulu.

## 2B. Assumptions and Target

Assumptions and target M14 adalah: device class berupa block driver sintetis; target environment berupa x86_64, QEMU, Windows 11 x64 + WSL 2; storage target berupa RAM-backed block device; kernel integration point berupa subsystem block internal; execution context awal berupa single-core boot path atau kernel thread pendidikan; dan crash model masih terbatas pada clean shutdown atau flush eksplisit. Asumsi ini harus dicatat pada laporan karena M14 belum memodelkan persistent media, torn write, write reordering, atau device write-cache lie.

---

## 3. Capaian Pembelajaran

Setelah menyelesaikan M14, mahasiswa mampu:

1. Menjelaskan perbedaan antara file-level I/O pada VFS M13 dan block-level I/O pada storage layer M14.
2. Mendesain kontrak block device yang memisahkan registry, operasi driver, validasi range, dan error code.
3. Mengimplementasikan RAM block driver yang deterministik, tidak bergantung pada dynamic allocation, dan dapat diuji di host.
4. Mengimplementasikan buffer cache minimal dengan `valid`, `dirty`, `lba`, `dev`, dan flush eksplisit.
5. Membuktikan dengan host unit test bahwa operasi read/write/flush dan validasi boundary berjalan sesuai kontrak.
6. Menghasilkan object freestanding x86_64 tanpa undefined symbol setelah linked relocatable aggregation.
7. Menyusun bukti audit menggunakan `nm`, `readelf`, `objdump`, `sha256sum`, QEMU log, dan laporan readiness.
8. Mengidentifikasi failure mode storage awal: out-of-range LBA, dirty buffer tidak di-flush, stale cache, block size mismatch, dan ketidakjelasan ownership buffer.

---

## 4. Prasyarat Teori

Mahasiswa harus menguasai ringkas materi berikut sebelum bekerja:

1. **Block device**: perangkat yang dibaca/ditulis dalam unit blok tetap, biasanya melalui LBA.
2. **LBA**: logical block address; nomor blok logis yang harus divalidasi agar tidak melewati `block_count`.
3. **Block size**: ukuran unit transfer. M14 memakai 512 byte sebagai minimum dan mengharuskan power-of-two agar alignment dan indeks sederhana.
4. **Driver operation table**: tabel function pointer `read`, `write`, `flush` untuk memisahkan caller block layer dari implementasi driver.
5. **Buffer cache**: cache blok memori yang menyimpan salinan blok storage. Entry dirty harus di-flush sebelum diganti atau sebelum shutdown.
6. **Write-back**: penulisan ke cache tidak langsung menulis ke media sampai flush; cepat tetapi berisiko kehilangan data jika crash.
7. **Write-through**: penulisan langsung ke media; lebih sederhana untuk konsistensi tetapi lebih lambat.
8. **Freestanding C**: kode kernel tidak boleh bergantung pada hosted libc, `malloc`, `printf`, atau runtime tersembunyi.
9. **Object audit**: `nm -u`, `readelf -h`, `objdump -dr`, dan checksum digunakan untuk membuktikan artefak build dapat diperiksa.

---

## 5. Peta Skill yang Digunakan

| Skill | Peran dalam M14 |
|---|---|
| `osdev-general` | Readiness gate, roadmap, acceptance criteria, dan integrasi lint dokumen. |
| `osdev-01-computer-foundation` | Invariant, state machine buffer cache, batas operasi, dan pembuktian boundary. |
| `osdev-02-low-level-programming` | C freestanding, object ELF, pointer ownership, overflow check, dan audit undefined symbol. |
| `osdev-03-computer-and-hardware-architecture` | Model perangkat blok, LBA, alignment, dan batas menuju driver hardware. |
| `osdev-04-kernel-development` | Integrasi ke kernel object, syscall/VFS handoff, error path, dan observability. |
| `osdev-05-filesystem-development` | Transfer ke filesystem persistent, block ownership, cache flush, dan crash risk. |
| `osdev-07-os-security` | Validasi input internal, risiko stale data, dan batas trust driver. |
| `osdev-08-device-driver-development` | Kontrak driver, probe/init sederhana, operation table, dan fault taxonomy. |
| `osdev-12-toolchain-devenv` | Makefile, freestanding compile, audit `nm`/`readelf`/`objdump`, checksum, dan reproducibility. |
| `osdev-14-cross-science` | Verification matrix, risk register, failure mode analysis, dan evidence baseline. |

---

## 6. Alat dan Versi yang Harus Dicatat

Mahasiswa wajib mencatat versi alat aktual dari host masing-masing. Jalankan perintah berikut dari WSL 2 pada root repository MCSOS.

Perintah ini mengumpulkan identitas host dan toolchain. Outputnya menjadi bukti bahwa praktikum dibangun pada lingkungan yang dapat diaudit.

```bash
mkdir -p artifacts/m14
{ uname -a; lsb_release -a 2>/dev/null || cat /etc/os-release; } | tee artifacts/m14/host_info.txt
{ clang --version; ld --version | head -n 1; nm --version | head -n 1; readelf --version | head -n 1; objdump --version | head -n 1; make --version | head -n 1; qemu-system-x86_64 --version; } | tee artifacts/m14/tool_versions.txt
```

Indikator hasil yang benar: `artifacts/m14/host_info.txt` dan `artifacts/m14/tool_versions.txt` terisi. Jika `qemu-system-x86_64` tidak ditemukan, perbaiki paket QEMU dari M0/M1 sebelum melanjutkan.

---

## 7. Repository Awal dan Branch Praktikum

Praktikum M14 harus dimulai dari repository yang sudah melewati M13. Gunakan branch baru agar perubahan dapat diaudit.

Perintah berikut memastikan working tree bersih dan membuat branch khusus M14.

```bash
git status --short
git switch -c praktikum-m14-block-device
mkdir -p include/mcsos kernel/block tests/host scripts artifacts/m14
```

Jika `git status --short` menampilkan file hasil kerja M13 yang belum dikomit, hentikan pekerjaan M14 dan lakukan salah satu tindakan berikut:

```bash
git add .
git commit -m "m13: complete vfs ramfs file descriptor baseline"
```

atau simpan sementara pekerjaan yang belum siap:

```bash
git stash push -u -m "temporary work before m14"
```

---

## 8. Pemeriksaan Kesiapan M0-M13

M14 tidak boleh dikerjakan di atas baseline yang tidak stabil. Jalankan pemeriksaan berikut sebelum menyalin source M14.

Perintah ini memeriksa keberadaan artefak utama dari praktikum sebelumnya. Sesuaikan path apabila repository mahasiswa memakai nama direktori berbeda, tetapi jangan menghapus kategori bukti.

```bash
cat > scripts/m14_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p artifacts/m14
LOG="artifacts/m14/preflight.log"
: > "$LOG"

require_file() {
  local path="$1"
  if [[ ! -e "$path" ]]; then
    echo "MISSING: $path" | tee -a "$LOG"
    return 1
  fi
  echo "OK: $path" | tee -a "$LOG"
}

require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "MISSING_CMD: $cmd" | tee -a "$LOG"
    return 1
  fi
  echo "OK_CMD: $cmd=$($cmd --version 2>/dev/null | head -n 1 || true)" | tee -a "$LOG"
}

require_cmd clang
require_cmd ld
require_cmd nm
require_cmd readelf
require_cmd objdump
require_cmd sha256sum
require_cmd make
require_cmd qemu-system-x86_64

for d in include kernel tests scripts; do
  [[ -d "$d" ]] && echo "OK_DIR: $d" | tee -a "$LOG" || echo "WARN_DIR_MISSING: $d" | tee -a "$LOG"
done

for f in OS_panduan_M0.md OS_panduan_M1.md OS_panduan_M2.md OS_panduan_M3.md OS_panduan_M4.md OS_panduan_M5.md OS_panduan_M6.md OS_panduan_M7.md OS_panduan_M8.md OS_panduan_M9.md OS_panduan_M10.md OS_panduan_M11.md OS_panduan_M12.md OS_panduan_M13.md; do
  [[ -e "$f" ]] && echo "OK_DOC: $f" | tee -a "$LOG" || echo "WARN_DOC_NOT_FOUND_IN_REPO: $f" | tee -a "$LOG"
done

git status --short | tee artifacts/m14/git_status_before_m14.txt
if [[ -s artifacts/m14/git_status_before_m14.txt ]]; then
  echo "WARN: working tree tidak bersih; commit atau stash perubahan sebelum final grading" | tee -a "$LOG"
fi

echo "M14_PREFLIGHT_DONE" | tee -a "$LOG"
EOF
chmod +x scripts/m14_preflight.sh
./scripts/m14_preflight.sh
```

Indikator hasil yang benar: log berakhir dengan `M14_PREFLIGHT_DONE`. Status `WARN_DOC_NOT_FOUND_IN_REPO` masih dapat diterima jika dokumen panduan tidak disimpan dalam repository kerja, tetapi bukti praktikum M0-M13 tetap harus tersedia pada laporan mahasiswa.

---

## 9. Saran Perbaikan Kendala dari M0-M13 Sebelum M14

| Gejala | Kemungkinan penyebab | Perbaikan konservatif sebelum lanjut M14 |
|---|---|---|
| `clang` tidak mengenali `--target=x86_64-elf` | Instalasi Clang tidak lengkap atau path salah | Jalankan ulang instalasi toolchain M0/M1; validasi `clang --version`; gunakan package distro yang konsisten. |
| `nm -u` menampilkan simbol runtime seperti `memcpy` | Kode kernel memanggil libc/builtin yang tidak disediakan | Gunakan loop copy internal atau implementasi `memcpy` kernel; tetap audit object dengan `nm -u`. |
| Kernel M13 gagal boot setelah penambahan file | Object baru belum ditambahkan ke link recipe atau urutan link salah | Tambahkan `kernel/block/*.o` ke Makefile kernel; audit linker map dan symbol table. |
| RAMFS M13 kehilangan isi saat reboot | RAMFS memang volatil | Jangan memperlakukan RAMFS sebagai persistent; M14 hanya menyiapkan block device, persistence baru dikaji pada M15+. |
| GDB tidak berhenti pada breakpoint | QEMU tidak dijalankan dengan `-S -s`, symbol file tidak cocok, atau binary berubah setelah build | Rebuild clean; jalankan QEMU dengan `-S -s`; load symbol dari `kernel.elf` yang sama dengan image. |
| Host test lulus tetapi kernel fault | Host test tidak membuktikan integrasi boot/runtime | Audit calling path kernel, stack, section placement, global initialization, dan log serial. |
| File descriptor M13 bocor | `close`/refcount belum konsisten | Perbaiki M13 sebelum M14 karena storage handle berikutnya akan memperbesar dampak leak. |
| Lock M12 belum stabil | Buffer cache M14 belum SMP-safe | Jalankan M14 pada single-core QEMU terlebih dahulu; jangan mengaktifkan preemptive/SMP path untuk block cache ini. |
| `readelf` menunjukkan bukan ELF64 x86-64 | Target triple atau compiler salah | Pastikan command memakai `--target=x86_64-elf`; hapus object lama; rebuild clean. |
| QEMU drive tidak terbaca | Opsi `-drive` salah atau format tidak eksplisit | Pakai `format=raw` dan dokumentasikan command; M14 tidak wajib membaca disk QEMU karena driver nyata belum diimplementasikan. |

---

## 10. Target Praktikum

Target teknis wajib M14 adalah sebagai berikut:

1. Membuat header `include/mcsos/block.h` yang mendefinisikan status code, struktur device, operation table, RAM block metadata, dan buffer cache metadata.
2. Membuat `kernel/block/block.c` sebagai registry dan wrapper validasi read/write/flush.
3. Membuat `kernel/block/ramblk.c` sebagai driver RAM-backed block device.
4. Membuat `kernel/block/bcache.c` sebagai buffer cache minimal dengan dirty flag dan flush eksplisit.
5. Membuat `tests/host/test_m14_block.c` sebagai host unit test.
6. Membuat Makefile target `host-test`, `freestanding`, dan `audit`.
7. Menjalankan host test sampai lulus.
8. Mengompilasi object freestanding x86_64.
9. Menggabungkan object menjadi linked relocatable `m14_block_layer.o` agar `nm -u` dapat memverifikasi tidak ada unresolved internal symbol.
10. Mengarsipkan `readelf`, `objdump`, checksum, log test, dan status Git.

---

## 11. Konsep Inti dan Invariant M14

### 11.1 Invariant Block Device

1. `dev != NULL` untuk seluruh operasi publik.
2. `dev->ops != NULL`, `dev->ops->read != NULL`, dan `dev->ops->write != NULL` sebelum device diregistrasi.
3. `dev->block_size >= 512` dan `dev->block_size` adalah power-of-two.
4. `dev->block_count > 0`.
5. Operasi valid harus memenuhi `lba < block_count` dan `count <= block_count - lba` agar tidak overflow atau out-of-range.
6. Operasi dengan `count == 0` ditolak sebagai `MCSOS_BLK_EINVAL` agar tidak menimbulkan ambiguity.
7. Registry hanya menyimpan pointer device yang lifetime-nya dijamin oleh pemilik device.

### 11.2 Invariant RAM Block Driver

1. `storage` dimiliki oleh caller; driver tidak melakukan dynamic allocation.
2. `storage_size` harus kelipatan `block_size`.
3. `byte_offset = lba * block_size` dan `byte_count = count * block_size` harus tetap berada dalam `storage_size`.
4. `flush` pada RAM block driver adalah no-op yang sukses karena data sudah berada di backing memory volatil.
5. Driver tidak menghapus data saat register; inisialisasi isi storage adalah tanggung jawab test atau kernel initializer.

### 11.3 Invariant Buffer Cache

1. Setiap cache entry memuat tepat satu block.
2. Entry valid harus memiliki pasangan `(dev, lba)` yang terdefinisi.
3. Entry dirty harus di-flush sebelum victim reuse.
4. `cache->block_size` harus sama dengan `dev->block_size`.
5. `bcache_write` mengubah cache dan menandai dirty; data belum wajib muncul di device sampai `flush_all` atau eviction.
6. M14 belum SMP-safe; caller tidak boleh memanggil buffer cache secara concurrent tanpa lock eksternal.

---

## 12. Arsitektur Ringkas

```text
                +-------------------------------+
                |        VFS / RAMFS M13        |
                +---------------+---------------+
                                |
                                | calon integrasi M15+
                                v
+-------------------+    +-----------------------+    +----------------------+
| Host Unit Tests   | -> | M14 Block Device API  | -> | Driver ops table     |
| QEMU smoke path   |    | read/write/flush      |    | read/write/flush     |
+-------------------+    +-----------+-----------+    +----------+-----------+
                                    |                           |
                                    v                           v
                         +-----------------------+    +----------------------+
                         | Buffer Cache          |    | RAM Block Driver     |
                         | valid/dirty/dev/lba   |    | byte-array storage   |
                         +-----------------------+    +----------------------+
```

M14 tidak mengganti VFS M13. M14 menambahkan storage abstraction di bawah VFS agar modul berikutnya dapat mengembangkan filesystem persistent berbasis blok atau block-backed RAMFS secara bertahap.

---


## 12A. Architecture and Design

Architecture and design M14 membagi tanggung jawab menjadi empat batas: API block layer, registry device, driver operation table, dan buffer cache. API block layer memvalidasi LBA, `count`, pointer buffer, dan `block_size`. Registry menyimpan pointer device dengan lifetime yang dimiliki caller. Driver operation table memuat `read`, `write`, dan `flush` agar perangkat sintetis atau perangkat nyata dapat diganti tanpa mengubah caller. Buffer cache menyimpan satu block per entry dan menjadi lapisan perantara untuk eksperimen write-back.

Interface/API internal M14 terdiri atas `mcsos_blk_register`, `mcsos_blk_get`, `mcsos_blk_read`, `mcsos_blk_write`, `mcsos_blk_flush`, `mcsos_ramblk_init`, `mcsos_bcache_init`, `mcsos_bcache_read`, `mcsos_bcache_write`, dan `mcsos_bcache_flush_all`. ABI eksternal ke user program belum dibuat; syscall file I/O M13 belum boleh langsung mengekspos pointer user ke block layer tanpa `copyin/copyout` dan validasi privilege.

## 12B. Kernel Invariants

Kernel invariants M14 adalah: object `mcsos_blk_device_t` harus hidup lebih lama daripada registry entry; caller wajib memastikan tidak ada concurrent access tanpa lock; buffer cache entry valid harus memiliki `(dev, lba)` yang konsisten; dirty entry tidak boleh di-evict tanpa flush sukses; error path harus mengembalikan status code, bukan panic sembarang; dan panic path hanya boleh dipakai untuk invariant fatal pada integrasi kernel, bukan untuk input invalid yang dapat diprediksi.

Cakupan domain kernel yang terdampak meliputi process/thread lifecycle secara tidak langsung melalui file descriptor M13, scheduler dan runqueue karena buffer cache belum boleh dipanggil preemptive tanpa lock, memory/page allocator karena cache pool harus berasal dari memori yang valid, syscall/ABI karena user pointer belum boleh diteruskan langsung, synchronization karena lock/mutex/spinlock belum internal, security karena belum ada credential/capability, observability melalui log/trace/panic/debug, dan testing melalui host test, emulator test, fuzz plan, stress plan, dan fault injection plan.

## 12C. Implementation Plan

Implementation plan M14 bersifat buildable: mulai dari header, registry, RAM block driver, buffer cache, host test, freestanding compile, linked relocatable audit, kernel integration, QEMU smoke test, lalu GDB triage. Setiap checkpoint harus menghasilkan artefak yang dapat diperiksa, bukan hanya klaim berhasil.

## 12D. Validation Plan

Validation plan M14 mencakup unit test host, negative test LBA, boundary test `count`, dirty flush test, freestanding compile, `nm` undefined-symbol check, `readelf` ELF header check, `objdump` disassembly check, checksum, QEMU smoke test, GDB breakpoint, dan rencana fuzz/stress lanjutan. Untuk tahap filesystem berikutnya, validation plan harus diperluas menjadi fuzz input block image, crash test, fsck/scrub test, benchmark latency/throughput, dan fault injection terhadap flush failure.

## 12E. Filesystem Contract Boundary

Filesystem contract M14 belum mendefinisikan format on-disk final. Namun boundary menuju filesystem persistent harus sudah jelas: operasi block layer membaca dan menulis blok tetap; error behavior dikembalikan sebagai `mcsos_blk_status_t`; caller filesystem kelak akan mengelola superblock, inode, directory, allocation bitmap atau extent map, mount version, feature flag, permission, ACL, xattr, quota, encryption metadata, fsync semantics, journal/recovery policy, dan fsck/scrub tool. Karena belum ada superblock, inode, directory, allocation, journal, recovery, dan fsck yang aktif, M14 hanya boleh disebut fondasi storage, bukan filesystem persistent.

Consistency and recovery M14 masih terbatas: dirty buffer hanya menjadi durable terhadap RAM-backed device setelah `flush_all`; crash sebelum flush dapat menyebabkan data hilang; tidak ada `fsync` semantik POSIX; tidak ada journal; tidak ada recovery replay; tidak ada corruption detection; dan diagnostic utama adalah log, test, checksum, `readelf`, `objdump`, serta GDB. Compatibility dengan POSIX, mount options, versioned on-disk feature flags, dan performance benchmark latency/throughput baru masuk tahap desain M15+. Concurrency filesystem berikutnya harus menetapkan lock order, reference lifetime, dan deadlock avoidance sebelum buffer cache dipakai bersama scheduler atau thread lain.

## 12F. Driver Resource and Lifecycle Model

Resource and lifecycle model M14 adalah init/register/use/flush/shutdown sederhana. `probe` disimulasikan oleh `mcsos_ramblk_init`; `remove` belum tersedia; `shutdown` direpresentasikan oleh `mcsos_bcache_flush_all` sebelum kernel berhenti. Device nyata kelak wajib menambahkan probe, remove, reset, shutdown, suspend, resume, hotplug, runtime PM, timeout, dan resource cleanup.

MMIO/register model M14 belum aktif karena RAM block driver tidak memakai MMIO, port I/O, register, atau doorbell. DMA model belum aktif: tidak ada coherent buffer, streaming mapping, scatter-gather, IOMMU, atau cache maintenance. Interrupt model belum aktif: tidak ada IRQ, MSI, MSI-X, completion queue, polling budget, atau interrupt storm handling. Security boundaries tetap harus dicatat karena driver nyata akan menghadapi untrusted descriptor, firmware, DMA, privilege, capability, dan user ABI attack surface.

## 12G. Acceptance Criteria

Acceptance criteria M14 adalah semua checkpoint buildable lulus, semua evidence tersimpan, dan readiness review menyatakan hanya siap uji QEMU untuk block device layer minimal. Gate kelulusan tidak boleh mengizinkan klaim produksi, persistent filesystem, data integrity, atau hardware readiness tanpa bukti tambahan.

## 13. Instruksi Implementasi Langkah demi Langkah

### Langkah 1 - Buat direktori M14

Perintah berikut menyiapkan direktori source, test, script, dan artefak.

```bash
mkdir -p include/mcsos kernel/block tests/host artifacts/m14 scripts
```

Indikator hasil: direktori `include/mcsos`, `kernel/block`, dan `tests/host` tersedia.

### Langkah 2 - Tambahkan header block layer

File header ini adalah kontrak publik internal kernel untuk M14. Header memuat struktur dan fungsi, tetapi tidak mengandung implementasi. Pastikan nama file persis `include/mcsos/block.h`.

```c
#ifndef MCSOS_BLOCK_H
#define MCSOS_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#define MCSOS_BLK_NAME_MAX 16u
#define MCSOS_BLK_MAX_DEVICES 8u
#define MCSOS_BLK_DEFAULT_SECTOR_SIZE 512u

typedef enum mcsos_blk_status {
    MCSOS_BLK_OK = 0,
    MCSOS_BLK_EINVAL = -1,
    MCSOS_BLK_ERANGE = -2,
    MCSOS_BLK_EFULL = -3,
    MCSOS_BLK_EIO = -4,
    MCSOS_BLK_ENODEV = -5
} mcsos_blk_status_t;

struct mcsos_blk_device;

typedef mcsos_blk_status_t (*mcsos_blk_rw_fn)(struct mcsos_blk_device *dev,
                                               uint64_t lba,
                                               uint32_t count,
                                               void *buffer);

typedef struct mcsos_blk_ops {
    mcsos_blk_rw_fn read;
    mcsos_blk_rw_fn write;
    mcsos_blk_rw_fn flush;
} mcsos_blk_ops_t;

typedef struct mcsos_blk_device {
    char name[MCSOS_BLK_NAME_MAX];
    uint32_t block_size;
    uint64_t block_count;
    uint32_t flags;
    const mcsos_blk_ops_t *ops;
    void *driver_data;
} mcsos_blk_device_t;

typedef struct mcsos_ramblk {
    uint8_t *storage;
    uint64_t storage_size;
} mcsos_ramblk_t;

typedef struct mcsos_bcache_entry {
    uint8_t *data;
    uint32_t capacity;
    uint64_t lba;
    int valid;
    int dirty;
    mcsos_blk_device_t *dev;
} mcsos_bcache_entry_t;

typedef struct mcsos_bcache {
    mcsos_bcache_entry_t *entries;
    uint32_t entry_count;
    uint8_t *data_pool;
    uint32_t block_size;
    uint64_t clock_hand;
} mcsos_bcache_t;

void mcsos_blk_registry_reset(void);
mcsos_blk_status_t mcsos_blk_register(mcsos_blk_device_t *dev);
mcsos_blk_device_t *mcsos_blk_get(uint32_t index);
uint32_t mcsos_blk_count(void);
mcsos_blk_status_t mcsos_blk_read(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, void *buffer);
mcsos_blk_status_t mcsos_blk_write(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, const void *buffer);
mcsos_blk_status_t mcsos_blk_flush(mcsos_blk_device_t *dev);

mcsos_blk_status_t mcsos_ramblk_init(mcsos_blk_device_t *dev,
                                     mcsos_ramblk_t *ram,
                                     const char *name,
                                     uint8_t *storage,
                                     uint64_t storage_size,
                                     uint32_t block_size);

mcsos_blk_status_t mcsos_bcache_init(mcsos_bcache_t *cache,
                                     mcsos_bcache_entry_t *entries,
                                     uint32_t entry_count,
                                     uint8_t *data_pool,
                                     uint32_t block_size);
mcsos_blk_status_t mcsos_bcache_read(mcsos_bcache_t *cache,
                                     mcsos_blk_device_t *dev,
                                     uint64_t lba,
                                     void *buffer);
mcsos_blk_status_t mcsos_bcache_write(mcsos_bcache_t *cache,
                                      mcsos_blk_device_t *dev,
                                      uint64_t lba,
                                      const void *buffer);
mcsos_blk_status_t mcsos_bcache_flush_all(mcsos_bcache_t *cache);

#endif
```

### Langkah 3 - Tambahkan registry dan wrapper validasi block device

File ini menolak operasi invalid sebelum mencapai driver. Hal ini penting karena driver hardware berikutnya tidak boleh menerima LBA out-of-range atau buffer null.

```c
#include "mcsos/block.h"

static mcsos_blk_device_t *g_blk_devices[MCSOS_BLK_MAX_DEVICES];
static uint32_t g_blk_count;

static int mcsos_is_power_of_two_u32(uint32_t value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

static int mcsos_name_is_nonempty(const char *s) {
    return s != 0 && s[0] != '\0';
}

static void mcsos_copy_name(char dst[MCSOS_BLK_NAME_MAX], const char *src) {
    uint32_t i = 0;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    while (i + 1u < MCSOS_BLK_NAME_MAX && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void mcsos_blk_registry_reset(void) {
    for (uint32_t i = 0; i < MCSOS_BLK_MAX_DEVICES; i++) {
        g_blk_devices[i] = 0;
    }
    g_blk_count = 0;
}

mcsos_blk_status_t mcsos_blk_register(mcsos_blk_device_t *dev) {
    if (dev == 0 || dev->ops == 0 || dev->ops->read == 0 || dev->ops->write == 0) {
        return MCSOS_BLK_EINVAL;
    }
    if (!mcsos_name_is_nonempty(dev->name) || dev->block_count == 0u ||
        dev->block_size < MCSOS_BLK_DEFAULT_SECTOR_SIZE ||
        !mcsos_is_power_of_two_u32(dev->block_size)) {
        return MCSOS_BLK_EINVAL;
    }
    if (g_blk_count >= MCSOS_BLK_MAX_DEVICES) {
        return MCSOS_BLK_EFULL;
    }
    g_blk_devices[g_blk_count++] = dev;
    return MCSOS_BLK_OK;
}

mcsos_blk_device_t *mcsos_blk_get(uint32_t index) {
    if (index >= g_blk_count) {
        return 0;
    }
    return g_blk_devices[index];
}

uint32_t mcsos_blk_count(void) {
    return g_blk_count;
}

static mcsos_blk_status_t mcsos_blk_validate_range(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, const void *buffer) {
    if (dev == 0 || buffer == 0 || count == 0u || dev->ops == 0) {
        return MCSOS_BLK_EINVAL;
    }
    if (lba >= dev->block_count) {
        return MCSOS_BLK_ERANGE;
    }
    if ((uint64_t)count > dev->block_count - lba) {
        return MCSOS_BLK_ERANGE;
    }
    return MCSOS_BLK_OK;
}

mcsos_blk_status_t mcsos_blk_read(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    mcsos_blk_status_t st = mcsos_blk_validate_range(dev, lba, count, buffer);
    if (st != MCSOS_BLK_OK) {
        return st;
    }
    if (dev->ops->read == 0) {
        return MCSOS_BLK_EINVAL;
    }
    return dev->ops->read(dev, lba, count, buffer);
}

mcsos_blk_status_t mcsos_blk_write(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, const void *buffer) {
    mcsos_blk_status_t st = mcsos_blk_validate_range(dev, lba, count, buffer);
    if (st != MCSOS_BLK_OK) {
        return st;
    }
    if (dev->ops->write == 0) {
        return MCSOS_BLK_EINVAL;
    }
    return dev->ops->write(dev, lba, count, (void *)buffer);
}

mcsos_blk_status_t mcsos_blk_flush(mcsos_blk_device_t *dev) {
    if (dev == 0 || dev->ops == 0) {
        return MCSOS_BLK_EINVAL;
    }
    if (dev->ops->flush == 0) {
        return MCSOS_BLK_OK;
    }
    return dev->ops->flush(dev, 0, 0, 0);
}

void mcsos_blk_copy_name_for_driver(char dst[MCSOS_BLK_NAME_MAX], const char *src) {
    mcsos_copy_name(dst, src);
}
```

### Langkah 4 - Tambahkan RAM block driver

Driver ini meniru block device dengan array memori. Driver tidak memakai `malloc`, tidak memakai libc, dan cocok untuk host unit test serta freestanding compile.

```c
#include "mcsos/block.h"

extern void mcsos_blk_copy_name_for_driver(char dst[MCSOS_BLK_NAME_MAX], const char *src);

static void mcsos_memcpy_u8(void *dst, const void *src, uint64_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint64_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static int mcsos_is_power_of_two_u32_local(uint32_t value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

static mcsos_blk_status_t mcsos_ramblk_rw(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, void *buffer, int is_write) {
    if (dev == 0 || dev->driver_data == 0 || buffer == 0) {
        return MCSOS_BLK_EINVAL;
    }
    mcsos_ramblk_t *ram = (mcsos_ramblk_t *)dev->driver_data;
    uint64_t byte_offset = lba * (uint64_t)dev->block_size;
    uint64_t byte_count = (uint64_t)count * (uint64_t)dev->block_size;
    if (byte_offset > ram->storage_size || byte_count > ram->storage_size - byte_offset) {
        return MCSOS_BLK_ERANGE;
    }
    if (is_write) {
        mcsos_memcpy_u8(ram->storage + byte_offset, buffer, byte_count);
    } else {
        mcsos_memcpy_u8(buffer, ram->storage + byte_offset, byte_count);
    }
    return MCSOS_BLK_OK;
}

static mcsos_blk_status_t mcsos_ramblk_read(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    return mcsos_ramblk_rw(dev, lba, count, buffer, 0);
}

static mcsos_blk_status_t mcsos_ramblk_write(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    return mcsos_ramblk_rw(dev, lba, count, buffer, 1);
}

static mcsos_blk_status_t mcsos_ramblk_flush(mcsos_blk_device_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    (void)dev;
    (void)lba;
    (void)count;
    (void)buffer;
    return MCSOS_BLK_OK;
}

static const mcsos_blk_ops_t g_ramblk_ops = {
    .read = mcsos_ramblk_read,
    .write = mcsos_ramblk_write,
    .flush = mcsos_ramblk_flush,
};

mcsos_blk_status_t mcsos_ramblk_init(mcsos_blk_device_t *dev,
                                     mcsos_ramblk_t *ram,
                                     const char *name,
                                     uint8_t *storage,
                                     uint64_t storage_size,
                                     uint32_t block_size) {
    if (dev == 0 || ram == 0 || storage == 0 || name == 0) {
        return MCSOS_BLK_EINVAL;
    }
    if (block_size < MCSOS_BLK_DEFAULT_SECTOR_SIZE || !mcsos_is_power_of_two_u32_local(block_size)) {
        return MCSOS_BLK_EINVAL;
    }
    if (storage_size < block_size || (storage_size % block_size) != 0u) {
        return MCSOS_BLK_EINVAL;
    }
    ram->storage = storage;
    ram->storage_size = storage_size;
    mcsos_blk_copy_name_for_driver(dev->name, name);
    dev->block_size = block_size;
    dev->block_count = storage_size / block_size;
    dev->flags = 0;
    dev->ops = &g_ramblk_ops;
    dev->driver_data = ram;
    return MCSOS_BLK_OK;
}
```

### Langkah 5 - Tambahkan buffer cache minimal

Buffer cache ini write-back sederhana. Tujuannya bukan performa, melainkan memperkenalkan invariant `valid`, `dirty`, `dev`, dan `lba` sebelum masuk filesystem persistent.

```c
#include "mcsos/block.h"

static void mcsos_memcpy_u8_bcache(void *dst, const void *src, uint64_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint64_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static mcsos_bcache_entry_t *mcsos_bcache_find(mcsos_bcache_t *cache, mcsos_blk_device_t *dev, uint64_t lba) {
    for (uint32_t i = 0; i < cache->entry_count; i++) {
        mcsos_bcache_entry_t *e = &cache->entries[i];
        if (e->valid && e->dev == dev && e->lba == lba) {
            return e;
        }
    }
    return 0;
}

static mcsos_blk_status_t mcsos_bcache_flush_entry(mcsos_bcache_entry_t *e) {
    if (e == 0 || !e->valid || !e->dirty) {
        return MCSOS_BLK_OK;
    }
    mcsos_blk_status_t st = mcsos_blk_write(e->dev, e->lba, 1u, e->data);
    if (st != MCSOS_BLK_OK) {
        return st;
    }
    e->dirty = 0;
    return MCSOS_BLK_OK;
}

static mcsos_blk_status_t mcsos_bcache_select_victim(mcsos_bcache_t *cache, mcsos_bcache_entry_t **out) {
    if (cache == 0 || out == 0 || cache->entry_count == 0u) {
        return MCSOS_BLK_EINVAL;
    }
    uint32_t start = (uint32_t)(cache->clock_hand % cache->entry_count);
    for (uint32_t pass = 0; pass < cache->entry_count; pass++) {
        uint32_t idx = (start + pass) % cache->entry_count;
        if (!cache->entries[idx].valid) {
            cache->clock_hand = idx + 1u;
            *out = &cache->entries[idx];
            return MCSOS_BLK_OK;
        }
    }
    uint32_t idx = start;
    cache->clock_hand = idx + 1u;
    mcsos_blk_status_t st = mcsos_bcache_flush_entry(&cache->entries[idx]);
    if (st != MCSOS_BLK_OK) {
        return st;
    }
    *out = &cache->entries[idx];
    return MCSOS_BLK_OK;
}

mcsos_blk_status_t mcsos_bcache_init(mcsos_bcache_t *cache,
                                     mcsos_bcache_entry_t *entries,
                                     uint32_t entry_count,
                                     uint8_t *data_pool,
                                     uint32_t block_size) {
    if (cache == 0 || entries == 0 || data_pool == 0 || entry_count == 0u || block_size == 0u) {
        return MCSOS_BLK_EINVAL;
    }
    cache->entries = entries;
    cache->entry_count = entry_count;
    cache->data_pool = data_pool;
    cache->block_size = block_size;
    cache->clock_hand = 0;
    for (uint32_t i = 0; i < entry_count; i++) {
        entries[i].data = data_pool + ((uint64_t)i * (uint64_t)block_size);
        entries[i].capacity = block_size;
        entries[i].lba = 0;
        entries[i].valid = 0;
        entries[i].dirty = 0;
        entries[i].dev = 0;
    }
    return MCSOS_BLK_OK;
}

mcsos_blk_status_t mcsos_bcache_read(mcsos_bcache_t *cache,
                                     mcsos_blk_device_t *dev,
                                     uint64_t lba,
                                     void *buffer) {
    if (cache == 0 || dev == 0 || buffer == 0 || cache->block_size != dev->block_size) {
        return MCSOS_BLK_EINVAL;
    }
    mcsos_bcache_entry_t *e = mcsos_bcache_find(cache, dev, lba);
    if (e == 0) {
        mcsos_blk_status_t st = mcsos_bcache_select_victim(cache, &e);
        if (st != MCSOS_BLK_OK) {
            return st;
        }
        st = mcsos_blk_read(dev, lba, 1u, e->data);
        if (st != MCSOS_BLK_OK) {
            e->valid = 0;
            return st;
        }
        e->dev = dev;
        e->lba = lba;
        e->valid = 1;
        e->dirty = 0;
    }
    mcsos_memcpy_u8_bcache(buffer, e->data, cache->block_size);
    return MCSOS_BLK_OK;
}

mcsos_blk_status_t mcsos_bcache_write(mcsos_bcache_t *cache,
                                      mcsos_blk_device_t *dev,
                                      uint64_t lba,
                                      const void *buffer) {
    if (cache == 0 || dev == 0 || buffer == 0 || cache->block_size != dev->block_size) {
        return MCSOS_BLK_EINVAL;
    }
    mcsos_bcache_entry_t *e = mcsos_bcache_find(cache, dev, lba);
    if (e == 0) {
        mcsos_blk_status_t st = mcsos_bcache_select_victim(cache, &e);
        if (st != MCSOS_BLK_OK) {
            return st;
        }
        e->dev = dev;
        e->lba = lba;
        e->valid = 1;
        e->dirty = 0;
    }
    mcsos_memcpy_u8_bcache(e->data, buffer, cache->block_size);
    e->dirty = 1;
    return MCSOS_BLK_OK;
}

mcsos_blk_status_t mcsos_bcache_flush_all(mcsos_bcache_t *cache) {
    if (cache == 0 || cache->entries == 0) {
        return MCSOS_BLK_EINVAL;
    }
    for (uint32_t i = 0; i < cache->entry_count; i++) {
        mcsos_blk_status_t st = mcsos_bcache_flush_entry(&cache->entries[i]);
        if (st != MCSOS_BLK_OK) {
            return st;
        }
    }
    return MCSOS_BLK_OK;
}
```

### Langkah 6 - Tambahkan host unit test

Host unit test menguji jalur yang tidak memerlukan QEMU: registrasi device, read/write RAM block, validasi range, write-back cache, dan flush. Test host tidak membuktikan runtime kernel, tetapi wajib sebelum integrasi kernel.

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mcsos/block.h"

#define EXPECT_OK(x) do { mcsos_blk_status_t st__ = (x); if (st__ != MCSOS_BLK_OK) { printf("FAIL:%s:%d status=%d\n", __FILE__, __LINE__, (int)st__); return 1; } } while (0)
#define EXPECT_EQ(a,b) do { uint64_t aa__=(uint64_t)(a); uint64_t bb__=(uint64_t)(b); if (aa__ != bb__) { printf("FAIL:%s:%d got=%llu want=%llu\n", __FILE__, __LINE__, (unsigned long long)aa__, (unsigned long long)bb__); return 1; } } while (0)
#define EXPECT_STATUS(expr,want) do { mcsos_blk_status_t st__=(expr); if (st__ != (want)) { printf("FAIL:%s:%d status=%d want=%d\n", __FILE__, __LINE__, (int)st__, (int)(want)); return 1; } } while (0)

static void fill(uint8_t *p, size_t n, uint8_t seed) {
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)(seed + (uint8_t)i);
    }
}

int main(void) {
    uint8_t backing[512u * 32u];
    uint8_t tmp[512u];
    uint8_t out[512u];
    memset(backing, 0, sizeof(backing));
    memset(tmp, 0, sizeof(tmp));
    memset(out, 0, sizeof(out));

    mcsos_blk_registry_reset();
    mcsos_blk_device_t dev;
    mcsos_ramblk_t ram;
    EXPECT_OK(mcsos_ramblk_init(&dev, &ram, "ram0", backing, sizeof(backing), 512u));
    EXPECT_OK(mcsos_blk_register(&dev));
    EXPECT_EQ(mcsos_blk_count(), 1u);
    EXPECT_EQ(mcsos_blk_get(0u), &dev);
    EXPECT_EQ(dev.block_count, 32u);

    fill(tmp, sizeof(tmp), 7u);
    EXPECT_OK(mcsos_blk_write(&dev, 3u, 1u, tmp));
    EXPECT_OK(mcsos_blk_read(&dev, 3u, 1u, out));
    EXPECT_EQ(memcmp(tmp, out, sizeof(tmp)), 0);
    EXPECT_STATUS(mcsos_blk_read(&dev, 32u, 1u, out), MCSOS_BLK_ERANGE);
    EXPECT_STATUS(mcsos_blk_write(&dev, 31u, 2u, tmp), MCSOS_BLK_ERANGE);
    EXPECT_STATUS(mcsos_blk_write(&dev, 0u, 0u, tmp), MCSOS_BLK_EINVAL);
    EXPECT_STATUS(mcsos_blk_write(&dev, 0u, 1u, 0), MCSOS_BLK_EINVAL);

    mcsos_bcache_t cache;
    mcsos_bcache_entry_t entries[2];
    uint8_t pool[2u * 512u];
    EXPECT_OK(mcsos_bcache_init(&cache, entries, 2u, pool, 512u));
    fill(tmp, sizeof(tmp), 42u);
    EXPECT_OK(mcsos_bcache_write(&cache, &dev, 4u, tmp));
    memset(out, 0, sizeof(out));
    EXPECT_OK(mcsos_bcache_read(&cache, &dev, 4u, out));
    EXPECT_EQ(memcmp(tmp, out, sizeof(tmp)), 0);
    memset(out, 0, sizeof(out));
    EXPECT_OK(mcsos_blk_read(&dev, 4u, 1u, out));
    EXPECT_EQ(memcmp(tmp, out, sizeof(tmp)) != 0, 1u);
    EXPECT_OK(mcsos_bcache_flush_all(&cache));
    EXPECT_OK(mcsos_blk_read(&dev, 4u, 1u, out));
    EXPECT_EQ(memcmp(tmp, out, sizeof(tmp)), 0);

    fill(tmp, sizeof(tmp), 100u);
    EXPECT_OK(mcsos_bcache_write(&cache, &dev, 5u, tmp));
    EXPECT_OK(mcsos_bcache_flush_all(&cache));
    EXPECT_OK(mcsos_blk_read(&dev, 5u, 1u, out));
    EXPECT_EQ(memcmp(tmp, out, sizeof(tmp)), 0);

    printf("M14 host tests PASS\n");
    return 0;
}
```

### Langkah 7 - Tambahkan Makefile M14

Makefile berikut menyediakan target host test, freestanding compile, linked relocatable object, dan audit artefak. Perhatikan bahwa `nm -u` dijalankan pada `build/m14_block_layer.o`, bukan pada setiap object terpisah, karena object terpisah secara normal masih memiliki referensi internal antar-file.

```makefile
CC ?= cc
CLANG ?= clang
CFLAGS_HOST := -std=c17 -Wall -Wextra -Werror -Iinclude -O2
CFLAGS_FREESTANDING := --target=x86_64-elf -std=c17 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra -Werror -Iinclude -O2 -c
SRC := kernel/block/block.c kernel/block/ramblk.c kernel/block/bcache.c
OBJ := build/block.o build/ramblk.o build/bcache.o

.PHONY: all host-test freestanding audit clean
all: host-test freestanding audit

host-test: build/test_m14_block
	./build/test_m14_block

build/test_m14_block: tests/host/test_m14_block.c $(SRC) include/mcsos/block.h
	mkdir -p build
	$(CC) $(CFLAGS_HOST) tests/host/test_m14_block.c $(SRC) -o $@

freestanding: $(OBJ)

build/%.o: kernel/block/%.c include/mcsos/block.h
	mkdir -p build
	$(CLANG) $(CFLAGS_FREESTANDING) $< -o $@

audit: freestanding
	ld -r -o build/m14_block_layer.o $(OBJ)
	nm -u build/m14_block_layer.o > artifacts/m14_nm_undefined.txt
	readelf -h build/m14_block_layer.o > artifacts/m14_readelf_block.txt
	objdump -dr build/m14_block_layer.o > artifacts/m14_objdump_block.txt
	sha256sum $(OBJ) build/m14_block_layer.o build/test_m14_block > artifacts/m14_sha256.txt
	test ! -s artifacts/m14_nm_undefined.txt

clean:
	rm -rf build artifacts/*
```

### Langkah 8 - Jalankan build dan test M14

Perintah berikut melakukan clean build dan seluruh pemeriksaan lokal M14.

```bash
make clean || true
make all | tee artifacts/m14/m14_make_all.log
```

Indikator hasil benar:

```text
M14 host tests PASS
```

Target `audit` juga harus menghasilkan file berikut:

```text
artifacts/m14_nm_undefined.txt
artifacts/m14_readelf_block.txt
artifacts/m14_objdump_block.txt
artifacts/m14_sha256.txt
```

`artifacts/m14_nm_undefined.txt` harus kosong. Jika tidak kosong, jangan lanjut ke integrasi kernel.

---

## 14. Bukti Pemeriksaan Source Code Lokal

Source code inti M14 dalam panduan ini telah diperiksa menggunakan host unit test, freestanding compile, linked relocatable aggregation, `nm`, `readelf`, `objdump`, dan checksum. Output ringkas pemeriksaan lokal:

```text
M14 host tests PASS
```

Ringkasan ELF header linked relocatable object:

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
  Start of section headers:          5664 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           64 (bytes)
  Number of section headers:         12
  Section header string table index: 11
```

Checksum artefak lokal:

```text
cf3ebd83f2b6cef120b2e787f5fae30d052016b75223cad1d46daf450dc1120c  build/block.o
224df2ba4178d6cf104b708cc11b8bed5bbde337b7e4e49feff2392c4ecbc686  build/ramblk.o
8aa9a274ae0a9d399ab00e0fc0a8187c78685daf1b840f52c9af6783f95db41d  build/bcache.o
c32de6b40517b09db1dc338e30ef8aa142950f86da00ea68e6d12e9f4834c514  build/m14_block_layer.o
6ca8619a39c2d8f6b8cc378ef5bdea5a2408d353977c8fd880b89d6f651229f2  build/test_m14_block

```

Catatan batasan: pemeriksaan lokal ini membuktikan source M14 dapat dikompilasi dan host test dapat dijalankan pada container pengujian. Mahasiswa tetap wajib menjalankan ulang semua perintah pada WSL 2 masing-masing karena versi Clang, GNU Binutils, QEMU, dan layout repository dapat berbeda.

---

## 15. Integrasi ke Kernel MCSOS

Setelah host test dan freestanding audit lulus, tambahkan source berikut ke build kernel utama:

```text
kernel/block/block.c
kernel/block/ramblk.c
kernel/block/bcache.c
```

Jika Makefile kernel M2-M13 memakai variabel `KERNEL_C_SRCS`, tambahkan:

```makefile
KERNEL_C_SRCS += kernel/block/block.c
KERNEL_C_SRCS += kernel/block/ramblk.c
KERNEL_C_SRCS += kernel/block/bcache.c
```

Tambahkan include path jika belum ada:

```makefile
KERNEL_CFLAGS += -Iinclude
```

Kemudian buat initializer pendidikan, misalnya `kernel/block/block_demo.c`, hanya bila repository sudah memiliki jalur logging kernel yang stabil dari M3:

```c
#include "mcsos/block.h"

static unsigned char g_m14_ramdisk_storage[512u * 64u];
static mcsos_blk_device_t g_m14_ramdisk_dev;
static mcsos_ramblk_t g_m14_ramdisk;

void m14_block_demo_init(void) {
    mcsos_blk_registry_reset();
    if (mcsos_ramblk_init(&g_m14_ramdisk_dev,
                          &g_m14_ramdisk,
                          "ram0",
                          g_m14_ramdisk_storage,
                          sizeof(g_m14_ramdisk_storage),
                          512u) != MCSOS_BLK_OK) {
        /* Ganti dengan panic/log path M3 pada repository mahasiswa. */
        return;
    }
    (void)mcsos_blk_register(&g_m14_ramdisk_dev);
}
```

Kontrak initializer:

1. Storage array bersifat static agar lifetime lebih panjang dari registry.
2. Inisialisasi dilakukan setelah `.bss` valid dan sebelum filesystem persistent memakai block layer.
3. Jika init gagal, kernel harus masuk panic/log path yang sudah dibuat pada M3, bukan melanjutkan dengan device invalid.
4. Pada M14, demo init tidak boleh dipakai sebagai bukti persistence.

---

## 16. Workflow QEMU Smoke Test

Setelah object M14 ditautkan ke kernel, jalankan QEMU smoke test menggunakan command dari M2-M13. Contoh konservatif:

```bash
mkdir -p artifacts/m14
make clean
make all 2>&1 | tee artifacts/m14/kernel_build.log
qemu-system-x86_64 \
  -machine q35 \
  -m 256M \
  -serial stdio \
  -no-reboot \
  -no-shutdown \
  -cdrom build/mcsos.iso \
  2>&1 | tee artifacts/m14/qemu_m14.log
```

Jika repository sudah mendukung disk image tetapi M14 belum punya driver disk fisik, boleh menambahkan disk mentah hanya untuk kesiapan command QEMU, bukan untuk klaim driver:

```bash
truncate -s 16M artifacts/m14/m14_disk.raw
qemu-system-x86_64 \
  -machine q35 \
  -m 256M \
  -serial stdio \
  -no-reboot \
  -no-shutdown \
  -cdrom build/mcsos.iso \
  -drive file=artifacts/m14/m14_disk.raw,if=ide,format=raw \
  2>&1 | tee artifacts/m14/qemu_m14_with_raw_drive.log
```

Indikator hasil minimal:

1. Kernel tetap mencapai log milestone M14 atau shell/panic path terkontrol.
2. Tidak terjadi triple fault, reboot loop, atau hang tanpa log.
3. Log serial tersimpan.
4. Jika block demo init dipanggil, log menunjukkan device `ram0` terdaftar atau status init terbaca.

---

## 17. Workflow GDB

Gunakan GDB saat QEMU smoke test gagal atau saat mahasiswa perlu membuktikan fungsi block layer terpanggil.

Terminal 1:

```bash
qemu-system-x86_64 \
  -machine q35 \
  -m 256M \
  -serial stdio \
  -no-reboot \
  -no-shutdown \
  -S -s \
  -cdrom build/mcsos.iso
```

Terminal 2:

```bash
gdb build/kernel.elf \
  -ex 'target remote :1234' \
  -ex 'break mcsos_blk_register' \
  -ex 'break mcsos_blk_read' \
  -ex 'break mcsos_blk_write' \
  -ex 'continue'
```

Bukti yang harus disimpan:

```bash
mkdir -p artifacts/m14
gdb build/kernel.elf \
  -ex 'target remote :1234' \
  -ex 'info breakpoints' \
  -ex 'info registers' \
  -ex 'quit' \
  | tee artifacts/m14/gdb_m14_session.txt
```

---

## 18. Checkpoint Buildable

| Checkpoint | Perintah | Bukti wajib | Status lulus |
|---|---|---|---|
| CP14.1 | `./scripts/m14_preflight.sh` | `artifacts/m14/preflight.log` | Toolchain dan baseline M0-M13 terdokumentasi. |
| CP14.2 | `make host-test` | `artifacts/m14/m14_make_all.log` atau output terminal | Host unit test lulus. |
| CP14.3 | `make freestanding` | `build/block.o`, `build/ramblk.o`, `build/bcache.o` | Object x86_64 freestanding terbentuk. |
| CP14.4 | `make audit` | `m14_nm_undefined.txt`, `readelf`, `objdump`, checksum | Undefined symbol kosong pada linked relocatable object. |
| CP14.5 | Integrasi ke kernel | linker map dan log build kernel | Kernel build tetap berhasil. |
| CP14.6 | QEMU smoke test | `qemu_m14.log` | Boot/log milestone terkontrol. |
| CP14.7 | Git commit | `git log --oneline -n 3` | Perubahan M14 terkomit. |

---

## 19. Tugas Implementasi

### Tugas Wajib

1. Implementasikan semua file source M14 sesuai panduan.
2. Jalankan `make all` sampai host test, freestanding compile, dan audit lulus.
3. Integrasikan object M14 ke build kernel utama.
4. Tambahkan log milestone M14 pada kernel, misalnya `M14: block layer initialized`.
5. Jalankan QEMU smoke test dan simpan log serial.
6. Jelaskan invariant block device, RAM block driver, dan buffer cache pada laporan.

### Tugas Pengayaan

1. Tambahkan counter statistik: jumlah read, write, flush, cache hit, cache miss, dan eviction.
2. Tambahkan mode write-through opsional pada buffer cache.
3. Tambahkan negative test untuk `block_size` tidak power-of-two.
4. Tambahkan `mcsos_blk_dump_devices()` yang menulis daftar device ke logger kernel.

### Tantangan Riset

1. Rancang perbandingan desain single-queue M14 dengan block request queue multi-queue konseptual seperti `blk-mq` Linux [2].
2. Buat model state machine buffer cache dan verifikasi property: dirty entry tidak boleh hilang saat eviction sukses.
3. Rancang interface block layer yang siap menerima driver virtio-blk pada modul berikutnya tanpa mengubah VFS.

---

## 20. Perintah Uji Lengkap

Jalankan urutan berikut sebagai grading lokal:

```bash
./scripts/m14_preflight.sh
make clean || true
make all 2>&1 | tee artifacts/m14/m14_make_all.log
cat artifacts/m14/m14_nm_undefined.txt
head -n 30 artifacts/m14/m14_readelf_block.txt
grep -E "Class:|Machine:|Type:" artifacts/m14/m14_readelf_block.txt
sha256sum build/m14_block_layer.o build/test_m14_block | tee artifacts/m14/m14_final_sha256.txt
git status --short | tee artifacts/m14/git_status_after_m14.txt
```

Kriteria output:

1. `make all` selesai tanpa error.
2. `M14 host tests PASS` muncul.
3. `m14_nm_undefined.txt` kosong.
4. `readelf` menunjukkan `Class: ELF64`, `Type: REL`, dan `Machine: Advanced Micro Devices X86-64`.
5. Checksum disimpan.

---

## 21. Failure Modes dan Diagnosis

| Failure mode | Sinyal | Diagnosis | Perbaikan |
|---|---|---|---|
| LBA out-of-range | Test boundary gagal atau kernel panic | `count > block_count - lba` tidak dicek | Gunakan validasi range seperti `mcsos_blk_validate_range`. |
| Integer overflow offset | Data corrupt pada LBA besar | `lba * block_size` overflow tidak dianalisis | Untuk tahap lanjut, tambah helper checked multiplication; pada M14 range wrapper mengurangi risiko tetapi driver tetap harus hati-hati. |
| Undefined symbol | `nm -u` tidak kosong | Kode memanggil libc atau object belum digabung | Jalankan `ld -r` pada seluruh object M14; implementasikan helper internal. |
| Dirty buffer hilang | Setelah eviction/flush data tidak ada di device | Victim reuse tidak flush dirty entry | Audit `mcsos_bcache_select_victim` dan `mcsos_bcache_flush_entry`. |
| Cache stale | Read dari cache tidak sesuai device setelah external write | Tidak ada invalidation protocol | M14 tidak mendukung external write; dokumentasikan batasan. |
| Device lifetime invalid | Crash setelah register | Device dialokasikan di stack lalu pointer disimpan registry | Gunakan static/global storage untuk device yang diregistrasi. |
| Host test lulus, QEMU gagal | Integrasi kernel salah | Makefile kernel, include path, linker script, atau init order bermasalah | Audit linker map, serial log, dan breakpoint GDB. |
| QEMU reboot loop | Triple fault atau panic restart | Fault sebelum log serial | Jalankan `-no-reboot -no-shutdown -S -s`, inspect GDB. |
| Block size mismatch | `MCSOS_BLK_EINVAL` dari cache | `cache->block_size != dev->block_size` | Samakan block size saat `mcsos_bcache_init`. |
| Registry penuh | `MCSOS_BLK_EFULL` | Melebihi `MCSOS_BLK_MAX_DEVICES` | Naikkan konstanta atau perbaiki duplicate registration. |

---

## 22. Prosedur Rollback

Jika M14 membuat kernel gagal boot, lakukan rollback bertahap, bukan menghapus seluruh repository.

1. Simpan bukti kegagalan:

```bash
mkdir -p artifacts/m14/failure
git diff > artifacts/m14/failure/m14_failure.diff
git status --short > artifacts/m14/failure/git_status.txt
cp artifacts/m14/qemu_m14.log artifacts/m14/failure/ 2>/dev/null || true
```

2. Nonaktifkan integrasi kernel tetapi pertahankan host test:

```bash
git restore Makefile
# atau hapus sementara kernel/block/*.c dari KERNEL_C_SRCS
make host-test
```

3. Kembali ke commit M13 stabil bila perlu:

```bash
git switch main
git log --oneline -n 5
```

4. Jika ingin membuang branch M14 yang rusak:

```bash
git branch -D praktikum-m14-block-device
```

Rollback valid jika repository kembali dapat membangun M13 atau checkpoint terakhir yang lulus.

---

## 23. Security dan Reliability Review

M14 memiliki risiko yang harus ditulis eksplisit pada laporan:

1. **Tidak ada user/kernel copy hardening**: API M14 bersifat kernel-internal. Jangan langsung mengekspos buffer pointer user ke block layer.
2. **Tidak ada DMA**: RAM block driver tidak membuktikan keamanan DMA, IOMMU, cache coherency, atau interrupt completion.
3. **Tidak SMP-safe**: buffer cache belum memakai spinlock/mutex M12. Caller wajib single-threaded atau memberi lock eksternal.
4. **Tidak crash-consistent**: dirty buffer dapat hilang jika kernel crash sebelum flush.
5. **Tidak persistent**: RAM block driver hilang saat reboot.
6. **Tidak ada access control**: device registry belum membedakan privilege atau capability.
7. **Tidak ada metadata integrity**: checksum block, journal, fsck, dan recovery belum ada.

Mitigasi M14:

1. Semua operasi public memvalidasi null pointer, count, LBA range, dan block size.
2. Host unit test mencakup negative test boundary.
3. Dirty buffer hanya ditulis ke device melalui flush eksplisit atau eviction.
4. Artifact audit memastikan object freestanding dapat diperiksa.
5. Dokumen readiness melarang klaim persistence, driver hardware, atau crash safety.

---

## 24. Verification Matrix

| Requirement | Evidence | Metode | Pass/Fail |
|---|---|---|---|
| R14.1 Block device API tersedia | `include/mcsos/block.h` | Source review | Diisi mahasiswa |
| R14.2 Registry menolak device invalid | Host unit test | `make host-test` | Diisi mahasiswa |
| R14.3 RAM block driver read/write benar | Host unit test | Pattern write/read | Diisi mahasiswa |
| R14.4 Boundary out-of-range ditolak | Host unit test | Negative test LBA/count | Diisi mahasiswa |
| R14.5 Buffer cache write-back bekerja | Host unit test | Write-cache-read-flush-read | Diisi mahasiswa |
| R14.6 Object freestanding terbentuk | `build/*.o` | `make freestanding` | Diisi mahasiswa |
| R14.7 Undefined symbol kosong | `m14_nm_undefined.txt` | `nm -u build/m14_block_layer.o` | Diisi mahasiswa |
| R14.8 ELF64 x86-64 tervalidasi | `m14_readelf_block.txt` | `readelf -h` | Diisi mahasiswa |
| R14.9 Disassembly tersedia | `m14_objdump_block.txt` | `objdump -dr` | Diisi mahasiswa |
| R14.10 QEMU boot tidak regresi | `qemu_m14.log` | QEMU smoke test | Diisi mahasiswa |
| R14.11 Perubahan terkomit | `git log` | Git evidence | Diisi mahasiswa |

---

## 25. Kriteria Lulus Praktikum

Minimum lulus M14:

1. Repository dapat dibangun dari clean checkout.
2. `./scripts/m14_preflight.sh` selesai dan log tersimpan.
3. `make all` selesai tanpa error pada host WSL 2 mahasiswa.
4. Host unit test menampilkan `M14 host tests PASS`.
5. Object freestanding x86_64 untuk `block.c`, `ramblk.c`, dan `bcache.c` terbentuk.
6. Linked relocatable object `build/m14_block_layer.o` tidak memiliki undefined symbol berdasarkan `nm -u`.
7. `readelf` membuktikan object adalah ELF64 x86-64 relocatable.
8. `objdump` tersimpan untuk audit instruksi dan relocation.
9. QEMU smoke test tidak mengalami regresi boot dari M13.
10. Log serial, checksum, dan status Git tersimpan di `artifacts/m14`.
11. Mahasiswa menjelaskan invariant, ownership, failure mode, rollback, dan batasan M14.
12. Laporan memakai template laporan praktikum seragam.

Kriteria tambahan untuk nilai tinggi:

1. Menambahkan statistik cache hit/miss dan write/flush counter.
2. Menambahkan negative tests yang lebih banyak.
3. Menunjukkan GDB breakpoint pada fungsi block layer.
4. Menyusun ADR singkat yang menjelaskan mengapa M14 memilih RAM block driver sebelum virtio-blk/NVMe.
5. Menghubungkan desain M14 dengan rencana filesystem persistent M15+.

---

## 26. Rubrik Penilaian 100 Poin

| Komponen | Poin | Kriteria |
|---|---:|---|
| Kebenaran fungsional | 30 | Block API, RAM block driver, buffer cache, host test, dan freestanding object berjalan sesuai kontrak. |
| Kualitas desain dan invariant | 20 | Invariant LBA, block size, dirty entry, ownership, dan batas concurrency ditulis jelas. |
| Pengujian dan bukti | 20 | Preflight, host test, `nm`, `readelf`, `objdump`, checksum, QEMU log, dan Git evidence lengkap. |
| Debugging/failure analysis | 10 | Failure mode dan diagnosis spesifik; ada rollback plan. |
| Keamanan dan robustness | 10 | Validasi argumen, batas trust, non-goals, dan risiko crash/persistence ditulis eksplisit. |
| Dokumentasi/laporan | 10 | Laporan mengikuti template, rapi, reproducible, dan referensi IEEE tersedia. |

---

## 27. Template Laporan Praktikum M14

Gunakan struktur berikut pada laporan mahasiswa.

### 27.1 Sampul

- Judul: Laporan Praktikum M14 - Block Device Layer, RAM Block Driver, dan Buffer Cache Minimal pada MCSOS
- Nama mahasiswa:
- NIM:
- Kelas:
- Mode pengerjaan: Individu / Kelompok
- Anggota kelompok jika ada:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi Pendidikan Teknologi Informasi
- Institut Pendidikan Indonesia

### 27.2 Tujuan

Tuliskan tujuan teknis dan konseptual M14.

### 27.3 Dasar Teori Ringkas

Jelaskan block device, LBA, buffer cache, dirty flag, flush, dan freestanding build.

### 27.4 Lingkungan

Isi tabel berikut:

| Item | Nilai |
|---|---|
| Host OS | |
| WSL distro | |
| Clang version | |
| GNU ld version | |
| Binutils version | |
| QEMU version | |
| GDB version | |
| Commit hash awal | |
| Commit hash akhir | |

### 27.5 Desain

Sertakan diagram arsitektur, struktur data, invariant, ownership, dan batasan.

### 27.6 Langkah Kerja

Tulis perintah yang dijalankan, file yang diubah, dan alasan teknis tiap perubahan.

### 27.7 Hasil Uji

Sertakan tabel:

| Uji | Perintah | Output ringkas | Pass/Fail |
|---|---|---|---|
| Preflight | `./scripts/m14_preflight.sh` | | |
| Host test | `make host-test` | | |
| Freestanding compile | `make freestanding` | | |
| Audit undefined symbol | `make audit` | | |
| QEMU smoke | `qemu-system-x86_64 ...` | | |

### 27.8 Analisis

Bahas apa yang berhasil, bug yang ditemukan, penyebab, dan perbaikan.

### 27.9 Keamanan dan Reliability

Bahas risiko null pointer, out-of-range LBA, stale cache, dirty buffer loss, concurrency, dan non-persistence.

### 27.10 Kesimpulan

Nyatakan status readiness dengan istilah yang benar: siap uji QEMU untuk block layer awal, bukan siap produksi.

### 27.11 Lampiran

Lampirkan diff ringkas, log penuh, checksum, `readelf`, `objdump`, dan screenshot bila ada.

---

## 28. Readiness Review

| Area | Status M14 | Catatan |
|---|---|---|
| Build reproducibility | Kandidat siap uji | Perlu clean checkout verification di mesin mahasiswa. |
| Host unit test | Siap uji | Source panduan telah diuji lokal; mahasiswa wajib mengulang. |
| Freestanding object | Siap audit | ELF64 x86-64 relocatable dapat dibuat. |
| QEMU runtime | Siap smoke test | Runtime tetap bergantung pada integrasi repository M2-M13. |
| Storage persistence | Belum siap | RAM block driver volatil. |
| Hardware driver | Belum siap | Tidak ada PCI/IDE/AHCI/NVMe/virtio-blk. |
| Crash consistency | Belum siap | Dirty buffer dapat hilang sebelum flush. |
| SMP safety | Belum siap | Belum ada locking internal buffer cache. |
| Security boundary | Belum siap | Belum ada capability, usercopy hardening, atau DMA isolation. |
| Dokumentasi | Siap digunakan | Panduan dan template laporan tersedia. |

**Keputusan readiness M14**: hasil M14 hanya dapat diberi label **siap uji QEMU untuk block device layer dan buffer cache minimal** apabila semua checkpoint M14 lulus pada mesin mahasiswa. M14 tidak boleh diberi label siap produksi, siap filesystem persistent, atau aman terhadap crash/power-loss.

---

## References

[1] Linux Kernel Documentation, “Block,” *The Linux Kernel documentation*. Available: https://docs.kernel.org/block/index.html. Accessed: 2026-05-03.

[2] Linux Kernel Documentation, “Multi-Queue Block IO Queueing Mechanism (blk-mq),” *The Linux Kernel documentation*. Available: https://docs.kernel.org/block/blk-mq.html. Accessed: 2026-05-03.

[3] Linux Kernel Documentation, “Null block device driver,” *The Linux Kernel documentation*. Available: https://www.kernel.org/doc/html/v5.15/block/null_blk.html. Accessed: 2026-05-03.

[4] QEMU Project, “Invocation,” *QEMU documentation*. Available: https://www.qemu.org/docs/master/system/invocation.html. Accessed: 2026-05-03.

[5] QEMU Project, “GDB usage,” *QEMU documentation*. Available: https://www.qemu.org/docs/master/system/gdb.html. Accessed: 2026-05-03.

[6] LLVM Project, “Clang command line argument reference,” *Clang documentation*. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html. Accessed: 2026-05-03.

[7] GNU Project, “GNU Binary Utilities,” *GNU Binutils documentation*. Available: https://www.sourceware.org/binutils/docs/binutils.html. Accessed: 2026-05-03.
