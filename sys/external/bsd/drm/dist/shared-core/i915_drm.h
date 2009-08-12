/*
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _I915_DRM_H_
#define _I915_DRM_H_

/* Please note that modifications to all structs defined here are
 * subject to backwards-compatibility constraints.
 */

#include "drm.h"

/* Each region is a minimum of 16k, and there are at most 255 of them.
 */
#define I915_NR_TEX_REGIONS 255	/* table size 2k - maximum due to use
				 * of chars for next/prev indices */
#define I915_LOG_MIN_TEX_REGION_SIZE 14

typedef struct _drm_i915_init {
	enum {
		I915_INIT_DMA = 0x01,
		I915_CLEANUP_DMA = 0x02,
		I915_RESUME_DMA = 0x03,

		/* Since this struct isn't versioned, just used a new
		 * 'func' code to indicate the presence of dri2 sarea
		 * info. */
		I915_INIT_DMA2 = 0x04
	} func;
	unsigned int mmio_offset;
	int sarea_priv_offset;
	unsigned int ring_start;
	unsigned int ring_end;
	unsigned int ring_size;
	unsigned int front_offset;
	unsigned int back_offset;
	unsigned int depth_offset;
	unsigned int w;
	unsigned int h;
	unsigned int pitch;
	unsigned int pitch_bits;
	unsigned int back_pitch;
	unsigned int depth_pitch;
	unsigned int cpp;
	unsigned int chipset;
	unsigned int sarea_handle;
} drm_i915_init_t;

typedef struct drm_i915_sarea {
	struct drm_tex_region texList[I915_NR_TEX_REGIONS + 1];
	int last_upload;	/* last time texture was uploaded */
	int last_enqueue;	/* last time a buffer was enqueued */
	int last_dispatch;	/* age of the most recently dispatched buffer */
	int ctxOwner;		/* last context to upload state */
	int texAge;
	int pf_enabled;		/* is pageflipping allowed? */
	int pf_active;
	int pf_current_page;	/* which buffer is being displayed? */
	int perf_boxes;		/* performance boxes to be displayed */
	int width, height;      /* screen size in pixels */

	drm_handle_t front_handle;
	int front_offset;
	int front_size;

	drm_handle_t back_handle;
	int back_offset;
	int back_size;

	drm_handle_t depth_handle;
	int depth_offset;
	int depth_size;

	drm_handle_t tex_handle;
	int tex_offset;
	int tex_size;
	int log_tex_granularity;
	int pitch;
	int rotation;           /* 0, 90, 180 or 270 */
	int rotated_offset;
	int rotated_size;
	int rotated_pitch;
	int virtualX, virtualY;

	unsigned int front_tiled;
	unsigned int back_tiled;
	unsigned int depth_tiled;
	unsigned int rotated_tiled;
	unsigned int rotated2_tiled;

	int planeA_x;
	int planeA_y;
	int planeA_w;
	int planeA_h;
	int planeB_x;
	int planeB_y;
	int planeB_w;
	int planeB_h;

	/* Triple buffering */
	drm_handle_t third_handle;
	int third_offset;
	int third_size;
	unsigned int third_tiled;

	/* buffer object handles for the static buffers.  May change
	 * over the lifetime of the client, though it doesn't in our current
	 * implementation.
	 */
	unsigned int front_bo_handle;
	unsigned int back_bo_handle;
	unsigned int third_bo_handle;
	unsigned int depth_bo_handle;
} drm_i915_sarea_t;

/* Driver specific fence types and classes.
 */

/* The only fence class we support */
#define DRM_I915_FENCE_CLASS_ACCEL 0
/* Fence type that guarantees read-write flush */
#define DRM_I915_FENCE_TYPE_RW 2
/* MI_FLUSH programmed just before the fence */
#define DRM_I915_FENCE_FLAG_FLUSHED 0x01000000

/* Flags for perf_boxes
 */
#define I915_BOX_RING_EMPTY    0x1
#define I915_BOX_FLIP          0x2
#define I915_BOX_WAIT          0x4
#define I915_BOX_TEXTURE_LOAD  0x8
#define I915_BOX_LOST_CONTEXT  0x10

/* I915 specific ioctls
 * The device specific ioctl range is 0x40 to 0x79.
 */
#define DRM_I915_INIT		0x00
#define DRM_I915_FLUSH		0x01
#define DRM_I915_FLIP		0x02
#define DRM_I915_BATCHBUFFER	0x03
#define DRM_I915_IRQ_EMIT	0x04
#define DRM_I915_IRQ_WAIT	0x05
#define DRM_I915_GETPARAM	0x06
#define DRM_I915_SETPARAM	0x07
#define DRM_I915_ALLOC		0x08
#define DRM_I915_FREE		0x09
#define DRM_I915_INIT_HEAP	0x0a
#define DRM_I915_CMDBUFFER	0x0b
#define DRM_I915_DESTROY_HEAP	0x0c
#define DRM_I915_SET_VBLANK_PIPE	0x0d
#define DRM_I915_GET_VBLANK_PIPE	0x0e
#define DRM_I915_VBLANK_SWAP	0x0f
#define DRM_I915_MMIO		0x10
#define DRM_I915_HWS_ADDR	0x11
#define DRM_I915_EXECBUFFER	0x12

