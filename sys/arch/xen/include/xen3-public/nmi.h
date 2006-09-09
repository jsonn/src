/******************************************************************************
 * nmi.h
 * 
 * NMI callback registration and reason codes.
 * 
 * Copyright (c) 2005, Keir Fraser <keir@xensource.com>
 */

#ifndef __XEN_PUBLIC_NMI_H__
#define __XEN_PUBLIC_NMI_H__

/*
 * NMI reason codes:
 * Currently these are x86-specific, stored in arch_shared_info.nmi_reason.
 */
 /* I/O-check error reported via ISA port 0x61, bit 6. */
#define _XEN_NMIREASON_io_error     0
#define XEN_NMIREASON_io_error      (1UL << _XEN_NMIREASON_io_error)
 /* Parity error reported via ISA port 0x61, bit 7. */
#define _XEN_NMIREASON_parity_error 1
#define XEN_NMIREASON_parity_error  (1UL << _XEN_NMIREASON_parity_error)
 /* Unknown hardware-generated NMI. */
#define _XEN_NMIREASON_unknown      2
#define XEN_NMIREASON_unknown       (1UL << _XEN_NMIREASON_unknown)

/*
 * long nmi_op(unsigned int cmd, void *arg)
 * NB. All ops return zero on success, else a negative error code.
 */

/*
 * Register NMI callback for this (calling) VCPU. Currently this only makes
 * sense for domain 0, vcpu 0. All other callers will be returned EINVAL.
 * arg == pointer to xennmi_callback structure.
 */
#define XENNMI_register_callback   0
typedef struct xennmi_callback {
    unsigned long handler_address;
    unsigned long pad;
} xennmi_callback_t;
DEFINE_GUEST_HANDLE(xennmi_callback_t);

/*
 * Deregister NMI callback for this (calling) VCPU.
 * arg == NULL.
 */
#define XENNMI_unregister_callback 1

#endif /* __XEN_PUBLIC_NMI_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
