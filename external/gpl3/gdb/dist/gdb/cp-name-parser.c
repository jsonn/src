/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 30 "cp-name-parser.y" /* yacc.c:339  */


#include "defs.h"

#include <unistd.h>
#include "safe-ctype.h"
#include "demangle.h"
#include "cp-support.h"

/* Bison does not make it easy to create a parser without global
   state, unfortunately.  Here are all the global variables used
   in this parser.  */

/* LEXPTR is the current pointer into our lex buffer.  PREV_LEXPTR
   is the start of the last token lexed, only used for diagnostics.
   ERROR_LEXPTR is the first place an error occurred.  GLOBAL_ERRMSG
   is the first error message encountered.  */

static const char *lexptr, *prev_lexptr, *error_lexptr, *global_errmsg;

/* The components built by the parser are allocated ahead of time,
   and cached in this structure.  */

#define ALLOC_CHUNK 100

struct demangle_info {
  int used;
  struct demangle_info *next;
  struct demangle_component comps[ALLOC_CHUNK];
};

static struct demangle_info *demangle_info;

static struct demangle_component *
d_grab (void)
{
  struct demangle_info *more;

  if (demangle_info->used >= ALLOC_CHUNK)
    {
      if (demangle_info->next == NULL)
	{
	  more = XNEW (struct demangle_info);
	  more->next = NULL;
	  demangle_info->next = more;
	}
      else
	more = demangle_info->next;

      more->used = 0;
      demangle_info = more;
    }
  return &demangle_info->comps[demangle_info->used++];
}

/* The parse tree created by the parser is stored here after a successful
   parse.  */

static struct demangle_component *global_result;

/* Prototypes for helper functions used when constructing the parse
   tree.  */

static struct demangle_component *d_qualify (struct demangle_component *, int,
					     int);

static struct demangle_component *d_int_type (int);

static struct demangle_component *d_unary (const char *,
					   struct demangle_component *);
static struct demangle_component *d_binary (const char *,
					    struct demangle_component *,
					    struct demangle_component *);

/* Flags passed to d_qualify.  */

#define QUAL_CONST 1
#define QUAL_RESTRICT 2
#define QUAL_VOLATILE 4

/* Flags passed to d_int_type.  */

#define INT_CHAR	(1 << 0)
#define INT_SHORT	(1 << 1)
#define INT_LONG	(1 << 2)
#define INT_LLONG	(1 << 3)

#define INT_SIGNED	(1 << 4)
#define INT_UNSIGNED	(1 << 5)

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth cpname_maxdepth
#define	yyparse	cpname_parse
#define	yylex	cpname_lex
#define	yyerror	cpname_error
#define	yylval	cpname_lval
#define	yychar	cpname_char
#define	yydebug	cpname_debug
#define	yypact	cpname_pact	
#define	yyr1	cpname_r1			
#define	yyr2	cpname_r2			
#define	yydef	cpname_def		
#define	yychk	cpname_chk		
#define	yypgo	cpname_pgo		
#define	yyact	cpname_act		
#define	yyexca	cpname_exca
#define yyerrflag cpname_errflag
#define yynerrs	cpname_nerrs
#define	yyps	cpname_ps
#define	yypv	cpname_pv
#define	yys	cpname_s
#define	yy_yys	cpname_yys
#define	yystate	cpname_state
#define	yytmp	cpname_tmp
#define	yyv	cpname_v
#define	yy_yyv	cpname_yyv
#define	yyval	cpname_val
#define	yylloc	cpname_lloc
#define yyreds	cpname_reds		/* With YYDEBUG defined */
#define yytoks	cpname_toks		/* With YYDEBUG defined */
#define yyname	cpname_name		/* With YYDEBUG defined */
#define yyrule	cpname_rule		/* With YYDEBUG defined */
#define yylhs	cpname_yylhs
#define yylen	cpname_yylen
#define yydefred cpname_yydefred
#define yydgoto	cpname_yydgoto
#define yysindex cpname_yysindex
#define yyrindex cpname_yyrindex
#define yygindex cpname_yygindex
#define yytable	 cpname_yytable
#define yycheck	 cpname_yycheck
#define yyss	cpname_yyss
#define yysslim	cpname_yysslim
#define yyssp	cpname_yyssp
#define yystacksize cpname_yystacksize
#define yyvs	cpname_yyvs
#define yyvsp	cpname_yyvsp

int yyparse (void);
static int yylex (void);
static void yyerror (char *);

/* Enable yydebug for the stand-alone parser.  */
#ifdef TEST_CPNAMES
# define YYDEBUG	1
#endif

/* Helper functions.  These wrap the demangler tree interface, handle
   allocation from our global store, and return the allocated component.  */

static struct demangle_component *
fill_comp (enum demangle_component_type d_type, struct demangle_component *lhs,
	   struct demangle_component *rhs)
{
  struct demangle_component *ret = d_grab ();
  int i;

  i = cplus_demangle_fill_component (ret, d_type, lhs, rhs);
  gdb_assert (i);

  return ret;
}

static struct demangle_component *
make_empty (enum demangle_component_type d_type)
{
  struct demangle_component *ret = d_grab ();
  ret->type = d_type;
  return ret;
}

static struct demangle_component *
make_operator (const char *name, int args)
{
  struct demangle_component *ret = d_grab ();
  int i;

  i = cplus_demangle_fill_operator (ret, name, args);
  gdb_assert (i);

  return ret;
}

static struct demangle_component *
make_dtor (enum gnu_v3_dtor_kinds kind, struct demangle_component *name)
{
  struct demangle_component *ret = d_grab ();
  int i;

  i = cplus_demangle_fill_dtor (ret, kind, name);
  gdb_assert (i);

  return ret;
}

static struct demangle_component *
make_builtin_type (const char *name)
{
  struct demangle_component *ret = d_grab ();
  int i;

  i = cplus_demangle_fill_builtin_type (ret, name);
  gdb_assert (i);

  return ret;
}

static struct demangle_component *
make_name (const char *name, int len)
{
  struct demangle_component *ret = d_grab ();
  int i;

  i = cplus_demangle_fill_name (ret, name, len);
  gdb_assert (i);

  return ret;
}

#define d_left(dc) (dc)->u.s_binary.left
#define d_right(dc) (dc)->u.s_binary.right


#line 296 "cp-name-parser.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif


/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    INT = 258,
    FLOAT = 259,
    NAME = 260,
    STRUCT = 261,
    CLASS = 262,
    UNION = 263,
    ENUM = 264,
    SIZEOF = 265,
    UNSIGNED = 266,
    COLONCOLON = 267,
    TEMPLATE = 268,
    ERROR = 269,
    NEW = 270,
    DELETE = 271,
    OPERATOR = 272,
    STATIC_CAST = 273,
    REINTERPRET_CAST = 274,
    DYNAMIC_CAST = 275,
    SIGNED_KEYWORD = 276,
    LONG = 277,
    SHORT = 278,
    INT_KEYWORD = 279,
    CONST_KEYWORD = 280,
    VOLATILE_KEYWORD = 281,
    DOUBLE_KEYWORD = 282,
    BOOL = 283,
    ELLIPSIS = 284,
    RESTRICT = 285,
    VOID = 286,
    FLOAT_KEYWORD = 287,
    CHAR = 288,
    WCHAR_T = 289,
    ASSIGN_MODIFY = 290,
    TRUEKEYWORD = 291,
    FALSEKEYWORD = 292,
    DEMANGLER_SPECIAL = 293,
    CONSTRUCTION_VTABLE = 294,
    CONSTRUCTION_IN = 295,
    OROR = 296,
    ANDAND = 297,
    EQUAL = 298,
    NOTEQUAL = 299,
    LEQ = 300,
    GEQ = 301,
    LSH = 302,
    RSH = 303,
    UNARY = 304,
    INCREMENT = 305,
    DECREMENT = 306,
    ARROW = 307
  };
#endif
/* Tokens.  */
#define INT 258
#define FLOAT 259
#define NAME 260
#define STRUCT 261
#define CLASS 262
#define UNION 263
#define ENUM 264
#define SIZEOF 265
#define UNSIGNED 266
#define COLONCOLON 267
#define TEMPLATE 268
#define ERROR 269
#define NEW 270
#define DELETE 271
#define OPERATOR 272
#define STATIC_CAST 273
#define REINTERPRET_CAST 274
#define DYNAMIC_CAST 275
#define SIGNED_KEYWORD 276
#define LONG 277
#define SHORT 278
#define INT_KEYWORD 279
#define CONST_KEYWORD 280
#define VOLATILE_KEYWORD 281
#define DOUBLE_KEYWORD 282
#define BOOL 283
#define ELLIPSIS 284
#define RESTRICT 285
#define VOID 286
#define FLOAT_KEYWORD 287
#define CHAR 288
#define WCHAR_T 289
#define ASSIGN_MODIFY 290
#define TRUEKEYWORD 291
#define FALSEKEYWORD 292
#define DEMANGLER_SPECIAL 293
#define CONSTRUCTION_VTABLE 294
#define CONSTRUCTION_IN 295
#define OROR 296
#define ANDAND 297
#define EQUAL 298
#define NOTEQUAL 299
#define LEQ 300
#define GEQ 301
#define LSH 302
#define RSH 303
#define UNARY 304
#define INCREMENT 305
#define DECREMENT 306
#define ARROW 307

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 261 "cp-name-parser.y" /* yacc.c:355  */

    struct demangle_component *comp;
    struct nested {
      struct demangle_component *comp;
      struct demangle_component **last;
    } nested;
    struct {
      struct demangle_component *comp, *last;
    } nested1;
    struct {
      struct demangle_component *comp, **last;
      struct nested fn;
      struct demangle_component *start;
      int fold_flag;
    } abstract;
    int lval;
    const char *opname;
  

#line 457 "cp-name-parser.c" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);



/* Copy the second part of user declarations.  */

#line 472 "cp-name-parser.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or xmalloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined xmalloc) \
             && (defined YYFREE || defined xfree)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC xmalloc
#   if ! defined xmalloc && ! defined EXIT_SUCCESS
void *xmalloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE xfree
#   if ! defined xfree && ! defined EXIT_SUCCESS
void xfree (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  84
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1072

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  40
/* YYNRULES -- Number of rules.  */
#define YYNRULES  194
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  324

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   307

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    72,     2,     2,     2,    63,    49,     2,
      73,    41,    61,    59,    42,    60,    68,    62,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    74,     2,
      52,    43,    53,    44,    58,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    69,     2,    70,    48,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    47,     2,    71,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    45,    46,    50,    51,
      54,    55,    56,    57,    64,    65,    66,    67
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   375,   375,   379,   381,   383,   388,   389,   396,   405,
     408,   412,   415,   434,   438,   442,   448,   454,   460,   466,
     468,   470,   472,   474,   476,   478,   480,   482,   484,   486,
     488,   490,   492,   494,   496,   498,   500,   502,   504,   506,
     508,   510,   512,   514,   516,   518,   520,   522,   530,   535,
     540,   544,   549,   557,   558,   560,   572,   573,   579,   581,
     582,   584,   587,   588,   591,   592,   596,   598,   601,   607,
     614,   620,   631,   635,   638,   649,   650,   654,   656,   658,
     661,   665,   670,   675,   681,   691,   695,   699,   707,   708,
     711,   713,   715,   719,   720,   727,   729,   731,   733,   735,
     737,   741,   742,   746,   748,   750,   752,   754,   756,   758,
     762,   768,   772,   780,   790,   794,   810,   812,   813,   815,
     818,   820,   821,   823,   826,   828,   830,   832,   837,   840,
     845,   852,   856,   867,   873,   891,   894,   902,   904,   915,
     922,   923,   929,   933,   937,   939,   944,   949,   962,   966,
     971,   979,   984,   993,   997,  1002,  1007,  1011,  1017,  1023,
    1026,  1033,  1035,  1040,  1044,  1048,  1055,  1071,  1078,  1085,
    1104,  1108,  1112,  1116,  1120,  1124,  1128,  1132,  1136,  1140,
    1144,  1148,  1152,  1156,  1160,  1164,  1168,  1173,  1177,  1181,
    1188,  1192,  1195,  1204,  1213
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "INT", "FLOAT", "NAME", "STRUCT",
  "CLASS", "UNION", "ENUM", "SIZEOF", "UNSIGNED", "COLONCOLON", "TEMPLATE",
  "ERROR", "NEW", "DELETE", "OPERATOR", "STATIC_CAST", "REINTERPRET_CAST",
  "DYNAMIC_CAST", "SIGNED_KEYWORD", "LONG", "SHORT", "INT_KEYWORD",
  "CONST_KEYWORD", "VOLATILE_KEYWORD", "DOUBLE_KEYWORD", "BOOL",
  "ELLIPSIS", "RESTRICT", "VOID", "FLOAT_KEYWORD", "CHAR", "WCHAR_T",
  "ASSIGN_MODIFY", "TRUEKEYWORD", "FALSEKEYWORD", "DEMANGLER_SPECIAL",
  "CONSTRUCTION_VTABLE", "CONSTRUCTION_IN", "')'", "','", "'='", "'?'",
  "OROR", "ANDAND", "'|'", "'^'", "'&'", "EQUAL", "NOTEQUAL", "'<'", "'>'",
  "LEQ", "GEQ", "LSH", "RSH", "'@'", "'+'", "'-'", "'*'", "'/'", "'%'",
  "UNARY", "INCREMENT", "DECREMENT", "ARROW", "'.'", "'['", "']'", "'~'",
  "'!'", "'('", "':'", "$accept", "result", "start", "start_opt",
  "function", "demangler_special", "oper", "conversion_op",
  "conversion_op_name", "unqualified_name", "colon_name", "name",
  "colon_ext_name", "colon_ext_only", "ext_only_name", "nested_name",
  "templ", "template_params", "template_arg", "function_args",
  "function_arglist", "qualifiers_opt", "qualifier", "qualifiers",
  "int_part", "int_seq", "builtin_type", "ptr_operator", "array_indicator",
  "typespec_2", "abstract_declarator", "direct_abstract_declarator",
  "abstract_declarator_fn", "type", "declarator", "direct_declarator",
  "declarator_1", "direct_declarator_1", "exp", "exp1", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,    41,    44,    61,    63,   296,   297,   124,    94,    38,
     298,   299,    60,    62,   300,   301,   302,   303,    64,    43,
      45,    42,    47,    37,   304,   305,   306,   307,    46,    91,
      93,   126,    33,    40,    58
};
# endif

