/* ELF linker support.
   Copyright 1995, 1996, 1997, 1998 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* ELF linker code.  */

/* This struct is used to pass information to routines called via
   elf_link_hash_traverse which must return failure.  */

struct elf_info_failed
{
  boolean failed;
  struct bfd_link_info *info;
};

static boolean elf_link_add_object_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf_link_add_archive_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf_merge_symbol
  PARAMS ((bfd *, struct bfd_link_info *, const char *, Elf_Internal_Sym *,
	   asection **, bfd_vma *, struct elf_link_hash_entry **,
	   boolean *, boolean *, boolean *));
static boolean elf_export_symbol
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_fix_symbol_flags
  PARAMS ((struct elf_link_hash_entry *, struct elf_info_failed *));
static boolean elf_adjust_dynamic_symbol
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_find_version_dependencies
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_find_version_dependencies
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_assign_sym_version
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_renumber_dynsyms
  PARAMS ((struct elf_link_hash_entry *, PTR));

/* Given an ELF BFD, add symbols to the global hash table as
   appropriate.  */

boolean
elf_bfd_link_add_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  switch (bfd_get_format (abfd))
    {
    case bfd_object:
      return elf_link_add_object_symbols (abfd, info);
    case bfd_archive:
      return elf_link_add_archive_symbols (abfd, info);
    default:
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }
}


/* Add symbols from an ELF archive file to the linker hash table.  We
   don't use _bfd_generic_link_add_archive_symbols because of a
   problem which arises on UnixWare.  The UnixWare libc.so is an
   archive which includes an entry libc.so.1 which defines a bunch of
   symbols.  The libc.so archive also includes a number of other
   object files, which also define symbols, some of which are the same
   as those defined in libc.so.1.  Correct linking requires that we
   consider each object file in turn, and include it if it defines any
   symbols we need.  _bfd_generic_link_add_archive_symbols does not do
   this; it looks through the list of undefined symbols, and includes
   any object file which defines them.  When this algorithm is used on
   UnixWare, it winds up pulling in libc.so.1 early and defining a
   bunch of symbols.  This means that some of the other objects in the
   archive are not included in the link, which is incorrect since they
   precede libc.so.1 in the archive.

   Fortunately, ELF archive handling is simpler than that done by
   _bfd_generic_link_add_archive_symbols, which has to allow for a.out
   oddities.  In ELF, if we find a symbol in the archive map, and the
   symbol is currently undefined, we know that we must pull in that
   object file.

   Unfortunately, we do have to make multiple passes over the symbol
   table until nothing further is resolved.  */

static boolean
elf_link_add_archive_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  symindex c;
  boolean *defined = NULL;
  boolean *included = NULL;
  carsym *symdefs;
  boolean loop;

  if (! bfd_has_map (abfd))
    {
      /* An empty archive is a special case.  */
      if (bfd_openr_next_archived_file (abfd, (bfd *) NULL) == NULL)
	return true;
      bfd_set_error (bfd_error_no_armap);
      return false;
    }

  /* Keep track of all symbols we know to be already defined, and all
     files we know to be already included.  This is to speed up the
     second and subsequent passes.  */
  c = bfd_ardata (abfd)->symdef_count;
  if (c == 0)
    return true;
  defined = (boolean *) bfd_malloc (c * sizeof (boolean));
  included = (boolean *) bfd_malloc (c * sizeof (boolean));
  if (defined == (boolean *) NULL || included == (boolean *) NULL)
    goto error_return;
  memset (defined, 0, c * sizeof (boolean));
  memset (included, 0, c * sizeof (boolean));

  symdefs = bfd_ardata (abfd)->symdefs;

  do
    {
      file_ptr last;
      symindex i;
      carsym *symdef;
      carsym *symdefend;

      loop = false;
      last = -1;

      symdef = symdefs;
      symdefend = symdef + c;
      for (i = 0; symdef < symdefend; symdef++, i++)
	{
	  struct elf_link_hash_entry *h;
	  bfd *element;
	  struct bfd_link_hash_entry *undefs_tail;
	  symindex mark;

	  if (defined[i] || included[i])
	    continue;
	  if (symdef->file_offset == last)
	    {
	      included[i] = true;
	      continue;
	    }

	  h = elf_link_hash_lookup (elf_hash_table (info), symdef->name,
				    false, false, false);

	  if (h == NULL)
	    {
	      char *p, *copy;

	      /* If this is a default version (the name contains @@),
		 look up the symbol again without the version.  The
		 effect is that references to the symbol without the
		 version will be matched by the default symbol in the
		 archive.  */

	      p = strchr (symdef->name, ELF_VER_CHR);
	      if (p == NULL || p[1] != ELF_VER_CHR)
		continue;

	      copy = bfd_alloc (abfd, p - symdef->name + 1);
	      if (copy == NULL)
		goto error_return;
	      memcpy (copy, symdef->name, p - symdef->name);
	      copy[p - symdef->name] = '\0';

	      h = elf_link_hash_lookup (elf_hash_table (info), copy,
					false, false, false);

	      bfd_release (abfd, copy);
	    }

	  if (h == NULL)
	    continue;

	  if (h->root.type != bfd_link_hash_undefined)
	    {
	      if (h->root.type != bfd_link_hash_undefweak)
		defined[i] = true;
	      continue;
	    }

	  /* We need to include this archive member.  */

	  element = _bfd_get_elt_at_filepos (abfd, symdef->file_offset);
	  if (element == (bfd *) NULL)
	    goto error_return;

	  if (! bfd_check_format (element, bfd_object))
	    goto error_return;

	  /* Doublecheck that we have not included this object
	     already--it should be impossible, but there may be
	     something wrong with the archive.  */
	  if (element->archive_pass != 0)
	    {
	      bfd_set_error (bfd_error_bad_value);
	      goto error_return;
	    }
	  element->archive_pass = 1;

	  undefs_tail = info->hash->undefs_tail;

	  if (! (*info->callbacks->add_archive_element) (info, element,
							 symdef->name))
	    goto error_return;
	  if (! elf_link_add_object_symbols (element, info))
	    goto error_return;

	  /* If there are any new undefined symbols, we need to make
	     another pass through the archive in order to see whether
	     they can be defined.  FIXME: This isn't perfect, because
	     common symbols wind up on undefs_tail and because an
	     undefined symbol which is defined later on in this pass
	     does not require another pass.  This isn't a bug, but it
	     does make the code less efficient than it could be.  */
	  if (undefs_tail != info->hash->undefs_tail)
	    loop = true;

	  /* Look backward to mark all symbols from this object file
	     which we have already seen in this pass.  */
	  mark = i;
	  do
	    {
	      included[mark] = true;
	      if (mark == 0)
		break;
	      --mark;
	    }
	  while (symdefs[mark].file_offset == symdef->file_offset);

	  /* We mark subsequent symbols from this object file as we go
	     on through the loop.  */
	  last = symdef->file_offset;
	}
    }
  while (loop);

  free (defined);
  free (included);

  return true;

 error_return:
  if (defined != (boolean *) NULL)
    free (defined);
  if (included != (boolean *) NULL)
    free (included);
  return false;
}

/* This function is called when we want to define a new symbol.  It
   handles the various cases which arise when we find a definition in
   a dynamic object, or when there is already a definition in a
   dynamic object.  The new symbol is described by NAME, SYM, PSEC,
   and PVALUE.  We set SYM_HASH to the hash table entry.  We set
   OVERRIDE if the old symbol is overriding a new definition.  We set
   TYPE_CHANGE_OK if it is OK for the type to change.  We set
   SIZE_CHANGE_OK if it is OK for the size to change.  By OK to
   change, we mean that we shouldn't warn if the type or size does
   change.  */

