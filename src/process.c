/**
 * @ingroup receptor
 *
 * @{
 * @file process.c
 * @brief implementation of ceptr processing: instructions and run tree reduction
 * @todo implement a way to define sys_processes input and output signatures
 *
 * @copyright Copyright (C) 2013-2015, The MetaCurrency Project (Eric Harris-Braun, Arthur Brock, et. al).  This file is part of the Ceptr platform and is released under the terms of the license contained in the file LICENSE (GPLv3).
 */
#include "process.h"
#include "def.h"
#include "semtrex.h"
#include <stdarg.h>
#include "receptor.h"
#include "../spec/spec_utils.h"
#include "util.h"

/**
 * implements the INTERPOLATE_FROM_MATCH process
 *
 * replaces the interpolation tree with the matched sub-parts from a semtrex match results tree
 *
 * @param[in] t interpolation tree to be scanned for INTERPOLATE_SYMBOL nodes
 * @param[in] match_results SEMTREX_MATCH_RESULTS tree
 * @param[in] match_tree original tree that was matched (needed to grab the data to interpolate)
 * @todo what to do if match has sibs??
 */
void _p_interpolate_from_match(T *t,T *match_results,T *match_tree) {
    if (semeq(_t_symbol(t),INTERPOLATE_SYMBOL)) {
        Symbol s = *(Symbol *)_t_surface(t);
        T *m = _t_get_match(match_results,s);
        if (!m) {
            raise_error0("expected to have match!");
        }
        int *path = (int *)_t_surface(_t_child(m,2));
        int sibs = *(int*)_t_surface(_t_child(m,3));
        T *x = _t_get(match_tree,path);

        if (!x) {
            raise_error0("expecting to get a value from match!!");
        }
        _t_morph(t,x);
    }
    DO_KIDS(t,_p_interpolate_from_match(_t_child(t,i),match_results,match_tree));
}

/**
 * check a group of parameters to see if they match a process input signature
 *
 * @param[in] defs definition trees needed for the checking
 * @param[in] p the Process we are checking against
 * @param[in] params list of parameters
 *
 * @returns Error code
 *
 * @todo add SIGNATURE_SYMBOL for setting up process signatures by Symbol not just Structure
 */
Error __p_check_signature(Defs *defs,Process p,T *params) {
    T *def = _d_get_process_code(defs->processes,p);
    T *input = _t_child(def,4);
    int i = _t_children(input);
    int c = _t_children(params);
    if (i > c) return tooFewParamsReductionErr;
    if (i < c) return tooManyParamsReductionErr;
    for(i=1;i<=c;i++) {
        T *sig = _t_child(_t_child(input,i),1);
        if(semeq(_t_symbol(sig),SIGNATURE_STRUCTURE)) {
            Structure ss = *(Symbol *)_t_surface(sig);
            if (!semeq(_d_get_symbol_structure(defs->symbols,_t_symbol(_t_child(params,i))),ss) && !semeq(ss,TREE))
                return signatureMismatchReductionErr;
        }
        else {
            raise_error("unknown signature checking symbol: %s",_d_get_symbol_name(0,_t_symbol(sig)));
        }
    }
    return 0;
}


/**
 * reduce system level processes in a run tree.  Assumes that the children have already been
 * reduced and all parameters have been filled in
 *
 * these system level processes are the equivalent of the instruction set of the ceptr virtual machine
 */
