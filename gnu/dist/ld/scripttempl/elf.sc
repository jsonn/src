#
# Unusual variables checked by this code:
#	NOP - two byte opcode for no-op (defaults to 0)
#	DATA_ADDR - if end-of-text-plus-one-page isn't right for data start
#	OTHER_READONLY_SECTIONS - other than .text .init .rodata ...
#		(e.g., .PARISC.milli)
#	OTHER_READWRITE_SECTIONS - other than .data .bss .ctors .sdata ...
#		(e.g., .PARISC.global)
#	OTHER_SECTIONS - at the end
#	EXECUTABLE_SYMBOLS - symbols that must be defined for an
#		executable (e.g., _DYNAMIC_LINK)
#	TEXT_START_SYMBOLS - symbols that appear at the start of the
#		.text section.
#	DATA_START_SYMBOLS - symbols that appear at the start of the
#		.data section.
#	OTHER_BSS_SYMBOLS - symbols that appear at the start of the
#		.bss section besides __bss_start.
#	DATA_PLT - .plt should be in data segment, not text segment.
#	EMBEDDED - whether this is for an embedded system. 
#
# When adding sections, do note that the names of some sections are used
# when specifying the start address of the next.
#
test -z "$ENTRY" && ENTRY=_start
test -z "${BIG_OUTPUT_FORMAT}" && BIG_OUTPUT_FORMAT=${OUTPUT_FORMAT}
test -z "${LITTLE_OUTPUT_FORMAT}" && LITTLE_OUTPUT_FORMAT=${OUTPUT_FORMAT}
if [ -z "$MACHINE" ]; then OUTPUT_ARCH=${ARCH}; else OUTPUT_ARCH=${ARCH}:${MACHINE}; fi
test "$LD_FLAG" = "N" && DATA_ADDR=.
INTERP=".interp   ${RELOCATING-0} : { *(.interp) 	}"
PLT=".plt    ${RELOCATING-0} : { *(.plt)	}"
ELFALIGN=`expr ${ELFSIZE} / 8`

# if this is for an embedded system, don't add SIZEOF_HEADERS.
if [ -z "$EMBEDDED" ]; then
   test -z "${TEXT_BASE_ADDRESS}" && TEXT_BASE_ADDRESS="${TEXT_START_ADDR} + SIZEOF_HEADERS"
else
   test -z "${TEXT_BASE_ADDRESS}" && TEXT_BASE_ADDRESS="${TEXT_START_ADDR}"
fi

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}", "${BIG_OUTPUT_FORMAT}",
	      "${LITTLE_OUTPUT_FORMAT}")
OUTPUT_ARCH(${OUTPUT_ARCH})
ENTRY(${ENTRY})

${RELOCATING+${LIB_SEARCH_DIRS}}
${RELOCATING+/* Do we need any of these for elf?
   __DYNAMIC = 0; ${STACKZERO+${STACKZERO}} ${SHLIB_PATH+${SHLIB_PATH}}  */}
${RELOCATING+${EXECUTABLE_SYMBOLS}}
${RELOCATING- /* For some reason, the Solaris linker makes bad executables
  if gld -r is used and the intermediate file has sections starting
  at non-zero addresses.  Could be a Solaris ld bug, could be a GNU ld
  bug.  But for now assigning the zero vmas works.  */}
