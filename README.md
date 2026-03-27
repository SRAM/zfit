# zfit
Compression for FIT files.
Contains a delta encoder / decoder and heatshrink compression.

heatshrink comes from https://github.com/atomicobject/heatshrink

To build:

  Type "make", which builds both static and dynamically-allocated
  compressor / decompressors.

To use on desktop:

  Start with the dynamic/fit_*zip.c files.

  The fit_zip executable can take two command-line arguments: -w sets
  the window size and -l sets the lookup size.  -w 0 has a special
  meaning: No compression is performed, just delta encoding.

  The fit_unzip executable can take one command-line flag: -d disables
  decompression and performs only delta decoding.

To use embedded:

  Start with static/fit_zip.c. You will need to supply FILE * file
  descriptors to read and write.  Only fread() and fwrite() are called
  on these, so they can be search-and-replaced to a different type of
  file descriptors if necessary. (or maybe macro-ized?)

  See zfit.pdf to choose appropriate parameters for your device.

File descriptions:

| ./README                      | this file                                    |
| ./LICENSE                     | ISC License, like heatshrink itself          |
| ./zfit.pdf                    | algorithm documenatation 		       |
| ./Makefile                    | Makes both static and dynamic variants       |
| ./heatshrink                  | git pull of heatshrink source                |
| ./static                      | statically-allocated variant.  Encoder only. |
| ./dynamic                     | dynamically-allocated, encoder and decoder   |
| ./static/heatshrink_config.h  | heatshrink configuration for static alloc    |
| ./static/fit_zip.c            | source for for static compressor             |
| ./static/Makefile             | builds static compressor                     |
| ./verify.py                   | Python helper for verification / testing     |
| ./dynamic/heatshrink_config.h | heatshrink configuration for dynamic alloc   |
| ./dynamic/fit_zip.c           | source for dynamic compressor                |
| ./dynamic/fit_unzip.c         | source for dynamic decompressor              |
| ./dynamic/Makefile            | builds dynamic compressor / decompressor     |
| ./fit_delta_encode.c          | fit delta encoder                            |