Error __p_reduce_sys_proc(R *context,Symbol s,T *code) {
    int b,c;
    char *str;
    Symbol sy;
    T *x,*t,*match_results,*match_tree;
    Error err = noReductionErr;
    switch(s.id) {
    case NOOP_ID:
        // noop simply replaces itself with it's own child
        x = _t_detach_by_idx(code,1);
        break;
    case IF_ID:
        t = _t_child(code,1);
        b = (*(int *)_t_surface(t)) ? 2 : 3;
        x = _t_detach_by_idx(code,b);
        break;
    case ADD_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        *((int *)&x->contents.surface) = c+*((int *)&x->contents.surface);
        break;
    case SUB_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)-c;
        break;
    case MULT_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)*c;
        break;
    case DIV_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        if (!c) {
            _t_free(x);
            return divideByZeroReductionErr;
        }
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)/c;
        break;
    case MOD_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        if (!c) {
            _t_free(x);
            return divideByZeroReductionErr;
        }
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)%c;
        break;
    case EQ_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)==c;
        x->contents.symbol = BOOLEAN;
        break;
    case LT_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)<c;
        x->contents.symbol = BOOLEAN;
        break;
    case GT_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)>c;
        x->contents.symbol = BOOLEAN;
        break;
    case LTE_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)<=c;
        x->contents.symbol = BOOLEAN;
        break;
    case GTE_INT_ID:
        x = _t_detach_by_idx(code,1);
        c = *(int *)_t_surface(_t_child(code,1));
        *((int *)&x->contents.surface) = *((int *)&x->contents.surface)>=c;
        x->contents.symbol = BOOLEAN;
        break;
    case CONCAT_STR_ID:
        // if the first parameter is a RESULT SYMBOL then we use that as the symbol type for the result tree.
        x = _t_detach_by_idx(code,1);
        sy = _t_symbol(x);
        if (semeq(RESULT_SYMBOL,sy)) {
            sy = *(Symbol *)_t_surface(x);
            _t_free(x);
            x = _t_detach_by_idx(code,1);
        }
        //@todo, add a bunch of sanity checking here to make sure the
        // parameters are all CSTRINGS
        c = _t_children(code);
        // make sure the surface was allocated and if not, converted to an alloced surface
        if (c > 0) {
            if (!(x->context.flags & TFLAG_ALLOCATED)) {
                int v = *((int *)&x->contents.surface); // copy the string as an integer
                str = (char *)&v; // calculate the length
                int size = strlen(str)+1;
                x->contents.surface = malloc(size);
                memcpy(x->contents.surface,str,size);
                t->context.flags = TFLAG_ALLOCATED;
            }
        }
        // @todo this would probably be faster with just one total realloc for all children
        for(b=1;b<=c;b++) {
            str = (char *)_t_surface(_t_child(code,b));
            int size = strlen(str);
            x->contents.surface = realloc(x->contents.surface,x->contents.size+size);
            memcpy(x->contents.surface+x->contents.size-1,str,size);
            x->contents.size+=size;
            *( (char *)x->contents.surface + x->contents.size -1) = 0;
        }
        x->contents.symbol = sy;
        break;
    case RESPOND_ID:
        {
            T *signal = _t_parent(context->run_tree);
            if (!signal || !semeq(_t_symbol(signal),SIGNAL))
                return notInSignalContextReductionError;

            T *response_contents = _t_detach_by_idx(code,1);
            T *envelope = _t_child(signal,1);
            Xaddr to = *(Xaddr *)_t_surface(_t_child(envelope,1)); // reverse the from and to
            Xaddr from = *(Xaddr *)_t_surface(_t_child(envelope,2));
            Aspect a = *(Aspect *)_t_surface(_t_child(envelope,3));

            // add the response signal into the outgoing signals list of the root
            // run-tree (which is always the last child)
            R *root = context;
            while (context->caller) root = context->caller;
            int kids = _t_children(root->run_tree);
            T *signals;
            if (kids == 1 || (!semeq(SIGNALS,_t_symbol(signals = _t_child(root->run_tree,kids)))))
                signals = _t_newr(root->run_tree,SIGNALS); // make signals list if it's not there
            T *response = __r_make_signal(from,to,a,response_contents);
            _t_add(signals,response);

            x = _t_newi(0,TEST_INT_SYMBOL,0);
        }
        // @todo figure what RESPOND should return, since really it's a side-effect instruction
        // perhaps some kind of signal context symbol or something.  Right now using TEST_INT_SYMBOL
        // as a bogus placeholder.
        break;
    case QUOTE_ID:
        x = _t_detach_by_idx(code,1);
        break;
    case EXPECT_ACT_ID:
        // detach the carrier and expectation and construction params, and enqueue the expectation and action
        // on the carrier
        {
            T *carrier_param = _t_detach_by_idx(code,1);
            T *carrier = *(T **)_t_surface(carrier_param);
            _t_free(carrier_param);
            T *ex = _t_detach_by_idx(code,1);
            T *expectation = _t_new_root(EXPECTATION);
            _t_add(expectation,ex);
            T *params = _t_detach_by_idx(code,1);

            //@todo: this is a fake way to add an expectation to a carrier (as a c pointer
            // out of the params)
            // we probably actually need a system representation for carriers and an API
            // that will also make this thread safe.  For example, in the case of carrier being
            // a receptor's aspect/flux then we should be using _r_add_listener here, but
            // unfortunately we don't want to have to know about receptors this far down in the
            // stack...  But it's not clear yet how we do know about the listening context as
            // I don't think it should be copied into every execution context (the R struct)
            _t_add(carrier,expectation);
            _t_add(carrier,params);
            // the action is a pointer back to this context for now were using a EXPECT_ACT
            // with the c pointer as the surface because I don't know what else to do...  @fixme
            // perhaps this should be a BLOCKED_EXPECT_ACTION process or something...
            _t_new(carrier,EXPECT_ACT,&context,sizeof(context));
        }
        rt_cur_child(code) = 1; // reset the current child count on the code
        x = _t_detach_by_idx(code,1);

        // the actually blocking happens in redcueq which can remove the process from the
        // round-robin
        err = Block;
        break;
    case SEND_ID:
        {
            T *t = _t_detach_by_idx(code,1);
            Xaddr to = *(Xaddr *)_t_surface(t);
            _t_free(t);
            T* signal_contents = _t_detach_by_idx(code,1);

            Xaddr from = {RECEPTOR_XADDR,0};  //@todo how do we say SELF??
            x = __r_make_signal(from,to,DEFAULT_ASPECT,signal_contents);
        }
        err = Send;
        break;
    case INTERPOLATE_FROM_MATCH_ID:
        match_results = _t_child(code,2);
        match_tree = _t_child(code,3);
        x = _t_detach_by_idx(code,1);
        // @todo interpolation errors?
        _p_interpolate_from_match(x,match_results,match_tree);
        break;
    case RAISE_ID:
        return raiseReductionErr;
        break;
    case READ_STREAM_ID:
        {
            T *s = _t_detach_by_idx(code,1);
            FILE *stream =*(FILE**)_t_surface(s);
            _t_free(s);
            s = _t_detach_by_idx(code,1);
            sy = _t_symbol(s);
            if (semeq(RESULT_SYMBOL,sy)) {
                sy = *(Symbol *)_t_surface(s);
                _t_free(s);
                int ch;
                char buf[1000]; //@todo handle buffer dynamically
                int i = 0;
                while ((ch = fgetc (stream)) != EOF && ch != '\n' && i < 1000)
                    buf[i++] = ch;
                if (i>=1000) {raise_error0("buffer overrun in READ_STREAM");}

                buf[i++]=0;
                x = _t_new(0,sy,buf,i);
            }
            else {raise_error0("expecting RESULT_SYMBOL");}
        }
        break;
    default:
        raise_error("unknown sys-process id: %d",s.id);
    }

    // any remaining children of 'code' are the parameters which have all now been "used up"
    // so we can call the low-level __t_free the clean them up and then replace the contents of
    // the 'code' node with the contents of the 'x' node that was either detached or produced
    // by the the process that just ran
    __t_free(code);
    code->structure.child_count = x->structure.child_count;
    code->structure.children = x->structure.children;
    code->contents = x->contents;
    code->context = x->context;
    free(x);
    return err;
}