#define YYPACT_NINF -203

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-203)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     769,    18,  -203,    15,   545,  -203,   -16,  -203,  -203,  -203,
    -203,  -203,  -203,  -203,  -203,  -203,  -203,  -203,   769,   769,
      23,    35,  -203,  -203,  -203,    -7,  -203,     5,  -203,   121,
     -25,  -203,    45,    49,   121,   883,  -203,   119,   121,   295,
    -203,  -203,   390,  -203,   121,  -203,    45,    66,    14,    26,
    -203,  -203,  -203,  -203,  -203,  -203,  -203,  -203,  -203,  -203,
    -203,  -203,  -203,  -203,  -203,  -203,  -203,  -203,  -203,  -203,
    -203,  -203,  -203,    43,    50,  -203,  -203,    86,   130,  -203,
    -203,  -203,    96,  -203,  -203,   390,    18,   769,  -203,   121,
       6,   608,  -203,     8,    49,   133,    52,  -203,    38,  -203,
    -203,   512,   133,    28,  -203,  -203,   147,  -203,  -203,    66,
     121,   121,  -203,  -203,  -203,    48,   796,   608,  -203,  -203,
      38,  -203,    51,   133,   311,  -203,    38,  -203,    38,  -203,
    -203,    75,   114,   122,   127,  -203,  -203,   661,   266,   266,
     266,   454,    10,  -203,   285,   899,  -203,  -203,   110,   111,
    -203,  -203,  -203,   769,    57,  -203,    67,  -203,  -203,   113,
    -203,    66,   143,   121,   285,    21,    17,   285,   285,   144,
      28,   121,   147,   769,  -203,   183,  -203,   178,  -203,  -203,
    -203,  -203,   121,  -203,  -203,  -203,    68,   716,   181,  -203,
    -203,   285,  -203,  -203,  -203,   182,  -203,   859,   859,   859,
     859,   769,  -203,   103,   103,   103,   685,   285,   159,   874,
     161,   390,  -203,  -203,   266,   266,   266,   266,   266,   266,
     266,   266,   266,   266,   266,   266,   266,   266,   266,   266,
     266,   266,   228,   229,  -203,  -203,  -203,  -203,   121,  -203,
      32,   121,  -203,   121,   793,  -203,  -203,  -203,    37,   769,
    -203,   716,  -203,   716,   196,    38,   769,   769,   199,   189,
     192,   194,   200,   769,  -203,   266,   266,  -203,  -203,   692,
     922,   944,   965,   985,  1004,   591,   591,   374,   374,   374,
     438,   438,   136,   136,   103,   103,   103,  -203,  -203,  -203,
    -203,  -203,  -203,   285,  -203,   214,  -203,  -203,  -203,  -203,
    -203,  -203,  -203,   185,   191,   193,  -203,   220,   103,   899,
     266,  -203,  -203,   492,   492,   492,  -203,   899,   221,   224,
     226,  -203,  -203,  -203
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    59,    97,     0,     0,    96,    99,   100,    95,    92,
      91,   105,   107,    90,   109,   104,    98,   108,     0,     0,
       0,     0,     2,     5,     4,    53,    50,     6,    67,   121,
       0,    64,     0,    61,    93,     0,   101,   103,   117,   140,
       3,    68,     0,    52,   125,    65,     0,     0,    15,    16,
      32,    43,    29,    40,    39,    26,    24,    25,    35,    36,
      30,    31,    37,    38,    33,    34,    19,    20,    21,    22,
      23,    41,    42,    45,     0,    27,    28,     0,     0,    48,
     106,    13,     0,    55,     1,     0,     0,     0,   111,    88,
       0,     0,    11,     0,     0,     6,   135,   134,   137,    12,
     120,     0,     6,    58,    49,    66,    60,    70,    94,     0,
     123,   119,    99,   102,   116,     0,     0,     0,    62,    56,
     149,    63,     0,     6,   128,   141,   130,     8,   150,   190,
     191,     0,     0,     0,     0,   193,   194,     0,     0,     0,
       0,     0,     0,    73,    75,    79,   124,    51,     0,     0,
      44,    47,    46,     0,     0,     7,     0,   110,    89,     0,
     114,     0,   109,    88,     0,     0,     0,   128,    80,     0,
       0,    88,     0,     0,   139,     0,   136,   132,   133,    10,
      69,    71,   127,   122,   118,    57,     0,   128,   156,   157,
       9,     0,   129,   148,   132,   154,   155,     0,     0,     0,
       0,     0,    77,   163,   165,   164,     0,   140,     0,   159,
       0,     0,    72,    76,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    17,    18,    14,    54,    88,   115,
       0,    88,    87,    88,     0,    81,   131,   112,     0,     0,
     126,     0,   147,   128,     0,   143,     0,     0,     0,     0,
       0,     0,     0,     0,   161,     0,     0,   158,    74,     0,
     186,   185,   184,   183,   182,   177,   178,   181,   179,   180,
     175,   176,   173,   174,   170,   171,   172,   187,   188,   113,
      86,    85,    84,    82,   138,     0,   142,   153,   145,   146,
     151,   152,   192,     0,     0,     0,    78,     0,   166,   160,
       0,    83,   144,     0,     0,     0,   162,   189,     0,     0,
       0,   167,   169,   168
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -203,  -203,    25,   -35,  -203,  -203,  -203,     1,  -203,     9,
    -203,    -1,   -34,   -24,     3,     0,   150,   186,    69,  -203,
     -23,  -149,  -203,   210,   231,  -203,   242,   -15,   -97,   188,
     -18,   -19,   187,   151,  -202,  -203,   154,  -203,    -6,  -159
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    21,   155,    92,    23,    24,    25,    26,    27,    28,
     118,    29,   252,    30,    31,    78,    33,   142,   143,   166,
      95,   157,    34,    35,    36,    37,    38,   167,    97,    39,
     169,   126,    99,    40,   254,   255,   127,   128,   209,   210
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint16 yytable[] =
{
      32,   178,    44,    46,    43,   120,    45,   102,    98,   159,
      86,    80,    96,   170,   242,   121,   123,    87,    32,    32,
       1,   125,   247,   189,   124,    22,   103,    93,    83,   178,
      41,   196,     4,   104,   110,    84,   145,   103,   119,   122,
     180,   105,   170,    81,    82,    85,    44,   147,   101,   295,
     103,   296,   211,     1,    88,   105,   103,    86,   243,   244,
     174,   107,     4,   212,   175,   116,    89,   179,   116,   171,
      42,     1,   103,   103,    90,   177,   160,    98,    91,   145,
      42,    96,   171,   148,     4,   116,    20,   156,   190,   289,
     120,   165,   290,   238,   291,   149,    93,   188,   238,   211,
     121,    88,   187,   194,   150,   195,   192,    90,   182,   124,
     237,   101,   171,    89,   185,   186,    20,   165,    45,    20,
     151,    90,    20,   119,   122,    91,   213,   152,   238,   238,
       2,   105,   203,   204,   205,   103,   153,    32,    20,    20,
       5,   112,     7,     8,    93,   173,     9,    10,   197,   192,
     245,    13,    16,    32,   318,   319,   320,   104,   299,   181,
      44,   240,   202,   121,    93,   105,   198,    93,    93,   192,
     232,   233,   253,    32,   199,   248,   187,    94,   236,   200,
     234,   235,   106,   239,   241,   246,   119,   122,    86,   125,
     249,    93,    79,   256,   257,   105,   106,   229,   230,   231,
     265,    32,   267,   232,   233,   145,    32,    93,   269,   270,
     271,   272,   273,   274,   275,   276,   277,   278,   279,   280,
     281,   282,   283,   284,   285,   286,   262,   121,   106,   121,
     144,   264,   298,   287,   288,   192,   253,   297,   253,   100,
     302,   306,   303,   172,   108,   304,    94,   305,   114,    32,
     119,   122,   119,   122,   146,   312,    32,    32,   313,   308,
     309,   316,   321,    32,   314,   322,   315,   323,   113,   129,
     130,   154,   106,   144,   294,   311,   131,   111,   193,   168,
     268,   300,   301,   176,   132,   133,   134,     0,   307,   168,
      86,     0,   208,    93,    94,     0,     0,   175,     0,   158,
       1,     0,   135,   136,   317,   168,   106,   115,     0,     0,
       0,     0,   116,     0,    94,   106,     1,    94,    94,     0,
     183,   184,     0,   115,     0,    94,   138,     0,   116,   207,
       0,     0,     0,     0,    88,     0,   106,   139,   140,   141,
       0,    94,     0,     0,    88,     0,    89,     0,   258,   259,
     260,   261,     0,     0,    90,     0,    89,    94,   164,     0,
      88,     0,     0,     0,    90,     0,    20,     0,   117,     0,
       0,     0,    89,   158,     0,     0,     0,     0,     0,     0,
      90,   158,    20,     0,   191,   207,   207,   207,   207,     0,
     106,     0,   250,   129,   130,     1,     0,     0,   172,   144,
     131,     2,    47,     0,     0,     0,     0,     0,   132,   133,
     134,     5,     6,     7,     8,     9,    10,    11,    12,     0,
      13,    14,    15,    16,    17,     0,   135,   136,     0,     0,
     225,   226,   293,   227,   228,   229,   230,   231,     0,   137,
       0,   232,   233,    94,     0,     0,     0,     0,   158,     0,
     138,   158,     0,   158,     0,     0,     0,   129,   130,     1,
       0,   139,   140,   141,   131,     2,    47,     0,     0,     0,
       0,     0,   132,   133,   134,     5,     6,     7,     8,     9,
      10,    11,    12,     0,    13,    14,    15,    16,    17,     0,
     135,   136,     0,     0,     0,   129,   130,   227,   228,   229,
     230,   231,   131,   206,     0,   232,   233,     0,     0,     0,
     132,   133,   134,     0,   138,     0,     0,     1,     0,     0,
       0,     0,     0,     2,    47,   139,   140,   141,   135,   136,
       0,     0,     0,     5,     6,     7,     8,     9,    10,    11,
      12,   206,    13,   162,    15,    16,    17,     0,     0,     0,
       1,     0,   138,   163,     0,     0,     2,    47,     0,     0,
      48,    49,     0,   139,   140,   141,     5,     6,     7,     8,
       9,    10,    11,    12,     0,    13,    14,    15,    16,    17,
      50,     0,     0,     0,     0,     0,     0,    51,    52,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,     0,    66,    67,    68,    69,    70,     0,
      71,    72,    73,     1,    74,     0,    75,    76,    77,     2,
     161,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     8,     9,    10,    11,    12,     0,    13,   162,
      15,    16,    17,   222,     0,   223,   224,   225,   226,   163,
     227,   228,   229,   230,   231,     0,     0,    88,   232,   233,
       0,     0,     0,     0,     0,     0,     1,     0,     0,    89,
       0,     0,     2,     3,     0,     0,     0,    90,     4,     0,
       0,   164,     5,     6,     7,     8,     9,    10,    11,    12,
       1,    13,    14,    15,    16,    17,     2,     3,     0,    18,
      19,     0,     4,     0,     0,     0,     5,     6,     7,     8,
       9,    10,    11,    12,     0,    13,    14,    15,    16,    17,
       0,     1,     0,    18,    19,     0,     0,     0,   115,     0,
       0,     0,    20,   116,   201,     0,   214,   215,   216,   217,
     218,   219,   220,   221,   222,     0,   223,   224,   225,   226,
       0,   227,   228,   229,   230,   231,    20,     0,   263,   232,
     233,     0,     0,     0,     0,    88,   310,     0,     0,     0,
       0,     0,     0,     0,     1,     0,     0,    89,     0,     0,
       2,     3,     0,     0,     0,    90,     4,    20,     0,   251,
       5,     6,     7,     8,     9,    10,    11,    12,     1,    13,
      14,    15,    16,    17,     2,    47,     0,    18,    19,     0,
       0,    48,    49,     0,     5,     6,     7,     8,     9,    10,
      11,    12,   292,    13,    14,    15,    16,    17,     0,     0,
       0,    50,     0,     0,     0,     0,     0,     0,    51,    52,
      20,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,     0,    66,    67,    68,    69,    70,
       0,    71,    72,    73,     1,    74,     0,    75,    76,    77,
       2,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     8,     9,    10,    11,    12,     1,    13,
      14,    15,    16,    17,     2,   109,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     8,     0,     0,
      11,    12,     0,     0,    14,    15,    16,    17,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   266,   223,   224,
     225,   226,     0,   227,   228,   229,   230,   231,     0,     0,
       0,   232,   233,   214,   215,   216,   217,   218,   219,   220,
     221,   222,     0,   223,   224,   225,   226,     0,   227,   228,
     229,   230,   231,     0,     0,     0,   232,   233,   216,   217,
     218,   219,   220,   221,   222,     0,   223,   224,   225,   226,
       0,   227,   228,   229,   230,   231,     0,     0,     0,   232,
     233,   217,   218,   219,   220,   221,   222,     0,   223,   224,
     225,   226,     0,   227,   228,   229,   230,   231,     0,     0,
       0,   232,   233,   218,   219,   220,   221,   222,     0,   223,
     224,   225,   226,     0,   227,   228,   229,   230,   231,     0,
       0,     0,   232,   233,   219,   220,   221,   222,     0,   223,
     224,   225,   226,     0,   227,   228,   229,   230,   231,     0,
       0,     0,   232,   233,   220,   221,   222,     0,   223,   224,
     225,   226,     0,   227,   228,   229,   230,   231,     0,     0,
       0,   232,   233
};

