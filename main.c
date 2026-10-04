#include "encode.h"
#include "decode.h"
#include <stdio.h>
#include <string.h>
#include "types.h"

static OperationType check_operation_type(char *argv[]);

static void print_usage(const char *program_name)
{
    printf("Usage:\n");
    printf("  %s -e <source.bmp> <secret.txt> <stego.bmp>\n", program_name);
    printf("  %s -d <stego.bmp> [output.txt]\n", program_name);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    OperationType st = check_operation_type(argv);
    if ((st == e_encode && argc != 5) ||
        (st == e_decode && (argc < 3 || argc > 4)))
    {
        print_usage(argv[0]);
        return 1;
    }

    if (st == e_decode)
    {
        DecodeInfo decInfo;
        memset(&decInfo, 0, sizeof(decInfo));

        if (read_and_validate_decode_args(argv, &decInfo) != e_success)
        {
            fprintf(stderr, "Error: Please provide a valid BMP image and TXT output file.\n");
            return 1;
        }

        printf("\n--- Steganography: Decode ---\n");
        printf("Input image : %s\n", decInfo.stego_image_fname);
        printf("Output file : %s\n", decInfo.output_fname);

        if (do_decoding(&decInfo) != e_success)
        {
            fprintf(stderr, "Error: Decoding failed. Check the input image.\n");
            return 1;
        }

        printf("\nDecoding completed successfully.\n");
        printf("Secret data saved to: %s\n", decInfo.output_fname);
        return 0;
    }
    else if (st != e_encode)
    {
        fprintf(stderr, "Error: Unsupported operation '%s'.\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    EncodeInfo encInfo;
    memset(&encInfo, 0, sizeof(encInfo));

    if (read_and_validate_encode_args(argv, &encInfo) != e_success)
    {
        fprintf(stderr, "Error: Please provide valid BMP and TXT filenames.\n");
        return 1;
    }

    printf("\n--- Steganography: Encode ---\n");
    printf("Source image: %s\n", encInfo.src_image_fname);
    printf("Secret file : %s\n", encInfo.secret_fname);
    printf("Output image: %s\n", encInfo.stego_image_fname);

    if (do_encoding(&encInfo) != e_success)
    {
        fprintf(stderr, "Error: Encoding failed.\n");
        return 1;
    }

    printf("\nEncoding completed successfully.\n");
    printf("Stego image saved to: %s\n", encInfo.stego_image_fname);

    return 0;
}

static OperationType check_operation_type(char *argv[])
{
    if (strcmp(argv[1], "-e") == 0)
        return e_encode;
    if (strcmp(argv[1], "-d") == 0)
        return e_decode;
    return e_unsupported;
}