/**
 * create a run-tree execution context.
 */
R *__p_make_context(T *run_tree,R *caller) {
    R *context = malloc(sizeof(R));
    context->state = Eval;
    context->err = 0;
    context->run_tree = run_tree;
    // start with the node_pointer at the first child of the run_tree
    context->node_pointer = _t_child(run_tree,1);
    context->parent = run_tree;
    context->idx = 1;
    context->caller = caller;
    if (caller) caller->callee = context;
    return context;
}

// unlink the queue element from the list rejoining the
// previous with the next
#define __p_dequeue(list,qe)                    \
    if (!qe->prev) {                            \
        list = qe->next;                        \
    }                                           \
    else {                                      \
        qe->prev->next = qe->next;              \
    }

// add the queue element onto the head of the list
#define __p_enqueue(list,qe) {                  \
        Qe *d = list;                           \
        qe->next = d;                           \
        if (d) d->prev = qe;                    \
        list = qe;                              \
    }

void _p_enqueue(Qe **listP,Qe *e) {
    __p_enqueue(*listP,e);
}

/**
 * search for the context in the q and unblock it interpolating the waiting
 * process with the match results first
 */
Error _p_unblock(Q *q,R *context) {
    // find the context in the queue
    Qe *e = q->blocked;
    while (e && e->context != context) e = e->next;
    if (!e) {raise_error0("contextNotFoundErr");}
    __p_dequeue(q->blocked,e);
    __p_enqueue(q->active,e);

    q->contexts_count++;
    context->state = Eval;
}

