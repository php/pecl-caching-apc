/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2005 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  |          George Schlossnagle <george@omniti.com>                     |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id$ */

#include "apc_php.h"
#include "apc_pair.h"
#include <assert.h>

/* {{{ rewrite helper functions */

/* cleans up and deallocates a znode */
static void clear_znode(znode* n)
{
    if (n->op_type == IS_CONST) {
        zval_dtor(&n->u.constant);
    }
    SET_UNUSED(*n);
}

/* overwrites dst with contents of src, clears src */
static void move_znode(znode* dst, znode* src)
{
    memcpy(dst, src, sizeof(znode));
    SET_UNUSED(*src);
}

/* cleans up and deallocates a zend_op */
static void clear_zend_op(zend_op* op)
{
    op->opcode = ZEND_NOP;
    clear_znode(&op->result);
    clear_znode(&op->op1);
    clear_znode(&op->op2);
}

/* overwrites dst with contents of src, clears src */
static void move_zend_op(zend_op* dst, zend_op* src)
{
    clear_zend_op(dst);
    dst->opcode = src->opcode;
    src->opcode = ZEND_NOP;
    dst->extended_value = src->extended_value;
    dst->lineno = src->lineno;
    move_znode(&dst->result, &src->result);
    move_znode(&dst->op1, &src->op1);
    move_znode(&dst->op2, &src->op2);
}

/* op is a branch instruction that branches to old, change it to new */
static void change_branch_target(zend_op* op, int old, int new)
{
    switch (op->opcode) {
      case ZEND_JMP:
        assert(op->op1.u.opline_num == old);
        op->op1.u.opline_num = new;
        break;
      case ZEND_JMPZ:
      case ZEND_JMPNZ:
      case ZEND_JMPZ_EX:
      case ZEND_JMPNZ_EX:
#ifndef ZEND_ENGINE_2
      case ZEND_JMP_NO_CTOR:
#endif
      case ZEND_FE_FETCH:
        assert(op->op2.u.opline_num == old);
        op->op2.u.opline_num = new;
        break;
      case ZEND_JMPZNZ:
        if (op->op2.u.opline_num == old) {
            op->op2.u.opline_num = new;
        }
        else if (op->extended_value == old) {
            op->extended_value = new;
        }
        else {
            assert(0);
        }
        break;
      default:
        assert(0);
    }
}

/* returns true if opcode is a branch instruction */
static int is_branch_op(int opcode)
{
    switch (opcode) {
      case ZEND_JMP:
      case ZEND_JMPZ:
      case ZEND_JMPNZ:
      case ZEND_JMPZ_EX:
      case ZEND_JMPNZ_EX:
#ifndef ZEND_ENGINE_2
      case ZEND_JMP_NO_CTOR:
#endif
      case ZEND_FE_FETCH:
      case ZEND_JMPZNZ:
        return 1;
    }
    return 0;
}

/* squeezes no-ops out of the zend_op array */
static int compress_ops(zend_op_array* op_array, Pair** jumps)
{
    int i, j, k, num_ops;

    zend_op *ops = op_array->opcodes;
    num_ops = op_array->last;

    for (i = 0, j = 0; j < num_ops; i++, j++) {
        if (ops[i].opcode == ZEND_NOP) {
            Pair* branches;

            /* search for a non-noop op, updating branch targets as we go */
            for (;;) {
                /* update branch targets */
                for (branches = jumps[j]; branches; branches = cdr(branches)) {
                    change_branch_target(&ops[car(branches)], j, i);
                }
                for (k = 0; k < op_array->last_brk_cont; k++) {
                    if(op_array->brk_cont_array[k].brk == j) {
                        op_array->brk_cont_array[k].brk = i;
                    }
                    if(op_array->brk_cont_array[k].cont == j) {
                        op_array->brk_cont_array[k].cont = i;
                    }
                }
                /* quit when we've found a non-noop instruction */
                if (ops[j].opcode != ZEND_NOP) {
                    break;
                }
                if (++j >= num_ops) {
                    j--;
                    break;
                }
            }

            /* update jump table IFF ops[j] is a branch */
            if (is_branch_op(ops[j].opcode)) {
                int x;
                for (x = 0; x < num_ops; x++) {
                    Pair* p = jumps[x];
                    for (; p; p = cdr(p)) {
                        if (car(p) == j) {
                            pair_set_car(p, i);
                        }
                    }
                }
            }

            move_zend_op(ops + i, ops + j);
        }
    }
    while (--j > i) {
        clear_zend_op(ops + j);
    }
    return i;
}