static const yytype_int16 yycheck[] =
{
       0,    98,     3,     3,     3,    39,     3,    30,    27,     3,
       5,    27,    27,     5,   163,    39,    39,    12,    18,    19,
       5,    39,   171,   120,    39,     0,     5,    27,     5,   126,
      12,   128,    17,    32,    35,     0,    42,     5,    39,    39,
      12,    32,     5,    18,    19,    52,    47,    46,    73,   251,
       5,   253,    42,     5,    49,    46,     5,     5,    41,    42,
      95,    12,    17,    53,    12,    17,    61,   102,    17,    61,
      52,     5,     5,     5,    69,    98,    70,    96,    73,    85,
      52,    96,    61,    69,    17,    17,    71,    87,   123,   238,
     124,    91,   241,    61,   243,    69,    96,   120,    61,    42,
     124,    49,   117,   126,    61,   128,   124,    69,   109,   124,
      53,    73,    61,    61,   115,   115,    71,   117,   115,    71,
      70,    69,    71,   124,   124,    73,   144,    41,    61,    61,
      11,   122,   138,   139,   140,     5,    40,   137,    71,    71,
      21,    22,    23,    24,   144,    12,    25,    26,    73,   167,
     168,    30,    33,   153,   313,   314,   315,   156,   255,    12,
     161,   161,   137,   187,   164,   156,    52,   167,   168,   187,
      67,    68,   187,   173,    52,   175,   191,    27,   153,    52,
      70,    70,    32,    70,    41,    41,   187,   187,     5,   207,
      12,   191,     4,    12,    12,   186,    46,    61,    62,    63,
      41,   201,    41,    67,    68,   211,   206,   207,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   201,   251,    78,   253,
      42,   206,   255,     5,     5,   253,   251,    41,   253,    29,
      41,    41,    53,    93,    34,    53,    96,    53,    38,   249,
     251,   251,   253,   253,    44,    41,   256,   257,    73,   265,
     266,    41,    41,   263,    73,    41,    73,    41,    37,     3,
       4,    85,   122,    85,   249,   293,    10,    35,   124,    91,
     211,   256,   257,    96,    18,    19,    20,    -1,   263,   101,
       5,    -1,   141,   293,   144,    -1,    -1,    12,    -1,    89,
       5,    -1,    36,    37,   310,   117,   156,    12,    -1,    -1,
      -1,    -1,    17,    -1,   164,   165,     5,   167,   168,    -1,
     110,   111,    -1,    12,    -1,   175,    60,    -1,    17,   141,
      -1,    -1,    -1,    -1,    49,    -1,   186,    71,    72,    73,
      -1,   191,    -1,    -1,    49,    -1,    61,    -1,   197,   198,
     199,   200,    -1,    -1,    69,    -1,    61,   207,    73,    -1,
      49,    -1,    -1,    -1,    69,    -1,    71,    -1,    73,    -1,
      -1,    -1,    61,   163,    -1,    -1,    -1,    -1,    -1,    -1,
      69,   171,    71,    -1,    73,   197,   198,   199,   200,    -1,
     240,    -1,   182,     3,     4,     5,    -1,    -1,   248,   211,
      10,    11,    12,    -1,    -1,    -1,    -1,    -1,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    -1,
      30,    31,    32,    33,    34,    -1,    36,    37,    -1,    -1,
      56,    57,   244,    59,    60,    61,    62,    63,    -1,    49,
      -1,    67,    68,   293,    -1,    -1,    -1,    -1,   238,    -1,
      60,   241,    -1,   243,    -1,    -1,    -1,     3,     4,     5,
      -1,    71,    72,    73,    10,    11,    12,    -1,    -1,    -1,
      -1,    -1,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    -1,    30,    31,    32,    33,    34,    -1,
      36,    37,    -1,    -1,    -1,     3,     4,    59,    60,    61,
      62,    63,    10,    49,    -1,    67,    68,    -1,    -1,    -1,
      18,    19,    20,    -1,    60,    -1,    -1,     5,    -1,    -1,
      -1,    -1,    -1,    11,    12,    71,    72,    73,    36,    37,
      -1,    -1,    -1,    21,    22,    23,    24,    25,    26,    27,
      28,    49,    30,    31,    32,    33,    34,    -1,    -1,    -1,
       5,    -1,    60,    41,    -1,    -1,    11,    12,    -1,    -1,
      15,    16,    -1,    71,    72,    73,    21,    22,    23,    24,
      25,    26,    27,    28,    -1,    30,    31,    32,    33,    34,
      35,    -1,    -1,    -1,    -1,    -1,    -1,    42,    43,    -1,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    61,    62,    63,    -1,
      65,    66,    67,     5,    69,    -1,    71,    72,    73,    11,
      12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    21,
      22,    23,    24,    25,    26,    27,    28,    -1,    30,    31,
      32,    33,    34,    52,    -1,    54,    55,    56,    57,    41,
      59,    60,    61,    62,    63,    -1,    -1,    49,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,     5,    -1,    -1,    61,
      -1,    -1,    11,    12,    -1,    -1,    -1,    69,    17,    -1,
      -1,    73,    21,    22,    23,    24,    25,    26,    27,    28,
       5,    30,    31,    32,    33,    34,    11,    12,    -1,    38,
      39,    -1,    17,    -1,    -1,    -1,    21,    22,    23,    24,
      25,    26,    27,    28,    -1,    30,    31,    32,    33,    34,
      -1,     5,    -1,    38,    39,    -1,    -1,    -1,    12,    -1,
      -1,    -1,    71,    17,    73,    -1,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    -1,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    71,    -1,    73,    67,
      68,    -1,    -1,    -1,    -1,    49,    74,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     5,    -1,    -1,    61,    -1,    -1,
      11,    12,    -1,    -1,    -1,    69,    17,    71,    -1,    73,
      21,    22,    23,    24,    25,    26,    27,    28,     5,    30,
      31,    32,    33,    34,    11,    12,    -1,    38,    39,    -1,
      -1,    15,    16,    -1,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    -1,    -1,
      -1,    35,    -1,    -1,    -1,    -1,    -1,    -1,    42,    43,
      71,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    61,    62,    63,
      -1,    65,    66,    67,     5,    69,    -1,    71,    72,    73,
      11,    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      21,    22,    23,    24,    25,    26,    27,    28,     5,    30,
      31,    32,    33,    34,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    21,    22,    23,    24,    -1,    -1,
      27,    28,    -1,    -1,    31,    32,    33,    34,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    67,    68,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    -1,    54,    55,    56,    57,    -1,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    67,    68,    46,    47,
      48,    49,    50,    51,    52,    -1,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    -1,    -1,    -1,    67,
      68,    47,    48,    49,    50,    51,    52,    -1,    54,    55,
      56,    57,    -1,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    67,    68,    48,    49,    50,    51,    52,    -1,    54,
      55,    56,    57,    -1,    59,    60,    61,    62,    63,    -1,
      -1,    -1,    67,    68,    49,    50,    51,    52,    -1,    54,
      55,    56,    57,    -1,    59,    60,    61,    62,    63,    -1,
      -1,    -1,    67,    68,    50,    51,    52,    -1,    54,    55,
      56,    57,    -1,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    67,    68
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     5,    11,    12,    17,    21,    22,    23,    24,    25,
      26,    27,    28,    30,    31,    32,    33,    34,    38,    39,
      71,    76,    77,    79,    80,    81,    82,    83,    84,    86,
      88,    89,    90,    91,    97,    98,    99,   100,   101,   104,
     108,    12,    52,    82,    86,    89,    90,    12,    15,    16,
      35,    42,    43,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    59,    60,    61,    62,
      63,    65,    66,    67,    69,    71,    72,    73,    90,   104,
      27,    77,    77,     5,     0,    52,     5,    12,    49,    61,
      69,    73,    78,    90,    91,    95,   102,   103,   106,   107,
      98,    73,    95,     5,    82,    84,    91,    12,    98,    12,
      86,   101,    22,    99,    98,    12,    17,    73,    85,    86,
      87,    88,    90,    95,   102,   105,   106,   111,   112,     3,
       4,    10,    18,    19,    20,    36,    37,    49,    60,    71,
      72,    73,    92,    93,   104,   113,    98,    82,    69,    69,
      61,    70,    41,    40,    92,    77,    90,    96,    98,     3,
      70,    12,    31,    41,    73,    90,    94,   102,   104,   105,
       5,    61,    91,    12,    78,    12,   107,    95,   103,    78,
      12,    12,    86,    98,    98,    86,    90,   102,    95,   103,
      78,    73,   105,   111,    95,    95,   103,    73,    52,    52,
      52,    73,    77,   113,   113,   113,    49,   104,   108,   113,
     114,    42,    53,   105,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    54,    55,    56,    57,    59,    60,    61,
      62,    63,    67,    68,    70,    70,    77,    53,    61,    70,
      90,    41,    96,    41,    42,   105,    41,    96,    90,    12,
      98,    73,    87,   102,   109,   110,    12,    12,   108,   108,
     108,   108,    77,    73,    77,    41,    53,    41,    93,   113,
     113,   113,   113,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,   113,   113,     5,     5,    96,
      96,    96,    29,   104,    77,   109,   109,    41,    95,   103,
      77,    77,    41,    53,    53,    53,    41,    77,   113,   113,
      74,   105,    41,    73,    73,    73,    41,   113,   114,   114,
     114,    41,    41,    41
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    75,    76,    77,    77,    77,    78,    78,    79,    79,
      79,    79,    79,    80,    80,    81,    81,    81,    81,    81,
      81,    81,    81,    81,    81,    81,    81,    81,    81,    81,
      81,    81,    81,    81,    81,    81,    81,    81,    81,    81,
      81,    81,    81,    81,    81,    81,    81,    81,    82,    83,
      83,    83,    83,    84,    84,    84,    85,    85,    86,    86,
      86,    86,    87,    87,    88,    88,    89,    89,    90,    90,
      90,    90,    91,    92,    92,    93,    93,    93,    93,    93,
      94,    94,    94,    94,    94,    95,    95,    95,    96,    96,
      97,    97,    97,    98,    98,    99,    99,    99,    99,    99,
      99,   100,   100,   101,   101,   101,   101,   101,   101,   101,
     102,   102,   102,   102,   103,   103,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   105,   105,
     105,   106,   106,   106,   106,   107,   107,   107,   107,   107,
     108,   108,   109,   109,   110,   110,   110,   110,   111,   111,
     111,   111,   111,   112,   112,   112,   112,   112,   113,   114,
     114,   114,   114,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     1,     0,     2,     2,     3,
       3,     2,     2,     2,     4,     2,     2,     4,     4,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     3,     2,     3,     3,     2,     2,
       1,     3,     2,     1,     4,     2,     1,     2,     2,     1,
       2,     1,     1,     1,     1,     2,     2,     1,     2,     3,
       2,     3,     4,     1,     3,     1,     2,     2,     4,     1,
       1,     2,     3,     4,     3,     4,     4,     3,     0,     1,
       1,     1,     1,     1,     2,     1,     1,     1,     1,     1,
       1,     1,     2,     1,     1,     1,     2,     1,     1,     1,
       2,     1,     3,     4,     2,     3,     2,     1,     3,     2,
       2,     1,     3,     2,     3,     2,     4,     3,     1,     2,
       1,     3,     2,     2,     1,     1,     2,     1,     4,     2,
       1,     2,     2,     1,     3,     2,     2,     1,     2,     1,
       1,     4,     4,     4,     2,     2,     2,     2,     3,     1,
       3,     2,     4,     2,     2,     2,     4,     7,     7,     7,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     5,
       1,     1,     4,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to xreallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to xreallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 376 "cp-name-parser.y" /* yacc.c:1646  */
    { global_result = (yyvsp[0].comp); }
#line 1941 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 6:
#line 388 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = NULL; }
#line 1947 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 7:
#line 390 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[0].comp); }
#line 1953 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 8:
#line 397 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[0].nested).comp;
			  *(yyvsp[0].nested).last = (yyvsp[-1].comp);
			}
