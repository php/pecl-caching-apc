/* 
   +----------------------------------------------------------------------+
   | Copyright (c) 2002 by Community Connect Inc. All rights reserved.    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/3_0.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "apc.h"
#include <regex.h>      /* for POSIX regular expressions */

#define NELEMS(a) (sizeof(a)/sizeof((a)[0]))

/* {{{ memory allocation wrappers */

void* apc_emalloc(size_t n)
{
    void* p = malloc(n);
    if (p == NULL) {
        apc_eprint("apc_emalloc: malloc failed to allocate %u bytes:", n);
    }
    return p;
}

void* apc_erealloc(void* p, size_t n)
{
    p = realloc(p, n);
    if (p == NULL) {
        apc_eprint("apc_erealloc: realloc failed to allocate %u bytes:", n);
    }
    return p;
}

void apc_efree(void* p)
{
    if (p == NULL) {
        apc_eprint("apc_efree: attempt to free null pointer");
    }
    free(p);
}

char* apc_estrdup(const char* s)
{
    int len;
    char* dup;

    if (s == NULL) {
        return NULL;
    }
    len = strlen(s);
    dup = (char*) malloc(len+1);
    if (dup == NULL) {
        apc_eprint("apc_estrdup: malloc failed to allocate %u bytes:", len+1);
    }
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

void* apc_xstrdup(const char* s, apc_malloc_t f)
{
    return s != NULL ? apc_xmemcpy(s, strlen(s)+1, f) : NULL;
}

void* apc_xmemcpy(const void* p, size_t n, apc_malloc_t f)
{
    void* q;

    if (p != NULL && (q = f(n)) != NULL) {
        memcpy(q, p, n);
        return q;
    }
    return NULL;
}

/* }}} */

/* {{{ console display functions */

static void my_log(int level, const char* fmt, va_list args)
{
    static const char* level_strings[] = {
        "apc-debug",
        "apc-notice",
        "apc-warning",
        "apc-error"
    };
    static const int num_levels = NELEMS(level_strings);

    time_t now;
    char* buf;          /* for ctime */

    fflush(stdout);

    if (level < 0)
        level = 0;
    else if (level >= num_levels)
        level = num_levels-1;
    
    now = time(0);
    buf = ctime(&now);  /* TODO: replace with reentrant impl */
    buf[24] = '\0';

    fprintf(stderr, "[%s] [%s] ", buf, level_strings[level]);
    vfprintf(stderr, fmt, args);

    if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':') {
        fprintf(stderr, " %s", strerror(errno));
    }
    fprintf(stderr, "\n");

    if (level == APC_ERROR) {
        exit(2);
    }
}

void apc_log(int level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    my_log(level, fmt, args);
    va_end(args);
}

void apc_eprint(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    my_log(APC_ERROR, fmt, args);
    va_end(args);
}

void apc_wprint(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    my_log(APC_WARNING, fmt, args);
    va_end(args);
}

void apc_dprint(const char* fmt, ...)
{
#ifdef APC_DBG
    va_list args;
    va_start(args, fmt);
    my_log(APC_DBG, fmt, args);
    va_end(args);
#endif
}

/* }}} */

/* {{{ string and text manipulation */

char* apc_append(const char* s, const char* t)
{
    int slen;
    int tlen;
    char* p;

    slen = strlen(s);
    tlen = strlen(t);

    p = (char*) apc_emalloc((slen + tlen + 1) * sizeof(char));
    memcpy(p, s, slen);
    memcpy(p + slen, t, tlen + 1);

    return p;
}

char* apc_substr(const char* s, int start, int length)
{
    char* substr;
    int src_len = strlen(s);

    /* bring start into range */
    if (start < 0) {
        start = 0;
    }
    else if (start >= src_len) {
        start = src_len - 1;
    }

    /* bring length into range */
    if (length < 0 || src_len - start < length) {
        length = src_len - start;
    }

    /* create the substring */
    substr = apc_xmemcpy(s + start, length + 1, apc_emalloc);
    substr[length] = '\0';
    return substr;
}

