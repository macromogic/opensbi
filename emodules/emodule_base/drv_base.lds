/* See LICENSE for license details. */
OUTPUT_ARCH( "riscv" )

ENTRY(_start)

SECTIONS
{
  . = 0xE0600000;
  _start = .;

  _text_start = .;
  .text : {
    *(.text.init)
    *(.text)
  }
  _text_end = .;

  . = ALIGN(0x1000);

  _init_data_start = .;
  .init.data : {
    *(.init.data .init.data.*)
  }
  _init_data_end = .;

  . = ALIGN(0x1000);

  _rodata_start = .;
  .rodata : {
    *(.rdata)
    *(.rodata)
  }
  _rodata_end = .;

  . = ALIGN(0x1000);

  _data_start = .;
  .data : {
    *(.data)
    *(.data.*)
  }
  _data_end = .;
  
  /* . = ALIGN(0x1000);

  _page_table_start = .;
  .page_table : {
    *(.page_table)
  }

  .page_table_trie : {
    *(.page_table_trie)
  }
  
  _page_table_end = .; */


  . = ALIGN(0x1000);

  _bss_start = .;
  .bss : { 
    *(.bss)
    *(.bss.*)
    *(.sbss*)
  }
  _bss_end = .;
  
  . = ALIGN(0x1000);

  _drv_console_start = .;
  .drv_enclave_console :
  {
    *(.drv_enclave_console)
  }
  _drv_console_end = .;

  _end = .;
  . = ALIGN(0x1000);

  _temp_stack_start = .;
  . = . + 0x1000;
  _temp_stack_end = .;

}