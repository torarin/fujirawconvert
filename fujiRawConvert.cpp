/*
clang-cl /EHsc /O2 /std:c++17 fujiRawConvert.cpp
or
clang -O2 -std=c++17 fujiRawConvert.cpp -o fujiRawConvert.exe

Author: Torarin Hals Bakke (2023)
*/
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <array>
#include <string>
#include <cmath>

template<class T>
std::unique_ptr<T> mufo(std::size_t n)
{
    return std::unique_ptr<T>(new typename std::remove_extent<T>::type[n]);
}

#pragma pack(push,1)
struct TiffTag {
  unsigned short tagType;
  unsigned short fieldType;
  int numVals;
  int val;
};
#pragma pack(pop)

int NUMCH = 3;

void writeTiff(const char *filename, unsigned short *buf, int width, int height) {
  std::ofstream file(filename, std::ios::binary);
  int head = 0x2A4949;
  int bufSize = width*height*NUMCH*2;
  unsigned short bps[] = {16,16,16,16};
  int offset = 8 + bufSize + sizeof(bps);
  file.write((char*) &head, 4); 
  file.write((char*) &offset, 4);
  file.write((char*) buf, bufSize);

  file.write((char*) &bps, sizeof(bps));

  std::vector<TiffTag> entries = {
    {0x100, 3, 1, width},
    {0x101, 3, 1, height},
    {0x102, 3, NUMCH, 8 + bufSize}, //bps
    {0x103, 3, 1, 1},
    {0x106, 3, 1, 2},
    {0x111, 4, 1, 8},
    {0x115, 3, 1, NUMCH},
    {0x116, 3, 1, height},
    {0x117, 4, 1, bufSize},
    //{0x11c, 3, 1, 1},
    //{0x153, 3, 1, 1}
  };
  if (NUMCH == 4)
    entries.push_back({0x152, 3, 1, 0}); // ExtraSamples
  
  unsigned short numEntries = entries.size();
  file.write((char*) &numEntries, 2); 
  file.write((char*) &entries[0], sizeof(TiffTag)*numEntries);
  int eof = 0;
  file.write((char*) &eof, 4);
}

/*
void fillBlanks(unsigned short *src, int w, int h) {
  const int ix[] = {-1, 1, 0, 0};
  const int iy[] = {0, 0, -1, 1};

  for (int ch = 0; ch < 3; ++ch) {
    for (int y = 0; y < h; ++y) {
      for (int x = 1 - y % 2; x < w; x += 2) {
        unsigned int sum = 0;
        unsigned int N = 0;
        for (int i = 0; i < 4; ++i) {
          int xs = x + ix[i];
          int ys = y + iy[i];
          if (
            xs < 0 || xs >= w ||
            ys < 0 || ys >= h)
            continue;
          unsigned int s = src[(ys*w + xs)*3 + ch];
          sum += s;
          ++N;
        }
        src[(y*w + x)*3 + ch] = int(double(sum)/N + 0.5);
      }
    }
  }
}*/

void fillBlanks2(unsigned short *src, int w, int h) {
  const int xF[] = {0, -1};
  const int yF[] = {0, -1};
  const int xE[] = {0, -1};
  const int yE[] = {-1, 0};

  for (int ch = 0; ch < NUMCH; ++ch) {
    for (int y = h-1; y > 0; --y) {
      for (int x = w-1; x > 0; --x) {
        int xs[2];
        int ys[2];
        if ((x + y) % 2 == 1) {
          xs[0] = x + xF[0];
          ys[0] = y + yF[0];
          xs[1] = x + xF[1];
          ys[1] = y + yF[1];
        } else {
          xs[0] = x + xE[0];
          ys[0] = y + yE[0];
          xs[1] = x + xE[1];
          ys[1] = y + yE[1];
        }
        unsigned int s1 = src[(ys[0]*w + xs[0])*NUMCH + ch];
        unsigned int s2 = src[(ys[1]*w + xs[1])*NUMCH + ch];
        src[(y*w + x)*NUMCH + ch] = (unsigned short)((s1+s2)/2);
      }
    }
  }
}


void cropTopLeft(unsigned short *src, int w, int h) {
  for (int y = 0; y < h-1; ++y) {
    std::copy_n(src + ((y+1)*w + 1)*NUMCH, (w-1)*NUMCH, src + y*(w-1)*NUMCH);
  }
}

