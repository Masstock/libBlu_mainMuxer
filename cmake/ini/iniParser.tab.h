/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_INI_MNT_WINDOWS_TRAVAIL_TS_MUXER_LIBBLU_MAINMUXER_CMAKE_INI_INIPARSER_TAB_H_INCLUDED
# define YY_INI_MNT_WINDOWS_TRAVAIL_TS_MUXER_LIBBLU_MAINMUXER_CMAKE_INI_INIPARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef INIDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define INIDEBUG 1
#  else
#   define INIDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define INIDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined INIDEBUG */
#if INIDEBUG
extern int inidebug;
#endif

/* Token kinds.  */
#ifndef INITOKENTYPE
# define INITOKENTYPE
  enum initokentype
  {
    INIEMPTY = -2,
    INIEOF = 0,                    /* "end of file"  */
    INIerror = 256,                /* error  */
    INIUNDEF = 257,                /* "invalid token"  */
    IDENT = 258,                   /* IDENT  */
    VAL = 259                      /* VAL  */
  };
  typedef enum initokentype initoken_kind_t;
#endif

/* Value type.  */
#if ! defined INISTYPE && ! defined INISTYPE_IS_DECLARED
union INISTYPE
{
#line 44 "/mnt/Windows/Travail/TS_Muxer/libblu-mainMuxer/src/ini/iniParser.y"

  IniFileNodePtr node;
  char * string;

#line 81 "/mnt/Windows/Travail/TS_Muxer/libblu-mainMuxer/cmake/ini/iniParser.tab.h"

};
typedef union INISTYPE INISTYPE;
# define INISTYPE_IS_TRIVIAL 1
# define INISTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined INILTYPE && ! defined INILTYPE_IS_DECLARED
typedef struct INILTYPE INILTYPE;
struct INILTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define INILTYPE_IS_DECLARED 1
# define INILTYPE_IS_TRIVIAL 1
#endif


extern INISTYPE inilval;
extern INILTYPE inilloc;

int iniparse (IniFileContext * ctx);

/* "%code provides" blocks.  */
#line 33 "/mnt/Windows/Travail/TS_Muxer/libblu-mainMuxer/src/ini/iniParser.y"

  #define YY_DECL                                                             \
    int inilex ()

  // Declare the scanner.
  YY_DECL;

#line 118 "/mnt/Windows/Travail/TS_Muxer/libblu-mainMuxer/cmake/ini/iniParser.tab.h"

#endif /* !YY_INI_MNT_WINDOWS_TRAVAIL_TS_MUXER_LIBBLU_MAINMUXER_CMAKE_INI_INIPARSER_TAB_H_INCLUDED  */
