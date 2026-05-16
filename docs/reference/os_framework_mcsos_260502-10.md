# os_framework_mcsos_260502-10.md

# Kerangka Pengembangan Sistem Operasi MCSOS Versi 260502-10

**Status dokumen:** kerangka arsitektur, roadmap, dan readiness gate untuk pengembangan sistem operasi pendidikan baru.  
**Nama sistem:** MCSOS 260502.  
**Target arsitektur:** x86_64 long mode.  
**Lingkungan pengembangan utama:** Windows 11 x64 dengan WSL 2 Linux environment.  
**Model kernel awal:** monolithic educational kernel dengan batas modul internal ketat dan capability-inspired handle model.  
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi:** Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.  
**Baseline dokumen:** 2026-05-02.  
**Format:** Markdown operasional.

Dokumen ini adalah kerangka hasil penyempurnaan dari beberapa kerangka pengembangan OS sebelumnya. Fokus penyempurnaannya adalah konsistensi target, readiness berbasis bukti, pengembangan bertahap, batas subsystem eksplisit, keamanan sejak awal, dan kemampuan adaptif untuk memaksimalkan perangkat keras secara aman. Keberhasilan boot di QEMU hanya boleh disebut **siap uji QEMU untuk milestone terkait**, bukan bukti bahwa sistem bebas cacat, siap produksi, atau siap dipakai luas.

---

## 1. Ringkasan Analisis Kerangka Sebelumnya

### 1.1 Kekuatan yang dipertahankan

1. Kerangka sebelumnya telah mencakup domain OS yang luas: boot, kernel, memory management, interrupt, scheduler, filesystem, driver, networking, security, graphics, virtualization, toolchain, observability, dan enterprise features.
2. Beberapa dokumen telah menempatkan x86_64, Windows 11 x64, QEMU, GDB, UEFI/OVMF, Limine, ELF64, higher-half kernel, PMM, VMM, syscall ABI, VFS, dan userspace sebagai fondasi teknis utama.
3. Dokumen terbaik sebelumnya telah memperkenalkan pendekatan evidence-first: setiap fase harus menghasilkan artefak verifikabel seperti image, serial log, kernel map, disassembly, test report, trace, screenshot, dan readiness report.
4. Kerangka kompetensi pengembang telah mengidentifikasi kebutuhan lintas-domain: fondasi ilmu komputer, low-level programming, arsitektur hardware, kernel, filesystem, networking, keamanan, driver, virtualisasi, boot, grafis, toolchain, enterprise, dan ilmu pendukung lintas disiplin.
5. RPS Sistem Operasi menegaskan bahwa desain MCSOS harus tetap relevan untuk pembelajaran proses, thread, scheduling, sinkronisasi, memory management, filesystem, I/O, keamanan, virtualisasi, debugging kernel panic, race condition, dan deadlock.

### 1.2 Kelemahan yang diperbaiki

1. **Scope terlalu cepat melebar.** Kerangka baru memindahkan enterprise-grade features, full POSIX, advanced TCP, virtualization, native GPU, dan update/rollback ke fase lanjut setelah boot, memory, syscall, VFS, security, dan observability stabil.
2. **Bootloader custom terlalu dini.** Jalur utama distandarkan pada Limine + UEFI/OVMF. UEFI loader buatan sendiri menjadi jalur pengayaan, bukan baseline awal.
3. **Lingkungan Windows tidak konsisten.** Kerangka baru menetapkan WSL 2 sebagai environment build utama. MSYS2 hanya fallback untuk tooling Windows-native yang benar-benar diperlukan.
4. **Readiness belum cukup ketat.** Setiap milestone kini memiliki gate, bukti minimal, failure modes, dan rollback.
5. **Adaptasi hardware belum dibatasi.** Fitur otonom/adaptif hanya boleh mengubah kebijakan kernel yang aman, terukur, dapat diobservasi, dan dapat di-rollback. Tidak ada overclocking otomatis, tidak ada akses register tidak terdokumentasi, dan tidak ada asumsi fitur CPU tanpa CPUID/ACPI/PCI evidence.
6. **Kode panjang tanpa kontrak.** Dokumen ini tidak memberikan implementasi final penuh untuk jalur berisiko tinggi. Jalur boot, interrupt, syscall, paging, context switch, filesystem recovery, DMA, dan crypto harus dimulai dari kontrak, invariants, dan checkpoint kecil.

### 1.3 Keputusan sintesis

MCSOS 260502-10 menggunakan pendekatan **small trusted core, buildable-first, evidence-first, security-from-phase-0, deterministic-QEMU-first, hardware-after-observability**. Sistem dirancang sebagai kernel pendidikan yang benar-benar baru dari sisi kernel, ABI internal, runtime awal, userspace awal, model keamanan, driver, filesystem, networking stack, dokumentasi, dan readiness process. Bootloader, compiler, emulator, debugger, firmware referensi, dan spesifikasi publik boleh digunakan sebagai alat bantu, tetapi bukan artefak OS utama.

---

## 2. Asumsi, Target, dan Non-goals

### 2.1 Asumsi utama

| Area | Keputusan baseline |
|---|---|
| Arsitektur | x86_64 long mode, 4-level paging baseline, persiapan konseptual untuk 5-level paging tetapi tidak diaktifkan pada fase awal |
| Firmware | UEFI melalui OVMF pada QEMU sebagai jalur utama; BIOS legacy hanya studi atau kompatibilitas riset |
| Bootloader | Limine Boot Protocol sebagai baseline; UEFI loader custom sebagai pengayaan |
| Host | Windows 11 x64 |
| Build environment | WSL 2 dengan Ubuntu LTS atau Debian stable, repository di filesystem Linux WSL |
| Emulator | QEMU system-x86_64, konfigurasi deterministik sebagai standar praktikum dan CI-local |
| Debugger | GDB/gdb-multiarch melalui QEMU gdbstub, serial log sebagai jalur bukti minimum |
| Bahasa kernel | Freestanding C17, assembly x86_64 minimal untuk entry, interrupt/trap, syscall, context switch, MSR, port I/O, dan CPU instruction khusus |
| Toolchain | Clang/LLD atau GCC/binutils yang dipin dalam toolchain BOM; target freestanding x86_64 ELF |
| Kernel model | Monolithic educational kernel dengan subsystem boundary ketat dan capability-inspired handle model |
| Compatibility | POSIX-like subset bertahap; tidak menargetkan ABI Linux penuh |
| Readiness awal | Siap uji QEMU per milestone; hardware terbatas setelah observability dan rollback tersedia |

### 2.2 Non-goals fase awal

