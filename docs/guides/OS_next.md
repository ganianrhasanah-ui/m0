# OS_next.md

# Kerangka Kerja Pengembangan Tahap Lanjut MCSOS Menuju Sistem Operasi Desktop/Laptop Modern

**Mata kuliah**: Praktikum Sistem Operasi Lanjut  
**Sistem operasi pendidikan**: MCSOS versi 260502  
**Dokumen**: OS_next.md  
**Dosen**: Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi**: Pendidikan Teknologi Informasi  
**Institusi**: Institut Pendidikan Indonesia  
**Target awal**: x86_64, UEFI/OVMF, QEMU, Windows 11 x64 + WSL 2 untuk pengembangan, kernel monolitik pendidikan yang berevolusi menjadi platform desktop/laptop bertahap.  
**Status dokumen**: kerangka kerja pengembangan tahap lanjut. Dokumen ini bukan klaim bahwa MCSOS sudah siap dipakai di komputer/laptop. Status yang boleh dicapai bertahap adalah: *siap uji QEMU*, *siap bring-up perangkat keras terbatas*, *siap demonstrasi desktop terbatas*, *kandidat developer preview terbatas*, dan hanya setelah bukti memadai dapat dinilai sebagai *kandidat penggunaan profesional terbatas*.

---

## 1. Ringkasan Eksekutif

MCSOS M0-M16 telah membentuk baseline pendidikan yang penting: lingkungan pengembangan reproducible, boot image, early console, panic/logging path, IDT/trap, interrupt/timer, physical memory manager, virtual memory manager awal, kernel heap, kernel thread, syscall ABI, ELF loader awal, sinkronisasi kernel, VFS/RAMFS, block layer, filesystem persistent minimal MCSFS1, dan write-ahead journal MCSFS1J [1]-[17]. Tahap lanjut tidak boleh sekadar menambah fitur secara acak. Agar MCSOS dapat bergerak menuju sistem operasi yang dapat digunakan di komputer/laptop untuk pekerjaan profesional sehari-hari, pengembangan harus berubah dari rangkaian praktikum subsistem menjadi program rekayasa sistem lengkap: kernel maturity, hardware enablement, POSIX-like compatibility, userland, desktop shell, security, update/rollback, observability, application ecosystem, dan hardware certification.

Kebutuhan profesional sehari-hari minimal mencakup: boot stabil pada perangkat acuan, manajemen daya laptop, storage persisten yang tidak mudah korup, login multi-user, file manager, editor dokumen, browser web, terminal dan tool pengembangan, koneksi jaringan kabel/nirkabel, audio/video conference, printer/scanner, update aman, backup, enkripsi penyimpanan, sandbox aplikasi, dan mekanisme pemulihan ketika update gagal. Target tersebut jauh melampaui M16. Karena itu kerangka OS_next memecah pekerjaan menjadi jalur-jalur independen tetapi saling menguatkan: **kernel hardening**, **hardware platform**, **storage/filesystem**, **networking**, **driver model**, **userspace/POSIX**, **desktop/graphics**, **security**, **virtualization/container**, **packaging/update**, **observability/operations**, dan **release certification**.

Prinsip utama dokumen ini adalah **evidence-gated development**. Setiap tahap hanya boleh naik tingkat jika memiliki bukti build, unit test, QEMU test, hardware test pada perangkat acuan, static analysis, fuzzing bila relevan, trace/log, failure-mode analysis, rollback procedure, dan readiness review. Keberhasilan boot atau keberhasilan demo GUI tidak boleh dipakai sebagai bukti sistem siap digunakan secara profesional.

---

## 2. Sumber Baseline dan Konteks Pengembangan

Kerangka lanjut ini meneruskan capaian berikut.

| Baseline | Fungsi yang sudah dibangun | Peran untuk tahap lanjut |
|---|---|---|
| M0-M1 | governance, repository, toolchain, reproducibility | menjadi trusted development base dan CI policy [1], [2] |
| M2-M3 | boot image, early console, panic/logging, linker map, disassembly audit | menjadi root of debuggability [3], [4] |
| M4-M5 | IDT, exception path, PIC/PIT timer | menjadi fondasi trap/interrupt, tetapi harus diganti/ditingkatkan dengan APIC/HPET/TSC deadline untuk laptop modern [5], [6] |
| M6-M8 | PMM, VMM, kernel heap | menjadi fondasi memory subsystem yang perlu dikembangkan ke address space per proses, page cache, mmap, copy-on-write, swapping, NUMA-lite [7]-[9] |
| M9-M12 | kernel thread, scheduler, syscall, ELF loader, sinkronisasi | menjadi fondasi process model, preemption, wait/exit, signal, IPC, dan POSIX-like ABI [10]-[13] |
| M13-M16 | VFS, block layer, MCSFS1, MCSFS1J journal/recovery | menjadi fondasi storage, tetapi perlu page cache, permissions, directory tree, fsck, crash testing, dan driver storage nyata [14]-[17] |

Sumber eksternal yang menjadi acuan tahap lanjut meliputi POSIX.1-2024 untuk interface portabilitas source-level [18], UEFI dan ACPI versi terbaru untuk boot, discovery, power, thermal, dan platform control [20], [21], Linux kernel documentation sebagai pembanding konseptual untuk VFS, journaling, driver, dan DMA [22]-[24], RFC/IETF untuk protokol jaringan inti [25]-[28], freedesktop/Wayland/D-Bus/XDG untuk ekosistem desktop interoperable [29]-[34], NIST/CISA/SLSA untuk secure development dan supply-chain security [35]-[37], serta OpenTelemetry untuk model observability [38].

---

## 3. Asumsi, Scope, dan Non-Goals

### 3.1 Asumsi target

| Aspek | Keputusan konservatif OS_next |
|---|---|
| Arsitektur primer | x86_64 PC/laptop modern |
| Firmware | UEFI; BIOS legacy hanya mode kompatibilitas terbatas |
| Platform discovery | ACPI, SMBIOS/DMI, PCI/PCIe enumeration, USB descriptors |
| Hardware acuan | QEMU/OVMF, satu desktop mini-PC referensi, satu laptop referensi Intel/AMD, satu konfigurasi virtio lengkap |
| Kernel model | Monolitik modular internal, dengan batas driver dan userland yang makin jelas |
| Bahasa kernel | C17 freestanding + assembly minimal; Rust `no_std` dapat menjadi jalur pengayaan setelah ABI dan build policy matang |
| Userland target | POSIX-like subset bertahap; kompatibilitas Linux bukan target awal penuh |
| Desktop target | GUI native MCSOS dengan Wayland-like compositor atau kompatibilitas subset protokol Wayland |
| Aplikasi profesional | Dicapai melalui kombinasi aplikasi native, port POSIX, tool CLI, dan runtime compatibility bertahap |
| Security model | Secure-by-design, least privilege, signed update, sandbox, audit, enkripsi data pengguna |
| Readiness akhir dokumen | Kandidat developer preview terbatas; bukan production-ready tanpa bukti longitudinal |

### 3.2 Non-goals tahap awal OS_next

OS_next tidak langsung menargetkan kompatibilitas Linux ABI penuh, menjalankan semua aplikasi Windows, mendukung semua laptop/driver pasar, driver GPU 3D produksi, Wi-Fi/Bluetooth semua chipset, Secure Boot produksi dengan semua OEM, filesystem setara ext4/btrfs, browser modern lengkap, atau sertifikasi keamanan. Semua target tersebut merupakan program multi-tahun yang harus melewati gate khusus.

---

## 4. Definisi Target “Dapat Digunakan untuk Pekerjaan Profesional Sehari-hari”

MCSOS dianggap mendekati penggunaan profesional terbatas hanya jika perangkat acuan dapat menyelesaikan skenario berikut secara berulang selama minimal 30 hari uji tanpa kehilangan data dan tanpa crash yang tidak dapat dipulihkan.

