/*	$Id: c-parse.h,v 1.2.2.2 1993/08/02 17:33:33 mycroft Exp $ */

typedef union {long itype; tree ttype; enum tree_code code;
	char *filename; int lineno; } YYSTYPE;
#define	IDENTIFIER	258
#define	TYPENAME	259
#define	SCSPEC	260
#define	TYPESPEC	261
#define	TYPE_QUAL	262
#define	CONSTANT	263
#define	STRING	264
#define	ELLIPSIS	265
#define	SIZEOF	266
#define	ENUM	267
#define	STRUCT	268
#define	UNION	269
#define	IF	270
#define	ELSE	271
#define	WHILE	272
#define	DO	273
#define	FOR	274
#define	SWITCH	275
#define	CASE	276
#define	DEFAULT	277
#define	BREAK	278
#define	CONTINUE	279
#define	RETURN	280
#define	GOTO	281
#define	ASM_KEYWORD	282
#define	TYPEOF	283
#define	ALIGNOF	284
#define	ALIGN	285
#define	ATTRIBUTE	286
#define	EXTENSION	287
#define	LABEL	288
#define	REALPART	289
#define	IMAGPART	290
#define	ASSIGN	291
#define	OROR	292
#define	ANDAND	293
#define	EQCOMPARE	294
#define	ARITHCOMPARE	295
#define	LSHIFT	296
#define	RSHIFT	297
#define	UNARY	298
#define	PLUSPLUS	299
#define	MINUSMINUS	300
#define	HYPERUNARY	301
#define	POINTSAT	302
#define	INTERFACE	303
#define	IMPLEMENTATION	304
#define	END	305
#define	SELECTOR	306
#define	DEFS	307
#define	ENCODE	308
#define	CLASSNAME	309
#define	PUBLIC	310
#define	PRIVATE	311
#define	PROTECTED	312
#define	PROTOCOL	313
#define	OBJECTNAME	314
#define	CLASS	315
#define	ALIAS	316
#define	OBJC_STRING	317


extern YYSTYPE yylval;