#line 1961 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 9:
#line 406 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, (yyvsp[-2].comp), (yyvsp[-1].nested).comp);
			  if ((yyvsp[0].comp)) (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, (yyval.comp), (yyvsp[0].comp)); }
#line 1968 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 10:
#line 409 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, (yyvsp[-2].comp), (yyvsp[-1].nested).comp);
			  if ((yyvsp[0].comp)) (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, (yyval.comp), (yyvsp[0].comp)); }
#line 1975 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 11:
#line 413 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[-1].nested).comp;
			  if ((yyvsp[0].comp)) (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, (yyval.comp), (yyvsp[0].comp)); }
#line 1982 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 12:
#line 416 "cp-name-parser.y" /* yacc.c:1646  */
    { if ((yyvsp[0].abstract).last)
			    {
			       /* First complete the abstract_declarator's type using
				  the typespec from the conversion_op_name.  */
			      *(yyvsp[0].abstract).last = *(yyvsp[-1].nested).last;
			      /* Then complete the conversion_op_name with the type.  */
			      *(yyvsp[-1].nested).last = (yyvsp[0].abstract).comp;
			    }
			  /* If we have an arglist, build a function type.  */
			  if ((yyvsp[0].abstract).fn.comp)
			    (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, (yyvsp[-1].nested).comp, (yyvsp[0].abstract).fn.comp);
			  else
			    (yyval.comp) = (yyvsp[-1].nested).comp;
			  if ((yyvsp[0].abstract).start) (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, (yyval.comp), (yyvsp[0].abstract).start);
			}
#line 2002 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 13:
#line 435 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_empty ((enum demangle_component_type) (yyvsp[-1].lval));
			  d_left ((yyval.comp)) = (yyvsp[0].comp);
			  d_right ((yyval.comp)) = NULL; }
#line 2010 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 14:
#line 439 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_CONSTRUCTION_VTABLE, (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 2016 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 15:
#line 443 "cp-name-parser.y" /* yacc.c:1646  */
    {
			  /* Match the whitespacing of cplus_demangle_operators.
			     It would abort on unrecognized string otherwise.  */
			  (yyval.comp) = make_operator ("new", 3);
			}
#line 2026 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 16:
#line 449 "cp-name-parser.y" /* yacc.c:1646  */
    {
			  /* Match the whitespacing of cplus_demangle_operators.
			     It would abort on unrecognized string otherwise.  */
			  (yyval.comp) = make_operator ("delete ", 1);
			}
#line 2036 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 17:
#line 455 "cp-name-parser.y" /* yacc.c:1646  */
    {
			  /* Match the whitespacing of cplus_demangle_operators.
			     It would abort on unrecognized string otherwise.  */
			  (yyval.comp) = make_operator ("new[]", 3);
			}
#line 2046 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 18:
#line 461 "cp-name-parser.y" /* yacc.c:1646  */
    {
			  /* Match the whitespacing of cplus_demangle_operators.
			     It would abort on unrecognized string otherwise.  */
			  (yyval.comp) = make_operator ("delete[] ", 1);
			}
#line 2056 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 19:
#line 467 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("+", 2); }
#line 2062 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 20:
#line 469 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("-", 2); }
#line 2068 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 21:
#line 471 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("*", 2); }
#line 2074 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 22:
#line 473 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("/", 2); }
#line 2080 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 23:
#line 475 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("%", 2); }
#line 2086 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 24:
#line 477 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("^", 2); }
#line 2092 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 25:
#line 479 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("&", 2); }
#line 2098 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 26:
#line 481 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("|", 2); }
#line 2104 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 27:
#line 483 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("~", 1); }
#line 2110 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 28:
#line 485 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("!", 1); }
#line 2116 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 29:
#line 487 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("=", 2); }
#line 2122 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 30:
#line 489 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("<", 2); }
#line 2128 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 31:
#line 491 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator (">", 2); }
#line 2134 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 32:
#line 493 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ((yyvsp[0].opname), 2); }
#line 2140 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 33:
#line 495 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("<<", 2); }
#line 2146 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 34:
#line 497 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator (">>", 2); }
#line 2152 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 35:
#line 499 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("==", 2); }
#line 2158 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 36:
#line 501 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("!=", 2); }
#line 2164 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 37:
#line 503 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("<=", 2); }
#line 2170 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 38:
#line 505 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator (">=", 2); }
#line 2176 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 39:
#line 507 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("&&", 2); }
#line 2182 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 40:
#line 509 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("||", 2); }
#line 2188 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 41:
#line 511 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("++", 1); }
#line 2194 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 42:
#line 513 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("--", 1); }
#line 2200 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 43:
#line 515 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator (",", 2); }
#line 2206 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 44:
#line 517 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("->*", 2); }
#line 2212 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 45:
#line 519 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("->", 2); }
#line 2218 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 46:
#line 521 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("()", 2); }
#line 2224 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 47:
#line 523 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_operator ("[]", 2); }
#line 2230 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 48:
#line 531 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_CONVERSION, (yyvsp[0].comp), NULL); }
#line 2236 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 49:
#line 536 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-1].nested1).comp;
			  d_right ((yyvsp[-1].nested1).last) = (yyvsp[0].comp);
			  (yyval.nested).last = &d_left ((yyvsp[0].comp));
			}
#line 2245 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 50:
#line 541 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[0].comp);
			  (yyval.nested).last = &d_left ((yyvsp[0].comp));
			}
#line 2253 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 51:
#line 545 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-1].nested1).comp;
			  d_right ((yyvsp[-1].nested1).last) = (yyvsp[0].comp);
			  (yyval.nested).last = &d_left ((yyvsp[0].comp));
			}
#line 2262 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 52:
#line 550 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[0].comp);
			  (yyval.nested).last = &d_left ((yyvsp[0].comp));
			}
#line 2270 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 54:
#line 559 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_TEMPLATE, (yyvsp[-3].comp), (yyvsp[-1].nested).comp); }
#line 2276 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 55:
#line 561 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_dtor (gnu_v3_complete_object_dtor, (yyvsp[0].comp)); }
#line 2282 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 57:
#line 574 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[0].comp); }
#line 2288 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 58:
#line 580 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[-1].nested1).comp; d_right ((yyvsp[-1].nested1).last) = (yyvsp[0].comp); }
#line 2294 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 60:
#line 583 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[-1].nested1).comp; d_right ((yyvsp[-1].nested1).last) = (yyvsp[0].comp); }
#line 2300 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 65:
#line 593 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[0].comp); }
#line 2306 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 66:
#line 597 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[-1].nested1).comp; d_right ((yyvsp[-1].nested1).last) = (yyvsp[0].comp); }
#line 2312 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 68:
#line 602 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested1).comp = make_empty (DEMANGLE_COMPONENT_QUAL_NAME);
			  d_left ((yyval.nested1).comp) = (yyvsp[-1].comp);
			  d_right ((yyval.nested1).comp) = NULL;
			  (yyval.nested1).last = (yyval.nested1).comp;
			}
#line 2322 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 69:
#line 608 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested1).comp = (yyvsp[-2].nested1).comp;
			  d_right ((yyvsp[-2].nested1).last) = make_empty (DEMANGLE_COMPONENT_QUAL_NAME);
			  (yyval.nested1).last = d_right ((yyvsp[-2].nested1).last);
			  d_left ((yyval.nested1).last) = (yyvsp[-1].comp);
			  d_right ((yyval.nested1).last) = NULL;
			}
#line 2333 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 70:
#line 615 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested1).comp = make_empty (DEMANGLE_COMPONENT_QUAL_NAME);
			  d_left ((yyval.nested1).comp) = (yyvsp[-1].comp);
			  d_right ((yyval.nested1).comp) = NULL;
			  (yyval.nested1).last = (yyval.nested1).comp;
			}
#line 2343 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 71:
#line 621 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested1).comp = (yyvsp[-2].nested1).comp;
			  d_right ((yyvsp[-2].nested1).last) = make_empty (DEMANGLE_COMPONENT_QUAL_NAME);
			  (yyval.nested1).last = d_right ((yyvsp[-2].nested1).last);
			  d_left ((yyval.nested1).last) = (yyvsp[-1].comp);
			  d_right ((yyval.nested1).last) = NULL;
			}
#line 2354 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 72:
#line 632 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_TEMPLATE, (yyvsp[-3].comp), (yyvsp[-1].nested).comp); }
#line 2360 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 73:
#line 636 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_TEMPLATE_ARGLIST, (yyvsp[0].comp), NULL);
			(yyval.nested).last = &d_right ((yyval.nested).comp); }
#line 2367 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 74:
#line 639 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-2].nested).comp;
			  *(yyvsp[-2].nested).last = fill_comp (DEMANGLE_COMPONENT_TEMPLATE_ARGLIST, (yyvsp[0].comp), NULL);
			  (yyval.nested).last = &d_right (*(yyvsp[-2].nested).last);
			}
#line 2376 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 76:
#line 651 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[0].abstract).comp;
			  *(yyvsp[0].abstract).last = (yyvsp[-1].comp);
			}
