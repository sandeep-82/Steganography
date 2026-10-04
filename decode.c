#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "decode.h"
#include "common.h"

#define MAX_FILE_EXTENSION_SIZE 20

static Status validate_stego_bmp(FILE *image);
static Status decode_magic_string(const char *magic_string, DecodeInfo *decInfo);
static Status decode_secret_file_extn_size(DecodeInfo *decInfo, int *extn_size);
static Status decode_secret_file_size(DecodeInfo *decInfo);
static Status decode_secret_file_data(DecodeInfo *decInfo);
static Status decode_byte_from_lsb(unsigned char *data, const unsigned char image_buffer[8]);

static Status validate_stego_bmp(FILE *image)
{
    unsigned char header[54];
    uint32_t pixel_offset;
    uint32_t dib_header_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;

    if (fread(header, 1, sizeof(header), image) != sizeof(header) ||
        header[0] != 'B' || header[1] != 'M')
        return e_failure;

    memcpy(&pixel_offset, header + 10, sizeof(pixel_offset));
    memcpy(&dib_header_size, header + 14, sizeof(dib_header_size));
    memcpy(&width, header + 18, sizeof(width));
    memcpy(&height, header + 22, sizeof(height));
    memcpy(&planes, header + 26, sizeof(planes));
    memcpy(&bits_per_pixel, header + 28, sizeof(bits_per_pixel));
    memcpy(&compression, header + 30, sizeof(compression));

    if (pixel_offset != sizeof(header) || dib_header_size < 40 ||
        width <= 0 || height == 0 || height == INT32_MIN ||
        planes != 1 || bits_per_pixel != 24 || compression != 0)
        return e_failure;

    return e_success;
}

Status read_and_validate_decode_args(char *argv[], DecodeInfo *decInfo)
{
    if (argv[2] == NULL)
        return e_failure;

    size_t image_length = strlen(argv[2]);
    if (image_length < 4 || strcmp(argv[2] + image_length - 4, ".bmp") != 0)
        return e_failure;

    if (argv[3] != NULL && strcmp(argv[2], argv[3]) == 0)
        return e_failure;

    decInfo->stego_image_fname = argv[2];

    if (argv[3] != NULL)
    {
        size_t output_length = strlen(argv[3]);
        if (output_length < 4 ||
            strcmp(argv[3] + output_length - 4, ".txt") != 0)
            return e_failure;
        decInfo->output_fname = argv[3];
    }
    else
    {
        decInfo->output_fname = "output.txt";
    }

    return e_success;
}
    

Status do_decoding(DecodeInfo *decInfo)
{
    decInfo->fptr_stego_image = NULL;
    decInfo->fptr_output = NULL;
    decInfo->fptr_stego_image = fopen(decInfo->stego_image_fname, "rb");
    if (decInfo->fptr_stego_image == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", decInfo->stego_image_fname);
        return e_failure;
    }

    Status status = e_failure;
    if (validate_stego_bmp(decInfo->fptr_stego_image) != e_success)
        goto cleanup;

    if (fseek(decInfo->fptr_stego_image, 54, SEEK_SET) != 0)
        goto cleanup;

    if (decode_magic_string(MAGIC_STRING, decInfo) == e_failure)
        goto cleanup;

    int extn_size;
    if (decode_secret_file_extn_size(decInfo, &extn_size) == e_failure)
        goto cleanup;

    if (fseek(decInfo->fptr_stego_image, (long)extn_size * 8, SEEK_CUR) != 0)
        goto cleanup;

    if (decode_secret_file_size(decInfo) == e_failure)
        goto cleanup;

    decInfo->fptr_output = fopen(decInfo->output_fname, "wb");
    if (decInfo->fptr_output == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", decInfo->output_fname);
        goto cleanup;
    }

    if (decode_secret_file_data(decInfo) == e_failure)
        goto cleanup;

    status = e_success;

cleanup:
    if (fclose(decInfo->fptr_stego_image) != 0)
        status = e_failure;
    if (decInfo->fptr_output != NULL && fclose(decInfo->fptr_output) != 0)
        status = e_failure;
    decInfo->fptr_stego_image = NULL;
    decInfo->fptr_output = NULL;
    return status;
}

static Status decode_magic_string(const char *magic_string, DecodeInfo *decInfo)
{
    unsigned char buffer[8];
    for (size_t ch = 0; ch < strlen(magic_string); ch++)
    {
        if (fread(buffer, 1, 8, decInfo->fptr_stego_image) != 8)
            return e_failure;

        unsigned char decoded_byte;
        if (decode_byte_from_lsb(&decoded_byte, buffer) != e_success)
            return e_failure;

        if (decoded_byte != (unsigned char)magic_string[ch])
            return e_failure;
    }
    return e_success;
}

static Status decode_secret_file_extn_size(DecodeInfo *decInfo, int *extn_size)
{
    unsigned char buffer[8];
    if (fread(buffer, 1, 8, decInfo->fptr_stego_image) != 8)
        return e_failure;

    unsigned char extn_size_byte = 0;
    if (decode_byte_from_lsb(&extn_size_byte, buffer) != e_success)
        return e_failure;

    if (extn_size_byte >= MAX_FILE_EXTENSION_SIZE)
        return e_failure;

    *extn_size = extn_size_byte;
    return e_success;
}

static Status decode_secret_file_size(DecodeInfo *decInfo)
{
    unsigned char buffer[8];
    uint32_t decoded_size = 0;

    for (int i = 0; i < 4; i++)
    {
        if (fread(buffer, 1, 8, decInfo->fptr_stego_image) != 8)
            return e_failure;

        unsigned char size_byte = 0;
        if (decode_byte_from_lsb(&size_byte, buffer) != e_success)
            return e_failure;
        decoded_size |= (uint32_t)size_byte << (i * 8);
    }

#if LONG_MAX < UINT32_MAX
    if (decoded_size > (uint32_t)LONG_MAX)
        return e_failure;
#endif

    long data_position = ftell(decInfo->fptr_stego_image);
    if (data_position < 0 || fseek(decInfo->fptr_stego_image, 0, SEEK_END) != 0)
        return e_failure;

    long image_end = ftell(decInfo->fptr_stego_image);
    if (image_end < data_position ||
        (uint64_t)decoded_size > (uint64_t)(image_end - data_position) / 8)
        return e_failure;

    if (fseek(decInfo->fptr_stego_image, data_position, SEEK_SET) != 0)
        return e_failure;

    decInfo->size_secret_file = (long)decoded_size;
    return e_success;
}


static Status decode_secret_file_data(DecodeInfo *decInfo)
{
    unsigned char buffer[8];
    for (long i = 0; i < decInfo->size_secret_file; i++)
    {
        if (fread(buffer, 1, 8, decInfo->fptr_stego_image) != 8)
            return e_failure;

        unsigned char secret_byte;
        if (decode_byte_from_lsb(&secret_byte, buffer) != e_success)
            return e_failure;

        if (fwrite(&secret_byte, 1, 1, decInfo->fptr_output) != 1)
            return e_failure;
    }
    return e_success; 
}

static Status decode_byte_from_lsb(unsigned char *data, const unsigned char image_buffer[8])
{
    unsigned char decoded_byte = 0;
    for (int i = 0; i < 8; i++)
    {
        decoded_byte |= (unsigned char)((image_buffer[i] & 0x01) << i);
    }
    *data = decoded_byte;
    return e_success;
}
