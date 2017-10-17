/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "tcpkali_expr_y.y" /* yacc.c:339  */


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


#line 84 "tcpkali_expr_y.c" /* yacc.c:339  */

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

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_TCPKALI_EXPR_Y_H_INCLUDED
# define YY_YY_TCPKALI_EXPR_Y_H_INCLUDED
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
    END = 0,
    TOK_ws = 258,
    TOK_raw = 259,
    TOK_ws_opcode = 260,
    TOK_ws_reserved_flag = 261,
    TOK_global = 262,
    TOK_connection = 263,
    TOK_message = 264,
    TOK_ptr = 265,
    TOK_uid = 266,
    TOK_regex = 267,
    TOK_marker = 268,
    TOK_ellipsis = 269,
    string_token = 270,
    class_range_token = 271,
    repeat_range_token = 272,
    quoted_string = 273,
    filename = 274,
    integer = 275
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
#define TOK_message 264
#define TOK_ptr 265
#define TOK_uid 266
#define TOK_regex 267
#define TOK_marker 268
#define TOK_ellipsis 269
#define string_token 270
#define class_range_token 271
#define repeat_range_token 272
#define quoted_string 273
#define filename 274
#define integer 275

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 21 "tcpkali_expr_y.y" /* yacc.c:355  */

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

#line 186 "tcpkali_expr_y.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (tk_expr_t **param);

#endif /* !YY_YY_TCPKALI_EXPR_Y_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 203 "tcpkali_expr_y.c" /* yacc.c:358  */

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
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
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
#define YYFINAL  22
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   78

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  39
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  20
/* YYNRULES -- Number of rules.  */
#define YYNRULES  49
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  81

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   278

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    28,     2,     2,
      37,    38,    33,    32,    34,     2,    27,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      29,     2,    30,    31,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    35,     2,    36,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    25,    21,    26,     2,     2,     2,     2,
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
      15,    16,    17,    18,    19,    20,    22,    23,    24
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    75,    75,    81,    87,    90,    95,    96,   107,   116,
     119,   123,   126,   134,   137,   148,   155,   166,   174,   184,
     192,   198,   204,   212,   214,   219,   225,   227,   243,   251,
     251,   254,   274,   277,   280,   285,   286,   291,   292,   293,
     294,   295,   296,   299,   302,   305,   310,   311,   316,   319
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of expression\"", "error", "$undefined", "\"ws\"", "\"raw\"",
  "\"text, binary, close, ping, pong, continuation\"",
  "\"rsv1, rsv2, rsv3\"", "\"global\"", "\"connection\"", "\"message\"",
  "\" ptr\"", "\"uid\"", "\"re\"", "\"marker\"", "\"...\"",
  "\"arbitrary string\"", "\"regex character class range\"",
  "\"regex repeat spec\"", "\"quoted string\"", "\"file name\"", "integer",
  "'|'", "\"some string or \\\\{expression}\"", "\"data and expressions\"",
  "\"connection, global, re, or <filename.ext>\"", "'{'", "'}'", "'.'",
  "'%'", "'<'", "'>'", "'?'", "'+'", "'*'", "','", "'['", "']'", "'('",
  "')'", "$accept", "Grammar", "ByteSequencesAndExpressions", "String",
  "ByteSequenceOrExpr", "WSExpression", "NonWSExpression", "NumericExpr",
  "WSFrameFinalized", "WSFrameWithData", "WSBasicFrame", "FileOrQuoted",
  "File", "CompleteRegex", "RegexAlternatives", "RegexSequence",
  "RepeatedRegex", "RegexPiece", "RegexClasses", "RegexClass", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   124,   276,   277,   278,   123,   125,    46,    37,    60,
      62,    63,    43,    42,    44,    91,    93,    40,    41
};
# endif

