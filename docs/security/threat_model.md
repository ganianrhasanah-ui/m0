# Threat Model - MCSOS 260502
- **Aset Utama:** Kernel Space Memory Integrity, Page Tables.
- **Ancaman M1/M2:** Stack overflow pada early execution, rusaknya stack akibat Red Zone.
- **Mitigasi:** Penggunaan flag `-mno-red-zone` secara ketat.