#line 2384 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 77:
#line 655 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator ("&", 1), (yyvsp[0].comp)); }
#line 2390 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 78:
#line 657 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator ("&", 1), (yyvsp[-1].comp)); }
#line 2396 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 80:
#line 662 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_ARGLIST, (yyvsp[0].comp), NULL);
			  (yyval.nested).last = &d_right ((yyval.nested).comp);
			}
#line 2404 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 81:
#line 666 "cp-name-parser.y" /* yacc.c:1646  */
    { *(yyvsp[0].abstract).last = (yyvsp[-1].comp);
			  (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_ARGLIST, (yyvsp[0].abstract).comp, NULL);
			  (yyval.nested).last = &d_right ((yyval.nested).comp);
			}
#line 2413 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 82:
#line 671 "cp-name-parser.y" /* yacc.c:1646  */
    { *(yyvsp[-2].nested).last = fill_comp (DEMANGLE_COMPONENT_ARGLIST, (yyvsp[0].comp), NULL);
			  (yyval.nested).comp = (yyvsp[-2].nested).comp;
			  (yyval.nested).last = &d_right (*(yyvsp[-2].nested).last);
			}
#line 2422 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 83:
#line 676 "cp-name-parser.y" /* yacc.c:1646  */
    { *(yyvsp[0].abstract).last = (yyvsp[-1].comp);
			  *(yyvsp[-3].nested).last = fill_comp (DEMANGLE_COMPONENT_ARGLIST, (yyvsp[0].abstract).comp, NULL);
			  (yyval.nested).comp = (yyvsp[-3].nested).comp;
			  (yyval.nested).last = &d_right (*(yyvsp[-3].nested).last);
			}
#line 2432 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 84:
#line 682 "cp-name-parser.y" /* yacc.c:1646  */
    { *(yyvsp[-2].nested).last
			    = fill_comp (DEMANGLE_COMPONENT_ARGLIST,
					   make_builtin_type ("..."),
					   NULL);
			  (yyval.nested).comp = (yyvsp[-2].nested).comp;
			  (yyval.nested).last = &d_right (*(yyvsp[-2].nested).last);
			}
#line 2444 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 85:
#line 692 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_FUNCTION_TYPE, NULL, (yyvsp[-2].nested).comp);
			  (yyval.nested).last = &d_left ((yyval.nested).comp);
			  (yyval.nested).comp = d_qualify ((yyval.nested).comp, (yyvsp[0].lval), 1); }
#line 2452 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 86:
#line 696 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_FUNCTION_TYPE, NULL, NULL);
			  (yyval.nested).last = &d_left ((yyval.nested).comp);
			  (yyval.nested).comp = d_qualify ((yyval.nested).comp, (yyvsp[0].lval), 1); }
#line 2460 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 87:
#line 700 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_FUNCTION_TYPE, NULL, NULL);
			  (yyval.nested).last = &d_left ((yyval.nested).comp);
			  (yyval.nested).comp = d_qualify ((yyval.nested).comp, (yyvsp[0].lval), 1); }
#line 2468 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 88:
#line 707 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = 0; }
#line 2474 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 90:
#line 712 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = QUAL_RESTRICT; }
#line 2480 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 91:
#line 714 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = QUAL_VOLATILE; }
#line 2486 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 92:
#line 716 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = QUAL_CONST; }
#line 2492 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 94:
#line 721 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = (yyvsp[-1].lval) | (yyvsp[0].lval); }
#line 2498 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 95:
#line 728 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = 0; }
#line 2504 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 96:
#line 730 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = INT_SIGNED; }
#line 2510 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 97:
#line 732 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = INT_UNSIGNED; }
#line 2516 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 98:
#line 734 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = INT_CHAR; }
#line 2522 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 99:
#line 736 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = INT_LONG; }
#line 2528 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 100:
#line 738 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = INT_SHORT; }
#line 2534 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 102:
#line 743 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.lval) = (yyvsp[-1].lval) | (yyvsp[0].lval); if ((yyvsp[-1].lval) & (yyvsp[0].lval) & INT_LONG) (yyval.lval) = (yyvsp[-1].lval) | INT_LLONG; }
#line 2540 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 103:
#line 747 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_int_type ((yyvsp[0].lval)); }
#line 2546 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 104:
#line 749 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_builtin_type ("float"); }
#line 2552 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 105:
#line 751 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_builtin_type ("double"); }
#line 2558 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 106:
#line 753 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_builtin_type ("long double"); }
#line 2564 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 107:
#line 755 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_builtin_type ("bool"); }
#line 2570 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 108:
#line 757 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_builtin_type ("wchar_t"); }
#line 2576 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 109:
#line 759 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_builtin_type ("void"); }
#line 2582 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 110:
#line 763 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = make_empty (DEMANGLE_COMPONENT_POINTER);
			  (yyval.nested).comp->u.s_binary.left = (yyval.nested).comp->u.s_binary.right = NULL;
			  (yyval.nested).last = &d_left ((yyval.nested).comp);
			  (yyval.nested).comp = d_qualify ((yyval.nested).comp, (yyvsp[0].lval), 0); }
#line 2591 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 111:
#line 769 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = make_empty (DEMANGLE_COMPONENT_REFERENCE);
			  (yyval.nested).comp->u.s_binary.left = (yyval.nested).comp->u.s_binary.right = NULL;
			  (yyval.nested).last = &d_left ((yyval.nested).comp); }
#line 2599 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 112:
#line 773 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = make_empty (DEMANGLE_COMPONENT_PTRMEM_TYPE);
			  (yyval.nested).comp->u.s_binary.left = (yyvsp[-2].nested1).comp;
			  /* Convert the innermost DEMANGLE_COMPONENT_QUAL_NAME to a DEMANGLE_COMPONENT_NAME.  */
			  *(yyvsp[-2].nested1).last = *d_left ((yyvsp[-2].nested1).last);
			  (yyval.nested).comp->u.s_binary.right = NULL;
			  (yyval.nested).last = &d_right ((yyval.nested).comp);
			  (yyval.nested).comp = d_qualify ((yyval.nested).comp, (yyvsp[0].lval), 0); }
#line 2611 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 113:
#line 781 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = make_empty (DEMANGLE_COMPONENT_PTRMEM_TYPE);
			  (yyval.nested).comp->u.s_binary.left = (yyvsp[-2].nested1).comp;
			  /* Convert the innermost DEMANGLE_COMPONENT_QUAL_NAME to a DEMANGLE_COMPONENT_NAME.  */
			  *(yyvsp[-2].nested1).last = *d_left ((yyvsp[-2].nested1).last);
			  (yyval.nested).comp->u.s_binary.right = NULL;
			  (yyval.nested).last = &d_right ((yyval.nested).comp);
			  (yyval.nested).comp = d_qualify ((yyval.nested).comp, (yyvsp[0].lval), 0); }
#line 2623 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 114:
#line 791 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_empty (DEMANGLE_COMPONENT_ARRAY_TYPE);
			  d_left ((yyval.comp)) = NULL;
			}
#line 2631 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 115:
#line 795 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = make_empty (DEMANGLE_COMPONENT_ARRAY_TYPE);
			  d_left ((yyval.comp)) = (yyvsp[-1].comp);
			}
#line 2639 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 116:
#line 811 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[-1].comp), (yyvsp[0].lval), 0); }
#line 2645 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 118:
#line 814 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[-1].comp), (yyvsp[-2].lval) | (yyvsp[0].lval), 0); }
#line 2651 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 119:
#line 816 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[0].comp), (yyvsp[-1].lval), 0); }
#line 2657 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 120:
#line 819 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[-1].comp), (yyvsp[0].lval), 0); }
#line 2663 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 122:
#line 822 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[-1].comp), (yyvsp[-2].lval) | (yyvsp[0].lval), 0); }
#line 2669 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 123:
#line 824 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[0].comp), (yyvsp[-1].lval), 0); }
#line 2675 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 124:
#line 827 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[-1].comp), (yyvsp[0].lval), 0); }
#line 2681 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 125:
#line 829 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[0].comp); }
#line 2687 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 126:
#line 831 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[-1].comp), (yyvsp[-3].lval) | (yyvsp[0].lval), 0); }
#line 2693 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 127:
#line 833 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_qualify ((yyvsp[0].comp), (yyvsp[-2].lval), 0); }
#line 2699 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 128:
#line 838 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).comp = (yyvsp[0].nested).comp; (yyval.abstract).last = (yyvsp[0].nested).last;
			  (yyval.abstract).fn.comp = NULL; (yyval.abstract).fn.last = NULL; }
#line 2706 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 129:
#line 841 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract) = (yyvsp[0].abstract); (yyval.abstract).fn.comp = NULL; (yyval.abstract).fn.last = NULL;
			  if ((yyvsp[0].abstract).fn.comp) { (yyval.abstract).last = (yyvsp[0].abstract).fn.last; *(yyvsp[0].abstract).last = (yyvsp[0].abstract).fn.comp; }
			  *(yyval.abstract).last = (yyvsp[-1].nested).comp;
			  (yyval.abstract).last = (yyvsp[-1].nested).last; }
#line 2715 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 130:
#line 846 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).fn.comp = NULL; (yyval.abstract).fn.last = NULL;
			  if ((yyvsp[0].abstract).fn.comp) { (yyval.abstract).last = (yyvsp[0].abstract).fn.last; *(yyvsp[0].abstract).last = (yyvsp[0].abstract).fn.comp; }
			}
#line 2723 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 131:
#line 853 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract) = (yyvsp[-1].abstract); (yyval.abstract).fn.comp = NULL; (yyval.abstract).fn.last = NULL; (yyval.abstract).fold_flag = 1;
			  if ((yyvsp[-1].abstract).fn.comp) { (yyval.abstract).last = (yyvsp[-1].abstract).fn.last; *(yyvsp[-1].abstract).last = (yyvsp[-1].abstract).fn.comp; }
			}
#line 2731 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 132:
#line 857 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).fold_flag = 0;
			  if ((yyvsp[-1].abstract).fn.comp) { (yyval.abstract).last = (yyvsp[-1].abstract).fn.last; *(yyvsp[-1].abstract).last = (yyvsp[-1].abstract).fn.comp; }
			  if ((yyvsp[-1].abstract).fold_flag)
			    {
			      *(yyval.abstract).last = (yyvsp[0].nested).comp;
			      (yyval.abstract).last = (yyvsp[0].nested).last;
			    }
			  else
			    (yyval.abstract).fn = (yyvsp[0].nested);
			}
#line 2746 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 133:
#line 868 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).fn.comp = NULL; (yyval.abstract).fn.last = NULL; (yyval.abstract).fold_flag = 0;
			  if ((yyvsp[-1].abstract).fn.comp) { (yyval.abstract).last = (yyvsp[-1].abstract).fn.last; *(yyvsp[-1].abstract).last = (yyvsp[-1].abstract).fn.comp; }
			  *(yyvsp[-1].abstract).last = (yyvsp[0].comp);
			  (yyval.abstract).last = &d_right ((yyvsp[0].comp));
			}
#line 2756 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 134:
#line 874 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).fn.comp = NULL; (yyval.abstract).fn.last = NULL; (yyval.abstract).fold_flag = 0;
			  (yyval.abstract).comp = (yyvsp[0].comp);
			  (yyval.abstract).last = &d_right ((yyvsp[0].comp));
			}
#line 2765 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 135:
#line 892 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).comp = (yyvsp[0].nested).comp; (yyval.abstract).last = (yyvsp[0].nested).last;
			  (yyval.abstract).fn.comp = NULL; (yyval.abstract).fn.last = NULL; (yyval.abstract).start = NULL; }
#line 2772 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 136:
#line 895 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract) = (yyvsp[0].abstract);
			  if ((yyvsp[0].abstract).last)
			    *(yyval.abstract).last = (yyvsp[-1].nested).comp;
			  else
			    (yyval.abstract).comp = (yyvsp[-1].nested).comp;
			  (yyval.abstract).last = (yyvsp[-1].nested).last;
			}