char** apc_tokenize(const char* s, char delim)
{
    char** tokens;      /* array of tokens, NULL terminated */
    int size;           /* size of tokens array */
    int n;              /* index of next token in tokens array */
    int cur;            /* current position in input string */
    int end;            /* final legal position in input string */
    int next;           /* position of next delimiter in input */
    
    if (!s) {
        return NULL;
    }

    size = 2;
    n    = 0;
    cur  = 0;
    end  = strlen(s) - 1;
    
    tokens = (char**) apc_emalloc(size * sizeof(char*));
    tokens[n] = NULL;

    while (cur <= end) {
        /* search for the next delimiter */
        char* p = strchr(s + cur, delim);
        next = p ? p-s : end+1;

        /* resize token array if necessary */
        if (n == size-1) {
            size *= 2;
            tokens = (char**) apc_erealloc(tokens, size * sizeof(char*));
        }

        /* save the current token */
        tokens[n] = apc_substr(s, cur, next-cur);

        tokens[++n] = NULL;
        cur = next + 1;
    }

    return tokens;
}

/* }}} */

/* {{{ filesystem functions */

int apc_stat_paths(const char* filename, const char* path, struct stat* buf)
{
    char filepath[1024];
    char** paths;
    int found = 0;
    int i;

    assert(filename && buf);

    paths = apc_tokenize(path, ':');    /* TODO - on windows, it's ';' */
    if (!paths)
        return -1;

    /* for each directory in paths, look for filename inside */
    for (i = 0; paths[i]; i++) {
        snprintf(filepath, sizeof(filepath), "%s/%s", paths[i], filename);
        if (stat(filepath, buf) == 0) {
            found = 1;
            break;
        }
    }

    /* free the value returned by apc_tokenize */
    for (i = 0; paths[i]; i++) {
        apc_efree(paths[i]);
    }
    apc_efree(paths);

    return found ? 0 : -1;
}

/* }}} */

/* {{{ regular expression wrapper functions */

typedef struct {
    regex_t *reg;
    unsigned char type;
} apc_regex;