/* op must be 'pure' and have two constant operands; returns its value */
static zval* compute_result_of_constant_op(zend_op* op)
{
    zval* result = 0;
    int (*binary_op)(zval*, zval*, zval* TSRMLS_DC) = 0;
	TSRMLS_FETCH();

    // TODO: add cases for other deterministic binary ops!
    // TODO: extend to this work with the few unary ops

    switch (op->opcode) {
      case ZEND_ADD:
        binary_op = add_function;
        break;
      case ZEND_SUB:
        binary_op = sub_function;
        break;
      case ZEND_MUL:
        binary_op = mul_function;
        break;
      case ZEND_DIV:
        binary_op = div_function;
        break;
      case ZEND_MOD:
        binary_op = mod_function;
        break;
      case ZEND_SL:
        binary_op = shift_left_function;
        break;
      case ZEND_SR:
        binary_op = shift_right_function;
        break;
      case ZEND_CONCAT:
        binary_op = concat_function;
        break;
    }

    if (binary_op) {
        ALLOC_INIT_ZVAL(result);
        binary_op(result, &op->op1.u.constant, &op->op2.u.constant TSRMLS_CC);
        return result;
    }
    
    assert(0);  /* no function found */
    return NULL;
}

/* }}} */

/* {{{ rewrite functions */

static void rewrite_inc(zend_op* ops, Pair* p)
{
    assert(pair_length(p) == 3);
    switch (ops[cadr(p)].opcode) {
    case ZEND_POST_INC:
        ops[cadr(p)].opcode = ZEND_PRE_INC;
        ops[cadr(p)].result.op_type = IS_VAR;
        ops[cadr(p)].result.u.EA.type |= EXT_TYPE_UNUSED;
        break; 
    case ZEND_POST_DEC:
        ops[cadr(p)].opcode = ZEND_PRE_DEC;
        ops[cadr(p)].result.op_type = IS_VAR;
        ops[cadr(p)].result.u.EA.type |= EXT_TYPE_UNUSED;
        break;
    default:
        assert(0);
        break;
    }

    clear_zend_op(&ops[caddr(p)]);
}


static void rewrite_const_cast(zend_op* ops, Pair* p)
{
    zend_op* cur;
    zval convert;

    cur = &ops[car(p)];

    memcpy(&convert, &cur->op1.u.constant, sizeof(zval));
    switch (cur->extended_value) {
      case IS_NULL:
        convert_to_null(&convert);
        break;
      case IS_BOOL:
        convert_to_boolean(&convert);
        break;
      case IS_LONG:
        convert_to_long(&convert);
        break;
      case IS_DOUBLE:
        convert_to_double(&convert);
        break;
      case IS_STRING:
        convert_to_string(&convert);
        break;
    }
    /* zval_dtor(&cur->op1.u.constant); */
    cur->opcode = ZEND_QM_ASSIGN;
    cur->extended_value = 0;
    cur->op1.op_type = IS_CONST;
    memcpy(&cur->op1.u.constant, &convert, sizeof(zval));
    cur->op2.op_type = IS_UNUSED;
}

static void rewrite_add_string(zend_op* ops, Pair* p)
{
    zend_op* first;
    zend_op* second;
    
    first = &ops[car(p)];
    second = &ops[cadr(p)];

    if (second->opcode == ZEND_ADD_STRING) {
        add_string_to_string(&first->op2.u.constant, &first->op2.u.constant, &second->op2.u.constant);
    } else {
        add_char_to_string(&first->op2.u.constant, &first->op2.u.constant, &second->op2.u.constant);
    }
    
    clear_zend_op(second);
}

