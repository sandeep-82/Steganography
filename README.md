# Steganography

A command-line **Steganography project implemented in C** that hides secret text data inside a 24-bit BMP image using **LSB (Least Significant Bit) technique** and retrieves the hidden data through decoding.

## Features

* Encode a `.txt` file into a `.bmp` image.
* Decode hidden data from a stego `.bmp` image.
* Uses LSB bit manipulation to hide and extract data.
* Validates input file extensions.
* Validates BMP image format.
* Checks image capacity before encoding.
* Stores a magic string to identify encoded images.
* Stores secret file extension and file size inside the image.
* Handles binary files using `fread()` and `fwrite()`.
* Handles file opening, reading, writing, and closing errors.
* Separates encoding and decoding into different modules.

## Encoding Process

```text
Source BMP + Secret TXT
          |
          v
    Check Capacity
          |
          v
    Copy BMP Header
          |
          v
    Encode Magic String
          |
          v
 Encode Secret Extension Size
          |
          v
    Encode Extension
          |
          v
    Encode Secret Size
          |
          v
    Encode Secret Data
          |
          v
    Copy Remaining Data
          |
          v
       Stego BMP
```

## Decoding Process

```text
       Stego BMP
           |
           v
    Read Image Data
           |
           v
    Decode Magic String
           |
           v
 Decode Extension Size
           |
           v
   Decode Extension
           |
           v
   Decode Secret Size
           |
           v
   Decode Secret Data
           |
           v
      Output TXT
```

## LSB Steganography

The project uses the **Least Significant Bit (LSB)** technique to hide secret data inside image bytes.

One secret byte contains 8 bits. Each bit is stored in the LSB of one image byte.

```text
Secret Byte
    |
    | 8 bits
    v
Image Byte 1 → LSB
Image Byte 2 → LSB
Image Byte 3 → LSB
Image Byte 4 → LSB
Image Byte 5 → LSB
Image Byte 6 → LSB
Image Byte 7 → LSB
Image Byte 8 → LSB
```

The encoding is performed using:

```text
encode_byte_to_lsb()
```

and the hidden byte is extracted using:

```text
decode_byte_from_lsb()
```

## Data Stored in Image

The following information is encoded into the BMP image:

```text
Magic String
     ↓
Secret File Extension Size
     ↓
Secret File Extension
     ↓
Secret File Size
     ↓
Secret File Data
```

The decoder reads the same information in the same order.

## Input Validation

The project validates:

* Encoding and decoding command-line arguments.
* `.bmp` image files.
* `.txt` secret files.
* Output `.bmp` file.
* Source and output file name conflicts.
* BMP signature.
* BMP width and height.
* 24-bit BMP format.
* Uncompressed BMP format.
* Image capacity before encoding.

## File Handling

The project works with binary files using:

* `fopen()`
* `fread()`
* `fwrite()`
* `fseek()`
* `ftell()`
* `rewind()`
* `fclose()`

Binary mode is used for BMP image processing.

## Project Structure

```text
Steganography/
│
├── main.c
├── encode.c
├── encode.h
├── decode.c
├── decode.h
├── common.h
├── types.h
├── .gitignore
└── README.md
```

### File Description

| File         | Description                                                          |
| ------------ | -------------------------------------------------------------------- |
| `main.c`     | Handles command-line arguments and controls encode/decode operations |
| `encode.c`   | Contains encoding implementation                                     |
| `encode.h`   | Contains encoding structures and function declarations               |
| `decode.c`   | Contains decoding implementation                                     |
| `decode.h`   | Contains decoding structures and function declarations               |
| `common.h`   | Contains shared constants such as `MAGIC_STRING`                     |
| `types.h`    | Contains user-defined types and status definitions                   |
| `.gitignore` | Prevents generated files and test files from being uploaded          |
| `README.md`  | Project documentation                                                |

## Technologies & Concepts

* C Programming
* File Handling
* Binary File Processing
* Structures
* Pointers
* Arrays
* Strings
* Enumerations
* Command-Line Arguments
* Bit Manipulation
* Bitwise Operators
* LSB Steganography
* BMP File Processing
* Modular Programming

## Compilation

Compile the project using GCC:

```bash
gcc main.c encode.c decode.c -o stego
```

For compilation with warnings:

```bash
gcc -Wall -Wextra -Wpedantic main.c encode.c decode.c -o stego
```

## Usage

### Encode

```bash
./stego -e <source.bmp> <secret.txt> <stego.bmp>
```

Example:

```bash
./stego -e beautiful.bmp secret.txt stego.bmp
```

### Decode

```bash
./stego -d <stego.bmp> [output.txt]
```

Example:

```bash
./stego -d stego.bmp output.txt
```

If the output filename is not provided, the program uses:

```text
output.txt
```

## Verification

The encoded image can be decoded back to the original secret file.

The decoded file can be compared with the original using:

```bash
cmp secret.txt output.txt
```

If `cmp` produces no output, both files are identical.

