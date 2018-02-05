#include "mife_util.h"
#include "util.h"

int
mife_ct_write(const mife_vtable *vt, mife_ct_t *ct, const char *ctname)
{
    FILE *fp = NULL;
    int ret = ERR;
    if ((fp = fopen(ctname, "w")) == NULL) {
        fprintf(stderr, "%s: %s: unable to open '%s' for writing\n",
                errorstr, __func__, ctname);
        goto cleanup;
    }
    if (vt->mife_ct_fwrite(ct, fp) == ERR) {
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

mife_ct_t *
mife_ct_read(const mmap_vtable *mmap, const mife_vtable *vt,
             const mife_ek_t *ek, const char *ctname)
{
    mife_ct_t *ct = NULL;
    FILE *fp = NULL;
    if ((fp = fopen(ctname, "r")) == NULL) {
        fprintf(stderr, "%s: %s: unable to open '%s' for reading\n",
                errorstr, __func__, ctname);
        goto cleanup;
    }
    if ((ct = vt->mife_ct_fread(mmap, ek, fp)) == NULL) {
        fprintf(stderr, "%s: %s: unable to read ciphertext '%s'\n",
                errorstr, __func__, ctname);
        goto cleanup;
    }
cleanup:
    if (fp)
        fclose(fp);
    return ct;
}
