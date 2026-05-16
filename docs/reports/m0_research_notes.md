# Laporan Tantangan Riset M0 - MCSOS

## 1. Perbandingan Output Object: Clang vs GCC
* Clang (`--target=x86_64-unknown-none`) menghasilkan struktur kode yang lebih ringkas secara default pada optimasi `-O1` dibanding GCC (`x86_64-elf-gcc`).
* Clang memiliki parser bawaan yang langsung mengenali target arsitektur tanpa perlu kompilasi silang toolchain mandiri yang rumit di awal.

## 2. Analisis Red-Zone (Bukti Objdump)
* Berdasarkan uji coba `objdump`, flag `-mno-red-zone` menambahkan instruksi `sub $0x18,%rsp` (prolog) dan `add $0x18,%rsp` (epilog).
* Hal ini wajib untuk keamanan kernel guna mencegah handler interupsi merusak memori runtunan (stack) lokal fungsi.

## 3. Policy Reproducible Build
Untuk memastikan kompilasi menghasilkan biner byte-demi-byte yang identik di komputer mana pun, kebijakan kompilasi wajib menggunakan kontrol berikut:
* **Kontrol Jalur Debug:** Menggunakan flag `-fdebug-prefix-map==.` atau `-Xclang -fdebug-compilation-dir -Xclang .` agar direktori absolut laptop pengguna tidak masuk ke dalam berkas biner.
* **Kontrol Waktu (Timestamp):** Menyematkan variabel lingkungan `SOURCE_DATE_EPOCH` saat proses pembuatan objek biner makro agar nilai `__DATE__` dan `__TIME__` bernilai statis.

## 4. Threat Model Supply-Chain Toolchain
* **Ancaman:** Modifikasi skrip build otomatis oleh pihak ketiga atau penyusupan biner jahat pada paket kompiler distro.
* **Mitigasi:** Membakukan verifikasi nilai kode hash (SHA-256) untuk setiap source archive GCC/Clang yang diunduh sebelum proses kompilasi silang dimulai.
