/* -*-C++-*-	$NetBSD: hpcmenu.h,v 1.1.2.2 2001/02/11 19:09:54 bouyer Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HPCBOOT_MENU_H_
#define _HPCBOOT_MENU_H_

#include <hpcdefs.h>

// forward declaration.
class Console;
class HpcBootApp;	
class RootWindow;
class BootButton;
class CancelButton;
class ProgressBar;
class TabWindowBase;
class MainTabWindow;
class OptionTabWindow;
class ConsoleTabWindow;
struct bootinfo;

// Application
class HpcBootApp {
public:
	HINSTANCE	_instance;
	HWND		_cmdbar;
	RootWindow	*_root;
	Console	*_cons;
	int		_cx_char, _cy_char; // 5, 14

private:
	void _get_font_size(void) {
		HDC hdc = GetDC(0);
		TEXTMETRIC tm;
		SelectObject(hdc, GetStockObject(SYSTEM_FONT));
		GetTextMetrics(hdc, &tm);
		_cx_char = tm.tmAveCharWidth;
		_cy_char = tm.tmHeight + tm.tmExternalLeading;
		ReleaseDC(0, hdc);
	}

public:
	explicit HpcBootApp(HINSTANCE instance) : _instance(instance) {
		_root	= 0;
		_cmdbar	= 0;
		_get_font_size();
	}
	virtual ~HpcBootApp(void) { /* NO-OP */ }

	BOOL registerClass(WNDPROC proc);
	int run(void);
};

// internal representation of user input.
class HpcMenuInterface
{
public:
	struct HpcMenuPreferences {
#define HPCBOOT_MAGIC		0x177d5753
		int		_magic;
		int		_version;
		size_t	_size;	// size of HpcMenuPreferences structure.
		int		dir;
		BOOL	dir_user;
		TCHAR	dir_user_path[MAX_PATH];
		BOOL	kernel_user;
		TCHAR	kernel_user_file[MAX_PATH];
		unsigned	platid_hi;
		unsigned	platid_lo;
		int		rootfs;
		TCHAR	rootfs_file[MAX_PATH];
		// kernel options.
		BOOL	boot_serial;
		BOOL	boot_verbose;
		BOOL	boot_single_user;
		BOOL	boot_ask_for_name;
		// boot loader options.
		int		auto_boot;
		BOOL	reverse_video;
		BOOL	pause_before_boot;
		BOOL	load_debug_info;
		BOOL	safety_message;
	};

	RootWindow		*_root;
	MainTabWindow		*_main;
	OptionTabWindow	*_option;
	ConsoleTabWindow	*_console;
	struct HpcMenuPreferences _pref;

	struct boot_hook_args {
		void(*func)(void *, struct HpcMenuPreferences &);
		void *arg;
	} _boot_hook;

	struct cons_hook_args {
		void(*func)(void *, unsigned char);
		void *arg;
	} _cons_hook [4];

private:
	static HpcMenuInterface *_instance;

	BOOL _find_pref_dir(TCHAR *);
	void _set_default_pref(void) {
		// set default.
		_pref._magic		= HPCBOOT_MAGIC;
		_pref.dir			= 0;
		_pref.dir_user		= FALSE;
		_pref.kernel_user		= FALSE;
		_pref.platid_hi		= 0;
		_pref.platid_lo		= 0;
		_pref.rootfs		= 0;

		_pref.boot_serial	= FALSE;
		_pref.boot_verbose	= FALSE;
		_pref.boot_single_user	= FALSE;
		_pref.boot_ask_for_name	= FALSE;
		_pref.auto_boot		= 0;
		_pref.reverse_video	= FALSE;
		_pref.pause_before_boot	= TRUE;
		_pref.safety_message	= TRUE;
	}
	enum _platform_op {
		_PLATFORM_OP_GET,
		_PLATFORM_OP_SET,
		_PLATFORM_OP_DEFAULT
	};
	void *_platform(int, enum _platform_op);

protected:
	HpcMenuInterface(void) {
		if (!load())
			_set_default_pref();
		_pref._version		= HPCBOOT_VERSION;
		_pref._size			= sizeof(HpcMenuPreferences);
    
		memset(_cons_hook, 0, sizeof(struct cons_hook_args) * 4);
		memset(&_boot_hook, 0, sizeof(struct boot_hook_args));
	}
	virtual ~HpcMenuInterface(void) { /* NO-OP */ }

public:
	static HpcMenuInterface &Instance(void);
	static void Destroy(void);

	// preferences.
	BOOL load(void);
	BOOL save(void);
  
	// Boot button
	// when user click `boot button' inquires all options.
	void get_options(void);
	enum { MAX_KERNEL_ARGS = 16 };
	int setup_kernel_args(vaddr_t, paddr_t);
	void setup_bootinfo(struct bootinfo &bi);
	void register_boot_hook(struct boot_hook_args &arg) {
		_boot_hook = arg;
	}
	// call architecture dependent boot function.
	void boot(void) {
		if (_boot_hook.func)
			_boot_hook.func(_boot_hook.arg, _pref);
	}
	// Progress bar.
	void progress(void);

	// Console window interface.
	void print(TCHAR *);
	void register_cons_hook(struct cons_hook_args &arg, int id) {
		if (id >= 0 && id < 4)
			_cons_hook[id] = arg;
	}

	// Main window options
	TCHAR *dir(int);
	int dir_default(void);
  
	// platform
	TCHAR *platform_get(int n) {
		return reinterpret_cast <TCHAR *>
			(_platform(n, _PLATFORM_OP_GET));
	}
	int platform_default(void) {
		return reinterpret_cast <int>
			(_platform(0, _PLATFORM_OP_DEFAULT));
	}
	void platform_set(int n) { _platform(n, _PLATFORM_OP_SET); }
};

#endif // _HPCBOOT_MENU_H_
