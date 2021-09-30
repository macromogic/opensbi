CC = riscv64-unknown-linux-gnu-gcc 
OBJCOPY = riscv64-unknown-linux-gnu-objcopy

CFLAGS = -nostdlib -static -mcmodel=medany -g -O0 -I../util -I../../include

link_script = drv_console.lds
headers = drv_console.h 
src = drv_console.c 

target_dir := ../../build/emodules/drv_console

all: dir $(target_dir)/drv_console.bin

dir:
	mkdir -p $(target_dir)

$(target_dir)/drv_console.bin: $(target_dir)/drv_console
	$(OBJCOPY) -O binary --set-section-flags .bss=alloc,load,contents $< $@

$(target_dir)/drv_console: $(src) $(headers) $(link_script)
	$(CC) $(CFLAGS) $(src) -T $(link_script) -o $@

.PHONY: all dir