static void rewrite_constant_fold(zend_op* ops, Pair *p)
{
    zval *result;
    zend_op *const_op = &ops[car(p)];
    zend_op *fetch_op = &ops[cadr(p)];

    result = compute_result_of_constant_op(const_op);
    if(const_op->result.u.var == fetch_op->op1.u.var) {
        fetch_op->op1.op_type = IS_CONST;
        fetch_op->op1.u.constant = *result;
        /* zval_copy_ctor(&fetch_op->op1.u.constant); */
    }
    else if(const_op->result.u.var == fetch_op->op2.u.var) {
        fetch_op->op2.op_type = IS_CONST;
        fetch_op->op2.u.constant = *result;
        /* zval_copy_ctor(&fetch_op->op2.u.constant); */
    }
    clear_zend_op(const_op);
}

static void rewrite_print(zend_op* ops, Pair* p)
{
    assert(pair_length(p) == 2);
    ops[car(p)].opcode = ZEND_ECHO;
    clear_zend_op(&ops[cadr(p)]);  // don't need this anymore
}

static void rewrite_multiple_echo(zend_op* ops, Pair* p)
{
    add_string_to_string(&ops[car(p)].op1.u.constant, &ops[car(p)].op1.u.constant, &ops[cadr(p)].op1.u.constant);
    clear_zend_op(&ops[cadr(p)]);
}

static void rewrite_fcall(zend_op* ops, Pair* p) 
{
    assert(pair_length(p) == 2);
    clear_zend_op(ops + car(p));
    ops[cadr(p)].opcode = ZEND_DO_FCALL;
}

static void rewrite_is_equal_bool(zend_op* ops, Pair* p)
{
    zend_op* op;

#define DETERMINE_VALUE(c, v) \
    (c = (v).u.constant.value.lval ? \
     (c == ZEND_IS_EQUAL ? ZEND_BOOL : ZEND_BOOL_NOT) : \
     (c == ZEND_IS_EQUAL ? ZEND_BOOL_NOT : ZEND_BOOL))
    
    op = &ops[car(p)];
    if (op->op1.op_type == IS_CONST && op->op1.u.constant.type == IS_BOOL) {
        DETERMINE_VALUE(op->opcode, op->op1);
        memcpy(&op->op1, &op->op2, sizeof(znode));
    } else {
        DETERMINE_VALUE(op->opcode, op->op2);
    }
    op->op2.op_type = IS_UNUSED;        

#undef DETERMINE_VALUE
}

static void rewrite_needless_bool(zend_op* ops, Pair* p)
{
    clear_zend_op(&ops[car(p)]);
    clear_zend_op(&ops[cadr(p)]);
}

/* }}} */

/* {{{ peephole helper functions */

/* scans for the next opcode, skipping no-ops */
static int next_op(zend_op* ops, int i, int num_ops)
{
    while (++i < num_ops) {
        if (ops[i].opcode != ZEND_NOP && ops[i].opcode != ZEND_EXT_NOP) {
            break;
        }
    }
    return i;
}

/* convert switch statement to use JMPs.  this is easier than handling them separately. */  

static void convert_switch(zend_op_array *op_array) 
{
    int i, original_nest_levels, nest_levels = 0;
    int array_offset = 0;
    for (i = 0; i < op_array->last; i++) {
        zend_op *opline = &op_array->opcodes[i];
        zend_brk_cont_element *jmp_to;
        if(opline->opcode != ZEND_BRK && opline->opcode != ZEND_CONT) {
            continue;
        }
        if(opline->op2.op_type == IS_CONST && 
           opline->op2.u.constant.type == IS_LONG) {
            nest_levels = opline->op2.u.constant.value.lval;
            original_nest_levels = nest_levels;
        } else {
            continue;
        }
        array_offset = opline->op1.u.opline_num;
        do {
            if (array_offset < 0) {
                zend_error(E_ERROR, "Cannot break/continue %d level (%s#%d)", 
                           original_nest_levels,op_array->filename,opline->lineno);
            }
            jmp_to = &op_array->brk_cont_array[array_offset];
            if (nest_levels>1) {
                zend_op *brk_opline = &op_array->opcodes[jmp_to->brk];
                switch (brk_opline->opcode) {
                  case ZEND_SWITCH_FREE:
                    break;
                  case ZEND_FREE:
                    break;
                }
            }
            array_offset = jmp_to->parent;
        } while (--nest_levels > 0);

        switch(opline->opcode) {
          case ZEND_BRK:
            opline->op1.u.opline_num = jmp_to->brk;
            break;
          case ZEND_CONT:
            opline->op1.u.opline_num = jmp_to->cont;
            break;
        }
        opline->op2.op_type = IS_UNUSED;
        opline->opcode = ZEND_JMP;
    }
}