1. Tidak menargetkan driver Linux compatibility.
2. Tidak menargetkan ABI Linux penuh.
3. Tidak menargetkan boot di semua motherboard x86_64.
4. Tidak menargetkan desktop lengkap sebelum syscall ABI, ELF loader, VFS, keamanan, input, framebuffer, dan observability stabil.
5. Tidak menargetkan production-ready.
6. Tidak mengaktifkan SMP, DMA kompleks, native GPU, secure update, atau filesystem persisten sebelum invariants dasar dan fault path terbukti.
7. Tidak mengklaim crash consistency tanpa crash injection, fsck/scrub, dan block-ordering evidence.
8. Tidak mengklaim secure boot chain tanpa signing, verification, key lifecycle, rollback, dan recovery evidence.

### 2.3 Persona penggunaan

| Persona | Kebutuhan utama | Implikasi desain |
|---|---|---|
| Dosen dan asisten praktikum | Modul bertahap, rubrik, bukti uji, failure modes | Setiap milestone punya artefak dan kriteria lulus |
| Mahasiswa pengembang | Instruksi operasional, checkpoint kecil, kontrak antarmuka | Tidak ada tugas besar tanpa precondition dan test obligation |
| Reviewer teknis | Invariants, acceptance criteria, traceability, risk register | Gate review wajib sebelum promosi milestone |
| Peneliti OS | Ruang eksperimen untuk scheduler, VM, FS, security, networking | Eksperimen dipisahkan dari baseline stabil |

---

## 3. Prinsip Arsitektur MCSOS 260502-10

1. **Buildable-first.** Setiap fase harus dapat dibangun dari clean checkout.
2. **Evidence-first.** Fitur selesai hanya jika ada bukti yang dapat diperiksa.
3. **Small trusted core.** Kernel awal harus kecil, observable, deterministik, dan mudah di-debug.
4. **Explicit contracts.** Boot handoff, ABI, allocator, driver, VFS, packet buffer, dan syscall harus memiliki kontrak tertulis.
5. **Fail closed.** Error path lebih baik menghasilkan panic yang terbaca daripada silent corruption.
6. **No hidden runtime.** Kernel tidak bergantung pada hosted libc, startup object, exception runtime, RTTI, unwinding, FPU/SIMD, atau stack protector runtime yang belum disediakan.
7. **Deterministic QEMU first.** Baseline diuji di QEMU headless dengan serial log sebelum fitur grafis atau hardware nyata.
8. **Hardware after observability.** Bring-up perangkat keras hanya setelah serial log, panic path, GDB recipe, crash artifact, dan rollback tersedia.
9. **Security from phase 0.** Threat model, user/kernel isolation, W^X, usercopy, capability policy, audit event, dan negative tests dirancang sejak awal.
10. **Documentation is part of build.** Dokumen kontrak dan readiness berubah bersama kode, bukan setelahnya.
11. **Adaptation with rollback.** Kebijakan adaptif hanya sah jika observasi, keputusan, aksi, verifikasi, dan rollback tercatat.
12. **No undocumented hardware control.** Semua optimasi hardware harus berdasarkan CPUID, ACPI, PCI capability, MSR terdokumentasi, datasheet, atau spesifikasi resmi.

---

## 4. Peta Skill dan Transfer ke Artefak MCSOS

| Domain | Transfer ke MCSOS | Artefak wajib |
|---|---|---|
| osdev-general | roadmap, phase gate, release gate, readiness review | `docs/roadmap.md`, `docs/readiness/*.md` |
| osdev-01-computer-foundation | state machine, invariants, algoritma, concurrency reasoning | `docs/architecture/invariants.md`, model state per subsystem |
| osdev-02-low-level-programming | freestanding C, assembly, ABI, linker, ELF, undefined behavior | `arch/x86_64/linker.ld`, `kernel.map`, `objdump.txt`, `docs/abi/*.md` |
| osdev-03-computer-and-hardware-architecture | MMU, TLB, APIC, ACPI, PCIe, timer, SMP, DMA | CPUID dump, ACPI dump, PCI dump, `docs/arch/x86_64/*.md` |
| osdev-04-kernel-development | trap, syscall, scheduler, VM, IPC, panic, observability | kernel ADR, trap log, syscall ABI tests |
| osdev-05-filesystem-development | VFS, inode, FD, ramfs, mcsfs, crash consistency | FS spec, fsck plan, crash test logs |
| osdev-06-networking-stack | Ethernet, ARP, IPv4, ICMP, UDP, TCP subset, socket ABI | pcap tests, socket ABI, packet fuzz corpus |
| osdev-07-os-security | threat model, user/kernel isolation, capabilities, fuzzing | `docs/security/threat_model.md`, negative tests, audit schema |
| osdev-08-device-driver-development | driver model, PCI, serial, framebuffer, virtio, block, NIC | driver lifecycle spec, DMA ownership tests |
| osdev-09-virtualization-and-containerization | virtio, namespaces, cgroups-lite, sandbox, escape testing | isolation tests, resource accounting tests |
| osdev-10-boot-firmware | UEFI, Limine, OVMF, memory map, ACPI, boot handoff | boot handoff contract, serial boot log |
| osdev-11-graphics-display | framebuffer console, modeset plan, compositor boundary | visual regression, framebuffer logs |
| osdev-12-toolchain-devenv | Windows 11, WSL 2, LLVM/GCC, QEMU, GDB, CI | toolchain BOM, build metadata, CI logs |
| osdev-13-enterprise-features | update/rollback, observability, tracing, monitoring | release runbook, telemetry schema, rollback drill |
| osdev-14-cross-science | requirements, verification matrix, reliability, statistics | traceability matrix, risk register, experiment plan |

---

## 5. Arsitektur Tingkat Tinggi

### 5.1 Model kernel

MCSOS memakai **monolithic educational kernel** dengan boundary modular internal. Scheduler, memory manager, interrupt handling, VFS, driver awal, dan networking awal berjalan di kernel space untuk menyederhanakan bootstrap. Namun authority tidak boleh disebarkan lewat pointer mentah atau akses global tanpa kontrol. Semua resource jangka panjang harus direpresentasikan melalui object registry, reference count, ownership tunggal, atau capability-inspired handle.

Konsekuensi desain:

1. Kernel memiliki satu address space global untuk kernel dan address space user per proses.
2. Syscall adalah satu-satunya jalur standar dari user ke kernel.
3. Semua user pointer harus melewati `copyin`, `copyout`, atau API usercopy setara.
4. Driver tidak boleh mengakses struktur internal scheduler, VM, VFS, atau security tanpa API subsystem.
5. Semua objek kernel memiliki lifecycle terdokumentasi: create, reference, transfer, close, destroy.
6. Panic path tersedia sebelum allocator kompleks, scheduler, interrupt eksternal, atau driver kompleks diaktifkan.

