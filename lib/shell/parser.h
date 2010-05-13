
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
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

/* "%code requires" blocks.  */

/* Line 1676 of yacc.c  */
#line 8 "script.y"

	#include "ast/node.h"
	#include "ast/assignexpr.h"
	#include "ast/binaryopexpr.h"
	#include "ast/cmpexpr.h"
	#include "ast/conststrexpr.h"
	#include "ast/ifstmt.h"
	#include "ast/intexpr.h"
	#include "ast/stmtlist.h"
	#include "ast/unaryopexpr.h"
	#include "ast/varexpr.h"
	#include "ast/command.h"
	#include "ast/cmdexprlist.h"
	#include "ast/subcmd.h"
	#include "ast/redirfd.h"
	#include "ast/redirfile.h"
	#include "ast/forstmt.h"
	#include "ast/exprstmt.h"
	#include "ast/dstrexpr.h"
	#include "ast/whilestmt.h"
	#include "ast/functionstmt.h"
	#include "exec/env.h"
	#include "mem.h"
	#include "shell.h"



/* Line 1676 of yacc.c  */
#line 68 "parser.h"

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     T_IF = 258,
     T_THEN = 259,
     T_ELSE = 260,
     T_FI = 261,
     T_FOR = 262,
     T_DO = 263,
     T_DONE = 264,
     T_WHILE = 265,
     T_FUNCTION = 266,
     T_BEGIN = 267,
     T_END = 268,
     T_NUMBER = 269,
     T_STRING = 270,
     T_STRING_SCONST = 271,
     T_VAR = 272,
     T_ERR2OUT = 273,
     T_OUT2ERR = 274,
     T_APPEND = 275,
     T_ERR2FILE = 276,
     T_OUT2FILE = 277,
     T_ASSIGN = 278,
     T_NEQ = 279,
     T_EQ = 280,
     T_GEQ = 281,
     T_LEQ = 282,
     T_SUB = 283,
     T_ADD = 284,
     T_MOD = 285,
     T_DIV = 286,
     T_MUL = 287,
     T_NEG = 288,
     T_DEC = 289,
     T_INC = 290,
     T_POW = 291
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */
#line 33 "script.y"

	int intval;
	char *strval;
	sASTNode *node;



/* Line 1676 of yacc.c  */
#line 129 "parser.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;