/* returns list of potential branch targets for this op */
static Pair* extract_branch_targets(zend_op_array* op_array, int i)
{
    zend_op *op = &op_array->opcodes[i];
    switch (op->opcode) {
      case ZEND_JMP:
        return cons(op->op1.u.opline_num, 0);
      case ZEND_JMPZ:
      case ZEND_JMPNZ:
      case ZEND_JMPZ_EX:
      case ZEND_JMPNZ_EX:
#ifndef ZEND_ENGINE_2
      case ZEND_JMP_NO_CTOR:
#endif
      case ZEND_FE_FETCH:
        return cons(op->op2.u.opline_num, 0);
      case ZEND_JMPZNZ:
        return cons(op->op2.u.opline_num, cons(op->extended_value, 0));
    }
    return 0;
}

/* retval[i] -> list of instruction indices that may branch to i */
static Pair** build_jump_array(zend_op_array* op_array)
{
    Pair** jumps;
    int i, num_ops;
    num_ops = op_array->last;

    jumps = (Pair**) malloc(num_ops * sizeof(Pair*));
    memset(jumps, 0, num_ops * sizeof(Pair*));
    for (i = 0; i < num_ops; i++) {
        Pair* targets = extract_branch_targets(op_array, i);
        while (targets) {
            jumps[car(targets)] = cons(i, jumps[car(targets)]);
            targets = cdr(targets);
        }
    }
    return jumps;
}

/* deallocates structure created by build_jump_array */
static void destroy_jump_array(Pair** p, int num_ops)
{
    int i;

    for (i = 0; i < num_ops; i++) {
        pair_destroy(p[i]);
    }
    free(p);
}

/* returns true if any of the indices in 'ops' are targets of a branch */
static int are_branch_targets(Pair* ops, Pair** jumps)
{
    return ops
        ? jumps[car(ops)] ? 1 : are_branch_targets(cdr(ops), jumps)
        : 0;
}

/* returns true if op is 'pure' and all its operands are constants */
static int is_constant_op(zend_op* op)
{
    switch (op->opcode) {
      case ZEND_ADD:
      case ZEND_SUB:
      case ZEND_MUL:
      case ZEND_DIV:
      case ZEND_MOD:
      case ZEND_SL:
      case ZEND_SR:
      case ZEND_CONCAT:
        return op->op1.op_type == IS_CONST && op->op2.op_type == IS_CONST;
    }
    return 0;
}

/* returns true if op is a deterministic binary instr. */
static int is_pure_binary_op(zend_op* op)
{
    switch (op->opcode) {
      case ZEND_ADD:
      case ZEND_SUB:
      case ZEND_MUL:
      case ZEND_DIV:
      case ZEND_MOD:
      case ZEND_SL:
      case ZEND_SR:
      case ZEND_CONCAT:
        return 1;
    }
    return 0;
}

/* }}} */

/* {{{ peephole match functions */

static Pair* peephole_const_cast(zend_op* ops, int i, int num_ops)
{
    if (ops[i].opcode == ZEND_CAST &&
        ops[i].op1.op_type == IS_CONST &&
        ops[i].result.op_type == IS_TMP_VAR &&
        ops[i].extended_value != IS_ARRAY &&
        ops[i].extended_value != IS_OBJECT &&
        ops[i].extended_value != IS_RESOURCE) {
        return cons(i, 0);
    }

    return NULL;
}

