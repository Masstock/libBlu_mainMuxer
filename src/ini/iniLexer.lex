%{
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

/* Local import paths are relative to main src folder. */
#include "ini/iniData.h"
#include "ini/iniParser.tab.h"

int lineno = 1;
int charno = 1;

#define YY_USER_ACTION                                                        \
  {                                                                           \
    inilloc.first_line = inilloc.last_line = yylineno;                        \
    inilloc.first_column = charno;                                            \
    inilloc.last_column = charno + yyleng;                                    \
    charno += yyleng;                                                         \
  }

%}

%x VALUE INLINE_COMMENT

%option noyywrap
%option noinput
%option nounput
%option yylineno
%option never-interactive

%%
=                       {BEGIN VALUE; return yytext[0]; }
[a-zA-Z0-9_]*           {inilval.string = lb_str_dup(yytext); return IDENT; }
[;#]                    {BEGIN INLINE_COMMENT; }
[\r\t ]*                ;
\n                      {lineno++; charno = 0; }

<VALUE>[;#]             {BEGIN INLINE_COMMENT; }
<VALUE>,                {BEGIN INITIAL; }
<VALUE>\n               {lineno++; charno = 0; BEGIN INITIAL; }
<VALUE>[^;#\n]*         {inilval.string = lb_str_dup_strip(yytext); return VAL; }


<INLINE_COMMENT>\n      {lineno++; charno = 0; BEGIN INITIAL; }
<INLINE_COMMENT>.       ;

.                       {return yytext[0]; }
<<EOF>>                 {return 0; }
%%

int yyerror(
  IniFileContext * ctx,
  char * format,
  ...
)
{
  va_list args;

  fprintf(
    stderr, "%d:%d: " ANSI_BOLD ANSI_COLOR_RED "error: " ANSI_RESET,
    lineno, charno
  );

  va_start(args, format);
  vfprintf(stderr, format, args);
  fprintf(stderr, ".\n");
  va_end(args);

  if (NULL != ctx && NULL != ctx->src) {
    assert(lineno-1 < (int) ctx->src_nb_lines);
    fprintf(stderr, "%4d | %s", lineno, ctx->src[lineno-1]);
  }

  return 1;
}