#define DRM_IOCTL_I915_INIT		DRM_IOW( DRM_COMMAND_BASE + DRM_I915_INIT, drm_i915_init_t)
#define DRM_IOCTL_I915_FLUSH		DRM_IO ( DRM_COMMAND_BASE + DRM_I915_FLUSH)
#define DRM_IOCTL_I915_FLIP		DRM_IOW( DRM_COMMAND_BASE + DRM_I915_FLIP, drm_i915_flip_t)
#define DRM_IOCTL_I915_BATCHBUFFER	DRM_IOW( DRM_COMMAND_BASE + DRM_I915_BATCHBUFFER, drm_i915_batchbuffer_t)
#define DRM_IOCTL_I915_IRQ_EMIT         DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_IRQ_EMIT, drm_i915_irq_emit_t)
#define DRM_IOCTL_I915_IRQ_WAIT         DRM_IOW( DRM_COMMAND_BASE + DRM_I915_IRQ_WAIT, drm_i915_irq_wait_t)
#define DRM_IOCTL_I915_GETPARAM         DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GETPARAM, drm_i915_getparam_t)
#define DRM_IOCTL_I915_SETPARAM         DRM_IOW( DRM_COMMAND_BASE + DRM_I915_SETPARAM, drm_i915_setparam_t)
#define DRM_IOCTL_I915_ALLOC            DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_ALLOC, drm_i915_mem_alloc_t)
#define DRM_IOCTL_I915_FREE             DRM_IOW( DRM_COMMAND_BASE + DRM_I915_FREE, drm_i915_mem_free_t)
#define DRM_IOCTL_I915_INIT_HEAP        DRM_IOW( DRM_COMMAND_BASE + DRM_I915_INIT_HEAP, drm_i915_mem_init_heap_t)
#define DRM_IOCTL_I915_CMDBUFFER	DRM_IOW( DRM_COMMAND_BASE + DRM_I915_CMDBUFFER, drm_i915_cmdbuffer_t)
#define DRM_IOCTL_I915_DESTROY_HEAP	DRM_IOW( DRM_COMMAND_BASE + DRM_I915_DESTROY_HEAP, drm_i915_mem_destroy_heap_t)
#define DRM_IOCTL_I915_SET_VBLANK_PIPE	DRM_IOW( DRM_COMMAND_BASE + DRM_I915_SET_VBLANK_PIPE, drm_i915_vblank_pipe_t)
#define DRM_IOCTL_I915_GET_VBLANK_PIPE	DRM_IOR( DRM_COMMAND_BASE + DRM_I915_GET_VBLANK_PIPE, drm_i915_vblank_pipe_t)
#define DRM_IOCTL_I915_VBLANK_SWAP	DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_VBLANK_SWAP, drm_i915_vblank_swap_t)
#define DRM_IOCTL_I915_MMIO             DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_MMIO, drm_i915_mmio)
#define DRM_IOCTL_I915_EXECBUFFER	DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_EXECBUFFER, struct drm_i915_execbuffer)

/* Asynchronous page flipping:
 */
typedef struct drm_i915_flip {
	/*
	 * This is really talking about planes, and we could rename it
	 * except for the fact that some of the duplicated i915_drm.h files
	 * out there check for HAVE_I915_FLIP and so might pick up this
	 * version.
	 */
	int pipes;
} drm_i915_flip_t;

/* Allow drivers to submit batchbuffers directly to hardware, relying
 * on the security mechanisms provided by hardware.
 */
typedef struct drm_i915_batchbuffer {
	int start;		/* agp offset */
	int used;		/* nr bytes in use */
	int DR1;		/* hw flags for GFX_OP_DRAWRECT_INFO */
	int DR4;		/* window origin for GFX_OP_DRAWRECT_INFO */
	int num_cliprects;	/* mulitpass with multiple cliprects? */
	struct drm_clip_rect __user *cliprects;	/* pointer to userspace cliprects */
} drm_i915_batchbuffer_t;

/* As above, but pass a pointer to userspace buffer which can be
 * validated by the kernel prior to sending to hardware.
 */
typedef struct _drm_i915_cmdbuffer {
	char __user *buf;	/* pointer to userspace command buffer */
	int sz;			/* nr bytes in buf */
	int DR1;		/* hw flags for GFX_OP_DRAWRECT_INFO */
	int DR4;		/* window origin for GFX_OP_DRAWRECT_INFO */
	int num_cliprects;	/* mulitpass with multiple cliprects? */
	struct drm_clip_rect __user *cliprects;	/* pointer to userspace cliprects */
} drm_i915_cmdbuffer_t;

/* Userspace can request & wait on irq's:
 */
typedef struct drm_i915_irq_emit {
	int __user *irq_seq;
} drm_i915_irq_emit_t;