### 5.2 Lapisan arsitektur

```text
+---------------------------------------------------------------+
| Userspace: init, shell, libc_min, test utilities, services     |
+---------------------------------------------------------------+
| ABI: syscall, ELF loader, FD table, signal/exit/wait subset    |
+---------------------------------------------------------------+
| Kernel executive: VFS, IPC, network, security, driver core     |
+---------------------------------------------------------------+
| Kernel core: panic, log, scheduler, VM, PMM, heap, sync        |
+---------------------------------------------------------------+
| x86_64 arch: GDT, IDT, TSS, APIC, paging, MSR, syscall entry   |
+---------------------------------------------------------------+
| Boot/Firmware: Limine, UEFI/OVMF, ACPI, memory map, initrd     |
+---------------------------------------------------------------+
| Hardware/Emulator: QEMU q35, CPU, RAM, PCIe, storage, NIC      |
+---------------------------------------------------------------+
```

### 5.3 Trust zones

| Zone | Isi | Boundary wajib |
|---|---|---|
| Boot trust base | firmware, bootloader, kernel image, initrd | signature/hash plan pada fase security; handoff validation sejak M2 |
| Kernel core | trap, panic, PMM, VMM, scheduler | tidak menerima input user langsung tanpa syscall/usercopy |
| Kernel executive | VFS, IPC, networking, driver core | capability check, refcount, lock order |
| Driver boundary | PCI, virtio, serial, framebuffer, block, NIC | MMIO accessor, DMA ownership, IRQ ownership |
| Userspace service | init, shell, test utilities, future daemon | syscall ABI, FD, capability handles |
| Adaptive policy | scheduler/NUMA/IRQ/storage/network/power policy | observe-decide-act-verify-rollback, audit log |

---

## 5A. Architecture and Design Summary

Bagian ini menandai secara eksplisit bahwa arsitektur dan design MCSOS 260502-10 memakai decomposisi berlapis: boot/firmware, arch x86_64, kernel core, kernel executive, driver boundary, userspace ABI, observability, dan adaptive policy. Semua design decision harus memiliki ADR, invariants, test obligation, failure mode, dan readiness gate sebelum dipromosikan ke milestone berikutnya.

## 6. Hardware Performance Adaptation Model

MCSOS 260502 menargetkan pemanfaatan perangkat keras secara otonom dan adaptif, tetapi mekanisme adaptif tidak boleh mengorbankan correctness, keamanan, atau kemampuan rollback. Adaptasi bukan auto-overclocking dan bukan akses register tidak terdokumentasi.

### 6.1 Siklus adaptasi

```text
observe -> classify -> decide -> act -> verify -> rollback-or-commit
```

| Fase | Tindakan | Evidence |
|---|---|---|
| Observe | baca CPUID, ACPI, PCI config, timer, counters, queue depth, error counters | `cpuid.log`, `acpi.log`, `pci.log`, tracepoint |
| Classify | identifikasi topology CPU, timer, memory, device, IRQ, queue | topology report |
| Decide | pilih policy konservatif berbasis konfigurasi dan telemetry | policy decision log |
| Act | ubah affinity, queue depth, allocator policy, timer mode, power hint | audit event |
| Verify | bandingkan latency, throughput, error, thermal, hang, drops | metric snapshot |
| Rollback | kembali ke policy aman jika threshold gagal | rollback log |

### 6.2 Ruang adaptasi yang diperbolehkan

1. Scheduler: CPU affinity, load balancing, preemption quantum, idle thread policy.
2. Memory: per-CPU page cache, NUMA-aware allocation setelah topology valid, huge page hanya bila fragmentation dan permission aman.
3. Interrupt: IRQ affinity dan interrupt moderation untuk NIC/block driver setelah APIC/MSI stabil.
4. Storage: request queue depth, read-ahead sederhana, writeback mode konservatif, flush/barrier tidak boleh dihilangkan.
5. Networking: RX/TX ring budget, packet queue backpressure, checksum policy, retransmission timer tuning berbasis test.
6. Power/thermal: hanya memakai ACPI/firmware interface yang terdokumentasi; tidak ada perubahan voltage/clock manual pada fase awal.
7. Graphics: framebuffer update batching dan dirty rectangle pada fase GUI, bukan native GPU scheduler awal.

### 6.3 Guardrail adaptasi

1. Semua policy adaptif harus dapat dinonaktifkan dengan kernel command line `mcsos.adapt=off`.
2. Semua keputusan adaptif harus mengeluarkan trace event ringkas.
3. Tidak ada adaptasi yang boleh mengubah page permission, DMA mapping, filesystem ordering, atau security policy tanpa gate khusus.
4. Jika metric error naik, timeout meningkat, atau panic counter bertambah, policy harus rollback ke mode konservatif.
5. Baseline praktikum menggunakan mode konservatif deterministik; mode adaptif menjadi pengayaan setelah M13.

---

## 7. Lingkungan Pengembangan Windows 11 x64

### 7.1 Strategi host

Windows 11 x64 digunakan sebagai host administratif, editor, terminal, dan manajemen file. Build kernel, toolchain target, QEMU test, GDB session, lint, dan CI-local rehearsal dilakukan di WSL 2. Repository diletakkan di filesystem Linux WSL, misalnya `~/src/mcsos`, bukan `/mnt/c/...`, untuk menghindari masalah permission bit, executable bit, case sensitivity, path handling, newline conversion, dan performa I/O.

### 7.2 Bootstrap WSL dari PowerShell

Perintah ini dijalankan dari PowerShell Administrator untuk memasang WSL dan memastikan distribusi memakai WSL 2.

```powershell
wsl --install
wsl --list --verbose
wsl --set-default-version 2
wsl --list --online
wsl --install -d Ubuntu
```

### 7.3 Paket dasar di WSL

Perintah ini memasang compiler, linker, assembler, emulator, debugger, tool image, static analysis, dan utilitas audit ELF.

```bash
sudo apt update
sudo apt install -y \
  build-essential git make cmake ninja-build pkg-config \
  clang lld llvm binutils nasm \
  qemu-system-x86 qemu-utils ovmf \
  gdb gdb-multiarch xorriso mtools dosfstools \
  python3 python3-pip python3-venv \
  shellcheck cppcheck clang-tidy
```

### 7.4 Toolchain BOM minimum

