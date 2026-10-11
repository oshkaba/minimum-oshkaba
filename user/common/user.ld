OUTPUT_ARCH(arm)
ENTRY(__minemu_user_start)

/* Match MINEMU_PAGE_SIZE in kernel/include/minemu/mmu.h. */
MINEMU_PAGE_SIZE = 4096;

PHDRS
{
  text PT_LOAD FLAGS(5); /* Read and execute. */
  data PT_LOAD FLAGS(6); /* Read and write. */
}

SECTIONS
{
  . = 0x00400000;

  .text :
  {
    *(.text*)
  } :text

  .rodata :
  {
    *(.rodata*)
  } :text

  /* Code and writable data must occupy separate pages. */
  . = ALIGN(MINEMU_PAGE_SIZE);

  .data :
  {
    *(.data*)
  } :data

  .bss (NOLOAD) :
  {
    *(.bss*)
    *(COMMON)
  } :data
}