obj-m += dmesg_injector.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# ---- EFI binary variant (needs no extra packages) ----
efi: dmesg_injector_efi.efi

dmesg_injector_efi.so: dmesg_injector_efi.c efi.lds
	gcc -m64 -mno-red-zone -mno-mmx -mno-sse -ffreestanding \
		-fshort-wchar -fpic -O2 -Wall \
		-c -o dmesg_injector_efi.o dmesg_injector_efi.c
	ld -nostdlib -shared -o $@ dmesg_injector_efi.o \
		-T efi.lds -e efi_main

dmesg_injector_efi.efi: dmesg_injector_efi.so
	objcopy -O pei-x86-64 --subsystem=10 \
		dmesg_injector_efi.so dmesg_injector_efi.efi
	ls -lh $@
	@echo "EFI binary ready: $@"

# ---- Go version ----
go: dmesg_injector
dmesg_injector: dmesg_injector.go
	go build -o $@ $<
	ls -lh $@
