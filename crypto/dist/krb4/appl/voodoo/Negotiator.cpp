/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: Negotiator.cpp,v 1.1.1.1.4.2 2000/06/16 18:46:23 thorpej Exp $ */

//
//
//
// Negotiator.cpp
//
//
#include <Windows.h>
#include "Negotiator.h"

Option** debugoptions;

Negotiator::Negotiator (CharStream *str, TelnetEngine *te)
{
	ZeroMemory(options, sizeof(options));
	mTelnetEngine  = te;
	stream = str;
	defaultOption = new DenyAllOption(str, te);
	debugoptions = (Option**)&options;
}

Negotiator::~Negotiator ()
{
	delete defaultOption;
}

void
Negotiator::RegisterOption (Option *newOption)
{
	options[newOption->option] = newOption;
}

Option *
Negotiator::FindOptionFor (unsigned char option)
{
	Option *item = options[option];
	if(!item)
		item = defaultOption; 
	return item;
}

void Negotiator::Negotiate ()
{
	
	unsigned char command, option;
	
	if(!stream->GetChar (&command))	return;
	if(!stream->GetChar (&option))	return;
	
	(FindOptionFor (option))->NegotiateOption (command);
}
