# RISC-V Open Source Supervisor Binary Interface (OpenSBI)

## Copyright and License

The OpenSBI project is copyright (c) 2019 Western Digital Corporation
or its affiliates and other contributors.

It is distributed under the terms of the BSD 2-clause license
("Simplified BSD License" or "FreeBSD License", SPDX: _BSD-2-Clause_).
A copy of this license with OpenSBI copyright can be found in the file
[COPYING.BSD].

All source files in OpenSBI contain the 2-Clause BSD license SPDX short
identifier in place of the full license text.

```
SPDX-License-Identifier:    BSD-2-Clause
```

This enables machine processing of license information based on the SPDX
License Identifiers that are available on the [SPDX] web site.

OpenSBI source code also contains code reused from other projects as listed
below. The original license text of these projects is included in the source
files where the reused code is present.

- The libfdt source code is disjunctively dual licensed
  (GPL-2.0+ OR BSD-2-Clause). Some of this project code is used in OpenSBI
  under the terms of the BSD 2-Clause license. Any contributions to this
  code must be made under the terms of both licenses.

See also the [third party notices] file for more information.

## Introduction

The **RISC-V Supervisor Binary Interface (SBI)** is the recommended interface
between:

1. A platform-specific firmware running in M-mode and a bootloader, a
   hypervisor or a general-purpose OS executing in S-mode or HS-mode.
2. A hypervisor running in HS-mode and a bootloader or a general-purpose OS
   executing in VS-mode.

The _RISC-V SBI specification_ is maintained as an independent project by the
RISC-V Foundation on [Github].

The goal of the OpenSBI project is to provide an open-source reference
implementation of the RISC-V SBI specifications for platform-specific firmwares
executing in M-mode (case 1 mentioned above). An OpenSBI implementation can be
easily extended by RISC-V platform and system-on-chip vendors to fit a
particular hardware configuration.

The main component of OpenSBI is provided in the form of a platform-independent
static library **libsbi.a** implementing the SBI interface. A firmware or
bootloader implementation can link against this library to ensure conformance
with the SBI interface specifications. _libsbi.a_ also defines an interface for
integrating with platform-specific operations provided by the platform firmware
implementation (e.g. console access functions, inter-processor interrupt
control, etc).

To illustrate the use of the _libsbi.a_ library, OpenSBI also provides a set of
platform-specific support examples. For each example, a platform-specific
static library _libplatsbi.a_ can be compiled. This library implements
SBI call processing by integrating _libsbi.a_ with the necessary
platform-dependent hardware manipulation functions. For all supported platforms,
OpenSBI also provides several runtime firmware examples built using the platform
_libplatsbi.a_. These example firmwares can be used to replace the legacy
_riscv-pk_ bootloader (aka BBL) and enable the use of well-known bootloaders
such as [U-Boot].

## Supported SBI version

Currently, OpenSBI fully supports SBI specification _v0.2_. OpenSBI also
supports Hart State Management (HSM) SBI extension starting from OpenSBI v0.7.
HSM extension allows S-mode software to boot all the harts a defined order
rather than legacy method of random booting of harts. As a result, many
required features such as CPU hotplug, kexec/kdump can also be supported easily
in S-mode. HSM extension in OpenSBI is implemented in a non-backward compatible
manner to reduce the maintenance burden and avoid confusion. That's why, any
S-mode software using OpenSBI will not be able to boot more than 1 hart if HSM
extension is not supported in S-mode.

Linux kernel already supports SBI v0.2 and HSM SBI extension starting from
**v5.7-rc1**. If you are using an Linux kernel older than **5.7-rc1** or any
other S-mode software without HSM SBI extension, you should stick to OpenSBI
v0.6 to boot all the harts. For a UMP systems, it doesn't matter.

N.B. Any S-mode boot loader (i.e. U-Boot) doesn't need to support HSM extension,
as it doesn't need to boot all the harts. The operating system should be
capable enough to bring up all other non-booting harts using HSM extension.

## Required Toolchain

OpenSBI can be compiled natively or cross-compiled on a x86 host. For
cross-compilation, you can build your own toolchain or just download
a prebuilt one from the [Bootlin toolchain repository].

Please note that only a 64-bit version of the toolchain is available in
the Bootlin toolchain repository for now.

## Building and Installing the OpenSBI Platform-Independent Library

The OpenSBI platform-independent static library _libsbi.a_ can be compiled
natively or it can be cross-compiled on a host with a different base
architecture than RISC-V.

For cross-compiling, the environment variable _CROSS_COMPILE_ must be defined
to specify the name prefix of the RISC-V compiler toolchain executables, e.g.
_riscv64-unknown-elf-_ if the gcc executable used is _riscv64-unknown-elf-gcc_.

To build _libsbi.a_ simply execute:

```
make
```

All compiled binaries as well as the resulting _libsbi.a_ static library file
will be placed in the _build/lib_ directory. To specify an alternate build root
directory path, run:

```
make O=<build_directory>
```

To generate files to be installed for using _libsbi.a_ in other projects, run:

```
make install
```

This will create the _install_ directory with all necessary include files
copied under the _install/include_ directory and the library file copied into
the _install/lib_ directory. To specify an alternate installation root
directory path, run:

```
make I=<install_directory> install
```

## Building and Installing a Reference Platform Static Library and Firmware

