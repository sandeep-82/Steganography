#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "encode.h"
#include "common.h"

static Status open_input_files(EncodeInfo *encInfo);
static Status copy_remaining_img_data(FILE *fptr_src, FILE *fptr_dest);
static Status check_capacity(EncodeInfo *encInfo);
static long get_file_size(FILE *fptr);
static Status copy_bmp_header(EncodeInfo *encInfo);
static Status encode_magic_string(const char *magic_string, EncodeInfo *encInfo);
static Status encode_secret_file_extn_size(int file_extn_size, EncodeInfo *encInfo);
static Status encode_secret_file_extn(const char *file_extn, EncodeInfo *encInfo);
static Status encode_secret_file_data(EncodeInfo *encInfo);
static Status encode_secret_file_size(long file_size, EncodeInfo *encInfo);
static Status encode_byte_to_lsb(unsigned char data, unsigned char image_buffer[8]);

/* Get image size
 * Input: Image file ptr
 * Output: width * height * bytes per pixel (3 in our case)
 * Description: In BMP Image, width is stored in offset 18,
 * and height after that. size is 4 bytes
 */
static uint get_image_size_for_bmp(FILE *fptr_image)
{
    unsigned char header[54];
    int32_t width;
    int32_t height;
    uint32_t pixel_offset;
    uint32_t dib_header_size;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;

    rewind(fptr_image);
    if (fread(header, 1, sizeof(header), fptr_image) != sizeof(header) ||
        header[0] != 'B' || header[1] != 'M')
        return 0;

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
        return 0;

    uint64_t absolute_height = height < 0 ? -(int64_t)height : height;
    uint64_t capacity = (uint64_t)width * absolute_height * 3;
    if (capacity > UINT_MAX)
        return 0;

    printf("Image dimensions: %u x %u pixels\n",
           (unsigned int)width, (unsigned int)absolute_height);
    return (uint)capacity;
}

/* 
 * Get File pointers for i/p and o/p files
 * Inputs: Src Image file, Secret file and
 * Stego Image file
 * Output: FILE pointer for above files
 * Return Value: e_success or e_failure, on file errors
 */
static Status open_input_files(EncodeInfo *encInfo)
{
    encInfo->fptr_src_image = NULL;
    encInfo->fptr_secret = NULL;
    encInfo->fptr_stego_image = NULL;

    encInfo->fptr_src_image = fopen(encInfo->src_image_fname, "rb");
    if (encInfo->fptr_src_image == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->src_image_fname);
        return e_failure;
    }

    encInfo->fptr_secret = fopen(encInfo->secret_fname, "rb");
    if (encInfo->fptr_secret == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->secret_fname);
        fclose(encInfo->fptr_src_image);
        encInfo->fptr_src_image = NULL;
        return e_failure;
    }

    return e_success;
}

/* Copy remaining image bytes from source to stego after encoding */
static Status copy_remaining_img_data(FILE *fptr_src, FILE *fptr_dest)
{
    char buffer[1024];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fptr_src)) > 0)
    {
        if (fwrite(buffer, 1, bytes_read, fptr_dest) != bytes_read)
        {
            return e_failure;
        }
    }

    return ferror(fptr_src) ? e_failure : e_success;
}

/* Read and validate Encode args from argv */
Status read_and_validate_encode_args(char *argv[], EncodeInfo *encInfo)
{
    if (argv[2] == NULL || argv[3] == NULL || argv[4] == NULL)
        return e_failure;

    size_t src_length = strlen(argv[2]);
    size_t secret_length = strlen(argv[3]);
    size_t stego_length = strlen(argv[4]);
    if (src_length < 4 || strcmp(argv[2] + src_length - 4, ".bmp") != 0 ||
        secret_length < 4 || strcmp(argv[3] + secret_length - 4, ".txt") != 0 ||
        stego_length < 4 || strcmp(argv[4] + stego_length - 4, ".bmp") != 0)
        return e_failure;

    if (strcmp(argv[2], argv[4]) == 0 || strcmp(argv[3], argv[4]) == 0)
        return e_failure;

    encInfo->src_image_fname = argv[2];
    encInfo->secret_fname = argv[3];
    encInfo->stego_image_fname = argv[4];

    return e_success;
}

