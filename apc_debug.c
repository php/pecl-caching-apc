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
    static const char S_ZEND_ISSET_ISEMPTY[] = "ISSET_ISEMPTY";
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
    static const char S_ZEND_DECLARE_FUNCTION_OR_CLASS[] = "DECLARE_FUNCTION_OR_CLASS";
    static const char S_ZEND_EXT_STMT[] = "EXT_STMT";
    static const char S_ZEND_EXT_FCALL_BEGIN[] = "EXT_FCALL_BEGIN";
    static const char S_ZEND_EXT_FCALL_END[] = "EXT_FCALL_END";
    static const char S_ZEND_EXT_NOP[] = "EXT_NOP";
    static const char S_ZEND_TICKS[] = "TICKS";
    static const char S_ZEND_SEND_VAR_NO_REF[] = "SEND_VAR_NO_REF";

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
      case ZEND_JMP_NO_CTOR:
        return S_ZEND_JMP_NO_CTOR;
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
      case ZEND_UNSET_DIM_OBJ:
        return S_ZEND_UNSET_DIM_OBJ;
      case ZEND_ISSET_ISEMPTY:
        return S_ZEND_ISSET_ISEMPTY;
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
      case ZEND_DECLARE_FUNCTION_OR_CLASS:
        return S_ZEND_DECLARE_FUNCTION_OR_CLASS;
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

      default:
        assert(0);
    }
}

void dump(zend_op_array *op_array)
{
    int i;
	if(op_array->filename) 
		fprintf(stderr, "Ops for %s<br>\n", op_array->filename);
	if(op_array->function_name) 
		fprintf(stderr, "Ops for func %s<br>\n", op_array->function_name);
    for(i = 0; i < op_array->last; i++) {
        fprintf(stderr, "OP %d: %s<br>\n", i, 
                optimizer_zend_util_opcode_to_string(op_array->opcodes[i].opcode)) ;
    }
}
