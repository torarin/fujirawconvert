# fujirawconvert
Convert raw CCD data from Fuji Frontier SP3000 to TIFF images. The TIFF files are 16 bit RGBA with A storing the infrared channel.

Compile with for instance:

`clang-cl /EHsc /O2 /std:c++17 fujiRawConvert.cpp`

Usage:

`fujiRawConvert.exe [.bin file] [.tag file] [log/lin] [output file (.tiff)]`

`log` gives an inverted logarithmic encoding, `lin` a regular linear file.