| Area kebutuhan | Kapabilitas minimum | Bukti wajib |
|---|---|---|
| Boot dan login | boot UEFI dari SSD, login multi-user, shell masuk desktop | boot log, audit boot chain, login test 100 siklus |
| File dan dokumen | create/open/save/copy/move/rename/delete, permissions, backup | fs test, crash test, fsck/scrub, user acceptance log |
| Jaringan | DHCP, DNS, IPv4, IPv6 baseline, TCP/UDP socket, TLS di userland | pcap replay, RFC conformance subset, interop dengan Linux/Windows |
| Browser/web | minimal browser port atau web runtime terbatas, TLS, font, input, sandbox | web compatibility matrix, crash/fuzz evidence |
| Produktivitas | editor teks, PDF viewer, office document workflow, print/export | app test suite, file-format roundtrip evidence |
| Pengembangan | terminal, shell, compiler/toolchain host, Git-like workflow | build project sample, package install logs |
| Multimedia | audio playback/record, camera path opsional, screen sharing long-term | latency test, underrun metrics, permission prompts |
| Laptop | battery indicator, suspend/resume, lid close, brightness, touchpad/keyboard | ACPI event tests, suspend/resume loop, power traces |
| Update | signed update, rollback, recovery boot entry | staged update test, rollback drill, checksum/signature evidence |
| Security | user/kernel isolation, app sandbox, secrets, audit log | negative tests, syscall fuzz, policy regression |
| Observability | logs, metrics, crash dump, support bundle | trace schema, crash reproduction, privacy review |

---

## 5. Architecture / Design Target Jangka Panjang

### 5.1 Lapisan sistem

```text
+-------------------------------------------------------------+
| Aplikasi Profesional                                        |
| Office, Browser, Terminal, IDE, PDF, Media, File Manager     |
+----------------------+--------------------------------------+
| Runtime & Userland   | libc, shell, package manager, daemon  |
|                      | D-Bus-like IPC, service manager       |
+----------------------+--------------------------------------+
| Desktop Stack        | compositor, window manager, UI toolkit|
|                      | font, input, clipboard, notifications |
+----------------------+--------------------------------------+
| POSIX-like ABI       | process, fd, mmap, signal, sockets    |
| dan Security ABI     | credentials, permissions, sandbox     |
+----------------------+--------------------------------------+
| Kernel Services      | scheduler, VM, IPC, VFS, page cache   |
|                      | network stack, storage stack, drivers |
+----------------------+--------------------------------------+
| Hardware Platform    | UEFI, ACPI, PCIe, USB, NVMe, NIC, GPU |
|                      | audio, input, power, thermal          |
+----------------------+--------------------------------------+
| Toolchain/CI/Release | reproducible build, signing, fuzzing  |
+-------------------------------------------------------------+
```

### 5.2 Invariants lintas subsistem

1. **Memory ownership**: setiap page/frame/buffer harus memiliki owner, lifetime, permission, dan teardown path eksplisit.
2. **No silent failure**: kegagalan boot, driver, filesystem, update, dan GUI harus menghasilkan log atau crash dump yang dapat diaudit.
3. **Fail-closed security**: input malformed dari user, network, disk, firmware table, atau device descriptor harus ditolak tanpa corrupt state.
4. **No dependency without gate**: fitur lanjutan tidak boleh bergantung pada subsistem yang belum lulus readiness gate.
5. **Rollback before risk**: semua perubahan boot, storage, update, driver, dan security policy harus memiliki rollback path.
6. **Reference-hardware first**: dukungan laptop umum hanya boleh diklaim setelah satu perangkat acuan lulus test matrix; perangkat lain dianggap eksperimental.
7. **Observability by default**: setiap subsistem harus memiliki counters, tracepoints, error codes, dan support-bundle schema.
8. **Compatibility is measured**: POSIX-like, Wayland-like, atau Linux-like tidak boleh diklaim tanpa conformance subset yang tertulis.
9. **User data is sacred**: setiap perubahan filesystem, package update, dan app sandbox harus diuji terhadap risiko data loss.
10. **Security and reliability are release blockers**: crash, privilege bypass, data loss, update bricking, dan filesystem corruption adalah blocker, bukan catatan kosmetik.

---

## 6. Roadmap Makro OS_next

Roadmap berikut tidak dimaksudkan sebagai satu semester tunggal. Ia adalah program pengembangan berlapis. Setiap milestone harus menghasilkan artefak buildable, testable, documented, dan reviewable.

| Track | Milestone | Fokus | Readiness target |
|---|---:|---|---|
| Stabilization | M17-M20 | integrasi repo final M0-M16, CI lokal, test harness, boot smoke berulang | siap uji QEMU berulang |
| Kernel maturity | M21-M28 | SMP, APIC, scheduler preemptive, process lifecycle, IPC, signal, mmap | siap kernel developer preview |
| Platform/hardware | M29-M36 | ACPI, PCIe, xHCI USB, NVMe/AHCI, HID, power, thermal | siap bring-up perangkat acuan |
| Storage/filesystem | M37-M44 | page cache, permission, directory tree, fsck, journal hardening, snapshot/rollback | siap uji data-preservation terbatas |
| Networking | M45-M52 | Ethernet/NIC, IPv4/IPv6, ARP/NDP, ICMP, UDP, TCP, sockets, firewall | siap interop jaringan terbatas |
| Userland/POSIX | M53-M60 | libc, shell, init/service manager, process tools, package bootstrap | siap userland CLI terbatas |
| Security | M61-M68 | credentials, DAC/ACL/capability, sandbox, secure update, audit, encryption | siap security review terbatas |
| Graphics/desktop | M69-M78 | framebuffer GUI, compositor, input, fonts, file manager, terminal GUI | siap demonstrasi desktop terbatas |
| Application ecosystem | M79-M88 | port aplikasi CLI/GUI, browser strategy, office workflow, multimedia | kandidat penggunaan profesional terbatas |
| Virtualization/container | M89-M94 | namespace/cgroup-like, OCI subset, VMM/virtio subset | siap developer sandbox terbatas |
| Release/operations | M95-M100 | installer, signed release, telemetry lokal, hardware matrix, support docs | kandidat developer preview terbatas |

---

## 7. Milestone Detail Tahap Lanjut

### M17 — Repository Integration, Build Matrix, dan Regression Baseline

**Tujuan**: menyatukan hasil M0-M16 menjadi satu repository yang benar-benar dapat dibangun dari clean checkout.

**Komponen**: root Makefile, module graph, `include/mcsos/`, `kernel/`, `user/`, `drivers/`, `tests/`, `tools/`, `docs/`, `evidence/`, target `make all`, `make test`, `make qemu-smoke`, `make audit`, `make dist`.

**Checkpoint buildable**:

```bash
make distclean
make all
make test
make audit
make qemu-smoke
```

**Acceptance criteria**: semua host unit test M6-M16 lulus; kernel boot QEMU sampai shell stub atau panic-controlled halt; `nm -u` untuk kernel object agregat terkontrol; manifest versi toolchain tersimpan; SHA-256 semua artefak rilis tersimpan.

### M18 — CI Lokal, Static Analysis, dan Fuzz Harness Awal

**Tujuan**: mengubah pengujian manual menjadi pipeline lokal dan CI yang reproducible.

**Komponen**: GitHub Actions/GitLab CI opsional, runner lokal WSL, `clang-tidy`, `cppcheck`, UBSan-compatible host tests, fuzzing untuk parser ELF, filesystem image, journal, packet parser, desktop entry parser.

**Acceptance criteria**: pipeline menolak warning kritis, dependency tidak terpin, test flake, dan undefined symbol tidak terdokumentasi.

### M19 — Hardware Abstraction Layer dan Platform Boot Contract

**Tujuan**: mengekstrak boundary arsitektur x86_64 agar kernel tidak tersebar dengan akses CPU/firmware langsung.

**Komponen**: `arch/x86_64/`, CPU feature discovery, CPUID, MSR wrapper, APIC capability detection, boot handoff contract, memory map validation, SMBIOS/ACPI RSDP discovery.

**Acceptance criteria**: boot log mencatat CPU feature set, firmware source, memory map summary, ACPI RSDP checksum status, dan fallback path jika ACPI tidak valid.

### M20 — Runtime Init Graph dan Service Supervision Awal

**Tujuan**: menyediakan urutan inisialisasi kernel dan userland yang eksplisit.

**Komponen**: initcall levels, dependency graph, `mcs_init`, service descriptors, watchdog untuk daemon inti, crash restart policy.