static boolean
elf_merge_symbol (abfd, info, name, sym, psec, pvalue, sym_hash,
		  override, type_change_ok, size_change_ok)
     bfd *abfd;
     struct bfd_link_info *info;
     const char *name;
     Elf_Internal_Sym *sym;
     asection **psec;
     bfd_vma *pvalue;
     struct elf_link_hash_entry **sym_hash;
     boolean *override;
     boolean *type_change_ok;
     boolean *size_change_ok;
{
  asection *sec;
  struct elf_link_hash_entry *h;
  int bind;
  bfd *oldbfd;
  boolean newdyn, olddyn, olddef, newdef, newdyncommon, olddyncommon;

  *override = false;
  *type_change_ok = false;
  *size_change_ok = false;

  sec = *psec;
  bind = ELF_ST_BIND (sym->st_info);

  if (! bfd_is_und_section (sec))
    h = elf_link_hash_lookup (elf_hash_table (info), name, true, false, false);
  else
    h = ((struct elf_link_hash_entry *)
	 bfd_wrapped_link_hash_lookup (abfd, info, name, true, false, false));
  if (h == NULL)
    return false;
  *sym_hash = h;

  /* This code is for coping with dynamic objects, and is only useful
     if we are doing an ELF link.  */
  if (info->hash->creator != abfd->xvec)
    return true;

  /* For merging, we only care about real symbols.  */

  while (h->root.type == bfd_link_hash_indirect
	 || h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* If we just created the symbol, mark it as being an ELF symbol.
     Other than that, there is nothing to do--there is no merge issue
     with a newly defined symbol--so we just return.  */

  if (h->root.type == bfd_link_hash_new)
    {
      h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;
      return true;
    }

  /* OLDBFD is a BFD associated with the existing symbol.  */

  switch (h->root.type)
    {
    default:
      oldbfd = NULL;
      break;

    case bfd_link_hash_undefined:
    case bfd_link_hash_undefweak:
      oldbfd = h->root.u.undef.abfd;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      oldbfd = h->root.u.def.section->owner;
      break;

    case bfd_link_hash_common:
      oldbfd = h->root.u.c.p->section->owner;
      break;
    }

  /* NEWDYN and OLDDYN indicate whether the new or old symbol,
     respectively, is from a dynamic object.  */

  if ((abfd->flags & DYNAMIC) != 0)
    newdyn = true;
  else
    newdyn = false;

  if (oldbfd == NULL || (oldbfd->flags & DYNAMIC) == 0)
    olddyn = false;
  else
    olddyn = true;

  /* NEWDEF and OLDDEF indicate whether the new or old symbol,
     respectively, appear to be a definition rather than reference.  */

  if (bfd_is_und_section (sec) || bfd_is_com_section (sec))
    newdef = false;
  else
    newdef = true;

  if (h->root.type == bfd_link_hash_undefined
      || h->root.type == bfd_link_hash_undefweak
      || h->root.type == bfd_link_hash_common)
    olddef = false;
  else
    olddef = true;

  /* NEWDYNCOMMON and OLDDYNCOMMON indicate whether the new or old
     symbol, respectively, appears to be a common symbol in a dynamic
     object.  If a symbol appears in an uninitialized section, and is
     not weak, and is not a function, then it may be a common symbol
     which was resolved when the dynamic object was created.  We want
     to treat such symbols specially, because they raise special
     considerations when setting the symbol size: if the symbol
     appears as a common symbol in a regular object, and the size in
     the regular object is larger, we must make sure that we use the
     larger size.  This problematic case can always be avoided in C,
     but it must be handled correctly when using Fortran shared
     libraries.

     Note that if NEWDYNCOMMON is set, NEWDEF will be set, and
     likewise for OLDDYNCOMMON and OLDDEF.

     Note that this test is just a heuristic, and that it is quite
     possible to have an uninitialized symbol in a shared object which
     is really a definition, rather than a common symbol.  This could
     lead to some minor confusion when the symbol really is a common
     symbol in some regular object.  However, I think it will be
     harmless.  */

  if (newdyn
      && newdef
      && (sec->flags & SEC_ALLOC) != 0
      && (sec->flags & SEC_LOAD) == 0
      && sym->st_size > 0
      && bind != STB_WEAK
      && ELF_ST_TYPE (sym->st_info) != STT_FUNC)
    newdyncommon = true;
  else
    newdyncommon = false;

  if (olddyn
      && olddef
      && h->root.type == bfd_link_hash_defined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
      && (h->root.u.def.section->flags & SEC_ALLOC) != 0
      && (h->root.u.def.section->flags & SEC_LOAD) == 0
      && h->size > 0
      && h->type != STT_FUNC)
    olddyncommon = true;
  else
    olddyncommon = false;

  /* It's OK to change the type if either the existing symbol or the
     new symbol is weak.  */

  if (h->root.type == bfd_link_hash_defweak
      || h->root.type == bfd_link_hash_undefweak
      || bind == STB_WEAK)
    *type_change_ok = true;

  /* It's OK to change the size if either the existing symbol or the
     new symbol is weak, or if the old symbol is undefined.  */

  if (*type_change_ok
      || h->root.type == bfd_link_hash_undefined)
    *size_change_ok = true;

  /* If both the old and the new symbols look like common symbols in a
     dynamic object, set the size of the symbol to the larger of the
     two.  */

  if (olddyncommon
      && newdyncommon
      && sym->st_size != h->size)
    {
      /* Since we think we have two common symbols, issue a multiple
         common warning if desired.  Note that we only warn if the
         size is different.  If the size is the same, we simply let
         the old symbol override the new one as normally happens with
         symbols defined in dynamic objects.  */

      if (! ((*info->callbacks->multiple_common)
	     (info, h->root.root.string, oldbfd, bfd_link_hash_common,
	      h->size, abfd, bfd_link_hash_common, sym->st_size)))
	return false;

      if (sym->st_size > h->size)
	h->size = sym->st_size;

      *size_change_ok = true;
    }

  /* If we are looking at a dynamic object, and we have found a
     definition, we need to see if the symbol was already defined by
     some other object.  If so, we want to use the existing
     definition, and we do not want to report a multiple symbol
     definition error; we do this by clobbering *PSEC to be
     bfd_und_section_ptr.

     We treat a common symbol as a definition if the symbol in the
     shared library is a function, since common symbols always
     represent variables; this can cause confusion in principle, but
     any such confusion would seem to indicate an erroneous program or
     shared library.  We also permit a common symbol in a regular
     object to override a weak symbol in a shared object.  */

  if (newdyn
      && newdef
      && (olddef
	  || (h->root.type == bfd_link_hash_common
	      && (bind == STB_WEAK
		  || ELF_ST_TYPE (sym->st_info) == STT_FUNC))))
    {
      *override = true;
      newdef = false;
      newdyncommon = false;

      *psec = sec = bfd_und_section_ptr;
      *size_change_ok = true;

      /* If we get here when the old symbol is a common symbol, then
         we are explicitly letting it override a weak symbol or
         function in a dynamic object, and we don't want to warn about
         a type change.  If the old symbol is a defined symbol, a type
         change warning may still be appropriate.  */

      if (h->root.type == bfd_link_hash_common)
	*type_change_ok = true;
    }

  /* Handle the special case of an old common symbol merging with a
     new symbol which looks like a common symbol in a shared object.
     We change *PSEC and *PVALUE to make the new symbol look like a
     common symbol, and let _bfd_generic_link_add_one_symbol will do
     the right thing.  */

  if (newdyncommon
      && h->root.type == bfd_link_hash_common)
    {
      *override = true;
      newdef = false;
      newdyncommon = false;
      *pvalue = sym->st_size;
      *psec = sec = bfd_com_section_ptr;
      *size_change_ok = true;
    }

  /* If the old symbol is from a dynamic object, and the new symbol is
     a definition which is not from a dynamic object, then the new
     symbol overrides the old symbol.  Symbols from regular files
     always take precedence over symbols from dynamic objects, even if
     they are defined after the dynamic object in the link.

     As above, we again permit a common symbol in a regular object to
     override a definition in a shared object if the shared object
     symbol is a function or is weak.  */

  if (! newdyn
      && (newdef
	  || (bfd_is_com_section (sec)
	      && (h->root.type == bfd_link_hash_defweak
		  || h->type == STT_FUNC)))
      && olddyn
      && olddef
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0)
    {
      /* Change the hash table entry to undefined, and let
	 _bfd_generic_link_add_one_symbol do the right thing with the
	 new definition.  */

      h->root.type = bfd_link_hash_undefined;
      h->root.u.undef.abfd = h->root.u.def.section->owner;
      *size_change_ok = true;

      olddef = false;
      olddyncommon = false;

      /* We again permit a type change when a common symbol may be
         overriding a function.  */

      if (bfd_is_com_section (sec))
	*type_change_ok = true;

      /* This union may have been set to be non-NULL when this symbol
	 was seen in a dynamic object.  We must force the union to be
	 NULL, so that it is correct for a regular symbol.  */

      h->verinfo.vertree = NULL;

      /* In this special case, if H is the target of an indirection,
         we want the caller to frob with H rather than with the
         indirect symbol.  That will permit the caller to redefine the
         target of the indirection, rather than the indirect symbol
         itself.  FIXME: This will break the -y option if we store a
         symbol with a different name.  */
      *sym_hash = h;
    }

  /* Handle the special case of a new common symbol merging with an
     old symbol that looks like it might be a common symbol defined in
     a shared object.  Note that we have already handled the case in
     which a new common symbol should simply override the definition
     in the shared library.  */

  if (! newdyn
      && bfd_is_com_section (sec)
      && olddyncommon)
    {
      /* It would be best if we could set the hash table entry to a
	 common symbol, but we don't know what to use for the section
	 or the alignment.  */
      if (! ((*info->callbacks->multiple_common)
	     (info, h->root.root.string, oldbfd, bfd_link_hash_common,
	      h->size, abfd, bfd_link_hash_common, sym->st_size)))
	return false;

      /* If the predumed common symbol in the dynamic object is
         larger, pretend that the new symbol has its size.  */

      if (h->size > *pvalue)
	*pvalue = h->size;

      /* FIXME: We no longer know the alignment required by the symbol
	 in the dynamic object, so we just wind up using the one from
	 the regular object.  */

      olddef = false;
      olddyncommon = false;

      h->root.type = bfd_link_hash_undefined;
      h->root.u.undef.abfd = h->root.u.def.section->owner;

      *size_change_ok = true;
      *type_change_ok = true;

      h->verinfo.vertree = NULL;
    }

  return true;
}

/* Add symbols from an ELF object file to the linker hash table.  */

static boolean
elf_link_add_object_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  boolean (*add_symbol_hook) PARAMS ((bfd *, struct bfd_link_info *,
				      const Elf_Internal_Sym *,
				      const char **, flagword *,
				      asection **, bfd_vma *));
  boolean (*check_relocs) PARAMS ((bfd *, struct bfd_link_info *,
				   asection *, const Elf_Internal_Rela *));
  boolean collect;
  Elf_Internal_Shdr *hdr;
  size_t symcount;
  size_t extsymcount;
  size_t extsymoff;
  Elf_External_Sym *buf = NULL;
  struct elf_link_hash_entry **sym_hash;
  boolean dynamic;
  bfd_byte *dynver = NULL;
  Elf_External_Versym *extversym = NULL;
  Elf_External_Versym *ever;
  Elf_External_Dyn *dynbuf = NULL;
  struct elf_link_hash_entry *weaks;
  Elf_External_Sym *esym;
  Elf_External_Sym *esymend;

  add_symbol_hook = get_elf_backend_data (abfd)->elf_add_symbol_hook;
  collect = get_elf_backend_data (abfd)->collect;

  if ((abfd->flags & DYNAMIC) == 0)
    dynamic = false;
  else
    {
      dynamic = true;

      /* You can't use -r against a dynamic object.  Also, there's no
	 hope of using a dynamic object which does not exactly match
	 the format of the output file.  */
      if (info->relocateable || info->hash->creator != abfd->xvec)
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  goto error_return;
	}
    }

  /* As a GNU extension, any input sections which are named
     .gnu.warning.SYMBOL are treated as warning symbols for the given
     symbol.  This differs from .gnu.warning sections, which generate
     warnings when they are included in an output file.  */
  if (! info->shared)
    {
      asection *s;

      for (s = abfd->sections; s != NULL; s = s->next)
	{
	  const char *name;

	  name = bfd_get_section_name (abfd, s);
	  if (strncmp (name, ".gnu.warning.", sizeof ".gnu.warning." - 1) == 0)
	    {
	      char *msg;
	      bfd_size_type sz;

	      name += sizeof ".gnu.warning." - 1;

	      /* If this is a shared object, then look up the symbol
		 in the hash table.  If it is there, and it is already
		 been defined, then we will not be using the entry
		 from this shared object, so we don't need to warn.
		 FIXME: If we see the definition in a regular object
		 later on, we will warn, but we shouldn't.  The only
		 fix is to keep track of what warnings we are supposed
		 to emit, and then handle them all at the end of the
		 link.  */
	      if (dynamic && abfd->xvec == info->hash->creator)
		{
		  struct elf_link_hash_entry *h;

		  h = elf_link_hash_lookup (elf_hash_table (info), name,
					    false, false, true);

		  /* FIXME: What about bfd_link_hash_common?  */
		  if (h != NULL
		      && (h->root.type == bfd_link_hash_defined
			  || h->root.type == bfd_link_hash_defweak))
		    {
		      /* We don't want to issue this warning.  Clobber
                         the section size so that the warning does not
                         get copied into the output file.  */
		      s->_raw_size = 0;
		      continue;
		    }
		}

	      sz = bfd_section_size (abfd, s);
	      msg = (char *) bfd_alloc (abfd, sz + 1);
	      if (msg == NULL)
		goto error_return;

	      if (! bfd_get_section_contents (abfd, s, msg, (file_ptr) 0, sz))
		goto error_return;
	      msg[sz] = '\0';

	      if (! (_bfd_generic_link_add_one_symbol
		     (info, abfd, name, BSF_WARNING, s, (bfd_vma) 0, msg,
		      false, collect, (struct bfd_link_hash_entry **) NULL)))
		goto error_return;

	      if (! info->relocateable)
		{
		  /* Clobber the section size so that the warning does
                     not get copied into the output file.  */
		  s->_raw_size = 0;
		}
	    }
	}
    }

  /* If this is a dynamic object, we always link against the .dynsym
     symbol table, not the .symtab symbol table.  The dynamic linker
     will only see the .dynsym symbol table, so there is no reason to
     look at .symtab for a dynamic object.  */

  if (! dynamic || elf_dynsymtab (abfd) == 0)
    hdr = &elf_tdata (abfd)->symtab_hdr;
  else
    hdr = &elf_tdata (abfd)->dynsymtab_hdr;

  if (dynamic)
    {
      /* Read in any version definitions.  */

      if (! _bfd_elf_slurp_version_tables (abfd))
	goto error_return;

      /* Read in the symbol versions, but don't bother to convert them
         to internal format.  */
      if (elf_dynversym (abfd) != 0)
	{
	  Elf_Internal_Shdr *versymhdr;

	  versymhdr = &elf_tdata (abfd)->dynversym_hdr;
	  extversym = (Elf_External_Versym *) bfd_malloc (hdr->sh_size);
	  if (extversym == NULL)
	    goto error_return;
	  if (bfd_seek (abfd, versymhdr->sh_offset, SEEK_SET) != 0
	      || (bfd_read ((PTR) extversym, 1, versymhdr->sh_size, abfd)
		  != versymhdr->sh_size))
	    goto error_return;
	}
    }

  symcount = hdr->sh_size / sizeof (Elf_External_Sym);

  /* The sh_info field of the symtab header tells us where the
     external symbols start.  We don't care about the local symbols at
     this point.  */
  if (elf_bad_symtab (abfd))
    {
      extsymcount = symcount;
      extsymoff = 0;
    }
  else
    {
      extsymcount = symcount - hdr->sh_info;
      extsymoff = hdr->sh_info;
    }

  buf = ((Elf_External_Sym *)
	 bfd_malloc (extsymcount * sizeof (Elf_External_Sym)));
  if (buf == NULL && extsymcount != 0)
    goto error_return;

  /* We store a pointer to the hash table entry for each external
     symbol.  */
  sym_hash = ((struct elf_link_hash_entry **)
	      bfd_alloc (abfd,
			 extsymcount * sizeof (struct elf_link_hash_entry *)));
  if (sym_hash == NULL)
    goto error_return;
  elf_sym_hashes (abfd) = sym_hash;

  if (! dynamic)
    {
      /* If we are creating a shared library, create all the dynamic
         sections immediately.  We need to attach them to something,
         so we attach them to this BFD, provided it is the right
         format.  FIXME: If there are no input BFD's of the same
         format as the output, we can't make a shared library.  */
      if (info->shared
	  && ! elf_hash_table (info)->dynamic_sections_created
	  && abfd->xvec == info->hash->creator)
	{
	  if (! elf_link_create_dynamic_sections (abfd, info))
	    goto error_return;
	}
    }
  else
    {
      asection *s;
      boolean add_needed;
      const char *name;
      bfd_size_type oldsize;
      bfd_size_type strindex;

      /* Find the name to use in a DT_NEEDED entry that refers to this
	 object.  If the object has a DT_SONAME entry, we use it.
	 Otherwise, if the generic linker stuck something in
	 elf_dt_name, we use that.  Otherwise, we just use the file
	 name.  If the generic linker put a null string into
	 elf_dt_name, we don't make a DT_NEEDED entry at all, even if
	 there is a DT_SONAME entry.  */
      add_needed = true;
      name = bfd_get_filename (abfd);
      if (elf_dt_name (abfd) != NULL)
	{
	  name = elf_dt_name (abfd);
	  if (*name == '\0')
	    add_needed = false;
	}
      s = bfd_get_section_by_name (abfd, ".dynamic");
      if (s != NULL)
	{
	  Elf_External_Dyn *extdyn;
	  Elf_External_Dyn *extdynend;
	  int elfsec;
	  unsigned long link;
	  int rpath;
	  int runpath;

	  dynbuf = (Elf_External_Dyn *) bfd_malloc ((size_t) s->_raw_size);
	  if (dynbuf == NULL)
	    goto error_return;

	  if (! bfd_get_section_contents (abfd, s, (PTR) dynbuf,
					  (file_ptr) 0, s->_raw_size))
	    goto error_return;

	  elfsec = _bfd_elf_section_from_bfd_section (abfd, s);
	  if (elfsec == -1)
	    goto error_return;
	  link = elf_elfsections (abfd)[elfsec]->sh_link;

	  extdyn = dynbuf;
	  extdynend = extdyn + s->_raw_size / sizeof (Elf_External_Dyn);
	  rpath = 0;
	  runpath = 0;
	  for (; extdyn < extdynend; extdyn++)
	    {
	      Elf_Internal_Dyn dyn;

	      elf_swap_dyn_in (abfd, extdyn, &dyn);
	      if (dyn.d_tag == DT_SONAME)
		{
		  name = bfd_elf_string_from_elf_section (abfd, link,
							  dyn.d_un.d_val);
		  if (name == NULL)
		    goto error_return;
		}
	      if (dyn.d_tag == DT_NEEDED)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;

		  n = ((struct bfd_link_needed_list *)
		       bfd_alloc (abfd, sizeof (struct bfd_link_needed_list)));
		  fnm = bfd_elf_string_from_elf_section (abfd, link,
							 dyn.d_un.d_val);
		  if (n == NULL || fnm == NULL)
		    goto error_return;
		  anm = bfd_alloc (abfd, strlen (fnm) + 1);
		  if (anm == NULL)
		    goto error_return;
		  strcpy (anm, fnm);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = &elf_hash_table (info)->needed;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		}
	      if (dyn.d_tag == DT_RUNPATH)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;

		  /* When we see DT_RPATH before DT_RUNPATH, we have
		     to clear runpath.  Do _NOT_ bfd_release, as that
		     frees all more recently bfd_alloc'd blocks as
		     well.  */
		  if (rpath && elf_hash_table (info)->runpath)
		    elf_hash_table (info)->runpath = NULL;

		  n = ((struct bfd_link_needed_list *)
		       bfd_alloc (abfd, sizeof (struct bfd_link_needed_list)));
		  fnm = bfd_elf_string_from_elf_section (abfd, link,
							 dyn.d_un.d_val);
		  if (n == NULL || fnm == NULL)
		    goto error_return;
		  anm = bfd_alloc (abfd, strlen (fnm) + 1);
		  if (anm == NULL)
		    goto error_return;
		  strcpy (anm, fnm);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = &elf_hash_table (info)->runpath;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		  runpath = 1;
		  rpath = 0;
		}
	      /* Ignore DT_RPATH if we have seen DT_RUNPATH.  */
	      if (!runpath && dyn.d_tag == DT_RPATH)
	        {
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;

		  n = ((struct bfd_link_needed_list *)
		       bfd_alloc (abfd, sizeof (struct bfd_link_needed_list)));
		  fnm = bfd_elf_string_from_elf_section (abfd, link,
							 dyn.d_un.d_val);
		  if (n == NULL || fnm == NULL)
		    goto error_return;
		  anm = bfd_alloc (abfd, strlen (fnm) + 1);
		  if (anm == NULL)
		    goto error_return;
		  strcpy (anm, fnm);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = &elf_hash_table (info)->runpath;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		  rpath = 1;
		}
	    }

	  free (dynbuf);
	  dynbuf = NULL;
	}

      /* We do not want to include any of the sections in a dynamic
	 object in the output file.  We hack by simply clobbering the
	 list of sections in the BFD.  This could be handled more
	 cleanly by, say, a new section flag; the existing
	 SEC_NEVER_LOAD flag is not the one we want, because that one
	 still implies that the section takes up space in the output
	 file.  */
      abfd->sections = NULL;
      abfd->section_count = 0;

      /* If this is the first dynamic object found in the link, create
	 the special sections required for dynamic linking.  */
      if (! elf_hash_table (info)->dynamic_sections_created)
	{
	  if (! elf_link_create_dynamic_sections (abfd, info))
	    goto error_return;
	}

      if (add_needed)
	{
	  /* Add a DT_NEEDED entry for this dynamic object.  */
	  oldsize = _bfd_stringtab_size (elf_hash_table (info)->dynstr);
	  strindex = _bfd_stringtab_add (elf_hash_table (info)->dynstr, name,
					 true, false);
	  if (strindex == (bfd_size_type) -1)
	    goto error_return;

	  if (oldsize == _bfd_stringtab_size (elf_hash_table (info)->dynstr))
	    {
	      asection *sdyn;
	      Elf_External_Dyn *dyncon, *dynconend;

	      /* The hash table size did not change, which means that
		 the dynamic object name was already entered.  If we
		 have already included this dynamic object in the
		 link, just ignore it.  There is no reason to include
		 a particular dynamic object more than once.  */
	      sdyn = bfd_get_section_by_name (elf_hash_table (info)->dynobj,
					      ".dynamic");
	      BFD_ASSERT (sdyn != NULL);

	      dyncon = (Elf_External_Dyn *) sdyn->contents;
	      dynconend = (Elf_External_Dyn *) (sdyn->contents +
						sdyn->_raw_size);
	      for (; dyncon < dynconend; dyncon++)
		{
		  Elf_Internal_Dyn dyn;

		  elf_swap_dyn_in (elf_hash_table (info)->dynobj, dyncon,
				   &dyn);
		  if (dyn.d_tag == DT_NEEDED
		      && dyn.d_un.d_val == strindex)
		    {
		      if (buf != NULL)
			free (buf);
		      if (extversym != NULL)
			free (extversym);
		      return true;
		    }
		}
	    }

	  if (! elf_add_dynamic_entry (info, DT_NEEDED, strindex))
	    goto error_return;
	}

      /* Save the SONAME, if there is one, because sometimes the
         linker emulation code will need to know it.  */
      if (*name == '\0')
	name = bfd_get_filename (abfd);
      elf_dt_name (abfd) = name;
    }

  if (bfd_seek (abfd,
		hdr->sh_offset + extsymoff * sizeof (Elf_External_Sym),
		SEEK_SET) != 0
      || (bfd_read ((PTR) buf, sizeof (Elf_External_Sym), extsymcount, abfd)
	  != extsymcount * sizeof (Elf_External_Sym)))
    goto error_return;

  weaks = NULL;

  ever = extversym != NULL ? extversym + extsymoff : NULL;
  esymend = buf + extsymcount;
  for (esym = buf;
       esym < esymend;
       esym++, sym_hash++, ever = (ever != NULL ? ever + 1 : NULL))
    {
      Elf_Internal_Sym sym;
      int bind;
      bfd_vma value;
      asection *sec;
      flagword flags;
      const char *name;
      struct elf_link_hash_entry *h;
      boolean definition;
      boolean size_change_ok, type_change_ok;
      boolean new_weakdef;
      unsigned int old_alignment;

      elf_swap_symbol_in (abfd, esym, &sym);

      flags = BSF_NO_FLAGS;
      sec = NULL;
      value = sym.st_value;
      *sym_hash = NULL;

      bind = ELF_ST_BIND (sym.st_info);
      if (bind == STB_LOCAL)
	{
	  /* This should be impossible, since ELF requires that all
	     global symbols follow all local symbols, and that sh_info
	     point to the first global symbol.  Unfortunatealy, Irix 5
	     screws this up.  */
	  continue;
	}
      else if (bind == STB_GLOBAL)
	{
	  if (sym.st_shndx != SHN_UNDEF
	      && sym.st_shndx != SHN_COMMON)
	    flags = BSF_GLOBAL;
	  else
	    flags = 0;
	}
      else if (bind == STB_WEAK)
	flags = BSF_WEAK;
      else
	{
	  /* Leave it up to the processor backend.  */
	}

      if (sym.st_shndx == SHN_UNDEF)
	sec = bfd_und_section_ptr;
      else if (sym.st_shndx > 0 && sym.st_shndx < SHN_LORESERVE)
	{
	  sec = section_from_elf_index (abfd, sym.st_shndx);
	  if (sec == NULL)
	    sec = bfd_abs_section_ptr;
	  else if ((abfd->flags & (EXEC_P | DYNAMIC)) != 0)
	    value -= sec->vma;
	}
      else if (sym.st_shndx == SHN_ABS)
	sec = bfd_abs_section_ptr;
      else if (sym.st_shndx == SHN_COMMON)
	{
	  sec = bfd_com_section_ptr;
	  /* What ELF calls the size we call the value.  What ELF
	     calls the value we call the alignment.  */
	  value = sym.st_size;
	}
      else
	{
	  /* Leave it up to the processor backend.  */
	}

      name = bfd_elf_string_from_elf_section (abfd, hdr->sh_link, sym.st_name);
      if (name == (const char *) NULL)
	goto error_return;

      if (add_symbol_hook)
	{
	  if (! (*add_symbol_hook) (abfd, info, &sym, &name, &flags, &sec,
				    &value))
	    goto error_return;

	  /* The hook function sets the name to NULL if this symbol
	     should be skipped for some reason.  */
	  if (name == (const char *) NULL)
	    continue;
	}

      /* Sanity check that all possibilities were handled.  */
      if (sec == (asection *) NULL)
	{
	  bfd_set_error (bfd_error_bad_value);
	  goto error_return;
	}

      if (bfd_is_und_section (sec)
	  || bfd_is_com_section (sec))
	definition = false;
      else
	definition = true;

      size_change_ok = false;
      type_change_ok = get_elf_backend_data (abfd)->type_change_ok;
      old_alignment = 0;
      if (info->hash->creator->flavour == bfd_target_elf_flavour)
	{
	  Elf_Internal_Versym iver;
	  unsigned int vernum = 0;
	  boolean override;

	  if (ever != NULL)
	    {
	      _bfd_elf_swap_versym_in (abfd, ever, &iver);
	      vernum = iver.vs_vers & VERSYM_VERSION;

	      /* If this is a hidden symbol, or if it is not version
                 1, we append the version name to the symbol name.
                 However, we do not modify a non-hidden absolute
                 symbol, because it might be the version symbol
                 itself.  FIXME: What if it isn't?  */
	      if ((iver.vs_vers & VERSYM_HIDDEN) != 0
		  || (vernum > 1 && ! bfd_is_abs_section (sec)))
		{
		  const char *verstr;
		  int namelen, newlen;
		  char *newname, *p;

		  if (sym.st_shndx != SHN_UNDEF)
		    {
		      if (vernum > elf_tdata (abfd)->dynverdef_hdr.sh_info)
			{
			  (*_bfd_error_handler)
			    ("%s: %s: invalid version %u (max %d)",
			     abfd->filename, name, vernum,
			     elf_tdata (abfd)->dynverdef_hdr.sh_info);
			  bfd_set_error (bfd_error_bad_value);
			  goto error_return;
			}
		      else if (vernum > 1)
			verstr =
			  elf_tdata (abfd)->verdef[vernum - 1].vd_nodename;
		      else
			verstr = "";
		    }
		  else
		    {
		      /* We cannot simply test for the number of
			 entries in the VERNEED section since the
			 numbers for the needed versions do not start
			 at 0.  */
		      Elf_Internal_Verneed *t;

		      verstr = NULL;
		      for (t = elf_tdata (abfd)->verref;
			   t != NULL;
			   t = t->vn_nextref)
			{
			  Elf_Internal_Vernaux *a;

			  for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
			    {
			      if (a->vna_other == vernum)
				{
				  verstr = a->vna_nodename;
				  break;
				}
			    }
			  if (a != NULL)
			    break;
			}
		      if (verstr == NULL)
			{
			  (*_bfd_error_handler)
			    ("%s: %s: invalid needed version %d",
			     abfd->filename, name, vernum);
			  bfd_set_error (bfd_error_bad_value);
			  goto error_return;
			}
		    }

		  namelen = strlen (name);
		  newlen = namelen + strlen (verstr) + 2;
		  if ((iver.vs_vers & VERSYM_HIDDEN) == 0)
		    ++newlen;

		  newname = (char *) bfd_alloc (abfd, newlen);
		  if (newname == NULL)
		    goto error_return;
		  strcpy (newname, name);
		  p = newname + namelen;
		  *p++ = ELF_VER_CHR;
		  if ((iver.vs_vers & VERSYM_HIDDEN) == 0)
		    *p++ = ELF_VER_CHR;
		  strcpy (p, verstr);

		  name = newname;
		}
	    }

	  if (! elf_merge_symbol (abfd, info, name, &sym, &sec, &value,
				  sym_hash, &override, &type_change_ok,
				  &size_change_ok))
	    goto error_return;

	  if (override)
	    definition = false;

	  h = *sym_hash;
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  /* Remember the old alignment if this is a common symbol, so
             that we don't reduce the alignment later on.  We can't
             check later, because _bfd_generic_link_add_one_symbol
             will set a default for the alignment which we want to
             override.  */
	  if (h->root.type == bfd_link_hash_common)
	    old_alignment = h->root.u.c.p->alignment_power;

	  if (elf_tdata (abfd)->verdef != NULL
	      && ! override
	      && vernum > 1
	      && definition)
	    h->verinfo.verdef = &elf_tdata (abfd)->verdef[vernum - 1];
	}

      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, name, flags, sec, value, (const char *) NULL,
	      false, collect, (struct bfd_link_hash_entry **) sym_hash)))
	goto error_return;

      h = *sym_hash;
      while (h->root.type == bfd_link_hash_indirect
	     || h->root.type == bfd_link_hash_warning)
	h = (struct elf_link_hash_entry *) h->root.u.i.link;
      *sym_hash = h;

      new_weakdef = false;
      if (dynamic
	  && definition
	  && (flags & BSF_WEAK) != 0
	  && ELF_ST_TYPE (sym.st_info) != STT_FUNC
	  && info->hash->creator->flavour == bfd_target_elf_flavour
	  && h->weakdef == NULL)
	{
	  /* Keep a list of all weak defined non function symbols from
	     a dynamic object, using the weakdef field.  Later in this
	     function we will set the weakdef field to the correct
	     value.  We only put non-function symbols from dynamic
	     objects on this list, because that happens to be the only
	     time we need to know the normal symbol corresponding to a
	     weak symbol, and the information is time consuming to
	     figure out.  If the weakdef field is not already NULL,
	     then this symbol was already defined by some previous
	     dynamic object, and we will be using that previous
	     definition anyhow.  */

	  h->weakdef = weaks;
	  weaks = h;
	  new_weakdef = true;
	}

      /* Set the alignment of a common symbol.  */
      if (sym.st_shndx == SHN_COMMON
	  && h->root.type == bfd_link_hash_common)
	{
	  unsigned int align;

	  align = bfd_log2 (sym.st_value);
	  if (align > old_alignment)
	    h->root.u.c.p->alignment_power = align;
	}

      if (info->hash->creator->flavour == bfd_target_elf_flavour)
	{
	  int old_flags;
	  boolean dynsym;
	  int new_flag;

	  /* Remember the symbol size and type.  */
	  if (sym.st_size != 0
	      && (definition || h->size == 0))
	    {
	      if (h->size != 0 && h->size != sym.st_size && ! size_change_ok)
		(*_bfd_error_handler)
		  ("Warning: size of symbol `%s' changed from %lu to %lu in %s",
		   name, (unsigned long) h->size, (unsigned long) sym.st_size,
		   bfd_get_filename (abfd));

	      h->size = sym.st_size;
	    }

	  /* If this is a common symbol, then we always want H->SIZE
             to be the size of the common symbol.  The code just above
             won't fix the size if a common symbol becomes larger.  We
             don't warn about a size change here, because that is
             covered by --warn-common.  */
	  if (h->root.type == bfd_link_hash_common)
	    h->size = h->root.u.c.size;

	  if (ELF_ST_TYPE (sym.st_info) != STT_NOTYPE
	      && (definition || h->type == STT_NOTYPE))
	    {
	      if (h->type != STT_NOTYPE
		  && h->type != ELF_ST_TYPE (sym.st_info)
		  && ! type_change_ok
		  /* Ignore  changes from data to text or _gp_disp */
  		  && !(h->type == STT_OBJECT && 
		       ELF_ST_TYPE (sym.st_info) <= STT_SECTION))
		(*_bfd_error_handler)
		  ("Warning: type of symbol `%s' changed from %d to %d in %s",
		   name, h->type, ELF_ST_TYPE (sym.st_info),
		   bfd_get_filename (abfd));

	      h->type = ELF_ST_TYPE (sym.st_info);
	    }

	  if (sym.st_other != 0
	      && (definition || h->other == 0))
	    h->other = sym.st_other;

	  /* Set a flag in the hash table entry indicating the type of
	     reference or definition we just found.  Keep a count of
	     the number of dynamic symbols we find.  A dynamic symbol
	     is one which is referenced or defined by both a regular
	     object and a shared object.  */
	  old_flags = h->elf_link_hash_flags;
	  dynsym = false;
	  if (! dynamic)
	    {
	      if (! definition)
		new_flag = ELF_LINK_HASH_REF_REGULAR;
	      else
		new_flag = ELF_LINK_HASH_DEF_REGULAR;
	      if (info->shared
		  || (old_flags & (ELF_LINK_HASH_DEF_DYNAMIC
				   | ELF_LINK_HASH_REF_DYNAMIC)) != 0)
		dynsym = true;
	    }
	  else
	    {
	      if (! definition)
		new_flag = ELF_LINK_HASH_REF_DYNAMIC;
	      else
		new_flag = ELF_LINK_HASH_DEF_DYNAMIC;
	      if ((old_flags & (ELF_LINK_HASH_DEF_REGULAR
				| ELF_LINK_HASH_REF_REGULAR)) != 0
		  || (h->weakdef != NULL
		      && ! new_weakdef
		      && h->weakdef->dynindx != -1))
		dynsym = true;
	    }

	  h->elf_link_hash_flags |= new_flag;

	  /* If this symbol has a version, and it is the default
             version, we create an indirect symbol from the default
             name to the fully decorated name.  This will cause
             external references which do not specify a version to be
             bound to this version of the symbol.  */
	  if (definition)
	    {
	      char *p;

	      p = strchr (name, ELF_VER_CHR);
	      if (p != NULL && p[1] == ELF_VER_CHR)
		{
		  char *shortname;
		  struct elf_link_hash_entry *hi;
		  boolean override;

		  shortname = bfd_hash_allocate (&info->hash->table,
						 p - name + 1);
		  if (shortname == NULL)
		    goto error_return;
		  strncpy (shortname, name, p - name);
		  shortname[p - name] = '\0';

		  /* We are going to create a new symbol.  Merge it
                     with any existing symbol with this name.  For the
                     purposes of the merge, act as though we were
                     defining the symbol we just defined, although we
                     actually going to define an indirect symbol.  */
		  if (! elf_merge_symbol (abfd, info, shortname, &sym, &sec,
					  &value, &hi, &override,
					  &type_change_ok, &size_change_ok))
		    goto error_return;

		  if (! override)
		    {
		      if (! (_bfd_generic_link_add_one_symbol
			     (info, abfd, shortname, BSF_INDIRECT,
			      bfd_ind_section_ptr, (bfd_vma) 0, name, false,
			      collect, (struct bfd_link_hash_entry **) &hi)))
			goto error_return;
		    }
		  else
		    {
		      /* In this case the symbol named SHORTNAME is
                         overriding the indirect symbol we want to
                         add.  We were planning on making SHORTNAME an
                         indirect symbol referring to NAME.  SHORTNAME
                         is the name without a version.  NAME is the
                         fully versioned name, and it is the default
                         version.

			 Overriding means that we already saw a
			 definition for the symbol SHORTNAME in a
			 regular object, and it is overriding the
			 symbol defined in the dynamic object.

			 When this happens, we actually want to change
			 NAME, the symbol we just added, to refer to
			 SHORTNAME.  This will cause references to
			 NAME in the shared object to become
			 references to SHORTNAME in the regular
			 object.  This is what we expect when we
			 override a function in a shared object: that
			 the references in the shared object will be
			 mapped to the definition in the regular
			 object.  */

		      while (hi->root.type == bfd_link_hash_indirect
			     || hi->root.type == bfd_link_hash_warning)
			hi = (struct elf_link_hash_entry *) hi->root.u.i.link;

		      h->root.type = bfd_link_hash_indirect;
		      h->root.u.i.link = (struct bfd_link_hash_entry *) hi;
		      if (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC)
			{
			  h->elf_link_hash_flags &=~ ELF_LINK_HASH_DEF_DYNAMIC;
			  hi->elf_link_hash_flags |= ELF_LINK_HASH_REF_DYNAMIC;
			  if (! _bfd_elf_link_record_dynamic_symbol (info, hi))
			    goto error_return;
			}

		      /* Now set HI to H, so that the following code
                         will set the other fields correctly.  */
		      hi = h;
		    }

		  /* If there is a duplicate definition somewhere,
		     then HI may not point to an indirect symbol.  We
		     will have reported an error to the user in that
		     case.  */

		  if (hi->root.type == bfd_link_hash_indirect)
		    {
		      struct elf_link_hash_entry *ht;

		      /* If the symbol became indirect, then we assume
			 that we have not seen a definition before.  */
		      BFD_ASSERT ((hi->elf_link_hash_flags
				   & (ELF_LINK_HASH_DEF_DYNAMIC
				      | ELF_LINK_HASH_DEF_REGULAR))
				  == 0);

		      ht = (struct elf_link_hash_entry *) hi->root.u.i.link;

		      /* Copy down any references that we may have
			 already seen to the symbol which just became
			 indirect.  */
		      ht->elf_link_hash_flags |=
			(hi->elf_link_hash_flags
			 & (ELF_LINK_HASH_REF_DYNAMIC
			    | ELF_LINK_HASH_REF_REGULAR));

		      /* Copy over the global table offset entry.
			 This may have been already set up by a
			 check_relocs routine.  */
		      if (ht->got_offset == (bfd_vma) -1)
			{
			  ht->got_offset = hi->got_offset;
			  hi->got_offset = (bfd_vma) -1;
			}
		      BFD_ASSERT (hi->got_offset == (bfd_vma) -1);

		      if (ht->dynindx == -1)
			{
			  ht->dynindx = hi->dynindx;
			  ht->dynstr_index = hi->dynstr_index;
			  hi->dynindx = -1;
			  hi->dynstr_index = 0;
			}
		      BFD_ASSERT (hi->dynindx == -1);

		      /* FIXME: There may be other information to copy
			 over for particular targets.  */

		      /* See if the new flags lead us to realize that
			 the symbol must be dynamic.  */
		      if (! dynsym)
			{
			  if (! dynamic)
			    {
			      if (info->shared
				  || ((hi->elf_link_hash_flags
				       & ELF_LINK_HASH_REF_DYNAMIC)
				      != 0))
				dynsym = true;
			    }
			  else
			    {
			      if ((hi->elf_link_hash_flags
				   & ELF_LINK_HASH_REF_REGULAR) != 0)
				dynsym = true;
			    }
			}
		    }

		  /* We also need to define an indirection from the
                     nondefault version of the symbol.  */

		  shortname = bfd_hash_allocate (&info->hash->table,
						 strlen (name));
		  if (shortname == NULL)
		    goto error_return;
		  strncpy (shortname, name, p - name);
		  strcpy (shortname + (p - name), p + 1);

		  /* Once again, merge with any existing symbol.  */
		  if (! elf_merge_symbol (abfd, info, shortname, &sym, &sec,
					  &value, &hi, &override,
					  &type_change_ok, &size_change_ok))
		    goto error_return;

		  if (override)
		    {
		      /* Here SHORTNAME is a versioned name, so we
                         don't expect to see the type of override we
                         do in the case above.  */
		      (*_bfd_error_handler)
			("%s: warning: unexpected redefinition of `%s'",
			 bfd_get_filename (abfd), shortname);
		    }
		  else
		    {
		      if (! (_bfd_generic_link_add_one_symbol
			     (info, abfd, shortname, BSF_INDIRECT,
			      bfd_ind_section_ptr, (bfd_vma) 0, name, false,
			      collect, (struct bfd_link_hash_entry **) &hi)))
			goto error_return;

		      /* If there is a duplicate definition somewhere,
                         then HI may not point to an indirect symbol.
                         We will have reported an error to the user in
                         that case.  */

		      if (hi->root.type == bfd_link_hash_indirect)
			{
			  /* If the symbol became indirect, then we
                             assume that we have not seen a definition
                             before.  */
			  BFD_ASSERT ((hi->elf_link_hash_flags
				       & (ELF_LINK_HASH_DEF_DYNAMIC
					  | ELF_LINK_HASH_DEF_REGULAR))
				      == 0);

			  /* Copy down any references that we may have
                             already seen to the symbol which just
                             became indirect.  */
			  h->elf_link_hash_flags |=
			    (hi->elf_link_hash_flags
			     & (ELF_LINK_HASH_REF_DYNAMIC
				| ELF_LINK_HASH_REF_REGULAR));

			  /* Copy over the global table offset entry.
                             This may have been already set up by a
                             check_relocs routine.  */
			  if (h->got_offset == (bfd_vma) -1)
			    {
			      h->got_offset = hi->got_offset;
			      hi->got_offset = (bfd_vma) -1;
			    }
			  BFD_ASSERT (hi->got_offset == (bfd_vma) -1);

			  if (h->dynindx == -1)
			    {
			      h->dynindx = hi->dynindx;
			      h->dynstr_index = hi->dynstr_index;
			      hi->dynindx = -1;
			      hi->dynstr_index = 0;
			    }
			  BFD_ASSERT (hi->dynindx == -1);

			  /* FIXME: There may be other information to
                             copy over for particular targets.  */

			  /* See if the new flags lead us to realize
                             that the symbol must be dynamic.  */
			  if (! dynsym)
			    {
			      if (! dynamic)
				{
				  if (info->shared
				      || ((hi->elf_link_hash_flags
					   & ELF_LINK_HASH_REF_DYNAMIC)
					  != 0))
				    dynsym = true;
				}
			      else
				{
				  if ((hi->elf_link_hash_flags
				       & ELF_LINK_HASH_REF_REGULAR) != 0)
				    dynsym = true;
				}
			    }
			}
		    }
		}
	    }

	  if (dynsym && h->dynindx == -1)
	    {
	      if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		goto error_return;
	      if (h->weakdef != NULL
		  && ! new_weakdef
		  && h->weakdef->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info,
							     h->weakdef))
		    goto error_return;
		}
	    }
	}
    }

  /* Now set the weakdefs field correctly for all the weak defined
     symbols we found.  The only way to do this is to search all the
     symbols.  Since we only need the information for non functions in
     dynamic objects, that's the only time we actually put anything on
     the list WEAKS.  We need this information so that if a regular
     object refers to a symbol defined weakly in a dynamic object, the
     real symbol in the dynamic object is also put in the dynamic
     symbols; we also must arrange for both symbols to point to the
     same memory location.  We could handle the general case of symbol
     aliasing, but a general symbol alias can only be generated in
     assembler code, handling it correctly would be very time
     consuming, and other ELF linkers don't handle general aliasing
     either.  */
  while (weaks != NULL)
    {
      struct elf_link_hash_entry *hlook;
      asection *slook;
      bfd_vma vlook;
      struct elf_link_hash_entry **hpp;
      struct elf_link_hash_entry **hppend;

      hlook = weaks;
      weaks = hlook->weakdef;
      hlook->weakdef = NULL;

      BFD_ASSERT (hlook->root.type == bfd_link_hash_defined
		  || hlook->root.type == bfd_link_hash_defweak
		  || hlook->root.type == bfd_link_hash_common
		  || hlook->root.type == bfd_link_hash_indirect);
      slook = hlook->root.u.def.section;
      vlook = hlook->root.u.def.value;

      hpp = elf_sym_hashes (abfd);
      hppend = hpp + extsymcount;
      for (; hpp < hppend; hpp++)
	{
	  struct elf_link_hash_entry *h;

	  h = *hpp;
	  if (h != NULL && h != hlook
	      && h->root.type == bfd_link_hash_defined
	      && h->root.u.def.section == slook
	      && h->root.u.def.value == vlook)
	    {
	      hlook->weakdef = h;

	      /* If the weak definition is in the list of dynamic
		 symbols, make sure the real definition is put there
		 as well.  */
	      if (hlook->dynindx != -1
		  && h->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		    goto error_return;
		}

	      /* If the real definition is in the list of dynamic
                 symbols, make sure the weak definition is put there
                 as well.  If we don't do this, then the dynamic
                 loader might not merge the entries for the real
                 definition and the weak definition.  */
	      if (h->dynindx != -1
		  && hlook->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info, hlook))
		    goto error_return;
		}

	      break;
	    }
	}
    }

  if (buf != NULL)
    {
      free (buf);
      buf = NULL;
    }

  if (extversym != NULL)
    {
      free (extversym);
      extversym = NULL;
    }

  /* If this object is the same format as the output object, and it is
     not a shared library, then let the backend look through the
     relocs.

     This is required to build global offset table entries and to
     arrange for dynamic relocs.  It is not required for the
     particular common case of linking non PIC code, even when linking
     against shared libraries, but unfortunately there is no way of
     knowing whether an object file has been compiled PIC or not.
     Looking through the relocs is not particularly time consuming.
     The problem is that we must either (1) keep the relocs in memory,
     which causes the linker to require additional runtime memory or
     (2) read the relocs twice from the input file, which wastes time.
     This would be a good case for using mmap.

     I have no idea how to handle linking PIC code into a file of a
     different format.  It probably can't be done.  */
  check_relocs = get_elf_backend_data (abfd)->check_relocs;
  if (! dynamic
      && abfd->xvec == info->hash->creator
      && check_relocs != NULL)
    {
      asection *o;

      for (o = abfd->sections; o != NULL; o = o->next)
	{
	  Elf_Internal_Rela *internal_relocs;
	  boolean ok;

	  if ((o->flags & SEC_RELOC) == 0
	      || o->reloc_count == 0
	      || ((info->strip == strip_all || info->strip == strip_debugger)
		  && (o->flags & SEC_DEBUGGING) != 0)
	      || bfd_is_abs_section (o->output_section))
	    continue;

	  internal_relocs = (NAME(_bfd_elf,link_read_relocs)
			     (abfd, o, (PTR) NULL,
			      (Elf_Internal_Rela *) NULL,
			      info->keep_memory));
	  if (internal_relocs == NULL)
	    goto error_return;

	  ok = (*check_relocs) (abfd, info, o, internal_relocs);

	  if (! info->keep_memory)
	    free (internal_relocs);

	  if (! ok)
	    goto error_return;
	}
    }

  /* If this is a non-traditional, non-relocateable link, try to
     optimize the handling of the .stab/.stabstr sections.  */
  if (! dynamic
      && ! info->relocateable
      && ! info->traditional_format
      && info->hash->creator->flavour == bfd_target_elf_flavour
      && (info->strip != strip_all && info->strip != strip_debugger))
    {
      asection *stab, *stabstr;

      stab = bfd_get_section_by_name (abfd, ".stab");
      if (stab != NULL)
	{
	  stabstr = bfd_get_section_by_name (abfd, ".stabstr");

	  if (stabstr != NULL)
	    {
	      struct bfd_elf_section_data *secdata;

	      secdata = elf_section_data (stab);
	      if (! _bfd_link_section_stabs (abfd,
					     &elf_hash_table (info)->stab_info,
					     stab, stabstr,
					     &secdata->stab_info))
		goto error_return;
	    }
	}
    }

  return true;

 error_return:
  if (buf != NULL)
    free (buf);
  if (dynbuf != NULL)
    free (dynbuf);
  if (dynver != NULL)
    free (dynver);
  if (extversym != NULL)
    free (extversym);
  return false;
}

