/* A Bison parser, made by GNU Bison 2.7.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2012 Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.7"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
/* Line 371 of yacc.c  */
#line 1 "tcpkali_expr_y.y"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "tcpkali_websocket.h"
#include "tcpkali_expr.h"

int yylex(void);
int yyerror(tk_expr_t **, const char *);

#define YYPARSE_PARAM   param
#define YYERROR_VERBOSE


/* Line 371 of yacc.c  */
#line 86 "tcpkali_expr_y.c"

# ifndef YY_NULL
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULL nullptr
#  else
#   define YY_NULL 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_TCPKALI_EXPR_Y_H_INCLUDED
# define YY_YY_TCPKALI_EXPR_Y_H_INCLUDED
/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     END = 0,
     TOK_ws = 258,
     TOK_raw = 259,
     TOK_ws_opcode = 260,
     TOK_ws_reserved_flag = 261,
     TOK_global = 262,
     TOK_connection = 263,
     TOK_ptr = 264,
     TOK_uid = 265,
     TOK_regex = 266,
     TOK_ellipsis = 267,
     string_token = 268,
     class_range_token = 269,
     repeat_range_token = 270,
     quoted_string = 271,
     filename = 272,
     integer = 273
   };
#endif
/* Tokens.  */
#define END 0
#define TOK_ws 258
#define TOK_raw 259
#define TOK_ws_opcode 260
#define TOK_ws_reserved_flag 261
#define TOK_global 262
#define TOK_connection 263
#define TOK_ptr 264
#define TOK_uid 265
#define TOK_regex 266
#define TOK_ellipsis 267
#define string_token 268
#define class_range_token 269
#define repeat_range_token 270
#define quoted_string 271
#define filename 272
#define integer 273



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
/* Line 387 of yacc.c  */
#line 21 "tcpkali_expr_y.y"

    tk_expr_t   *tv_expr;
    tregex      *tv_regex;
    long         tv_long;
    struct {
        char  *buf;
        size_t len;
    } tv_string;
    struct {
        unsigned char from;
        unsigned char to;
    } tv_class_range;
    struct {
        unsigned char from;
        unsigned char to;
    } tv_repeat_range;
    enum ws_frame_opcode tv_opcode;
    char  tv_char;


/* Line 387 of yacc.c  */
#line 188 "tcpkali_expr_y.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (tk_expr_t **param);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */

#endif /* !YY_YY_TCPKALI_EXPR_Y_H_INCLUDED  */

/* Copy the second part of user declarations.  */