#line 2784 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 137:
#line 903 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).comp = (yyvsp[0].abstract).comp; (yyval.abstract).last = (yyvsp[0].abstract).last; (yyval.abstract).fn = (yyvsp[0].abstract).fn; (yyval.abstract).start = NULL; }
#line 2790 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 138:
#line 905 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).start = (yyvsp[0].comp);
			  if ((yyvsp[-3].abstract).fn.comp) { (yyval.abstract).last = (yyvsp[-3].abstract).fn.last; *(yyvsp[-3].abstract).last = (yyvsp[-3].abstract).fn.comp; }
			  if ((yyvsp[-3].abstract).fold_flag)
			    {
			      *(yyval.abstract).last = (yyvsp[-2].nested).comp;
			      (yyval.abstract).last = (yyvsp[-2].nested).last;
			    }
			  else
			    (yyval.abstract).fn = (yyvsp[-2].nested);
			}
#line 2805 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 139:
#line 916 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.abstract).fn = (yyvsp[-1].nested);
			  (yyval.abstract).start = (yyvsp[0].comp);
			  (yyval.abstract).comp = NULL; (yyval.abstract).last = NULL;
			}
#line 2814 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 141:
#line 924 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[0].abstract).comp;
			  *(yyvsp[0].abstract).last = (yyvsp[-1].comp);
			}
#line 2822 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 142:
#line 930 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[0].nested).comp;
			  (yyval.nested).last = (yyvsp[-1].nested).last;
			  *(yyvsp[0].nested).last = (yyvsp[-1].nested).comp; }
#line 2830 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 144:
#line 938 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested) = (yyvsp[-1].nested); }
#line 2836 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 145:
#line 940 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-1].nested).comp;
			  *(yyvsp[-1].nested).last = (yyvsp[0].nested).comp;
			  (yyval.nested).last = (yyvsp[0].nested).last;
			}
#line 2845 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 146:
#line 945 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-1].nested).comp;
			  *(yyvsp[-1].nested).last = (yyvsp[0].comp);
			  (yyval.nested).last = &d_right ((yyvsp[0].comp));
			}
#line 2854 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 147:
#line 950 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = make_empty (DEMANGLE_COMPONENT_TYPED_NAME);
			  d_left ((yyval.nested).comp) = (yyvsp[0].comp);
			  (yyval.nested).last = &d_right ((yyval.nested).comp);
			}
#line 2863 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 148:
#line 963 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[0].nested).comp;
			  (yyval.nested).last = (yyvsp[-1].nested).last;
			  *(yyvsp[0].nested).last = (yyvsp[-1].nested).comp; }
#line 2871 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 149:
#line 967 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = make_empty (DEMANGLE_COMPONENT_TYPED_NAME);
			  d_left ((yyval.nested).comp) = (yyvsp[0].comp);
			  (yyval.nested).last = &d_right ((yyval.nested).comp);
			}
#line 2880 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 151:
#line 980 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, (yyvsp[-3].comp), (yyvsp[-2].nested).comp);
			  (yyval.nested).last = (yyvsp[-2].nested).last;
			  (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, (yyval.nested).comp, (yyvsp[0].comp));
			}
#line 2889 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 152:
#line 985 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-3].nested).comp;
			  *(yyvsp[-3].nested).last = (yyvsp[-2].nested).comp;
			  (yyval.nested).last = (yyvsp[-2].nested).last;
			  (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, (yyval.nested).comp, (yyvsp[0].comp));
			}
#line 2899 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 153:
#line 994 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-1].nested).comp;
			  (yyval.nested).last = (yyvsp[-2].nested).last;
			  *(yyvsp[-1].nested).last = (yyvsp[-2].nested).comp; }
#line 2907 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 154:
#line 998 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-1].nested).comp;
			  *(yyvsp[-1].nested).last = (yyvsp[0].nested).comp;
			  (yyval.nested).last = (yyvsp[0].nested).last;
			}
#line 2916 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 155:
#line 1003 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = (yyvsp[-1].nested).comp;
			  *(yyvsp[-1].nested).last = (yyvsp[0].comp);
			  (yyval.nested).last = &d_right ((yyvsp[0].comp));
			}
#line 2925 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 156:
#line 1008 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, (yyvsp[-1].comp), (yyvsp[0].nested).comp);
			  (yyval.nested).last = (yyvsp[0].nested).last;
			}
#line 2933 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 157:
#line 1012 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.nested).comp = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, (yyvsp[-1].comp), (yyvsp[0].comp));
			  (yyval.nested).last = &d_right ((yyvsp[0].comp));
			}
#line 2941 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 158:
#line 1018 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = (yyvsp[-1].comp); }
#line 2947 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 160:
#line 1027 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary (">", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 2953 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 161:
#line 1034 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator ("&", 1), (yyvsp[0].comp)); }
#line 2959 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 162:
#line 1036 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator ("&", 1), (yyvsp[-1].comp)); }
#line 2965 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 163:
#line 1041 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_unary ("-", (yyvsp[0].comp)); }
#line 2971 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 164:
#line 1045 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_unary ("!", (yyvsp[0].comp)); }
#line 2977 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 165:
#line 1049 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_unary ("~", (yyvsp[0].comp)); }
#line 2983 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 166:
#line 1056 "cp-name-parser.y" /* yacc.c:1646  */
    { if ((yyvsp[0].comp)->type == DEMANGLE_COMPONENT_LITERAL
		      || (yyvsp[0].comp)->type == DEMANGLE_COMPONENT_LITERAL_NEG)
		    {
		      (yyval.comp) = (yyvsp[0].comp);
		      d_left ((yyvsp[0].comp)) = (yyvsp[-2].comp);
		    }
		  else
		    (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_UNARY,
				      fill_comp (DEMANGLE_COMPONENT_CAST, (yyvsp[-2].comp), NULL),
				      (yyvsp[0].comp));
		}
#line 2999 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 167:
#line 1072 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_UNARY,
				    fill_comp (DEMANGLE_COMPONENT_CAST, (yyvsp[-4].comp), NULL),
				    (yyvsp[-1].comp));
		}
#line 3008 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 168:
#line 1079 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_UNARY,
				    fill_comp (DEMANGLE_COMPONENT_CAST, (yyvsp[-4].comp), NULL),
				    (yyvsp[-1].comp));
		}
#line 3017 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 169:
#line 1086 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_UNARY,
				    fill_comp (DEMANGLE_COMPONENT_CAST, (yyvsp[-4].comp), NULL),
				    (yyvsp[-1].comp));
		}
#line 3026 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 170:
#line 1105 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("*", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3032 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 171:
#line 1109 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("/", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3038 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 172:
#line 1113 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("%", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3044 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 173:
#line 1117 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("+", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3050 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 174:
#line 1121 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("-", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3056 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 175:
#line 1125 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("<<", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3062 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 176:
#line 1129 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary (">>", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3068 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 177:
#line 1133 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("==", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3074 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 178:
#line 1137 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("!=", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3080 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 179:
#line 1141 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("<=", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3086 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 180:
#line 1145 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary (">=", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3092 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 181:
#line 1149 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("<", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3098 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 182:
#line 1153 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("&", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3104 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 183:
#line 1157 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("^", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3110 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 184:
#line 1161 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("|", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3116 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 185:
#line 1165 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("&&", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3122 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 186:
#line 1169 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("||", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3128 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 187:
#line 1174 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary ("->", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3134 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 188:
#line 1178 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = d_binary (".", (yyvsp[-2].comp), (yyvsp[0].comp)); }
#line 3140 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 189:
#line 1182 "cp-name-parser.y" /* yacc.c:1646  */
    { (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_TRINARY, make_operator ("?", 3),
				    fill_comp (DEMANGLE_COMPONENT_TRINARY_ARG1, (yyvsp[-4].comp),
						 fill_comp (DEMANGLE_COMPONENT_TRINARY_ARG2, (yyvsp[-2].comp), (yyvsp[0].comp))));
		}
#line 3149 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 192:
#line 1196 "cp-name-parser.y" /* yacc.c:1646  */
    {
		  /* Match the whitespacing of cplus_demangle_operators.
		     It would abort on unrecognized string otherwise.  */
		  (yyval.comp) = d_unary ("sizeof ", (yyvsp[-1].comp));
		}
#line 3159 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 193:
#line 1205 "cp-name-parser.y" /* yacc.c:1646  */
    { struct demangle_component *i;
		  i = make_name ("1", 1);
		  (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_LITERAL,
				    make_builtin_type ("bool"),
				    i);
		}
#line 3170 "cp-name-parser.c" /* yacc.c:1646  */
    break;

  case 194:
#line 1214 "cp-name-parser.y" /* yacc.c:1646  */
    { struct demangle_component *i;
		  i = make_name ("0", 1);
		  (yyval.comp) = fill_comp (DEMANGLE_COMPONENT_LITERAL,
				    make_builtin_type ("bool"),
				    i);
		}
#line 3181 "cp-name-parser.c" /* yacc.c:1646  */
    break;


#line 3185 "cp-name-parser.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 1224 "cp-name-parser.y" /* yacc.c:1906  */


/* Apply QUALIFIERS to LHS and return a qualified component.  IS_METHOD
   is set if LHS is a method, in which case the qualifiers are logically
   applied to "this".  We apply qualifiers in a consistent order; LHS
   may already be qualified; duplicate qualifiers are not created.  */

struct demangle_component *
d_qualify (struct demangle_component *lhs, int qualifiers, int is_method)
{
  struct demangle_component **inner_p;
  enum demangle_component_type type;

  /* For now the order is CONST (innermost), VOLATILE, RESTRICT.  */

#define HANDLE_QUAL(TYPE, MTYPE, QUAL)				\
  if ((qualifiers & QUAL) && (type != TYPE) && (type != MTYPE))	\
    {								\
      *inner_p = fill_comp (is_method ? MTYPE : TYPE,	\
			      *inner_p, NULL);			\
      inner_p = &d_left (*inner_p);				\
      type = (*inner_p)->type;					\
    }								\
  else if (type == TYPE || type == MTYPE)			\
    {								\
      inner_p = &d_left (*inner_p);				\
      type = (*inner_p)->type;					\
    }

  inner_p = &lhs;

  type = (*inner_p)->type;

  HANDLE_QUAL (DEMANGLE_COMPONENT_RESTRICT, DEMANGLE_COMPONENT_RESTRICT_THIS, QUAL_RESTRICT);
  HANDLE_QUAL (DEMANGLE_COMPONENT_VOLATILE, DEMANGLE_COMPONENT_VOLATILE_THIS, QUAL_VOLATILE);
  HANDLE_QUAL (DEMANGLE_COMPONENT_CONST, DEMANGLE_COMPONENT_CONST_THIS, QUAL_CONST);

  return lhs;
}

/* Return a builtin type corresponding to FLAGS.  */

static struct demangle_component *
d_int_type (int flags)
{
  const char *name;

  switch (flags)
    {
    case INT_SIGNED | INT_CHAR:
      name = "signed char";
      break;
    case INT_CHAR:
      name = "char";
      break;
    case INT_UNSIGNED | INT_CHAR:
      name = "unsigned char";
      break;
    case 0:
    case INT_SIGNED:
      name = "int";
      break;
    case INT_UNSIGNED:
      name = "unsigned int";
      break;
    case INT_LONG:
    case INT_SIGNED | INT_LONG:
      name = "long";
      break;
    case INT_UNSIGNED | INT_LONG:
      name = "unsigned long";
      break;
    case INT_SHORT:
    case INT_SIGNED | INT_SHORT:
      name = "short";
      break;
    case INT_UNSIGNED | INT_SHORT:
      name = "unsigned short";
      break;
    case INT_LLONG | INT_LONG:
    case INT_SIGNED | INT_LLONG | INT_LONG:
      name = "long long";
      break;
    case INT_UNSIGNED | INT_LLONG | INT_LONG:
      name = "unsigned long long";
      break;
    default:
      return NULL;
    }

  return make_builtin_type (name);
}

/* Wrapper to create a unary operation.  */

static struct demangle_component *
d_unary (const char *name, struct demangle_component *lhs)
{
  return fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator (name, 1), lhs);
}

/* Wrapper to create a binary operation.  */

static struct demangle_component *
d_binary (const char *name, struct demangle_component *lhs, struct demangle_component *rhs)
{
  return fill_comp (DEMANGLE_COMPONENT_BINARY, make_operator (name, 2),
		      fill_comp (DEMANGLE_COMPONENT_BINARY_ARGS, lhs, rhs));
}

