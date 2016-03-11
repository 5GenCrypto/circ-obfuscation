#include "circuit.h"
#include "clt13.h"
#include "obfuscate.h"
#include "util.h"
#include "level.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

// obfuscate
// save circuit w/o consts
// evaluate

extern int yyparse();
extern FILE *yyin;
int extern g_verbose;

size_t chunker_group_in_order (size_t input_num, size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    return floor((double)input_num / (double) chunksize);
}

int main( int argc, char **argv ){
    ++argv, --argc;
    if (argc > 0)
        yyin = fopen( argv[0], "r" );

    /*char dir[100];*/
    /*int lambda = 4;*/
    /*sprintf(dir, "%s.%d", argv[0], lambda);*/

    /*// make the obfuscation directory if it doesn't exist already*/
    /*struct stat st = {0};*/
    /*if (stat(dir, &st) == -1) mkdir(dir, 0700);*/

    g_verbose = 1;

    circuit c;
    circ_init(&c);
    yyparse(&c);

    printf("circuit: ninputs=%lu nconsts=%lu ngates=%lu ntests=%lu nrefs=%lu\n",
                     c.ninputs, c.nconsts, c.ngates, c.ntests, c.nrefs);
    ensure(&c);

    size_t nsyms = 8;
    uint32_t *type = calloc(nsyms + 1, sizeof(uint32_t));
    for (int i = 0; i < c.noutputs; i++) {
        type_degree(type, c.outrefs[i], &c, nsyms, chunker_group_in_order);
        printf("c=%lu o=%d type=", nsyms, i);
        print_array(type, nsyms + 1);
        puts("");
    }
    free(type);

    /*level_params lp = { 4, 4, 2, NULL };*/
    /*level vstar;*/
    /*level_init(&vstar, &lp);*/
    /*level_set_vstar(&vstar);*/
    /*level_print(&vstar);*/
    /*level_clear(&vstar);*/

    /*clt_state mmap;*/
    /*clt_setup(&mmap, get_kappa(&c), lambda + depth(&c, c.outref), nzs, tl, dir, true);*/
    /*obfuscate(&mmap, &c);*/
    /*clt_state_clear(&mmap);*/

    /*circ_clear(&c);*/
    return 0;
}