/* Line 390 of yacc.c  */
#line 216 "tcpkali_expr_y.c"

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
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
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
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(N) (N)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
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
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

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
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  21
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   78

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  37
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  20
/* YYNRULES -- Number of rules.  */
#define YYNRULES  48
/* YYNRULES -- Number of states.  */
#define YYNSTATES  78

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   276

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    26,     2,     2,
      35,    36,    31,    30,    32,     2,    25,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      27,     2,    28,    29,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    33,     2,    34,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    23,    19,    24,     2,     2,     2,     2,
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
      15,    16,    17,    18,    20,    21,    22
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     5,     8,    10,    13,    15,    18,    20,
      24,    28,    30,    32,    34,    37,    42,    47,    52,    55,
      59,    63,    67,    69,    72,    75,    77,    80,    84,    86,
      88,    92,    94,    96,   100,   102,   105,   107,   110,   113,
     116,   121,   128,   130,   134,   138,   140,   143,   145
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      38,     0,    -1,     0,    -1,    39,     0,    -1,    41,    -1,
      41,    39,    -1,    13,    -1,    40,    13,    -1,    40,    -1,
      23,    42,    24,    -1,    23,    43,    24,    -1,    45,    -1,
      49,    -1,    44,    -1,     4,    48,    -1,     4,    23,    43,
      24,    -1,     7,    25,    11,    50,    -1,     8,    25,    11,
      50,    -1,    11,    50,    -1,    44,    26,    18,    -1,     8,
      25,     9,    -1,     8,    25,    10,    -1,    46,    -1,    45,
      12,    -1,    45,     6,    -1,    47,    -1,    46,    48,    -1,
       3,    25,     5,    -1,    16,    -1,    49,    -1,    27,    17,
      28,    -1,    51,    -1,    52,    -1,    51,    19,    52,    -1,
      53,    -1,    52,    53,    -1,    54,    -1,    54,    29,    -1,
      54,    30,    -1,    54,    31,    -1,    54,    23,    18,    24,
      -1,    54,    23,    18,    32,    18,    24,    -1,    13,    -1,
      33,    55,    34,    -1,    35,    50,    36,    -1,    56,    -1,
      55,    56,    -1,    40,    -1,    14,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    73,    73,    79,    85,    88,    93,    94,   105,   114,
     117,   121,   124,   132,   136,   147,   154,   165,   173,   183,
     190,   195,   202,   204,   209,   215,   217,   233,   241,   241,
     244,   264,   267,   270,   275,   276,   281,   282,   283,   284,
     285,   286,   289,   292,   295,   300,   301,   306,   309
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of expression\"", "error", "$undefined", "\"ws\"", "\"raw\"",
  "\"text, binary, close, ping, pong, continuation\"",
  "\"rsv1, rsv2, rsv3\"", "\"global\"", "\"connection\"", "\" ptr\"",
  "\"uid\"", "\"re\"", "\"...\"", "\"arbitrary string\"",
  "\"regex character class range\"", "\"regex repeat spec\"",
  "\"quoted string\"", "\"file name\"", "integer", "'|'",
  "\"some string or \\\\{expression}\"", "\"data and expressions\"",
  "\"connection, global, re, or <filename.ext>\"", "'{'", "'}'", "'.'",
  "'%'", "'<'", "'>'", "'?'", "'+'", "'*'", "','", "'['", "']'", "'('",
  "')'", "$accept", "Grammar", "ByteSequencesAndExpressions", "String",
  "ByteSequenceOrExpr", "WSExpression", "NonWSExpression", "NumericExpr",
  "WSFrameFinalized", "WSFrameWithData", "WSBasicFrame", "FileOrQuoted",
  "File", "CompleteRegex", "RegexAlternatives", "RegexSequence",
  "RepeatedRegex", "RegexPiece", "RegexClasses", "RegexClass", YY_NULL
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   124,
     274,   275,   276,   123,   125,    46,    37,    60,    62,    63,
      43,    42,    44,    91,    93,    40,    41
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    37,    38,    38,    39,    39,    40,    40,    41,    41,
      41,    42,    43,    43,    43,    43,    43,    43,    43,    44,
      44,    44,    45,    45,    45,    46,    46,    47,    48,    48,
      49,    50,    51,    51,    52,    52,    53,    53,    53,    53,
      53,    53,    54,    54,    54,    55,    55,    56,    56
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     1,     2,     1,     2,     1,     3,
       3,     1,     1,     1,     2,     4,     4,     4,     2,     3,
       3,     3,     1,     2,     2,     1,     2,     3,     1,     1,
       3,     1,     1,     3,     1,     2,     1,     2,     2,     2,
       4,     6,     1,     3,     3,     1,     2,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     2,     6,     0,     0,     0,     8,     4,     0,     0,
       0,     0,     0,     0,     0,     0,    13,    11,    22,    25,
      12,     1,     3,     7,     5,     0,    28,     0,    14,    29,
       0,     0,    42,     0,     0,    18,    31,    32,    34,    36,
       0,     9,    10,     0,    24,    23,    26,    27,     0,     0,
      20,    21,     0,    48,    47,     0,    45,     0,     0,    35,
       0,    37,    38,    39,    30,    19,    15,    16,    17,    43,
      46,    44,    33,     0,    40,     0,     0,    41
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     4,     5,     6,     7,    14,    15,    16,    17,    18,
      19,    28,    20,    35,    36,    37,    38,    39,    55,    56
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -38
static const yytype_int8 yypact[] =
{
       3,   -38,   -38,    -2,    13,    20,    28,     1,    27,     6,
      29,    30,    -3,    36,    32,    33,    35,    34,    15,   -38,
     -38,   -38,   -38,   -38,   -38,    53,   -38,     0,   -38,   -38,
      48,    38,   -38,    37,    -3,   -38,    41,    -3,   -38,    14,
      39,   -38,   -38,    44,   -38,   -38,   -38,   -38,    40,    -3,
     -38,   -38,    -3,   -38,    28,     4,   -38,    42,    -3,   -38,
      45,   -38,   -38,   -38,   -38,   -38,   -38,   -38,   -38,   -38,
     -38,   -38,    -3,    -9,   -38,    47,    46,   -38
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -38,   -38,    59,   -21,   -38,   -38,    49,   -38,   -38,   -38,
     -38,    50,    10,   -13,   -38,    11,   -37,   -38,   -38,    16
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      59,     8,     9,     1,     9,    10,    11,    10,    11,    12,
      32,    12,    54,    21,     2,    74,     2,     2,    53,    29,
      22,    57,    26,    75,     3,    13,     3,    13,    29,    27,
      33,    26,    34,    13,    54,    59,    67,    60,    69,    68,
      44,    23,    13,    61,    62,    63,    45,    50,    51,    52,
       2,    53,    25,    40,    30,    31,    41,    42,    47,    49,
      58,    43,    65,    73,    66,    76,    24,    64,    46,    72,
      77,    70,     0,     0,     0,     0,    48,     0,    71
};

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-38)))

