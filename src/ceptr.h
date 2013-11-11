#ifndef _CEPTR_H
#define _CEPTR_H

#define DEFAULT_CACHE_SIZE 100000
#define STACK_SIZE 10000
#define OPERANDS_SIZE (sizeof(Xaddr) * 2)

#include <signal.h>
#include <string.h>
#include <stdio.h>

char error[255];

#define raise_error(error_msg, val)        \
    printf(error_msg, val);            \
    raise(SIGINT);
#define raise_error2(error_msg, val1,val2)        \
    printf(error_msg, val1,val2);                \
    raise(SIGINT);

typedef int Symbol;

#define SYMBOL_PATH_TERMINATOR 0xFFFF
#define STRING_TERMINATOR 0xFFFFFFFF
#define ESCAPE_STRING_TERMINATOR 0xFFFFFFFE

#define BUILTIN_ELEMENT_COUNT 6

enum FunctionNames {
    PRINT, GET_SIZE, INC, ADD
};

typedef int FunctionName;

typedef struct {
    int key;
    Symbol noun;
} Xaddr;

typedef struct {
    Xaddr noun;
    int offset;
} Offset;

typedef struct {
    Xaddr namedElement;
    char *label;
} NounSurface;

enum Opcodes {
    RETURN, PUSH_IMMEDIATE
};

typedef int Opcode;

typedef struct {
    Opcode opcode;
    char operands[OPERANDS_SIZE];
} Instruction;

typedef struct {
    Xaddr type;
    int size;
} SemStackFrame;

typedef struct {
    Xaddr xaddrs[DEFAULT_CACHE_SIZE];
    Symbol xaddr_scape[DEFAULT_CACHE_SIZE];
    char cache[DEFAULT_CACHE_SIZE];
    int current_xaddr;
    int cache_index;
} Data;

typedef int(*ProcessFunc)(void *,Symbol,void*);

typedef struct {
    FunctionName name;
    ProcessFunc function;
} Process;

typedef struct {
    Symbol name;
    int process_count;
    Process processes;
} ElementSurface;

typedef struct {
    SemStackFrame semStack[STACK_SIZE];
    int semStackPointer;
    char valStack[DEFAULT_CACHE_SIZE];
    int valStackPointer;
    //built in xaddrs:
    Xaddr patternSpecXaddr;
    Xaddr arraySpecXaddr;
    Xaddr stringSpecXaddr;
    Xaddr intPatternSpecXaddr;
    Xaddr pointPatternSpecXaddr;
    Xaddr linePatternSpecXaddr;

    ElementSurface builtin_element_surfaces[BUILTIN_ELEMENT_COUNT];
    Data data;
} Receptor;

typedef struct {
    size_t size;
    int children_count;
    Offset children;
} PatternBody;

typedef struct {
    Symbol noun;
} RepsBody;

Process *getProcess(ElementSurface *es, FunctionName name) {
    printf("getProcess looking for name %d in %d \n", name, es->name);
    int i = es->process_count;
    Process *p = &es->processes;
    while (i--) {
        if (p->name == name) {
            return p;
        }
        p++;
    }
    return 0;
}

enum Symbols {
    NULL_SYMBOL = -1, CSPEC = -2, NOUN_SPEC = -3, PATTERN_SPEC = -4,  ARRAY_SPEC = -5, STRING_SPEC = -6
};

void dump_xaddrs(Receptor *r);

void *_find_builtin_element_spec(Receptor *r, Symbol name) {
    int i;
    for (i=0; i< BUILTIN_ELEMENT_COUNT; i++){
        if (r->builtin_element_surfaces[i].name == name){
            return &r->builtin_element_surfaces[i];
        }
    }
    raise_error("unknown builtin element type %d", name);
    return 0;
}

void *_get_noun_element_spec(Receptor *r, Symbol *nounType, Symbol noun){
    Symbol key;
    if (noun < 0) {
        switch(noun){
            case STRING_SPEC:
            case ARRAY_SPEC:
            case PATTERN_SPEC:
            case NOUN_SPEC:
                key = CSPEC;
                break;
            case CSPEC:
                key = NULL_SYMBOL;
                break;
        }
    } else {
        Xaddr *elementXaddr = &((NounSurface *) &r->data.cache[noun])->namedElement;
        *nounType = elementXaddr->noun;
        key = elementXaddr->key;
    }
    if (key < 0) {
        return _find_builtin_element_spec(r, key);
    }
    return &r->data.cache[key];
}