#define YYPACT_NINF -40

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-40)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
       3,   -40,   -40,    -2,    23,    41,    28,     5,    27,    13,
      29,    31,    32,   -11,    38,    34,    35,    37,    39,    11,
     -40,   -40,   -40,   -40,   -40,   -40,    50,   -40,     4,   -40,
     -40,    51,    36,    49,   -40,     6,   -11,   -40,    43,   -11,
     -40,    19,    40,   -40,   -40,    46,   -40,   -40,   -40,   -40,
      42,   -11,   -40,   -40,   -11,   -40,   -40,    28,    -1,   -40,
      33,   -11,   -40,    47,   -40,   -40,   -40,   -40,   -40,   -40,
     -40,   -40,   -40,   -40,   -40,   -11,    -9,   -40,    52,    48,
     -40
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     2,     6,     0,     0,     0,     8,     4,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    13,    11,    23,
      26,    12,     1,     3,     7,     5,     0,    29,     0,    14,
      30,     0,     0,     0,    43,     0,     0,    18,    32,    33,
      35,    37,     0,     9,    10,     0,    25,    24,    27,    28,
       0,     0,    20,    21,     0,    22,    49,    48,     0,    46,
       0,     0,    36,     0,    38,    39,    40,    31,    19,    15,
      16,    17,    44,    47,    45,    34,     0,    41,     0,     0,
      42
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -40,   -40,    62,   -26,   -40,   -40,    45,   -40,   -40,   -40,
     -40,    56,    30,   -17,   -40,    15,   -39,   -40,   -40,    20
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     4,     5,     6,     7,    15,    16,    17,    18,    19,
      20,    29,    21,    37,    38,    39,    40,    41,    58,    59
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      62,     8,     9,     1,    34,    10,    11,    12,     9,    57,
      13,    10,    11,    12,     2,    56,    13,    77,     2,    60,
       2,     2,    56,    22,    35,    78,    36,    14,     3,    27,
       3,    27,    57,    14,    70,    72,    62,    71,    28,    30,
      14,    23,    14,    24,    63,    46,    52,    53,    54,    30,
      64,    65,    66,    47,    26,    49,    31,    42,    32,    33,
      43,    44,    55,    51,    61,    45,    68,    76,    69,    25,
      67,    74,    79,    50,    80,    48,    75,     0,    73
};

static const yytype_int8 yycheck[] =
{
      39,     3,     4,     0,    15,     7,     8,     9,     4,    35,
      12,     7,     8,     9,    15,    16,    12,    26,    15,    36,
      15,    15,    16,     0,    35,    34,    37,    29,    25,    18,
      25,    18,    58,    29,    51,    36,    75,    54,    25,     9,
      29,     0,    29,    15,    25,     6,    10,    11,    12,    19,
      31,    32,    33,    14,    27,     5,    27,    19,    27,    27,
      26,    26,    13,    12,    21,    28,    20,    20,    26,     7,
      30,    38,    20,    28,    26,    19,    61,    -1,    58
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     0,    15,    25,    40,    41,    42,    43,     3,     4,
       7,     8,     9,    12,    29,    44,    45,    46,    47,    48,
      49,    51,     0,     0,    15,    41,    27,    18,    25,    50,
      51,    27,    27,    27,    15,    35,    37,    52,    53,    54,
      55,    56,    19,    26,    26,    28,     6,    14,    50,     5,
      45,    12,    10,    11,    12,    13,    16,    42,    57,    58,
      52,    21,    55,    25,    31,    32,    33,    30,    20,    26,
      52,    52,    36,    58,    38,    54,    20,    26,    34,    20,
      26
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    39,    40,    40,    41,    41,    42,    42,    43,    43,
      43,    44,    45,    45,    45,    45,    45,    45,    45,    46,
      46,    46,    46,    47,    47,    47,    48,    48,    49,    50,
      50,    51,    52,    53,    53,    54,    54,    55,    55,    55,
      55,    55,    55,    56,    56,    56,    57,    57,    58,    58
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     1,     2,     1,     2,     1,     3,
       3,     1,     1,     1,     2,     4,     4,     4,     2,     3,
       3,     3,     3,     1,     2,     2,     1,     2,     3,     1,
       1,     3,     1,     1,     3,     1,     2,     1,     2,     2,
       2,     4,     6,     1,     3,     3,     1,     2,     1,     1
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
      yyerror (param, YY_("syntax error: cannot back up")); \
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
                  Type, Value, param); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, tk_expr_t **param)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (param);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, tk_expr_t **param)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, param);
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, tk_expr_t **param)
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
                                              , param);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, param); \
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, tk_expr_t **param)
{
  YYUSE (yyvaluep);
  YYUSE (param);
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
yyparse (tk_expr_t **param)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

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
#line 75 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        *(tk_expr_t **)param = expr;
        return 0;
    }
#line 1344 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 3:
#line 81 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        *(tk_expr_t **)param = (yyvsp[-1].tv_expr);
        return 0;
    }
#line 1353 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 4:
#line 87 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = (yyvsp[0].tv_expr);
    }
#line 1361 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 5:
#line 90 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = concat_expressions((yyvsp[-1].tv_expr), (yyvsp[0].tv_expr));
    }
#line 1369 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 7:
#line 96 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        size_t len = (((yyvsp[-1].tv_string)).len + ((yyvsp[0].tv_string)).len);
        char *p = malloc(len + 1);
        memcpy(p, ((yyvsp[-1].tv_string)).buf, ((yyvsp[-1].tv_string)).len);
        memcpy(p + ((yyvsp[-1].tv_string)).len, ((yyvsp[0].tv_string)).buf, ((yyvsp[0].tv_string)).len);
        p[len] = '\0';
        (yyval.tv_string).buf = p;
        (yyval.tv_string).len = len;
    }