/* Find the end of a symbol name starting at LEXPTR.  */

static const char *
symbol_end (const char *lexptr)
{
  const char *p = lexptr;

  while (*p && (ISALNUM (*p) || *p == '_' || *p == '$' || *p == '.'))
    p++;

  return p;
}

/* Take care of parsing a number (anything that starts with a digit).
   The number starts at P and contains LEN characters.  Store the result in
   YYLVAL.  */

static int
parse_number (const char *p, int len, int parsed_float)
{
  int unsigned_p = 0;

  /* Number of "L" suffixes encountered.  */
  int long_p = 0;

  struct demangle_component *signed_type;
  struct demangle_component *unsigned_type;
  struct demangle_component *type, *name;
  enum demangle_component_type literal_type;

  if (p[0] == '-')
    {
      literal_type = DEMANGLE_COMPONENT_LITERAL_NEG;
      p++;
      len--;
    }
  else
    literal_type = DEMANGLE_COMPONENT_LITERAL;

  if (parsed_float)
    {
      /* It's a float since it contains a point or an exponent.  */
      char c;

      /* The GDB lexer checks the result of scanf at this point.  Not doing
         this leaves our error checking slightly weaker but only for invalid
         data.  */

      /* See if it has `f' or `l' suffix (float or long double).  */

      c = TOLOWER (p[len - 1]);

      if (c == 'f')
      	{
      	  len--;
      	  type = make_builtin_type ("float");
      	}
      else if (c == 'l')
	{
	  len--;
	  type = make_builtin_type ("long double");
	}
      else if (ISDIGIT (c) || c == '.')
	type = make_builtin_type ("double");
      else
	return ERROR;

      name = make_name (p, len);
      yylval.comp = fill_comp (literal_type, type, name);

      return FLOAT;
    }

  /* This treats 0x1 and 1 as different literals.  We also do not
     automatically generate unsigned types.  */

  long_p = 0;
  unsigned_p = 0;
  while (len > 0)
    {
      if (p[len - 1] == 'l' || p[len - 1] == 'L')
	{
	  len--;
	  long_p++;
	  continue;
	}
      if (p[len - 1] == 'u' || p[len - 1] == 'U')
	{
	  len--;
	  unsigned_p++;
	  continue;
	}
      break;
    }

  if (long_p == 0)
    {
      unsigned_type = make_builtin_type ("unsigned int");
      signed_type = make_builtin_type ("int");
    }
  else if (long_p == 1)
    {
      unsigned_type = make_builtin_type ("unsigned long");
      signed_type = make_builtin_type ("long");
    }
  else
    {
      unsigned_type = make_builtin_type ("unsigned long long");
      signed_type = make_builtin_type ("long long");
    }

   if (unsigned_p)
     type = unsigned_type;
   else
     type = signed_type;

   name = make_name (p, len);
   yylval.comp = fill_comp (literal_type, type, name);

   return INT;
}

static char backslashable[] = "abefnrtv";
static char represented[] = "\a\b\e\f\n\r\t\v";

/* Translate the backslash the way we would in the host character set.  */
static int
c_parse_backslash (int host_char, int *target_char)
{
  const char *ix;
  ix = strchr (backslashable, host_char);
  if (! ix)
    return 0;
  else
    *target_char = represented[ix - backslashable];
  return 1;
}

/* Parse a C escape sequence.  STRING_PTR points to a variable
   containing a pointer to the string to parse.  That pointer
   should point to the character after the \.  That pointer
   is updated past the characters we use.  The value of the
   escape sequence is returned.

   A negative value means the sequence \ newline was seen,
   which is supposed to be equivalent to nothing at all.

   If \ is followed by a null character, we return a negative
   value and leave the string pointer pointing at the null character.

   If \ is followed by 000, we return 0 and leave the string pointer
   after the zeros.  A value of 0 does not mean end of string.  */

static int
cp_parse_escape (const char **string_ptr)
{
  int target_char;
  int c = *(*string_ptr)++;
  if (c_parse_backslash (c, &target_char))
    return target_char;
  else
    switch (c)
      {
      case '\n':
	return -2;
      case 0:
	(*string_ptr)--;
	return 0;
      case '^':
	{
	  c = *(*string_ptr)++;

	  if (c == '?')
	    return 0177;
	  else if (c == '\\')
	    target_char = cp_parse_escape (string_ptr);
	  else
	    target_char = c;

	  /* Now target_char is something like `c', and we want to find
	     its control-character equivalent.  */
	  target_char = target_char & 037;

	  return target_char;
	}

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
	{
	  int i = c - '0';
	  int count = 0;
	  while (++count < 3)
	    {
	      c = (**string_ptr);
	      if (c >= '0' && c <= '7')
		{
		  (*string_ptr)++;
		  i *= 8;
		  i += c - '0';
		}
	      else
		{
		  break;
		}
	    }
	  return i;
	}
      default:
	return c;
      }
}

#define HANDLE_SPECIAL(string, comp)				\
  if (strncmp (tokstart, string, sizeof (string) - 1) == 0)	\
    {								\
      lexptr = tokstart + sizeof (string) - 1;			\
      yylval.lval = comp;					\
      return DEMANGLER_SPECIAL;					\
    }

#define HANDLE_TOKEN2(string, token)			\
  if (lexptr[1] == string[1])				\
    {							\
      lexptr += 2;					\
      yylval.opname = string;				\
      return token;					\
    }      

#define HANDLE_TOKEN3(string, token)			\
  if (lexptr[1] == string[1] && lexptr[2] == string[2])	\
    {							\
      lexptr += 3;					\
      yylval.opname = string;				\
      return token;					\
    }      

/* Read one token, getting characters through LEXPTR.  */