**Acceptance criteria**: init graph dapat divisualisasikan; siklus dependency ditolak; panic/recovery path terdokumentasi.

---

## 8. Kernel Maturity Track

### M21 — APIC, IOAPIC, HPET/TSC Deadline Timer

Legacy PIC/PIT M5 cukup untuk pendidikan awal, tetapi laptop/desktop modern membutuhkan LAPIC/IOAPIC, MSI/MSI-X, HPET atau TSC-deadline, serta interrupt routing berbasis ACPI MADT. Milestone ini mengganti model interrupt awal menjadi IRQ domain yang eksplisit.

**Artefak**: parser MADT, LAPIC init, IOAPIC routing table, timer calibration, interrupt affinity placeholder, interrupt storm detector.

**Acceptance criteria**: timer tick stabil di QEMU dan perangkat acuan; IRQ routing tidak bentrok dengan exception; interrupt storm dimask dan dilaporkan.

### M22 — SMP Bring-up dan Per-CPU State

**Tujuan**: menyalakan Application Processor secara terkendali, menyiapkan per-CPU stacks, GDT/IDT/TSS per CPU, per-CPU scheduler state, dan TLB shootdown protocol.

**Acceptance criteria**: `-smp 2/4` QEMU lulus stress; race detector host tests lulus; lock ordering terdokumentasi; satu CPU panic tidak membuat state global tidak terdiagnosis.

### M23 — Preemptive Scheduler dan Timer Accounting

**Tujuan**: meningkatkan scheduler M9 dari kooperatif menjadi preemptive single-class dengan accounting, sleep queue, wakeup, timer wheel/min-heap, dan priority placeholder.

**Acceptance criteria**: fairness test, starvation test, timer accuracy test, context-switch register preservation test, dan interrupt-preemption race test lulus.

### M24 — Process Lifecycle: fork-like, spawn, exec, wait, exit

**Tujuan**: membangun process object penuh di atas ELF loader M11, syscall M10, VFS M13, dan VMM M7.

**Komponen**: PID allocator, process table, parent-child relation, exit status, wait queue, file descriptor inheritance, address-space teardown.

**Acceptance criteria**: 1.000 siklus spawn/exit tanpa leak; invalid ELF fail-closed; fd leak test lulus; zombie reaping test lulus.

### M25 — Signals, Timers, dan Job Control Awal

**Tujuan**: menyediakan subset yang dibutuhkan shell, terminal, dan program POSIX-like.

**Komponen**: signal delivery, signal mask, default actions, `SIGCHLD`, process groups, terminal foreground group placeholder.

**Acceptance criteria**: shell dapat menjalankan foreground/background job sederhana; `Ctrl-C`/interrupt path pada terminal tidak merusak kernel.

### M26 — mmap, Page Fault Recovery, Copy-on-Write, dan Demand Paging

**Tujuan**: membangun memory model yang diperlukan runtime modern dan file mapping.

**Komponen**: VMA tree, `mmap/munmap/mprotect`, page fault resolver, anonymous memory, file-backed mapping, COW fork, guard pages.

**Acceptance criteria**: page fault classification lengkap; malformed user pointer tidak panic; COW isolation test lulus; `mmap` file roundtrip lulus.

### M27 — IPC Kernel: Pipes, Event, Shared Memory, dan Message Queue

**Tujuan**: menyediakan primitive untuk shell pipeline, daemon, compositor, dan service bus.

**Komponen**: pipe, eventfd-like, shared memory object, bounded message queue, poll/select/epoll-like readiness.

**Acceptance criteria**: pipeline shell bekerja; backpressure tidak deadlock; poll wakeup tidak lost wakeup; fuzz descriptor invalid lulus.

### M28 — Kernel Observability: Tracepoints, Counters, Crash Dump

**Tujuan**: membuat debugging pasca-kegagalan dapat diulang.

**Komponen**: trace ring buffer, crash dump header, kernel counters, subsystem error taxonomy, support bundle generator.

**Acceptance criteria**: crash dump dapat dianalisis offline; boot, driver, storage, network, GUI memiliki tracepoint minimum.

---

## 9. Hardware Platform dan Driver Track

### M29 — PCI/PCIe Enumeration dan Driver Core

**Tujuan**: menemukan perangkat modern dan mengikat driver secara aman.

**Komponen**: PCI config access, BAR parsing, capability list, MSI/MSI-X discovery, driver registry, probe/remove, resource ownership.

**Acceptance criteria**: QEMU PCI inventory cocok dengan expected manifest; BAR range divalidasi; probe failure membersihkan resource.

### M30 — xHCI USB Host Controller dan USB Core

**Tujuan**: mendukung keyboard, mouse, storage USB, dan perangkat kelas umum.

**Komponen**: USB device model, descriptor parsing, xHCI rings, control/bulk/interrupt transfer, HID keyboard/mouse, mass storage pengayaan.

**Acceptance criteria**: descriptor fuzz tidak panic; keyboard/mouse USB berfungsi di perangkat acuan; hotplug test lulus.

### M31 — NVMe dan AHCI/SATA Storage Driver

**Tujuan**: mengganti RAM block M14 dengan storage driver nyata pada perangkat acuan.

**Komponen**: NVMe admin queue, I/O queue, PRP/SGL minimal, AHCI fallback, block request queue, timeouts, reset path.

**Acceptance criteria**: read-only scan disk lulus; write test hanya pada image/partisi lab; fault injection timeout tidak corrupt kernel.

### M32 — HID/Input Stack

**Tujuan**: menyatukan keyboard PS/2/USB, mouse/touchpad, keymap, repeat, focus event, dan input permission.

**Acceptance criteria**: input event stream deterministic; keymap basic lulus; touchpad/mouse tidak membuat event flood tanpa backpressure.

### M33 — Power, Thermal, Battery, Suspend/Resume

**Tujuan**: membuat laptop tidak sekadar boot, tetapi dapat dipakai sebagai perangkat bergerak.

**Komponen**: ACPI namespace subset, EC/battery query, lid event, brightness, CPU idle, suspend-to-RAM experimental, resume diagnostics.

**Acceptance criteria**: battery/AC status tampil; suspend/resume 50 siklus pada perangkat acuan; data disk tidak korup setelah suspend.

### M34 — Audio Stack Awal

**Tujuan**: mendukung kebutuhan meeting dan multimedia dasar.

**Komponen**: HDA/USB audio path awal, ring buffer audio, mixer, userland audio daemon, permission prompt.

**Acceptance criteria**: playback/record roundtrip; underrun metrics; audio service restart tanpa reboot.

### M35 — Network Device Driver

**Tujuan**: driver virtio-net/e1000 untuk QEMU dan satu NIC referensi.

**Komponen**: RX/TX ring, DMA ownership, checksum policy, interrupt moderation placeholder, link status.

**Acceptance criteria**: ping, TCP transfer, packet loss stress, DMA fault-injection test.

### M36 — Printer/Scanner dan Peripheral Policy

**Tujuan**: dukungan periferal profesional melalui model driver userspace/daemon.

**Komponen**: USB printer class atau IPP-over-network path, scanner SANE-like service pengayaan, permission prompts.

**Acceptance criteria**: print-to-PDF native; IPP network printing pada perangkat acuan; USB printer hanya setelah sandbox driver tersedia.

---

## 10. Storage dan Filesystem Track

### M37 — Page Cache dan Unified Buffer Cache

**Tujuan**: menyatukan VFS, block layer, dan memory manager sehingga read/write file tidak selalu operasi blok langsung.

**Komponen**: page cache, dirty page tracking, writeback policy, readahead, cache invalidation, memory pressure integration.

**Acceptance criteria**: cache coherency test; concurrent read/write test; writeback crash test; memory pressure tidak deadlock.

### M38 — MCSFS2: Directory Tree, Permissions, dan Metadata Versioning

**Tujuan**: mengembangkan MCSFS1J menjadi filesystem yang dapat menyimpan home directory dan konfigurasi user.

**Komponen**: directory bertingkat, inode mode, owner/group, timestamps, link count, rename atomicity, fs versioning.

**Acceptance criteria**: create/unlink/rename/mkdir/rmdir stress; fsck mendeteksi semua metadata corruption yang bisa dibuat kernel.

### M39 — fsck/scrub/recovery Tooling