| Tool | Peran | Evidence wajib |
|---|---|---|
| Clang atau GCC cross-capable | kompilasi freestanding C | `clang --version` atau `x86_64-elf-gcc --version`, object inspection |
| LLD atau GNU ld | link ELF kernel | `readelf -lW`, `kernel.map` |
| NASM atau GNU as | assembly x86_64 | `objdump -drwC` pada entry path |
| QEMU system-x86_64 | emulator dan fault injection awal | QEMU command log, serial log |
| OVMF | firmware UEFI | path firmware, boot log |
| GDB/gdb-multiarch | debug kernel via gdbstub | `.gdbinit`, breakpoint proof |
| xorriso/mtools/dosfstools | ISO/ESP/disk image | SHA-256 image |
| Python | scripts, lint, harness | virtualenv/lockfile jika dipakai |
| Git | traceability | commit hash di banner dan laporan |

### 7.5 Metadata versi toolchain

Perintah ini mencatat metadata build untuk audit dan reproducibility.

```bash
mkdir -p build/meta
{
  date -u +"date_utc=%Y-%m-%dT%H:%M:%SZ"
  uname -a
  clang --version | head -n 1 || true
  ld.lld --version | head -n 1 || true
  nasm -v || true
  qemu-system-x86_64 --version | head -n 1 || true
  gdb --version | head -n 1 || true
  git --version
  git rev-parse --short HEAD 2>/dev/null || true
} | tee build/meta/toolchain-versions.txt
```

### 7.6 Flag kompilasi baseline

Baseline freestanding C untuk kernel:

```text
-std=gnu17
-ffreestanding
-fno-builtin
-nostdlib
-fno-stack-protector        # sampai runtime stack protector tersedia
-mno-red-zone
-mno-mmx
-mno-sse
-mno-sse2                  # sampai FPU/SIMD save/restore selesai
-Wall
-Wextra
-Werror
-O2                        # release milestone; -Og untuk debug milestone
-g3
```

Untuk GCC higher-half kernel yang memakai negative address range, `-mcmodel=kernel` dapat digunakan. Jika layout MCSOS tidak cocok dengan model tersebut, gunakan model yang terdokumentasi dalam ADR dan verifikasi melalui `readelf`, `objdump`, dan `kernel.map`. Clang harus diberi target eksplisit, misalnya `--target=x86_64-unknown-elf`, agar tidak menghasilkan object untuk host.

---

## 8. Struktur Repository

```text
mcsos/
  README.md
  LICENSE
  Makefile
  toolchain.lock
  docs/
    adr/
    architecture/
      overview.md
      invariants.md
      vm_layout.md
      subsystem_boundaries.md
    practicum/
    readiness/
      gates.md
    security/
      threat_model.md
    testing/
      test_matrix.md
    reports/
  tools/
    image/
    lint/
    qemu/
    scripts/
  configs/
    qemu/
      x86_64-uefi.conf.md
    limine/
      limine.conf
    ci/
  boot/
    limine/
    efi/
  arch/
    x86_64/
      boot/
      cpu/
      interrupts/
      mm/
      smp/
      include/
  include/
    mcsos/
      abi/
      kernel/
      drivers/
      fs/
      net/
      security/
  kernel/
    core/
    mm/
    sched/
    ipc/
    syscall/
    security/
    observability/
  drivers/
    core/
    serial/
    framebuffer/
    pci/
    virtio/
    block/
    net/
  fs/
    vfs/
    ramfs/
    mcsfs/
    fsck/
  net/
    core/
    ethernet/
    arp/
    ipv4/
    icmp/
    udp/
    tcp/
    socket/
  userspace/
    crt/
    libc_min/
    init/
    shell/
    tests/
  tests/
    unit/
    integration/
    qemu/
    fuzz/
    fault/
  ci/
  build/                    # generated, tidak dikomit
```

File minimum repository:

| File | Isi minimum |
|---|---|
| `README.md` | target, host, cara build, cara test, status readiness |
| `toolchain.lock` | versi toolchain, emulator, firmware, hash artefak penting |
| `docs/architecture/overview.md` | arsitektur ringkas dan dependency graph |
| `docs/architecture/invariants.md` | invariants lintas subsystem |
| `docs/security/threat_model.md` | threat model awal |
| `docs/testing/test_matrix.md` | matriks test dan evidence |
| `docs/readiness/gates.md` | gate M0 sampai M16 |
| `Makefile` | target `meta`, `check`, `build`, `run`, `debug`, `test`, `clean`, `distclean` |
| `configs/qemu/x86_64-uefi.conf.md` | QEMU command canonical |
| `arch/x86_64/linker.ld` | layout kernel dan symbol boundary |

---

## 9. Boot dan Firmware Contract

### 9.1 Jalur boot primer

1. QEMU q35 memuat OVMF.
2. OVMF memuat Limine dari EFI System Partition.
3. Limine memuat `kernel.elf`, initrd, dan konfigurasi.
4. Limine menyerahkan boot information ke entry kernel.
5. Kernel memvalidasi boot information sebelum menggunakan memory map, framebuffer, RSDP, command line, dan modules.
6. Kernel mengaktifkan early serial log dan panic path.
7. Kernel masuk ke `kernel_main` tanpa heap allocation sebelum PMM siap.

### 9.2 Data handoff wajib

| Data | Validasi |
|---|---|
| Memory map | entry count, length, alignment, type, overlap, bounds |
| Kernel physical/virtual range | tidak overlap dengan usable free frame |
| Initrd/module | address, size, alignment, bounds, lifetime |
| RSDP/ACPI pointer | checksum, signature, revision, bounds |
| Framebuffer | base, width, height, pitch, format, memory type |
| Command line | length limit, UTF-8/ASCII policy, option parser bounds |
| CPU state | long mode, interrupt state, stack alignment, CR3 policy |

### 9.3 Larangan boot phase

1. Tidak memakai UEFI Boot Services setelah firmware handoff final.
2. Tidak mempercayai memory map tanpa bounds check.
3. Tidak memakai boot data setelah allocator umum mengambil alih kecuali data sudah disalin atau dipin.
4. Tidak mengaktifkan interrupt sebelum IDT minimal dan panic path siap.
5. Tidak mengaktifkan SMP sebelum per-CPU data, stack, TSS, AP startup, dan TLB shootdown protocol diuji.

---

## 10. ABI, Calling Convention, dan Low-level Boundary

### 10.1 ABI internal kernel