ElementSurface *_get_noun_pattern_spec(Receptor *r, Symbol noun) {
    Xaddr *elementXaddr = &((NounSurface *) &r->data.cache[noun])->namedElement;
    if (elementXaddr->noun == PATTERN_SPEC) {
        return (ElementSurface *) &r->data.cache[elementXaddr->key];
    }
    raise_error2("noun (%d) named item (%d) is not a pattern\n", noun, elementXaddr->noun);
    return 0;
}

int sem_check(Receptor *r, Xaddr xaddr) {
    Symbol noun = r->data.xaddr_scape[xaddr.key];
    return (noun == xaddr.noun);
}

void *op_get(Receptor *r, Xaddr xaddr) {
    if (!sem_check(r, xaddr)) return 0;
    return &r->data.cache[xaddr.key];
}

#define element_header_size(h) (sizeof(ElementSurface)-sizeof(Process)+(((ElementSurface *)h)->process_count)*sizeof(Process))
#define skip_elem_header(h) (element_header_size(h) + (void *)h)
#define REPS_GET_NOUN(reps) (((RepsBody *)skip_elem_header(reps))->noun)
#define REPS_SET_NOUN(reps,n) (((RepsBody *)skip_elem_header(reps))->noun = n)
#define PATTERN_GET_SIZE(pat) (((PatternBody *)skip_elem_header(pat))->size)
#define PATTERN_GET_CHILDREN_COUNT(pat) (((PatternBody *)skip_elem_header(pat))->children_count)
#define PATTERN_SET_CHILDREN_COUNT(pat,count) (((PatternBody *)skip_elem_header(pat))->children_count=count)
#define PATTERN_SET_SIZE(pat,s) (((PatternBody *)skip_elem_header(pat))->size = s)
#define PATTERN_GET_CHILDREN(pat) (&(((PatternBody *)skip_elem_header(pat))->children))

ElementSurface *_get_reps_pattern_spec(Receptor *r,ElementSurface *rs) {
    return _get_noun_pattern_spec(r,REPS_GET_NOUN(rs));
}

size_t _get_pattern_spec_size(ElementSurface *es) {
    return element_header_size(es) + sizeof(PatternBody) - sizeof(Offset) + sizeof(Offset) * PATTERN_GET_CHILDREN_COUNT(es);
}

#define _op_get_array_length(surface) (*((int*)surface))

int op_get_array_length(Receptor *r, Xaddr rX) {
    return _op_get_array_length(op_get(r,rX));
}


int proc_array_get_size(Receptor *r, Symbol noun, void *surface) {
printf("proc_array_get_size %d\n", noun);
    int length = _op_get_array_length(surface);
    int size = sizeof(int);
    Symbol arrayItemType;
    int rep_size;

    Symbol nounType;
    ElementSurface *spec_surface = _get_noun_element_spec(r, &nounType, noun);

    Symbol repsNoun = REPS_GET_NOUN(spec_surface);
    ElementSurface *es = _get_noun_element_spec(r,&arrayItemType,repsNoun);
    if (arrayItemType == PATTERN_SPEC) {
        size += length * PATTERN_GET_SIZE(es);
    }
    else if (arrayItemType == ARRAY_SPEC) {
        surface += sizeof(int);
        while (length--) {
            rep_size = proc_array_get_size(r,repsNoun,surface);
            size += rep_size;
            surface += rep_size;
        }
    }
        //TODO: handle arrays of strings
    else {raise_error2("illegal noun (%d) as array element type for %d\n",arrayItemType,noun);}
    return size ;
}

int proc_string_get_size(Receptor *r, Symbol noun, void *surface) {
    Symbol nounType;
    ElementSurface *spec_surface = _get_noun_element_spec(r, &nounType, noun);
    ElementSurface *es = _get_reps_pattern_spec(r,spec_surface);
    int rep_size = PATTERN_GET_SIZE(es);
    int size = 0;
    while(*(int *)surface != STRING_TERMINATOR) {
        if (*(int *)surface == ESCAPE_STRING_TERMINATOR) {
            surface += sizeof(int);
            size +=sizeof(int);
        }
        size += rep_size;
        surface += rep_size;
    }
    return sizeof(int) + size;
}