/* Create some sections which will be filled in with dynamic linking
   information.  ABFD is an input file which requires dynamic sections
   to be created.  The dynamic sections take up virtual memory space
   when the final executable is run, so we need to create them before
   addresses are assigned to the output sections.  We work out the
   actual contents and size of these sections later.  */

boolean
elf_link_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_link_hash_entry *h;
  struct elf_backend_data *bed;

  if (elf_hash_table (info)->dynamic_sections_created)
    return true;

  /* Make sure that all dynamic sections use the same input BFD.  */
  if (elf_hash_table (info)->dynobj == NULL)
    elf_hash_table (info)->dynobj = abfd;
  else
    abfd = elf_hash_table (info)->dynobj;

  /* Note that we set the SEC_IN_MEMORY flag for all of these
     sections.  */
  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
	   | SEC_IN_MEMORY | SEC_LINKER_CREATED);

  /* A dynamically linked executable has a .interp section, but a
     shared library does not.  */
  if (! info->shared)
    {
      s = bfd_make_section (abfd, ".interp");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY))
	return false;
    }

  /* Create sections to hold version informations.  These are removed
     if they are not needed.  */
  s = bfd_make_section (abfd, ".gnu.version_d");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  s = bfd_make_section (abfd, ".gnu.version");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, 1))
    return false;

  s = bfd_make_section (abfd, ".gnu.version_r");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  s = bfd_make_section (abfd, ".dynsym");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  s = bfd_make_section (abfd, ".dynstr");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY))
    return false;

  /* Create a strtab to hold the dynamic symbol names.  */
  if (elf_hash_table (info)->dynstr == NULL)
    {
      elf_hash_table (info)->dynstr = elf_stringtab_init ();
      if (elf_hash_table (info)->dynstr == NULL)
	return false;
    }

  s = bfd_make_section (abfd, ".dynamic");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  /* The special symbol _DYNAMIC is always set to the start of the
     .dynamic section.  This call occurs before we have processed the
     symbols for any dynamic object, so we don't have to worry about
     overriding a dynamic definition.  We could set _DYNAMIC in a
     linker script, but we only want to define it if we are, in fact,
     creating a .dynamic section.  We don't want to define it if there
     is no .dynamic section, since on some ELF platforms the start up
     code examines it to decide how to initialize the process.  */
  h = NULL;
  if (! (_bfd_generic_link_add_one_symbol
	 (info, abfd, "_DYNAMIC", BSF_GLOBAL, s, (bfd_vma) 0,
	  (const char *) NULL, false, get_elf_backend_data (abfd)->collect,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! _bfd_elf_link_record_dynamic_symbol (info, h))
    return false;

  s = bfd_make_section (abfd, ".hash");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  /* Let the backend create the rest of the sections.  This lets the
     backend set the right flags.  The backend will normally create
     the .got and .plt sections.  */
  bed = get_elf_backend_data (abfd);
  if (! (*bed->elf_backend_create_dynamic_sections) (abfd, info))
    return false;

  elf_hash_table (info)->dynamic_sections_created = true;

  return true;
}

/* Add an entry to the .dynamic table.  */

boolean
elf_add_dynamic_entry (info, tag, val)
     struct bfd_link_info *info;
     bfd_vma tag;
     bfd_vma val;
{
  Elf_Internal_Dyn dyn;
  bfd *dynobj;
  asection *s;
  size_t newsize;
  bfd_byte *newcontents;

  dynobj = elf_hash_table (info)->dynobj;

  s = bfd_get_section_by_name (dynobj, ".dynamic");
  BFD_ASSERT (s != NULL);

  newsize = s->_raw_size + sizeof (Elf_External_Dyn);
  newcontents = (bfd_byte *) bfd_realloc (s->contents, newsize);
  if (newcontents == NULL)
    return false;

  dyn.d_tag = tag;
  dyn.d_un.d_val = val;
  elf_swap_dyn_out (dynobj, &dyn,
		    (Elf_External_Dyn *) (newcontents + s->_raw_size));

  s->_raw_size = newsize;
  s->contents = newcontents;

  return true;
}


/* Read and swap the relocs for a section.  They may have been cached.
   If the EXTERNAL_RELOCS and INTERNAL_RELOCS arguments are not NULL,
   they are used as buffers to read into.  They are known to be large
   enough.  If the INTERNAL_RELOCS relocs argument is NULL, the return
   value is allocated using either malloc or bfd_alloc, according to
   the KEEP_MEMORY argument.  */

Elf_Internal_Rela *
NAME(_bfd_elf,link_read_relocs) (abfd, o, external_relocs, internal_relocs,
				 keep_memory)
     bfd *abfd;
     asection *o;
     PTR external_relocs;
     Elf_Internal_Rela *internal_relocs;
     boolean keep_memory;
{
  Elf_Internal_Shdr *rel_hdr;
  PTR alloc1 = NULL;
  Elf_Internal_Rela *alloc2 = NULL;

  if (elf_section_data (o)->relocs != NULL)
    return elf_section_data (o)->relocs;

  if (o->reloc_count == 0)
    return NULL;

  rel_hdr = &elf_section_data (o)->rel_hdr;

  if (internal_relocs == NULL)
    {
      size_t size;

      size = o->reloc_count * sizeof (Elf_Internal_Rela);
      if (keep_memory)
	internal_relocs = (Elf_Internal_Rela *) bfd_alloc (abfd, size);
      else
	internal_relocs = alloc2 = (Elf_Internal_Rela *) bfd_malloc (size);
      if (internal_relocs == NULL)
	goto error_return;
    }

  if (external_relocs == NULL)
    {
      alloc1 = (PTR) bfd_malloc ((size_t) rel_hdr->sh_size);
      if (alloc1 == NULL)
	goto error_return;
      external_relocs = alloc1;
    }

  if ((bfd_seek (abfd, rel_hdr->sh_offset, SEEK_SET) != 0)
      || (bfd_read (external_relocs, 1, rel_hdr->sh_size, abfd)
	  != rel_hdr->sh_size))
    goto error_return;

  /* Swap in the relocs.  For convenience, we always produce an
     Elf_Internal_Rela array; if the relocs are Rel, we set the addend
     to 0.  */
  if (rel_hdr->sh_entsize == sizeof (Elf_External_Rel))
    {
      Elf_External_Rel *erel;
      Elf_External_Rel *erelend;
      Elf_Internal_Rela *irela;

      erel = (Elf_External_Rel *) external_relocs;
      erelend = erel + o->reloc_count;
      irela = internal_relocs;
      for (; erel < erelend; erel++, irela++)
	{
	  Elf_Internal_Rel irel;

	  elf_swap_reloc_in (abfd, erel, &irel);
	  irela->r_offset = irel.r_offset;
	  irela->r_info = irel.r_info;
	  irela->r_addend = 0;
	}
    }
  else
    {
      Elf_External_Rela *erela;
      Elf_External_Rela *erelaend;
      Elf_Internal_Rela *irela;

      BFD_ASSERT (rel_hdr->sh_entsize == sizeof (Elf_External_Rela));

      erela = (Elf_External_Rela *) external_relocs;
      erelaend = erela + o->reloc_count;
      irela = internal_relocs;
      for (; erela < erelaend; erela++, irela++)
	elf_swap_reloca_in (abfd, erela, irela);
    }

  /* Cache the results for next time, if we can.  */
  if (keep_memory)
    elf_section_data (o)->relocs = internal_relocs;

  if (alloc1 != NULL)
    free (alloc1);

  /* Don't free alloc2, since if it was allocated we are passing it
     back (under the name of internal_relocs).  */

  return internal_relocs;

 error_return:
  if (alloc1 != NULL)
    free (alloc1);
  if (alloc2 != NULL)
    free (alloc2);
  return NULL;
}


/* Record an assignment to a symbol made by a linker script.  We need
   this in case some dynamic object refers to this symbol.  */

/*ARGSUSED*/
boolean
NAME(bfd_elf,record_link_assignment) (output_bfd, info, name, provide)
     bfd *output_bfd;
     struct bfd_link_info *info;
     const char *name;
     boolean provide;
{
  struct elf_link_hash_entry *h;

  if (info->hash->creator->flavour != bfd_target_elf_flavour)
    return true;

  h = elf_link_hash_lookup (elf_hash_table (info), name, true, true, false);
  if (h == NULL)
    return false;

  if (h->root.type == bfd_link_hash_new)
    h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;

  /* If this symbol is being provided by the linker script, and it is
     currently defined by a dynamic object, but not by a regular
     object, then mark it as undefined so that the generic linker will
     force the correct value.  */
  if (provide
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    h->root.type = bfd_link_hash_undefined;

  /* If this symbol is not being provided by the linker script, and it is
     currently defined by a dynamic object, but not by a regular object,
     then clear out any version information because the symbol will not be
     associated with the dynamic object any more.  */
  if (!provide
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    h->verinfo.verdef = NULL;

  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (((h->elf_link_hash_flags & (ELF_LINK_HASH_DEF_DYNAMIC
				  | ELF_LINK_HASH_REF_DYNAMIC)) != 0
       || info->shared)
      && h->dynindx == -1)
    {
      if (! _bfd_elf_link_record_dynamic_symbol (info, h))
	return false;

      /* If this is a weak defined symbol, and we know a corresponding
	 real symbol from the same dynamic object, make sure the real
	 symbol is also made into a dynamic symbol.  */
      if (h->weakdef != NULL
	  && h->weakdef->dynindx == -1)
	{
	  if (! _bfd_elf_link_record_dynamic_symbol (info, h->weakdef))
	    return false;
	}
    }

  return true;
}

/* This structure is used to pass information to
   elf_link_assign_sym_version.  */

struct elf_assign_sym_version_info
{
  /* Output BFD.  */
  bfd *output_bfd;
  /* General link information.  */
  struct bfd_link_info *info;
  /* Version tree.  */
  struct bfd_elf_version_tree *verdefs;
  /* Whether we are exporting all dynamic symbols.  */
  boolean export_dynamic;
  /* Whether we removed any symbols from the dynamic symbol table.  */
  boolean removed_dynamic;
  /* Whether we had a failure.  */
  boolean failed;
};

/* This structure is used to pass information to
   elf_link_find_version_dependencies.  */

struct elf_find_verdep_info
{
  /* Output BFD.  */
  bfd *output_bfd;
  /* General link information.  */
  struct bfd_link_info *info;
  /* The number of dependencies.  */
  unsigned int vers;
  /* Whether we had a failure.  */
  boolean failed;
};

/* Array used to determine the number of hash table buckets to use
   based on the number of symbols there are.  If there are fewer than
   3 symbols we use 1 bucket, fewer than 17 symbols we use 3 buckets,
   fewer than 37 we use 17 buckets, and so forth.  We never use more
   than 32771 buckets.  */

static const size_t elf_buckets[] =
{
  1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209,
  16411, 32771, 0
};

/* Set up the sizes and contents of the ELF dynamic sections.  This is
   called by the ELF linker emulation before_allocation routine.  We
   must set the sizes of the sections before the linker sets the
   addresses of the various sections.  */

boolean
NAME(bfd_elf,size_dynamic_sections) (output_bfd, soname, rpath,
				     export_dynamic, filter_shlib,
				     auxiliary_filters, info, sinterpptr,
				     verdefs)
     bfd *output_bfd;
     const char *soname;
     const char *rpath;
     boolean export_dynamic;
     const char *filter_shlib;
     const char * const *auxiliary_filters;
     struct bfd_link_info *info;
     asection **sinterpptr;
     struct bfd_elf_version_tree *verdefs;
{
  bfd_size_type soname_indx;
  bfd *dynobj;
  struct elf_backend_data *bed;
  bfd_size_type old_dynsymcount;
  struct elf_assign_sym_version_info asvinfo;

  *sinterpptr = NULL;

  soname_indx = (bfd_size_type) -1;

  if (info->hash->creator->flavour != bfd_target_elf_flavour)
    return true;

  /* The backend may have to create some sections regardless of whether
     we're dynamic or not.  */
  bed = get_elf_backend_data (output_bfd);
  if (bed->elf_backend_always_size_sections
      && ! (*bed->elf_backend_always_size_sections) (output_bfd, info))
    return false;

  dynobj = elf_hash_table (info)->dynobj;

  /* If there were no dynamic objects in the link, there is nothing to
     do here.  */
  if (dynobj == NULL)
    return true;

  /* If we are supposed to export all symbols into the dynamic symbol
     table (this is not the normal case), then do so.  */
  if (export_dynamic)
    {
      struct elf_info_failed eif;

      eif.failed = false;
      eif.info = info;
      elf_link_hash_traverse (elf_hash_table (info), elf_export_symbol,
			      (PTR) &eif);
      if (eif.failed)
	return false;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      struct elf_info_failed eif;
      struct elf_link_hash_entry *h;
      bfd_size_type strsize;

      *sinterpptr = bfd_get_section_by_name (dynobj, ".interp");
      BFD_ASSERT (*sinterpptr != NULL || info->shared);

      if (soname != NULL)
	{
	  soname_indx = _bfd_stringtab_add (elf_hash_table (info)->dynstr,
					    soname, true, true);
	  if (soname_indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, DT_SONAME, soname_indx))
	    return false;
	}

      if (info->symbolic)
	{
	  if (! elf_add_dynamic_entry (info, DT_SYMBOLIC, 0))
	    return false;
	}

      if (rpath != NULL)
	{
	  bfd_size_type indx;

	  indx = _bfd_stringtab_add (elf_hash_table (info)->dynstr, rpath,
				     true, true);
	  if (indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, DT_RPATH, indx))
	    return false;
	}

      if (filter_shlib != NULL)
	{
	  bfd_size_type indx;

	  indx = _bfd_stringtab_add (elf_hash_table (info)->dynstr,
				     filter_shlib, true, true);
	  if (indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, DT_FILTER, indx))
	    return false;
	}

      if (auxiliary_filters != NULL)
	{
	  const char * const *p;

	  for (p = auxiliary_filters; *p != NULL; p++)
	    {
	      bfd_size_type indx;

	      indx = _bfd_stringtab_add (elf_hash_table (info)->dynstr,
					 *p, true, true);
	      if (indx == (bfd_size_type) -1
		  || ! elf_add_dynamic_entry (info, DT_AUXILIARY, indx))
		return false;
	    }
	}

      /* Attach all the symbols to their version information.  */
      asvinfo.output_bfd = output_bfd;
      asvinfo.info = info;
      asvinfo.verdefs = verdefs;
      asvinfo.export_dynamic = export_dynamic;
      asvinfo.removed_dynamic = false;
      asvinfo.failed = false;

      elf_link_hash_traverse (elf_hash_table (info),
			      elf_link_assign_sym_version,
			      (PTR) &asvinfo);
      if (asvinfo.failed)
	return false;

      /* Find all symbols which were defined in a dynamic object and make
	 the backend pick a reasonable value for them.  */
      eif.failed = false;
      eif.info = info;
      elf_link_hash_traverse (elf_hash_table (info),
			      elf_adjust_dynamic_symbol,
			      (PTR) &eif);
      if (eif.failed)
	return false;

      /* Add some entries to the .dynamic section.  We fill in some of the
	 values later, in elf_bfd_final_link, but we must add the entries
	 now so that we know the final size of the .dynamic section.  */
      h =  elf_link_hash_lookup (elf_hash_table (info), "_init", false,
				false, false);
      if (h != NULL
	  && (h->elf_link_hash_flags & (ELF_LINK_HASH_REF_REGULAR
					| ELF_LINK_HASH_DEF_REGULAR)) != 0)
	{
	  if (! elf_add_dynamic_entry (info, DT_INIT, 0))
	    return false;
	}
      h =  elf_link_hash_lookup (elf_hash_table (info), "_fini", false,
				 false, false);
      if (h != NULL
	  && (h->elf_link_hash_flags & (ELF_LINK_HASH_REF_REGULAR
					| ELF_LINK_HASH_DEF_REGULAR)) != 0)
	{
	  if (! elf_add_dynamic_entry (info, DT_FINI, 0))
	    return false;
	}
      strsize = _bfd_stringtab_size (elf_hash_table (info)->dynstr);
      if (! elf_add_dynamic_entry (info, DT_HASH, 0)
	  || ! elf_add_dynamic_entry (info, DT_STRTAB, 0)
	  || ! elf_add_dynamic_entry (info, DT_SYMTAB, 0)
	  || ! elf_add_dynamic_entry (info, DT_STRSZ, strsize)
	  || ! elf_add_dynamic_entry (info, DT_SYMENT,
				      sizeof (Elf_External_Sym)))
	return false;
    }

  /* The backend must work out the sizes of all the other dynamic
     sections.  */
  old_dynsymcount = elf_hash_table (info)->dynsymcount;
  if (! (*bed->elf_backend_size_dynamic_sections) (output_bfd, info))
    return false;

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      size_t dynsymcount;
      asection *s;
      size_t i;
      size_t bucketcount = 0;
      Elf_Internal_Sym isym;

      /* Set up the version definition section.  */
      s = bfd_get_section_by_name (dynobj, ".gnu.version_d");
      BFD_ASSERT (s != NULL);

      /* We may have created additional version definitions if we are
         just linking a regular application.  */
      verdefs = asvinfo.verdefs;

      if (verdefs == NULL)
	{
	  asection **spp;

	  /* Don't include this section in the output file.  */
	  for (spp = &output_bfd->sections;
	       *spp != s->output_section;
	       spp = &(*spp)->next)
	    ;
	  *spp = s->output_section->next;
	  --output_bfd->section_count;
	}
      else
	{
	  unsigned int cdefs;
	  bfd_size_type size;
	  struct bfd_elf_version_tree *t;
	  bfd_byte *p;
	  Elf_Internal_Verdef def;
	  Elf_Internal_Verdaux defaux;

	  if (asvinfo.removed_dynamic)
	    {
	      /* Some dynamic symbols were changed to be local
		 symbols.  In this case, we renumber all of the
		 dynamic symbols, so that we don't have a hole.  If
		 the backend changed dynsymcount, then assume that the
		 new symbols are at the start.  This is the case on
		 the MIPS.  FIXME: The names of the removed symbols
		 will still be in the dynamic string table, wasting
		 space.  */
	      elf_hash_table (info)->dynsymcount =
		1 + (elf_hash_table (info)->dynsymcount - old_dynsymcount);
	      elf_link_hash_traverse (elf_hash_table (info),
				      elf_link_renumber_dynsyms,
				      (PTR) info);
	    }

	  cdefs = 0;
	  size = 0;

	  /* Make space for the base version.  */
	  size += sizeof (Elf_External_Verdef);
	  size += sizeof (Elf_External_Verdaux);
	  ++cdefs;

	  for (t = verdefs; t != NULL; t = t->next)
	    {
	      struct bfd_elf_version_deps *n;

	      size += sizeof (Elf_External_Verdef);
	      size += sizeof (Elf_External_Verdaux);
	      ++cdefs;

	      for (n = t->deps; n != NULL; n = n->next)
		size += sizeof (Elf_External_Verdaux);
	    }

	  s->_raw_size = size;
	  s->contents = (bfd_byte *) bfd_alloc (output_bfd, s->_raw_size);
	  if (s->contents == NULL && s->_raw_size != 0)
	    return false;

	  /* Fill in the version definition section.  */

	  p = s->contents;

	  def.vd_version = VER_DEF_CURRENT;
	  def.vd_flags = VER_FLG_BASE;
	  def.vd_ndx = 1;
	  def.vd_cnt = 1;
	  def.vd_aux = sizeof (Elf_External_Verdef);
	  def.vd_next = (sizeof (Elf_External_Verdef)
			 + sizeof (Elf_External_Verdaux));

	  if (soname_indx != (bfd_size_type) -1)
	    {
	      def.vd_hash = bfd_elf_hash ((const unsigned char *) soname);
	      defaux.vda_name = soname_indx;
	    }
	  else
	    {
	      const char *name;
	      bfd_size_type indx;

	      name = output_bfd->filename;
	      def.vd_hash = bfd_elf_hash ((const unsigned char *) name);
	      indx = _bfd_stringtab_add (elf_hash_table (info)->dynstr,
					    name, true, false);
	      if (indx == (bfd_size_type) -1)
		return false;
	      defaux.vda_name = indx;
	    }
	  defaux.vda_next = 0;

	  _bfd_elf_swap_verdef_out (output_bfd, &def,
				    (Elf_External_Verdef *)p);
	  p += sizeof (Elf_External_Verdef);
	  _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
				     (Elf_External_Verdaux *) p);
	  p += sizeof (Elf_External_Verdaux);

	  for (t = verdefs; t != NULL; t = t->next)
	    {
	      unsigned int cdeps;
	      struct bfd_elf_version_deps *n;
	      struct elf_link_hash_entry *h;

	      cdeps = 0;
	      for (n = t->deps; n != NULL; n = n->next)
		++cdeps;

	      /* Add a symbol representing this version.  */
	      h = NULL;
	      if (! (_bfd_generic_link_add_one_symbol
		     (info, dynobj, t->name, BSF_GLOBAL, bfd_abs_section_ptr,
		      (bfd_vma) 0, (const char *) NULL, false,
		      get_elf_backend_data (dynobj)->collect,
		      (struct bfd_link_hash_entry **) &h)))
		return false;
	      h->elf_link_hash_flags &= ~ ELF_LINK_NON_ELF;
	      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	      h->type = STT_OBJECT;
	      h->verinfo.vertree = t;

	      if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		return false;

	      def.vd_version = VER_DEF_CURRENT;
	      def.vd_flags = 0;
	      if (t->globals == NULL && t->locals == NULL && ! t->used)
		def.vd_flags |= VER_FLG_WEAK;
	      def.vd_ndx = t->vernum + 1;
	      def.vd_cnt = cdeps + 1;
	      def.vd_hash = bfd_elf_hash ((const unsigned char *) t->name);
	      def.vd_aux = sizeof (Elf_External_Verdef);
	      if (t->next != NULL)
		def.vd_next = (sizeof (Elf_External_Verdef)
			       + (cdeps + 1) * sizeof (Elf_External_Verdaux));
	      else
		def.vd_next = 0;

	      _bfd_elf_swap_verdef_out (output_bfd, &def,
					(Elf_External_Verdef *) p);
	      p += sizeof (Elf_External_Verdef);

	      defaux.vda_name = h->dynstr_index;
	      if (t->deps == NULL)
		defaux.vda_next = 0;
	      else
		defaux.vda_next = sizeof (Elf_External_Verdaux);
	      t->name_indx = defaux.vda_name;

	      _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
					 (Elf_External_Verdaux *) p);
	      p += sizeof (Elf_External_Verdaux);

	      for (n = t->deps; n != NULL; n = n->next)
		{
		  if (n->version_needed == NULL)
		    {
		      /* This can happen if there was an error in the
			 version script.  */
		      defaux.vda_name = 0;
		    }
		  else
		    defaux.vda_name = n->version_needed->name_indx;
		  if (n->next == NULL)
		    defaux.vda_next = 0;
		  else
		    defaux.vda_next = sizeof (Elf_External_Verdaux);

		  _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
					     (Elf_External_Verdaux *) p);
		  p += sizeof (Elf_External_Verdaux);
		}
	    }

	  if (! elf_add_dynamic_entry (info, DT_VERDEF, 0)
	      || ! elf_add_dynamic_entry (info, DT_VERDEFNUM, cdefs))
	    return false;

	  elf_tdata (output_bfd)->cverdefs = cdefs;
	}

      /* Work out the size of the version reference section.  */

      s = bfd_get_section_by_name (dynobj, ".gnu.version_r");
      BFD_ASSERT (s != NULL);
      {
	struct elf_find_verdep_info sinfo;

	sinfo.output_bfd = output_bfd;
	sinfo.info = info;
	sinfo.vers = elf_tdata (output_bfd)->cverdefs;
	if (sinfo.vers == 0)
	  sinfo.vers = 1;
	sinfo.failed = false;

	elf_link_hash_traverse (elf_hash_table (info),
				elf_link_find_version_dependencies,
				(PTR) &sinfo);

	if (elf_tdata (output_bfd)->verref == NULL)
	  {
	    asection **spp;

	    /* We don't have any version definitions, so we can just
               remove the section.  */

	    for (spp = &output_bfd->sections;
		 *spp != s->output_section;
		 spp = &(*spp)->next)
	      ;
	    *spp = s->output_section->next;
	    --output_bfd->section_count;
	  }
	else
	  {
	    Elf_Internal_Verneed *t;
	    unsigned int size;
	    unsigned int crefs;
	    bfd_byte *p;

	    /* Build the version definition section.  */
	    size = 0;
	    crefs = 0;
	    for (t = elf_tdata (output_bfd)->verref;
		 t != NULL;
		 t = t->vn_nextref)
	      {
		Elf_Internal_Vernaux *a;

		size += sizeof (Elf_External_Verneed);
		++crefs;
		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  size += sizeof (Elf_External_Vernaux);
	      }

	    s->_raw_size = size;
	    s->contents = (bfd_byte *) bfd_alloc (output_bfd, size);
	    if (s->contents == NULL)
	      return false;

	    p = s->contents;
	    for (t = elf_tdata (output_bfd)->verref;
		 t != NULL;
		 t = t->vn_nextref)
	      {
		unsigned int caux;
		Elf_Internal_Vernaux *a;
		bfd_size_type indx;

		caux = 0;
		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  ++caux;

		t->vn_version = VER_NEED_CURRENT;
		t->vn_cnt = caux;
		if (elf_dt_name (t->vn_bfd) != NULL)
		  indx = _bfd_stringtab_add (elf_hash_table (info)->dynstr,
					     elf_dt_name (t->vn_bfd),
					     true, false);
		else
		  indx = _bfd_stringtab_add (elf_hash_table (info)->dynstr,
					     t->vn_bfd->filename, true, false);
		if (indx == (bfd_size_type) -1)
		  return false;
		t->vn_file = indx;
		t->vn_aux = sizeof (Elf_External_Verneed);
		if (t->vn_nextref == NULL)
		  t->vn_next = 0;
		else
		  t->vn_next = (sizeof (Elf_External_Verneed)
				+ caux * sizeof (Elf_External_Vernaux));

		_bfd_elf_swap_verneed_out (output_bfd, t,
					   (Elf_External_Verneed *) p);
		p += sizeof (Elf_External_Verneed);

		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  {
		    a->vna_hash = bfd_elf_hash ((const unsigned char *)
						a->vna_nodename);
		    indx = _bfd_stringtab_add (elf_hash_table (info)->dynstr,
					       a->vna_nodename, true, false);
		    if (indx == (bfd_size_type) -1)
		      return false;
		    a->vna_name = indx;
		    if (a->vna_nextptr == NULL)
		      a->vna_next = 0;
		    else
		      a->vna_next = sizeof (Elf_External_Vernaux);

		    _bfd_elf_swap_vernaux_out (output_bfd, a,
					       (Elf_External_Vernaux *) p);
		    p += sizeof (Elf_External_Vernaux);
		  }
	      }

	    if (! elf_add_dynamic_entry (info, DT_VERNEED, 0)
		|| ! elf_add_dynamic_entry (info, DT_VERNEEDNUM, crefs))
	      return false;

	    elf_tdata (output_bfd)->cverrefs = crefs;
	  }
      }

      dynsymcount = elf_hash_table (info)->dynsymcount;

      /* Work out the size of the symbol version section.  */
      s = bfd_get_section_by_name (dynobj, ".gnu.version");
      BFD_ASSERT (s != NULL);
      if (dynsymcount == 0
	  || (verdefs == NULL && elf_tdata (output_bfd)->verref == NULL))
	{
	  asection **spp;

	  /* We don't need any symbol versions; just discard the
             section.  */
	  for (spp = &output_bfd->sections;
	       *spp != s->output_section;
	       spp = &(*spp)->next)
	    ;
	  *spp = s->output_section->next;
	  --output_bfd->section_count;
	}
      else
	{
	  s->_raw_size = dynsymcount * sizeof (Elf_External_Versym);
	  s->contents = (bfd_byte *) bfd_zalloc (output_bfd, s->_raw_size);
	  if (s->contents == NULL)
	    return false;

	  if (! elf_add_dynamic_entry (info, DT_VERSYM, 0))
	    return false;
	}

      /* Set the size of the .dynsym and .hash sections.  We counted
	 the number of dynamic symbols in elf_link_add_object_symbols.
	 We will build the contents of .dynsym and .hash when we build
	 the final symbol table, because until then we do not know the
	 correct value to give the symbols.  We built the .dynstr
	 section as we went along in elf_link_add_object_symbols.
	 FIXME: We use bfd_zalloc() here because there may be holes
	 where sections were deleted above.  */
      s = bfd_get_section_by_name (dynobj, ".dynsym");
      BFD_ASSERT (s != NULL);
      s->_raw_size = dynsymcount * sizeof (Elf_External_Sym);
      s->contents = (bfd_byte *) bfd_zalloc (output_bfd, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return false;

      /* The first entry in .dynsym is a dummy symbol.  */
      isym.st_value = 0;
      isym.st_size = 0;
      isym.st_name = 0;
      isym.st_info = 0;
      isym.st_other = 0;
      isym.st_shndx = 0;
      elf_swap_symbol_out (output_bfd, &isym,
			   (PTR) (Elf_External_Sym *) s->contents);

      for (i = 0; elf_buckets[i] != 0; i++)
	{
	  bucketcount = elf_buckets[i];
	  if (dynsymcount < elf_buckets[i + 1])
	    break;
	}

      s = bfd_get_section_by_name (dynobj, ".hash");
      BFD_ASSERT (s != NULL);
      s->_raw_size = (2 + bucketcount + dynsymcount) * (ARCH_SIZE / 8);
      s->contents = (bfd_byte *) bfd_alloc (output_bfd, s->_raw_size);
      if (s->contents == NULL)
	return false;
      memset (s->contents, 0, (size_t) s->_raw_size);

      put_word (output_bfd, bucketcount, s->contents);
      put_word (output_bfd, dynsymcount, s->contents + (ARCH_SIZE / 8));

      elf_hash_table (info)->bucketcount = bucketcount;

      s = bfd_get_section_by_name (dynobj, ".dynstr");
      BFD_ASSERT (s != NULL);
      s->_raw_size = _bfd_stringtab_size (elf_hash_table (info)->dynstr);

      if (! elf_add_dynamic_entry (info, DT_NULL, 0))
	return false;
    }

  return true;
}