//#define debug_reduce

/**
 * reduce a run tree by executing the instructions in it and replacing the tree values in place
 *
 * a run_tree is expected to have a code tree as the first child, parameters as the second,
 * and optionally an error handling routine as the third child.
 *
 * @param[in] processes context of defined processes
 * @param[in] run_tree the run tree being reduced
 * @returns Error status of the reduction
 *
 * <b>Examples (from test suite):</b>
 * @snippet spec/process_spec.h testProcessErrorTrickleUp
 */
Error _p_reduce(Defs *defs,T *rt) {
    T *run_tree = rt;
    R *context = __p_make_context(run_tree,0);
    Error e;

    while(_p_step(defs, &context) != Done);
    e = context->err;
    free(context);
    return e;
}

/**
 * take one step in the execution state machine given a run-tree context
 *
 * a run_tree is expected to have a code tree as the first child, parameters as the second,
 * and optionally an error handling routine as the third child.
 *
 * @param[in] processes context of defined processes
 * @param[in] pointer to context pointer
 * @returns the next state that will be called for the context
 */
Error _p_step(Defs *defs, R **contextP) {
    R *context = *contextP;

    switch(context->state) {
    case noReductionErr:
    case Block:
    case Send:
        raise_error0("whoa, virtual states can't be executed!"); // shouldn't be calling step if Done or noErr or Block or Send
        break;
    case Pop:
        // if this was the successful reduction by an error handler
        // move the value to the 1st child
        if (context->err) {
            T *t = _t_detach_by_idx(context->run_tree,3);
            if (t) {
                _t_replace(context->run_tree,1,t);
                context->err = noReductionErr;
            }
        }

        // if this is top caller on the stack then we are completely done
        if (!context->caller) {
            context->state = Done;
            break;
        }
        else {
            // otherwise pop the context
            R *ctx = context;
            context = context->caller;  // set the new context

            if (!ctx->err) {
                // get results of the run_tree
                T *np = _t_detach_by_idx(ctx->run_tree,1);
                _t_replace(context->parent,context->idx,np); // replace the process call node with the result
                rt_cur_child(np) = RUN_TREE_EVALUATED;
                context->node_pointer = np;
                context->state = Eval;  // or possible ascend??
            }
            else context->state = ctx->err;
            // cleanup
            _t_free(ctx->run_tree);
            free(ctx);
            context->callee = 0;
            *contextP = context;
        }

        break;
    case Eval:
        {
            T *np = context->node_pointer;
            if (!np) {
                raise_error0("Whoa! Null node pointer");
            }
            Process s = _t_symbol(np);

            if (semeq(s,PARAM_REF)) {
                T *param = _t_get(context->run_tree,(int *)_t_surface(np));
                if (!param) {
                    raise_error0("request for non-existent param");
                }
                context->node_pointer = np = _t_rclone(param);
                _t_replace(context->parent, context->idx,np);
                s = _t_symbol(np);
            }
            // @todo what if the replaced parameter is itself a PARAM_REF tree ??

            // if this node is not a process, i.e. it's data, then we are done descending
            // and it will be the result so ascend
            if (!is_process(s)) {
                context->state = Ascend;
            }
            else {
                int c = _t_children(np);
                if (c == rt_cur_child(np) || semeq(s,QUOTE)) {
                    // if the current child == the child count this means
                    // all the children have been processed, so we can evaluate this process
                    // if the process is QUOTE that's a special case and we evaluate it
                    // immediately without descending.
                    if (!is_sys_process(s)) {
                        // if it's user defined process then we check the signature and then make
                        // a new run-tree run that process
                        Error e = __p_check_signature(defs,s,np);
                        if (e) context->state = e;
                        else {
                            T *run_tree = __p_make_run_tree(defs->processes,s,np);
                            context->state = Pushed;
                            *contextP = __p_make_context(run_tree,context);
                        }
                    }
                    else {
                        // if it's a sys process we can just reduce it in and then ascend
                        // or move to the error handling state
                        Error e = __p_reduce_sys_proc(context,s,np);
                        context->state = e ? e : Ascend;
                    }
                }
                else if(c) {
                    //descend and increment the current child we're working on!
                    context->state = Descend;
                }
                else {
                    raise_error0("whoa! brain fart!");
                }
            }
        }
        break;
    case Ascend:
        rt_cur_child(context->node_pointer) = RUN_TREE_EVALUATED;
        context->node_pointer = context->parent;
        context->parent = _t_parent(context->node_pointer);
        if (!context->parent || context->parent == context->run_tree) {
            context->idx = 1;
        }
        else context->idx = rt_cur_child(context->parent);
        if (context->node_pointer == context->run_tree)
            context->state = Pop;
        else
            context->state = Eval;
        break;
    case Descend:
        context->parent = context->node_pointer;
        context->idx = ++rt_cur_child(context->node_pointer);
        context->node_pointer = _t_child(context->node_pointer,context->idx);
        context->state = Eval;
        break;
    default:
        context->err = context->state;
        if (_t_children(context->run_tree) <= 2) {
            // no error handler so just return the error
            context->state = Pop;
        }
        else {
            // the first parameter to the error code is always a reduction error
            // which gets added on as the 4th child of the run tree when the
            // error happens.
            T *ps = _t_newr(context->run_tree,PARAMS);

            //@todo: fix this so we don't actually use an error value that
            // then has to be translated into a symbol, but rather so that we
            // can programatically calculate the symbol.
            Symbol se;
            switch(context->state) {
            case tooFewParamsReductionErr: se=TOO_FEW_PARAMS_ERR;break;
            case tooManyParamsReductionErr: se=TOO_MANY_PARAMS_ERR;break;
            case signatureMismatchReductionErr: se=SIGNATURE_MISMATCH_ERR;break;
            case notProcessReductionError: se=NOT_A_PROCESS_ERR;break;
            case notInSignalContextReductionError: se=NOT_IN_SIGNAL_CONTEXT_ERR;
            case divideByZeroReductionErr: se=ZERO_DIVIDE_ERR;break;
            case incompatibleTypeReductionErr: se=INCOMPATIBLE_TYPE_ERR;break;
            case raiseReductionErr:
                se = *(Symbol *)_t_surface(_t_child(context->node_pointer,1));
                break;
            default: raise_error("unknown reduction error: %d",context->state);
            }
            T *err = __t_new(ps,se,0,0,sizeof(rT));
            int *path = _t_get_path(context->node_pointer);
            _t_new(err,ERROR_LOCATION,path,sizeof(int)*(_t_path_depth(path)+1));
            free(path);

            // switch the node_pointer to the top of the error handling routine
            context->node_pointer = _t_child(context->run_tree,3);
            context->idx = 3;
            context->parent = context->run_tree;

            context->state = Eval;
        }
    }
    return context->state;
}