**Tujuan**: menyediakan recovery offline dan online scrub terbatas.

**Komponen**: userspace `mcsfsck`, image verifier, metadata checksum, orphan recovery, lost+found policy.

**Acceptance criteria**: corruption corpus; fsck tidak memperburuk image; repair idempotent.

### M40 — Snapshot dan System Rollback Storage

**Tujuan**: mendukung update OS yang dapat di-rollback.

**Komponen**: snapshot metadata atau A/B root volume, immutable system image, writable user data separation.

**Acceptance criteria**: failed update rollback; bootloader entry rollback; user data tidak hilang.

### M41 — File Encryption dan Secret Storage

**Tujuan**: melindungi data laptop.

**Komponen**: key hierarchy, per-user encryption, TPM integration pengayaan, recovery key, lock-screen semantics.

**Acceptance criteria**: offline disk image tidak membuka user data tanpa key; key rotation test; recovery workflow terdokumentasi.

### M42 — Filesystem Performance dan Longevity

**Tujuan**: memastikan usability pada pekerjaan nyata.

**Komponen**: metadata benchmark, sequential/random I/O, fsync-heavy workloads, long-haul write test, powercut simulation.

**Acceptance criteria**: 72 jam stress pada image; tidak ada journal replay failure; performance regression threshold.

### M43 — Removable Media dan FAT/exFAT Read-Only Bridge

**Tujuan**: interoperabilitas flash drive.

**Komponen**: partition table parser GPT/MBR, FAT read-only awal, safe mount policy, userspace file copy tool.

**Acceptance criteria**: FAT image corpus; malformed image fail-closed; removable unmount policy.

### M44 — Backup, Restore, dan User Data Migration

**Tujuan**: menyediakan mekanisme kerja profesional yang aman terhadap kegagalan perangkat.

**Komponen**: backup manifest, incremental copy, restore verifier, checksum, encrypted backup pengayaan.

**Acceptance criteria**: restore dry-run; random file corpus; simulated disk failure recovery.

---

## 11. Networking Track

### M45 — Packet Buffer, Checksum, dan Loopback

**Tujuan**: fondasi network stack yang aman terhadap packet malformed.

**Komponen**: mbuf/skbuff-like packet buffer, endian helpers, checksum IPv4/UDP/TCP, loopback interface.

**Acceptance criteria**: parser fuzz; no out-of-bounds; loopback socket tests.

### M46 — Ethernet, ARP, IPv4, ICMP

**Tujuan**: konektivitas dasar pada LAN.

**Acceptance criteria**: ARP exchange, ping Linux/Windows, pcap replay, malformed ARP drop.

### M47 — IPv6, NDP, ICMPv6

**Tujuan**: mendukung jaringan modern berbasis IPv6.

**Acceptance criteria**: SLAAC minimal atau static IPv6; NDP neighbor cache; pcap conformance; invalid extension header handling.

### M48 — UDP, DNS, DHCP

**Tujuan**: mendapatkan alamat jaringan otomatis dan resolusi nama.

**Komponen**: UDP sockets, DHCP client daemon, DNS resolver daemon, `/etc/resolv.conf`-like config.

**Acceptance criteria**: DHCP renewal; DNS cache test; malformed DNS response fail-closed.

### M49 — TCP Subset dan Socket API

**Tujuan**: mendukung web, SSH-like, package manager, dan aplikasi profesional.

**Komponen**: TCP state machine, retransmission, flow control, congestion-control placeholder, listen/connect/accept/send/recv/shutdown.

**Acceptance criteria**: RFC-based state tests, interop dengan Linux, packet loss impairment test, connection churn stress.

### M50 — TLS/Userland Crypto Integration

**Tujuan**: konektivitas aman untuk web, package, update, dan remote services.

**Komponen**: port library TLS yang diaudit, certificate store, trust policy, time validation.

**Acceptance criteria**: TLS handshake test dengan server acuan; invalid certificate rejection; update signing terpisah dari TLS trust.

### M51 — Firewall, Network Namespace, dan Policy

**Tujuan**: mencegah aplikasi mengambil akses jaringan tanpa kebijakan.

**Komponen**: packet filter hooks, per-process network capability, namespace-lite, audit events.

**Acceptance criteria**: denied connection menghasilkan audit log; bypass tests; raw socket restricted.

### M52 — Wi-Fi/Bluetooth Strategy

**Tujuan**: menyusun jalur realistis untuk laptop.

**Keputusan konservatif**: dukungan Wi-Fi/Bluetooth native adalah target tinggi karena firmware, regulatory domain, security, dan driver complexity. Tahap awal memakai Ethernet/USB tethering/USB Wi-Fi tertentu atau userspace driver sandbox.

**Acceptance criteria**: dokumen driver target, firmware policy, regulatory compliance, negative tests. Tidak ada klaim dukungan Wi-Fi umum sebelum hardware matrix tersedia.

---

## 12. Userland, POSIX-like ABI, dan Application Runtime Track

### M53 — libc Minimal dan Syscall Header Stabil

**Tujuan**: menyediakan ABI C untuk program user.

**Komponen**: `libmcs`, startup `crt0`, errno, file I/O, process I/O, memory allocation, time, environment, dynamic loader non-goal awal.

**Acceptance criteria**: program C kecil compile/link/run; syscall ABI versioning; ABI break detector.

### M54 — Shell, Core Utilities, dan Terminal TTY

**Tujuan**: menyediakan workflow profesional CLI.

**Komponen**: TTY, pseudo-terminal, shell minimal, `ls/cat/cp/mv/rm/mkdir/grep/find/ps/kill/top/mount/df` subset.

**Acceptance criteria**: script smoke test; shell pipeline; job control dasar.

### M55 — init dan Service Manager

**Tujuan**: mengelola daemon, login, network, audio, desktop session, dan recovery.

**Komponen**: service unit format sederhana, dependency graph, restart policy, logs, socket activation pengayaan.

**Acceptance criteria**: failed daemon restart; dependency cycle detection; boot parallelism tidak merusak determinisme.

### M56 — Package Manager dan Root Filesystem Layout

**Tujuan**: memasang, menghapus, dan memperbarui aplikasi secara aman.

**Komponen**: package manifest, signature, dependency graph, file ownership DB, post-install policy terbatas, `/usr`, `/etc`, `/var`, `/home`, XDG directories.

**Acceptance criteria**: install/rollback package; file conflict detection; package DB corruption recovery.

### M57 — Dynamic Linking dan Shared Libraries

**Tujuan**: mengurangi ukuran aplikasi dan mendukung porting.

**Komponen**: ELF dynamic loader, relocation subset, symbol resolution, library path policy, ABI versioning.

**Acceptance criteria**: malicious ELF rejection; relocation tests; no arbitrary write through loader.

### M58 — POSIX Compatibility Profile

**Tujuan**: mendefinisikan subset POSIX-like yang terukur.

**Komponen**: conformance matrix terhadap POSIX.1-2024 area prioritas: process, file, directory, terminal, shell utilities, signals, time, pthread subset.

**Acceptance criteria**: setiap interface memiliki status: implemented, partial, unsupported, intentionally different; test suite subset berjalan.

### M59 — Developer Toolchain Native

**Tujuan**: memungkinkan MCSOS membangun sebagian aplikasinya sendiri.

**Komponen**: native compiler port atau cross-to-native pipeline, assembler, linker, make, editor, Git-compatible tool.

**Acceptance criteria**: build aplikasi userland native; reproducibility metadata; sandbox compiler.

### M60 — Application Porting SDK

**Tujuan**: menyediakan dokumentasi dan SDK untuk aplikasi native.

**Komponen**: headers, sysroot, pkg-config-like metadata, samples, ABI checker, app sandbox manifest.

**Acceptance criteria**: aplikasi contoh GUI/CLI dapat dibangun dari SDK bersih.

---

## 13. Security Track

### M61 — Credential Model, UID/GID, DAC, dan Permission

**Tujuan**: multi-user tidak bermakna tanpa identitas dan akses kontrol.

**Komponen**: user database, group, file mode, process credential, setuid non-goal awal, permission check pada VFS, sockets, devices.

**Acceptance criteria**: negative authorization tests; privilege escalation corpus; audit log permission denial.