/* Fix up the flags for a symbol.  This handles various cases which
   can only be fixed after all the input files are seen.  This is
   currently called by both adjust_dynamic_symbol and
   assign_sym_version, which is unnecessary but perhaps more robust in
   the face of future changes.  */

static boolean
elf_fix_symbol_flags (h, eif)
     struct elf_link_hash_entry *h;
     struct elf_info_failed *eif;
{
  /* If this symbol was mentioned in a non-ELF file, try to set
     DEF_REGULAR and REF_REGULAR correctly.  This is the only way to
     permit a non-ELF file to correctly refer to a symbol defined in
     an ELF dynamic object.  */
  if ((h->elf_link_hash_flags & ELF_LINK_NON_ELF) != 0)
    {
      if (h->root.type != bfd_link_hash_defined
	  && h->root.type != bfd_link_hash_defweak)
	h->elf_link_hash_flags |= ELF_LINK_HASH_REF_REGULAR;
      else
	{
	  if (h->root.u.def.section->owner != NULL
	      && (bfd_get_flavour (h->root.u.def.section->owner)
		  == bfd_target_elf_flavour))
	    h->elf_link_hash_flags |= ELF_LINK_HASH_REF_REGULAR;
	  else
	    h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	}

      if (h->dynindx == -1
	  && ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	      || (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0))
	{
	  if (! _bfd_elf_link_record_dynamic_symbol (eif->info, h))
	    {
	      eif->failed = true;
	      return false;
	    }
	}
    }

