CC = riscv64-unknown-linux-gnu-gcc 
OBJCOPY = riscv64-unknown-linux-gnu-objcopy

CFLAGS = -nostdlib -static -mcmodel=medany -g -O0 -I../../include

link_script = drv_base.lds
headers = drv_base.h drv_elf.h drv_handler.h drv_list.h drv_mem.h drv_syscall.h drv_util.h mm/*.h md2.h
src = drv_base.c drv_elf.c drv_handler.c drv_list.c drv_mem.c drv_syscall.c drv_util.c drv_entry.S mm/*.c md2.c

target_dir := ../../build/emodules/emodule_base

all: dir $(target_dir)/emodule_base.bin

dir:
	mkdir -p $(target_dir)

$(target_dir)/emodule_base.bin: $(target_dir)/emodule_base
	$(OBJCOPY) -O binary --set-section-flags .bss=alloc,load,contents $< $@

$(target_dir)/emodule_base: $(src) $(headers) $(link_script)
	$(CC) $(CFLAGS) $(src) -T $(link_script) -o $@