static Pair* peephole_post_inc(zend_op* ops, int i, int num_ops)
{
    int j;      /* next op after i */
    int k;      /* next op after j */

    j = next_op(ops, i, num_ops);
    k = next_op(ops, j, num_ops);

    if (j == num_ops || k == num_ops) {
        return 0;   /* not enough ops left to match */
    }

    /* Try to match a (FETCH_RW, POST_INC, FREE) tuple. */

    if (ops[i].opcode == ZEND_FETCH_RW  && 
        ops[j].opcode == ZEND_POST_INC  &&
        ops[k].opcode == ZEND_FREE)
    {
        /* Insure that the fetch op reads into a register subsequently
         * incremented by the increment op, and that the free op subsequently
         * frees the temporary result used by the incr op. Basically, we're
         * just going to be SUPER careful that the rewrite is safe. */

        zend_op* load_op = &ops[i];
        zend_op* incr_op = &ops[j];
        zend_op* free_op = &ops[k];

        if (
#ifdef ZEND_ENGINE_2
            load_op->op2.u.EA.type == ZEND_FETCH_LOCAL   &&
#else
            load_op->op2.u.fetch_type == ZEND_FETCH_LOCAL   &&
#endif
            load_op->op1.u.constant.type == IS_STRING       &&
            load_op->op1.op_type == IS_CONST                &&
            load_op->result.op_type != IS_CONST             &&
            incr_op->op1.op_type != IS_CONST                &&
            load_op->result.u.var == incr_op->op1.u.var     &&
            incr_op->result.op_type != IS_CONST             &&
            free_op->op1.op_type != IS_CONST                &&
            incr_op->result.u.var == free_op->op1.u.var)
        {
            return cons(i, cons(j, cons(k, 0)));
        }
    }

    return 0;
}

static Pair* peephole_inc(zend_op* ops, int i, int num_ops)
{
    int j;      /* next op after i */
    int k;      /* next op after j */

    j = next_op(ops, i, num_ops);
    k = next_op(ops, j, num_ops);

    if (j == num_ops || k == num_ops) {
        return 0;   /* not enough ops left to match */
    }

    /* Try to match a (FETCH_RW, POST_INC, FREE) tuple. */

    if (ops[i].opcode == ZEND_FETCH_RW  && 
        (ops[j].opcode == ZEND_POST_INC  || 
         ops[j].opcode == ZEND_POST_DEC ) &&
        ops[k].opcode == ZEND_FREE)
    {
        /* Insure that the fetch op reads into a register subsequently
         * incremented by the increment op, and that the free op subsequently
         * frees the temporary result used by the incr op. Basically, we're
         * just going to be SUPER careful that the rewrite is safe. */

        zend_op* load_op = &ops[i];
        zend_op* incr_op = &ops[j];
        zend_op* free_op = &ops[k];

        if (
#ifdef ZEND_ENGINE_2
            load_op->op2.u.EA.type == ZEND_FETCH_LOCAL   &&
#else
            load_op->op2.u.fetch_type == ZEND_FETCH_LOCAL   &&
#endif
            load_op->op1.u.constant.type == IS_STRING       &&
            load_op->op1.op_type == IS_CONST                &&
            load_op->result.op_type != IS_CONST             &&
            incr_op->op1.op_type != IS_CONST                &&
            load_op->result.u.var == incr_op->op1.u.var     &&
            incr_op->result.op_type != IS_CONST             &&
            free_op->op1.op_type != IS_CONST                &&
            incr_op->result.u.var == free_op->op1.u.var)
        {
            return cons(i, cons(j, cons(k, 0)));
        }
    }

    return 0;
}


static Pair* peephole_multiple_echo(zend_op* ops, int i, int num_ops)
{
    int j;

    j = next_op(ops, i, num_ops);

    if (j == num_ops) {
        return NULL;
    }

    if (ops[i].opcode == ZEND_ECHO && ops[i].op1.op_type == IS_CONST &&
        ops[j].opcode == ZEND_ECHO && ops[j].op1.op_type == IS_CONST) {
        return cons(i, cons(j, 0));
    }

    return NULL;
}

