/*
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   |          George Schlossnagle <george@omniti.com>                     |
   +----------------------------------------------------------------------+
*/

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
      case ZEND_JMP_NO_CTOR:
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
      case ZEND_JMP_NO_CTOR:
      case ZEND_FE_FETCH:
      case ZEND_JMPZNZ:
        return 1;
    }
    return 0;
}

/* squeezes no-ops out of the zend_op array */
static int compress_ops(zend_op* ops, Pair** jumps, int num_ops)
{
    int i, j;

    for (i = 0, j = 0; j < num_ops; i++, j++) {
        if (ops[i].opcode == ZEND_NOP) {
            Pair* branches;

            /* search for a non-noop op, updating branch targets as we go */
            for (;;) {
                /* update branch targets */
                for (branches = jumps[j]; branches; branches = cdr(branches)) {
                    change_branch_target(&ops[car(branches)], j, i);
                }
                /* quit when we've found a non-noop instruction */
                if (ops[j].opcode != ZEND_NOP) {
                    break;
                }
                j++;
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
        binary_op(result, &op->op1.u.constant, &op->op2.u.constant);
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
        break; 
      case ZEND_POST_DEC:
        ops[cadr(p)].opcode = ZEND_PRE_DEC;
        break;
      default:
        assert(0);
        break;
    }
    clear_zend_op(&ops[caddr(p)]);  // don't need this anymore
}

static void rewrite_add_string(zend_op* ops, Pair* p)
{
    int curr = car(p);

    assert(pair_length(p) >= 2);

    for (p = cdr(p); p; p = cdr(p)) {
        concat_function(&ops[car(p)].op2.u.constant,
                        &ops[car(p)].op2.u.constant,
                        &ops[curr].op2.u.constant);
        clear_zend_op(ops + curr);
        curr = car(p);
    }
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
    int i, nest_levels = 0;
    int array_offset = 0;
    for (i = 0; i < op_array->last; i++) {
        zend_op *opline = &op_array->opcodes[i];
        zend_brk_cont_element *jmp_to;
        if(opline->opcode != ZEND_BRK && opline->opcode != ZEND_CONT) {
            continue;
        }
        if(opline->op2.op_type == IS_CONST && opline->op2.u.constant.type == IS_LONG) {
            nest_levels = opline->op2.u.constant.value.lval;
        } else {
            assert(0);
        }
        array_offset = opline->op1.u.opline_num;
        do {
            if (array_offset < 0) {
                 assert(0);
                // fail
            }
            jmp_to = &op_array->brk_cont_array[array_offset];
            if (nest_levels>1) {
                zend_op *brk_opline = &op_array->opcodes[jmp_to->brk];
                switch (brk_opline->opcode) {
                  case ZEND_SWITCH_FREE:
                    // fail
                    assert(0);
                    break;
                  case ZEND_FREE:
                    // fail
                    assert(0);
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
        fprintf(stderr, "Rewriting op\n");
    }
}


/* returns list of potential branch targets for this op */
static Pair* extract_branch_targets(zend_op* op)
{
    switch (op->opcode) {
      case ZEND_JMP:
        return cons(op->op1.u.opline_num, 0);
      case ZEND_JMPZ:
      case ZEND_JMPNZ:
      case ZEND_JMPZ_EX:
      case ZEND_JMPNZ_EX:
      case ZEND_JMP_NO_CTOR:
      case ZEND_FE_FETCH:
        return cons(op->op2.u.opline_num, 0);
      case ZEND_JMPZNZ:
        return cons(op->op2.u.opline_num, cons(op->extended_value, 0));
        
    }
    return 0;
}

/* retval[i] -> list of instruction indices that may branch to i */
static Pair** build_jump_array(zend_op* ops, int num_ops)
{
    Pair** jumps;
    int i;

    jumps = (Pair**) malloc(num_ops * sizeof(Pair*));
    memset(jumps, 0, num_ops * sizeof(Pair*));
    for (i = 0; i < num_ops; i++) {
        Pair* targets = extract_branch_targets(&ops[i]);
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

        if (load_op->op2.u.fetch_type == ZEND_FETCH_LOCAL   &&
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

        if (load_op->op2.u.fetch_type == ZEND_FETCH_LOCAL   &&
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

static Pair* peephole_add_string(zend_op* ops, int i, int num_ops)
{
    int j;      /* next op after i */
    Pair *p = NULL;
    int tmp_var_result;
    int tmp_var_op1;

    if (ops[i].opcode != ZEND_ADD_STRING    ||
        ops[i].result.op_type != IS_TMP_VAR ||
        ops[i].op1.op_type != IS_TMP_VAR    ||
        ops[i].op2.op_type != IS_CONST)
    {
        return 0;
    }

    tmp_var_result = ops[i].result.u.var;
    tmp_var_op1 = ops[i].op1.u.var;

    for (j = next_op(ops, i, num_ops); j < num_ops; j = next_op(ops, j, num_ops)) {
        if (ops[j].opcode != ZEND_ADD_STRING) {
            return p;
        } 

        if (ops[j].op2.op_type == IS_CONST                   &&
            ops[j].result.op_type == IS_TMP_VAR              &&
            ops[j].op1.op_type == IS_TMP_VAR                 &&
            ops[j].result.u.var == tmp_var_result            &&
            ops[j].op1.u.var == tmp_var_op1)
        {
            if (!p) {
                p = cons(j, cons(i, 0));
            }
            else {
                p = cons(j, p);
            }
        }
    }
    return p;
}

/* }}} */

/* {{{ apc_optimize_op_array */

zend_op_array *apc_optimize_op_array(zend_op_array* op_array)
{
    #define RESTART_PEEPHOLE_LOOP { pair_destroy(p); i = -1; continue; }

    Pair** jumps;
    int i;
    int jump_array_size;

    if (!op_array->opcodes) {
        return op_array;
    }

    convert_switch(op_array);
    jump_array_size = op_array->last;
    jumps = build_jump_array(op_array->opcodes, jump_array_size);
    for (i = 0; i < op_array->last; i++) {
        Pair* p;
        if ((p = peephole_inc(op_array->opcodes, i, op_array->last))) {
            if (!are_branch_targets(cdr(p), jumps)) {
                rewrite_inc(op_array->opcodes, p);
            }
            RESTART_PEEPHOLE_LOOP;
        }
        if ((p = peephole_add_string(op_array->opcodes, i, op_array->last))) {
            if (!are_branch_targets(cdr(p), jumps)) {
                rewrite_add_string(op_array->opcodes, p);
            }
            RESTART_PEEPHOLE_LOOP;
        }
    }
    op_array->last = compress_ops(op_array->opcodes, jumps, op_array->last);
    destroy_jump_array(jumps, jump_array_size);

    return op_array;

    #undef RESTART_PEEPHOLE_LOOP
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