  /* If this is a final link, and the symbol was defined as a common
     symbol in a regular object file, and there was no definition in
     any dynamic object, then the linker will have allocated space for
     the symbol in a common section but the ELF_LINK_HASH_DEF_REGULAR
     flag will not have been set.  */
  if (h->root.type == bfd_link_hash_defined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
      && (h->root.u.def.section->owner->flags & DYNAMIC) == 0)
    h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;

  /* If -Bsymbolic was used (which means to bind references to global
     symbols to the definition within the shared object), and this
     symbol was defined in a regular object, then it actually doesn't
     need a PLT entry.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0
      && eif->info->shared
      && eif->info->symbolic
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0)
    h->elf_link_hash_flags &=~ ELF_LINK_HASH_NEEDS_PLT;

  return true;
}

/* Make the backend pick a good value for a dynamic symbol.  This is
   called via elf_link_hash_traverse, and also calls itself
   recursively.  */

static boolean
elf_adjust_dynamic_symbol (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_info_failed *eif = (struct elf_info_failed *) data;
  bfd *dynobj;
  struct elf_backend_data *bed;

  /* Ignore indirect symbols.  These are added by the versioning code.  */
  if (h->root.type == bfd_link_hash_indirect)
    return true;

  /* Fix the symbol flags.  */
  if (! elf_fix_symbol_flags (h, eif))
    return false;

  /* If this symbol does not require a PLT entry, and it is not
     defined by a dynamic object, or is not referenced by a regular
     object, ignore it.  We do have to handle a weak defined symbol,
     even if no regular object refers to it, if we decided to add it
     to the dynamic symbol table.  FIXME: Do we normally need to worry
     about symbols which are defined by one dynamic object and
     referenced by another one?  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) == 0
      && ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
	  || (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
	  || ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0
	      && (h->weakdef == NULL || h->weakdef->dynindx == -1))))
    return true;

  /* If we've already adjusted this symbol, don't do it again.  This
     can happen via a recursive call.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DYNAMIC_ADJUSTED) != 0)
    return true;

  /* Don't look at this symbol again.  Note that we must set this
     after checking the above conditions, because we may look at a
     symbol once, decide not to do anything, and then get called
     recursively later after REF_REGULAR is set below.  */
  h->elf_link_hash_flags |= ELF_LINK_HASH_DYNAMIC_ADJUSTED;

  /* If this is a weak definition, and we know a real definition, and
     the real symbol is not itself defined by a regular object file,
     then get a good value for the real definition.  We handle the
     real symbol first, for the convenience of the backend routine.

     Note that there is a confusing case here.  If the real definition
     is defined by a regular object file, we don't get the real symbol
     from the dynamic object, but we do get the weak symbol.  If the
     processor backend uses a COPY reloc, then if some routine in the
     dynamic object changes the real symbol, we will not see that
     change in the corresponding weak symbol.  This is the way other
     ELF linkers work as well, and seems to be a result of the shared
     library model.

     I will clarify this issue.  Most SVR4 shared libraries define the
     variable _timezone and define timezone as a weak synonym.  The
     tzset call changes _timezone.  If you write
       extern int timezone;
       int _timezone = 5;
       int main () { tzset (); printf ("%d %d\n", timezone, _timezone); }
     you might expect that, since timezone is a synonym for _timezone,
     the same number will print both times.  However, if the processor
     backend uses a COPY reloc, then actually timezone will be copied
     into your process image, and, since you define _timezone
     yourself, _timezone will not.  Thus timezone and _timezone will
     wind up at different memory locations.  The tzset call will set
     _timezone, leaving timezone unchanged.  */

  if (h->weakdef != NULL)
    {
      struct elf_link_hash_entry *weakdef;

      BFD_ASSERT (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak);
      weakdef = h->weakdef;
      BFD_ASSERT (weakdef->root.type == bfd_link_hash_defined
		  || weakdef->root.type == bfd_link_hash_defweak);
      BFD_ASSERT (weakdef->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC);
      if ((weakdef->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0)
	{
	  /* This symbol is defined by a regular object file, so we
	     will not do anything special.  Clear weakdef for the
	     convenience of the processor backend.  */
	  h->weakdef = NULL;
	}
      else
	{
	  /* There is an implicit reference by a regular object file
	     via the weak symbol.  */
	  weakdef->elf_link_hash_flags |= ELF_LINK_HASH_REF_REGULAR;
	  if (! elf_adjust_dynamic_symbol (weakdef, (PTR) eif))
	    return false;
	}
    }

  dynobj = elf_hash_table (eif->info)->dynobj;
  bed = get_elf_backend_data (dynobj);
  if (! (*bed->elf_backend_adjust_dynamic_symbol) (eif->info, h))
    {
      eif->failed = true;
      return false;
    }

  return true;
}

/* This routine is used to export all defined symbols into the dynamic
   symbol table.  It is called via elf_link_hash_traverse.  */

static boolean
elf_export_symbol (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_info_failed *eif = (struct elf_info_failed *) data;

  /* Ignore indirect symbols.  These are added by the versioning code.  */
  if (h->root.type == bfd_link_hash_indirect)
    return true;

  if (h->dynindx == -1
      && (h->elf_link_hash_flags
	  & (ELF_LINK_HASH_DEF_REGULAR | ELF_LINK_HASH_REF_REGULAR)) != 0)
    {
      if (! _bfd_elf_link_record_dynamic_symbol (eif->info, h))
	{
	  eif->failed = true;
	  return false;
	}
    }

  return true;
}

/* Look through the symbols which are defined in other shared
   libraries and referenced here.  Update the list of version
   dependencies.  This will be put into the .gnu.version_r section.
   This function is called via elf_link_hash_traverse.  */

static boolean
elf_link_find_version_dependencies (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_find_verdep_info *rinfo = (struct elf_find_verdep_info *) data;
  Elf_Internal_Verneed *t;
  Elf_Internal_Vernaux *a;

  /* We only care about symbols defined in shared objects with version
     information.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
      || (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
      || h->dynindx == -1
      || h->verinfo.verdef == NULL)
    return true;

  /* See if we already know about this version.  */
  for (t = elf_tdata (rinfo->output_bfd)->verref; t != NULL; t = t->vn_nextref)
    {
      if (t->vn_bfd != h->verinfo.verdef->vd_bfd)
	continue;

      for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
	if (a->vna_nodename == h->verinfo.verdef->vd_nodename)
	  return true;

      break;
    }

  /* This is a new version.  Add it to tree we are building.  */

  if (t == NULL)
    {
      t = (Elf_Internal_Verneed *) bfd_zalloc (rinfo->output_bfd, sizeof *t);
      if (t == NULL)
	{
	  rinfo->failed = true;
	  return false;
	}

      t->vn_bfd = h->verinfo.verdef->vd_bfd;
      t->vn_nextref = elf_tdata (rinfo->output_bfd)->verref;
      elf_tdata (rinfo->output_bfd)->verref = t;
    }

  a = (Elf_Internal_Vernaux *) bfd_zalloc (rinfo->output_bfd, sizeof *a);

  /* Note that we are copying a string pointer here, and testing it
     above.  If bfd_elf_string_from_elf_section is ever changed to
     discard the string data when low in memory, this will have to be
     fixed.  */
  a->vna_nodename = h->verinfo.verdef->vd_nodename;

  a->vna_flags = h->verinfo.verdef->vd_flags;
  a->vna_nextptr = t->vn_auxptr;

  h->verinfo.verdef->vd_exp_refno = rinfo->vers;
  ++rinfo->vers;

  a->vna_other = h->verinfo.verdef->vd_exp_refno + 1;

  t->vn_auxptr = a;

  return true;
}

/* Figure out appropriate versions for all the symbols.  We may not
   have the version number script until we have read all of the input
   files, so until that point we don't know which symbols should be
   local.  This function is called via elf_link_hash_traverse.  */

static boolean
elf_link_assign_sym_version (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_assign_sym_version_info *sinfo =
    (struct elf_assign_sym_version_info *) data;
  struct bfd_link_info *info = sinfo->info;
  struct elf_info_failed eif;
  char *p;

  /* Fix the symbol flags.  */
  eif.failed = false;
  eif.info = info;
  if (! elf_fix_symbol_flags (h, &eif))
    {
      if (eif.failed)
	sinfo->failed = true;
      return false;
    }

  /* We only need version numbers for symbols defined in regular
     objects.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    return true;

  p = strchr (h->root.root.string, ELF_VER_CHR);
  if (p != NULL && h->verinfo.vertree == NULL)
    {
      struct bfd_elf_version_tree *t;
      boolean hidden;

      hidden = true;

      /* There are two consecutive ELF_VER_CHR characters if this is
         not a hidden symbol.  */
      ++p;
      if (*p == ELF_VER_CHR)
	{
	  hidden = false;
	  ++p;
	}

      /* If there is no version string, we can just return out.  */
      if (*p == '\0')
	{
	  if (hidden)
	    h->elf_link_hash_flags |= ELF_LINK_HIDDEN;
	  return true;
	}

      /* Look for the version.  If we find it, it is no longer weak.  */
      for (t = sinfo->verdefs; t != NULL; t = t->next)
	{
	  if (strcmp (t->name, p) == 0)
	    {
	      int len;
	      char *alc;
	      struct bfd_elf_version_expr *d;

	      len = p - h->root.root.string;
	      alc = bfd_alloc (sinfo->output_bfd, len);
	      if (alc == NULL)
	        return false;
	      strncpy (alc, h->root.root.string, len - 1);
	      alc[len - 1] = '\0';
	      if (alc[len - 2] == ELF_VER_CHR)
	        alc[len - 2] = '\0';

	      h->verinfo.vertree = t;
	      t->used = true;
	      d = NULL;

	      if (t->globals != NULL)
		{
		  for (d = t->globals; d != NULL; d = d->next)
		    {
		      if ((d->match[0] == '*' && d->match[1] == '\0')
			  || fnmatch (d->match, alc, 0) == 0)
			break;
		    }
		}

	      /* See if there is anything to force this symbol to
                 local scope.  */
	      if (d == NULL && t->locals != NULL)
		{
		  for (d = t->locals; d != NULL; d = d->next)
		    {
		      if ((d->match[0] == '*' && d->match[1] == '\0')
			  || fnmatch (d->match, alc, 0) == 0)
			{
			  if (h->dynindx != -1
			      && info->shared
			      && ! sinfo->export_dynamic)
			    {
			      sinfo->removed_dynamic = true;
			      h->elf_link_hash_flags |= ELF_LINK_FORCED_LOCAL;
			      h->elf_link_hash_flags &=~
				ELF_LINK_HASH_NEEDS_PLT;
			      h->dynindx = -1;
			      /* FIXME: The name of the symbol has
				 already been recorded in the dynamic
				 string table section.  */
			    }

			  break;
			}
		    }
		}

	      bfd_release (sinfo->output_bfd, alc);
	      break;
	    }
	}

      /* If we are building an application, we need to create a
         version node for this version.  */
      if (t == NULL && ! info->shared)
	{
	  struct bfd_elf_version_tree **pp;
	  int version_index;

	  /* If we aren't going to export this symbol, we don't need
             to worry about it. */
	  if (h->dynindx == -1)
	    return true;

	  t = ((struct bfd_elf_version_tree *)
	       bfd_alloc (sinfo->output_bfd, sizeof *t));
	  if (t == NULL)
	    {
	      sinfo->failed = true;
	      return false;
	    }

	  t->next = NULL;
	  t->name = p;
	  t->globals = NULL;
	  t->locals = NULL;
	  t->deps = NULL;
	  t->name_indx = (unsigned int) -1;
	  t->used = true;

	  version_index = 1;
	  for (pp = &sinfo->verdefs; *pp != NULL; pp = &(*pp)->next)
	    ++version_index;
	  t->vernum = version_index;

	  *pp = t;

	  h->verinfo.vertree = t;
	}
      else if (t == NULL)
	{
	  /* We could not find the version for a symbol when
             generating a shared archive.  Return an error.  */
	  (*_bfd_error_handler)
	    ("%s: undefined versioned symbol name %s",
	     bfd_get_filename (sinfo->output_bfd), h->root.root.string);
	  bfd_set_error (bfd_error_bad_value);
	  sinfo->failed = true;
	  return false;
	}

      if (hidden)
	h->elf_link_hash_flags |= ELF_LINK_HIDDEN;
    }

  /* If we don't have a version for this symbol, see if we can find
     something.  */
  if (h->verinfo.vertree == NULL && sinfo->verdefs != NULL)
    {
      struct bfd_elf_version_tree *t;
      struct bfd_elf_version_tree *deflt;
      struct bfd_elf_version_expr *d;

      /* See if can find what version this symbol is in.  If the
         symbol is supposed to be local, then don't actually register
         it.  */
      deflt = NULL;
      for (t = sinfo->verdefs; t != NULL; t = t->next)
	{
	  if (t->globals != NULL)
	    {
	      for (d = t->globals; d != NULL; d = d->next)
		{
		  if (fnmatch (d->match, h->root.root.string, 0) == 0)
		    {
		      h->verinfo.vertree = t;
		      break;
		    }
		}

	      if (d != NULL)
		break;
	    }

	  if (t->locals != NULL)
	    {
	      for (d = t->locals; d != NULL; d = d->next)
		{
		  if (d->match[0] == '*' && d->match[1] == '\0')
		    deflt = t;
		  else if (fnmatch (d->match, h->root.root.string, 0) == 0)
		    {
		      h->verinfo.vertree = t;
		      if (h->dynindx != -1
			  && info->shared
			  && ! sinfo->export_dynamic)
			{
			  sinfo->removed_dynamic = true;
			  h->elf_link_hash_flags |= ELF_LINK_FORCED_LOCAL;
			  h->elf_link_hash_flags &=~ ELF_LINK_HASH_NEEDS_PLT;
			  h->dynindx = -1;
			  /* FIXME: The name of the symbol has already
			     been recorded in the dynamic string table
			     section.  */
			}
		      break;
		    }
		}

	      if (d != NULL)
		break;
	    }
	}

      if (deflt != NULL && h->verinfo.vertree == NULL)
	{
	  h->verinfo.vertree = deflt;
	  if (h->dynindx != -1
	      && info->shared
	      && ! sinfo->export_dynamic)
	    {
	      sinfo->removed_dynamic = true;
	      h->elf_link_hash_flags |= ELF_LINK_FORCED_LOCAL;
	      h->elf_link_hash_flags &=~ ELF_LINK_HASH_NEEDS_PLT;
	      h->dynindx = -1;
	      /* FIXME: The name of the symbol has already been
		 recorded in the dynamic string table section.  */
	    }
	}
    }

  return true;
}

/* This function is used to renumber the dynamic symbols, if some of
   them are removed because they are marked as local.  This is called
   via elf_link_hash_traverse.  */

static boolean
elf_link_renumber_dynsyms (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *) data;

  if (h->dynindx != -1)
    {
      h->dynindx = elf_hash_table (info)->dynsymcount;
      ++elf_hash_table (info)->dynsymcount;
    }

  return true;
}

/* Final phase of ELF linker.  */

/* A structure we use to avoid passing large numbers of arguments.  */

struct elf_final_link_info
{
  /* General link information.  */
  struct bfd_link_info *info;
  /* Output BFD.  */
  bfd *output_bfd;
  /* Symbol string table.  */
  struct bfd_strtab_hash *symstrtab;
  /* .dynsym section.  */
  asection *dynsym_sec;
  /* .hash section.  */
  asection *hash_sec;
  /* symbol version section (.gnu.version).  */
  asection *symver_sec;
  /* Buffer large enough to hold contents of any section.  */
  bfd_byte *contents;
  /* Buffer large enough to hold external relocs of any section.  */
  PTR external_relocs;
  /* Buffer large enough to hold internal relocs of any section.  */
  Elf_Internal_Rela *internal_relocs;
  /* Buffer large enough to hold external local symbols of any input
     BFD.  */
  Elf_External_Sym *external_syms;
  /* Buffer large enough to hold internal local symbols of any input
     BFD.  */
  Elf_Internal_Sym *internal_syms;
  /* Array large enough to hold a symbol index for each local symbol
     of any input BFD.  */
  long *indices;
  /* Array large enough to hold a section pointer for each local
     symbol of any input BFD.  */
  asection **sections;
  /* Buffer to hold swapped out symbols.  */
  Elf_External_Sym *symbuf;
  /* Number of swapped out symbols in buffer.  */
  size_t symbuf_count;
  /* Number of symbols which fit in symbuf.  */
  size_t symbuf_size;
};

