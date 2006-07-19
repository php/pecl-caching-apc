/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  |          Arun C. Murthy <arunc@yahoo-inc.com>                        |
  |          Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.
*/

/* $Id$ */
#include "apc.h"
#include <stdio.h>
#include "zend_compile.h"


const char* optimizer_zend_util_opcode_to_string(int opcode)
{
    static const char S_ZEND_NOP[] = "NOP";
    static const char S_ZEND_ADD[] = "ADD";
    static const char S_ZEND_SUB[] = "SUB";
    static const char S_ZEND_MUL[] = "MUL";
    static const char S_ZEND_DIV[] = "DIV";
    static const char S_ZEND_MOD[] = "MOD";
    static const char S_ZEND_SL[] = "SL";
    static const char S_ZEND_SR[] = "SR";
    static const char S_ZEND_CONCAT[] = "CONCAT";
    static const char S_ZEND_BW_OR[] = "BW_OR";
    static const char S_ZEND_BW_AND[] = "BW_AND";
    static const char S_ZEND_BW_XOR[] = "BW_XOR";
    static const char S_ZEND_BW_NOT[] = "BW_NOT";
    static const char S_ZEND_BOOL_NOT[] = "BOOL_NOT";
    static const char S_ZEND_BOOL_XOR[] = "BOOL_XOR";
    static const char S_ZEND_IS_IDENTICAL[] = "IS_IDENTICAL";
    static const char S_ZEND_IS_NOT_IDENTICAL[] = "IS_NOT_IDENTICAL";
    static const char S_ZEND_IS_EQUAL[] = "IS_EQUAL";
    static const char S_ZEND_IS_NOT_EQUAL[] = "IS_NOT_EQUAL";
    static const char S_ZEND_IS_SMALLER[] = "IS_SMALLER";
    static const char S_ZEND_IS_SMALLER_OR_EQUAL[] = "IS_SMALLER_OR_EQUAL";
    static const char S_ZEND_CAST[] = "CAST";
    static const char S_ZEND_QM_ASSIGN[] = "QM_ASSIGN";
    static const char S_ZEND_ASSIGN_ADD[] = "ASSIGN_ADD";
    static const char S_ZEND_ASSIGN_SUB[] = "ASSIGN_SUB";
    static const char S_ZEND_ASSIGN_MUL[] = "ASSIGN_MUL";
    static const char S_ZEND_ASSIGN_DIV[] = "ASSIGN_DIV";
    static const char S_ZEND_ASSIGN_MOD[] = "ASSIGN_MOD";
    static const char S_ZEND_ASSIGN_SL[] = "ASSIGN_SL";
    static const char S_ZEND_ASSIGN_SR[] = "ASSIGN_SR";
    static const char S_ZEND_ASSIGN_CONCAT[] = "ASSIGN_CONCAT";
    static const char S_ZEND_ASSIGN_BW_OR[] = "ASSIGN_BW_OR";
    static const char S_ZEND_ASSIGN_BW_AND[] = "ASSIGN_BW_AND";
    static const char S_ZEND_ASSIGN_BW_XOR[] = "ASSIGN_BW_XOR";
    static const char S_ZEND_PRE_INC[] = "PRE_INC";
    static const char S_ZEND_PRE_DEC[] = "PRE_DEC";
    static const char S_ZEND_POST_INC[] = "POST_INC";
    static const char S_ZEND_POST_DEC[] = "POST_DEC";
    static const char S_ZEND_ASSIGN[] = "ASSIGN";
    static const char S_ZEND_ASSIGN_REF[] = "ASSIGN_REF";
    static const char S_ZEND_ECHO[] = "ECHO";
    static const char S_ZEND_PRINT[] = "PRINT";
    static const char S_ZEND_JMP[] = "JMP";
    static const char S_ZEND_JMPZ[] = "JMPZ";
    static const char S_ZEND_JMPNZ[] = "JMPNZ";
    static const char S_ZEND_JMPZNZ[] = "JMPZNZ";
    static const char S_ZEND_JMPZ_EX[] = "JMPZ_EX";
    static const char S_ZEND_JMPNZ_EX[] = "JMPNZ_EX";
    static const char S_ZEND_CASE[] = "CASE";
    static const char S_ZEND_SWITCH_FREE[] = "SWITCH_FREE";
    static const char S_ZEND_BRK[] = "BRK";
    static const char S_ZEND_CONT[] = "CONT";
    static const char S_ZEND_BOOL[] = "BOOL";
    static const char S_ZEND_INIT_STRING[] = "INIT_STRING";
    static const char S_ZEND_ADD_CHAR[] = "ADD_CHAR";
    static const char S_ZEND_ADD_STRING[] = "ADD_STRING";
    static const char S_ZEND_ADD_VAR[] = "ADD_VAR";
    static const char S_ZEND_BEGIN_SILENCE[] = "BEGIN_SILENCE";
    static const char S_ZEND_END_SILENCE[] = "END_SILENCE";
    static const char S_ZEND_INIT_FCALL_BY_NAME[] = "INIT_FCALL_BY_NAME";
    static const char S_ZEND_DO_FCALL[] = "DO_FCALL";
    static const char S_ZEND_DO_FCALL_BY_NAME[] = "DO_FCALL_BY_NAME";
    static const char S_ZEND_RETURN[] = "RETURN";
    static const char S_ZEND_RECV[] = "RECV";
    static const char S_ZEND_RECV_INIT[] = "RECV_INIT";
    static const char S_ZEND_SEND_VAL[] = "SEND_VAL";
    static const char S_ZEND_SEND_VAR[] = "SEND_VAR";
    static const char S_ZEND_SEND_REF[] = "SEND_REF";
    static const char S_ZEND_NEW[] = "NEW";
    static const char S_ZEND_JMP_NO_CTOR[] = "JMP_NO_CTOR";
    static const char S_ZEND_FREE[] = "FREE";
    static const char S_ZEND_INIT_ARRAY[] = "INIT_ARRAY";
    static const char S_ZEND_ADD_ARRAY_ELEMENT[] = "ADD_ARRAY_ELEMENT";
    static const char S_ZEND_INCLUDE_OR_EVAL[] = "INCLUDE_OR_EVAL";
    static const char S_ZEND_UNSET_VAR[] = "UNSET_VAR";
    static const char S_ZEND_UNSET_DIM_OBJ[] = "UNSET_DIM_OBJ";
#ifndef ZEND_ENGINE_2
    static const char S_ZEND_ISSET_ISEMPTY[] = "ISSET_ISEMPTY";
#endif    
    static const char S_ZEND_FE_RESET[] = "FE_RESET";
    static const char S_ZEND_FE_FETCH[] = "FE_FETCH";
    static const char S_ZEND_EXIT[] = "EXIT";
    static const char S_ZEND_FETCH_R[] = "FETCH_R";
    static const char S_ZEND_FETCH_DIM_R[] = "FETCH_DIM_R";
    static const char S_ZEND_FETCH_OBJ_R[] = "FETCH_OBJ_R";
    static const char S_ZEND_FETCH_W[] = "FETCH_W";
    static const char S_ZEND_FETCH_DIM_W[] = "FETCH_DIM_W";
    static const char S_ZEND_FETCH_OBJ_W[] = "FETCH_OBJ_W";
    static const char S_ZEND_FETCH_RW[] = "FETCH_RW";
    static const char S_ZEND_FETCH_DIM_RW[] = "FETCH_DIM_RW";
    static const char S_ZEND_FETCH_OBJ_RW[] = "FETCH_OBJ_RW";
    static const char S_ZEND_FETCH_IS[] = "FETCH_IS";
    static const char S_ZEND_FETCH_DIM_IS[] = "FETCH_DIM_IS";
    static const char S_ZEND_FETCH_OBJ_IS[] = "FETCH_OBJ_IS";
    static const char S_ZEND_FETCH_FUNC_ARG[] = "FETCH_FUNC_ARG";
    static const char S_ZEND_FETCH_DIM_FUNC_ARG[] = "FETCH_DIM_FUNC_ARG";
    static const char S_ZEND_FETCH_OBJ_FUNC_ARG[] = "FETCH_OBJ_FUNC_ARG";
    static const char S_ZEND_FETCH_UNSET[] = "FETCH_UNSET";
    static const char S_ZEND_FETCH_DIM_UNSET[] = "FETCH_DIM_UNSET";
    static const char S_ZEND_FETCH_OBJ_UNSET[] = "FETCH_OBJ_UNSET";
    static const char S_ZEND_FETCH_DIM_TMP_VAR[] = "FETCH_DIM_TMP_VAR";
    static const char S_ZEND_FETCH_CONSTANT[] = "FETCH_CONSTANT";
#ifndef ZEND_ENGINE_2
    static const char S_ZEND_DECLARE_FUNCTION_OR_CLASS[] = "DECLARE_FUNCTION_OR_CLASS";
#endif    
    static const char S_ZEND_EXT_STMT[] = "EXT_STMT";
    static const char S_ZEND_EXT_FCALL_BEGIN[] = "EXT_FCALL_BEGIN";
    static const char S_ZEND_EXT_FCALL_END[] = "EXT_FCALL_END";
    static const char S_ZEND_EXT_NOP[] = "EXT_NOP";
    static const char S_ZEND_TICKS[] = "TICKS";
    static const char S_ZEND_SEND_VAR_NO_REF[] = "SEND_VAR_NO_REF";
#ifdef ZEND_ENGINE_2
    static const char S_ZEND_CATCH[] = "ZEND_CATCH";
    static const char S_ZEND_THROW[] = "ZEND_THROW";
    static const char S_ZEND_FETCH_CLASS[] = "ZEND_FETCH_CLASS";
    static const char S_ZEND_CLONE[] = "ZEND_CLONE";
    static const char S_ZEND_INIT_CTOR_CALL[] = "ZEND_INIT_CTOR_CALL";
    static const char S_ZEND_INIT_METHOD_CALL[] = "ZEND_INIT_METHOD_CALL";
    static const char S_ZEND_INIT_STATIC_METHOD_CALL[] = "ZEND_INIT_STATIC_METHOD_CALL";
    static const char S_ZEND_ISSET_ISEMPTY_VAR[] = "ZEND_ISSET_ISEMPTY_VAR";
    static const char S_ZEND_ISSET_ISEMPTY_DIM_OBJ[] = "ZEND_ISSET_ISEMPTY_DIM_OBJ";
    static const char S_ZEND_IMPORT_FUNCTION[] = "ZEND_IMPORT_FUNCTION";
    static const char S_ZEND_IMPORT_CLASS[] = "ZEND_IMPORT_CLASS";
    static const char S_ZEND_IMPORT_CONST[] = "ZEND_IMPORT_CONST";
    static const char S_ZEND_ASSIGN_ADD_OBJ[] = "ZEND_ASSIGN_ADD_OBJ";
    static const char S_ZEND_ASSIGN_SUB_OBJ[] = "ZEND_ASSIGN_SUB_OBJ";
    static const char S_ZEND_ASSIGN_MUL_OBJ[] = "ZEND_ASSIGN_MUL_OBJ";
    static const char S_ZEND_ASSIGN_DIV_OBJ[] = "ZEND_ASSIGN_DIV_OBJ";
    static const char S_ZEND_ASSIGN_MOD_OBJ[] = "ZEND_ASSIGN_MOD_OBJ";
    static const char S_ZEND_ASSIGN_SL_OBJ[] = "ZEND_ASSIGN_SL_OBJ";
    static const char S_ZEND_ASSIGN_SR_OBJ[] = "ZEND_ASSIGN_SR_OBJ";
    static const char S_ZEND_ASSIGN_CONCAT_OBJ[] = "ZEND_ASSIGN_CONCAT_OBJ";
    static const char S_ZEND_ASSIGN_BW_OR_OBJ[] = "ZEND_ASSIGN_BW_OR_OBJ";
    static const char S_END_ASSIGN_BW_AND_OBJ[] = "END_ASSIGN_BW_AND_OBJ";
    static const char S_END_ASSIGN_BW_XOR_OBJ[] = "END_ASSIGN_BW_XOR_OBJ";
    static const char S_ZEND_PRE_INC_OBJ[] = "ZEND_PRE_INC_OBJ";
    static const char S_ZEND_PRE_DEC_OBJ[] = "ZEND_PRE_DEC_OBJ";
    static const char S_ZEND_POST_INC_OBJ[] = "ZEND_POST_INC_OBJ";
    static const char S_ZEND_POST_DEC_OBJ[] = "ZEND_POST_DEC_OBJ";
    static const char S_ZEND_ASSIGN_OBJ[] = "ZEND_ASSIGN_OBJ";
    static const char S_ZEND_OP_DATA[] = "ZEND_OP_DATA";
    static const char S_ZEND_MAKE_VAR[] = "ZEND_MAKE_VAR";
    static const char S_ZEND_INSTANCEOF[] = "ZEND_INSTANCEOF";
    static const char S_ZEND_DECLARE_CLASS[] = "ZEND_DECLARE_CLASS";
    static const char S_ZEND_DECLARE_INHERITED_CLASS[] = "ZEND_DECLARE_INHERITED_CLASS";
    static const char S_ZEND_DECLARE_FUNCTION[] = "ZEND_DECLARE_FUNCTION";
    static const char S_ZEND_RAISE_ABSTRACT_ERROR[] = "ZEND_RAISE_ABSTRACT_ERROR";
    static const char S_ZEND_ADD_INTERFACE[] = "ZEND_ADD_INTERFACE";
    static const char S_ZEND_VERIFY_ABSTRACT_CLASS[] = "ZEND_VERIFY_ABSTRACT_CLASS";
    static const char S_ZEND_ASSIGN_DIM[] = "ZEND_ASSIGN_DIM";
    static const char S_ZEND_ISSET_ISEMPTY_PROP_OBJ[] = "ZEND_ISSET_ISEMPTY_PROP_OBJ";
    static const char S_ZEND_HANDLE_EXCEPTION[] = "ZEND_HANDLE_EXCEPTION";
#endif

    // extended opcodes
    static const char S_DARG_LOAD_CONSTANT[] = "DARG_LOAD_CONSTANT";
    static const char S_DARG_INCDEC_VAR[] = "DARG_INCDEC_VAR";

    switch (opcode) {
      case ZEND_NOP:
        return S_ZEND_NOP;
      case ZEND_ADD:
        return S_ZEND_ADD;
      case ZEND_SUB:
        return S_ZEND_SUB;
      case ZEND_MUL:
        return S_ZEND_MUL;
      case ZEND_DIV:
        return S_ZEND_DIV;
      case ZEND_MOD:
        return S_ZEND_MOD;
      case ZEND_SL:
        return S_ZEND_SL;
      case ZEND_SR:
        return S_ZEND_SR;
      case ZEND_CONCAT:
        return S_ZEND_CONCAT;
      case ZEND_BW_OR:
        return S_ZEND_BW_OR;
      case ZEND_BW_AND:
        return S_ZEND_BW_AND;
      case ZEND_BW_XOR:
        return S_ZEND_BW_XOR;
      case ZEND_BW_NOT:
        return S_ZEND_BW_NOT;
      case ZEND_BOOL_NOT:
        return S_ZEND_BOOL_NOT;
      case ZEND_BOOL_XOR:
        return S_ZEND_BOOL_XOR;
      case ZEND_IS_IDENTICAL:
        return S_ZEND_IS_IDENTICAL;
      case ZEND_IS_NOT_IDENTICAL:
        return S_ZEND_IS_NOT_IDENTICAL;
      case ZEND_IS_EQUAL:
        return S_ZEND_IS_EQUAL;
      case ZEND_IS_NOT_EQUAL:
        return S_ZEND_IS_NOT_EQUAL;
      case ZEND_IS_SMALLER:
        return S_ZEND_IS_SMALLER;
      case ZEND_IS_SMALLER_OR_EQUAL:
        return S_ZEND_IS_SMALLER_OR_EQUAL;
      case ZEND_CAST:
        return S_ZEND_CAST;
      case ZEND_QM_ASSIGN:
        return S_ZEND_QM_ASSIGN;
      case ZEND_ASSIGN_ADD:
        return S_ZEND_ASSIGN_ADD;
      case ZEND_ASSIGN_SUB:
        return S_ZEND_ASSIGN_SUB;
      case ZEND_ASSIGN_MUL:
        return S_ZEND_ASSIGN_MUL;
      case ZEND_ASSIGN_DIV:
        return S_ZEND_ASSIGN_DIV;
      case ZEND_ASSIGN_MOD:
        return S_ZEND_ASSIGN_MOD;
      case ZEND_ASSIGN_SL:
        return S_ZEND_ASSIGN_SL;
      case ZEND_ASSIGN_SR:
        return S_ZEND_ASSIGN_SR;
      case ZEND_ASSIGN_CONCAT:
        return S_ZEND_ASSIGN_CONCAT;
      case ZEND_ASSIGN_BW_OR:
        return S_ZEND_ASSIGN_BW_OR;
      case ZEND_ASSIGN_BW_AND:
        return S_ZEND_ASSIGN_BW_AND;
      case ZEND_ASSIGN_BW_XOR:
        return S_ZEND_ASSIGN_BW_XOR;
      case ZEND_PRE_INC:
        return S_ZEND_PRE_INC;
      case ZEND_PRE_DEC:
        return S_ZEND_PRE_DEC;
      case ZEND_POST_INC:
        return S_ZEND_POST_INC;
      case ZEND_POST_DEC:
        return S_ZEND_POST_DEC;
      case ZEND_ASSIGN:
        return S_ZEND_ASSIGN;
      case ZEND_ASSIGN_REF:
        return S_ZEND_ASSIGN_REF;
      case ZEND_ECHO:
        return S_ZEND_ECHO;
      case ZEND_PRINT:
        return S_ZEND_PRINT;
      case ZEND_JMP:
        return S_ZEND_JMP;
      case ZEND_JMPZ:
        return S_ZEND_JMPZ;
      case ZEND_JMPNZ:
        return S_ZEND_JMPNZ;
      case ZEND_JMPZNZ:
        return S_ZEND_JMPZNZ;
      case ZEND_JMPZ_EX:
        return S_ZEND_JMPZ_EX;
      case ZEND_JMPNZ_EX:
        return S_ZEND_JMPNZ_EX;
      case ZEND_CASE:
        return S_ZEND_CASE;
      case ZEND_SWITCH_FREE:
        return S_ZEND_SWITCH_FREE;
      case ZEND_BRK:
        return S_ZEND_BRK;
      case ZEND_CONT:
        return S_ZEND_CONT;
      case ZEND_BOOL:
        return S_ZEND_BOOL;
      case ZEND_INIT_STRING:
        return S_ZEND_INIT_STRING;
      case ZEND_ADD_CHAR:
        return S_ZEND_ADD_CHAR;
      case ZEND_ADD_STRING:
        return S_ZEND_ADD_STRING;
      case ZEND_ADD_VAR:
        return S_ZEND_ADD_VAR;
      case ZEND_BEGIN_SILENCE:
        return S_ZEND_BEGIN_SILENCE;
      case ZEND_END_SILENCE:
        return S_ZEND_END_SILENCE;
      case ZEND_INIT_FCALL_BY_NAME:
        return S_ZEND_INIT_FCALL_BY_NAME;
      case ZEND_DO_FCALL:
        return S_ZEND_DO_FCALL;
      case ZEND_DO_FCALL_BY_NAME:
        return S_ZEND_DO_FCALL_BY_NAME;
      case ZEND_RETURN:
        return S_ZEND_RETURN;
      case ZEND_RECV:
        return S_ZEND_RECV;
      case ZEND_RECV_INIT:
        return S_ZEND_RECV_INIT;
      case ZEND_SEND_VAL:
        return S_ZEND_SEND_VAL;
      case ZEND_SEND_VAR:
        return S_ZEND_SEND_VAR;
      case ZEND_SEND_REF:
        return S_ZEND_SEND_REF;
      case ZEND_NEW:
        return S_ZEND_NEW;
#ifndef ZEND_ENGINE_2        
      case ZEND_JMP_NO_CTOR:
        return S_ZEND_JMP_NO_CTOR;
#endif
      case ZEND_FREE:
        return S_ZEND_FREE;
      case ZEND_INIT_ARRAY:
        return S_ZEND_INIT_ARRAY;
      case ZEND_ADD_ARRAY_ELEMENT:
        return S_ZEND_ADD_ARRAY_ELEMENT;
      case ZEND_INCLUDE_OR_EVAL:
        return S_ZEND_INCLUDE_OR_EVAL;
      case ZEND_UNSET_VAR:
        return S_ZEND_UNSET_VAR;
#ifndef ZEND_ENGINE_2        
      case ZEND_UNSET_DIM_OBJ:
        return S_ZEND_UNSET_DIM_OBJ;
      case ZEND_ISSET_ISEMPTY:
        return S_ZEND_ISSET_ISEMPTY;
#endif        
      case ZEND_FE_RESET:
        return S_ZEND_FE_RESET;
      case ZEND_FE_FETCH:
        return S_ZEND_FE_FETCH;
      case ZEND_EXIT:
        return S_ZEND_EXIT;
      case ZEND_FETCH_R:
        return S_ZEND_FETCH_R;
      case ZEND_FETCH_DIM_R:
        return S_ZEND_FETCH_DIM_R;
      case ZEND_FETCH_OBJ_R:
        return S_ZEND_FETCH_OBJ_R;
      case ZEND_FETCH_W:
        return S_ZEND_FETCH_W;
      case ZEND_FETCH_DIM_W:
        return S_ZEND_FETCH_DIM_W;
      case ZEND_FETCH_OBJ_W:
        return S_ZEND_FETCH_OBJ_W;
      case ZEND_FETCH_RW:
        return S_ZEND_FETCH_RW;
      case ZEND_FETCH_DIM_RW:
        return S_ZEND_FETCH_DIM_RW;
      case ZEND_FETCH_OBJ_RW:
        return S_ZEND_FETCH_OBJ_RW;
      case ZEND_FETCH_IS:
        return S_ZEND_FETCH_IS;
      case ZEND_FETCH_DIM_IS:
        return S_ZEND_FETCH_DIM_IS;
      case ZEND_FETCH_OBJ_IS:
        return S_ZEND_FETCH_OBJ_IS;
      case ZEND_FETCH_FUNC_ARG:
        return S_ZEND_FETCH_FUNC_ARG;
      case ZEND_FETCH_DIM_FUNC_ARG:
        return S_ZEND_FETCH_DIM_FUNC_ARG;
      case ZEND_FETCH_OBJ_FUNC_ARG:
        return S_ZEND_FETCH_OBJ_FUNC_ARG;
      case ZEND_FETCH_UNSET:
        return S_ZEND_FETCH_UNSET;
      case ZEND_FETCH_DIM_UNSET:
        return S_ZEND_FETCH_DIM_UNSET;
      case ZEND_FETCH_OBJ_UNSET:
        return S_ZEND_FETCH_OBJ_UNSET;
      case ZEND_FETCH_DIM_TMP_VAR:
        return S_ZEND_FETCH_DIM_TMP_VAR;
      case ZEND_FETCH_CONSTANT:
        return S_ZEND_FETCH_CONSTANT;
#ifndef ZEND_ENGINE_2        
      case ZEND_DECLARE_FUNCTION_OR_CLASS:
        return S_ZEND_DECLARE_FUNCTION_OR_CLASS;
#endif
      case ZEND_EXT_STMT:
        return S_ZEND_EXT_STMT;
      case ZEND_EXT_FCALL_BEGIN:
        return S_ZEND_EXT_FCALL_BEGIN;
      case ZEND_EXT_FCALL_END:
        return S_ZEND_EXT_FCALL_END;
      case ZEND_EXT_NOP:
        return S_ZEND_EXT_NOP;
      case ZEND_TICKS:
        return S_ZEND_TICKS;
      case ZEND_SEND_VAR_NO_REF:
        return S_ZEND_SEND_VAR_NO_REF;

#ifdef ZEND_ENGINE_2
      case ZEND_CATCH:
        return S_ZEND_CATCH;
      case ZEND_THROW:
        return S_ZEND_THROW;
      case ZEND_FETCH_CLASS:
        return S_ZEND_FETCH_CLASS;
      case ZEND_CLONE:
        return S_ZEND_CLONE;
/*      case ZEND_INIT_CTOR_CALL: */
/*        return S_ZEND_INIT_CTOR_CALL; */
      case ZEND_INIT_METHOD_CALL:
        return S_ZEND_INIT_METHOD_CALL;
      case ZEND_INIT_STATIC_METHOD_CALL:
        return S_ZEND_INIT_STATIC_METHOD_CALL;
      case ZEND_ISSET_ISEMPTY_VAR:
        return S_ZEND_ISSET_ISEMPTY_VAR;
      case ZEND_ISSET_ISEMPTY_DIM_OBJ:
        return S_ZEND_ISSET_ISEMPTY_DIM_OBJ;
/*      case ZEND_IMPORT_FUNCTION:*/
/*        return S_ZEND_IMPORT_FUNCTION;*/
/*      case ZEND_IMPORT_CLASS:*/
/*        return S_ZEND_IMPORT_CLASS;*/
/*      case ZEND_IMPORT_CONST:*/
/*        return S_ZEND_IMPORT_CONST;*/

#ifdef ZEND_ENGINE_2_1
      case ZEND_ASSIGN_ADD_OBJ:
        return S_ZEND_ASSIGN_ADD_OBJ;
      case ZEND_ASSIGN_SUB_OBJ:
        return S_ZEND_ASSIGN_SUB_OBJ;
      case ZEND_ASSIGN_MUL_OBJ:
        return S_ZEND_ASSIGN_MUL_OBJ;
      case ZEND_ASSIGN_DIV_OBJ:
        return S_ZEND_ASSIGN_DIV_OBJ;
      case ZEND_ASSIGN_MOD_OBJ:
        return S_ZEND_ASSIGN_MOD_OBJ;
      case ZEND_ASSIGN_SL_OBJ:
        return S_ZEND_ASSIGN_SL_OBJ;
      case ZEND_ASSIGN_SR_OBJ:
        return S_ZEND_ASSIGN_SR_OBJ;
      case ZEND_ASSIGN_CONCAT_OBJ:
        return S_ZEND_ASSIGN_CONCAT_OBJ;
      case ZEND_ASSIGN_BW_OR_OBJ:
        return S_ZEND_ASSIGN_BW_OR_OBJ;
      case END_ASSIGN_BW_AND_OBJ:
        return S_END_ASSIGN_BW_AND_OBJ;
      case END_ASSIGN_BW_XOR_OBJ:
        return S_END_ASSIGN_BW_XOR_OBJ;
#endif
      case ZEND_PRE_INC_OBJ:
        return S_ZEND_PRE_INC_OBJ;
      case ZEND_PRE_DEC_OBJ:
        return S_ZEND_PRE_DEC_OBJ;
      case ZEND_POST_INC_OBJ:
        return S_ZEND_POST_INC_OBJ;
      case ZEND_POST_DEC_OBJ:
        return S_ZEND_POST_DEC_OBJ;
      case ZEND_ASSIGN_OBJ:
        return S_ZEND_ASSIGN_OBJ;
      case ZEND_OP_DATA:
        return S_ZEND_OP_DATA;
#ifdef ZEND_ENGINE_2_1
      case ZEND_MAKE_VAR:
        return S_ZEND_MAKE_VAR;
#endif
      case ZEND_INSTANCEOF:
        return S_ZEND_INSTANCEOF;
      case ZEND_DECLARE_CLASS:
        return S_ZEND_DECLARE_CLASS;
      case ZEND_DECLARE_INHERITED_CLASS:
        return S_ZEND_DECLARE_INHERITED_CLASS;
      case ZEND_DECLARE_FUNCTION:
        return S_ZEND_DECLARE_FUNCTION;
      case ZEND_RAISE_ABSTRACT_ERROR:
        return S_ZEND_RAISE_ABSTRACT_ERROR;
      case ZEND_ADD_INTERFACE:
        return S_ZEND_ADD_INTERFACE;
      case ZEND_VERIFY_ABSTRACT_CLASS:
        return S_ZEND_VERIFY_ABSTRACT_CLASS;
      case ZEND_ASSIGN_DIM:
        return S_ZEND_ASSIGN_DIM;
      case ZEND_ISSET_ISEMPTY_PROP_OBJ:
        return S_ZEND_ISSET_ISEMPTY_PROP_OBJ;
      case ZEND_HANDLE_EXCEPTION:
        return S_ZEND_HANDLE_EXCEPTION;
#endif
        
      default:
        fprintf( stderr, "Unknown opcode 0x%04x\n", opcode ); 
        assert(0);
    }

    return NULL; /* Keep the compiler happy... */
}

void dump(zend_op_array *op_array)
{
  int i;
  return;
	if(op_array->filename) {
		fprintf(stderr, "Ops for %s\n", op_array->filename);
  }
  
	if(op_array->function_name) {
		fprintf(stderr, "Ops for func %s\n", op_array->function_name);
  }
  
  fprintf(stderr, "Starting at %p\n", op_array->opcodes);  
  
  for(i = 0; i < op_array->last; i++) {
    zend_op * zo = &(op_array->opcodes[i]); 
    fprintf(stderr, "%s %s", optimizer_zend_util_opcode_to_string(op_array->opcodes[i].opcode), op_array->opcodes[i].extended_value);

    switch (zo->opcode)
    {
      case ZEND_JMP:
        fprintf(stderr, " 0x%04x\n", zo->op1.u.opline_num);
        break;
      case ZEND_JMPZ:
      case ZEND_JMPNZ:
      case ZEND_JMPZ_EX:
      case ZEND_JMPNZ_EX:
        fprintf(stderr, " 0x%04x\n", zo->op2.u.opline_num);
        break;
      default:
        fprintf(stderr," \n", zo->op1, zo->op2);
        break;
    }
  }

  fprintf(stderr, "\n\n\n");
}
