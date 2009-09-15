/*	$NetBSD: tls_rsa.c,v 1.1.1.1.2.2 2009/09/15 06:03:51 snj Exp $	*/

/*++
/* NAME
/*	tls_rsa
/* SUMMARY
/*	RSA support
/* SYNOPSIS
/*	#define TLS_INTERNAL
/*	#include <tls.h>
/*
/*	RSA	*tls_tmp_rsa_cb(ssl, export, keylength)
/*	SSL	*ssl; /* unused */
/*	int	export;
/*	int	keylength;
/* DESCRIPTION
/*	This module maintains parameters for Diffie-Hellman key generation.
/*
/*	tls_tmp_rsa_cb() is a call-back routine for the
/*	SSL_CTX_set_tmp_rsa_callback() function.
/* LICENSE
/* .ad
/* .fi
/*	This software is free. You can do with it whatever you want.
/*	The original author kindly requests that you acknowledge
/*	the use of his software.
/* AUTHOR(S)
/*	Originally written by:
/*	Lutz Jaenicke
/*	BTU Cottbus
/*	Allgemeine Elektrotechnik
/*	Universitaetsplatz 3-4
/*	D-03044 Cottbus, Germany
/*
/*	Updated by:
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>

#ifdef USE_TLS

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>

/* tls_tmp_rsa_cb - call-back to generate ephemeral RSA key */

RSA *tls_tmp_rsa_cb(SSL *unused_ssl, int unused_export, int keylength)
{
    static RSA *rsa_tmp;

    /* Code adapted from OpenSSL apps/s_cb.c */

    if (rsa_tmp == 0)
	rsa_tmp = RSA_generate_key(keylength, RSA_F4, NULL, NULL);
    return (rsa_tmp);
}

#ifdef TEST

int main(int unused_argc, char **unused_argv)
{
    tls_tmp_rsa_cb(0, 1, 512);
    tls_tmp_rsa_cb(0, 1, 1024);
    tls_tmp_rsa_cb(0, 1, 2048);
    tls_tmp_rsa_cb(0, 0, 512);
}

#endif

#endif