SECTIONS
{
  /* Read-only sections, merged into text segment: */
  ${CREATE_SHLIB-${RELOCATING+. = ${TEXT_BASE_ADDRESS};}}
  ${CREATE_SHLIB+${RELOCATING+. = SIZEOF_HEADERS;}}
  ${CREATE_SHLIB-${INTERP}}
  .hash        ${RELOCATING-0} : { *(.hash)		}
  .dynsym      ${RELOCATING-0} : { *(.dynsym)		}
  .dynstr      ${RELOCATING-0} : { *(.dynstr)		}
  .gnu.version ${RELOCATING-0} : { *(.gnu.version)	}
  .gnu.version_d ${RELOCATING-0} : { *(.gnu.version_d)	}
  .gnu.version_r ${RELOCATING-0} : { *(.gnu.version_r)	}
  .rel.text    ${RELOCATING-0} :
    { *(.rel.text) *(.rel.gnu.linkonce.t*) }
  .rela.text   ${RELOCATING-0} :
    { *(.rela.text) *(.rela.gnu.linkonce.t*) }
  .rel.data    ${RELOCATING-0} :
    { *(.rel.data) *(.rel.gnu.linkonce.d*) }
  .rela.data   ${RELOCATING-0} :
    { *(.rela.data) *(.rela.gnu.linkonce.d*) }
  .rel.rodata  ${RELOCATING-0} :
    { *(.rel.rodata) *(.rel.gnu.linkonce.r*) }
  .rela.rodata ${RELOCATING-0} :
    { *(.rela.rodata) *(.rela.gnu.linkonce.r*) }
  .rel.got     ${RELOCATING-0} : { *(.rel.got)		}
  .rela.got    ${RELOCATING-0} : { *(.rela.got)		}
  .rel.ctors   ${RELOCATING-0} : { *(.rel.ctors)	}
  .rela.ctors  ${RELOCATING-0} : { *(.rela.ctors)	}
  .rel.dtors   ${RELOCATING-0} : { *(.rel.dtors)	}
  .rela.dtors  ${RELOCATING-0} : { *(.rela.dtors)	}
  .rel.init    ${RELOCATING-0} : { *(.rel.init)	}
  .rela.init   ${RELOCATING-0} : { *(.rela.init)	}
  .rel.fini    ${RELOCATING-0} : { *(.rel.fini)	}
  .rela.fini   ${RELOCATING-0} : { *(.rela.fini)	}
  .rel.bss     ${RELOCATING-0} : { *(.rel.bss)		}
  .rela.bss    ${RELOCATING-0} : { *(.rela.bss)		}
  .rel.plt     ${RELOCATING-0} : { *(.rel.plt)		}
  .rela.plt    ${RELOCATING-0} : { *(.rela.plt)		}
  .init        ${RELOCATING-0} : { *(.init)	} =${NOP-0}
  ${DATA_PLT-${PLT}}
  .text    ${RELOCATING-0} :
  {
    ${RELOCATING+${TEXT_START_SYMBOLS}}
    *(.text)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
    *(.gnu.linkonce.t*)
  } =${NOP-0}
  ${RELOCATING+_etext = .;}
  ${RELOCATING+PROVIDE (etext = .);}
  .fini    ${RELOCATING-0} : { *(.fini)    } =${NOP-0}
  .rodata  ${RELOCATING-0} : { *(.rodata) *(.gnu.linkonce.r*) }
  .rodata1 ${RELOCATING-0} : { *(.rodata1) }
  ${RELOCATING+${OTHER_READONLY_SECTIONS}}

  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  ${RELOCATING+. = ${DATA_ADDR-ALIGN(${MAXPAGESIZE}) + (. & (${MAXPAGESIZE} - 1))};}

  .data  ${RELOCATING-0} :
  {
    ${RELOCATING+${DATA_START_SYMBOLS}}
    *(.data)
    *(.gnu.linkonce.d*)
    ${CONSTRUCTING+CONSTRUCTORS}
  }
  .data1 ${RELOCATING-0} : { *(.data1) }
  ${RELOCATING+${OTHER_READWRITE_SECTIONS}}
  .ctors       ${RELOCATING-0} :
  {
    ${CONSTRUCTING+${CTOR_START}}
    *(.ctors)
    ${CONSTRUCTING+${CTOR_END}}
  }
  .dtors       ${RELOCATING-0} :
  {
    ${CONSTRUCTING+${DTOR_START}}
    *(.dtors)
    ${CONSTRUCTING+${DTOR_END}}
  }
  ${DATA_PLT+${PLT}}
  .got         ${RELOCATING-0} : { *(.got.plt) *(.got) }
  .dynamic     ${RELOCATING-0} : { *(.dynamic) }
  /* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata   ${RELOCATING-0} : { *(.sdata) }
  ${RELOCATING+_edata  =  .;}
  ${RELOCATING+PROVIDE (edata = .);}
  ${RELOCATING+__bss_start = .;}
  ${RELOCATING+${OTHER_BSS_SYMBOLS}}
  .sbss    ${RELOCATING-0} : { *(.sbss) *(.scommon) }
  .bss     ${RELOCATING-0} :
  {
   *(.dynbss)
   *(.bss)
   *(COMMON)
  }
  ${RELOCATING+. = ALIGN(${ELFALIGN});}
  ${RELOCATING+_end = . ;}
  ${RELOCATING+PROVIDE (end = .);}

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }

  .comment 0 : { *(.comment) }

  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */

  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }

  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }

  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }

  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }

  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }

  ${RELOCATING+${OTHER_RELOCATING_SECTIONS}}

  /* These must appear regardless of ${RELOCATING}.  */
  ${OTHER_SECTIONS}
}
EOF