### M62 — Capability dan Privileged Operation Boundary

**Tujuan**: membatasi operasi root menjadi capability granular.

**Komponen**: capability set per process, privileged syscalls, device access, raw socket, mount, power, network admin.

**Acceptance criteria**: rootless app tidak dapat mount/raw socket; daemon dapat diberi capability minimum.

### M63 — Usercopy, Syscall Fuzzing, dan Kernel Self-Protection

**Tujuan**: memperkuat boundary user/kernel.

**Komponen**: hardened usercopy, guard pages, W^X, NX, KASLR/ASLR roadmap, stack canary, syscall fuzz harness.

**Acceptance criteria**: malformed pointer corpus; no kernel info leak in release logs; fuzzing budget minimum per release.

### M64 — Secure/Measured Boot dan Signed Kernel

**Tujuan**: memastikan chain of trust boot.

**Komponen**: signed boot artifacts, measured boot TPM pengayaan, key enrollment policy, rollback protection, recovery key.

**Acceptance criteria**: unsigned kernel ditolak pada secure profile; rollback attack test; recovery boot works.

### M65 — Secrets, Keyring, dan Certificate Store

**Tujuan**: mendukung TLS, disk encryption, package signing, dan user secrets.

**Acceptance criteria**: secrets tidak terekspos ke proses lain; lock screen clears sensitive tokens; certificate update rollback.

### M66 — Application Sandbox

**Tujuan**: browser, document viewer, media parser, dan aplikasi eksternal berjalan dengan hak minimum.

**Komponen**: syscall filter, file portal, network permission, device permission, per-app storage, broker process.

**Acceptance criteria**: sandbox escape negative tests; document parser tidak dapat membaca `$HOME` tanpa portal.

### M67 — Audit Logging dan Incident Response

**Tujuan**: membuat security event dapat ditelusuri.

**Komponen**: login, privilege, policy denial, package update, network policy, sandbox violation, kernel panic audit.

**Acceptance criteria**: incident reconstruction exercise; log tamper evidence; privacy/redaction policy.

### M68 — Vulnerability Management dan Security Release Process

**Tujuan**: membentuk proses respons kerentanan.

**Komponen**: CVE intake, severity model, advisory template, patch branch, backport policy, security test gate.

**Acceptance criteria**: mock incident drill; emergency update rollback; known-risk disclosure.

---

## 14. Graphics, Desktop, dan Human Interface Track

### M69 — Framebuffer Console dan Font Renderer

**Tujuan**: transisi dari serial/log ke tampilan lokal yang dapat dipakai.

**Komponen**: framebuffer mapping, text console, Unicode subset, font cache, panic screen fallback.

**Acceptance criteria**: panic readable on screen; serial remains canonical diagnostic channel.

### M70 — DRM/KMS-like Display Model

**Tujuan**: model display yang siap untuk mode setting, buffer, plane, connector, CRTC, vblank.

**Komponen**: simple framebuffer driver, EDID parser, page flip, vblank counter, fallback mode.

**Acceptance criteria**: mode switch test; wrong EDID fail-safe; visual regression screenshot.

### M71 — Compositor dan Windowing Protocol

**Tujuan**: membuat GUI multi-aplikasi.

**Komponen**: surface, buffer, seat/input, output, clipboard, data transfer, window roles, damage tracking.

**Acceptance criteria**: dua aplikasi GUI berjalan; focus/input isolation; compositor crash restart policy.

### M72 — Input Method, Accessibility, dan Internationalization

**Tujuan**: memenuhi kebutuhan pengguna profesional lintas bahasa.

**Komponen**: keymap, IME, clipboard unicode, screen scaling, high contrast, keyboard navigation.

**Acceptance criteria**: keyboard-only workflow; font fallback; input method test.

### M73 — Desktop Shell, Launcher, File Manager

**Tujuan**: menyediakan lingkungan kerja dasar.

**Komponen**: session manager, app launcher, `.desktop`-like entries, MIME registry, file manager, settings.

**Acceptance criteria**: app launch from menu; MIME open-with; file operations use VFS permissions.

### M74 — Terminal Emulator GUI dan Text Editor

**Tujuan**: workflow pengembangan dan administrasi.

**Komponen**: pseudo-terminal, terminal rendering, clipboard integration, editor with save/recovery.

**Acceptance criteria**: build project from GUI terminal; crash recovery for unsaved editor buffer.

### M75 — Multimedia Framework

**Tujuan**: audio/video playback, meeting path, and screen recording long-term.

**Komponen**: audio graph, video frame transport, device permission, low-latency policy, media service.

**Acceptance criteria**: audio playback while CPU load; capture permission; underrun metrics.

### M76 — Printing, PDF, dan Document Workflow

**Tujuan**: kebutuhan profesional dokumen.

**Komponen**: PDF viewer, print spooler, IPP client, document preview, file association.

**Acceptance criteria**: print-to-PDF, open large PDF, sandboxed viewer.

### M77 — Browser Strategy

**Tujuan**: web adalah kebutuhan profesional utama, tetapi browser modern adalah subsistem besar.

**Strategi bertahap**:

1. Web documentation viewer sederhana untuk HTML statis.
2. Port browser ringan dengan TLS, font, input, sandbox dasar.
3. Long-term port Chromium/Firefox-like hanya setelah POSIX, threading, mmap, shared memory, GPU, network, file sandbox, JIT policy, dan process isolation matang.

**Acceptance criteria**: web compatibility matrix, TLS validation, process sandbox, crash isolation. Tidak ada klaim “browser modern penuh” sebelum test suite dan sandbox evidence tersedia.

### M78 — Desktop Usability Study dan UX Hardening

**Tujuan**: menilai apakah sistem dapat digunakan manusia, bukan hanya boot.

**Komponen**: task completion tests, error message review, accessibility review, recovery flows.

**Acceptance criteria**: minimal 20 skenario pengguna; bug triage; UX regression list.

---

## 15. Application Ecosystem Track

### M79 — Native Application Set Minimum

**Aplikasi minimum**: terminal, editor teks, file manager, settings, PDF viewer, image viewer, media player, archive manager, package manager GUI/CLI, system monitor.

**Acceptance criteria**: semua aplikasi berjalan sebagai user non-root; setiap aplikasi memiliki manifest, permission, crash handling, dan update path.

### M80 — Office Workflow Bridge

**Strategi realistis**: jangan mengklaim kompatibilitas office suite penuh sejak awal. Target bertahap adalah membuka/mengekspor format sederhana, PDF workflow, dan kemudian port office suite open-source melalui POSIX compatibility profile.

**Acceptance criteria**: dokumen roundtrip corpus; PDF export; file locking; font rendering.

### M81 — Developer Workflow

**Tujuan**: MCSOS dapat dipakai untuk mengembangkan aplikasi MCSOS.

**Komponen**: SDK, editor, terminal, compiler, debugger, build tools, source control.

**Acceptance criteria**: membangun aplikasi native dari MCSOS sendiri; debug user process; crash dump symbolization.

### M82 — Cloud/Remote Work Tools

**Tujuan**: kebutuhan profesional modern sering berbasis layanan jaringan.

**Komponen**: TLS, certificate store, WebDAV/SFTP-like client, sync conflict policy, remote terminal.

**Acceptance criteria**: file sync simulation; credential isolation; offline conflict resolution.

### M83 — Compatibility Layer Decision

**Pilihan**:

1. POSIX source-port compatibility sebagai jalur utama.
2. Linux syscall compatibility subset sebagai eksperimen jangka panjang.
3. VM/container untuk menjalankan Linux userland sebagai fallback developer.

**Keputusan awal**: prioritaskan source-port POSIX dan container/VM developer sandbox. Binary compatibility Linux penuh bukan target awal.

### M84 — App Store/Repository dan Trust Policy

**Tujuan**: instalasi aplikasi dengan rantai kepercayaan.

**Komponen**: signed repository metadata, package provenance, dependency policy, rollback, vulnerability database.

**Acceptance criteria**: tampered package rejected; downgrade attack rejected; package rollback test.

---

## 16. Virtualization dan Container Track

### M85 — Resource Control dan Namespace-lite

**Tujuan**: isolasi aplikasi dan daemon.