static Pair* peephole_print(zend_op* ops, int i, int num_ops)
{
    int j;      /* next op after i */

    j = next_op(ops, i, num_ops);

    if (j == num_ops) {
        return 0;   /* not enough ops left to match */
    }

    /* Try to match a (PRINT, FREE) tuple. */

    if (ops[i].opcode == ZEND_PRINT  &&
        ops[j].opcode == ZEND_FREE)
    {
        return cons(i, cons(j, 0));
    }
    return 0;
}

static Pair* peephole_constant_fold(zend_op* ops, int i, int num_ops)
{
    int j;  /* next op using the result of the ocnstant op */
    int tmp_var_result;

    if(!is_constant_op(ops + i)) {
        return 0;
    } 
    tmp_var_result = ops[i].result.u.var;
    for( j = i+1; j < num_ops; j++) {
        if (ops[j].op1.op_type == IS_TMP_VAR && 
            ops[j].op1.u.var == tmp_var_result) {
            return cons(i, cons(j, 0));
        }
        if (ops[j].op2.op_type == IS_TMP_VAR && 
            ops[j].op2.u.var == tmp_var_result) {
            return cons(i, cons(j, 0));
        }
    }
    return 0;
}

static Pair* peephole_add_string(zend_op* ops, int i, int num_ops)
{
    int j;      /* next op after i */
/*    Pair *p = NULL;*/
    
    j = next_op(ops, i, num_ops);
    if (j == num_ops) {
        return NULL;
    }
    
    if ((ops[i].opcode == ZEND_ADD_STRING &&
         ops[i].result.op_type == IS_TMP_VAR &&
         ops[i].op1.op_type == IS_TMP_VAR &&
         ops[i].op2.op_type == IS_CONST) && 
        ((ops[j].opcode == ZEND_ADD_STRING || ops[j].opcode == ZEND_ADD_CHAR) &&
         ops[j].result.op_type == IS_TMP_VAR &&
         ops[j].op1.op_type == IS_TMP_VAR &&
         ops[j].op2.op_type == IS_CONST)) {
        return cons(i, cons(j, 0));
    }
    return NULL;
}

static Pair* peephole_fcall(zend_op* ops, int i, int num_ops)
{
    int j;  /* next op after i */
    j = next_op(ops, i, num_ops);

    if (j == num_ops) {
        return 0;   /* not enough ops left to match */
    }

    if(ops[i].opcode == ZEND_INIT_FCALL_BY_NAME &&
       ops[i].op1.op_type == IS_UNUSED &&
       ops[i].op2.op_type == IS_CONST &&
       ops[j].opcode == ZEND_DO_FCALL_BY_NAME &&
       ops[j].op1.op_type == IS_CONST &&
       ops[j].extended_value == 0 && 
       !zend_binary_zval_strcmp(&ops[i].op2.u.constant, &ops[j].op1.u.constant)) {

       return cons(i, cons(j,0));
    }
    return 0;
}

static Pair* peephole_is_equal_bool(zend_op* ops, int i, int num_ops)
{
    if ((ops[i].opcode == ZEND_IS_EQUAL || ops[i].opcode == ZEND_IS_NOT_EQUAL) && 
        (ops[i].op1.op_type == IS_CONST || ops[i].op2.op_type == IS_CONST) &&
        (ops[i].op1.u.constant.type == IS_BOOL || ops[i].op2.u.constant.type == IS_BOOL)) {
        return cons(i, 0);
    }
    
    return NULL;
}

static Pair* peephole_needless_bool(zend_op* ops, int i, int num_ops)
{
    int j;

    j = next_op(ops, i, num_ops);
    if (j == num_ops) {
        return NULL;
    }

    /* Try and match a (BOOL, FREE) tuple */
    if (ops[i].opcode == ZEND_BOOL && ops[j].opcode == ZEND_FREE) {
        return cons(i, cons(j, 0));
    }
    
    return NULL;
}

/* }}} */