static int
yylex (void)
{
  int c;
  int namelen;
  const char *tokstart;

 retry:
  prev_lexptr = lexptr;
  tokstart = lexptr;

  switch (c = *tokstart)
    {
    case 0:
      return 0;

    case ' ':
    case '\t':
    case '\n':
      lexptr++;
      goto retry;

    case '\'':
      /* We either have a character constant ('0' or '\177' for example)
	 or we have a quoted symbol reference ('foo(int,int)' in C++
	 for example). */
      lexptr++;
      c = *lexptr++;
      if (c == '\\')
	c = cp_parse_escape (&lexptr);
      else if (c == '\'')
	{
	  yyerror (_("empty character constant"));
	  return ERROR;
	}

      c = *lexptr++;
      if (c != '\'')
	{
	  yyerror (_("invalid character constant"));
	  return ERROR;
	}

      /* FIXME: We should refer to a canonical form of the character,
	 presumably the same one that appears in manglings - the decimal
	 representation.  But if that isn't in our input then we have to
	 allocate memory for it somewhere.  */
      yylval.comp = fill_comp (DEMANGLE_COMPONENT_LITERAL,
				 make_builtin_type ("char"),
				 make_name (tokstart, lexptr - tokstart));

      return INT;

    case '(':
      if (strncmp (tokstart, "(anonymous namespace)", 21) == 0)
	{
	  lexptr += 21;
	  yylval.comp = make_name ("(anonymous namespace)",
				     sizeof "(anonymous namespace)" - 1);
	  return NAME;
	}
	/* FALL THROUGH */

    case ')':
    case ',':
      lexptr++;
      return c;

    case '.':
      if (lexptr[1] == '.' && lexptr[2] == '.')
	{
	  lexptr += 3;
	  return ELLIPSIS;
	}

      /* Might be a floating point number.  */
      if (lexptr[1] < '0' || lexptr[1] > '9')
	goto symbol;		/* Nope, must be a symbol. */

      goto try_number;

    case '-':
      HANDLE_TOKEN2 ("-=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("--", DECREMENT);
      HANDLE_TOKEN2 ("->", ARROW);

      /* For construction vtables.  This is kind of hokey.  */
      if (strncmp (tokstart, "-in-", 4) == 0)
	{
	  lexptr += 4;
	  return CONSTRUCTION_IN;
	}

      if (lexptr[1] < '0' || lexptr[1] > '9')
	{
	  lexptr++;
	  return '-';
	}
      /* FALL THRU into number case.  */

    try_number:
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      {
	/* It's a number.  */
	int got_dot = 0, got_e = 0, toktype;
	const char *p = tokstart;
	int hex = 0;

	if (c == '-')
	  p++;

	if (c == '0' && (p[1] == 'x' || p[1] == 'X'))
	  {
	    p += 2;
	    hex = 1;
	  }
	else if (c == '0' && (p[1]=='t' || p[1]=='T' || p[1]=='d' || p[1]=='D'))
	  {
	    p += 2;
	    hex = 0;
	  }

	for (;; ++p)
	  {
	    /* This test includes !hex because 'e' is a valid hex digit
	       and thus does not indicate a floating point number when
	       the radix is hex.  */
	    if (!hex && !got_e && (*p == 'e' || *p == 'E'))
	      got_dot = got_e = 1;
	    /* This test does not include !hex, because a '.' always indicates
	       a decimal floating point number regardless of the radix.

	       NOTE drow/2005-03-09: This comment is not accurate in C99;
	       however, it's not clear that all the floating point support
	       in this file is doing any good here.  */
	    else if (!got_dot && *p == '.')
	      got_dot = 1;
	    else if (got_e && (p[-1] == 'e' || p[-1] == 'E')
		     && (*p == '-' || *p == '+'))
	      /* This is the sign of the exponent, not the end of the
		 number.  */
	      continue;
	    /* We will take any letters or digits.  parse_number will
	       complain if past the radix, or if L or U are not final.  */
	    else if (! ISALNUM (*p))
	      break;
	  }
	toktype = parse_number (tokstart, p - tokstart, got_dot|got_e);
        if (toktype == ERROR)
	  {
	    char *err_copy = (char *) alloca (p - tokstart + 1);

	    memcpy (err_copy, tokstart, p - tokstart);
	    err_copy[p - tokstart] = 0;
	    yyerror (_("invalid number"));
	    return ERROR;
	  }
	lexptr = p;
	return toktype;
      }

    case '+':
      HANDLE_TOKEN2 ("+=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("++", INCREMENT);
      lexptr++;
      return c;
    case '*':
      HANDLE_TOKEN2 ("*=", ASSIGN_MODIFY);
      lexptr++;
      return c;
    case '/':
      HANDLE_TOKEN2 ("/=", ASSIGN_MODIFY);
      lexptr++;
      return c;
    case '%':
      HANDLE_TOKEN2 ("%=", ASSIGN_MODIFY);
      lexptr++;
      return c;
    case '|':
      HANDLE_TOKEN2 ("|=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("||", OROR);
      lexptr++;
      return c;
    case '&':
      HANDLE_TOKEN2 ("&=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("&&", ANDAND);
      lexptr++;
      return c;
    case '^':
      HANDLE_TOKEN2 ("^=", ASSIGN_MODIFY);
      lexptr++;
      return c;
    case '!':
      HANDLE_TOKEN2 ("!=", NOTEQUAL);
      lexptr++;
      return c;
    case '<':
      HANDLE_TOKEN3 ("<<=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("<=", LEQ);
      HANDLE_TOKEN2 ("<<", LSH);
      lexptr++;
      return c;
    case '>':
      HANDLE_TOKEN3 (">>=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 (">=", GEQ);
      HANDLE_TOKEN2 (">>", RSH);
      lexptr++;
      return c;
    case '=':
      HANDLE_TOKEN2 ("==", EQUAL);
      lexptr++;
      return c;
    case ':':
      HANDLE_TOKEN2 ("::", COLONCOLON);
      lexptr++;
      return c;

    case '[':
    case ']':
    case '?':
    case '@':
    case '~':
    case '{':
    case '}':
    symbol:
      lexptr++;
      return c;

    case '"':
      /* These can't occur in C++ names.  */
      yyerror (_("unexpected string literal"));
      return ERROR;
    }

  if (!(c == '_' || c == '$' || ISALPHA (c)))
    {
      /* We must have come across a bad character (e.g. ';').  */
      yyerror (_("invalid character"));
      return ERROR;
    }

  /* It's a name.  See how long it is.  */
  namelen = 0;
  do
    c = tokstart[++namelen];
  while (ISALNUM (c) || c == '_' || c == '$');

  lexptr += namelen;

  /* Catch specific keywords.  Notice that some of the keywords contain
     spaces, and are sorted by the length of the first word.  They must
     all include a trailing space in the string comparison.  */
  switch (namelen)
    {
    case 16:
      if (strncmp (tokstart, "reinterpret_cast", 16) == 0)
        return REINTERPRET_CAST;
      break;
    case 12:
      if (strncmp (tokstart, "construction vtable for ", 24) == 0)
	{
	  lexptr = tokstart + 24;
	  return CONSTRUCTION_VTABLE;
	}
      if (strncmp (tokstart, "dynamic_cast", 12) == 0)
        return DYNAMIC_CAST;
      break;
    case 11:
      if (strncmp (tokstart, "static_cast", 11) == 0)
        return STATIC_CAST;
      break;
    case 9:
      HANDLE_SPECIAL ("covariant return thunk to ", DEMANGLE_COMPONENT_COVARIANT_THUNK);
      HANDLE_SPECIAL ("reference temporary for ", DEMANGLE_COMPONENT_REFTEMP);
      break;
    case 8:
      HANDLE_SPECIAL ("typeinfo for ", DEMANGLE_COMPONENT_TYPEINFO);
      HANDLE_SPECIAL ("typeinfo fn for ", DEMANGLE_COMPONENT_TYPEINFO_FN);
      HANDLE_SPECIAL ("typeinfo name for ", DEMANGLE_COMPONENT_TYPEINFO_NAME);
      if (strncmp (tokstart, "operator", 8) == 0)
	return OPERATOR;
      if (strncmp (tokstart, "restrict", 8) == 0)
	return RESTRICT;
      if (strncmp (tokstart, "unsigned", 8) == 0)
	return UNSIGNED;
      if (strncmp (tokstart, "template", 8) == 0)
	return TEMPLATE;
      if (strncmp (tokstart, "volatile", 8) == 0)
	return VOLATILE_KEYWORD;
      break;
    case 7:
      HANDLE_SPECIAL ("virtual thunk to ", DEMANGLE_COMPONENT_VIRTUAL_THUNK);
      if (strncmp (tokstart, "wchar_t", 7) == 0)
	return WCHAR_T;
      break;
    case 6:
      if (strncmp (tokstart, "global constructors keyed to ", 29) == 0)
	{
	  const char *p;
	  lexptr = tokstart + 29;
	  yylval.lval = DEMANGLE_COMPONENT_GLOBAL_CONSTRUCTORS;
	  /* Find the end of the symbol.  */
	  p = symbol_end (lexptr);
	  yylval.comp = make_name (lexptr, p - lexptr);
	  lexptr = p;
	  return DEMANGLER_SPECIAL;
	}
      if (strncmp (tokstart, "global destructors keyed to ", 28) == 0)
	{
	  const char *p;
	  lexptr = tokstart + 28;
	  yylval.lval = DEMANGLE_COMPONENT_GLOBAL_DESTRUCTORS;
	  /* Find the end of the symbol.  */
	  p = symbol_end (lexptr);
	  yylval.comp = make_name (lexptr, p - lexptr);
	  lexptr = p;
	  return DEMANGLER_SPECIAL;
	}

      HANDLE_SPECIAL ("vtable for ", DEMANGLE_COMPONENT_VTABLE);
      if (strncmp (tokstart, "delete", 6) == 0)
	return DELETE;
      if (strncmp (tokstart, "struct", 6) == 0)
	return STRUCT;
      if (strncmp (tokstart, "signed", 6) == 0)
	return SIGNED_KEYWORD;
      if (strncmp (tokstart, "sizeof", 6) == 0)
	return SIZEOF;
      if (strncmp (tokstart, "double", 6) == 0)
	return DOUBLE_KEYWORD;
      break;
    case 5:
      HANDLE_SPECIAL ("guard variable for ", DEMANGLE_COMPONENT_GUARD);
      if (strncmp (tokstart, "false", 5) == 0)
	return FALSEKEYWORD;
      if (strncmp (tokstart, "class", 5) == 0)
	return CLASS;
      if (strncmp (tokstart, "union", 5) == 0)
	return UNION;
      if (strncmp (tokstart, "float", 5) == 0)
	return FLOAT_KEYWORD;
      if (strncmp (tokstart, "short", 5) == 0)
	return SHORT;
      if (strncmp (tokstart, "const", 5) == 0)
	return CONST_KEYWORD;
      break;
    case 4:
      if (strncmp (tokstart, "void", 4) == 0)
	return VOID;
      if (strncmp (tokstart, "bool", 4) == 0)
	return BOOL;
      if (strncmp (tokstart, "char", 4) == 0)
	return CHAR;
      if (strncmp (tokstart, "enum", 4) == 0)
	return ENUM;
      if (strncmp (tokstart, "long", 4) == 0)
	return LONG;
      if (strncmp (tokstart, "true", 4) == 0)
	return TRUEKEYWORD;
      break;
    case 3:
      HANDLE_SPECIAL ("VTT for ", DEMANGLE_COMPONENT_VTT);
      HANDLE_SPECIAL ("non-virtual thunk to ", DEMANGLE_COMPONENT_THUNK);
      if (strncmp (tokstart, "new", 3) == 0)
	return NEW;
      if (strncmp (tokstart, "int", 3) == 0)
	return INT_KEYWORD;
      break;
    default:
      break;
    }

  yylval.comp = make_name (tokstart, namelen);
  return NAME;
}

static void
yyerror (char *msg)
{
  if (global_errmsg)
    return;

  error_lexptr = prev_lexptr;
  global_errmsg = msg ? msg : "parse error";
}

/* Allocate a chunk of the components we'll need to build a tree.  We
   generally allocate too many components, but the extra memory usage
   doesn't hurt because the trees are temporary and the storage is
   reused.  More may be allocated later, by d_grab.  */
static struct demangle_info *
allocate_info (void)
{
  struct demangle_info *info = XNEW (struct demangle_info);

  info->next = NULL;
  info->used = 0;
  return info;
}

/* Convert RESULT to a string.  The return value is allocated
   using xmalloc.  ESTIMATED_LEN is used only as a guide to the
   length of the result.  This functions handles a few cases that
   cplus_demangle_print does not, specifically the global destructor
   and constructor labels.  */

char *
cp_comp_to_string (struct demangle_component *result, int estimated_len)
{
  size_t err;

  return cplus_demangle_print (DMGL_PARAMS | DMGL_ANSI, result, estimated_len,
			       &err);
}

/* A convenience function to allocate and initialize a new struct
   demangled_parse_info.  */

struct demangle_parse_info *
cp_new_demangle_parse_info (void)
{
  struct demangle_parse_info *info;

  info = XNEW (struct demangle_parse_info);
  info->info = NULL;
  info->tree = NULL;
  obstack_init (&info->obstack);

  return info;
}

/* Free any memory associated with the given PARSE_INFO.  */

void
cp_demangled_name_parse_free (struct demangle_parse_info *parse_info)
{
  struct demangle_info *info = parse_info->info;

  /* Free any allocated chunks of memory for the parse.  */
  while (info != NULL)
    {
      struct demangle_info *next = info->next;

      xfree (info);
      info = next;
    }

  /* Free any memory allocated during typedef replacement.  */
  obstack_free (&parse_info->obstack, NULL);

  /* Free the parser info.  */
  xfree (parse_info);
}

/* Merge the two parse trees given by DEST and SRC.  The parse tree
   in SRC is attached to DEST at the node represented by TARGET.
   SRC is then freed.

   NOTE 1: Since there is no API to merge obstacks, this function does
   even attempt to try it.  Fortunately, we do not (yet?) need this ability.
   The code will assert if SRC->obstack is not empty.

   NOTE 2: The string from which SRC was parsed must not be freed, since
   this function will place pointers to that string into DEST.  */

void
cp_merge_demangle_parse_infos (struct demangle_parse_info *dest,
			       struct demangle_component *target,
			       struct demangle_parse_info *src)

{
  struct demangle_info *di;

  /* Copy the SRC's parse data into DEST.  */
  *target = *src->tree;
  di = dest->info;
  while (di->next != NULL)
    di = di->next;
  di->next = src->info;

  /* Clear the (pointer to) SRC's parse data so that it is not freed when
     cp_demangled_parse_info_free is called.  */
  src->info = NULL;

  /* Free SRC.  */
  cp_demangled_name_parse_free (src);
}

/* Convert a demangled name to a demangle_component tree.  On success,
   a structure containing the root of the new tree is returned; it must
   be freed by calling cp_demangled_name_parse_free. On error, NULL is
   returned, and an error message will be set in *ERRMSG (which does
   not need to be freed).  */

struct demangle_parse_info *
cp_demangled_name_to_comp (const char *demangled_name, const char **errmsg)
{
  static char errbuf[60];
  struct demangle_parse_info *result;

  prev_lexptr = lexptr = demangled_name;
  error_lexptr = NULL;
  global_errmsg = NULL;

  demangle_info = allocate_info ();

  result = cp_new_demangle_parse_info ();
  result->info = demangle_info;

  if (yyparse ())
    {
      if (global_errmsg && errmsg)
	{
	  snprintf (errbuf, sizeof (errbuf) - 2, "%s, near `%s",
		    global_errmsg, error_lexptr);
	  strcat (errbuf, "'");
	  *errmsg = errbuf;
	}
      cp_demangled_name_parse_free (result);
      return NULL;
    }

  result->tree = global_result;
  global_result = NULL;

  return result;
}

#ifdef TEST_CPNAMES

static void
cp_print (struct demangle_component *result)
{
  char *str;
  size_t err = 0;

  str = cplus_demangle_print (DMGL_PARAMS | DMGL_ANSI, result, 64, &err);
  if (str == NULL)
    return;

  fputs (str, stdout);

  xfree (str);
}

static char
trim_chars (char *lexptr, char **extra_chars)
{
  char *p = (char *) symbol_end (lexptr);
  char c = 0;

  if (*p)
    {
      c = *p;
      *p = 0;
      *extra_chars = p + 1;
    }

  return c;
}

/* When this file is built as a standalone program, xmalloc comes from
   libiberty --- in which case we have to provide xfree ourselves.  */

void
xfree (void *ptr)
{
  if (ptr != NULL)
    {
      /* Literal `free' would get translated back to xfree again.  */
      CONCAT2 (fr,ee) (ptr);
    }
}

/* GDB normally defines internal_error itself, but when this file is built
   as a standalone program, we must also provide an implementation.  */

void
internal_error (const char *file, int line, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  fprintf (stderr, "%s:%d: internal error: ", file, line);
  vfprintf (stderr, fmt, ap);
  exit (1);
}

int
main (int argc, char **argv)
{
  char *str2, *extra_chars = "", c;
  char buf[65536];
  int arg;
  const char *errmsg;
  struct demangle_parse_info *result;

  arg = 1;
  if (argv[arg] && strcmp (argv[arg], "--debug") == 0)
    {
      yydebug = 1;
      arg++;
    }

  if (argv[arg] == NULL)
    while (fgets (buf, 65536, stdin) != NULL)
      {
	int len;
	buf[strlen (buf) - 1] = 0;
	/* Use DMGL_VERBOSE to get expanded standard substitutions.  */
	c = trim_chars (buf, &extra_chars);
	str2 = cplus_demangle (buf, DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE);
	if (str2 == NULL)
	  {
	    printf ("Demangling error\n");
	    if (c)
	      printf ("%s%c%s\n", buf, c, extra_chars);
	    else
	      printf ("%s\n", buf);
	    continue;
	  }
	result = cp_demangled_name_to_comp (str2, &errmsg);
	if (result == NULL)
	  {
	    fputs (errmsg, stderr);
	    fputc ('\n', stderr);
	    continue;
	  }

	cp_print (result->tree);
	cp_demangled_name_parse_free (result);

	xfree (str2);
	if (c)
	  {
	    putchar (c);
	    fputs (extra_chars, stdout);
	  }
	putchar ('\n');
      }
  else
    {
      result = cp_demangled_name_to_comp (argv[arg], &errmsg);
      if (result == NULL)
	{
	  fputs (errmsg, stderr);
	  fputc ('\n', stderr);
	  return 0;
	}
      cp_print (result->tree);
      cp_demangled_name_parse_free (result);
      putchar ('\n');
    }
  return 0;
}

#endif