static boolean elf_link_output_sym
  PARAMS ((struct elf_final_link_info *, const char *,
	   Elf_Internal_Sym *, asection *));
static boolean elf_link_flush_output_syms
  PARAMS ((struct elf_final_link_info *));
static boolean elf_link_output_extsym
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_input_bfd
  PARAMS ((struct elf_final_link_info *, bfd *));
static boolean elf_reloc_link_order
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   struct bfd_link_order *));

/* This struct is used to pass information to elf_link_output_extsym.  */

struct elf_outext_info
{
  boolean failed;
  boolean localsyms;
  struct elf_final_link_info *finfo;
};

/* Do the final step of an ELF link.  */

boolean
elf_bfd_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  boolean dynamic;
  bfd *dynobj;
  struct elf_final_link_info finfo;
  register asection *o;
  register struct bfd_link_order *p;
  register bfd *sub;
  size_t max_contents_size;
  size_t max_external_reloc_size;
  size_t max_internal_reloc_count;
  size_t max_sym_count;
  file_ptr off;
  Elf_Internal_Sym elfsym;
  unsigned int i;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Shdr *symstrtab_hdr;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  struct elf_outext_info eoinfo;

  if (info->shared)
    abfd->flags |= DYNAMIC;

  dynamic = elf_hash_table (info)->dynamic_sections_created;
  dynobj = elf_hash_table (info)->dynobj;

  finfo.info = info;
  finfo.output_bfd = abfd;
  finfo.symstrtab = elf_stringtab_init ();
  if (finfo.symstrtab == NULL)
    return false;

  if (! dynamic)
    {
      finfo.dynsym_sec = NULL;
      finfo.hash_sec = NULL;
      finfo.symver_sec = NULL;
    }
  else
    {
      finfo.dynsym_sec = bfd_get_section_by_name (dynobj, ".dynsym");
      finfo.hash_sec = bfd_get_section_by_name (dynobj, ".hash");
      BFD_ASSERT (finfo.dynsym_sec != NULL && finfo.hash_sec != NULL);
      finfo.symver_sec = bfd_get_section_by_name (dynobj, ".gnu.version");
      /* Note that it is OK if symver_sec is NULL.  */
    }

  finfo.contents = NULL;
  finfo.external_relocs = NULL;
  finfo.internal_relocs = NULL;
  finfo.external_syms = NULL;
  finfo.internal_syms = NULL;
  finfo.indices = NULL;
  finfo.sections = NULL;
  finfo.symbuf = NULL;
  finfo.symbuf_count = 0;

  /* Count up the number of relocations we will output for each output
     section, so that we know the sizes of the reloc sections.  We
     also figure out some maximum sizes.  */
  max_contents_size = 0;
  max_external_reloc_size = 0;
  max_internal_reloc_count = 0;
  max_sym_count = 0;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      o->reloc_count = 0;

      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_section_reloc_link_order
	      || p->type == bfd_symbol_reloc_link_order)
	    ++o->reloc_count;
	  else if (p->type == bfd_indirect_link_order)
	    {
	      asection *sec;

	      sec = p->u.indirect.section;

	      /* Mark all sections which are to be included in the
		 link.  This will normally be every section.  We need
		 to do this so that we can identify any sections which
		 the linker has decided to not include.  */
	      sec->linker_mark = true;

	      if (info->relocateable)
		o->reloc_count += sec->reloc_count;

	      if (sec->_raw_size > max_contents_size)
		max_contents_size = sec->_raw_size;
	      if (sec->_cooked_size > max_contents_size)
		max_contents_size = sec->_cooked_size;

	      /* We are interested in just local symbols, not all
		 symbols.  */
	      if (bfd_get_flavour (sec->owner) == bfd_target_elf_flavour
		  && (sec->owner->flags & DYNAMIC) == 0)
		{
		  size_t sym_count;

		  if (elf_bad_symtab (sec->owner))
		    sym_count = (elf_tdata (sec->owner)->symtab_hdr.sh_size
				 / sizeof (Elf_External_Sym));
		  else
		    sym_count = elf_tdata (sec->owner)->symtab_hdr.sh_info;

		  if (sym_count > max_sym_count)
		    max_sym_count = sym_count;

		  if ((sec->flags & SEC_RELOC) != 0)
		    {
		      size_t ext_size;

		      ext_size = elf_section_data (sec)->rel_hdr.sh_size;
		      if (ext_size > max_external_reloc_size)
			max_external_reloc_size = ext_size;
		      if (sec->reloc_count > max_internal_reloc_count)
			max_internal_reloc_count = sec->reloc_count;
		    }
		}
	    }
	}

      if (o->reloc_count > 0)
	o->flags |= SEC_RELOC;
      else
	{
	  /* Explicitly clear the SEC_RELOC flag.  The linker tends to
	     set it (this is probably a bug) and if it is set
	     assign_section_numbers will create a reloc section.  */
	  o->flags &=~ SEC_RELOC;
	}

      /* If the SEC_ALLOC flag is not set, force the section VMA to
	 zero.  This is done in elf_fake_sections as well, but forcing
	 the VMA to 0 here will ensure that relocs against these
	 sections are handled correctly.  */
      if ((o->flags & SEC_ALLOC) == 0
	  && ! o->user_set_vma)
	o->vma = 0;
    }

  /* Figure out the file positions for everything but the symbol table
     and the relocs.  We set symcount to force assign_section_numbers
     to create a symbol table.  */
  abfd->symcount = info->strip == strip_all ? 0 : 1;
  BFD_ASSERT (! abfd->output_has_begun);
  if (! _bfd_elf_compute_section_file_positions (abfd, info))
    goto error_return;

  /* That created the reloc sections.  Set their sizes, and assign
     them file positions, and allocate some buffers.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0)
	{
	  Elf_Internal_Shdr *rel_hdr;
	  register struct elf_link_hash_entry **p, **pend;

	  rel_hdr = &elf_section_data (o)->rel_hdr;

	  rel_hdr->sh_size = rel_hdr->sh_entsize * o->reloc_count;

	  /* The contents field must last into write_object_contents,
	     so we allocate it with bfd_alloc rather than malloc.  */
	  rel_hdr->contents = (PTR) bfd_alloc (abfd, rel_hdr->sh_size);
	  if (rel_hdr->contents == NULL && rel_hdr->sh_size != 0)
	    goto error_return;

	  p = ((struct elf_link_hash_entry **)
	       bfd_malloc (o->reloc_count
			   * sizeof (struct elf_link_hash_entry *)));
	  if (p == NULL && o->reloc_count != 0)
	    goto error_return;
	  elf_section_data (o)->rel_hashes = p;
	  pend = p + o->reloc_count;
	  for (; p < pend; p++)
	    *p = NULL;

	  /* Use the reloc_count field as an index when outputting the
	     relocs.  */
	  o->reloc_count = 0;
	}
    }

  _bfd_elf_assign_file_positions_for_relocs (abfd);

  /* We have now assigned file positions for all the sections except
     .symtab and .strtab.  We start the .symtab section at the current
     file position, and write directly to it.  We build the .strtab
     section in memory.  */
  abfd->symcount = 0;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  /* sh_name is set in prep_headers.  */
  symtab_hdr->sh_type = SHT_SYMTAB;
  symtab_hdr->sh_flags = 0;
  symtab_hdr->sh_addr = 0;
  symtab_hdr->sh_size = 0;
  symtab_hdr->sh_entsize = sizeof (Elf_External_Sym);
  /* sh_link is set in assign_section_numbers.  */
  /* sh_info is set below.  */
  /* sh_offset is set just below.  */
  symtab_hdr->sh_addralign = 4;  /* FIXME: system dependent?  */

  off = elf_tdata (abfd)->next_file_pos;
  off = _bfd_elf_assign_file_position_for_section (symtab_hdr, off, true);

  /* Note that at this point elf_tdata (abfd)->next_file_pos is
     incorrect.  We do not yet know the size of the .symtab section.
     We correct next_file_pos below, after we do know the size.  */

  /* Allocate a buffer to hold swapped out symbols.  This is to avoid
     continuously seeking to the right position in the file.  */
  if (! info->keep_memory || max_sym_count < 20)
    finfo.symbuf_size = 20;
  else
    finfo.symbuf_size = max_sym_count;
  finfo.symbuf = ((Elf_External_Sym *)
		  bfd_malloc (finfo.symbuf_size * sizeof (Elf_External_Sym)));
  if (finfo.symbuf == NULL)
    goto error_return;

  /* Start writing out the symbol table.  The first symbol is always a
     dummy symbol.  */
  if (info->strip != strip_all || info->relocateable)
    {
      elfsym.st_value = 0;
      elfsym.st_size = 0;
      elfsym.st_info = 0;
      elfsym.st_other = 0;
      elfsym.st_shndx = SHN_UNDEF;
      if (! elf_link_output_sym (&finfo, (const char *) NULL,
				 &elfsym, bfd_und_section_ptr))
	goto error_return;
    }

#if 0
  /* Some standard ELF linkers do this, but we don't because it causes
     bootstrap comparison failures.  */
  /* Output a file symbol for the output file as the second symbol.
     We output this even if we are discarding local symbols, although
     I'm not sure if this is correct.  */
  elfsym.st_value = 0;
  elfsym.st_size = 0;
  elfsym.st_info = ELF_ST_INFO (STB_LOCAL, STT_FILE);
  elfsym.st_other = 0;
  elfsym.st_shndx = SHN_ABS;
  if (! elf_link_output_sym (&finfo, bfd_get_filename (abfd),
			     &elfsym, bfd_abs_section_ptr))
    goto error_return;
#endif

  /* Output a symbol for each section.  We output these even if we are
     discarding local symbols, since they are used for relocs.  These
     symbols have no names.  We store the index of each one in the
     index field of the section, so that we can find it again when
     outputting relocs.  */
  if (info->strip != strip_all || info->relocateable)
    {
      elfsym.st_size = 0;
      elfsym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
      elfsym.st_other = 0;
      for (i = 1; i < elf_elfheader (abfd)->e_shnum; i++)
	{
	  o = section_from_elf_index (abfd, i);
	  if (o != NULL)
	    o->target_index = abfd->symcount;
	  elfsym.st_shndx = i;
	  if (info->relocateable || o == NULL)
	    elfsym.st_value = 0;
	  else
	    elfsym.st_value = o->vma;
	  if (! elf_link_output_sym (&finfo, (const char *) NULL,
				     &elfsym, o))
	    goto error_return;
	}
    }

  /* Allocate some memory to hold information read in from the input
     files.  */
  finfo.contents = (bfd_byte *) bfd_malloc (max_contents_size);
  finfo.external_relocs = (PTR) bfd_malloc (max_external_reloc_size);
  finfo.internal_relocs = ((Elf_Internal_Rela *)
			   bfd_malloc (max_internal_reloc_count
				       * sizeof (Elf_Internal_Rela)));
  finfo.external_syms = ((Elf_External_Sym *)
			 bfd_malloc (max_sym_count
				     * sizeof (Elf_External_Sym)));
  finfo.internal_syms = ((Elf_Internal_Sym *)
			 bfd_malloc (max_sym_count
				     * sizeof (Elf_Internal_Sym)));
  finfo.indices = (long *) bfd_malloc (max_sym_count * sizeof (long));
  finfo.sections = ((asection **)
		    bfd_malloc (max_sym_count * sizeof (asection *)));
  if ((finfo.contents == NULL && max_contents_size != 0)
      || (finfo.external_relocs == NULL && max_external_reloc_size != 0)
      || (finfo.internal_relocs == NULL && max_internal_reloc_count != 0)
      || (finfo.external_syms == NULL && max_sym_count != 0)
      || (finfo.internal_syms == NULL && max_sym_count != 0)
      || (finfo.indices == NULL && max_sym_count != 0)
      || (finfo.sections == NULL && max_sym_count != 0))
    goto error_return;

  /* Since ELF permits relocations to be against local symbols, we
     must have the local symbols available when we do the relocations.
     Since we would rather only read the local symbols once, and we
     would rather not keep them in memory, we handle all the
     relocations for a single input file at the same time.

     Unfortunately, there is no way to know the total number of local
     symbols until we have seen all of them, and the local symbol
     indices precede the global symbol indices.  This means that when
     we are generating relocateable output, and we see a reloc against
     a global symbol, we can not know the symbol index until we have
     finished examining all the local symbols to see which ones we are
     going to output.  To deal with this, we keep the relocations in
     memory, and don't output them until the end of the link.  This is
     an unfortunate waste of memory, but I don't see a good way around
     it.  Fortunately, it only happens when performing a relocateable
     link, which is not the common case.  FIXME: If keep_memory is set
     we could write the relocs out and then read them again; I don't
     know how bad the memory loss will be.  */

  for (sub = info->input_bfds; sub != NULL; sub = sub->next)
    sub->output_has_begun = false;
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_indirect_link_order
	      && (bfd_get_flavour (p->u.indirect.section->owner)
		  == bfd_target_elf_flavour))
	    {
	      sub = p->u.indirect.section->owner;
	      if (! sub->output_has_begun)
		{
		  if (! elf_link_input_bfd (&finfo, sub))
		    goto error_return;
		  sub->output_has_begun = true;
		}
	    }
	  else if (p->type == bfd_section_reloc_link_order
		   || p->type == bfd_symbol_reloc_link_order)
	    {
	      if (! elf_reloc_link_order (abfd, info, o, p))
		goto error_return;
	    }
	  else
	    {
	      if (! _bfd_default_link_order (abfd, info, o, p))
		goto error_return;
	    }
	}
    }

  /* That wrote out all the local symbols.  Finish up the symbol table
     with the global symbols.  */

  if (info->strip != strip_all && info->shared)
    {
      /* Output any global symbols that got converted to local in a
         version script.  We do this in a separate step since ELF
         requires all local symbols to appear prior to any global
         symbols.  FIXME: We should only do this if some global
         symbols were, in fact, converted to become local.  FIXME:
         Will this work correctly with the Irix 5 linker?  */
      eoinfo.failed = false;
      eoinfo.finfo = &finfo;
      eoinfo.localsyms = true;
      elf_link_hash_traverse (elf_hash_table (info), elf_link_output_extsym,
			      (PTR) &eoinfo);
      if (eoinfo.failed)
	return false;
    }

  /* The sh_info field records the index of the first non local
     symbol.  */
  symtab_hdr->sh_info = abfd->symcount;
  if (dynamic)
    elf_section_data (finfo.dynsym_sec->output_section)->this_hdr.sh_info = 1;

  /* We get the global symbols from the hash table.  */
  eoinfo.failed = false;
  eoinfo.localsyms = false;
  eoinfo.finfo = &finfo;
  elf_link_hash_traverse (elf_hash_table (info), elf_link_output_extsym,
			  (PTR) &eoinfo);
  if (eoinfo.failed)
    return false;

  /* Flush all symbols to the file.  */
  if (! elf_link_flush_output_syms (&finfo))
    return false;

  /* Now we know the size of the symtab section.  */
  off += symtab_hdr->sh_size;

  /* Finish up and write out the symbol string table (.strtab)
     section.  */
  symstrtab_hdr = &elf_tdata (abfd)->strtab_hdr;
  /* sh_name was set in prep_headers.  */
  symstrtab_hdr->sh_type = SHT_STRTAB;
  symstrtab_hdr->sh_flags = 0;
  symstrtab_hdr->sh_addr = 0;
  symstrtab_hdr->sh_size = _bfd_stringtab_size (finfo.symstrtab);
  symstrtab_hdr->sh_entsize = 0;
  symstrtab_hdr->sh_link = 0;
  symstrtab_hdr->sh_info = 0;
  /* sh_offset is set just below.  */
  symstrtab_hdr->sh_addralign = 1;

  off = _bfd_elf_assign_file_position_for_section (symstrtab_hdr, off, true);
  elf_tdata (abfd)->next_file_pos = off;

  if (abfd->symcount > 0)
    {
      if (bfd_seek (abfd, symstrtab_hdr->sh_offset, SEEK_SET) != 0
	  || ! _bfd_stringtab_emit (abfd, finfo.symstrtab))
	return false;
    }

  /* Adjust the relocs to have the correct symbol indices.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      struct elf_link_hash_entry **rel_hash;
      Elf_Internal_Shdr *rel_hdr;

      if ((o->flags & SEC_RELOC) == 0)
	continue;

      rel_hash = elf_section_data (o)->rel_hashes;
      rel_hdr = &elf_section_data (o)->rel_hdr;
      for (i = 0; i < o->reloc_count; i++, rel_hash++)
	{
	  if (*rel_hash == NULL)
	    continue;

	  BFD_ASSERT ((*rel_hash)->indx >= 0);

	  if (rel_hdr->sh_entsize == sizeof (Elf_External_Rel))
	    {
	      Elf_External_Rel *erel;
	      Elf_Internal_Rel irel;

	      erel = (Elf_External_Rel *) rel_hdr->contents + i;
	      elf_swap_reloc_in (abfd, erel, &irel);
	      irel.r_info = ELF_R_INFO ((*rel_hash)->indx,
					ELF_R_TYPE (irel.r_info));
	      elf_swap_reloc_out (abfd, &irel, erel);
	    }
	  else
	    {
	      Elf_External_Rela *erela;
	      Elf_Internal_Rela irela;

	      BFD_ASSERT (rel_hdr->sh_entsize
			  == sizeof (Elf_External_Rela));

	      erela = (Elf_External_Rela *) rel_hdr->contents + i;
	      elf_swap_reloca_in (abfd, erela, &irela);
	      irela.r_info = ELF_R_INFO ((*rel_hash)->indx,
					 ELF_R_TYPE (irela.r_info));
	      elf_swap_reloca_out (abfd, &irela, erela);
	    }
	}

      /* Set the reloc_count field to 0 to prevent write_relocs from
	 trying to swap the relocs out itself.  */
      o->reloc_count = 0;
    }

  /* If we are linking against a dynamic object, or generating a
     shared library, finish up the dynamic linking information.  */
  if (dynamic)
    {
      Elf_External_Dyn *dyncon, *dynconend;

      /* Fix up .dynamic entries.  */
      o = bfd_get_section_by_name (dynobj, ".dynamic");
      BFD_ASSERT (o != NULL);

      dyncon = (Elf_External_Dyn *) o->contents;
      dynconend = (Elf_External_Dyn *) (o->contents + o->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  unsigned int type;

	  elf_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	      /* SVR4 linkers seem to set DT_INIT and DT_FINI based on
                 magic _init and _fini symbols.  This is pretty ugly,
                 but we are compatible.  */
	    case DT_INIT:
	      name = "_init";
	      goto get_sym;
	    case DT_FINI:
	      name = "_fini";
	    get_sym:
	      {
		struct elf_link_hash_entry *h;

		h = elf_link_hash_lookup (elf_hash_table (info), name,
					  false, false, true);
		if (h != NULL
		    && (h->root.type == bfd_link_hash_defined
			|| h->root.type == bfd_link_hash_defweak))
		  {
		    dyn.d_un.d_val = h->root.u.def.value;
		    o = h->root.u.def.section;
		    if (o->output_section != NULL)
		      dyn.d_un.d_val += (o->output_section->vma
					 + o->output_offset);
		    else
		      {
			/* The symbol is imported from another shared
			   library and does not apply to this one.  */
			dyn.d_un.d_val = 0;
		      }

		    elf_swap_dyn_out (dynobj, &dyn, dyncon);
		  }
	      }
	      break;

	    case DT_HASH:
	      name = ".hash";
	      goto get_vma;
	    case DT_STRTAB:
	      name = ".dynstr";
	      goto get_vma;
	    case DT_SYMTAB:
	      name = ".dynsym";
	      goto get_vma;
	    case DT_VERDEF:
	      name = ".gnu.version_d";
	      goto get_vma;
	    case DT_VERNEED:
	      name = ".gnu.version_r";
	      goto get_vma;
	    case DT_VERSYM:
	      name = ".gnu.version";
	    get_vma:
	      o = bfd_get_section_by_name (abfd, name);
	      BFD_ASSERT (o != NULL);
	      dyn.d_un.d_ptr = o->vma;
	      elf_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;

	    case DT_REL:
	    case DT_RELA:
	    case DT_RELSZ:
	    case DT_RELASZ:
	      if (dyn.d_tag == DT_REL || dyn.d_tag == DT_RELSZ)
		type = SHT_REL;
	      else
		type = SHT_RELA;
	      dyn.d_un.d_val = 0;
	      for (i = 1; i < elf_elfheader (abfd)->e_shnum; i++)
		{
		  Elf_Internal_Shdr *hdr;

		  hdr = elf_elfsections (abfd)[i];
		  if (hdr->sh_type == type
		      && (hdr->sh_flags & SHF_ALLOC) != 0)
		    {
		      if (dyn.d_tag == DT_RELSZ || dyn.d_tag == DT_RELASZ)
			dyn.d_un.d_val += hdr->sh_size;
		      else
			{
			  if (dyn.d_un.d_val == 0
			      || hdr->sh_addr < dyn.d_un.d_val)
			    dyn.d_un.d_val = hdr->sh_addr;
			}
		    }
		}
	      elf_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;
	    }
	}
    }

  /* If we have created any dynamic sections, then output them.  */
  if (dynobj != NULL)
    {
      if (! (*bed->elf_backend_finish_dynamic_sections) (abfd, info))
	goto error_return;

      for (o = dynobj->sections; o != NULL; o = o->next)
	{
	  if ((o->flags & SEC_HAS_CONTENTS) == 0
	      || o->_raw_size == 0)
	    continue;
	  if ((o->flags & SEC_LINKER_CREATED) == 0)
	    {
	      /* At this point, we are only interested in sections
                 created by elf_link_create_dynamic_sections.  */
	      continue;
	    }
	  if ((elf_section_data (o->output_section)->this_hdr.sh_type
	       != SHT_STRTAB)
	      || strcmp (bfd_get_section_name (abfd, o), ".dynstr") != 0)
	    {
	      if (! bfd_set_section_contents (abfd, o->output_section,
					      o->contents, o->output_offset,
					      o->_raw_size))
		goto error_return;
	    }
	  else
	    {
	      file_ptr off;

	      /* The contents of the .dynstr section are actually in a
                 stringtab.  */
	      off = elf_section_data (o->output_section)->this_hdr.sh_offset;
	      if (bfd_seek (abfd, off, SEEK_SET) != 0
		  || ! _bfd_stringtab_emit (abfd,
					    elf_hash_table (info)->dynstr))
		goto error_return;
	    }
	}
    }

  /* If we have optimized stabs strings, output them.  */
  if (elf_hash_table (info)->stab_info != NULL)
    {
      if (! _bfd_write_stab_strings (abfd, &elf_hash_table (info)->stab_info))
	goto error_return;
    }

  if (finfo.symstrtab != NULL)
    _bfd_stringtab_free (finfo.symstrtab);
  if (finfo.contents != NULL)
    free (finfo.contents);
  if (finfo.external_relocs != NULL)
    free (finfo.external_relocs);
  if (finfo.internal_relocs != NULL)
    free (finfo.internal_relocs);
  if (finfo.external_syms != NULL)
    free (finfo.external_syms);
  if (finfo.internal_syms != NULL)
    free (finfo.internal_syms);
  if (finfo.indices != NULL)
    free (finfo.indices);
  if (finfo.sections != NULL)
    free (finfo.sections);
  if (finfo.symbuf != NULL)
    free (finfo.symbuf);
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0
	  && elf_section_data (o)->rel_hashes != NULL)
	free (elf_section_data (o)->rel_hashes);
    }

  elf_tdata (abfd)->linker = true;

  return true;

 error_return:
  if (finfo.symstrtab != NULL)
    _bfd_stringtab_free (finfo.symstrtab);
  if (finfo.contents != NULL)
    free (finfo.contents);
  if (finfo.external_relocs != NULL)
    free (finfo.external_relocs);
  if (finfo.internal_relocs != NULL)
    free (finfo.internal_relocs);
  if (finfo.external_syms != NULL)
    free (finfo.external_syms);
  if (finfo.internal_syms != NULL)
    free (finfo.internal_syms);
  if (finfo.indices != NULL)
    free (finfo.indices);
  if (finfo.sections != NULL)
    free (finfo.sections);
  if (finfo.symbuf != NULL)
    free (finfo.symbuf);
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0
	  && elf_section_data (o)->rel_hashes != NULL)
	free (elf_section_data (o)->rel_hashes);
    }

  return false;
}

