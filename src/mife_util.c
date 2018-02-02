#include "mife_util.h"
#include "util.h"

int
mife_ct_write(const mife_vtable *vt, mife_ct_t *ct, const char *ctname,
              const obf_params_t *op)
{
    FILE *fp = NULL;
    int ret = ERR;
    if ((fp = fopen(ctname, "w")) == NULL) {
        fprintf(stderr, "%s: %s: unable to open '%s' for writing\n",
                errorstr, __func__, ctname);
        goto cleanup;
    }
    if (vt->mife_ct_fwrite(ct, op, fp) == ERR) {
        fprintf(stderr, "%s: %s: unable to write ciphertext to disk\n",
                errorstr, __func__);
        goto cleanup;
    }
    ret = OK;
cleanup:
    if (fp)
        fclose(fp);
    return ret;
}