#line 1383 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 8:
#line 107 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        /* If there's nothing to parse, don't return anything */
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ((yyvsp[0].tv_string)).buf;
        expr->u.data.size = ((yyvsp[0].tv_string)).len;
        expr->estimate_size = ((yyvsp[0].tv_string)).len;
        (yyval.tv_expr) = expr;
    }
#line 1397 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 9:
#line 116 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = (yyvsp[-1].tv_expr);
    }
#line 1405 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 10:
#line 119 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = (yyvsp[-1].tv_expr);
    }
#line 1413 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 12:
#line 126 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {    /* \{<filename.txt>} */
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ((yyvsp[0].tv_string)).buf;
        expr->u.data.size = ((yyvsp[0].tv_string)).len;
        expr->estimate_size = ((yyvsp[0].tv_string)).len;
        (yyval.tv_expr) = expr;
    }
#line 1426 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 13:
#line 134 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = (yyvsp[0].tv_expr);
    }
#line 1434 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 14:
#line 137 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ((yyvsp[0].tv_string)).buf;
        expr->u.data.size = ((yyvsp[0].tv_string)).len;
        expr->estimate_size = ((yyvsp[0].tv_string)).len;
        (yyval.tv_expr) = calloc(1, sizeof(tk_expr_t));
        (yyval.tv_expr)->type = EXPR_RAW;
        (yyval.tv_expr)->u.raw.expr = expr;
        (yyval.tv_expr)->estimate_size = expr->estimate_size;
    }
#line 1450 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 15:
#line 148 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = calloc(1, sizeof(tk_expr_t));
        (yyval.tv_expr)->type = EXPR_RAW;
        (yyval.tv_expr)->u.raw.expr = (yyvsp[-1].tv_expr);
        (yyval.tv_expr)->estimate_size = (yyvsp[-1].tv_expr)->estimate_size;
        (yyval.tv_expr)->dynamic_scope = (yyvsp[-1].tv_expr)->dynamic_scope;
    }
#line 1462 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 16:
#line 155 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        char *data = malloc(tregex_max_size((yyvsp[0].tv_regex)) + 1);
        assert(data);
        expr->u.data.data = data;
        expr->u.data.size = tregex_eval((yyvsp[0].tv_regex), data, tregex_max_size((yyvsp[0].tv_regex))+ 1);
        expr->estimate_size = expr->u.data.size;
        tregex_free((yyvsp[0].tv_regex));
        (yyval.tv_expr) = expr;
    }
#line 1478 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 17:
#line 166 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_REGEX;
        expr->u.regex.re = (yyvsp[0].tv_regex);
        expr->estimate_size = tregex_max_size((yyvsp[0].tv_regex));
        expr->dynamic_scope = DS_PER_CONNECTION;
        (yyval.tv_expr) = expr;
    }
#line 1491 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 18:
#line 174 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_REGEX;
        expr->u.regex.re = (yyvsp[0].tv_regex);
        expr->estimate_size = tregex_max_size((yyvsp[0].tv_regex));
        expr->dynamic_scope = DS_PER_MESSAGE;
        (yyval.tv_expr) = expr;
    }
#line 1504 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 19:
#line 184 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_MODULO;
        (yyval.tv_expr)->u.modulo.expr = (yyvsp[-2].tv_expr);
        (yyval.tv_expr)->u.modulo.modulo_value = (yyvsp[0].tv_long);
        (yyval.tv_expr)->estimate_size = (yyvsp[-2].tv_expr)->estimate_size;
        (yyval.tv_expr)->dynamic_scope = (yyvsp[-2].tv_expr)->dynamic_scope;
    }
#line 1517 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 20:
#line 192 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_CONNECTION_PTR;
        (yyval.tv_expr)->estimate_size = sizeof("100000000000000");
        (yyval.tv_expr)->dynamic_scope = DS_PER_CONNECTION;
    }
#line 1528 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 21:
#line 198 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_CONNECTION_UID;
        (yyval.tv_expr)->estimate_size = sizeof("100000000000000");
        (yyval.tv_expr)->dynamic_scope = DS_PER_CONNECTION;
    }
#line 1539 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 22:
#line 204 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_MESSAGE_MARKER;
        (yyval.tv_expr)->estimate_size = sizeof("1000000000000" "1000000000000000" "!") - 1;
        (yyval.tv_expr)->dynamic_scope = DS_PER_MESSAGE;
    }
#line 1550 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 24:
#line 214 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = (yyvsp[-1].tv_expr);
        (yyval.tv_expr)->u.ws_frame.fin = 0; /* Expect continuation. */
    }
#line 1559 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 25:
#line 219 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = (yyvsp[-1].tv_expr);
        (yyval.tv_expr)->u.ws_frame.rsvs |= (yyvsp[0].tv_long);
    }