1. Fungsi C internal mengikuti x86_64 System V ABI kecuali entry assembly khusus mendokumentasikan penyimpangan.
2. Stack alignment sebelum call C harus 16-byte aligned sesuai kontrak ABI yang dipilih.
3. Kernel dikompilasi dengan red zone nonaktif.
4. Floating point, MMX, SSE, AVX, dan extended state tidak boleh dipakai sebelum mekanisme save/restore FPU selesai.
5. Interrupt entry menyimpan register sesuai `struct mcsos_trap_frame`.
6. Semua assembly boundary harus mendokumentasikan clobber, register preserved, interrupt state, stack use, dan return path.

### 10.2 Syscall ABI versi 0

| Register | Fungsi |
|---|---|
| `rax` | nomor syscall saat masuk, return value saat keluar |
| `rdi` | argumen 1 |
| `rsi` | argumen 2 |
| `rdx` | argumen 3 |
| `r10` | argumen 4 |
| `r8` | argumen 5 |
| `r9` | argumen 6 |

Kontrak:

1. Error internal dikembalikan sebagai nilai negatif terstandar.
2. Nomor syscall, pointer, length, flags, dan capability harus divalidasi.
3. User pointer tidak boleh di-dereference langsung.
4. ABI version tag ditempatkan di `include/mcsos/abi/syscall.h`.
5. Breaking change menaikkan ABI version.

---

## 11. Memory Layout dan Invariants

### 11.1 Layout konseptual

Definisi final wajib berada di `arch/x86_64/include/mcsos/arch/vm_layout.h` dan diverifikasi melalui dump page table.

| Region | Tujuan | Catatan |
|---|---|---|
| Low physical memory | firmware remnants, boot data, AP trampoline terbatas | tidak dipakai bebas sebelum memory map valid |
| HHDM/direct map | mapping physical memory ke kernel virtual | kernel only, permission minimal |
| Kernel text | kode kernel | RX, tidak writable |
| Kernel rodata | constant data | R, NX |
| Kernel data/bss | data kernel | RW, NX |
| Kernel heap | slab/general heap | guard dan poison pada debug config |
| Per-CPU area | data per CPU | setelah topology ditemukan |
| Kernel stacks | stack kernel per thread/CPU | guard page pada debug config |
| User lower-half | program userspace | tidak boleh map supervisor page |
| MMIO windows | perangkat | cache attribute sesuai device, akses via MMIO API |

### 11.2 Invariants memory

1. Setiap physical frame memiliki tepat satu state: `free`, `reserved`, `kernel`, `user`, `page_table`, `dma_pinned`, `mmio`, atau `bad`.
2. Frame tidak boleh berada pada free list jika masih dimap, dipin DMA, atau direferensi page table.
3. Kernel text tidak boleh writable.
4. Halaman W+X dilarang pada release config.
5. Mapping user tidak boleh mengandung supervisor-only assumption.
6. Semua perubahan PTE yang mengubah permission atau physical target wajib diikuti TLB invalidation.
7. Pada SMP, TLB shootdown harus memiliki acknowledgment atau fallback panic yang terbaca.

---

## 12. Subsystem Boundary

| Subsystem | Tanggung jawab | Tidak boleh melakukan | Evidence minimal |
|---|---|---|---|
| Boot | validasi handoff, entry, stack, early log | memanggil firmware service setelah handoff final | boot log, handoff report |
| Arch x86_64 | GDT, IDT, TSS, paging, APIC, MSR | menyembunyikan asumsi fitur tanpa CPUID | CPUID dump, trap test |
| Kernel core | panic, log, init sequence, object registry | akses register driver langsung | panic test, stage log |
| PMM | frame ownership | free frame yang masih dimap/dipin | allocator invariant test |
| VMM | page table, VMA, permission, usercopy | mengizinkan W+X tanpa gate | page table dump |
| Scheduler | thread state, runqueue, preemption | sleep di interrupt hard path | scheduler stress log |
| Sync | spinlock, mutex, rwlock, wait queue | memegang spinlock saat blocking | lock-order report |
| Syscall | ABI, dispatch, validation | dereference user pointer langsung | syscall fuzz log |
| IPC | message, pipe, shared memory | bypass capability check | IPC negative test |
| VFS | path, inode, file, FD table | direct block write bypass | VFS refcount test |
| FS | on-disk format, recovery, fsck hooks | klaim crash safe tanpa crash tests | fsck, crash injection |
| Block | queue, cache, flush, barrier | mengabaikan write ordering | block trace |
| Network | packet buffer, protocol, socket | trust packet length | pcap/fuzz report |
| Driver core | probe/remove, IRQ, DMA, MMIO | bind sebelum resource valid | driver lifecycle log |
| Security | credential, policy, audit | menjadi add-on tanpa hook | threat model, negative tests |
| Graphics | framebuffer, console, display model | menghapus serial fallback | visual regression |
| Observability | logs, trace, metrics, dump | membocorkan secret/pointer non-debug | telemetry schema |

---

## 13. Roadmap Milestone M0-M16

### M0 - Requirements, governance, dan baseline arsitektur

**Tujuan:** menetapkan batas sistem, stakeholder, non-goals, risiko, dan evidence requirement.  
**Deliverables:** `overview.md`, `invariants.md`, `threat_model.md`, `test_matrix.md`, `gates.md`, risk register, verification matrix.  
**Evidence:** dokumen dapat dilint, semua milestone punya artefak bukti, setiap risiko punya owner/mitigation.  
**Failure modes:** scope terlalu luas, target berubah tanpa ADR, definisi selesai tidak jelas.  
**Rollback:** freeze fitur, kembali ke ADR terakhir, pecah milestone.

### M1 - Toolchain reproducible di Windows 11 x64 melalui WSL 2

**Tujuan:** build environment dapat direproduksi dari clean checkout.  
**Deliverables:** `toolchain.lock`, `build/meta/toolchain-versions.txt`, `Makefile`, `check_toolchain.sh`.  
**Evidence:** `make meta`, `make check`, object file x86_64 ELF diperiksa dengan `readelf`/`objdump`.  
**Failure modes:** compiler host salah, linker default dipakai, repository di `/mnt/c`, QEMU command tidak tercatat.  
**Rollback:** pin toolchain stabil, TCG-only baseline, matikan optimasi agresif.

### M2 - Boot image, ELF64 kernel, dan early serial console

**Tujuan:** image bootable masuk ke entry kernel dan mencetak stage marker ke serial log.  
**Deliverables:** `kernel.elf`, `mcsos.iso` atau disk image, `kernel.map`, `build/qemu-serial.log`, `boot_handoff.md`.  
**Evidence:** banner MCSOS 260502, commit hash, stage marker, no unexpected reset, image checksum.  
**Failure modes:** kernel tidak ditemukan, wrong entry, bad stack, malformed handoff, serial tidak keluar.  
**Rollback:** kembali ke image terakhir, QEMU `-display none -serial file`, minimal Limine config.