int proc_pattern_get_size(Receptor *r, Symbol noun, void *this){
    Symbol nounType;
    ElementSurface *spec_surface = _get_noun_element_spec(r, &nounType, noun);
    printf("----------proc_pattern_get_size");
    printf("noun %d surface %d\n ", noun, spec_surface->name);
    return PATTERN_GET_SIZE(spec_surface);
}


void dump_noun(Receptor *r, NounSurface *ns);


//TODO: refactor out the code in here that walks noun-types.  This code is seen in dump_xaddrs and also in array_nth

//FIXME: should we allow check for surface == 0 and raise an error for those cases where it matters?



size_t _new_get_noun_size(Receptor *r, Symbol noun, void *surface) {
printf("--->_get_noun_size: %d\n", noun);
    Xaddr nounXaddr = { noun, NOUN_SPEC };
//    dump_noun(r, (NounSurface *)op_get(r, nounXaddr));
    Symbol nounType;
    ElementSurface *spec_surface = _get_noun_element_spec(r, &nounType, noun);
    if (getProcess(spec_surface, GET_SIZE)){

        return (getProcess(spec_surface, GET_SIZE)->function)(r, noun, surface);
    } else {
        raise_error2("couldn't find size function for noun %d in element surface %d\n", noun, spec_surface->name);
        return 0;
    }
}


size_t _get_noun_size(Receptor *r, Symbol noun, void *surface) {
printf("_get_noun_size %d \n", noun);
    if (noun < 0){
        return _new_get_noun_size(r, noun, surface);
    }

    Symbol nounType;
    ElementSurface *spec_surface = _get_noun_element_spec(r, &nounType, noun);

    switch (nounType) {
        case PATTERN_SPEC:
            return _new_get_noun_size(r, noun, surface);

        case ARRAY_SPEC:
printf("looking up ARRAY_SPEC %d\n", noun);
//            return _new_get_noun_size(r, noun, surface);
            return proc_array_get_size(r, noun, surface);

        case STRING_SPEC:
            return proc_string_get_size(r, noun, surface);

    }
    return _new_get_noun_size(r, noun, surface);

}

void _record_existence(Receptor *r,size_t current_index,Symbol noun) {
    r->data.current_xaddr++;
    r->data.xaddrs[r->data.current_xaddr].key = current_index;
    r->data.xaddrs[r->data.current_xaddr].noun = noun;
    r->data.xaddr_scape[current_index] = noun;
}

Symbol op_new_noun(Receptor *r, Xaddr xaddr, char *label) {
    NounSurface ns;
    ns.namedElement.key = xaddr.key;
    ns.namedElement.noun = xaddr.noun;
    ns.label = label;
    size_t current_index = r->data.cache_index;
    void *surface = &r->data.cache[current_index];
    memcpy(surface, &ns, sizeof(NounSurface));
    r->data.cache_index += sizeof(NounSurface);

    _record_existence(r,current_index,NOUN_SPEC);

    return current_index;
}

void op_set(Receptor *r, Xaddr xaddr, void *value) {
    void *surface = &r->data.cache[xaddr.key];
    if (!sem_check(r, xaddr)) {
        raise_error("I do not think that word (%d) means what you think it means!\n", xaddr.noun);
    }
    size_t size = _get_noun_size(r, xaddr.noun, value);

    memcpy(surface, value, size);
}

Xaddr op_new(Receptor *r, Symbol noun, void *surface) {
    size_t current_index = r->data.cache_index;
    r->data.cache_index += _get_noun_size(r, noun, surface);

    _record_existence(r,current_index,noun);

    Xaddr new_xaddr = {current_index, noun};
    op_set(r, new_xaddr, surface);
    return new_xaddr;
}

void _init_element(Receptor *r, char *label,Symbol element_spec,ElementSurface *es,int processCount, Process *processes) {
    int i;
    Xaddr elemX;
    elemX.key = element_spec;
    elemX.noun = CSPEC;

    es->name = op_new_noun(r, elemX, label);
    es->process_count = processCount;
    Process *p = &es->processes;
    for (i = 0; i < processCount; i++) {
        p->name = processes[i].name;
        p->function = processes[i].function;
        p++;
    }
}