When the _PLATFORM=<platform_subdir>_ argument is specified on the make command
line, the platform-specific static library _libplatsbi.a_ and firmware examples
are built for the platform _<platform_subdir>_ present in the directory
_platform_ in the OpenSBI top directory. For example, to compile the platform
library and the firmware examples for the QEMU RISC-V _virt_ machine,
_<platform_subdir>_ should be _generic_.

To build _libsbi.a_, _libplatsbi.a_ and the firmware for one of the supported
platforms, run:

```
make PLATFORM=<platform_subdir>
```

An alternate build directory path can also be specified:

```
make PLATFORM=<platform_subdir> O=<build_directory>
```

The platform-specific library _libplatsbi.a_ will be generated in the
_build/platform/<platform_subdir>/lib_ directory. The platform firmware files
will be under the _build/platform/<platform_subdir>/firmware_ directory.
The compiled firmwares will be available in two different formats: an ELF file
and an expanded image file.

To install _libsbi.a_, _libplatsbi.a_, and the compiled firmwares, run:

```
make PLATFORM=<platform_subdir> install
```

This will copy the compiled platform-specific libraries and firmware files
under the _install/platform/<platform_subdir>/_ directory. An alternate
install root directory path can be specified as follows:

```
make PLATFORM=<platform_subdir> I=<install_directory> install
```

In addition, platform-specific configuration options can be specified with the
top-level make command line. These options, such as _PLATFORM\_<xyz>_ or
_FW\_<abc>_, are platform-specific and described in more details in the
_docs/platform/<platform_name>.md_ files and
_docs/firmware/<firmware_name>.md_ files.

## Building 32-bit / 64-bit OpenSBI Images

By default, building OpenSBI generates 32-bit or 64-bit images based on the
supplied RISC-V cross-compile toolchain. For example if _CROSS_COMPILE_ is set
to _riscv64-unknown-elf-_, 64-bit OpenSBI images will be generated. If building
32-bit OpenSBI images, _CROSS_COMPILE_ should be set to a toolchain that is
pre-configured to generate 32-bit RISC-V codes, like _riscv32-unknown-elf-_.

However it's possible to explicitly specify the image bits we want to build with
a given RISC-V toolchain. This can be done by setting the environment variable
_PLATFORM_RISCV_XLEN_ to the desired width, for example:

```
export CROSS_COMPILE=riscv64-unknown-elf-
export PLATFORM_RISCV_XLEN=32
```

will generate 32-bit OpenSBI images. And vice vesa.

## Contributing to OpenSBI

The OpenSBI project encourages and welcomes contributions. Contributions should
follow the rules described in the OpenSBI [Contribution Guideline] document.
In particular, all patches sent should contain a Signed-off-by tag.

The [Contributors List] document provides a list of individuals and
organizations actively contributing to the OpenSBI project.

## Documentation

Detailed documentation of various aspects of OpenSBI can be found under the
_docs_ directory. The documentation covers the following topics.

- [Contribution Guideline]: Guideline for contributing code to OpenSBI project
- [Library Usage]: API documentation of OpenSBI static library _libsbi.a_
- [Platform Requirements]: Requirements for using OpenSBI on a platform
- [Platform Support Guide]: Guideline for implementing support for new platforms
- [Platform Documentation]: Documentation of the platforms currently supported.
- [Firmware Documentation]: Documentation for the different types of firmware
  examples build supported by OpenSBI.
- [Domain Support]: Documentation for the OpenSBI domain support which helps
  users achieve system-level partitioning using OpenSBI.

OpenSBI source code is also well documented. For source level documentation,
doxygen style is used. Please refer to the [Doxygen manual] for details on this
format.

Doxygen can be installed on Linux distributions using _.deb_ packages using
the following command.

```
sudo apt-get install doxygen doxygen-latex doxygen-doc doxygen-gui graphviz
```

For _.rpm_ based Linux distributions, the following commands can be used.

```
sudo yum install doxygen doxygen-latex doxywizard graphviz
```

or

```
sudo yum install doxygen doxygen-latex doxywizard graphviz
```

To build a consolidated _refman.pdf_ of all documentation, run:

```
make docs
```

or

```
make O=<build_directory> docs
```

the resulting _refman.pdf_ will be available under the directory
_<build_directory>/docs/latex_. To install this file, run:

```
make install_docs
```

or

```
make I=<install_directory> install_docs
```

_refman.pdf_ will be installed under _<install_directory>/docs_.

[github]: https://github.com/riscv/riscv-sbi-doc
[u-boot]: https://www.denx.de/wiki/U-Boot/SourceCode
[bootlin toolchain repository]: https://toolchains.bootlin.com/
[copying.bsd]: COPYING.BSD
[spdx]: http://spdx.org/licenses/
[contribution guideline]: docs/contributing.md
[contributors list]: CONTRIBUTORS.md
[library usage]: docs/library_usage.md
[platform requirements]: docs/platform_requirements.md
[platform support guide]: docs/platform_guide.md
[platform documentation]: docs/platform/platform.md
[firmware documentation]: docs/firmware/fw.md
[domain support]: docs/domain_support.md
[doxygen manual]: http://www.doxygen.nl/manual/index.html
[kendryte standalone sdk]: https://github.com/kendryte/kendryte-standalone-sdk
[third party notices]: ThirdPartyNotices.md