### M3 - Panic path, GDB, linker map, dan disassembly audit

**Tujuan:** setiap fault awal menghasilkan bukti yang dapat ditriase.  
**Deliverables:** `panic.c`, `printk`, `.gdbinit`, `objdump.txt`, `readelf.txt`, debug runbook.  
**Evidence:** breakpoint `kernel_main`, manual panic terbaca, map file cocok dengan linker script.  
**Failure modes:** triple fault tanpa log, symbol mismatch, wrong load address, stack corruption.  
**Rollback:** disable paging custom, boot dengan minimal entry, inspect PHDR/section.

### M4 - CPU foundation, trap, interrupt, dan timer

**Tujuan:** IDT, GDT, TSS, exception, timer, dan interrupt masking bekerja deterministik.  
**Deliverables:** trap frame spec, IDT handlers, exception report, LAPIC/PIT/HPET plan, timer test.  
**Evidence:** divide-by-zero/page-fault test, timer tick counter, interrupt enable point terdokumentasi.  
**Failure modes:** bad `iretq`, wrong error code, interrupt storm, double fault, timer drift ekstrem.  
**Rollback:** mask external IRQ, gunakan exception-only mode, kembali ke polling timer.

### M5 - PMM, VMM, kernel heap, dan page-table invariants

**Tujuan:** frame fisik, page table, direct map, heap awal, dan permission policy stabil.  
**Deliverables:** PMM bitmap/buddy awal, VMM map/unmap, heap/slab awal, page table dump.  
**Evidence:** allocator invariant test, W^X check, kernel text non-writable, TLB invalidation test.  
**Failure modes:** double-free frame, frame leak, writable text, stale TLB, heap metadata corruption.  
**Rollback:** disable heap complex, fixed allocator, single address-space debug mode.

### M6 - Thread, scheduler, synchronization, dan wait queue

**Tujuan:** thread lifecycle, context switch, timer preemption, spinlock, mutex, dan wait queue terbukti pada single-core.  
**Deliverables:** `struct thread`, runqueue, context switch stub, idle thread, sleep/wakeup.  
**Evidence:** thread state transition tests, preemption counter, lock-order notes, no sleep in hard IRQ.  
**Failure modes:** lost wakeup, runqueue corruption, stack switch bug, deadlock, priority inversion awal.  
**Rollback:** cooperative scheduling, single runqueue, disable preemption around suspect paths.

### M7 - User mode, syscall ABI, dan ELF loader awal

**Tujuan:** proses userspace minimal dapat berjalan dan melakukan syscall terbatas.  
**Deliverables:** ring-3 transition, syscall entry/exit, `copyin/copyout`, ELF PT_LOAD loader, `init` minimal.  
**Evidence:** user program mencetak via syscall, invalid pointer menghasilkan error aman, syscall fuzz dasar.  
**Failure modes:** wrong TSS/RSP0, `sysret` canonical bug, user pointer deref, kernel pointer leak.  
**Rollback:** gunakan interrupt-gate syscall sementara, batasi syscall ke `write`, `exit`, `yield`.

### M8 - VFS, file descriptor, ramfs, dan initrd

**Tujuan:** object model file, FD table, path lookup, ramfs/initrd, dan read-only file access bekerja.  
**Deliverables:** VFS objects, `open/read/close/stat`, ramfs, initrd parser, FD table.  
**Evidence:** lookup/open/read tests, refcount check, unmount negative test, no leaked file ref.  
**Failure modes:** dentry ref leak, path traversal bug, FD reuse bug, initrd bounds bug.  
**Rollback:** read-only ramfs, flat namespace, no rename/unlink.

### M9 - Device model, PCI, virtio, serial, framebuffer, dan block layer awal

**Tujuan:** device discovery dan driver lifecycle aman sebelum persistent FS.  
**Deliverables:** driver core, probe/remove contract, PCI enumeration, virtio-blk or simple block, framebuffer driver.  
**Evidence:** PCI dump, probe failure cleanup, MMIO accessor audit, block read smoke test.  
**Failure modes:** bind driver sebelum BAR valid, IRQ storm, MMIO cached access, stale device state.  
**Rollback:** disable driver auto-probe, use initrd-only boot, serial-only console.

### M10 - Persistent filesystem: MCSFS atau ext2-like teaching FS

**Tujuan:** filesystem persisten sederhana dengan fsck dan crash injection.  
**Deliverables:** on-disk format spec, superblock, inode, directory, free map, write path, fsck prototype.  
**Evidence:** mount/read/write/create/unlink tests, fsck detects known corruptions, crash injection log.  
**Failure modes:** double allocation, orphan inode, directory corruption, lost flush, rename violation.  
**Rollback:** read-only mode, force fsck on mount, disable metadata writeback.

### M11 - Networking stack minimum

**Tujuan:** loopback, Ethernet, ARP, IPv4, ICMP echo, UDP, TCP subset, dan socket ABI minimal.  
**Deliverables:** packet buffer, checksum library, loopback, virtio-net/e1000 path, socket syscalls.  
**Evidence:** pcap replay, ping test, UDP echo, TCP state subset test, malformed packet fuzz.  
**Failure modes:** unchecked packet length, checksum bug, buffer leak, timer bug, queue overflow.  
**Rollback:** loopback-only, disable raw sockets, drop malformed packets fail-closed.

### M12 - Security model dan hardening baseline

**Tujuan:** threat model, credential, capability handles, syscall validation, audit, dan exploit mitigation baseline.  
**Deliverables:** credentials, capability table, W^X, ASLR plan, usercopy hardening, audit schema, syscall fuzz.  
**Evidence:** negative authorization tests, invalid pointer fuzz, no kernel pointer leak non-debug, audit events.  
**Failure modes:** confused deputy, capability leak, TOCTOU, usercopy overflow, rollback bypass.  
**Rollback:** deny-by-default policy, disable privileged syscalls, minimal capability set.

### M13 - SMP, scalability, dan adaptive policy foundation

**Tujuan:** AP bring-up, per-CPU data, TLB shootdown, SMP-safe locks, scheduler load balancing, dan adaptive policy safe-mode.  
**Deliverables:** AP trampoline, per-CPU stack/TSS/runqueue, IPI, TLB shootdown, stress tests.  
**Evidence:** N-core boot log, TLB shootdown acknowledgment, lock stress, `mcsos.adapt=off/on` trace.  
**Failure modes:** AP hang, race, stale TLB, per-CPU corruption, lock convoy.  
**Rollback:** `nosmp`, `maxcpus=1`, disable adaptive policy.