void *op_get_array_nth(Receptor *r,int index, Xaddr rX) {
    void *surface = op_get(r,rX);
    int length = _op_get_array_length(surface);
    if (index >= length){
        raise_error2("index %d into array %d greater than length\n",rX.key,index);
    }
    surface += sizeof(int);
    if (index > 0) {

        Symbol nounSpecType;
        ElementSurface *rs = _get_noun_element_spec(r,&nounSpecType,rX.noun);
        if (nounSpecType != ARRAY_SPEC) {
            raise_error2("xaddr points to a %d, expected array(%d)\n",nounSpecType,ARRAY_SPEC);
        }
        Symbol repsNoun = REPS_GET_NOUN(rs);
        Symbol arrayItemType;
        ElementSurface *es = _get_noun_element_spec(r,&arrayItemType,repsNoun);
        int size;
        if (arrayItemType == PATTERN_SPEC) {
            size = PATTERN_GET_SIZE(es);
            surface += size*index;
        }
        else if (arrayItemType == ARRAY_SPEC) {
            while (index--) {
                surface += _get_noun_size(r,repsNoun,surface);
            }
        }
        else {raise_error("bad array item type %d\n",arrayItemType);}
    }
    return surface;
}

#define BUFFER_SIZE 10000
Xaddr _op_new_rep(Receptor *r, Symbol rep_type, char *label, Symbol repsNoun, int processCount, Process *processes){
    char rs[BUFFER_SIZE];
    memset(rs, 0, BUFFER_SIZE);
    _init_element(r,label,rep_type,(ElementSurface *)rs,processCount,processes);
    REPS_SET_NOUN(rs,repsNoun);
    return op_new(r, rep_type, rs);
}

Xaddr op_new_string(Receptor *r, char *label, Symbol repsNoun, int processCount, Process *processes){
    return _op_new_rep(r,STRING_SPEC,label,repsNoun,processCount,processes);
}

Xaddr op_new_array(Receptor *r, char *label, Symbol repsNoun, int processCount, Process *processes){
    return _op_new_rep(r,ARRAY_SPEC,label,repsNoun,processCount,processes);
}

Xaddr op_new_pattern(Receptor *r, char *label, int childCount, Xaddr *children, int processCount, Process *processes) {
    char ps[BUFFER_SIZE];
    memset(ps, 0, BUFFER_SIZE);
    int i;
    _init_element(r,label,PATTERN_SPEC,(ElementSurface *)ps,processCount,processes);

    if (children == 0) {
        PATTERN_SET_SIZE(ps,childCount);
    } else {
        NounSurface *noun;
        ElementSurface *child_pattern_surface;
        int size = 0;
        Offset *pschildren = PATTERN_GET_CHILDREN(ps);
        for (i = 0; i < childCount; i++) {
            if (children[i].noun == NOUN_SPEC) {
                noun = (NounSurface *) op_get(r, children[i]);
                child_pattern_surface = (ElementSurface *) op_get(r, noun->namedElement);
            } else if (children[i].noun == PATTERN_SPEC) {
                child_pattern_surface = (ElementSurface *) op_get(r, children[i]);
            } else {
                raise_error("Unknown child element type %d\n", children[i].noun);
            }
            pschildren[i].noun.key = children[i].key;
            pschildren[i].noun.noun = children[i].noun;
            pschildren[i].offset = size;
            size += PATTERN_GET_SIZE(child_pattern_surface);
        }
        PATTERN_SET_SIZE(ps,size);
        PATTERN_SET_CHILDREN_COUNT(ps,childCount);
    }
    return op_new(r, PATTERN_SPEC, ps);
}

ElementSurface *_get_pattern_spec(Receptor *r, Symbol patternName) {
    Xaddr px = {patternName, PATTERN_SPEC};
    return (ElementSurface *) op_get(r, px);
}

int op_exec(Receptor *r, Xaddr xaddr, FunctionName processName) {
    ElementSurface *ps = _get_noun_pattern_spec(r, xaddr.noun);
    Process *p = getProcess(ps, processName);
    return (*p->function)(r, xaddr.noun, op_get(r, xaddr));
}

