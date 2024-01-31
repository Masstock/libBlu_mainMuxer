%{
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

/* Local import paths are relative to main src folder. */
#include "ini/iniData.h"
#include "ini/iniTree.h"

#define inierror yyerror

extern int yylex();
extern int yyerror(IniFileContext * ctx, char * format, ...);
extern char * yytext;
extern int lineno;

#define LOC(yyloc)                                                            \
  (                                                                           \
    (IniFileSrcLocation) {                                                    \
      .first_line   = (yyloc).first_line,                                     \
      .first_column = (yyloc).first_column,                                   \
      .last_line    = (yyloc).last_line,                                      \
      .last_column  = (yyloc).last_column                                     \
    }                                                                         \
  )

%}

%define api.prefix {ini}
%code provides
{
  #define YY_DECL                                                             \
    int inilex ()

  // Declare the scanner.
  YY_DECL;
}

%locations
%parse-param {IniFileContext * ctx}

%union {
  IniFileNodePtr node;
  char * string;
}

%type <node> file sections section section_inner entry
%type <string> IDENT VAL
%token IDENT VAL

%%
file:
  section_inner sections
  {
    if (NULL == ($$ = createSectionIniFileNode(NULL)))
      return 1;
    attachChildIniFileNode($$, $1);
    attachChildIniFileNode($$, $2);
    ctx->tree = $$;
  }
  ;

sections:
    sections section
    {
      attachSiblingIniFileNode($2, $1);
      $$ = $2;
    }
    | { $$ = NULL; }
    ;

section:
    '[' IDENT ']' section_inner
    {
      if (NULL == ($$ = createSectionIniFileNode($2)))
        return 1;
      free($2);
      attachChildIniFileNode($$, $4);
    }
    ;

section_inner:
    section_inner entry
    {
      attachSiblingIniFileNode($2, $1);
      $$ = $2;
    }
    | { $$ = NULL; }
    ;

entry:
    IDENT '=' VAL
    {
      if (NULL == ($$ = createEntryIniFileNode($1, $3)))
        return 1;
      free($1);
      free($3);
    }
    ;