typedef struct drm_i915_irq_wait {
	int irq_seq;
} drm_i915_irq_wait_t;

/* Ioctl to query kernel params:
 */
#define I915_PARAM_IRQ_ACTIVE            1
#define I915_PARAM_ALLOW_BATCHBUFFER     2
#define I915_PARAM_LAST_DISPATCH         3
#define I915_PARAM_CHIPSET_ID            4

typedef struct drm_i915_getparam {
	int param;
	int __user *value;
} drm_i915_getparam_t;

/* Ioctl to set kernel params:
 */
#define I915_SETPARAM_USE_MI_BATCHBUFFER_START            1
#define I915_SETPARAM_TEX_LRU_LOG_GRANULARITY             2
#define I915_SETPARAM_ALLOW_BATCHBUFFER                   3

typedef struct drm_i915_setparam {
	int param;
	int value;
} drm_i915_setparam_t;

/* A memory manager for regions of shared memory:
 */
#define I915_MEM_REGION_AGP 1

typedef struct drm_i915_mem_alloc {
	int region;
	int alignment;
	int size;
	int __user *region_offset;	/* offset from start of fb or agp */
} drm_i915_mem_alloc_t;

typedef struct drm_i915_mem_free {
	int region;
	int region_offset;
} drm_i915_mem_free_t;

typedef struct drm_i915_mem_init_heap {
	int region;
	int size;
	int start;
} drm_i915_mem_init_heap_t;

/* Allow memory manager to be torn down and re-initialized (eg on
 * rotate):
 */
typedef struct drm_i915_mem_destroy_heap {
	int region;
} drm_i915_mem_destroy_heap_t;

/* Allow X server to configure which pipes to monitor for vblank signals
 */
#define	DRM_I915_VBLANK_PIPE_A	1
#define	DRM_I915_VBLANK_PIPE_B	2

typedef struct drm_i915_vblank_pipe {
	int pipe;
} drm_i915_vblank_pipe_t;

/* Schedule buffer swap at given vertical blank:
 */
typedef struct drm_i915_vblank_swap {
	drm_drawable_t drawable;
	enum drm_vblank_seq_type seqtype;
	unsigned int sequence;
} drm_i915_vblank_swap_t;

#define I915_MMIO_READ	0
#define I915_MMIO_WRITE 1

#define I915_MMIO_MAY_READ	0x1
#define I915_MMIO_MAY_WRITE	0x2

#define MMIO_REGS_IA_PRIMATIVES_COUNT		0
#define MMIO_REGS_IA_VERTICES_COUNT		1
#define MMIO_REGS_VS_INVOCATION_COUNT		2
#define MMIO_REGS_GS_PRIMITIVES_COUNT		3
#define MMIO_REGS_GS_INVOCATION_COUNT		4
#define MMIO_REGS_CL_PRIMITIVES_COUNT		5
#define MMIO_REGS_CL_INVOCATION_COUNT		6
#define MMIO_REGS_PS_INVOCATION_COUNT		7
#define MMIO_REGS_PS_DEPTH_COUNT		8

typedef struct drm_i915_mmio_entry {
	unsigned int flag;
	unsigned int offset;
	unsigned int size;
} drm_i915_mmio_entry_t;

typedef struct drm_i915_mmio {
	unsigned int read_write:1;
	unsigned int reg:31;
	void __user *data;
} drm_i915_mmio_t;

typedef struct drm_i915_hws_addr {
	uint64_t addr;
} drm_i915_hws_addr_t;

/*
 * Relocation header is 4 uint32_ts
 * 0 - 32 bit reloc count
 * 1 - 32-bit relocation type
 * 2-3 - 64-bit user buffer handle ptr for another list of relocs.
 */
#define I915_RELOC_HEADER 4

/*
 * type 0 relocation has 4-uint32_t stride
 * 0 - offset into buffer
 * 1 - delta to add in
 * 2 - buffer handle
 * 3 - reserved (for optimisations later).
 */
/*
 * type 1 relocation has 4-uint32_t stride.
 * Hangs off the first item in the op list.
 * Performed after all valiations are done.
 * Try to group relocs into the same relocatee together for
 * performance reasons.
 * 0 - offset into buffer
 * 1 - delta to add in
 * 2 - buffer index in op list.
 * 3 - relocatee index in op list.
 */
#define I915_RELOC_TYPE_0 0
#define I915_RELOC0_STRIDE 4
#define I915_RELOC_TYPE_1 1
#define I915_RELOC1_STRIDE 4


struct drm_i915_op_arg {
	uint64_t next;
	uint64_t reloc_ptr;
	int handled;
	unsigned int pad64;
	union {
		struct drm_bo_op_req req;
		struct drm_bo_arg_rep rep;
	} d;

};

struct drm_i915_execbuffer {
	uint64_t ops_list;
	uint32_t num_buffers;
	struct drm_i915_batchbuffer batch;
	drm_context_t context; /* for lockless use in the future */
	struct drm_fence_arg fence_arg;
};

#endif				/* _I915_DRM_H_ */
