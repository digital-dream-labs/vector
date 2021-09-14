#include "build/simple/JavascriptTag.h"
#include "build/simple/Javascript.h"
#include <stdio.h>
#include <fstream>
#include <vector>
#include <cmath>

const static std::string sTestString16 = "Hi, a string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. The End.";
const static uint8_t sTestFArray8[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 };
const static uint8_t sTestVArray8[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
const static float sTestVArrayFloat[] = { 1.23f, 2.34f, 3.45f, 4.56f, 5.67f, 6.78f, 7.89f };
const static std::string sTestStringArray[] = { "First", "Second", "Third", "Fourth", "Fifth", "Sixth" };

template<class T>
bool ArrayMatches(std::vector<T> a, std::vector<T> b) {
  if(a.size() != b.size()) {
    return false;
  }

  for(int i = 0; i < a.size(); i++) {
    if(a[i] != b[i]) {
      return false;
    }
  }

  return true;
}

template<>
bool ArrayMatches<float>(std::vector<float> a, std::vector<float> b) {
  if(a.size() != b.size()) {
    return false;
  }

  for(int i = 0; i < a.size(); i++) {
    if(std::abs(float(a[i] - b[i])) >= 0.001f) {
      return false;
    }
  }

  return true;
}

template<class T>
bool ArrayMatches(std::array<T, 27> a, std::vector<T> b) {
  if(a.size() != b.size()) {
    return false;
  }

  for(int i = 0; i < a.size(); i++) {
    if(a[i] != b[i]) {
      return false;
    }
  }

  return true;
}

int main() {
  std::ifstream f("buffer.tmp");

  f.seekg(0, f.end);
  size_t size = f.tellg();
  f.seekg(0, f.beg);

  char buffer[2048];

  if(size > sizeof(buffer)) {
    size = sizeof(buffer);
  }

  f.read(buffer, size);

  TestClad::Message msg;

  msg.Unpack((uint8_t*)buffer, size);

  std::vector<std::string> testStringVector;
  for(int i = 0; i < sizeof(sTestStringArray) / sizeof(std::string); i++) {
    testStringVector.push_back(sTestStringArray[i]);
  }

  assert(msg.testBool == true);
  assert(msg.testUint8 == 147);
  assert(msg.testUint16 == 35149);
  assert(msg.testUint32 == 1290345);
  assert(msg.testUint64 == 5239140128);
  assert(std::abs(float(msg.testFloat32 - 343.2f)) < 0.001f);
  assert(std::abs(float(msg.testFloat64 - 12999.1234f)) < 0.001f);
  assert(msg.testString == "Hello, world!");
  assert(msg.testString16 == sTestString16);
  assert(ArrayMatches(msg.testFArray8, std::vector<uint8_t>(sTestFArray8, sTestFArray8 + sizeof(sTestFArray8))));
  assert(ArrayMatches(msg.testVArray8, std::vector<uint8_t>(sTestVArray8, sTestVArray8 + sizeof(sTestVArray8))));
  assert(ArrayMatches(msg.testStringArray, testStringVector));
  assert(ArrayMatches(msg.testVArrayFloat32, std::vector<float>(sTestVArrayFloat, sTestVArrayFloat + sizeof(sTestVArrayFloat) / sizeof(float))));
  printf("Test passed: JavaScript emitter interop with CPP emitter.\n");

  return 0;
}