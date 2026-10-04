#ifndef ENCODE_H
#define ENCODE_H

#include <stdio.h>
#include "types.h"

typedef struct _EncodeInfo
{
    char *src_image_fname;
    FILE *fptr_src_image;

    char *secret_fname;
    FILE *fptr_secret;
    long size_secret_file;

    char *stego_image_fname;
    FILE *fptr_stego_image;
} EncodeInfo;

Status read_and_validate_encode_args(char *argv[], EncodeInfo *encInfo);
Status do_encoding(EncodeInfo *encInfo);

#endif