#define yytable_value_is_error(Yytable_value) \
  YYID (0)

static const yytype_int8 yycheck[] =
{
      37,     3,     4,     0,     4,     7,     8,     7,     8,    11,
      13,    11,    33,     0,    13,    24,    13,    13,    14,     9,
       0,    34,    16,    32,    23,    27,    23,    27,    18,    23,
      33,    16,    35,    27,    55,    72,    49,    23,    34,    52,
       6,    13,    27,    29,    30,    31,    12,     9,    10,    11,
      13,    14,    25,    17,    25,    25,    24,    24,     5,    11,
      19,    26,    18,    18,    24,    18,     7,    28,    18,    58,
      24,    55,    -1,    -1,    -1,    -1,    27,    -1,    36
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     0,    13,    23,    38,    39,    40,    41,     3,     4,
       7,     8,    11,    27,    42,    43,    44,    45,    46,    47,
      49,     0,     0,    13,    39,    25,    16,    23,    48,    49,
      25,    25,    13,    33,    35,    50,    51,    52,    53,    54,
      17,    24,    24,    26,     6,    12,    48,     5,    43,    11,
       9,    10,    11,    14,    40,    55,    56,    50,    19,    53,
      23,    29,    30,    31,    28,    18,    24,    50,    50,    34,
      56,    36,    52,    18,    24,    32,    18,    24
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

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
      yyerror (param, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))

/* Error token number */
#define YYTERROR	1
#define YYERRCODE	256


/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */
#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, param); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, tk_expr_t **param)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, param)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    tk_expr_t **param;
#endif
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
  YYUSE (param);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
        break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, tk_expr_t **param)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, param)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    tk_expr_t **param;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, param);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, tk_expr_t **param)
#else
static void
yy_reduce_print (yyvsp, yyrule, param)
    YYSTYPE *yyvsp;
    int yyrule;
    tk_expr_t **param;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , param);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, param); \
} while (YYID (0))

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
#ifndef	YYINITDEPTH
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
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
  YYSIZE_T yysize0 = yytnamerr (YY_NULL, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULL;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
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
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULL, yytname[yyx]);
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

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, tk_expr_t **param)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, param)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    tk_expr_t **param;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (param);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}




/* The lookahead symbol.  */
int yychar;


#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval YY_INITIAL_VALUE(yyval_default);

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (tk_expr_t **param)
#else
int
yyparse (param)
    tk_expr_t **param;
#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

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
	/* Give user a chance to reallocate the stack.  Use copies of
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
      yychar = YYLEX;
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
     `$$ = $1'.

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
/* Line 1792 of yacc.c  */
#line 73 "tcpkali_expr_y.y"
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        *(tk_expr_t **)param = expr;
        return 0;
    }
    break;

  case 3:
/* Line 1792 of yacc.c  */
#line 79 "tcpkali_expr_y.y"
    {
        *(tk_expr_t **)param = (yyvsp[(1) - (2)].tv_expr);
        return 0;
    }
    break;

  case 4:
/* Line 1792 of yacc.c  */
#line 85 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = (yyvsp[(1) - (1)].tv_expr);
    }
    break;

  case 5:
/* Line 1792 of yacc.c  */
#line 88 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = concat_expressions((yyvsp[(1) - (2)].tv_expr), (yyvsp[(2) - (2)].tv_expr));
    }
    break;

  case 7:
/* Line 1792 of yacc.c  */
#line 94 "tcpkali_expr_y.y"
    {
        size_t len = (((yyvsp[(1) - (2)].tv_string)).len + ((yyvsp[(2) - (2)].tv_string)).len);
        char *p = malloc(len + 1);
        memcpy(p, ((yyvsp[(1) - (2)].tv_string)).buf, ((yyvsp[(1) - (2)].tv_string)).len);
        memcpy(p + ((yyvsp[(1) - (2)].tv_string)).len, ((yyvsp[(2) - (2)].tv_string)).buf, ((yyvsp[(2) - (2)].tv_string)).len);
        p[len] = '\0';
        (yyval.tv_string).buf = p;
        (yyval.tv_string).len = len;
    }
    break;

  case 8:
/* Line 1792 of yacc.c  */
#line 105 "tcpkali_expr_y.y"
    {
        /* If there's nothing to parse, don't return anything */
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ((yyvsp[(1) - (1)].tv_string)).buf;
        expr->u.data.size = ((yyvsp[(1) - (1)].tv_string)).len;
        expr->estimate_size = ((yyvsp[(1) - (1)].tv_string)).len;
        (yyval.tv_expr) = expr;
    }
    break;

  case 9:
/* Line 1792 of yacc.c  */
#line 114 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = (yyvsp[(2) - (3)].tv_expr);
    }
    break;

  case 10:
/* Line 1792 of yacc.c  */
#line 117 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = (yyvsp[(2) - (3)].tv_expr);
    }
    break;

  case 12:
/* Line 1792 of yacc.c  */
#line 124 "tcpkali_expr_y.y"
    {    /* \{<filename.txt>} */
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ((yyvsp[(1) - (1)].tv_string)).buf;
        expr->u.data.size = ((yyvsp[(1) - (1)].tv_string)).len;
        expr->estimate_size = ((yyvsp[(1) - (1)].tv_string)).len;
        (yyval.tv_expr) = expr;
    }
    break;

  case 13:
/* Line 1792 of yacc.c  */
#line 132 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = (yyvsp[(1) - (1)].tv_expr);
        (yyval.tv_expr)->dynamic_scope = DS_PER_CONNECTION;
    }
    break;

  case 14:
/* Line 1792 of yacc.c  */
#line 136 "tcpkali_expr_y.y"
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ((yyvsp[(2) - (2)].tv_string)).buf;
        expr->u.data.size = ((yyvsp[(2) - (2)].tv_string)).len;
        expr->estimate_size = ((yyvsp[(2) - (2)].tv_string)).len;
        (yyval.tv_expr) = calloc(1, sizeof(tk_expr_t));
        (yyval.tv_expr)->type = EXPR_RAW;
        (yyval.tv_expr)->u.raw.expr = expr;
        (yyval.tv_expr)->estimate_size = expr->estimate_size;
    }
    break;

  case 15:
/* Line 1792 of yacc.c  */
#line 147 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = calloc(1, sizeof(tk_expr_t));
        (yyval.tv_expr)->type = EXPR_RAW;
        (yyval.tv_expr)->u.raw.expr = (yyvsp[(3) - (4)].tv_expr);
        (yyval.tv_expr)->estimate_size = (yyvsp[(3) - (4)].tv_expr)->estimate_size;
        (yyval.tv_expr)->dynamic_scope = (yyvsp[(3) - (4)].tv_expr)->dynamic_scope;
    }
    break;

  case 16:
/* Line 1792 of yacc.c  */
#line 154 "tcpkali_expr_y.y"
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        char *data = malloc(tregex_max_size((yyvsp[(4) - (4)].tv_regex)) + 1);
        assert(data);
        expr->u.data.data = data;
        expr->u.data.size = tregex_eval((yyvsp[(4) - (4)].tv_regex), data, tregex_max_size((yyvsp[(4) - (4)].tv_regex))+ 1);
        expr->estimate_size = expr->u.data.size;
        tregex_free((yyvsp[(4) - (4)].tv_regex));
        (yyval.tv_expr) = expr;
    }
    break;

  case 17:
/* Line 1792 of yacc.c  */
#line 165 "tcpkali_expr_y.y"
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_REGEX;
        expr->u.regex.re = (yyvsp[(4) - (4)].tv_regex);
        expr->estimate_size = tregex_max_size((yyvsp[(4) - (4)].tv_regex));
        expr->dynamic_scope = DS_PER_CONNECTION;
        (yyval.tv_expr) = expr;
    }
    break;

  case 18:
/* Line 1792 of yacc.c  */
#line 173 "tcpkali_expr_y.y"
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_REGEX;
        expr->u.regex.re = (yyvsp[(2) - (2)].tv_regex);
        expr->estimate_size = tregex_max_size((yyvsp[(2) - (2)].tv_regex));
        expr->dynamic_scope = DS_PER_MESSAGE;
        (yyval.tv_expr) = expr;
    }
    break;

  case 19:
/* Line 1792 of yacc.c  */
#line 183 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_MODULO;
        (yyval.tv_expr)->u.modulo.expr = (yyvsp[(1) - (3)].tv_expr);
        (yyval.tv_expr)->u.modulo.modulo_value = (yyvsp[(3) - (3)].tv_long);
        (yyval.tv_expr)->estimate_size = (yyvsp[(1) - (3)].tv_expr)->estimate_size;
    }
    break;

  case 20:
/* Line 1792 of yacc.c  */
#line 190 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_CONNECTION_PTR;
        (yyval.tv_expr)->estimate_size = sizeof("100000000000000");
    }
    break;

  case 21:
/* Line 1792 of yacc.c  */
#line 195 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_CONNECTION_UID;
        (yyval.tv_expr)->estimate_size = sizeof("100000000000000");
    }
    break;

  case 23:
/* Line 1792 of yacc.c  */
#line 204 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = (yyvsp[(1) - (2)].tv_expr);
        (yyval.tv_expr)->u.ws_frame.fin = 0; /* Expect continuation. */
    }
    break;

  case 24:
/* Line 1792 of yacc.c  */
#line 209 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = (yyvsp[(1) - (2)].tv_expr);
        (yyval.tv_expr)->u.ws_frame.rsvs |= (yyvsp[(2) - (2)].tv_long);
    }
    break;

  case 26:
/* Line 1792 of yacc.c  */
#line 217 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = (yyvsp[(1) - (2)].tv_expr);
        /* Combine old data with new data. */
        size_t total_size = (yyval.tv_expr)->u.ws_frame.size + ((yyvsp[(2) - (2)].tv_string)).len;
        char *p = malloc(total_size + 1);
        assert(p);
        memcpy(p, (yyval.tv_expr)->u.ws_frame.data, (yyval.tv_expr)->u.ws_frame.size);
        memcpy(p + (yyval.tv_expr)->u.ws_frame.size, ((yyvsp[(2) - (2)].tv_string)).buf, ((yyvsp[(2) - (2)].tv_string)).len);
        p[total_size] = '\0';
        free((void *)(yyval.tv_expr)->u.ws_frame.data);
        (yyval.tv_expr)->u.ws_frame.data = p;
        (yyval.tv_expr)->u.ws_frame.size = total_size;
        (yyval.tv_expr)->estimate_size += ((yyvsp[(2) - (2)].tv_string)).len;
    }
    break;

  case 27:
/* Line 1792 of yacc.c  */
#line 233 "tcpkali_expr_y.y"
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_WS_FRAME;
        (yyval.tv_expr)->u.ws_frame.opcode = (yyvsp[(3) - (3)].tv_opcode);
        (yyval.tv_expr)->u.ws_frame.fin = 1; /* Complete frame */
        (yyval.tv_expr)->estimate_size = WEBSOCKET_MAX_FRAME_HDR_SIZE;
    }
    break;

  case 30:
/* Line 1792 of yacc.c  */
#line 244 "tcpkali_expr_y.y"
    {
        const char *name = (yyvsp[(2) - (3)].tv_string).buf;
        FILE *fp = fopen(name, "r");
        if(!fp) {
            fprintf(stderr, "Can't open \"%s\": %s\n", name, strerror(errno));
            exit(1);
        }
        fseek(fp, 0, SEEK_END);
        (yyval.tv_string).len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        (yyval.tv_string).buf = malloc((yyval.tv_string).len + 1);
        if((yyval.tv_string).buf == NULL || fread((yyval.tv_string).buf, 1, (yyval.tv_string).len, fp) != (yyval.tv_string).len) {
            fprintf(stderr, "Can't read \"%s\": %s\n", name, strerror(errno));
            exit(1);
        }
        fclose(fp);
        (yyval.tv_string).buf[(yyval.tv_string).len] = '\0';
    }
    break;

  case 32:
/* Line 1792 of yacc.c  */
#line 267 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = tregex_alternative((yyvsp[(1) - (1)].tv_regex));
    }
    break;

  case 33:
/* Line 1792 of yacc.c  */
#line 270 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = tregex_alternative_add((yyvsp[(1) - (3)].tv_regex), (yyvsp[(3) - (3)].tv_regex));
    }
    break;

  case 35:
/* Line 1792 of yacc.c  */
#line 276 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = tregex_join((yyvsp[(1) - (2)].tv_regex), (yyvsp[(2) - (2)].tv_regex));
    }
    break;

  case 37:
/* Line 1792 of yacc.c  */
#line 282 "tcpkali_expr_y.y"
    { (yyval.tv_regex) = tregex_repeat((yyvsp[(1) - (2)].tv_regex), 0, 1); }
    break;

  case 38:
/* Line 1792 of yacc.c  */
#line 283 "tcpkali_expr_y.y"
    { (yyval.tv_regex) = tregex_repeat((yyvsp[(1) - (2)].tv_regex), 1, 16); }
    break;

  case 39:
/* Line 1792 of yacc.c  */
#line 284 "tcpkali_expr_y.y"
    { (yyval.tv_regex) = tregex_repeat((yyvsp[(1) - (2)].tv_regex), 0, 16); }
    break;

  case 40:
/* Line 1792 of yacc.c  */
#line 285 "tcpkali_expr_y.y"
    { (yyval.tv_regex) = tregex_repeat((yyvsp[(1) - (4)].tv_regex), (yyvsp[(3) - (4)].tv_long), (yyvsp[(3) - (4)].tv_long)); }
    break;

  case 41:
/* Line 1792 of yacc.c  */
#line 286 "tcpkali_expr_y.y"
    { (yyval.tv_regex) = tregex_repeat((yyvsp[(1) - (6)].tv_regex), (yyvsp[(3) - (6)].tv_long), (yyvsp[(5) - (6)].tv_long)); }
    break;

  case 42:
/* Line 1792 of yacc.c  */
#line 289 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = tregex_string((yyvsp[(1) - (1)].tv_string).buf, (yyvsp[(1) - (1)].tv_string).len);
    }
    break;

  case 43:
/* Line 1792 of yacc.c  */
#line 292 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = (yyvsp[(2) - (3)].tv_regex);
    }
    break;

  case 44:
/* Line 1792 of yacc.c  */
#line 295 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = (yyvsp[(2) - (3)].tv_regex);
    }
    break;

  case 46:
/* Line 1792 of yacc.c  */
#line 301 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = tregex_union_ranges((yyvsp[(1) - (2)].tv_regex), (yyvsp[(2) - (2)].tv_regex));
    }
    break;

  case 47:
/* Line 1792 of yacc.c  */
#line 306 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = tregex_range_from_string((yyvsp[(1) - (1)].tv_string).buf, (yyvsp[(1) - (1)].tv_string).len);
    }
    break;

  case 48:
/* Line 1792 of yacc.c  */
#line 309 "tcpkali_expr_y.y"
    {
        (yyval.tv_regex) = tregex_range((yyvsp[(1) - (1)].tv_class_range).from, (yyvsp[(1) - (1)].tv_class_range).to);
    }
    break;


/* Line 1792 of yacc.c  */
#line 1859 "tcpkali_expr_y.c"
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

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (param, YY_("syntax error"));
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
        yyerror (param, yymsgp);
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
		      yytoken, &yylval, param);
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

  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

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
		  yystos[yystate], yyvsp, param);
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
  yyerror (param, YY_("memory exhausted"));
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
                  yytoken, &yylval, param);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, param);
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
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


/* Line 2055 of yacc.c  */
#line 313 "tcpkali_expr_y.y"


int
yyerror(tk_expr_t **param, const char *msg) {
    (void)param;
    extern char *yytext;
    fprintf(stderr, "Parse error near token \"%s\": %s\n", yytext, msg);
    return -1;
}