/* Add a symbol to the output symbol table.  */

static boolean
elf_link_output_sym (finfo, name, elfsym, input_sec)
     struct elf_final_link_info *finfo;
     const char *name;
     Elf_Internal_Sym *elfsym;
     asection *input_sec;
{
  boolean (*output_symbol_hook) PARAMS ((bfd *,
					 struct bfd_link_info *info,
					 const char *,
					 Elf_Internal_Sym *,
					 asection *));

  output_symbol_hook = get_elf_backend_data (finfo->output_bfd)->
    elf_backend_link_output_symbol_hook;
  if (output_symbol_hook != NULL)
    {
      if (! ((*output_symbol_hook)
	     (finfo->output_bfd, finfo->info, name, elfsym, input_sec)))
	return false;
    }

  if (name == (const char *) NULL || *name == '\0')
    elfsym->st_name = 0;
  else
    {
      elfsym->st_name = (unsigned long) _bfd_stringtab_add (finfo->symstrtab,
							    name, true,
							    false);
      if (elfsym->st_name == (unsigned long) -1)
	return false;
    }

  if (finfo->symbuf_count >= finfo->symbuf_size)
    {
      if (! elf_link_flush_output_syms (finfo))
	return false;
    }

  elf_swap_symbol_out (finfo->output_bfd, elfsym,
		       (PTR) (finfo->symbuf + finfo->symbuf_count));
  ++finfo->symbuf_count;

  ++finfo->output_bfd->symcount;

  return true;
}

/* Flush the output symbols to the file.  */

static boolean
elf_link_flush_output_syms (finfo)
     struct elf_final_link_info *finfo;
{
  if (finfo->symbuf_count > 0)
    {
      Elf_Internal_Shdr *symtab;

      symtab = &elf_tdata (finfo->output_bfd)->symtab_hdr;

      if (bfd_seek (finfo->output_bfd, symtab->sh_offset + symtab->sh_size,
		    SEEK_SET) != 0
	  || (bfd_write ((PTR) finfo->symbuf, finfo->symbuf_count,
			 sizeof (Elf_External_Sym), finfo->output_bfd)
	      != finfo->symbuf_count * sizeof (Elf_External_Sym)))
	return false;

      symtab->sh_size += finfo->symbuf_count * sizeof (Elf_External_Sym);

      finfo->symbuf_count = 0;
    }

  return true;
}

/* Add an external symbol to the symbol table.  This is called from
   the hash table traversal routine.  When generating a shared object,
   we go through the symbol table twice.  The first time we output
   anything that might have been forced to local scope in a version
   script.  The second time we output the symbols that are still
   global symbols.  */

