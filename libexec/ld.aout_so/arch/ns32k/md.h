/*	$NetBSD: md.h,v 1.7.2.1 2001/04/01 15:51:07 he Exp $  */

/*
 *	- ns32k dependent definitions
 */

#if defined(CROSS_LINKER) && defined(XHOST) && (XHOST==m68k || XHOST==sparc)
#define NEED_SWAP
#endif

#define	MAX_ALIGNMENT		(sizeof (long))

#define PAGSIZ			__LDPGSZ

#define N_SET_FLAG(ex,f)	N_SETMAGIC(ex,N_GETMAGIC(ex), MID_MACHINE, \
						N_GETFLAG(ex)|(f))

#define N_IS_DYNAMIC(ex)	((N_GETFLAG(ex) & EX_DYNAMIC))

#define N_BADMID(ex) \
	(N_GETMID(ex) != 0 && N_GETMID(ex) != MID_MACHINE)

/*
 * Should be handled by a.out.h ?
 */
#define N_ADJUST(ex)		(((ex).a_entry < PAGSIZ) ? -PAGSIZ : 0)
#define TEXT_START(ex)		(N_TXTADDR(ex) + N_ADJUST(ex))
#define DATA_START(ex)		(N_DATADDR(ex) + N_ADJUST(ex))

#define RELOC_STATICS_THROUGH_GOT_P(r)		(1)
#define JMPSLOT_NEEDS_RELOC			(0)
#define RELOC_INIT_SEGMENT_RELOC(r)		((r)->r_disp = 2)
#define	JMPSLOT_NONEXTERN_IS_INTERMODULE	(0)

#define md_got_reloc(r)			(0)

#define md_get_rt_segment_addend(r,a)	md_get_addend(r,a)

/* Width of a Global Offset Table entry */
#define GOT_ENTRY_SIZE	4
typedef long	got_t;

typedef struct jmpslot {
	u_char	code[6];
	u_short	reloc_index;
#define JMPSLOT_RELOC_MASK		0xffff
} jmpslot_t;

#define	NOP	0xa2		/* NOP opcode */
#define BSR	0x02a2		/* NOP BSR opcode */
#define BR	0xeaa2		/* NOP BR opcode */
#define BPT	0xf2		/* BPT opcode */

static inline void _cachectl __P((void *, unsigned int));

static inline void
_cachectl(addr, bytes)
	void *addr;
	unsigned int bytes;
{
	void cinv __P((vaddr_t));

	vaddr_t start;
	for(start = (vaddr_t) addr & 0xfffffff0;
	    start < (vaddr_t) addr + bytes; start += 0x10)
		cinv(start);
}

/*
 * Byte swap defs for cross linking
 */

#if !defined(NEED_SWAP)
# define md_swapin_exec_hdr(h)
# define md_swapout_exec_hdr(h)
# define md_swapin_symbols(s,n)
# define md_swapout_symbols(s,n)
# define md_swapin_zsymbols(s,n)
# define md_swapout_zsymbols(s,n)
# define md_swapin_reloc(r,n)
# define md_swapout_reloc(r,n)
# define md_swapin__dynamic(l)
# define md_swapout__dynamic(l)
# define md_swapin_section_dispatch_table(l)
# define md_swapout_section_dispatch_table(l)
# define md_swapin_so_debug(d)
# define md_swapout_so_debug(d)
# define md_swapin_rrs_hash(f,n)
# define md_swapout_rrs_hash(f,n)
# define md_swapin_sod(l,n)
# define md_swapout_sod(l,n)
# define md_swapout_jmpslot(j,n)
# define md_swapout_got(g,n)
# define md_swapin_ranlib_hdr(h,n)
# define md_swapout_ranlib_hdr(h,n)
#endif /* NEED_SWAP */

#ifdef CROSS_LINKER
# ifdef NEED_SWAP

/* Define IO byte swapping routines */

void	md_swapin_exec_hdr __P((struct exec *));
void	md_swapout_exec_hdr __P((struct exec *));
void	md_swapin_reloc __P((struct relocation_info *, int));
void	md_swapout_reloc __P((struct relocation_info *, int));
void	md_swapout_jmpslot __P((jmpslot_t *, int));

#  define md_swapin_symbols(s,n)		swap_symbols(s,n)
#  define md_swapout_symbols(s,n)		swap_symbols(s,n)
#  define md_swapin_zsymbols(s,n)		swap_zsymbols(s,n)
#  define md_swapout_zsymbols(s,n)		swap_zsymbols(s,n)
#  define md_swapin__dynamic(l)			swap__dynamic(l)
#  define md_swapout__dynamic(l)		swap__dynamic(l)
#  define md_swapin_section_dispatch_table(l)	swap_section_dispatch_table(l)
#  define md_swapout_section_dispatch_table(l)	swap_section_dispatch_table(l)
#  define md_swapin_so_debug(d)			swap_so_debug(d)
#  define md_swapout_so_debug(d)		swap_so_debug(d)
#  define md_swapin_rrs_hash(f,n)		swap_rrs_hash(f,n)
#  define md_swapout_rrs_hash(f,n)		swap_rrs_hash(f,n)
#  define md_swapin_sod(l,n)			swapin_link_object(l,n)
#  define md_swapout_sod(l,n)			swapout_link_object(l,n)
#  define md_swapout_got(g,n)			swap_longs((long*)(g),n)
#  define md_swapin_ranlib_hdr(h,n)		swap_ranlib_hdr(h,n)
#  define md_swapout_ranlib_hdr(h,n)		swap_ranlib_hdr(h,n)

#  define md_swap_short(x) ( (((x) >> 8) & 0xff) | (((x) & 0xff) << 8) )

#  define md_swap_long(x) ( (((x) >> 24) & 0xff    ) | (((x) >> 8 ) & 0xff00    ) | \
			    (((x) << 8 ) & 0xff0000) | (((x) << 24) & 0xff000000) )

# else	/* We need not swap, but must pay attention to alignment: */

#  define md_swap_short(x)	(x)
#  define md_swap_long(x)	(x)

# endif /* NEED_SWAP */

#else	/* Not a cross linker: use native */

# define md_swap_short(x)	(x)
# define md_swap_long(x)	(x)

#endif /* CROSS_LINKER */