#ifdef ZEND_ENGINE_2_1
/* we need these unless the apc optimizer is installed as op_array_handler */
/* {{{ apc_restore_opline_num */
static void apc_restore_opline_num(zend_op_array *op_array)
{
    zend_op *opline, *end;
    opline = op_array->opcodes;
    end = opline + op_array->last;

    while (opline < end) {
        switch (opline->opcode) {
            case ZEND_JMP:
                opline->op1.u.opline_num = opline->op1.u.jmp_addr - op_array->opcodes;
                break;
            case ZEND_JMPZ:
            case ZEND_JMPNZ:
            case ZEND_JMPZ_EX:
            case ZEND_JMPNZ_EX:
                opline->op2.u.opline_num = opline->op2.u.jmp_addr - op_array->opcodes;
                break;
        }
        opline++;
    }
}
/* }}} */
#endif

/* {{{ apc_do_pass_two */
static void apc_do_pass_two(zend_op_array *op_array)
{
    zend_op *opline, *end;
    opline = op_array->opcodes;
    end = opline + op_array->last;

    while (opline < end) {
        if (opline->op1.op_type == IS_CONST) {
            opline->op1.u.constant.is_ref = 1;
            opline->op1.u.constant.refcount = 2; /* Make sure is_ref won't be reset */
        }
        if (opline->op2.op_type == IS_CONST) {
            opline->op2.u.constant.is_ref = 1;
            opline->op2.u.constant.refcount = 2;
        }
#ifdef ZEND_ENGINE_2_1
        switch (opline->opcode) {
            case ZEND_JMP:
                opline->op1.u.jmp_addr = &op_array->opcodes[opline->op1.u.opline_num];
                break;
            case ZEND_JMPZ:
            case ZEND_JMPNZ:
            case ZEND_JMPZ_EX:
            case ZEND_JMPNZ_EX:
                opline->op2.u.jmp_addr = &op_array->opcodes[opline->op2.u.opline_num];
                break;
        }
        ZEND_VM_SET_OPCODE_HANDLER(opline);
#endif
        opline++;
    }
}
/* }}} */

/* {{{ apc_optimize_op_array */
zend_op_array* apc_optimize_op_array(zend_op_array* op_array)
{
#define RESTART_PEEPHOLE_LOOP { pair_destroy(p); i = -1; continue; }
#define OPTIMIZE1(name) { \
    if ((p = peephole_ ## name(op_array->opcodes, i, op_array->last))) { \
        rewrite_ ## name(op_array->opcodes, p); \
        pair_destroy(p); \
    } \
}

#define OPTIMIZE2(name) { \
    if ((p = peephole_ ## name(op_array->opcodes, i, op_array->last))) { \
        if (!are_branch_targets(cdr(p), jumps)) { \
            rewrite_ ## name(op_array->opcodes, p); \
            RESTART_PEEPHOLE_LOOP; \
        } \
        pair_destroy(p); \
    } \
}

    Pair** jumps;
    int i;
    int jump_array_size;

    if (!op_array->opcodes) {
        return op_array;
    }
#ifdef ZEND_ENGINE_2_1
    apc_restore_opline_num(op_array);
#endif
    convert_switch(op_array);
    jump_array_size = op_array->last;
    jumps = build_jump_array(op_array);
    for (i = 0; i < op_array->last; i++) {
        Pair* p;
    
        OPTIMIZE1(const_cast);
        OPTIMIZE1(is_equal_bool);
        OPTIMIZE2(inc);
        OPTIMIZE2(print);
        OPTIMIZE2(multiple_echo);
        /* OPTIMIZE2(constant_fold); */
        OPTIMIZE2(fcall);
        OPTIMIZE2(add_string);
        OPTIMIZE2(needless_bool);
    }

    op_array->last = compress_ops(op_array, jumps);
    destroy_jump_array(jumps, jump_array_size);
    apc_do_pass_two(op_array); 

    return op_array;

#undef OPTIMIZE1
#undef OPTIMIZE2
#undef RESTART_PEEPHOLE_LOOP
}
/* }}} */

/*
 * Local variables:
 * indent-tabs-mode: nil
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: fdm=marker
 * vim: expandtab sw=4 ts=4 sts=4
 */