/*
  special runtree builder that uses the actual params tree node.  This is used
  in the process of reduction because we don't have to clone the param values, we
  can use the actual tree nodes that are being reduced as they are already rT nodes
  and they are only used once, i.e. in this reduction
*/
T *__p_make_run_tree(T *processes,Process p,T *params) {
    T *code_def = _d_get_process_code(processes,p);
    T *code = _t_child(code_def,3);
    T *t = _t_new_root(RUN_TREE);
    T *c = _t_rclone(code);
    _t_add(t,c);
    T *ps = _t_newr(t,PARAMS);
    int i,num_params = _t_children(params);
    for(i=1;i<=num_params;i++) {
        _t_add(ps,_t_detach_by_idx(params,1));
    }
    return t;}

/**
 * low level functon to build a run tree from a code tree and a variable list of params
 */
T *__p_build_run_tree_va(T* code,int num_params,va_list params) {
    T *t = _t_new_root(RUN_TREE);
    T *c = _t_rclone(code);
    _t_add(t,c);
    T *ps = _t_newr(t,PARAMS);
    int i;
    for(i=1;i<=num_params;i++) {
        _t_add(ps,_t_clone(va_arg(params,T *)));
    }
    return t;
}

T *__p_build_run_tree(T* code,int num_params,...) {
    va_list params;
    va_start(params,num_params);
    T *t = __p_build_run_tree_va(code,num_params,params);
    va_end(params);
    return t;
}

