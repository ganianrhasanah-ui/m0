# Proyek: MCSOS 260502 - Makefile Subset M1
CC = clang
LD = ld.lld

.PHONY: meta clean

meta:
	mkdir -p build/meta
	uname -a > build/meta/host-readiness.txt
	$(CC) --version > build/meta/toolchain-versions.txt
	$(LD) --version >> build/meta/toolchain-versions.txt
	qemu-system-x86_64 --version >> build/meta/toolchain-versions.txt

clean:
	rm -rf build/