int op_push_pattern(Receptor *r, Symbol patternName, void *surface) {
    SemStackFrame *ssf = &r->semStack[++r->semStackPointer];
    ssf->type.key = patternName;
    ssf->type.noun = PATTERN_SPEC;
    ElementSurface *ps = _get_pattern_spec(r, patternName);
    ssf->size = PATTERN_GET_SIZE(ps);
    memcpy(&r->valStack[r->valStackPointer], surface, ssf->size);
    r->valStackPointer += ssf->size;
    return 0;
}

#define NULL_XADDR(x) ((x.noun)==(x.key))

int getOffset(ElementSurface *ps, Symbol name) {
    Offset *o = PATTERN_GET_CHILDREN(ps);
    while (!NULL_XADDR(o->noun)) {
        if (o->noun.key == name) {
            return o->offset;
        }
        o++;
    }
    raise_error2("offset not found for: %d in getOffset for patternSpec %d\n", name, ps->name);
    return 0;
}

ElementSurface *walk_path(Receptor *r, Xaddr xaddr, Symbol *path, int *offset) {
    ElementSurface *ps = _get_noun_pattern_spec(r, xaddr.noun);
    *offset = 0;
    int i = 0;
    while (path[i] != SYMBOL_PATH_TERMINATOR) {
        *offset += getOffset(ps, path[i]);
        ps = _get_noun_pattern_spec(r, path[i]);
        i++;
    }
    return ps;
}

void *op_set_by_path(Receptor *r, Xaddr xaddr, Symbol *path, void *value) {
    int offset;
    ElementSurface *ps = walk_path(r, xaddr, path, &offset);
    void *surface = &r->data.cache[xaddr.key + offset];
    return memcpy(surface, value, PATTERN_GET_SIZE(ps));
}

void *op_get_by_path(Receptor *r, Xaddr xaddr, Symbol *path) {
    int offset;
    walk_path(r, xaddr, path, &offset);
    return &r->data.cache[xaddr.key + offset];
}



int proc_int_inc(Receptor *r, Symbol noun, void *this) {
    ++*(int *) (this);
    return 0;
}

int proc_int_add(Receptor *r, Symbol noun, void *this) {
    // semCheck please
    int *stackSurface = (int *) &r->valStack[r->valStackPointer - r->semStack[r->semStackPointer].size];
    *stackSurface = *(int *) this + *stackSurface;
    return 0;
}

int proc_int_print(Receptor *r, Symbol noun, void *this) {
    printf("%d", *(int *) this);
    return 0;
}

int proc_point_print(Receptor *r, Symbol noun, void *this) {
    printf("%d,%d", *(int *) this, *(((int *) this) + 1));
    return 0;
}

int proc_line_print(Receptor *r, Symbol noun, void *this) {
    int *surface = (int *) this;
    printf("[%d,%d - %d,%d] ", *surface, *(surface + 1), *(surface + 2), *(surface + 3));
    return 0;
}

typedef struct {
    Symbol name;
    int valueOffset;
    // pad to size of full operand
    char padding[OPERANDS_SIZE  - sizeof(Symbol) - sizeof(int)];
} ImmediatePatternOperand;

int run(Receptor *r, Instruction *instructions, void *values) {
    int counter = 0;
    ImmediatePatternOperand *op;
    while (1) {
        switch (instructions[counter].opcode) {
            case RETURN:
                return 0;
            case PUSH_IMMEDIATE:
                op = (ImmediatePatternOperand *) &instructions[counter].operands;
                op_push_pattern(r, op->name, (char *) values + op->valueOffset);
                break;
        }
        counter++;
    }
}

//FIXME: this is needs to be implemented as a scape, not a linear scan of all Xaddrs!!
Symbol getSymbol(Receptor *r, char *label) {
    NounSurface *ns;
    Symbol noun;
    int i;
    for (i = 0; i <= r->data.current_xaddr; i++) {
        if (r->data.xaddrs[i].noun == NOUN_SPEC) {
            noun = r->data.xaddrs[i].key;
            ns = (NounSurface *) &r->data.cache[noun];
            if (!strcmp(label, ns->label)) {
                return noun;
            }
        }
    }
    return 0;
}

