// PUKE SO HARD
// Go will only build source files in the same folder as the package, and obviously these files are not
// so, include them to avoid linker failure
#include "../../../support/cpp/source/SafeMessageBuffer.cpp"
#include "build/cpp/aligned/AutoUnionTest.cpp"
#include "build/cpp/Bar/Bar.cpp"
#include "build/cpp/Foo/Foo.cpp"
#include "build/cpp/UnionOfUnion/UnionOfUnion.cpp"

#include "cbridge.h"
#include <vector>

extern "C" {

size_t RoundTrip(int type, const uint8_t* inBuf, size_t inLen, uint8_t* outBuf, size_t outLen)
{
  switch (type) {
    case Type_Funky:
    {
      Funky r{inBuf, inLen};
      return r.Pack(outBuf, outLen);
    }
    case Type_Monkey:
    {
      Monkey r{inBuf, inLen};
      return r.Pack(outBuf, outLen);
    }
    case Type_Music:
    {
      Music r{inBuf, inLen};
      return r.Pack(outBuf, outLen);
    }
    case Type_Fire:
    {
      Dragon::Fire r{inBuf, inLen};
      return r.Pack(outBuf, outLen);
    }
    case Type_FunkyMessage:
    {
      FunkyMessage r{inBuf, inLen};
      return r.Pack(outBuf, outLen);
    }
    case Type_UnionOfUnion:
    {
      UnionOfUnion r{inBuf, inLen};
      return r.Pack(outBuf, outLen);
    }
    case Type_MessageOfUnion:
    {
      MessageOfUnion r{inBuf, inLen};
      return r.Pack(outBuf, outLen);
    }
    default:
    {
      break;
    }
  }
  return 0;
}

};