static boolean
elf_link_output_extsym (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_outext_info *eoinfo = (struct elf_outext_info *) data;
  struct elf_final_link_info *finfo = eoinfo->finfo;
  boolean strip;
  Elf_Internal_Sym sym;
  asection *input_sec;

  /* Decide whether to output this symbol in this pass.  */
  if (eoinfo->localsyms)
    {
      if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	return true;
    }
  else
    {
      if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
	return true;
    }

  /* If we are not creating a shared library, and this symbol is
     referenced by a shared library but is not defined anywhere, then
     warn that it is undefined.  If we do not do this, the runtime
     linker will complain that the symbol is undefined when the
     program is run.  We don't have to worry about symbols that are
     referenced by regular files, because we will already have issued
     warnings for them.  */
  if (! finfo->info->relocateable
      && ! finfo->info->shared
      && h->root.type == bfd_link_hash_undefined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    {
      if (! ((*finfo->info->callbacks->undefined_symbol)
	     (finfo->info, h->root.root.string, h->root.u.undef.abfd,
	      (asection *) NULL, 0)))
	{
	  eoinfo->failed = true;
	  return false;
	}
    }

  /* We don't want to output symbols that have never been mentioned by
     a regular file, or that we have been told to strip.  However, if
     h->indx is set to -2, the symbol is used by a reloc and we must
     output it.  */
  if (h->indx == -2)
    strip = false;
  else if (((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	    || (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = true;
  else if (finfo->info->strip == strip_all
	   || (finfo->info->strip == strip_some
	       && bfd_hash_lookup (finfo->info->keep_hash,
				   h->root.root.string,
				   false, false) == NULL))
    strip = true;
  else
    strip = false;

  /* If we're stripping it, and it's not a dynamic symbol, there's
     nothing else to do.  */
  if (strip && h->dynindx == -1)
    return true;

  sym.st_value = 0;
  sym.st_size = h->size;
  sym.st_other = h->other;
  if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
    sym.st_info = ELF_ST_INFO (STB_LOCAL, h->type);
  else if (h->root.type == bfd_link_hash_undefweak
	   || h->root.type == bfd_link_hash_defweak)
    sym.st_info = ELF_ST_INFO (STB_WEAK, h->type);
  else
    sym.st_info = ELF_ST_INFO (STB_GLOBAL, h->type);

  switch (h->root.type)
    {
    default:
    case bfd_link_hash_new:
      abort ();
      return false;

    case bfd_link_hash_undefined:
      input_sec = bfd_und_section_ptr;
      sym.st_shndx = SHN_UNDEF;
      break;

    case bfd_link_hash_undefweak:
      input_sec = bfd_und_section_ptr;
      sym.st_shndx = SHN_UNDEF;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      {
	input_sec = h->root.u.def.section;
	if (input_sec->output_section != NULL)
	  {
	    sym.st_shndx =
	      _bfd_elf_section_from_bfd_section (finfo->output_bfd,
						 input_sec->output_section);
	    if (sym.st_shndx == (unsigned short) -1)
	      {
		eoinfo->failed = true;
		return false;
	      }

	    /* ELF symbols in relocateable files are section relative,
	       but in nonrelocateable files they are virtual
	       addresses.  */
	    sym.st_value = h->root.u.def.value + input_sec->output_offset;
	    if (! finfo->info->relocateable)
	      sym.st_value += input_sec->output_section->vma;
	  }
	else
	  {
	    BFD_ASSERT (input_sec->owner == NULL
			|| (input_sec->owner->flags & DYNAMIC) != 0);
	    sym.st_shndx = SHN_UNDEF;
	    input_sec = bfd_und_section_ptr;
	  }
      }
      break;

    case bfd_link_hash_common:
      input_sec = h->root.u.c.p->section;
      sym.st_shndx = SHN_COMMON;
      sym.st_value = 1 << h->root.u.c.p->alignment_power;
      break;

    case bfd_link_hash_indirect:
      /* These symbols are created by symbol versioning.  They point
         to the decorated version of the name.  For example, if the
         symbol foo@@GNU_1.2 is the default, which should be used when
         foo is used with no version, then we add an indirect symbol
         foo which points to foo@@GNU_1.2.  We ignore these symbols,
         since the indirected symbol is already in the hash table.  If
         the indirect symbol is non-ELF, fall through and output it.  */
      if ((h->elf_link_hash_flags & ELF_LINK_NON_ELF) == 0)
	return true;

      /* Fall through.  */
    case bfd_link_hash_warning:
      /* We can't represent these symbols in ELF, although a warning
         symbol may have come from a .gnu.warning.SYMBOL section.  We
         just put the target symbol in the hash table.  If the target
         symbol does not really exist, don't do anything.  */
      if (h->root.u.i.link->type == bfd_link_hash_new)
	return true;
      return (elf_link_output_extsym
	      ((struct elf_link_hash_entry *) h->root.u.i.link, data));
    }

  /* Give the processor backend a chance to tweak the symbol value,
     and also to finish up anything that needs to be done for this
     symbol.  */
  if ((h->dynindx != -1
       || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
      && elf_hash_table (finfo->info)->dynamic_sections_created)
    {
      struct elf_backend_data *bed;

      bed = get_elf_backend_data (finfo->output_bfd);
      if (! ((*bed->elf_backend_finish_dynamic_symbol)
	     (finfo->output_bfd, finfo->info, h, &sym)))
	{
	  eoinfo->failed = true;
	  return false;
	}
    }

  /* If this symbol should be put in the .dynsym section, then put it
     there now.  We have already know the symbol index.  We also fill
     in the entry in the .hash section.  */
  if (h->dynindx != -1
      && elf_hash_table (finfo->info)->dynamic_sections_created)
    {
      char *p, *copy;
      const char *name;
      size_t bucketcount;
      size_t bucket;
      bfd_byte *bucketpos;
      bfd_vma chain;

      sym.st_name = h->dynstr_index;

      elf_swap_symbol_out (finfo->output_bfd, &sym,
			   (PTR) (((Elf_External_Sym *)
				   finfo->dynsym_sec->contents)
				  + h->dynindx));

      /* We didn't include the version string in the dynamic string
         table, so we must not consider it in the hash table.  */
      name = h->root.root.string;
      p = strchr (name, ELF_VER_CHR);
      if (p == NULL)
	copy = NULL;
      else
	{
	  copy = bfd_alloc (finfo->output_bfd, p - name + 1);
	  strncpy (copy, name, p - name);
	  copy[p - name] = '\0';
	  name = copy;
	}

      bucketcount = elf_hash_table (finfo->info)->bucketcount;
      bucket = bfd_elf_hash ((const unsigned char *) name) % bucketcount;
      bucketpos = ((bfd_byte *) finfo->hash_sec->contents
		   + (bucket + 2) * (ARCH_SIZE / 8));
      chain = get_word (finfo->output_bfd, bucketpos);
      put_word (finfo->output_bfd, h->dynindx, bucketpos);
      put_word (finfo->output_bfd, chain,
		((bfd_byte *) finfo->hash_sec->contents
		 + (bucketcount + 2 + h->dynindx) * (ARCH_SIZE / 8)));

      if (copy != NULL)
	bfd_release (finfo->output_bfd, copy);

      if (finfo->symver_sec != NULL && finfo->symver_sec->contents != NULL)
	{
	  Elf_Internal_Versym iversym;

	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	    {
	      if (h->verinfo.verdef == NULL)
		iversym.vs_vers = 0;
	      else
		iversym.vs_vers = h->verinfo.verdef->vd_exp_refno + 1;
	    }
	  else
	    {
	      if (h->verinfo.vertree == NULL)
		iversym.vs_vers = 1;
	      else
		iversym.vs_vers = h->verinfo.vertree->vernum + 1;
	    }

	  if ((h->elf_link_hash_flags & ELF_LINK_HIDDEN) != 0)
	    iversym.vs_vers |= VERSYM_HIDDEN;

	  _bfd_elf_swap_versym_out (finfo->output_bfd, &iversym,
				    (((Elf_External_Versym *)
				      finfo->symver_sec->contents)
				     + h->dynindx));
	}
    }

  /* If we're stripping it, then it was just a dynamic symbol, and
     there's nothing else to do.  */
  if (strip)
    return true;

  h->indx = finfo->output_bfd->symcount;

  if (! elf_link_output_sym (finfo, h->root.root.string, &sym, input_sec))
    {
      eoinfo->failed = true;
      return false;
    }

  return true;
}

/* Link an input file into the linker output file.  This function
   handles all the sections and relocations of the input file at once.
   This is so that we only have to read the local symbols once, and
   don't have to keep them in memory.  */

static boolean
elf_link_input_bfd (finfo, input_bfd)
     struct elf_final_link_info *finfo;
     bfd *input_bfd;
{
  boolean (*relocate_section) PARAMS ((bfd *, struct bfd_link_info *,
				       bfd *, asection *, bfd_byte *,
				       Elf_Internal_Rela *,
				       Elf_Internal_Sym *, asection **));
  bfd *output_bfd;
  Elf_Internal_Shdr *symtab_hdr;
  size_t locsymcount;
  size_t extsymoff;
  Elf_External_Sym *external_syms;
  Elf_External_Sym *esym;
  Elf_External_Sym *esymend;
  Elf_Internal_Sym *isym;
  long *pindex;
  asection **ppsection;
  asection *o;

  output_bfd = finfo->output_bfd;
  relocate_section =
    get_elf_backend_data (output_bfd)->elf_backend_relocate_section;

  /* If this is a dynamic object, we don't want to do anything here:
     we don't want the local symbols, and we don't want the section
     contents.  */
  if ((input_bfd->flags & DYNAMIC) != 0)
    return true;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  if (elf_bad_symtab (input_bfd))
    {
      locsymcount = symtab_hdr->sh_size / sizeof (Elf_External_Sym);
      extsymoff = 0;
    }
  else
    {
      locsymcount = symtab_hdr->sh_info;
      extsymoff = symtab_hdr->sh_info;
    }

  /* Read the local symbols.  */
  if (symtab_hdr->contents != NULL)
    external_syms = (Elf_External_Sym *) symtab_hdr->contents;
  else if (locsymcount == 0)
    external_syms = NULL;
  else
    {
      external_syms = finfo->external_syms;
      if (bfd_seek (input_bfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	  || (bfd_read (external_syms, sizeof (Elf_External_Sym),
			locsymcount, input_bfd)
	      != locsymcount * sizeof (Elf_External_Sym)))
	return false;
    }

  /* Swap in the local symbols and write out the ones which we know
     are going into the output file.  */
  esym = external_syms;
  esymend = esym + locsymcount;
  isym = finfo->internal_syms;
  pindex = finfo->indices;
  ppsection = finfo->sections;
  for (; esym < esymend; esym++, isym++, pindex++, ppsection++)
    {
      asection *isec;
      const char *name;
      Elf_Internal_Sym osym;

      elf_swap_symbol_in (input_bfd, esym, isym);
      *pindex = -1;

      if (elf_bad_symtab (input_bfd))
	{
	  if (ELF_ST_BIND (isym->st_info) != STB_LOCAL)
	    {
	      *ppsection = NULL;
	      continue;
	    }
	}

      if (isym->st_shndx == SHN_UNDEF)
	isec = bfd_und_section_ptr;
      else if (isym->st_shndx > 0 && isym->st_shndx < SHN_LORESERVE)
	isec = section_from_elf_index (input_bfd, isym->st_shndx);
      else if (isym->st_shndx == SHN_ABS)
	isec = bfd_abs_section_ptr;
      else if (isym->st_shndx == SHN_COMMON)
	isec = bfd_com_section_ptr;
      else
	{
	  /* Who knows?  */
	  isec = NULL;
	}

      *ppsection = isec;

      /* Don't output the first, undefined, symbol.  */
      if (esym == external_syms)
	continue;

      /* If we are stripping all symbols, we don't want to output this
	 one.  */
      if (finfo->info->strip == strip_all)
	continue;

      /* We never output section symbols.  Instead, we use the section
	 symbol of the corresponding section in the output file.  */
      if (ELF_ST_TYPE (isym->st_info) == STT_SECTION)
	continue;

      /* If we are discarding all local symbols, we don't want to
	 output this one.  If we are generating a relocateable output
	 file, then some of the local symbols may be required by
	 relocs; we output them below as we discover that they are
	 needed.  */
      if (finfo->info->discard == discard_all)
	continue;

      /* If this symbol is defined in a section which we are
         discarding, we don't need to keep it, but note that
         linker_mark is only reliable for sections that have contents.
         For the benefit of the MIPS ELF linker, we check SEC_EXCLUDE
         as well as linker_mark.  */
      if (isym->st_shndx > 0
	  && isym->st_shndx < SHN_LORESERVE
	  && isec != NULL
	  && ((! isec->linker_mark && (isec->flags & SEC_HAS_CONTENTS) != 0)
	      || (! finfo->info->relocateable
		  && (isec->flags & SEC_EXCLUDE) != 0)))
	continue;

      /* Get the name of the symbol.  */
      name = bfd_elf_string_from_elf_section (input_bfd, symtab_hdr->sh_link,
					      isym->st_name);
      if (name == NULL)
	return false;

      /* See if we are discarding symbols with this name.  */
      if ((finfo->info->strip == strip_some
	   && (bfd_hash_lookup (finfo->info->keep_hash, name, false, false)
	       == NULL))
	  || (finfo->info->discard == discard_l
	      && bfd_is_local_label_name (input_bfd, name)))
	continue;

      /* If we get here, we are going to output this symbol.  */

      osym = *isym;

      /* Adjust the section index for the output file.  */
      osym.st_shndx = _bfd_elf_section_from_bfd_section (output_bfd,
							 isec->output_section);
      if (osym.st_shndx == (unsigned short) -1)
	return false;

      *pindex = output_bfd->symcount;

      /* ELF symbols in relocateable files are section relative, but
	 in executable files they are virtual addresses.  Note that
	 this code assumes that all ELF sections have an associated
	 BFD section with a reasonable value for output_offset; below
	 we assume that they also have a reasonable value for
	 output_section.  Any special sections must be set up to meet
	 these requirements.  */
      osym.st_value += isec->output_offset;
      if (! finfo->info->relocateable)
	osym.st_value += isec->output_section->vma;

      if (! elf_link_output_sym (finfo, name, &osym, isec))
	return false;
    }

  /* Relocate the contents of each section.  */
  for (o = input_bfd->sections; o != NULL; o = o->next)
    {
      bfd_byte *contents;

      if (! o->linker_mark)
	{
	  /* This section was omitted from the link.  */
	  continue;
	}

      if ((o->flags & SEC_HAS_CONTENTS) == 0
	  || (o->_raw_size == 0 && (o->flags & SEC_RELOC) == 0))
	continue;

      if ((o->flags & SEC_LINKER_CREATED) != 0)
	{
	  /* Section was created by elf_link_create_dynamic_sections
	     or somesuch.  */
	  continue;
	}

      /* Get the contents of the section.  They have been cached by a
         relaxation routine.  Note that o is a section in an input
         file, so the contents field will not have been set by any of
         the routines which work on output files.  */
      if (elf_section_data (o)->this_hdr.contents != NULL)
	contents = elf_section_data (o)->this_hdr.contents;
      else
	{
	  contents = finfo->contents;
	  if (! bfd_get_section_contents (input_bfd, o, contents,
					  (file_ptr) 0, o->_raw_size))
	    return false;
	}

      if ((o->flags & SEC_RELOC) != 0)
	{
	  Elf_Internal_Rela *internal_relocs;

	  /* Get the swapped relocs.  */
	  internal_relocs = (NAME(_bfd_elf,link_read_relocs)
			     (input_bfd, o, finfo->external_relocs,
			      finfo->internal_relocs, false));
	  if (internal_relocs == NULL
	      && o->reloc_count > 0)
	    return false;

	  /* Relocate the section by invoking a back end routine.

	     The back end routine is responsible for adjusting the
	     section contents as necessary, and (if using Rela relocs
	     and generating a relocateable output file) adjusting the
	     reloc addend as necessary.

	     The back end routine does not have to worry about setting
	     the reloc address or the reloc symbol index.

	     The back end routine is given a pointer to the swapped in
	     internal symbols, and can access the hash table entries
	     for the external symbols via elf_sym_hashes (input_bfd).

	     When generating relocateable output, the back end routine
	     must handle STB_LOCAL/STT_SECTION symbols specially.  The
	     output symbol is going to be a section symbol
	     corresponding to the output section, which will require
	     the addend to be adjusted.  */

	  if (! (*relocate_section) (output_bfd, finfo->info,
				     input_bfd, o, contents,
				     internal_relocs,
				     finfo->internal_syms,
				     finfo->sections))
	    return false;

	  if (finfo->info->relocateable)
	    {
	      Elf_Internal_Rela *irela;
	      Elf_Internal_Rela *irelaend;
	      struct elf_link_hash_entry **rel_hash;
	      Elf_Internal_Shdr *input_rel_hdr;
	      Elf_Internal_Shdr *output_rel_hdr;

	      /* Adjust the reloc addresses and symbol indices.  */

	      irela = internal_relocs;
	      irelaend = irela + o->reloc_count;
	      rel_hash = (elf_section_data (o->output_section)->rel_hashes
			  + o->output_section->reloc_count);
	      for (; irela < irelaend; irela++, rel_hash++)
		{
		  unsigned long r_symndx;
		  Elf_Internal_Sym *isym;
		  asection *sec;

		  irela->r_offset += o->output_offset;

		  r_symndx = ELF_R_SYM (irela->r_info);

		  if (r_symndx == 0)
		    continue;

		  if (r_symndx >= locsymcount
		      || (elf_bad_symtab (input_bfd)
			  && finfo->sections[r_symndx] == NULL))
		    {
		      struct elf_link_hash_entry *rh;
		      long indx;

		      /* This is a reloc against a global symbol.  We
			 have not yet output all the local symbols, so
			 we do not know the symbol index of any global
			 symbol.  We set the rel_hash entry for this
			 reloc to point to the global hash table entry
			 for this symbol.  The symbol index is then
			 set at the end of elf_bfd_final_link.  */
		      indx = r_symndx - extsymoff;
		      rh = elf_sym_hashes (input_bfd)[indx];
		      while (rh->root.type == bfd_link_hash_indirect
			     || rh->root.type == bfd_link_hash_warning)
			rh = (struct elf_link_hash_entry *) rh->root.u.i.link;

		      /* Setting the index to -2 tells
			 elf_link_output_extsym that this symbol is
			 used by a reloc.  */
		      BFD_ASSERT (rh->indx < 0);
		      rh->indx = -2;

		      *rel_hash = rh;

		      continue;
		    }

		  /* This is a reloc against a local symbol. */

		  *rel_hash = NULL;
		  isym = finfo->internal_syms + r_symndx;
		  sec = finfo->sections[r_symndx];
		  if (ELF_ST_TYPE (isym->st_info) == STT_SECTION)
		    {
		      /* I suppose the backend ought to fill in the
			 section of any STT_SECTION symbol against a
			 processor specific section.  If we have
			 discarded a section, the output_section will
			 be the absolute section.  */
		      if (sec != NULL
			  && (bfd_is_abs_section (sec)
			      || (sec->output_section != NULL
				  && bfd_is_abs_section (sec->output_section))))
			r_symndx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return false;
			}
		      else
			{
			  r_symndx = sec->output_section->target_index;
			  BFD_ASSERT (r_symndx != 0);
			}
		    }
		  else
		    {
		      if (finfo->indices[r_symndx] == -1)
			{
			  unsigned long link;
			  const char *name;
			  asection *osec;

			  if (finfo->info->strip == strip_all)
			    {
			      /* You can't do ld -r -s.  */
			      bfd_set_error (bfd_error_invalid_operation);
			      return false;
			    }

			  /* This symbol was skipped earlier, but
			     since it is needed by a reloc, we
			     must output it now.  */
			  link = symtab_hdr->sh_link;
			  name = bfd_elf_string_from_elf_section (input_bfd,
								  link,
								  isym->st_name);
			  if (name == NULL)
			    return false;

			  osec = sec->output_section;
			  isym->st_shndx =
			    _bfd_elf_section_from_bfd_section (output_bfd,
							       osec);
			  if (isym->st_shndx == (unsigned short) -1)
			    return false;

			  isym->st_value += sec->output_offset;
			  if (! finfo->info->relocateable)
			    isym->st_value += osec->vma;

			  finfo->indices[r_symndx] = output_bfd->symcount;

			  if (! elf_link_output_sym (finfo, name, isym, sec))
			    return false;
			}

		      r_symndx = finfo->indices[r_symndx];
		    }

		  irela->r_info = ELF_R_INFO (r_symndx,
					      ELF_R_TYPE (irela->r_info));
		}

	      /* Swap out the relocs.  */
	      input_rel_hdr = &elf_section_data (o)->rel_hdr;
	      output_rel_hdr = &elf_section_data (o->output_section)->rel_hdr;
	      BFD_ASSERT (output_rel_hdr->sh_entsize
			  == input_rel_hdr->sh_entsize);
	      irela = internal_relocs;
	      irelaend = irela + o->reloc_count;
	      if (input_rel_hdr->sh_entsize == sizeof (Elf_External_Rel))
		{
		  Elf_External_Rel *erel;

		  erel = ((Elf_External_Rel *) output_rel_hdr->contents
			  + o->output_section->reloc_count);
		  for (; irela < irelaend; irela++, erel++)
		    {
		      Elf_Internal_Rel irel;

		      irel.r_offset = irela->r_offset;
		      irel.r_info = irela->r_info;
		      BFD_ASSERT (irela->r_addend == 0);
		      elf_swap_reloc_out (output_bfd, &irel, erel);
		    }
		}
	      else
		{
		  Elf_External_Rela *erela;

		  BFD_ASSERT (input_rel_hdr->sh_entsize
			      == sizeof (Elf_External_Rela));
		  erela = ((Elf_External_Rela *) output_rel_hdr->contents
			   + o->output_section->reloc_count);
		  for (; irela < irelaend; irela++, erela++)
		    elf_swap_reloca_out (output_bfd, irela, erela);
		}

	      o->output_section->reloc_count += o->reloc_count;
	    }
	}

      /* Write out the modified section contents.  */
      if (elf_section_data (o)->stab_info == NULL)
	{
	  if (! bfd_set_section_contents (output_bfd, o->output_section,
					  contents, o->output_offset,
					  (o->_cooked_size != 0
					   ? o->_cooked_size
					   : o->_raw_size)))
	    return false;
	}
      else
	{
	  if (! (_bfd_write_section_stabs
		 (output_bfd, &elf_hash_table (finfo->info)->stab_info,
		  o, &elf_section_data (o)->stab_info, contents)))
	    return false;
	}
    }

  return true;
}

/* Generate a reloc when linking an ELF file.  This is a reloc
   requested by the linker, and does come from any input file.  This
   is used to build constructor and destructor tables when linking
   with -Ur.  */

static boolean
elf_reloc_link_order (output_bfd, info, output_section, link_order)
     bfd *output_bfd;
     struct bfd_link_info *info;
     asection *output_section;
     struct bfd_link_order *link_order;
{
  reloc_howto_type *howto;
  long indx;
  bfd_vma offset;
  bfd_vma addend;
  struct elf_link_hash_entry **rel_hash_ptr;
  Elf_Internal_Shdr *rel_hdr;

  howto = bfd_reloc_type_lookup (output_bfd, link_order->u.reloc.p->reloc);
  if (howto == NULL)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  addend = link_order->u.reloc.p->addend;

  /* Figure out the symbol index.  */
  rel_hash_ptr = (elf_section_data (output_section)->rel_hashes
		  + output_section->reloc_count);
  if (link_order->type == bfd_section_reloc_link_order)
    {
      indx = link_order->u.reloc.p->u.section->target_index;
      BFD_ASSERT (indx != 0);
      *rel_hash_ptr = NULL;
    }
  else
    {
      struct elf_link_hash_entry *h;

      /* Treat a reloc against a defined symbol as though it were
         actually against the section.  */
      h = ((struct elf_link_hash_entry *)
	   bfd_wrapped_link_hash_lookup (output_bfd, info,
					 link_order->u.reloc.p->u.name,
					 false, false, true));
      if (h != NULL
	  && (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak))
	{
	  asection *section;

	  section = h->root.u.def.section;
	  indx = section->output_section->target_index;
	  *rel_hash_ptr = NULL;
	  /* It seems that we ought to add the symbol value to the
             addend here, but in practice it has already been added
             because it was passed to constructor_callback.  */
	  addend += section->output_section->vma + section->output_offset;
	}
      else if (h != NULL)
	{
	  /* Setting the index to -2 tells elf_link_output_extsym that
	     this symbol is used by a reloc.  */
	  h->indx = -2;
	  *rel_hash_ptr = h;
	  indx = 0;
	}
      else
	{
	  if (! ((*info->callbacks->unattached_reloc)
		 (info, link_order->u.reloc.p->u.name, (bfd *) NULL,
		  (asection *) NULL, (bfd_vma) 0)))
	    return false;
	  indx = 0;
	}
    }

  /* If this is an inplace reloc, we must write the addend into the
     object file.  */
  if (howto->partial_inplace && addend != 0)
    {
      bfd_size_type size;
      bfd_reloc_status_type rstat;
      bfd_byte *buf;
      boolean ok;

      size = bfd_get_reloc_size (howto);
      buf = (bfd_byte *) bfd_zmalloc (size);
      if (buf == (bfd_byte *) NULL)
	return false;
      rstat = _bfd_relocate_contents (howto, output_bfd, addend, buf);
      switch (rstat)
	{
	case bfd_reloc_ok:
	  break;
	default:
	case bfd_reloc_outofrange:
	  abort ();
	case bfd_reloc_overflow:
	  if (! ((*info->callbacks->reloc_overflow)
		 (info,
		  (link_order->type == bfd_section_reloc_link_order
		   ? bfd_section_name (output_bfd,
				       link_order->u.reloc.p->u.section)
		   : link_order->u.reloc.p->u.name),
		  howto->name, addend, (bfd *) NULL, (asection *) NULL,
		  (bfd_vma) 0)))
	    {
	      free (buf);
	      return false;
	    }
	  break;
	}
      ok = bfd_set_section_contents (output_bfd, output_section, (PTR) buf,
				     (file_ptr) link_order->offset, size);
      free (buf);
      if (! ok)
	return false;
    }

  /* The address of a reloc is relative to the section in a
     relocateable file, and is a virtual address in an executable
     file.  */
  offset = link_order->offset;
  if (! info->relocateable)
    offset += output_section->vma;

  rel_hdr = &elf_section_data (output_section)->rel_hdr;

  if (rel_hdr->sh_type == SHT_REL)
    {
      Elf_Internal_Rel irel;
      Elf_External_Rel *erel;

      irel.r_offset = offset;
      irel.r_info = ELF_R_INFO (indx, howto->type);
      erel = ((Elf_External_Rel *) rel_hdr->contents
	      + output_section->reloc_count);
      elf_swap_reloc_out (output_bfd, &irel, erel);
    }
  else
    {
      Elf_Internal_Rela irela;
      Elf_External_Rela *erela;

      irela.r_offset = offset;
      irela.r_info = ELF_R_INFO (indx, howto->type);
      irela.r_addend = addend;
      erela = ((Elf_External_Rela *) rel_hdr->contents
	       + output_section->reloc_count);
      elf_swap_reloca_out (output_bfd, &irela, erela);
    }

  ++output_section->reloc_count;

  return true;
}


/* Allocate a pointer to live in a linker created section.  */

boolean
elf_create_pointer_linker_section (abfd, info, lsect, h, rel)
     bfd *abfd;
     struct bfd_link_info *info;
     elf_linker_section_t *lsect;
     struct elf_link_hash_entry *h;
     const Elf_Internal_Rela *rel;
{
  elf_linker_section_pointers_t **ptr_linker_section_ptr = NULL;
  elf_linker_section_pointers_t *linker_section_ptr;
  unsigned long r_symndx = ELF_R_SYM (rel->r_info);;

  BFD_ASSERT (lsect != NULL);

  /* Is this a global symbol? */
  if (h != NULL)
    {
      /* Has this symbol already been allocated, if so, our work is done */
      if (_bfd_elf_find_pointer_linker_section (h->linker_section_pointer,
						rel->r_addend,
						lsect->which))
	return true;

      ptr_linker_section_ptr = &h->linker_section_pointer;
      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1)
	{
	  if (! elf_link_record_dynamic_symbol (info, h))
	    return false;
	}

      if (lsect->rel_section)
	lsect->rel_section->_raw_size += sizeof (Elf_External_Rela);
    }

  else  /* Allocation of a pointer to a local symbol */
    {
      elf_linker_section_pointers_t **ptr = elf_local_ptr_offsets (abfd);

      /* Allocate a table to hold the local symbols if first time */
      if (!ptr)
	{
	  unsigned int num_symbols = elf_tdata (abfd)->symtab_hdr.sh_info;
	  register unsigned int i;

	  ptr = (elf_linker_section_pointers_t **)
	    bfd_alloc (abfd, num_symbols * sizeof (elf_linker_section_pointers_t *));

	  if (!ptr)
	    return false;

	  elf_local_ptr_offsets (abfd) = ptr;
	  for (i = 0; i < num_symbols; i++)
	    ptr[i] = (elf_linker_section_pointers_t *)0;
	}

      /* Has this symbol already been allocated, if so, our work is done */
      if (_bfd_elf_find_pointer_linker_section (ptr[r_symndx],
						rel->r_addend,
						lsect->which))
	return true;

      ptr_linker_section_ptr = &ptr[r_symndx];

      if (info->shared)
	{
	  /* If we are generating a shared object, we need to
	     output a R_<xxx>_RELATIVE reloc so that the
	     dynamic linker can adjust this GOT entry.  */
	  BFD_ASSERT (lsect->rel_section != NULL);
	  lsect->rel_section->_raw_size += sizeof (Elf_External_Rela);
	}
    }

  /* Allocate space for a pointer in the linker section, and allocate a new pointer record
     from internal memory.  */
  BFD_ASSERT (ptr_linker_section_ptr != NULL);
  linker_section_ptr = (elf_linker_section_pointers_t *)
    bfd_alloc (abfd, sizeof (elf_linker_section_pointers_t));

  if (!linker_section_ptr)
    return false;

  linker_section_ptr->next = *ptr_linker_section_ptr;
  linker_section_ptr->addend = rel->r_addend;
  linker_section_ptr->which = lsect->which;
  linker_section_ptr->written_address_p = false;
  *ptr_linker_section_ptr = linker_section_ptr;

#if 0
  if (lsect->hole_size && lsect->hole_offset < lsect->max_hole_offset)
    {
      linker_section_ptr->offset = lsect->section->_raw_size - lsect->hole_size + (ARCH_SIZE / 8);
      lsect->hole_offset += ARCH_SIZE / 8;
      lsect->sym_offset  += ARCH_SIZE / 8;
      if (lsect->sym_hash)	/* Bump up symbol value if needed */
	{
	  lsect->sym_hash->root.u.def.value += ARCH_SIZE / 8;
#ifdef DEBUG
	  fprintf (stderr, "Bump up %s by %ld, current value = %ld\n",
		   lsect->sym_hash->root.root.string,
		   (long)ARCH_SIZE / 8,
		   (long)lsect->sym_hash->root.u.def.value);
#endif
	}
    }
  else
#endif
    linker_section_ptr->offset = lsect->section->_raw_size;

  lsect->section->_raw_size += ARCH_SIZE / 8;

#ifdef DEBUG
  fprintf (stderr, "Create pointer in linker section %s, offset = %ld, section size = %ld\n",
	   lsect->name, (long)linker_section_ptr->offset, (long)lsect->section->_raw_size);
#endif

  return true;
}


#if ARCH_SIZE==64
#define bfd_put_ptr(BFD,VAL,ADDR) bfd_put_64 (BFD, VAL, ADDR)
#endif
#if ARCH_SIZE==32
#define bfd_put_ptr(BFD,VAL,ADDR) bfd_put_32 (BFD, VAL, ADDR)
#endif

/* Fill in the address for a pointer generated in alinker section.  */

bfd_vma
elf_finish_pointer_linker_section (output_bfd, input_bfd, info, lsect, h, relocation, rel, relative_reloc)
     bfd *output_bfd;
     bfd *input_bfd;
     struct bfd_link_info *info;
     elf_linker_section_t *lsect;
     struct elf_link_hash_entry *h;
     bfd_vma relocation;
     const Elf_Internal_Rela *rel;
     int relative_reloc;
{
  elf_linker_section_pointers_t *linker_section_ptr;

  BFD_ASSERT (lsect != NULL);

  if (h != NULL)		/* global symbol */
    {
      linker_section_ptr = _bfd_elf_find_pointer_linker_section (h->linker_section_pointer,
								 rel->r_addend,
								 lsect->which);

      BFD_ASSERT (linker_section_ptr != NULL);

      if (! elf_hash_table (info)->dynamic_sections_created
	  || (info->shared
	      && info->symbolic
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
	{
	  /* This is actually a static link, or it is a
	     -Bsymbolic link and the symbol is defined
	     locally.  We must initialize this entry in the
	     global section.

	     When doing a dynamic link, we create a .rela.<xxx>
	     relocation entry to initialize the value.  This
	     is done in the finish_dynamic_symbol routine.  */
	  if (!linker_section_ptr->written_address_p)
	    {
	      linker_section_ptr->written_address_p = true;
	      bfd_put_ptr (output_bfd, relocation + linker_section_ptr->addend,
			  lsect->section->contents + linker_section_ptr->offset);
	    }
	}
    }
  else				/* local symbol */
    {
      unsigned long r_symndx = ELF_R_SYM (rel->r_info);
      BFD_ASSERT (elf_local_ptr_offsets (input_bfd) != NULL);
      BFD_ASSERT (elf_local_ptr_offsets (input_bfd)[r_symndx] != NULL);
      linker_section_ptr = _bfd_elf_find_pointer_linker_section (elf_local_ptr_offsets (input_bfd)[r_symndx],
								 rel->r_addend,
								 lsect->which);

      BFD_ASSERT (linker_section_ptr != NULL);

      /* Write out pointer if it hasn't been rewritten out before */
      if (!linker_section_ptr->written_address_p)
	{
	  linker_section_ptr->written_address_p = true;
	  bfd_put_ptr (output_bfd, relocation + linker_section_ptr->addend,
		       lsect->section->contents + linker_section_ptr->offset);

	  if (info->shared)
	    {
	      asection *srel = lsect->rel_section;
	      Elf_Internal_Rela outrel;

	      /* We need to generate a relative reloc for the dynamic linker.  */
	      if (!srel)
		lsect->rel_section = srel = bfd_get_section_by_name (elf_hash_table (info)->dynobj,
								     lsect->rel_name);

	      BFD_ASSERT (srel != NULL);

	      outrel.r_offset = (lsect->section->output_section->vma
				 + lsect->section->output_offset
				 + linker_section_ptr->offset);
	      outrel.r_info = ELF_R_INFO (0, relative_reloc);
	      outrel.r_addend = 0;
	      elf_swap_reloca_out (output_bfd, &outrel,
				   (((Elf_External_Rela *)
				     lsect->section->contents)
				    + lsect->section->reloc_count));
	      ++lsect->section->reloc_count;
	    }
	}
    }

  relocation = (lsect->section->output_offset
		+ linker_section_ptr->offset
		- lsect->hole_offset
		- lsect->sym_offset);

#ifdef DEBUG
  fprintf (stderr, "Finish pointer in linker section %s, offset = %ld (0x%lx)\n",
	   lsect->name, (long)relocation, (long)relocation);
#endif

  /* Subtract out the addend, because it will get added back in by the normal
     processing.  */
  return relocation - linker_section_ptr->addend;
}