**Komponen**: process namespace, mount namespace-lite, network namespace-lite, cgroup-like CPU/memory/I/O limits.

**Acceptance criteria**: resource exhaustion test; process cannot escape namespace via fd leak.

### M86 — Container Runtime Subset

**Tujuan**: developer sandbox dan service isolation.

**Komponen**: image unpack, rootfs mount, process isolation, capability drop, seccomp-like filter.

**Acceptance criteria**: OCI-inspired manifest subset; escape negative tests; resource accounting.

### M87 — Hypervisor/VMM Feasibility

**Tujuan**: menjalankan guest kecil atau Linux VM sebagai compatibility bridge long-term.

**Komponen**: VMX/SVM discovery, vCPU object, EPT/NPT plan, virtio device plan.

**Acceptance criteria**: feasibility report; no default inclusion until security boundary reviewed.

### M88 — Virtio-first Strategy

**Tujuan**: memaksimalkan QEMU/cloud/testing sebelum hardware nyata.

**Komponen**: virtio-blk, virtio-net, virtio-gpu, virtio-input, virtio-rng.

**Acceptance criteria**: virtio test matrix; malformed descriptor fail-closed; DMA/IOMMU assumptions explicit.

---

## 17. Release Engineering, Installer, dan Operations Track

### M89 — Reproducible Release Image

**Tujuan**: menghasilkan image yang dapat dibangun ulang byte-identical atau nondeterminism terdokumentasi.

**Komponen**: build manifest, SBOM, compiler version pinning, source date epoch, checksum, signing.

**Acceptance criteria**: clean-room rebuild; artifact signature verification; provenance bundle.

### M90 — Installer dan Partitioning

**Tujuan**: instalasi ke perangkat acuan tanpa merusak data pengguna.

**Komponen**: live installer, disk selection, GPT/ESP handling, encrypted home, rollback partition, confirmation UX.

**Acceptance criteria**: installer dry-run; destructive action confirmation; recovery media.

### M91 — Update and Rollback System

**Tujuan**: update tidak boleh membuat laptop tidak bisa boot.

**Komponen**: A/B system image atau immutable root + snapshot, signed update, staged rollout, health check, rollback trigger.

**Acceptance criteria**: simulated failed update; automatic rollback; user data preserved.

### M92 — Crash Reporting dan Support Bundle

**Tujuan**: masalah pengguna dapat didiagnosis tanpa akses manual ke kernel debugger.

**Komponen**: crash dump, kernel logs, hardware inventory, package list, privacy redaction, opt-in export.

**Acceptance criteria**: support bundle contains enough reproduction data without leaking secrets.

### M93 — Performance, Power, dan Reliability Benchmark

**Tujuan**: mengukur penggunaan nyata.

**Komponen**: boot time, app launch, file copy, network throughput, suspend battery drain, idle power, memory pressure, 99th percentile latency.

**Acceptance criteria**: benchmark protocol reproducible; regression threshold; confidence intervals where applicable.

### M94 — Hardware Certification Matrix

**Tujuan**: menyatakan dukungan hanya pada perangkat yang diuji.

**Tingkat dukungan**:

| Level | Definisi |
|---|---|
| H0 | QEMU-only |
| H1 | Reference desktop boots and runs CLI |
| H2 | Reference laptop boots, storage/network/input works |
| H3 | Desktop GUI usable on reference laptop |
| H4 | Daily-use pilot on reference laptop for 30 days |
| H5 | Expanded hardware matrix with known quirks |

### M95 — Documentation, Runbook, dan Education-to-Engineering Transition

**Tujuan**: memisahkan modul praktikum dari dokumentasi developer preview.

**Komponen**: developer docs, user docs, install guide, recovery guide, architecture guide, API reference.

**Acceptance criteria**: new contributor can build and run QEMU from clean checkout; user can recover failed update using documented steps.

### M96 — Legal, Licensing, dan Third-Party Policy

**Tujuan**: memastikan penggunaan library, driver, firmware, dan aplikasi sesuai lisensi.

**Komponen**: license inventory, firmware redistribution policy, patent-risk notes for codecs, export-control awareness for crypto.

**Acceptance criteria**: release blocked if license unknown; SBOM includes third-party components.

### M97 — Privacy dan Data Governance

**Tujuan**: logs, telemetry, crash dump, dan support bundle tidak melanggar privasi.

**Komponen**: local-first telemetry, opt-in export, redaction rules, data retention, audit event classification.

**Acceptance criteria**: privacy review before developer preview.

### M98 — Developer Preview Gate

**Tujuan**: menilai apakah MCSOS layak dirilis sebagai developer preview terbatas.

**Minimal evidence**: QEMU pass, reference laptop pass, signed image, rollback pass, filesystem crash pass, network pass, GUI desktop pass, security negative tests, support docs, known issues.

### M99 — Limited Professional Pilot Gate

**Tujuan**: menilai apakah MCSOS dapat digunakan untuk pekerjaan profesional terbatas oleh pengguna teknis internal.

**Minimal evidence**: 30-day pilot, no unrecoverable data loss, daily tasks pass, backup/restore pass, security review pass, update rollback pass, known limitations disclosed.

### M100 — Post-Pilot Roadmap

**Tujuan**: menyusun roadmap setelah pilot berdasarkan evidence, bukan asumsi.

**Output**: bug taxonomy, performance data, security findings, hardware gaps, application gaps, user feedback, go/no-go decision.

---

## 18. Dependency Graph Strategis

```text
M17 integration
  -> M18 CI/fuzz
  -> M19 hardware abstraction
  -> M21 APIC/modern timer
  -> M22 SMP/per-CPU
  -> M23 scheduler
  -> M24 process lifecycle
  -> M26 mmap/page fault/COW
  -> M53 libc
  -> M54 shell/coreutils
  -> M55 service manager
  -> M56 package manager

M29 PCIe/driver core
  -> M31 NVMe/AHCI
  -> M37 page cache
  -> M38 MCSFS2
  -> M40 snapshot/rollback
  -> M91 update rollback

M29 PCIe/driver core
  -> M30 xHCI USB
  -> M32 input
  -> M69 framebuffer
  -> M70 KMS-like
  -> M71 compositor
  -> M73 desktop shell

M35 NIC
  -> M45 packet buffer
  -> M46 IPv4/ARP/ICMP
  -> M47 IPv6/NDP
  -> M48 UDP/DHCP/DNS
  -> M49 TCP/sockets
  -> M50 TLS
  -> M77 browser strategy

M61 credentials
  -> M62 capability
  -> M63 syscall fuzz/usercopy
  -> M66 app sandbox
  -> M79 native apps
  -> M84 app repository
```

---

## 19. Verification Matrix Utama

| Requirement | Subsystem owner | Evidence minimum | Gate blocker |
|---|---|---|---|
| Clean checkout build | Toolchain | CI log, checksum, build manifest | ya |
| QEMU boot deterministic | Boot/kernel | serial log, boot trace | ya |
| Reference hardware boot | Platform | boot log, hardware inventory | ya untuk pilot |
| No hidden libc in kernel | Low-level | `nm -u`, linker map | ya |
| Syscall rejects malformed args | Kernel/security | fuzz log, negative tests | ya |
| Filesystem recovers after crash | FS/storage | powercut simulation, fsck log | ya |
| Update can rollback | Release/storage | failed-update drill | ya |
| Network interop works | Networking | pcap, Linux/Windows interop | ya untuk desktop |
| GUI runs unprivileged apps | Graphics/security | sandbox tests, compositor logs | ya |
| User data protected | Security/storage | encryption test, backup test | ya |
| Power management works on laptop | Hardware/enterprise | suspend/resume loop | ya untuk laptop claim |
| Crash is diagnosable | Observability | support bundle, crash dump | ya |
| Application install is trusted | Package/security | signature/provenance log | ya |
| Documentation reproduces setup | Docs/education | fresh-user build test | ya |

---

## 20. Test Campaigns

### 20.1 Unit dan host tests

Setiap struktur data kernel harus memiliki host unit test jika dapat dipisahkan dari hardware: allocator, VMM model, scheduler queue, syscall dispatcher, ELF parser, VFS, MCSFS2, journal, packet parser, DNS parser, package manifest parser, desktop entry parser.

### 20.2 Emulator tests