/**
 * Build a run tree from a code tree and params
 *
 * @param[in] processes processes trees
 * @param[in] process Process tree node to be turned into run tree
 * @param[in] num_params the number of parameters to add to the parameters child
 * @param[in] ... T params
 * @returns T RUN_TREE tree
 */
T *_p_make_run_tree(T *processes,T *process,int num_params,...) {

    T *t = NULL;
    va_list params;

    Process p = *(Process *)_t_surface(process);
    if (!is_process(p)) {
        raise_error("%s is not a Process",_d_get_process_name(processes,p));
    }
    if (is_sys_process(p)) {
        t =  _t_new_root(RUN_TREE);
        // if it's a sys process we add the parameters directly as children to the process
        // because no sys-processes refer to PARAMS by path
        // this also means we need rclone them instead of clone them because they
        // will actually need to have space for the status marks by the processing code
        T *c = __t_new(t,p,0,0,sizeof(rT));

        va_start(params,num_params);
        int i;
        for(i=0;i<num_params;i++) {
            _t_add(c,_t_rclone(va_arg(params,T *)));
        }
        va_end(params);
    }
    else {
        T *code_def = _d_get_process_code(processes,p);
        T *code = _t_child(code_def,3);

        va_list params;
        va_start(params,num_params);
        t = __p_build_run_tree_va(code,num_params,params);
        va_end(params);
    }
    return t;
}

/**
 * create a new processing queue
 *
 * @param[in] defs definitions that
 * @returns Q the processing queue
 */
Q *_p_newq(Defs *defs,T *pending_signals) {
    Q *q = malloc(sizeof(Q));
    q->defs = defs;
    q->contexts_count = 0;
    q->active = NULL;
    q->completed = NULL;
    q->blocked = NULL;
    q->pending_signals = pending_signals;
    return q;
}

// clean up a queue element
_p_free_elements(Qe *e) {
    while(e) {
        _p_free_context(e->context);
        Qe *n = e->next;
        free(e);
        e = n;
    }
}

// clean up a context including its run-trees
_p_free_context(R *c) {
    while(c) {
        // free any run_trees that are roots, i.e. assume
        // that a tree in a context that's part of another tree
        // will get freed elsewhere.
        if (!_t_parent(c->run_tree))
            _t_free(c->run_tree);
        R *n = c->caller;
        free(c);
        c = n;
    }
}

/**
 * free the resources in a queue, including any run-trees
 *
 * @param[in] q the queue to be freed
 */
