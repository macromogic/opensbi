# Coffer: A software-based TEE architecture on RISC-V

## Installation

1. Install [RISC-V GNU Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain).
1. Install [U-boot](https://github.com/u-boot/u-boot).
1. Clone this project.
1. Refer to [OpenSBI instructions](README_OpenSBI.md) for compilation and installation.

## Quick start

The enclave functionalities are implemented by `ecall` with `a7=0x19260817` and `a6=<funcid>`.

Available funcids are:

| Function Name  | Funcid |                      Arguments                      |  Returns   |
| :------------: | :----: | :-------------------------------------------------: | :--------: |
| Create enclave |  399   | `a0=<payload_addr> a1=<payload_size> a2=<drv_mask>` | Enclave ID |
| Enter enclave  |  400   |                  `a0=<enclave-id>`                  |            |
|  Exit enclave  |  401   |          `a0=<enclave-id> a1=<return-val>`          |            |

## Running in QEMU

```bash
qemu-system-riscv64 -M virt -nographic -m 8G -smp <num-cores> \
    -bios build/platform/<platform>/firmware/fw_jump.elf \
    -kernel <path-to-linux-image> \
    -drive file=<rootfs-file>,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -append "root=/dev/vda rw console=ttyS0 earlycon=sbi"
```