QEMU matrix wajib mencakup: `-smp 1`, `-smp 2`, virtio storage, virtio net, e1000, USB keyboard/mouse, framebuffer, low memory, high memory, ACPI on/off jika memungkinkan, malformed disk image, network packet replay.

### 20.3 Hardware tests

Hardware acuan minimal: satu desktop/mini-PC dan satu laptop. Test wajib: boot 100 siklus, suspend/resume 50 siklus, file copy 100 GB pada disk test, network throughput, audio playback, input devices, display mode changes, battery drain idle, thermal load.

### 20.4 Fault injection

Fault injection wajib mencakup: disk write failure, journal corrupt, partial update, driver timeout, packet malformed, USB descriptor malformed, ELF malformed, package signature invalid, ACPI table corrupt, low memory, process fork bomb, app sandbox violation.

### 20.5 Fuzzing

Fuzz target prioritas: syscall ABI, ELF loader, filesystem image parser, journal replay, network packet parser, DNS parser, USB descriptors, ACPI parser, desktop entry parser, package manifest parser.

### 20.6 Security tests

Negative tests: privilege bypass, raw device access, file permission violation, invalid user pointer, kernel info leak, sandbox escape, package downgrade, unsigned update, tampered boot artifact, secrets exfiltration.

---

## 21. Readiness Gates OS_next

| Gate | Nama | Kriteria ringkas |
|---|---|---|
| N0 | Integrated educational kernel | M0-M16 menyatu, build/test/audit lulus |
| N1 | Emulator developer kernel | QEMU boot, SMP basic, process/syscall, VFS/storage basic |
| N2 | Reference hardware bring-up | UEFI/ACPI/PCIe/storage/input/network dasar pada perangkat acuan |
| N3 | CLI userland preview | shell, libc, coreutils, package bootstrap, service manager |
| N4 | Storage-safe preview | filesystem crash recovery, fsck, snapshot/rollback, backup |
| N5 | Networked workstation preview | TCP/IP, DNS/DHCP/TLS, firewall, update repository |
| N6 | Desktop demonstration | GUI shell, compositor, input, file manager, terminal GUI |
| N7 | Security-reviewed developer preview | credentials, permissions, sandbox, signed update, audit |
| N8 | Reference laptop daily-use pilot | 30-day pilot, no unrecoverable data loss, known issues disclosed |
| N9 | Expanded hardware preview | multiple devices, driver matrix, power/thermal notes |
| N10 | Candidate professional limited use | release process, support, rollback, security response, app workflow |

---

## 22. Minimum Acceptance Criteria Sebelum Klaim “Kandidat Penggunaan Profesional Terbatas”

1. Build reproducible dari clean checkout pada toolchain yang dipin.
2. QEMU matrix lulus otomatis.
3. Minimal satu laptop referensi lulus boot, storage, display, input, network, suspend/resume, audio, dan update tests.
4. Filesystem lulus crash-consistency campaign dan fsck/scrub campaign.
5. Signed update dan rollback lulus minimal 50 siklus update/failure injection.
6. User/kernel isolation, syscall fuzzing, sandbox negative tests, dan permission tests lulus.
7. Desktop shell dapat menjalankan minimal aplikasi native: terminal, editor, file manager, PDF viewer, settings, system monitor.
8. Networking mendukung DHCP, DNS, TCP, TLS, dan package repository access.
9. Backup/restore workflow lulus pada dataset pengguna simulasi.
10. Crash dump dan support bundle cukup untuk reproduksi bug tanpa membocorkan secrets.
11. Dokumentasi instalasi, recovery, known issues, dan hardware support tersedia.
12. Seluruh residual risk didokumentasikan dan disetujui dalam readiness review.

---

## 23. Risiko Kritis dan Mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Scope terlalu besar | roadmap tidak selesai | hardware acuan sempit, gate ketat, non-goals eksplisit |
| Data loss filesystem | kegagalan paling serius untuk penggunaan profesional | crash testing, journal hardening, fsck, snapshot, backup |
| Driver DMA corrupt memory | kernel crash/data loss/security issue | IOMMU plan, DMA ownership model, sandbox driver long-term |
| Browser terlalu kompleks | target desktop tidak realistis | staged web strategy, sandbox, port lightweight first |
| Wi-Fi/Bluetooth firmware complexity | laptop usability terbatas | Ethernet/USB tethering fallback, target chipset sempit |
| GUI demo tanpa security | aplikasi dapat mengakses data bebas | sandbox/portal sebelum app ecosystem luas |
| Update bricking | perangkat tidak bisa boot | A/B image, signed rollback, recovery media |
| POSIX ambiguity | aplikasi porting gagal | compatibility profile dan conformance matrix |
| Debuggability lemah | bug hardware sulit dilacak | tracepoints, crash dump, support bundle |
| Security postponed | desain sulit diperbaiki | secure-by-design gate sejak M61 dan policy lint sejak awal |

---

## 24. Struktur Repository Lanjutan yang Disarankan

```text
mcsos/
├── arch/
│   └── x86_64/
├── boot/
│   ├── limine/
│   └── uefi/
├── kernel/
│   ├── core/
│   ├── mm/
│   ├── sched/
│   ├── syscall/
│   ├── ipc/
│   ├── fs/
│   ├── net/
│   ├── security/
│   ├── drivers/
│   └── trace/
├── drivers/
│   ├── pci/
│   ├── usb/
│   ├── storage/
│   ├── net/
│   ├── input/
│   ├── gpu/
│   └── audio/
├── user/
│   ├── libc/
│   ├── init/
│   ├── shell/
│   ├── coreutils/
│   ├── services/
│   ├── desktop/
│   └── apps/
├── sdk/
├── pkg/
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── fuzz/
│   ├── qemu/
│   ├── hardware/
│   └── conformance/
├── tools/
│   ├── build/
│   ├── image/
│   ├── debug/
│   ├── release/
│   └── support/
├── docs/
│   ├── architecture/
│   ├── adr/
│   ├── api/
│   ├── security/
│   ├── readiness/
│   ├── user-guide/
│   └── teaching/
└── evidence/
    ├── ci/
    ├── qemu/
    ├── hardware/
    ├── fuzz/
    ├── security/
    └── release/
```

---

## 25. Kebijakan Dokumentasi dan Laporan untuk Tahap Lanjut

Setiap milestone M17-M100 harus memiliki dokumen:

1. Judul dan identitas akademik.
2. Assumptions and target.
3. Goals dan non-goals.
4. Design contract.
5. Kernel/user/hardware/security invariants.
6. Build steps.
7. Test steps.
8. Evidence matrix.
9. Failure modes.
10. Rollback plan.
11. Security review.
12. Performance/reliability notes.
13. Readiness review.
14. References IEEE.

Untuk milestone yang menyentuh driver, filesystem, network, security, update, atau boot, laporan wajib memuat fault-injection evidence. Untuk milestone desktop/app, laporan wajib memuat usability scenario dan crash recovery.

---

## 26. Roadmap Implementasi 12 Bulan, 24 Bulan, dan 36 Bulan

### 12 bulan pertama: developer kernel dan CLI preview

Target realistis: M17-M28, sebagian M29-M31, M53-M55. Output: QEMU dan satu hardware acuan dapat boot sampai shell CLI, process lifecycle bekerja, storage dasar bekerja pada disk test, network virtio/e1000 awal, dan update belum untuk pengguna umum.

### 24 bulan: hardware reference workstation dan desktop demo

Target realistis: M29-M52, M56-M60, M61-M66, M69-M74. Output: satu perangkat acuan dapat menjalankan desktop sederhana, terminal GUI, file manager, network, package install, dan sandbox dasar.

### 36 bulan: limited professional pilot

Target realistis: M75-M100 dengan scope perangkat sangat terbatas. Output: pilot internal teknis untuk pekerjaan ringan: teks, terminal, file, jaringan, PDF, editor, package update, backup/restore. Browser dan office suite penuh tetap bergantung pada porting dan sandbox maturity.

---

## 27. Readiness Review Akhir Dokumen

Berdasarkan baseline M0-M16, MCSOS saat ini layak diposisikan sebagai **fondasi pendidikan siap uji QEMU dan host-test terbatas**, bukan sistem operasi desktop/laptop siap pakai. OS_next menetapkan jalur menuju sistem operasi modern dengan kebutuhan profesional, tetapi klaim penggunaan profesional hanya sah setelah lulus gate N8-N10.

