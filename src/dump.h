#import "ceptr.h"

void dump_hex(char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = addr;

    // Output description if given.
    if (desc != NULL)
        printf("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf("  %s\n", buff);

            // Output the offset.
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
}

void dump_children_array(Offset *children,int count) {
    int i = 0;
    while (i < count) {
        if (i != 0) {
            printf(",");
        }
        printf("{ %d, %d }(%d)", children[i].noun.key, children[i].noun.noun, children[i].offset);
        i++;
    }
}


void dump_process_array(Process *process,int count) {
    int i = 0;
    while (i < count) {
        if (i != 0) {
            printf(",");
        }
        printf("{ %d, %zu }", process[i].name, (size_t) process[i].function);
        i++;
    }
}

void dump_reps_spec(Receptor *r, ElementSurface *rs,char * type){
    printf("%s Spec\n",type);
    printf("    name: %s(%d)\n", noun_label(r, rs->name), rs->name);
    int noun = REPS_GET_NOUN(rs);
    printf("    repsNoun: %s(%d) \n", noun_label(r,noun),noun);
//    dump_process_array(rs->processes);
    printf("\n");
}

void dump_pattern_spec(Receptor *r, ElementSurface *ps) {
    printf("Pattern Spec\n");
    printf("    name: %s(%d)\n", noun_label(r, ps->name), ps->name);
    printf("    size: %d\n", (int) PATTERN_GET_SIZE(ps));
    int count = PATTERN_GET_CHILDREN_COUNT(ps);
    printf("    %d children: ",count);
    dump_children_array(PATTERN_GET_CHILDREN(ps),count);
    printf("\n    %d processes: ",ps->process_count);
    dump_process_array(&ps->processes,ps->process_count);
    printf("\n");
}

void dump_pattern_value(Receptor *r, ElementSurface *ps, Symbol noun, void *surface) {
    Process *print_proc;
    print_proc = getProcess(ps, PRINT);
    if (print_proc) {
        (*print_proc->function)(r, noun, surface);
    } else {
        dump_hex("hexDump of surface", surface, PATTERN_GET_SIZE(ps));
    }
}

void dump_array_value(Receptor *r, ElementSurface *rs, void *surface) {
    int count = *(int*)surface;
    surface += sizeof(int);
    Symbol arrayItemType;
    Symbol repsNoun = REPS_GET_NOUN(rs);
    ElementSurface *es = _get_noun_element_spec(r,&arrayItemType,repsNoun);
    int size;
    if (arrayItemType == PATTERN_SPEC) {
        printf("%s(%d) array of %d %s(%d)s\n",noun_label(r,rs->name),rs->name,count,noun_label(r,repsNoun),repsNoun );
        size = PATTERN_GET_SIZE(es);
        while (count > 0) {
            printf("    ");
            dump_pattern_value(r,es,repsNoun,surface);
            printf("\n");
            surface += size;
            count--;
        }
    }
    else if (arrayItemType == ARRAY_SPEC) {
        printf("array of %d %s(%d) arrays\n", count,noun_label(r,rs->name),rs->name);
        while (count > 0) {
            printf("    ");
            dump_array_value(r,es,surface);
            surface += _new_get_noun_size(r,repsNoun,surface);
            count--;
        }
    }
}

void dump_string_value(Receptor *r, ElementSurface *rs, void *surface) {
    int count = 0;
    ElementSurface *ps = _get_reps_pattern_spec(r,rs);
    Symbol repsNoun = REPS_GET_NOUN(rs);
    printf(" %s(%d) string of %s(%d)\n",noun_label(r,rs->name),rs->name,noun_label(r,repsNoun),repsNoun);
    while (*(int *)surface != STRING_TERMINATOR) {
        if (*(int *)surface == ESCAPE_STRING_TERMINATOR) {
            surface += sizeof(int);
        }
        printf("    ");
        dump_pattern_value(r,ps,repsNoun,surface);
        printf("\n");
        surface += PATTERN_GET_SIZE(ps);
        count++;
    }
    printf(" %d elements found\n",count);
}


void dump_noun(Receptor *r, NounSurface *ns) {
    printf("Noun      { %d, %5d } %s", ns->namedElement.key, ns->namedElement.noun, ns->label);
}

void dump_xaddr(Receptor *r, Xaddr xaddr, int indent_level) {
    int i;
    ElementSurface *es;
    NounSurface *ns;
    void *surface;
    int key = xaddr.key;
    int noun = xaddr.noun;
    if (1) {
	if (noun == ELEMENT) {

	}
        else if (noun == PATTERN_SPEC) {
            dump_pattern_spec(r, (ElementSurface *)&r->data.cache[key]);
        }
        else if(noun == NOUN_SPEC){
            ns = (NounSurface *) &r->data.cache[key];
            dump_noun(r, ns);
        }
	else if (noun == ARRAY_SPEC){
            dump_reps_spec(r, (ElementSurface *)&r->data.cache[key], "Array");
	}
	else if (noun == STRING_SPEC) {
            dump_reps_spec(r, (ElementSurface *)&r->data.cache[key], "String");
        }
	else {
            surface = op_get(r, noun_to_xaddr(noun));
            ns = (NounSurface *) surface;
            printf("%s : ", ns->label);
            es = (ElementSurface *) op_get(r, ns->namedElement);
	    Symbol n = ns->namedElement.noun;
		if (n  == PATTERN_SPEC) {
                    dump_pattern_value(r, es, noun, op_get(r, xaddr));
                }
                if (n == ARRAY_SPEC) {
                    dump_array_value(r,es,op_get(r,xaddr));
                }
                if (n == STRING_SPEC) {
                    dump_string_value(r,es,op_get(r,xaddr));
		}
            }
    }
}

void dump_xaddrs(Receptor *r) {
    int i;
    NounSurface *ns;
    void *surface;
    for (i = 0; i <= r->data.current_xaddr; i++) {
        printf("Xaddr { %5d, %5d } - ", r->data.xaddrs[i].key, r->data.xaddrs[i].noun);
        dump_xaddr(r, r->data.xaddrs[i], 0);
        printf("\n");
    }
}

void dump_stack(Receptor *r) {
    int i, v = 0;
    char *unknown = "<unknown>";
    char *label;
    ElementSurface *ps;
    for (i = 0; i <= r->semStackPointer; i++) {
        Xaddr type = r->semStack[i].type;
        if (type.noun == PATTERN_SPEC) {
            ps = (ElementSurface *) &r->data.cache[type.key];
            label = noun_label(r, ps->name);

        }
        else {
            label = unknown;
        }
        printf("\nStack frame: %d is a %s({%d,%d}) size:%d\n", i, label, type.key, type.noun, r->semStack[i].size);
        printf("   Value:");
        dump_pattern_value(r, ps, type.noun, &r->valStack[v]);
        v += r->semStack[i].size;
        printf("\n");
    }

}