/* Check that the source image can hold the encoded secret */
static Status check_capacity(EncodeInfo *encInfo)
{
    uint image_capacity = get_image_size_for_bmp(encInfo->fptr_src_image);
    encInfo->size_secret_file = get_file_size(encInfo->fptr_secret);
    long source_size = get_file_size(encInfo->fptr_src_image);
    if (image_capacity == 0 || encInfo->size_secret_file < 0 ||
        source_size < 0 ||
        (uint64_t)source_size < UINT64_C(54) + image_capacity ||
        (uint64_t)encInfo->size_secret_file > UINT32_MAX)
        return e_failure;

    uint64_t extension_size = strlen(".txt");
    uint64_t encoded_bytes = strlen(MAGIC_STRING) + 1 + extension_size + 4 +
                             (uint64_t)encInfo->size_secret_file;
    uint64_t required_capacity = encoded_bytes * 8;
    if (required_capacity <= image_capacity)
        return e_success;

    return e_failure;
}

/* Get file size */
static long get_file_size(FILE *fptr)
{
    if (fseek(fptr, 0L, SEEK_END) != 0)
        return -1;

    long size = ftell(fptr);
    rewind(fptr);
    return size;
}

static Status close_encode_files(EncodeInfo *encInfo)
{
    int close_failed = 0;
    if (encInfo->fptr_src_image != NULL)
        close_failed |= fclose(encInfo->fptr_src_image) != 0;
    if (encInfo->fptr_secret != NULL)
        close_failed |= fclose(encInfo->fptr_secret) != 0;
    if (encInfo->fptr_stego_image != NULL)
        close_failed |= fclose(encInfo->fptr_stego_image) != 0;

    encInfo->fptr_src_image = NULL;
    encInfo->fptr_secret = NULL;
    encInfo->fptr_stego_image = NULL;
    return close_failed ? e_failure : e_success;
}

static Status open_stego_output(EncodeInfo *encInfo)
{
    encInfo->fptr_stego_image = fopen(encInfo->stego_image_fname, "wb");
    if (encInfo->fptr_stego_image == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->stego_image_fname);
        return e_failure;
    }

    return e_success;
}

Status do_encoding(EncodeInfo *encInfo)
{
    if (open_input_files(encInfo) != e_success)
        return e_failure;

    Status status = e_failure;
    if (check_capacity(encInfo) != e_success)
    {
        fprintf(stderr, "ERROR: Source image cannot hold the secret file.\n");
        goto cleanup;
    }

    if (open_stego_output(encInfo) != e_success)
        goto cleanup;

    if (copy_bmp_header(encInfo) != e_success ||
        encode_magic_string(MAGIC_STRING, encInfo) != e_success ||
        encode_secret_file_extn_size((int)strlen(".txt"), encInfo) != e_success ||
        encode_secret_file_extn(".txt", encInfo) != e_success ||
        encode_secret_file_size(encInfo->size_secret_file, encInfo) != e_success ||
        encode_secret_file_data(encInfo) != e_success ||
        copy_remaining_img_data(encInfo->fptr_src_image,
                                encInfo->fptr_stego_image) != e_success)
        goto cleanup;

    status = e_success;

cleanup:
    if (close_encode_files(encInfo) != e_success)
        status = e_failure;
    return status;
}