**Status yang direkomendasikan setelah OS_next diadopsi**: *roadmap tahap lanjut siap digunakan untuk perencanaan pengembangan dan penyusunan modul M17+*.  
**Status yang tidak boleh diklaim**: *MCSOS siap menggantikan Windows/Linux/macOS*, *MCSOS siap produksi*, atau *MCSOS bebas error*.  
**Keputusan lanjut**: mulai dari M17 dengan repository integration, CI, regression baseline, dan QEMU smoke berulang sebelum menulis fitur baru.

---

## References

[1] Muhaemin Sidiq, “Panduan Praktikum M0 — Baseline Requirements, Governance, dan Lingkungan Pengembangan Reproducible MCSOS 260502,” Institut Pendidikan Indonesia, 2026.  
[2] Muhaemin Sidiq, “Panduan Praktikum M1 — Toolchain Reproducible dan Pemeriksaan Kesiapan Lingkungan Pengembangan MCSOS 260502,” Institut Pendidikan Indonesia, 2026.  
[3] Muhaemin Sidiq, “Panduan Praktikum M2 — Boot Image, Kernel ELF64, Early Serial Console, dan Readiness Gate M2 MCSOS 260502,” Institut Pendidikan Indonesia, 2026.  
[4] Muhaemin Sidiq, “Panduan Praktikum M3 — Panic Path, Kernel Logging, GDB Debug Workflow, Linker Map, dan Disassembly Audit MCSOS 260502,” Institut Pendidikan Indonesia, 2026.  
[5] Muhaemin Sidiq, “Panduan Praktikum M4 — Interrupt Descriptor Table, Exception Trap Path, Trap Frame, dan Fault-Handling Awal MCSOS 260502,” Institut Pendidikan Indonesia, 2026.  
[6] Muhaemin Sidiq, “Panduan Praktikum M5 — External Interrupt, Legacy PIC Remap, dan PIT Timer Tick pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[7] Muhaemin Sidiq, “Panduan Praktikum M6 — Physical Memory Manager, Boot Memory Map, dan Bitmap Frame Allocator pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[8] Muhaemin Sidiq, “Panduan Praktikum M7 — Virtual Memory Manager Awal, Page Table x86_64, dan Page Fault Diagnostics pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[9] Muhaemin Sidiq, “Panduan Praktikum M8 — Kernel Heap Awal, Allocator Dinamis, Validasi Invariant, dan Integrasi Bertahap dengan PMM/VMM pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[10] Muhaemin Sidiq, “Panduan Praktikum M9 — Kernel Thread, Runqueue Round-Robin Kooperatif, Context Switch x86_64, dan Integrasi Scheduler Awal pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[11] Muhaemin Sidiq, “Panduan Praktikum M10 — ABI System Call Awal, Dispatcher Syscall, Validasi Argumen, dan Jalur int 0x80 Terkendali pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[12] Muhaemin Sidiq, “Panduan Praktikum M11 — ELF64 User Program Loader Awal, Process Image Plan, User Address-Space Contract, dan Kesiapan Transisi Userspace pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[13] Muhaemin Sidiq, “Panduan Praktikum M12 — Sinkronisasi Kernel Awal: Spinlock, Mutex Kooperatif, Lock-Order Validator, dan Diagnosis Race/Deadlock pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[14] Muhaemin Sidiq, “Panduan Praktikum M13 — VFS Minimal, File Descriptor Table, RAMFS, dan Syscall File I/O Awal pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[15] Muhaemin Sidiq, “Panduan Praktikum M14 — Block Device Layer, RAM Block Driver, Buffer Cache Minimal, dan Jalur Persiapan Filesystem Persistent pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[16] Muhaemin Sidiq, “Panduan Praktikum M15 — Filesystem Persistent Minimal MCSFS1, On-Disk Superblock/Inode/Directory, dan Fsck-Lite pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[17] Muhaemin Sidiq, “Panduan Praktikum M16 — Crash Consistency, Write-Ahead Journal, Recovery, dan Fault-Injection Test untuk MCSFS1J pada MCSOS,” Institut Pendidikan Indonesia, 2026.  
[18] IEEE Standards Association, “IEEE 1003.1-2024: IEEE/Open Group Standard for Information Technology—Portable Operating System Interface (POSIX™) Base Specifications, Issue 8,” IEEE, 2024. [Online]. Available: https://standards.ieee.org/ieee/1003.1/7700/  
[19] The Open Group, “The Single UNIX Specification, Version 5 (2024),” The Open Group, 2024. [Online]. Available: https://www.unix.org/overview.html  
[20] UEFI Forum, “Specifications: Latest Versions of the UEFI Specifications,” UEFI Forum, 2026. [Online]. Available: https://uefi.org/specifications  
[21] UEFI Forum, “Preexisting ACPI Specifications,” UEFI Forum. [Online]. Available: https://uefi.org/acpi/specs  
[22] Linux Kernel Documentation, “Overview of the Linux Virtual File System,” Linux kernel documentation. [Online]. Available: https://docs.kernel.org/filesystems/vfs.html  
[23] Linux Kernel Documentation, “Journal (jbd2),” Linux ext4 documentation. [Online]. Available: https://www.kernel.org/doc/html/latest/filesystems/ext4/journal.html  
[24] Linux Kernel Documentation, “Dynamic DMA mapping using the generic device,” Linux kernel documentation. [Online]. Available: https://docs.kernel.org/core-api/dma-api.html  
[25] J. Postel, “Internet Protocol,” RFC 791, IETF, Sep. 1981. [Online]. Available: https://datatracker.ietf.org/doc/rfc791/  
[26] S. Deering and R. Hinden, “Internet Protocol, Version 6 (IPv6) Specification,” RFC 8200, IETF, Jul. 2017. [Online]. Available: https://datatracker.ietf.org/doc/html/rfc8200  
[27] W. Eddy, Ed., “Transmission Control Protocol (TCP),” RFC 9293, IETF, Aug. 2022. [Online]. Available: https://datatracker.ietf.org/doc/html/rfc9293  
[28] D. C. Plummer, “An Ethernet Address Resolution Protocol,” RFC 826, IETF, Nov. 1982. [Online]. Available: https://datatracker.ietf.org/doc/html/rfc826  
[29] freedesktop.org, “XDG Base Directory Specification, Version 0.8,” 2021. [Online]. Available: https://specifications.freedesktop.org/basedir/latest/  
[30] freedesktop.org, “Desktop Entry Specification, Version 1.5,” 2020. [Online]. Available: https://specifications.freedesktop.org/desktop-entry-spec/latest/  
[31] D-Bus Project, “D-Bus Specification, Version 0.43,” freedesktop.org, 2024. [Online]. Available: https://dbus.freedesktop.org/doc/dbus-specification.html  
[32] Wayland Project, “Wayland Protocol Specification,” freedesktop.org. [Online]. Available: https://wayland.freedesktop.org/docs/html/apa.html  
[33] PipeWire Project, “PipeWire Documentation,” PipeWire, 2026. [Online]. Available: https://docs.pipewire.org/  
[34] Khronos Group, “Vulkan Specification,” Vulkan Documentation Project. [Online]. Available: https://docs.vulkan.org/spec/latest/chapters/introduction.html  
[35] M. Souppaya, K. Scarfone, and D. Dodson, “Secure Software Development Framework (SSDF) Version 1.1: Recommendations for Mitigating the Risk of Software Vulnerabilities,” NIST SP 800-218, Feb. 2022. [Online]. Available: https://doi.org/10.6028/NIST.SP.800-218  
[36] Cybersecurity and Infrastructure Security Agency, “Secure by Design,” CISA. [Online]. Available: https://www.cisa.gov/securebydesign  
[37] OpenSSF, “Supply-chain Levels for Software Artifacts (SLSA),” Open Source Security Foundation. [Online]. Available: https://openssf.org/projects/slsa/  
[38] OpenTelemetry, “OpenTelemetry Logging,” OpenTelemetry Specification. [Online]. Available: https://opentelemetry.io/docs/specs/otel/logs/