int main(int argc, char *argv[]) {

auto usage = "Usage: [-lin] [-ir] <.bin file> <.tag file> <output .tiff file>\n";

if (argc < 4) {
  std::cout << usage;
  return 1;
}

bool linear = false;

int ai = 1;

for (; ai < argc; ++ai)  {
  if (argv[ai][0] != '-')
    break;
  std::string sw = argv[ai];
  if (sw == "-lin") {
    linear = true;
  } else if (sw == "-ir") {
    NUMCH = 4;
  } else {
    std::cout << "Unknown option: " << sw << "\n";
    return 1;
  }
}

if (ai + 3 != argc) {
  std::cout << usage;
  return 1;
}

const char *imgFilename = argv[ai];
const char *tagFilename = argv[ai+1];
const char *outFilename = argv[ai+2];

std::error_code ec{};
const size_t imgFileSize = std::filesystem::file_size(imgFilename, ec);
if (imgFileSize == -1) {
    std::cout << ".bin file not found\n";
    return 1;
}

int width{};
int height{};

{
  std::ifstream file(tagFilename, std::ios::binary);
  if (!file) {
    std::cout << ".tag file not found\n";
    return 1;
  }
  unsigned short w;
  unsigned short h;
  file.seekg(0xA);
  file.read((char*)&w, 2);
  file.seekg(0xE);
  file.read((char*)&h, 2);
  w = (w >> 8) | (w << 8);
  h = (h >> 8) | (h << 8);
  width = w;
  height = h;
}

int widthInts = (width + 2)/3;
int widthBytes = widthInts*4;

int fieldSize = widthBytes*height*4;
int numFields = imgFileSize/fieldSize;
if (imgFileSize%fieldSize ||
  (numFields != 1 && numFields != 2 && numFields != 4 && numFields != 8)) {

  std::cout << ".bin size doesn't match .tag data\n";
  return 1;
}

std::cout << "Number of fields: " << numFields << "\n";

auto imgFile = std::make_unique<unsigned char[]>(imgFileSize);
unsigned int *pixels = (unsigned int *)imgFile.get();

{
  std::ifstream file(imgFilename, std::ios::binary);
  file.read((char*) imgFile.get(), imgFileSize);
}

auto cnvTblR = mufo<unsigned short[]>(1024);
auto cnvTblG = mufo<unsigned short[]>(1024);
auto cnvTblB = mufo<unsigned short[]>(1024);

if (!linear) {
  double gamR = 1.3;
  double gamG = 1.1;
  double gamB = 1;
  for (int i = 0; i < 1024; ++i) {
    cnvTblR[i] = (unsigned short)std::min((int)(gamR*i*0xffff/0x3ff + 0.5), 0xffff);
    cnvTblG[i] = (unsigned short)std::min((int)(gamG*i*0xffff/0x3ff + 0.5), 0xffff);
    cnvTblB[i] = (unsigned short)std::min((int)(gamB*i*0xffff/0x3ff + 0.5), 0xffff);
  }
} else {
  for (int i = 0; i < 1024; ++i) {
    // Assume units of 0.002 density, like Cineon
    cnvTblR[i] = (unsigned short)(int)(0xffff*std::pow(10, -i/500.0) + 0.5);
    cnvTblB[i] = cnvTblG[i] = cnvTblR[i];
  }
}


std::array<int,8> xsh{};
std::array<int,8> ysh{};
int xoff{};
int yoff{};
int xcrop{};
int ycrop{};
int sc = 1;

if (numFields == 1) {
  sc = 1;
  xsh = {0};
  ysh = {0};
  xoff = 0;
  yoff = 0;
  xcrop = 0;
  ycrop = 0;
} else if (numFields == 2) {
  sc = 1;
  xsh = {0, 0};
  ysh = {0, 1};
  xoff = 0;
  yoff = 0;
  xcrop = 0;
  ycrop = 0;
} else if (numFields == 4) {
  sc = 2;
  xsh = {1, 0, 1, 2};
  ysh = {0, 1, 2, 1};
  xoff = -1;
  yoff = -1;
  xcrop = 1;
  ycrop = 1;
} else /*if (numFields == 8)*/ {
  sc = 2;
  xsh = {2, 1, 0, 1, 2, 3, 2, 1};
  ysh = {0, 0, 1, 1, 1, 1, 2, 2};
  xoff = -1;
  yoff = 0;
  xcrop = 1;
  ycrop = 0;
}

int h = sc*height*2 - ycrop;
int w = sc*width - xcrop;

auto outFile = std::make_unique<unsigned short[]>(w*h*NUMCH);
auto of = outFile.get();

for (int i = 0; i < numFields; ++i) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int yP = y*2 + (x + 1)%2;
      yP *= sc;
      int xP = x*sc;
      xP += xsh[i] + xoff;
      yP += ysh[i] + yoff;
      if (
          xP < 0 || xP >= w ||
          yP < 0 || yP >= h)
        continue;
      
      int dw = x/3;
      int ix = x%3;
      int ir =(pixels[(y + (4*i + 0)*height)*widthInts + dw] >> (ix*10)) & 0x3ff;
      int b = (pixels[(y + (4*i + 1)*height)*widthInts + dw] >> (ix*10)) & 0x3ff;
      int g = (pixels[(y + (4*i + 2)*height)*widthInts + dw] >> (ix*10)) & 0x3ff;
      int r = (pixels[(y + (4*i + 3)*height)*widthInts + dw] >> (ix*10)) & 0x3ff;
      of[(w*yP + xP)*NUMCH + 0] = cnvTblR[r];
      of[(w*yP + xP)*NUMCH + 1] = cnvTblG[g];
      of[(w*yP + xP)*NUMCH + 2] = cnvTblB[b];
      if (NUMCH == 4)
        of[(w*yP + xP)*NUMCH + 3] = cnvTblB[ir];
    }
  }
}

if (numFields == 1 || numFields == 4)
  fillBlanks2(outFile.get(), w, h);

cropTopLeft(outFile.get(), w, h);
w -= 1; h -= 1;

writeTiff(outFilename, outFile.get(), w, h);

return 0;
}