/* Copy BMP header without using the image data offset */
static Status copy_bmp_header(EncodeInfo *encInfo)
{
    unsigned char header[54];
    size_t bytes_read;

    rewind(encInfo->fptr_src_image);
    rewind(encInfo->fptr_stego_image);

    bytes_read = fread(header, 1, sizeof(header), encInfo->fptr_src_image);
    if (bytes_read != sizeof(header))
        return e_failure;

    if (fwrite(header, 1, bytes_read, encInfo->fptr_stego_image) != bytes_read)
        return e_failure;

    return e_success;
}

/* Encode magic string */
static Status encode_magic_string(const char *magic_string, EncodeInfo *encInfo)
{
    unsigned char buffer[8];

    for (size_t ch = 0; ch < strlen(magic_string); ch++)
    {
        if (fread(buffer, 1, 8, encInfo->fptr_src_image) != 8)
            return e_failure;

        if (encode_byte_to_lsb((unsigned char)magic_string[ch], buffer) != e_success)
            return e_failure;

        if (fwrite(buffer, 1, 8, encInfo->fptr_stego_image) != 8)
            return e_failure;
    }

    return e_success;
}

/* Encode secret file extension size */
static Status encode_secret_file_extn_size(int file_extn_size, EncodeInfo *encInfo)
{
    unsigned char buffer[8];

    if (fread(buffer, 1, 8, encInfo->fptr_src_image) != 8)
        return e_failure;

    if (encode_byte_to_lsb((unsigned char)(file_extn_size & 0xFF), buffer) != e_success)
        return e_failure;

    if (fwrite(buffer, 1, 8, encInfo->fptr_stego_image) != 8)
        return e_failure;

    return e_success;
}

/* Encode secret file extension */
static Status encode_secret_file_extn(const char *file_extn, EncodeInfo *encInfo)
{
    unsigned char buffer[8];

    for (size_t i = 0; i < strlen(file_extn); i++)
    {
        if (fread(buffer, 1, 8, encInfo->fptr_src_image) != 8)
            return e_failure;

        if (encode_byte_to_lsb((unsigned char)file_extn[i], buffer) != e_success)
            return e_failure;

        if (fwrite(buffer, 1, 8, encInfo->fptr_stego_image) != 8)
            return e_failure;
    }
    return e_success;
}

/* Encode secret file data */
static Status encode_secret_file_data(EncodeInfo *encInfo)
{
    unsigned char buffer[8];
    unsigned char data;

    while (fread(&data, 1, 1, encInfo->fptr_secret) == 1)
    {
        if (fread(buffer, 1, 8, encInfo->fptr_src_image) != 8)
            return e_failure;

        if (encode_byte_to_lsb(data, buffer) != e_success)
            return e_failure;

        if (fwrite(buffer, 1, 8, encInfo->fptr_stego_image) != 8)
            return e_failure;
    }

    return ferror(encInfo->fptr_secret) ? e_failure : e_success;
}

/* Encode secret file size */
static Status encode_secret_file_size(long file_size, EncodeInfo *encInfo)
{
    unsigned char buffer[8];
    unsigned char size_bytes[4];

    size_bytes[0] = (unsigned char)(file_size & 0xFF);
    size_bytes[1] = (unsigned char)((file_size >> 8) & 0xFF);
    size_bytes[2] = (unsigned char)((file_size >> 16) & 0xFF);
    size_bytes[3] = (unsigned char)((file_size >> 24) & 0xFF);

    for (int i = 0; i < 4; i++)
    {
        if (fread(buffer, 1, 8, encInfo->fptr_src_image) != 8)
            return e_failure;

        if (encode_byte_to_lsb(size_bytes[i], buffer) != e_success)
            return e_failure;

        if (fwrite(buffer, 1, 8, encInfo->fptr_stego_image) != 8)
            return e_failure;
    }

    return e_success;
}

/* Encode a byte into LSB of image data array */
static Status encode_byte_to_lsb(unsigned char data, unsigned char image_buffer[8])
{
    for (int i = 0; i < 8; i++)
        image_buffer[i] = (image_buffer[i] & 0xFE) | ((data >> i) & 0x01);

    return e_success;
}
