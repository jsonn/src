/*	$NetBSD: rumpkern_if_priv.h,v 1.6.2.3 2010/08/11 22:55:07 yamt Exp $	*/

/*
 * Automatically generated.  DO NOT EDIT.
 * from: NetBSD: rumpkern.ifspec,v 1.4 2010/03/05 18:41:46 pooka Exp 
 * by:   NetBSD: makerumpif.sh,v 1.4 2009/10/15 00:29:19 pooka Exp 
 */

void rump_reboot(int);
int rump_getversion(void);
int rump_module_init(const struct modinfo * const *, size_t);
int rump_module_fini(const struct modinfo *);
int rump_kernelfsym_load(void *, uint64_t, char *, uint64_t);
struct uio * rump_uio_setup(void *, size_t, off_t, enum rump_uiorw);
size_t rump_uio_getresid(struct uio *);
off_t rump_uio_getoff(struct uio *);
size_t rump_uio_free(struct uio *);
struct kauth_cred* rump_cred_create(uid_t, gid_t, size_t, gid_t *);
struct kauth_cred* rump_cred_suserget(void);
void rump_cred_put(struct kauth_cred *);
struct lwp * rump_newproc_switch(void);
struct lwp * rump_lwp_alloc(pid_t, lwpid_t);
struct lwp * rump_lwp_alloc_and_switch(pid_t, lwpid_t);
struct lwp * rump_lwp_curlwp(void);
void rump_lwp_switch(struct lwp *);
void rump_lwp_release(struct lwp *);
int rump_sysproxy_set(rump_sysproxy_t, void *);
int rump_sysproxy_socket_setup_client(int);
int rump_sysproxy_socket_setup_server(int);