#line 1568 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 27:
#line 227 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = (yyvsp[-1].tv_expr);
        /* Combine old data with new data. */
        size_t total_size = (yyval.tv_expr)->u.ws_frame.size + ((yyvsp[0].tv_string)).len;
        char *p = malloc(total_size + 1);
        assert(p);
        memcpy(p, (yyval.tv_expr)->u.ws_frame.data, (yyval.tv_expr)->u.ws_frame.size);
        memcpy(p + (yyval.tv_expr)->u.ws_frame.size, ((yyvsp[0].tv_string)).buf, ((yyvsp[0].tv_string)).len);
        p[total_size] = '\0';
        free((void *)(yyval.tv_expr)->u.ws_frame.data);
        (yyval.tv_expr)->u.ws_frame.data = p;
        (yyval.tv_expr)->u.ws_frame.size = total_size;
        (yyval.tv_expr)->estimate_size += ((yyvsp[0].tv_string)).len;
    }
#line 1587 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 28:
#line 243 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_expr) = calloc(1, sizeof(*((yyval.tv_expr))));
        (yyval.tv_expr)->type = EXPR_WS_FRAME;
        (yyval.tv_expr)->u.ws_frame.opcode = (yyvsp[0].tv_opcode);
        (yyval.tv_expr)->u.ws_frame.fin = 1; /* Complete frame */
        (yyval.tv_expr)->estimate_size = WEBSOCKET_MAX_FRAME_HDR_SIZE;
    }
#line 1599 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 31:
#line 254 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        const char *name = (yyvsp[-1].tv_string).buf;
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
#line 1622 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 33:
#line 277 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = tregex_alternative((yyvsp[0].tv_regex));
    }
#line 1630 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 34:
#line 280 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = tregex_alternative_add((yyvsp[-2].tv_regex), (yyvsp[0].tv_regex));
    }
#line 1638 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 36:
#line 286 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = tregex_join((yyvsp[-1].tv_regex), (yyvsp[0].tv_regex));
    }
#line 1646 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 38:
#line 292 "tcpkali_expr_y.y" /* yacc.c:1661  */
    { (yyval.tv_regex) = tregex_repeat((yyvsp[-1].tv_regex), 0, 1); }
#line 1652 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 39:
#line 293 "tcpkali_expr_y.y" /* yacc.c:1661  */
    { (yyval.tv_regex) = tregex_repeat((yyvsp[-1].tv_regex), 1, 16); }
#line 1658 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 40:
#line 294 "tcpkali_expr_y.y" /* yacc.c:1661  */
    { (yyval.tv_regex) = tregex_repeat((yyvsp[-1].tv_regex), 0, 16); }
#line 1664 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 41:
#line 295 "tcpkali_expr_y.y" /* yacc.c:1661  */
    { (yyval.tv_regex) = tregex_repeat((yyvsp[-3].tv_regex), (yyvsp[-1].tv_long), (yyvsp[-1].tv_long)); }
#line 1670 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 42:
#line 296 "tcpkali_expr_y.y" /* yacc.c:1661  */
    { (yyval.tv_regex) = tregex_repeat((yyvsp[-5].tv_regex), (yyvsp[-3].tv_long), (yyvsp[-1].tv_long)); }
#line 1676 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 43:
#line 299 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = tregex_string((yyvsp[0].tv_string).buf, (yyvsp[0].tv_string).len);
    }
#line 1684 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 44:
#line 302 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = (yyvsp[-1].tv_regex);
    }
#line 1692 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 45:
#line 305 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = (yyvsp[-1].tv_regex);
    }
#line 1700 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 47:
#line 311 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = tregex_union_ranges((yyvsp[-1].tv_regex), (yyvsp[0].tv_regex));
    }
#line 1708 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 48:
#line 316 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = tregex_range_from_string((yyvsp[0].tv_string).buf, (yyvsp[0].tv_string).len);
    }
#line 1716 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;

  case 49:
#line 319 "tcpkali_expr_y.y" /* yacc.c:1661  */
    {
        (yyval.tv_regex) = tregex_range((yyvsp[0].tv_class_range).from, (yyvsp[0].tv_class_range).to);
    }
#line 1724 "tcpkali_expr_y.c" /* yacc.c:1661  */
    break;


#line 1728 "tcpkali_expr_y.c" /* yacc.c:1661  */
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
  /* Do not reclaim the symbols of the rule whose action triggered
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
  return yyresult;
}
#line 323 "tcpkali_expr_y.y" /* yacc.c:1906  */


int
yyerror(tk_expr_t **param, const char *msg) {
    (void)param;
    extern char *yytext;
    fprintf(stderr, "Parse error near token \"%s\": %s\n", yytext, msg);
    return -1;
}

