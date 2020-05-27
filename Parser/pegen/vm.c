#include <Python.h>
#include <errcode.h>
#include "../tokenizer.h"

#include "pegen.h"
#include "parse_string.h"

#include "vm.h"

static Frame *
push_frame(Stack *stack, Rule *rule)
{
    assert(stack->top < 100);
    Frame *f = &stack->frames[stack->top++];
    f->rule = rule;
    f->ialt = 0;
    f->iop = 0;
    f->cut = 0;
    return f;
}

static Frame *
pop_frame(Stack *stack)
{
    assert(stack->top > 0);
    Frame *f = &stack->frames[--stack->top];
    return f;
}

static void *
run_vm(Parser *p, Rule rules[], int start)
{
    Stack stack = {p, 0, {{0}}};
    Frame *f = push_frame(&stack, &rules[start]);
    void *v;
    int oparg;

 top:
    for (int i = 0; i < stack.top; i++) printf(" ");
    printf("Rule: %s; ialt=%d; iop=%d; op=%s\n", f->rule->name, f->ialt, f->iop, opcode_names[f->rule->opcodes[f->iop]]);
    switch (f->rule->opcodes[f->iop++]) {
    case OP_NAME:
        v = _PyPegen_name_token(p);
        break;
    case OP_NUMBER:
        v = _PyPegen_number_token(p);
        break;
    case OP_CUT:
        f->cut = 1;
        goto top;
    case OP_TOKEN:
        oparg = f->rule->opcodes[f->iop++];
        v = _PyPegen_expect_token(p, oparg);
        break;
    case OP_RULE:
        oparg = f->rule->opcodes[f->iop++];
        f = push_frame(&stack, &rules[oparg]);
        goto top;
    case OP_RETURN:
        oparg = f->rule->opcodes[f->iop++];
        v = (void*) 1;  // dummy action result
        f = pop_frame(&stack);
        break;
    case OP_SUCCESS:
        oparg = f->rule->opcodes[f->iop++];
        v = (void*) 1;  // dummy action result
        if (v) {
            return v;
        }
        // fallthrough
    case OP_FAILURE:
        return RAISE_SYNTAX_ERROR("A syntax error");
    default:
        abort();
    }

    if (v) {
        printf("            OK\n");
        // f->values.append(v)
        goto top;
    }
    if (PyErr_Occurred()) {
        printf("            PyErr\n");
        p->error_indicator = 1;
        return NULL;
    }

 fail:
    printf("            fail\n");
    if (f->cut)
        goto pop;
    f->iop = f->rule->alts[++f->ialt];
    if (f->iop == -1)
        goto pop;
    goto top;

 pop:
    f = pop_frame(&stack);
    goto fail;
}

void *
_PyPegen_vmparser(Parser *p)
{
    p->keywords = reserved_keywords;
    p->n_keyword_lists = n_keyword_lists;

    return run_vm(p, all_rules, 0);
}
