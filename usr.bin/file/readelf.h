/*	$NetBSD: readelf.h,v 1.4 1998/09/20 15:27:16 christos Exp $	*/

/*
 * readelf.h 
 * @(#)Id: readelf.h,v 1.6 1998/09/12 13:21:01 christos Exp 
 *
 * Provide elf data structures for non-elf machines, allowing file
 * non-elf hosts to determine if an elf binary is stripped.
 * Note: cobbled from the linux header file, with modifications
 */
#ifndef __fake_elf_h__
#define __fake_elf_h__

typedef unsigned int	Elf32_Addr;
typedef unsigned int	Elf32_Off;
typedef unsigned short	Elf32_Half;
typedef unsigned int	Elf32_Word;
typedef unsigned char	Elf32_Char;

#ifdef __GNUC__
typedef	unsigned long long Elf64_Addr;
typedef	unsigned long long Elf64_Off;
#else
/* XXX: We need 64 bit numbers here */
typedef unsigned int	Elf64_Addr[2];
typedef unsigned int	Elf64_Off[2];
#endif
typedef unsigned short	Elf64_Half;
typedef unsigned int	Elf64_Word;
typedef unsigned char	Elf64_Char;

#define EI_NIDENT	16

typedef struct {
    Elf32_Char	e_ident[EI_NIDENT];
    Elf32_Half	e_type;
    Elf32_Half	e_machine;
    Elf32_Word	e_version;
    Elf32_Addr	e_entry;  /* Entry point */
    Elf32_Off	e_phoff;
    Elf32_Off	e_shoff;
    Elf32_Word	e_flags;
    Elf32_Half	e_ehsize;
    Elf32_Half	e_phentsize;
    Elf32_Half	e_phnum;
    Elf32_Half	e_shentsize;
    Elf32_Half	e_shnum;
    Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    Elf64_Char	e_ident[EI_NIDENT];
    Elf64_Half	e_type;
    Elf64_Half	e_machine;
    Elf64_Word	e_version;
    Elf64_Addr	e_entry;  /* Entry point */
    Elf64_Off	e_phoff;
    Elf64_Off	e_shoff;
    Elf64_Word	e_flags;
    Elf64_Half	e_ehsize;
    Elf64_Half	e_phentsize;
    Elf64_Half	e_phnum;
    Elf64_Half	e_shentsize;
    Elf64_Half	e_shnum;
    Elf64_Half	e_shstrndx;
} Elf64_Ehdr;

/* e_type */
#define ET_EXEC		2
#define ET_CORE		4

/* sh_type */
#define SHT_SYMTAB	2
#define SHT_NOTE	7

/* elf type */
#define ELFDATANONE	0		/* e_ident[EI_DATA] */
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2

/* elf class */
#define ELFCLASSNONE	0
#define ELFCLASS32	1
#define ELFCLASS64	2

/* magic number */
#define	EI_MAG0		0		/* e_ident[] indexes */
#define	EI_MAG1		1
#define	EI_MAG2		2
#define	EI_MAG3		3
#define	EI_CLASS	4
#define	EI_DATA		5
#define	EI_VERSION	6
#define	EI_PAD		7

#define	ELFMAG0		0x7f		/* EI_MAG */
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'
#define	ELFMAG		"\177ELF"

#define	OLFMAG1		'O'
#define	OLFMAG		"\177OLF"

typedef struct {
    Elf32_Word	p_type;
    Elf32_Off	p_offset;
    Elf32_Addr	p_vaddr;
    Elf32_Addr	p_paddr;
    Elf32_Word	p_filesz;
    Elf32_Word	p_memsz;
    Elf32_Word	p_flags;
    Elf32_Word	p_align;
} Elf32_Phdr;

#define	PT_NULL		0		/* p_type */
#define	PT_LOAD		1
#define	PT_DYNAMIC	2
#define	PT_INTERP	3
#define	PT_NOTE		4
#define	PT_SHLIB	5
#define	PT_PHDR		6
#define	PT_NUM		7

typedef struct {
    Elf32_Word	sh_name;
    Elf32_Word	sh_type;
    Elf32_Word	sh_flags;
    Elf32_Addr	sh_addr;
    Elf32_Off	sh_offset;
    Elf32_Word	sh_size;
    Elf32_Word	sh_link;
    Elf32_Word	sh_info;
    Elf32_Word	sh_addralign;
    Elf32_Word	sh_entsize;
} Elf32_Shdr;

typedef struct {
    Elf64_Word	sh_name;
    Elf64_Word	sh_type;
    Elf64_Off	sh_flags;
    Elf64_Addr	sh_addr;
    Elf64_Off	sh_offset;
    Elf64_Off	sh_size;
    Elf64_Word	sh_link;
    Elf64_Word	sh_info;
    Elf64_Off	sh_addralign;
    Elf64_Off	sh_entsize;
} Elf64_Shdr;

/* Notes used in ET_CORE */
#define NT_PRSTATUS	1
#define NT_PRFPREG	2
#define NT_PRPSINFO	3
#define NT_TASKSTRUCT	4

/* Note header in a PT_NOTE section */
typedef struct elf_note {
  Elf32_Word	n_namesz;	/* Name size */
  Elf32_Word	n_descsz;	/* Content size */
  Elf32_Word	n_type;		/* Content type */
} Elf32_Nhdr;

typedef struct {
    Elf64_Word	n_namesz;
    Elf64_Word	n_descsz;
    Elf64_Word	n_type;
} Elf64_Nhdr;

#define	NT_PRSTATUS	1
#define	NT_PRFPREG	2
#define	NT_PRPSINFO	3
#define	NT_PRXREG	4
#define	NT_PLATFORM	5
#define	NT_AUXV		6

#endif