void _p_freeq(Q *q) {
    Qe *e = q->active;
    _p_free_elements(q->active);
    _p_free_elements(q->completed);
    _p_free_elements(q->blocked);
    if (q->pending_signals) _t_free(q->pending_signals);
    free(q);
}

/**
 * add a run tree into a processing queue
 *
 * @todo make thread safe.  currently you shouldn't call this if the Q is being actively reduced
 */
void _p_addrt2q(Q *q,T *run_tree) {
    Qe *n = malloc(sizeof(Qe));
    n->prev = NULL;
    n->context = __p_make_context(run_tree,0);
    n->accounts.elapsed_time = 0;
    __p_enqueue(q->active,n);
    q->contexts_count++;
}

/**
 * reduce all the processes in a queue, and terminate thread when completed
 *
 * @param[in] q the queue to be processed
 */
void *_p_reduceq_thread(void *arg){
    Q *q = (Q*)arg;

    int err;
    err = _p_reduceq((Q *)arg);
    pthread_exit(err);
}

/**
 * reduce all the processes in a queue
 *
 * @param[in] q the queue to be processed
 */
Error _p_reduceq(Q *q) {
#ifdef debug_reduce
    printf("\n\nStarting reduce:\n");
#endif
    Qe *qe = q->active;
    Error next_state;
    struct timespec start, end;

    while (q->contexts_count) {

#ifdef debug_reduce
        R *context = qe->context;
        char *sn[]={"Ascend","Descend","Pushed","Pop","Eval","Block","Send","Done"};
        char *s = context->state <= 0 ? sn[-context->state -1] : "Error";
        printf("ID:%p -- State %s : %d\n",qe,s,context->state);
        printf("  idx:%d\n",context->idx);
        puts(_t2s(q->defs,context->run_tree));
        if (context) {
            if (context->node_pointer == 0) {
                printf("Node Pointer: NULL!\n");
            }
            else {
                printf("rt_cur_child:%d\n",rt_cur_child(context->node_pointer));
                int *path = _t_get_path(context->node_pointer);
                char pp[255];
                _t_sprint_path(path,pp);
                printf("Node Pointer:%s\n",pp);
                free(path);
            }
        }
        printf("\n");
#endif

        clock_gettime(CLOCK_MONOTONIC, &start);
        next_state = _p_step(q->defs, &qe->context); // next state is set in directly in the context
        clock_gettime(CLOCK_MONOTONIC, &end);
        qe->accounts.elapsed_time +=  diff_micro(&start, &end);

        Qe *next = qe->next;
        if (next_state == Done) {
            // remove from the round-robin
            __p_dequeue(q->active,qe);

            // add to the completed list
            __p_enqueue(q->completed,qe);
            q->contexts_count--;
        }
        else if (next_state == Block) {
            // remove from the round-robin
            __p_dequeue(q->active,qe);

            // add to the blocked list
            __p_enqueue(q->blocked,qe);
            q->contexts_count--;
        }
        else if (next_state == Send) {
            // remove from the round-robin
            __p_dequeue(q->active,qe);

            // take the signal off the run tree and send it, adding a send result in it's place
            T *signal = qe->context->node_pointer;
            T *parent = _t_parent(signal);

            //@todo figure out what that return value should be.  Probably some result from
            // the actual signal sending machinery, or at least what ever is going to
            // evaluate the destination address for validity.
            T *result = _t_newi(0,TEST_INT_SYMBOL,0);

            //@todo refactor this into a version of _t_replace that swaps out the given child and returns it
            // rather than freeing it.
            parent->structure.children[0] = result; // 0 is the first child
            result->structure.parent = parent;
            signal->structure.parent = NULL;

            _t_add(q->pending_signals,signal);

            // add to the blocked list
            __p_enqueue(q->blocked,qe);
            q->contexts_count--;
        }
        qe = next ? next : q->active;  // next in round robing or wrap
    };
    // @todo figure out what error we should be sending back here, i.e. what if
    // one process ended ok, but one did not.  What's the error?  Probably
    // the errors here would be at a different level, and the caller would be
    // expected to inspect the errors of the reduced processes.
    return 0;
}
/** @}*/