void* apc_regex_compile_array(char* patterns[])
{
    apc_regex** regs;
    int npat;
    int i;

    if (!patterns)
        return NULL;

    /* count the number of patterns in patterns */
    for (npat = 0; patterns[npat] != NULL; npat++) {}

    if (npat == 0)
        return NULL;

    /* allocate the array of compiled expressions */
    regs = (apc_regex**) apc_emalloc(sizeof(apc_regex*) * (npat + 1));
    for (i = 0; i <= npat; i++) {
        regs[i] = (apc_regex *) apc_emalloc(sizeof(apc_regex));
        regs[i]->reg = NULL;
        regs[i]->type = APC_NEGATIVE_MATCH;
    }

    /* compile the expressions */
    for (i = 0; i < npat; i++) {
        char *pattern = patterns[i];
        if(pattern[0]=='+') { regs[i]->type = APC_POSITIVE_MATCH; pattern = patterns[i]+sizeof(char); }
        else if(pattern[0]=='-') { regs[i]->type = APC_NEGATIVE_MATCH; pattern = patterns[i]+sizeof(char); }

        regs[i]->reg = (regex_t*) apc_emalloc(sizeof(regex_t));

        if (regcomp(regs[i]->reg, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
            apc_wprint("apc_regex_compile_array: invalid expression '%s'",
                       pattern);

            apc_efree(regs[i]->reg);
            apc_efree(regs[i]);
            apc_regex_destroy_array(regs);

            return NULL;
        }
    }

    return (void*) regs;
}

void apc_regex_destroy_array(void* p)
{
    if (p != NULL) {
        apc_regex** regs = (apc_regex**) p;
        int i;

        for (i = 0; regs[i]->reg != NULL; i++) {
            regfree(regs[i]->reg);
            apc_efree(regs[i]->reg);
            apc_efree(regs[i]);
        }
        apc_efree(regs);
    }
}

int apc_regex_match_array(void* p, const char* input)
{
    apc_regex** regs;
    int i;

    if (!p)
        return 0;

    regs = (apc_regex**) p;
    for (i = 0; regs[i]->reg != NULL; i++)
        if (regexec(regs[i]->reg, input, 0, NULL, 0) == 0)
            return (int)(regs[i]->type);

    return 0;
}

/* }}} */

/* {{{ crc32 implementation */

/* this table was generated by crc32gen() */
static unsigned int crc32tab[] = {
    /*   0 */  0x00000000, 0x3b83984b, 0x77073096, 0x4c84a8dd, 
    /*   4 */  0xee0e612c, 0xd58df967, 0x990951ba, 0xa28ac9f1, 
    /*   8 */  0x076dc419, 0x3cee5c52, 0x706af48f, 0x4be96cc4, 
    /*  12 */  0xe963a535, 0xd2e03d7e, 0x9e6495a3, 0xa5e70de8, 
    /*  16 */  0x0edb8832, 0x35581079, 0x79dcb8a4, 0x425f20ef, 
    /*  20 */  0xe0d5e91e, 0xdb567155, 0x97d2d988, 0xac5141c3, 
    /*  24 */  0x09b64c2b, 0x3235d460, 0x7eb17cbd, 0x4532e4f6, 
    /*  28 */  0xe7b82d07, 0xdc3bb54c, 0x90bf1d91, 0xab3c85da, 
    /*  32 */  0x1db71064, 0x2634882f, 0x6ab020f2, 0x5133b8b9, 
    /*  36 */  0xf3b97148, 0xc83ae903, 0x84be41de, 0xbf3dd995, 
    /*  40 */  0x1adad47d, 0x21594c36, 0x6ddde4eb, 0x565e7ca0, 
    /*  44 */  0xf4d4b551, 0xcf572d1a, 0x83d385c7, 0xb8501d8c, 
    /*  48 */  0x136c9856, 0x28ef001d, 0x646ba8c0, 0x5fe8308b, 
    /*  52 */  0xfd62f97a, 0xc6e16131, 0x8a65c9ec, 0xb1e651a7, 
    /*  56 */  0x14015c4f, 0x2f82c404, 0x63066cd9, 0x5885f492, 
    /*  60 */  0xfa0f3d63, 0xc18ca528, 0x8d080df5, 0xb68b95be, 
    /*  64 */  0x3b6e20c8, 0x00edb883, 0x4c69105e, 0x77ea8815, 
    /*  68 */  0xd56041e4, 0xeee3d9af, 0xa2677172, 0x99e4e939, 
    /*  72 */  0x3c03e4d1, 0x07807c9a, 0x4b04d447, 0x70874c0c, 
    /*  76 */  0xd20d85fd, 0xe98e1db6, 0xa50ab56b, 0x9e892d20, 
    /*  80 */  0x35b5a8fa, 0x0e3630b1, 0x42b2986c, 0x79310027, 
    /*  84 */  0xdbbbc9d6, 0xe038519d, 0xacbcf940, 0x973f610b, 
    /*  88 */  0x32d86ce3, 0x095bf4a8, 0x45df5c75, 0x7e5cc43e, 
    /*  92 */  0xdcd60dcf, 0xe7559584, 0xabd13d59, 0x9052a512, 
    /*  96 */  0x26d930ac, 0x1d5aa8e7, 0x51de003a, 0x6a5d9871, 
    /* 100 */  0xc8d75180, 0xf354c9cb, 0xbfd06116, 0x8453f95d, 
    /* 104 */  0x21b4f4b5, 0x1a376cfe, 0x56b3c423, 0x6d305c68, 
    /* 108 */  0xcfba9599, 0xf4390dd2, 0xb8bda50f, 0x833e3d44, 
    /* 112 */  0x2802b89e, 0x138120d5, 0x5f058808, 0x64861043, 
    /* 116 */  0xc60cd9b2, 0xfd8f41f9, 0xb10be924, 0x8a88716f, 
    /* 120 */  0x2f6f7c87, 0x14ece4cc, 0x58684c11, 0x63ebd45a, 
    /* 124 */  0xc1611dab, 0xfae285e0, 0xb6662d3d, 0x8de5b576, 
    /* 128 */  0x76dc4190, 0x4d5fd9db, 0x01db7106, 0x3a58e94d, 
    /* 132 */  0x98d220bc, 0xa351b8f7, 0xefd5102a, 0xd4568861, 
    /* 136 */  0x71b18589, 0x4a321dc2, 0x06b6b51f, 0x3d352d54, 
    /* 140 */  0x9fbfe4a5, 0xa43c7cee, 0xe8b8d433, 0xd33b4c78, 
    /* 144 */  0x7807c9a2, 0x438451e9, 0x0f00f934, 0x3483617f, 
    /* 148 */  0x9609a88e, 0xad8a30c5, 0xe10e9818, 0xda8d0053, 
    /* 152 */  0x7f6a0dbb, 0x44e995f0, 0x086d3d2d, 0x33eea566, 
    /* 156 */  0x91646c97, 0xaae7f4dc, 0xe6635c01, 0xdde0c44a, 
    /* 160 */  0x6b6b51f4, 0x50e8c9bf, 0x1c6c6162, 0x27eff929, 
    /* 164 */  0x856530d8, 0xbee6a893, 0xf262004e, 0xc9e19805, 
    /* 168 */  0x6c0695ed, 0x57850da6, 0x1b01a57b, 0x20823d30, 
    /* 172 */  0x8208f4c1, 0xb98b6c8a, 0xf50fc457, 0xce8c5c1c, 
    /* 176 */  0x65b0d9c6, 0x5e33418d, 0x12b7e950, 0x2934711b, 
    /* 180 */  0x8bbeb8ea, 0xb03d20a1, 0xfcb9887c, 0xc73a1037, 
    /* 184 */  0x62dd1ddf, 0x595e8594, 0x15da2d49, 0x2e59b502, 
    /* 188 */  0x8cd37cf3, 0xb750e4b8, 0xfbd44c65, 0xc057d42e, 
    /* 192 */  0x4db26158, 0x7631f913, 0x3ab551ce, 0x0136c985, 
    /* 196 */  0xa3bc0074, 0x983f983f, 0xd4bb30e2, 0xef38a8a9, 
    /* 200 */  0x4adfa541, 0x715c3d0a, 0x3dd895d7, 0x065b0d9c, 
    /* 204 */  0xa4d1c46d, 0x9f525c26, 0xd3d6f4fb, 0xe8556cb0, 
    /* 208 */  0x4369e96a, 0x78ea7121, 0x346ed9fc, 0x0fed41b7, 
    /* 212 */  0xad678846, 0x96e4100d, 0xda60b8d0, 0xe1e3209b, 
    /* 216 */  0x44042d73, 0x7f87b538, 0x33031de5, 0x088085ae, 
    /* 220 */  0xaa0a4c5f, 0x9189d414, 0xdd0d7cc9, 0xe68ee482, 
    /* 224 */  0x5005713c, 0x6b86e977, 0x270241aa, 0x1c81d9e1, 
    /* 228 */  0xbe0b1010, 0x8588885b, 0xc90c2086, 0xf28fb8cd, 
    /* 232 */  0x5768b525, 0x6ceb2d6e, 0x206f85b3, 0x1bec1df8, 
    /* 236 */  0xb966d409, 0x82e54c42, 0xce61e49f, 0xf5e27cd4, 
    /* 240 */  0x5edef90e, 0x655d6145, 0x29d9c998, 0x125a51d3, 
    /* 244 */  0xb0d09822, 0x8b530069, 0xc7d7a8b4, 0xfc5430ff, 
    /* 248 */  0x59b33d17, 0x6230a55c, 0x2eb40d81, 0x153795ca, 
    /* 252 */  0xb7bd5c3b, 0x8c3ec470, 0xc0ba6cad, 0xfb39f4e6, 
};

unsigned int apc_crc32(const char* buf, int len)
{
    int i;
    int k;
    unsigned int crc;

    /* preconditioning */
    crc = 0xFFFFFFFF;
    
    for (i = 0; i < len; i++) {
        k = (crc ^ buf[i]) & 0x000000FF;
        crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32tab[k];
    }

    /* postconditioning */
    return ~crc;
}

/* crc32gen: generate the nth (0..255) crc32 table value */
#if 0
static unsigned long crc32gen(int n)
{
    int i;
    unsigned long crc;
    
    crc = n;
    for (i = 8; i >= 0; i--) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xEDB88320;
        }
        else {
            crc >>= 1;
        }
    }
    return crc;
}
#endif

/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
