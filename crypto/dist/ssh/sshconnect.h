/*	$OpenBSD: sshconnect.h,v 1.6 2001/02/15 23:19:59 markus Exp $	*/

/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef SSHCONNECT_H
#define SSHCONNECT_H
/*
 * Opens a TCP/IP connection to the remote server on the given host.  If port
 * is 0, the default port will be used.  If anonymous is zero, a privileged
 * port will be allocated to make the connection. This requires super-user
 * privileges if anonymous is false. Connection_attempts specifies the
 * maximum number of tries, one per second.  This returns true on success,
 * and zero on failure.  If the connection is successful, this calls
 * packet_set_connection for the connection.
 */
int
ssh_connect(const char *host, struct sockaddr_storage * hostaddr,
    u_short port, int connection_attempts,
    int anonymous, uid_t original_real_uid,
    const char *proxy_command);

/*
 * Starts a dialog with the server, and authenticates the current user on the
 * server.  This does not need any extra privileges.  The basic connection to
 * the server must already have been established before this is called. If
 * login fails, this function prints an error and never returns. This
 * initializes the random state, and leaves it initialized (it will also have
 * references from the packet module).
 */

void
ssh_login(int host_key_valid, RSA * host_key, const char *host,
    struct sockaddr * hostaddr, uid_t original_real_uid);


void
check_host_key(char *host, struct sockaddr *hostaddr, Key *host_key,
    const char *user_hostfile, const char *system_hostfile);

void	ssh_kex(char *host, struct sockaddr *hostaddr);
void
ssh_userauth(const char * local_user, const char * server_user, char *host,
    int host_key_valid, RSA *own_host_key);

void	ssh_kex2(char *host, struct sockaddr *hostaddr);
void	ssh_userauth2(const char *server_user, char *host);

void	ssh_put_password(char *password);

#endif
