# fujirawconvert
Convert raw CCD data from Fuji Frontier SP3000 to TIFF images.

You can find precompiled binaries in the Releases section or compile with:

`clang-cl /EHsc /O2 /std:c++17 fujiRawConvert.cpp`

or

`clang -O2 -std=c++17 fujiRawConvert.cpp -o fujiRawConvert.exe`

Usage:

`fujiRawConvert.exe [-lin] [-ir] <.bin file> <.tag file> <output .tiff file>`

By default saves a logarithmic inverted image. The `-lin` option instead gives an uninverted linear image.

The `-ir` option includes the infrared channel in the tiff alpha channel.