int proc_cspec_get_size(Receptor *r, Symbol noun, void*this){
    if (noun == PATTERN_SPEC) {
        return _get_pattern_spec_size((ElementSurface *)this);
    } else if ((noun == ARRAY_SPEC) || (noun == STRING_SPEC)) {
        return element_header_size(this) + sizeof(RepsBody);
    }
    return 0;
}

void init(Receptor *r) {
    int i;
    for (i = 0; i < DEFAULT_CACHE_SIZE; i++) r->data.xaddr_scape[i] = NULL_SYMBOL;

    r->data.cache_index = 0;
    r->semStackPointer = -1;
    r->valStackPointer = 0;
    r->patternSpecXaddr.key = PATTERN_SPEC;
    r->patternSpecXaddr.noun = CSPEC;
    r->arraySpecXaddr.key = ARRAY_SPEC;
    r->arraySpecXaddr.noun = CSPEC;
    r->stringSpecXaddr.key = STRING_SPEC;
    r->stringSpecXaddr.noun = CSPEC;
    r->data.current_xaddr = -1;

    // ********************** bootstrap built in types

    Process pattern_spec_procs[] = {{GET_SIZE, (ProcessFunc)&proc_pattern_get_size }};
    Process array_spec_procs[] =  {{GET_SIZE, (ProcessFunc)&proc_array_get_size }};
    Process string_spec_procs[] = {{GET_SIZE, (ProcessFunc)&proc_string_get_size }};
    Process cspec_spec_procs[] = {{GET_SIZE, (ProcessFunc)&proc_cspec_get_size }};

    ElementSurface builtin_element_surfaces[] = {
        { NULL_SYMBOL, 0 , {}},
        { CSPEC, 1, cspec_spec_procs[0]},
        { NOUN_SPEC, 0 , {}},
        { PATTERN_SPEC, 1, pattern_spec_procs[0]},
        { ARRAY_SPEC, 1, array_spec_procs[0] },
        { STRING_SPEC, 1, string_spec_procs[0] }
    };
    r->builtin_element_surfaces[0] = builtin_element_surfaces[0];
    r->builtin_element_surfaces[1] = builtin_element_surfaces[1];
    r->builtin_element_surfaces[2] = builtin_element_surfaces[2];
    r->builtin_element_surfaces[3] = builtin_element_surfaces[3];
    r->builtin_element_surfaces[4] = builtin_element_surfaces[4];
    r->builtin_element_surfaces[5] = builtin_element_surfaces[5];


    // INT
    Process int_processes[] = {
        {PRINT, (ProcessFunc)&proc_int_print},
        {INC, (ProcessFunc)&proc_int_inc},
        {ADD, (ProcessFunc)&proc_int_add}
    };
    r->intPatternSpecXaddr = op_new_pattern(r, "INT", sizeof(int), 0, 3, int_processes);

    // POINT
    Symbol X = op_new_noun(r, r->intPatternSpecXaddr, "X");
    Symbol Y = op_new_noun(r, r->intPatternSpecXaddr, "Y");

    Process point_processes[] = {
        {PRINT, (ProcessFunc)&proc_point_print}
    };

    // LINE
    Xaddr point_children[2] = {{X, NOUN_SPEC}, {Y, NOUN_SPEC}};
    r->pointPatternSpecXaddr = op_new_pattern(r, "POINT", 2, point_children, 1, point_processes);

    Symbol A = op_new_noun(r, r->pointPatternSpecXaddr, "A");
    Symbol B = op_new_noun(r, r->pointPatternSpecXaddr, "B");

    Xaddr line_children[2] = {{A, NOUN_SPEC}, {B, NOUN_SPEC}};

    Process line_processes[] = {
        {PRINT, (ProcessFunc)&proc_line_print}
    };

    r->linePatternSpecXaddr = op_new_pattern(r, "LINE", 2, line_children, 1, line_processes);

}

// utilities

Xaddr noun_to_xaddr(Symbol noun) {
    Xaddr nounXaddr = {noun, NOUN_SPEC};
    return nounXaddr;
}

char *noun_label(Receptor *r, Symbol noun) {
    NounSurface *ns = (NounSurface *) op_get(r, noun_to_xaddr(noun));
    return ns->label;
}

#endif