### M14 - Graphics dan display path

**Tujuan:** framebuffer console stabil, visual regression, dan rencana modesetting terbatas.  
**Deliverables:** framebuffer console, font renderer sederhana, panic framebuffer fallback, screenshot harness.  
**Evidence:** serial tetap tersedia, panic tampil, visual regression stable, pitch/format validation.  
**Failure modes:** blank screen, wrong pitch, framebuffer overrun, panic lost due graphics.  
**Rollback:** serial-only console, text mode fallback jika tersedia.

### M15 - Virtualization/container subset

**Tujuan:** subset isolasi berbasis namespace/cgroup-lite dan virtio readiness, bukan full hypervisor production.  
**Deliverables:** PID/mount namespace prototype, resource accounting, sandbox policy, escape tests.  
**Evidence:** namespace negative tests, cgroup limit tests, device access denied by default.  
**Failure modes:** namespace leak, device bypass, mount escape, accounting overflow.  
**Rollback:** disable namespaces, single global namespace, privileged-only mount.

### M16 - Observability, update/rollback, release readiness review

**Tujuan:** sistem memiliki bukti operasional untuk developer preview terbatas.  
**Deliverables:** tracepoints, metrics, support bundle, crash dump plan, signed image plan, rollback drill, release notes.  
**Evidence:** reproducible build report, test matrix pass/fail, known issues, rollback test, security signoff.  
**Failure modes:** telemetry gap, support bundle leaks secret, rollback failed, artifact hash mismatch.  
**Rollback:** freeze release, revert to previous milestone image, publish known-risk report.

---

## 14. Validation Matrix

| Area | Test minimum | Artefak |
|---|---|---|
| Build | clean checkout build, warning-as-error, dependency check | `build.log`, `toolchain-versions.txt` |
| Link/image | `readelf -lW`, `objdump -drwC`, `kernel.map` review | `readelf.txt`, `objdump.txt`, `kernel.map` |
| Boot | QEMU headless boot to stage marker | `qemu-serial.log`, QEMU command |
| Debug | GDB breakpoint and register inspection | `gdb-session.txt` |
| Trap | exception injection | trap logs |
| Memory | allocator and page table invariant tests | unit logs, page dump |
| Scheduler | thread lifecycle and stress | stress report |
| Syscall | valid/invalid syscall fuzz | fuzz corpus, report |
| VFS/FS | golden image, fsck, crash injection | image copy, fsck logs |
| Block/driver | probe/remove, MMIO, IRQ, DMA ownership | driver trace |
| Network | pcap replay, malformed packet fuzz | `.pcap`, fuzz report |
| Security | negative authorization, usercopy, W^X | security test report |
| SMP | per-CPU, TLB shootdown, lock stress | SMP log |
| Graphics | framebuffer screenshot and visual regression | screenshot, diff |
| Update/rollback | signed/hash image, failed update simulation | rollback drill log |

---

## 15. Failure Modes dan Triage Standard

### 15.1 Klasifikasi fault

| Fault | Indikator | Triage awal |
|---|---|---|
| Build failure | compiler/linker error | cek target triple, flags, generated file |
| Wrong ELF layout | bootloader gagal load atau entry salah | `readelf -lW`, `kernel.map`, `objdump` |
| Boot hang | tidak ada serial stage | cek QEMU command, Limine config, entry, stack |
| Triple fault/reset | QEMU reboot/no log | jalankan `-no-reboot -d int`, GDB start at entry |
| Page fault | CR2/error code | page table dump, permission, mapping ownership |
| GPF | vector 13 | seg selector, `iretq`, bad MSR, wrong descriptor |
| Interrupt storm | serial banjir/timer hang | mask IRQ, audit EOI/ack order |
| Deadlock | no progress | lock-order graph, interrupt state, wait queue |
| Race | flake under stress | repeat seed, trace state transition |
| FS corruption | fsck fails | block trace, crash injection sequence |
| DMA corruption | random memory corruption | DMA ownership, IOMMU, cache coherency |
| Packet parser bug | malformed packet panic | bounds check, pcap minimization |
| Security bypass | unauthorized access succeeds | audit capability path, negative test |
| Graphics blank | no display but serial alive | pitch/format/mapping, fallback serial |
| Rollback failure | cannot boot previous image | rescue media, signed artifact, state partition |

### 15.2 Triage discipline

1. Simpan artefak: serial log, QEMU command, commit hash, kernel map, disassembly, core/panic dump.
2. Reproduksi pada clean checkout.
3. Bandingkan dengan milestone terakhir yang hijau.
4. Minimalkan konfigurasi: single CPU, no adaptive policy, no external IRQ, serial-only.
5. Gunakan GDB untuk entry, CR3, IDT, stack, dan register fault.
6. Tambahkan regression test sebelum memperbaiki bug.
7. Catat root cause, fix, test evidence, dan residual risk.

---

## 16. Security Baseline

### 16.1 Asset utama

1. Kernel image dan symbol policy.
2. Boot configuration dan initrd.
3. Page table dan address-space isolation.
4. Credential, capability table, FD table, IPC objects.
5. Filesystem metadata dan user data.
6. Driver MMIO/DMA mappings.
7. Network packet buffers dan socket state.
8. Update artifacts dan rollback slots.
9. Logs, traces, and support bundles.

### 16.2 Attack surface awal

| Surface | Kontrol awal |
|---|---|
| Boot handoff | structural validation, checksum where available, bounds checks |
| Syscall | number/pointer/length/flags/capability validation |
| User memory | `copyin/copyout`, no raw dereference |
| Filesystem | path normalization, refcount, mount policy |
| Network | bounds-checked parser, packet fuzz, rate/backpressure |
| Driver | MMIO accessors, DMA ownership, probe cleanup |
| Debug | no kernel pointer leak on non-debug config |
| Update | signed/hash plan, rollback rehearsal |

### 16.3 Audit event minimum

1. `exec`, `exit`, `fork/spawn`, `setuid/credential-change` jika tersedia.
2. `mount`, `unmount`, device open, raw socket open.
3. Capability grant, revoke, transfer, and failure.
4. Policy change, debug mode enable, module load/unload jika tersedia.
5. Update start, verify, commit, rollback, failure.
6. Security-relevant syscall denial.

---

## 17. Standar Praktikum dan Penilaian

### 17.1 Struktur modul praktikum turunan

Setiap modul MCSOS yang diturunkan dari kerangka ini wajib memiliki:

1. Judul.
2. Identitas dosen: Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.
3. Capaian pembelajaran.
4. Prasyarat teori.
5. Peta skill yang digunakan.
6. Alat dan versi.
7. Repository awal.
8. Target praktikum.
9. Konsep inti.
10. Arsitektur ringkas.
11. Instruksi langkah demi langkah.
12. Checkpoint buildable.
13. Tugas implementasi.
14. Perintah uji.
15. Bukti yang harus dikumpulkan.
16. Pertanyaan analisis.
17. Rubrik penilaian.
18. Failure modes.
19. Prosedur rollback.
20. Template laporan.
21. Kriteria Lulus Praktikum.
22. Readiness Review.

### 17.2 Kriteria Lulus Praktikum minimum

1. Proyek dapat dibangun dari clean checkout.
2. Perintah build terdokumentasi.
3. QEMU boot atau test target berjalan deterministik sesuai milestone.
4. Semua unit test/praktikum test lulus atau kegagalan terdokumentasi sebagai known issue yang disetujui.
5. Serial log disimpan.
6. Panic path terbaca.
7. Tidak ada warning kritis pada konfigurasi praktikum.
8. Perubahan Git terkomit.
9. Mahasiswa menjelaskan desain, invariants, dan failure modes.
10. Laporan berisi screenshot/log/test evidence cukup.

### 17.3 Rubrik 100 poin

| Komponen | Bobot |
|---|---:|
| Kebenaran fungsional | 30 |
| Kualitas desain dan invariants | 20 |
| Pengujian dan bukti | 20 |
| Debugging dan failure analysis | 10 |
| Keamanan dan robustness | 10 |
| Dokumentasi/laporan | 10 |

### 17.4 Template laporan praktikum

1. Sampul: judul praktikum, nama mahasiswa, NIM, kelas, dosen Muhaemin Sidiq, S.Pd., M.Pd., Program Studi Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia.
2. Tujuan: capaian teknis dan konseptual.
3. Dasar teori ringkas: konsep OS yang diuji.
4. Lingkungan: OS host, versi compiler, QEMU, GDB, target arsitektur, commit hash.
5. Desain: diagram singkat, struktur data, invariants, alur kontrol, batasan.
6. Langkah kerja: perintah, perubahan file, dan alasan teknis.
7. Hasil uji: output build, log QEMU, screenshot, hasil `make test`, trace, dan tabel pass/fail.
8. Analisis: penyebab keberhasilan, bug, failure modes, dan perbandingan dengan teori.
9. Keamanan dan reliability: risiko privilege, memory safety, race, data loss, dan mitigasi.
10. Kesimpulan: apa yang berhasil, apa yang belum, dan rencana perbaikan.
11. Lampiran: potongan kode penting, diff ringkas, log penuh, dan referensi.

---

## 18. Readiness Review Kerangka

### 18.1 Status kerangka ini

Kerangka MCSOS 260502-10 berstatus **siap dijadikan baseline arsitektur dan roadmap pengembangan praktikum**. Status ini tidak berarti MCSOS telah boot, tidak berarti bebas error, dan tidak berarti siap produksi. Status implementasi hanya dapat dinaikkan per milestone setelah evidence gate terpenuhi.

### 18.2 Label readiness yang diperbolehkan

| Label | Syarat minimum |
|---|---|
| Siap uji QEMU M2 | image boot, serial log, kernel map, clean build, no unexpected reset |
| Siap uji subsystem | unit/integration test subsystem lulus, negative test tersedia, failure modes terdokumentasi |
| Siap demonstrasi praktikum | instruksi, rubrik, log, screenshot, rollback, dan laporan tersedia |
| Siap bring-up perangkat keras terbatas | observability, panic path, rollback, hardware matrix kecil, known risks tersedia |
| Kandidat developer preview terbatas | M16 evidence lengkap, security signoff, known issues, update/rollback, reproducible build |

### 18.3 Keputusan akhir

Kerangka ini valid sebagai dokumen pengendali karena:

1. Target, host, toolchain, kernel model, dan non-goals eksplisit.
2. Setiap subsystem memiliki boundary dan larangan yang dapat diuji.
3. Roadmap membangun fondasi secara bertahap dari M0 sampai M16.
4. Adaptasi hardware dibatasi oleh telemetry, safety, dan rollback.
5. Security dan observability tidak ditempatkan sebagai fitur akhir, melainkan kontrol sejak fase awal.
6. Setiap klaim kesiapan bergantung pada artefak verifikabel.

---

## References

[1] Microsoft, “Install WSL,” Microsoft Learn, 2025. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/install. Accessed: May 2, 2026.

[2] The LLVM Project, “Cross-compilation using Clang,” Clang Documentation, 2026. [Online]. Available: https://clang.llvm.org/docs/CrossCompilation.html. Accessed: May 2, 2026.

[3] Free Software Foundation, “x86 Options,” GCC 14.3.0 Manual, 2025. [Online]. Available: https://gcc.gnu.org/onlinedocs/gcc-14.3.0/gcc/x86-Options.html. Accessed: May 2, 2026.

[4] QEMU Project, “GDB usage,” QEMU Documentation, 2026. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html. Accessed: May 2, 2026.

[5] UEFI Forum, “UEFI Specifications,” 2026. [Online]. Available: https://uefi.org/specifications. Accessed: May 2, 2026.

[6] UEFI Forum, “Advanced Configuration and Power Interface Specification, Version 6.6,” May 2025. [Online]. Available: https://uefi.org/specs/ACPI/6.6/. Accessed: May 2, 2026.

[7] Intel Corporation, “Intel 64 and IA-32 Architectures Software Developer’s Manuals,” Version Latest, updated Apr. 6, 2026. [Online]. Available: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html. Accessed: May 2, 2026.

[8] Advanced Micro Devices, Inc., “AMD64 Architecture Programmer’s Manual Volume 2: System Programming,” Document ID 24593, Rev. 3.44, Mar. 6, 2026. [Online]. Available: https://docs.amd.com/v/u/en-US/24593_3.44_APM_Vol2. Accessed: May 2, 2026.

[9] Limine Bootloader Project, “Limine: Modern, advanced, portable, multiprotocol bootloader and boot manager,” GitHub Repository, 2026. [Online]. Available: https://github.com/limine-bootloader/limine. Accessed: May 2, 2026.

[10] A. R. Regenscheid, “Platform Firmware Resiliency Guidelines,” NIST Special Publication 800-193, National Institute of Standards and Technology, May 2018. [Online]. Available: https://doi.org/10.6028/NIST.SP.800-193. Accessed: May 2, 2026.
