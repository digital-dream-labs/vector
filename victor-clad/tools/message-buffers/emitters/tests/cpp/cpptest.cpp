/*
 * Copyright 2015-2016 Anki Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include "DefaultValues.h"
#include "ExplicitUnion.h"
#include "JsonSerialization.h"
#include "SimpleTest.h"
#include "TestEnum.h"
#include "UnionOfUnion.h"
#include "DupesAllowedUnion.h"
#include "DupesAutoUnion.h"
#include "aligned/AutoUnionTest.h"
#include "json/json.h"


// AMAZING UNIT TEST FRAMEWORK RIGHT HERE
// JUST ONE HEADER (pauley)
#include <greatest.h>

#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <type_traits>

#pragma GCC diagnostic ignored "-Wmissing-braces"

TEST AnkiEnum_Basics()
{
  using ae = AnkiTypes::AnkiEnum;

  // These are based on the clad file, change that and these will break.
  ASSERT_EQ((int)ae::e1, 15);
  ASSERT_EQ((int)ae::e2, 16);
  ASSERT_EQ((int)ae::e3, 17);
  ASSERT_EQ((int)ae::d1, 5);
  ASSERT_EQ((int)ae::d2, 6);
  ASSERT_EQ((int)ae::d3, 7);

  ASSERT_EQ("e1", AnkiTypes::AnkiEnumToString(ae::e1));
  ASSERT_EQ("myReallySilly_EnumVal", AnkiTypes::AnkiEnumToString(ae::myReallySilly_EnumVal));
  ASSERT_EQ(nullptr, AnkiTypes::AnkiEnumToString((ae) (-1)));

  ASSERT_EQ((int)AnkiTypes::AnkiEnumNumEntries, 7);

  PASS();
}

TEST AnkiEnum_NoClass()
{
  using AnkiEnum   = AnkiTypes::AnkiEnum;
  using AnkiNoClassEnum = AnkiTypes::AnkiNoClassEnum;

  // both are enums
  ASSERT_EQ(std::is_enum<AnkiEnum>::value, true);
  ASSERT_EQ(std::is_enum<AnkiNoClassEnum>::value, true);

  // enum class can't be automatically converted to their underlying_type
  bool isClassAnkiEnum = std::is_convertible<AnkiEnum, std::underlying_type<AnkiEnum>::type>::value;
  bool isClassAnkiNoClassEnum = std::is_convertible<AnkiNoClassEnum, std::underlying_type<AnkiNoClassEnum>::type>::value;
  ASSERT_EQ(isClassAnkiEnum, false);
  ASSERT_EQ(isClassAnkiNoClassEnum, true);

  ASSERT_EQ((int)AnkiTypes::AnkiEnumNumEntries, 7);
  ASSERT_EQ((int)AnkiTypes::AnkiNoClassEnumNumEntries, 7);

  PASS();
}

TEST Foo_should_round_trip()
{
  Foo myFoo;
  myFoo.isFoo = true;
  myFoo.myByte = 0x0f;
  myFoo.byteTwo = 0x0e;
  myFoo.myShort = 0x0c0a;
  myFoo.myFloat = -1823913982.0f;
  myFoo.myNormal = 0x0eadbeef;
  myFoo.myFoo = AnkiTypes::AnkiEnum::d2;
  myFoo.myString = "Blah Blah Blah";
  size_t length = myFoo.Size();
  // If this breaks:
  // we've either changed the clad file or we've broken binary compatiblity!
  ASSERT_EQ(29, length);

  uint8_t* buff = new uint8_t[length];
  myFoo.Pack(&buff[0], length);
  Foo otherFoo;
  otherFoo.Unpack(&buff[0], length);
  delete[] buff;
  
  ASSERT_EQ(myFoo, otherFoo);
  PASS();
}

TEST Bar_should_round_trip()
{
  Bar myBar {
        { true, false, false, true, true },
        { 0, 1, 2, 3, 4},
        { 5, 6, 7 },
        { AnkiTypes::AnkiEnum::d1, AnkiTypes::AnkiEnum::e1 },
        { 1, 1, 1 },
        "Foo Bar Baz",
        { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20},
        { true, true, true, true, true, false, false, false, false, false },
        { AnkiTypes::AnkiEnum::e1, AnkiTypes::AnkiEnum::e3 }
  };
  // If this breaks:
  // we've either changed the clad file or we've broken binary compatiblity!
  ASSERT_EQ(114, myBar.Size());

  size_t length = myBar.Size();
  uint8_t* buff = new uint8_t[length];
  Bar otherBar;
  myBar.Pack(buff, length);
  otherBar.Unpack(buff, length);
  delete[] buff;

  ASSERT_EQ(myBar, otherBar);
  PASS();
}

TEST Dog_should_round_trip()
{
  Baz::Dog myDog { AnkiTypes::AnkiEnum::e2, 5 };

  // If this breaks:
  // we've either changed the clad file or we've broken binary compatiblity!
  ASSERT_EQ(2, myDog.Size());

  size_t length = myDog.Size();
  uint8_t* buff = new uint8_t[length];
  Baz::Dog otherDog;
  myDog.Pack(buff, length);
  otherDog.Unpack(buff, length);
  delete[] buff;

  ASSERT_EQ(myDog, otherDog);
  PASS();
}

TEST Poodle_should_init()
{
  Baz::Poodle myPoodle;
  ASSERT_EQ(AnkiTypes::AnkiEnum::d1, myPoodle.a);
  PASS();
}

TEST SoManyStrings_should_round_trip()
{
  SoManyStrings mySoManyStrings {
    { "one", "two", "three", "four"},
    { "uno", "dos", "tres"},
    { "" },
    { "yi", ""}
  };

  // If this breaks:
  // we've either changed the clad file or we've broken binary compatiblity!
  ASSERT_EQ(41, mySoManyStrings.Size());

  size_t length = mySoManyStrings.Size();
  uint8_t* buff = new uint8_t[length];
  SoManyStrings otherSoManyStrings;
  mySoManyStrings.Pack(buff, length);
  otherSoManyStrings.Unpack(buff, length);
  delete[] buff;

  ASSERT_EQ(mySoManyStrings, otherSoManyStrings);
  PASS();
}

TEST od432_should_round_trip()
{
  // Message in a message
  Foo aFoo {false, 1, 2, 3, 1.0, 5555, AnkiTypes::AnkiEnum::e3, "hello"};
  od432 myOD432 { aFoo, 5, LEDColor::Color1 };

  ASSERT_EQ(25, myOD432.Size());

  size_t length = myOD432.Size();
  uint8_t* buff = new uint8_t[length];
  od432 otherOD432;
  myOD432.Pack(buff, length);
  otherOD432.Unpack(buff, length);
  delete[] buff;

  ASSERT_EQ(myOD432, otherOD432);
  PASS();
}

TEST od433_should_round_trip()
{

  Foo aFoo {false, 1, 2, 3, 1.0, 5555, AnkiTypes::AnkiEnum::e3, "hello"};
  Foo bFoo {true, 3, 2, 1, 5.0, 999, AnkiTypes::AnkiEnum::e1, "world"};
  Foo cFoo {false, 4, 5, 6, 2.0, 4555, AnkiTypes::AnkiEnum::d1, "bye"};
  Foo dFoo {true, 7, 8, 9, 7.0, 989, AnkiTypes::AnkiEnum::d2, "bye"};
  od433 myOD433 { {aFoo, bFoo}, {cFoo, dFoo}, 5};

  ASSERT_EQ(79, myOD433.Size());

  size_t length = myOD433.Size();
  uint8_t* buff = new uint8_t[length];
  od433 otherOD433;
  myOD433.Pack(buff, length);
  otherOD433.Unpack(buff, length);
  delete[] buff;

  ASSERT_EQ(myOD433, otherOD433);
  PASS();
}

TEST union_should_round_trip_after_reuse()
{
  Cat::MyMessage message;
  //std::hash<Cat::MyMessageTag> type_hash_fn;
  Foo myFoo;
  myFoo.isFoo = true;
  myFoo.myByte = 0x0f;
  myFoo.byteTwo = 0x0e;
  myFoo.myShort = 0x0c0a;
  myFoo.myNormal = 0x0eadbeef;
  myFoo.myFloat = -18.02e-33f;
  myFoo.myFoo = AnkiTypes::AnkiEnum::d2;
  myFoo.myString = "Whatever";
  message.Set_myFoo(myFoo);

  ASSERT_EQ(24, message.Size());
  ASSERT_EQ(Cat::MyMessage::Tag::myFoo, message.GetTag());
  // Assert this.
  // std::cout << "hash of msg.Tag = " << type_hash_fn(message.GetTag()) << std::endl;

  CLAD::SafeMessageBuffer  buff(message.Size());
  message.Pack(buff);
  Cat::MyMessage otherMessage(buff);

  ASSERT_EQ(Cat::MyMessageTag::myFoo, otherMessage.GetTag());
  ASSERT_EQ(message.Get_myFoo(), otherMessage.Get_myFoo());

  Bar myBar;
  myBar.boolBuff = std::vector<bool> { true, false, false, true, true, false };
  myBar.byteBuff  = std::vector<int8_t> { 0x0f, 0x0e, 0x0c, 0x0a };
  myBar.shortBuff = std::vector<int16_t> { 0x0fed, 0x0caf, 0x0a2f, 0x0a12 };
  myBar.enumBuff  = std::vector<AnkiTypes::AnkiEnum> { AnkiTypes::AnkiEnum::myReallySilly_EnumVal, AnkiTypes::AnkiEnum::e2 };
  myBar.doubleBuff = std::vector<double> { 3128312.031312e132, 123131e-12, 123 };
  myBar.myLongerString = "SomeLongerStupidString";
  myBar.fixedBuff = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
  myBar.fixedBoolBuff = { true, true, true, true, true, false, false, false, false, false };
  myBar.fixedEnumBuff = { AnkiTypes::AnkiEnum::e2, AnkiTypes::AnkiEnum::e3 };
  message.Set_myBar(myBar);

  ASSERT_EQ(128, message.Size());

  // Assert This
  // std::cout << "hash of msg.Tag = " << type_hash_fn(message.GetTag()) << std::endl;
  buff.~SafeMessageBuffer();
  new (&buff) CLAD::SafeMessageBuffer(message.Size());
  message.Pack(buff);
  otherMessage.Unpack(buff);
  buff.~SafeMessageBuffer();

  ASSERT_EQ(Cat::MyMessageTag::myBar, otherMessage.GetTag());
  ASSERT_EQ(message.Get_myBar(), otherMessage.Get_myBar());

  Baz::Dog myDog;
  myDog.a = AnkiTypes::AnkiEnum::e2;
  myDog.b = 55;

  message.Set_myDog(myDog);

  ASSERT_EQ(3, message.Size());
  // Assert this
  // std::cout << "hash of msg.Tag = " << type_hash_fn(message.GetTag()) << std::endl;

  buff.~SafeMessageBuffer();
  new (&buff) CLAD::SafeMessageBuffer(message.Size());
  message.Pack(buff);
  otherMessage.Unpack(buff);
  buff.~SafeMessageBuffer();

  ASSERT_EQ(Cat::MyMessageTag::myDog, otherMessage.GetTag());
  ASSERT_EQ(message.Get_myDog(), otherMessage.Get_myDog());

  od432 myOD432;
  Foo aFoo {false, 1, 2, 3, 1.0, 5555, AnkiTypes::AnkiEnum::e3, "hello"};
  myOD432.aFoo = aFoo;
  myOD432.otherByte = 5;
  myOD432.color = LEDColor::CurrentColor;

  message.Set_myOD432(myOD432);
  ASSERT_EQ(26, message.Size());

  // Assert this
  // std::cout << "hash of msg.Tag = " << type_hash_fn(message.GetTag()) << std::endl;
  buff.~SafeMessageBuffer();
  new (&buff) CLAD::SafeMessageBuffer(message.Size());
  message.Pack(buff);
  otherMessage.Unpack(buff);
  buff.~SafeMessageBuffer();

  ASSERT_EQ(Cat::MyMessageTag::myOD432, otherMessage.GetTag());
  ASSERT_EQ(message.Get_myOD432(), otherMessage.Get_myOD432());

  PASS();
}

TEST Autounion_should_exist() {
  FunkyMessage msg;
  Funky funky {AnkiTypes::AnkiEnum::e1, 3};
  Monkey aMonkey {123182931, funky};


  // I'm sure autounion will suck all sorts of stuff up
  // and the test will likely break any time you touch the clad file.
  // (pauley)
  msg.Set_Monkey(aMonkey);
  ASSERT_EQ(11, msg.Size());
  ASSERT_EQ(FunkyMessage::Tag::Monkey, msg.GetTag());

  Music music {{123}, funky};
  msg.Set_Music(music);

  PASS();
}

TEST CopyConstructors_should_round_trip() {
  Foo aFoo {false, 1, 2, 3, 1.0, 5555, AnkiTypes::AnkiEnum::e3, "hello"};
  Foo bFoo (aFoo);

  ASSERT_EQ(AnkiTypes::AnkiEnum::e3, aFoo.myFoo);
  ASSERT_EQ(AnkiTypes::AnkiEnum::e3, bFoo.myFoo);

  ASSERT_EQ(aFoo, bFoo);

#if defined(HELPER_CONSTRUCTORS) && HELPER_CONSTRUCTORS != 0
  // Looks like we default here to the first member of type Foo
  Cat::MyMessage aWrapper(std::move(aFoo));
#else
  Cat::MyMessage aWrapper;
  aWrapper.Set_myFoo(aFoo);
#endif
  // use copy contructor
  Cat::MyMessage bWrapper(aWrapper);
  const Foo& bWrappersFoo = bWrapper.Get_myFoo();

  // This is actually not defined.
  // aFoo has been moved into aWrapper
  ASSERT_EQ("hello", bWrappersFoo.myString);
  ASSERT_EQ(AnkiTypes::AnkiEnum::e3, bWrapper.Get_myFoo().myFoo);

  // use move constructor
  Cat::MyMessage cWrapper(std::move(aWrapper));

  ASSERT_EQ("hello", cWrapper.Get_myFoo().myString);
  ASSERT_EQ(AnkiTypes::AnkiEnum::e3, cWrapper.Get_myFoo().myFoo);
  ASSERT_EQ(aWrapper.GetTag(), Cat::MyMessageTag::INVALID);
  ASSERT_EQ(cWrapper.GetTag(), Cat::MyMessageTag::myFoo);
  
  // There was a bug that would cause cWrapper's tag to get cleared
  cWrapper = cWrapper;
  ASSERT_EQ(cWrapper.GetTag(), Cat::MyMessageTag::myFoo);

  PASS();
}


TEST AssignmentOperators() {
  Foo aFoo {false, 1, 2, 3, 1.0, 5555, AnkiTypes::AnkiEnum::e3, "hello"};
  Foo bFoo;
  bFoo.myFoo = AnkiTypes::AnkiEnum::myReallySilly_EnumVal;
  bFoo = aFoo;

  ASSERT_EQ(AnkiTypes::AnkiEnum::e3, aFoo.myFoo);
  ASSERT_EQ(AnkiTypes::AnkiEnum::e3, bFoo.myFoo);

  Cat::MyMessage aWrapper = Cat::MyMessage::CreatemyFoo(std::move(aFoo));
  // use assignment opperator
  Cat::MyMessage bWrapper;
  bWrapper.Set_myDog(Baz::Dog(AnkiTypes::AnkiEnum::e3, 5));
  bWrapper = aWrapper;
  const Foo& bWrappersFoo = bWrapper.Get_myFoo();

  ASSERT_EQ("hello", aFoo.myString);
  ASSERT_EQ("hello", bWrappersFoo.myString);
  ASSERT_EQ(AnkiTypes::AnkiEnum::e3, bWrappersFoo.myFoo);

  // use move assignment opperator
  Cat::MyMessage cWrapper;
  cWrapper.Set_myDog(Baz::Dog(AnkiTypes::AnkiEnum::e3, 5));
  cWrapper = std::move(aWrapper);
  ASSERT_EQ(AnkiTypes::AnkiEnum::e3, cWrapper.Get_myFoo().myFoo);
  ASSERT_EQ("hello", cWrapper.Get_myFoo().myString);

  PASS();
}

TEST Union_UnionOfUnion() {
  UnionOfUnion myUnionOfUnion;
  UnionOfUnion otherUnionOfUnion;
  FooBarUnion myFooBarUnion;
  BarFooUnion myBarFooUnion;
  Foo aFoo {false, 1, 2, 3, 1.0, 5555, AnkiTypes::AnkiEnum::e3, "hello"};

  myFooBarUnion.Set_myFoo(aFoo);
  myUnionOfUnion.Set_myFooBar(myFooBarUnion);
  CLAD::SafeMessageBuffer  buff(myUnionOfUnion.Size());

  myUnionOfUnion.Pack(buff);
  otherUnionOfUnion = UnionOfUnion(buff);
  ASSERT_EQ(myUnionOfUnion, otherUnionOfUnion);

  PASS();
}

TEST Union_MessageOfUnion() {
  MessageOfUnion myMessageOfUnion;
  MessageOfUnion otherMessageOfUnion;
  FooBarUnion myFooBarUnion;
  BarFooUnion myBarFooUnion;
  Foo aFoo {false, 1, 2, 3, 1.0, 5555, AnkiTypes::AnkiEnum::e3, "hello"};

  myFooBarUnion.Set_myFoo(aFoo);
  myMessageOfUnion.anInt = 11;
  myMessageOfUnion.myFooBar = myFooBarUnion;
  myMessageOfUnion.aBool = true;

  CLAD::SafeMessageBuffer buff(myMessageOfUnion.Size());

  myMessageOfUnion.Pack(buff);
  otherMessageOfUnion = MessageOfUnion(buff);
  ASSERT_EQ(myMessageOfUnion, otherMessageOfUnion);

  PASS();
}

TEST Union_TagToType() {
  // Make sure the types match.
  // We do the check at runtime as is customary with unit tests, although
  // we could tell the answer at compile time with static_assert
  constexpr bool myFooIsCorrectType
    = std::is_same<Cat::MyMessage_TagToType<Cat::MyMessageTag::myFoo>::type, Foo>::value;
  ASSERT(myFooIsCorrectType);

  constexpr bool myBarIsCorrectType
    = std::is_same<Cat::MyMessage_TagToType<Cat::MyMessageTag::myBar>::type, Bar>::value;
  ASSERT(myBarIsCorrectType);

  PASS();
}

TEST Union_TemplatedAccessors() {
  // Regular Getter and templated Getter should return the same thing
  Foo aFoo {false, 1, 2, 3, 1.0, 5555, AnkiTypes::AnkiEnum::e3, "hello"};
  Cat::MyMessage msg;
  msg.Set_myFoo(aFoo);

  ASSERT_EQ(msg.Get_myFoo(), msg.Get_<Cat::MyMessageTag::myFoo>());
  ASSERT_EQ(msg.Get_myFoo().myShort, msg.Get_<Cat::MyMessageTag::myFoo>().myShort);

  Baz::Dog myDog;
  myDog.a = AnkiTypes::AnkiEnum::e2;
  myDog.b = 55;
  msg.Set_myDog(myDog);

  ASSERT_EQ(msg.Get_myDog().a, myDog.a);
  ASSERT_EQ(msg.Get_myDog(), msg.Get_<Cat::MyMessageTag::myDog>());

  PASS();
}

//
// Version Hash Tests
//
TEST Union_VersionHash() {
  // This will break if you change the contents of the ExplicitlyTaggedUnion.clad
  // Use the following to re-capture the hash value
  //std::cout << "ExplicitlyTaggedUnion Hash: " << ExplicitlyTaggedUnionVersionHashStr << std::endl;
  ASSERT_EQ(std::string("0334bf9f44f4305ce38459d6e463e3c3"), ExplicitlyTaggedUnionVersionHashStr);

  // The following is a cheezy trick to verify that a buffer is the same as the
  // given hex string.  This will not break when changing the clad file, so if
  // this breaks, it's a real bug. (pauley)
  std::stringstream buf;
  for (auto c : ExplicitlyTaggedUnionVersionHash) {
    buf << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }
  ASSERT_EQ(ExplicitlyTaggedUnionVersionHashStr, buf.str());
  PASS();
}


// For verifying that T(Arg&&) does or does not exist
template <bool B>
using bool_constant = std::integral_constant<bool, B>;
template <typename T, typename Arg>
struct IsExplicitlyConstructible 
  : public bool_constant< std::is_constructible<T, Arg&&>::value
                          && !std::is_convertible<Arg, T>::value >
{
};

TEST Union_DupesAllowed() {
  // This test is only valid if --output-union-helper-constructors is set.
  // This tests that the union should exist and can be set, and that it should not have
  // constructors accepting the union member structures if they appear in duplicate

  {
    DupesAllowedUnion testUnion;
    DuplicatedType duped;
    NonDuplicatedType normal;

    testUnion.Set_dupedMember1(duped);
    ASSERT_EQ(testUnion.GetTag(), DupesAllowedUnionTag::dupedMember1);
    
    testUnion.Set_dupedMember2(duped);
    ASSERT_EQ(testUnion.GetTag(), DupesAllowedUnionTag::dupedMember2);

    testUnion.Set_normalMember(normal);
    ASSERT_EQ(testUnion.GetTag(), DupesAllowedUnionTag::normalMember);
  }

  // check ctors for the types we care about
  constexpr bool dupedTypeHasCtor 
    = IsExplicitlyConstructible<DupesAllowedUnion, DuplicatedType>::value;
  constexpr bool nondupedTypeHasCtor 
    = IsExplicitlyConstructible<DupesAllowedUnion, NonDuplicatedType>::value;

  // if default (non- dupes_allowed) unions have constructors for creating the 
  // union from the union member type, then --output-union-helper-constructors is 
  // probably set.
  const bool otherTypeCtor = IsExplicitlyConstructible<FunkyMessage, Monkey>::value;
  if( otherTypeCtor ) {
    ASSERT_EQ( nondupedTypeHasCtor, true ); // no dupes==>possible to construct
    ASSERT_EQ( dupedTypeHasCtor, false ); // dupes==>no ctor so that we can allow dupes
  } else {
    // if the normal one doesnt have the ctors, no one else should either.
    // perhaps --output-union-helper-constructors was not set.
    ASSERT_EQ( nondupedTypeHasCtor, false );
    ASSERT_EQ( dupedTypeHasCtor, false );
  }
  
  PASS();
}

TEST Autounion_DupesAllowed() {
  DupesAutoUnion msg;

  AutoWithDupe withDupe{10};
  AutoNoDupe noDupe{true};

  msg.Set_explicitMember( withDupe );
  ASSERT_EQ(DupesAutoUnion::Tag::explicitMember, msg.GetTag());

  msg.Set_AutoWithDupe( withDupe );
  ASSERT_EQ(DupesAutoUnion::Tag::AutoWithDupe, msg.GetTag());

  msg.Set_AutoNoDupe( noDupe );
  ASSERT_EQ(DupesAutoUnion::Tag::AutoNoDupe, msg.GetTag());

  // check ctors for the types we care about
  constexpr bool dupedTypeHasCtor 
    = IsExplicitlyConstructible<DupesAutoUnion, AutoWithDupe>::value;
  constexpr bool nondupedTypeHasCtor 
    = IsExplicitlyConstructible<DupesAutoUnion, AutoNoDupe>::value;

  // if default (non- dupes_allowed) unions have constructors for creating the 
  // union from the union member type, then --output-union-helper-constructors is 
  // probably set.
  const bool otherTypeCtor = IsExplicitlyConstructible<FunkyMessage, Monkey>::value;
  if( otherTypeCtor ) {
    ASSERT_EQ( nondupedTypeHasCtor, true ); // no dupes==>possible to construct
    ASSERT_EQ( dupedTypeHasCtor, false ); // dupes==>no ctor so that we can allow dupes
  } else {
    // if the normal one doesnt have the ctors, no one else should either.
    // perhaps --output-union-helper-constructors was not set.
    ASSERT_EQ( nondupedTypeHasCtor, false );
    ASSERT_EQ( dupedTypeHasCtor, false );
  }  

  PASS();
}

TEST Enum_VersionHash() {
  // This will break if you change the contents of the ExplicitlyTaggedUnion.clad
  // Use the following to re-capture the hash value
  ASSERT_EQ(std::string("4377df63afd1c6d3fc8a46605033cd2e"), AnkiTypes::AnkiEnumVersionHashStr);

  std::stringstream buf;
  for (auto c : AnkiTypes::AnkiEnumVersionHash) {
    buf << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }
  ASSERT_EQ(AnkiTypes::AnkiEnumVersionHashStr, buf.str());
  PASS();
}

TEST Enum_NumEntries() {
  // This will break if you change the contents of the AnkiEnum.clad
  
  // NumEntries is really only useful if the entries are consecutive.
  // AnkiEnum contains non-consecutive entries, so should NOT be looped over
  // like we are here, but just for the sake of testing:

  const int8_t start = static_cast<int8_t>(AnkiTypes::AnkiEnum::e1);
  ASSERT_EQ(start, 15); 
  const int8_t nonConsecIdx = 4;
  const int8_t nonConsecVal = static_cast<int8_t>(AnkiTypes::AnkiEnum::d1);
  ASSERT_EQ(nonConsecVal, 5); 
  const int8_t numEntries = AnkiTypes::AnkiEnumNumEntries;
  ASSERT_EQ(numEntries, 7);
  const int8_t last = static_cast<int8_t>(AnkiTypes::AnkiEnum::d3);
  ASSERT_EQ(numEntries, last); // because non-consecutive

  AnkiTypes::AnkiEnum expected[7] = {AnkiTypes::AnkiEnum::e1, AnkiTypes::AnkiEnum::e2, AnkiTypes::AnkiEnum::e3,
                                     AnkiTypes::AnkiEnum::myReallySilly_EnumVal,
                                     AnkiTypes::AnkiEnum::d1, AnkiTypes::AnkiEnum::d2, AnkiTypes::AnkiEnum::d3};
  int8_t intVal=start;
  AnkiTypes::AnkiEnum val;
  for (int8_t entryIdx=0; entryIdx<numEntries; ++entryIdx) {
    if( nonConsecIdx==entryIdx ) { 
      // nonconsecutive...jump to the new element
      intVal = nonConsecVal;
    } 
    val = static_cast<AnkiTypes::AnkiEnum>(intVal);
    ASSERT(val == expected[entryIdx]);
    ++intVal;
  }

  PASS();
}
TEST Message_VersionHash() {
  // This will break if you change the contents of the ExplicitlyTaggedUnion.clad
  // Use the following to re-capture the hash value
  //std::cout << "Foo Hash: " << FooVersionHashStr << std::endl;
  ASSERT_EQ(std::string("e58f3490bd215aea36240c4456416437"), FooVersionHashStr);

  std::stringstream buf;
  for (auto c : FooVersionHash) {
    buf << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }
  ASSERT_EQ(FooVersionHashStr, buf.str());
  PASS();
}

TEST DefaultValues_Ints() {
  // This will break if the default values specified in DefaultValues.clad change
  IntsWithDefaultValue firstData;
  ASSERT_EQ(firstData.a, 42);
  ASSERT_EQ(firstData.b, 0xff);
  ASSERT_EQ(firstData.c, -2);
  ASSERT_EQ(firstData.d, 1);

  // Ensure we can still fully specify the data
  IntsWithDefaultValue otherData { 1, 1, 1, 0 };
  ASSERT_EQ(otherData.a, 1);
  ASSERT_EQ(otherData.b, 1);
  ASSERT_EQ(otherData.c, 1);
  ASSERT_EQ(otherData.d, 0);

  // Ensure we can still partially specify the data
  IntsWithDefaultValue lastData;
  lastData.c = -10;
  lastData.d = false;
  ASSERT_EQ(lastData.a, 42);
  ASSERT_EQ(lastData.b, 0xff);
  ASSERT_EQ(lastData.c, -10);
  ASSERT_EQ(lastData.d, false);

  PASS();
}

TEST DefaultValues_Floats() {
  // This will break if the default values specified in DefaultValues.clad change
  FloatsWithDefaultValue firstData;
  ASSERT_EQ(firstData.a, 0.42f);
  ASSERT_EQ(firstData.b, 12.0f);
  ASSERT_EQ(firstData.c, 10.0101);
  ASSERT_EQ(firstData.d, -2.0f);

  // Ensure we can still fully specify the data
  FloatsWithDefaultValue otherData { 1.0, 1.0, 1.0, 1.0 };
  ASSERT_EQ(otherData.a, 1.0);
  ASSERT_EQ(otherData.b, 1.0);
  ASSERT_EQ(otherData.c, 1.0);
  ASSERT_EQ(otherData.d, 1.0);

  // Ensure we can still partially specify the data
  FloatsWithDefaultValue lastData;
  lastData.c = -10;
  lastData.d = false;
  ASSERT_EQ(lastData.a, 0.42f);
  ASSERT_EQ(lastData.b, 12.0f);
  ASSERT_EQ(lastData.c, -10);
  ASSERT_EQ(lastData.d, false);

  PASS();
}

TEST JsonSerialization_BasicTypes() {
  std::string json = R"json(
{
	"testBool": true,
	"testString": "stringValue",
	"testShort": 26723,
  "testFloat": 111.5
}
)json";

  // Load Json from string into jsoncpp Json::Value
  Json::Reader reader;
  Json::Value root;
  ASSERT(reader.parse(json, root));

  // testStruct is the CLAD generated class that we want to serialize into.
  JsonSerialization::testStructure_BasicTypes testStruct;
  ASSERT(testStruct.SetFromJSON(root));

  // Ensure data in objects matches data from the json string.
  ASSERT_EQ(testStruct.testBool, true);
  ASSERT_EQ(testStruct.testString, "stringValue");
  ASSERT_EQ(testStruct.testShort, 26723);
  ASSERT_EQ(testStruct.testFloat, 111.5);

  // GetJSON() will serialize a CLAD generated structure into a Json::Value, the
  // opposite of what we're doing above.
  // This reloads the output of GetJSON() back into a Json::Value and then back into
  // another CLAD object, they should then contain the same data.
  JsonSerialization::testStructure_BasicTypes testStructReserialized;
  testStructReserialized.SetFromJSON(testStruct.GetJSON());
  ASSERT(testStruct == testStructReserialized);

  PASS();
}

TEST JsonSerialization_ListOfEnums() {
  std::string json = R"json(
{
	"vals": ["Zero", "One", "Two", "Five", "One"]
}
)json";

  using namespace JsonSerialization;
  
  // Load Json from string into jsoncpp Json::Value
  Json::Reader reader;
  Json::Value root;
  ASSERT(reader.parse(json, root));

  // testStruct is the CLAD generated class that we want to serialize into.
  testStructure_ListOfEnums testStruct;
  ASSERT(testStruct.SetFromJSON(root));

  // Ensure data in objects matches data from the json string.
  ASSERT_EQ(testStruct.vals.size(), 5);

  ASSERT_EQ(testStruct.vals[0], testEnum::Zero);
  ASSERT_EQ(testStruct.vals[1], testEnum::One);
  ASSERT_EQ(testStruct.vals[2], testEnum::Two);
  ASSERT_EQ(testStruct.vals[3], testEnum::Five);
  ASSERT_EQ(testStruct.vals[4], testEnum::One);

  // GetJSON() will serialize a CLAD generated structure into a Json::Value, the
  // opposite of what we're doing above.
  // This reloads the output of GetJSON() back into a Json::Value and then back into
  // another CLAD object, they should then contain the same data.
  testStructure_ListOfEnums testStructReserialized;
  testStructReserialized.SetFromJSON(testStruct.GetJSON());
  ASSERT(testStruct == testStructReserialized);

  PASS();
}

TEST JsonSerialization_List() {
  std::string json = R"json(
{
	"testList": [0,1,2,3,4,5,6,7,8,9]
}
)json";

  // Load Json from string into jsoncpp Json::Value
  Json::Reader reader;
  Json::Value root;
  ASSERT(reader.parse(json, root));

  // testStruct is the CLAD generated class that we want to serialize into.
  JsonSerialization::testStructure_Lists testStruct;
  ASSERT(testStruct.SetFromJSON(root));

  // Ensure data in objects matches data from the json string.
  ASSERT_EQ(testStruct.testList.size(), 10);

  for(int i = 0; i < 10; i++) {
    ASSERT_EQ(testStruct.testList[i], i);
  }

  // GetJSON() will serialize a CLAD generated structure into a Json::Value, the
  // opposite of what we're doing above.
  // This reloads the output of GetJSON() back into a Json::Value and then back into
  // another CLAD object, they should then contain the same data.
  JsonSerialization::testStructure_Lists testStructReserialized;
  testStructReserialized.SetFromJSON(testStruct.GetJSON());
  ASSERT(testStruct == testStructReserialized);

  PASS();
}

TEST JsonSerialization_Nested() {
  std::string json = R"json(
{
  "testNestedStructure": {
    "testDouble": -111.5,
    "testUint64": 18446744073709551615,
    "testStruct1": {
      "test": true
    },
    "testStruct2": {
      "test": 99999
    },
    "testStruct3": {
      "test": "Just string things"
    }
  }
}
)json";

  // Load Json from string into jsoncpp Json::Value
  Json::Reader reader;
  Json::Value root;
  ASSERT(reader.parse(json, root));

  // testStruct is the CLAD generated class that we want to serialize into.
  JsonSerialization::testStructure_Nested testStruct;
  ASSERT(testStruct.SetFromJSON(root));

  // Ensure data in objects matches data from the json string.
  ASSERT_EQ(testStruct.testNestedStructure.testDouble, -111.5);
  ASSERT_EQ(testStruct.testNestedStructure.testUint64, 18446744073709551615UL);

  ASSERT_EQ(testStruct.testNestedStructure.testStruct1.test, true);
  ASSERT_EQ(testStruct.testNestedStructure.testStruct2.test, 99999);
  ASSERT_EQ(testStruct.testNestedStructure.testStruct3.test, "Just string things");

  // Ensure that GetJSON is producing equivalent results as loading from the file.
  JsonSerialization::testStructure_Nested testStructReserialized;
  testStructReserialized.SetFromJSON(testStruct.GetJSON());
  ASSERT(testStruct == testStructReserialized);

  {
    using namespace JsonSerialization;
    // Ensure that no_default_constructor is working with JsonSerialization
    ASSERT_EQ((std::is_constructible<testStructure_NoDefault>::value), false);
    ASSERT_EQ((std::is_constructible<testStructure_NoDefault, std::string>::value), true);

    ASSERT_EQ((std::is_constructible<testStructure_NestedNoDefault>::value), false);
    ASSERT_EQ((std::is_constructible<testStructure_NestedNoDefault, bool, int, testStructure_NoDefault>::value), true);

    // Unions are actually constructible even if they contain no_default_constructor members, because the
    // default union constrcutor doesn't construct a member
    ASSERT_EQ((std::is_constructible<testUnion_NestedNoDefault>::value), true);
  }

  PASS();
}

TEST JsonSerialization_Unions() {
  std::string json = R"json(
{
  "testUnionPrimitive": {
    "type": "testUint8",
    "value": 127
  },
  "testUnionStructures": {
    "type": "testStruct3",
    "test": "stringValue"
  }
}
)json";

  // Load Json from string into jsoncpp Json::Value
  Json::Reader reader;
  Json::Value root;
  ASSERT(reader.parse(json, root));

  // testStruct is the CLAD generated class that we want to serialize into.
  JsonSerialization::testStructure_Unions testStruct;
  ASSERT(testStruct.SetFromJSON(root));

  // Ensure data in objects matches data from the json string.
  ASSERT_EQ(testStruct.testUnionPrimitive.GetTag(), JsonSerialization::testUnionPrimitiveTag::testUint8);
  ASSERT_EQ(testStruct.testUnionPrimitive.Get_testUint8(), 127);
  ASSERT_EQ(testStruct.testUnionStructures.GetTag(), JsonSerialization::testUnionStructuresTag::testStruct3);
  ASSERT_EQ(testStruct.testUnionStructures.Get_testStruct3().test, "stringValue");

  // Ensure that GetJSON is producing equivalent results as loading from the file.
  JsonSerialization::testStructure_Unions testStructReserialized;
  testStructReserialized.SetFromJSON(testStruct.GetJSON());
  ASSERT(testStruct == testStructReserialized);

  PASS();
}

TEST JsonSerialization_Enums() {

  JsonSerialization::testEnum e = JsonSerialization::testEnum::Five;
  const std::string& str = JsonSerialization::testEnumToString(e);
  ASSERT_EQ(str, "Five");

  const bool ret1 = JsonSerialization::EnumFromString("One", e);
  ASSERT(ret1);
  ASSERT_EQ(e, JsonSerialization::testEnum::One);

  const bool ret2 = JsonSerialization::testEnumFromString("Two", e);
  ASSERT(ret2);
  ASSERT_EQ(e, JsonSerialization::testEnum::Two);

  const bool ret3 = JsonSerialization::testEnumFromString("asdf", e);
  ASSERT(!ret3);

  std::string json = R"json(
{
  "enumVal": "Two"
}
)json";
  
  // Load Json from string into jsoncpp Json::Value
  Json::Reader reader;
  Json::Value root;
  ASSERT(reader.parse(json, root));

  // testStruct is the CLAD generated class that we want to serialize into.
  JsonSerialization::testStructure_Enums testStruct;
  ASSERT(testStruct.SetFromJSON(root));

  ASSERT_EQ(testStruct.enumVal, JsonSerialization::testEnum::Two);

  ASSERT_EQ((int)JsonSerialization::testEnum::Two, 2);
  ASSERT_EQ((int)testStruct.enumVal, 2);

  // Ensure that GetJSON is producing equivalent results as loading from the file.
  JsonSerialization::testStructure_Enums testStructReserialized;
  testStructReserialized.SetFromJSON(testStruct.GetJSON());
  ASSERT(testStruct == testStructReserialized);

  PASS();
}
  

TEST JsonSerialization_PartialJson() {
  std::string json = R"json(
{
  "testBool": false
}
)json";

  // Load Json from string into jsoncpp Json::Value
  Json::Reader reader;
  Json::Value root;
  ASSERT(reader.parse(json, root));

  // testStruct is the CLAD generated class that we want to serialize into.
  JsonSerialization::testStructure_PartialJson testStruct;
  testStruct.testBool = true; // This value should change because it exists in the Json string.
  testStruct.unreadValue = "This should not change when calling SetFromJSON.";

  ASSERT(testStruct.SetFromJSON(root));

  // Ensure data in objects matches data from the json string, and that SetFromJSON() did
  // not change the value that was not in the json string.
  ASSERT_EQ(testStruct.testBool, false);
  ASSERT_EQ(testStruct.unreadValue, "This should not change when calling SetFromJSON.");

  // Ensure that GetJSON is producing equivalent results as loading from the file.
  JsonSerialization::testStructure_PartialJson testStructReserialized;
  testStructReserialized.SetFromJSON(testStruct.GetJSON());
  ASSERT(testStruct == testStructReserialized);

  PASS();
}

TEST JsonSerialization_ReturnVal()
{
  // a list of pairs of json values (as string) and a bool. SetFromJSON should return the same value as the
  // bool

  std::vector<std::pair<std::string, bool>> jsonMap = {{
    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [],
  "floatValList": [],
  "intValList": [],
  "enumValList": [],
  "unionValList": []
})json",
   true},

    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "asdf",
    "test": false
  },

  "boolValList": [],
  "floatValList": [],
  "intValList": [],
  "enumValList": [],
  "unionValList": []
})json",
   false},
    
    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "asdf",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [],
  "floatValList": [],
  "intValList": [],
  "enumValList": [],
  "unionValList": []
})json",
   false},

    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [true, false, false],
  "floatValList": [1.05, 2, -99.999, 0.01],
  "intValList": [1, 2, 3],
  "enumValList": ["One", "Two", "Five"],
  "unionValList": [
    {
      "type": "testStruct1",
      "test": true
    },
    {
      "type": "testStruct2",
      "test": 33
    },
    {
      "type": "testStruct3",
      "test": "strVal"
    }
  ]
})json",
   true},

    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [true, false, false],
  "floatValList": [1.05, 2, -99.999, 0.01],
  "intValList": [1, 2, 3],
  "enumValList": ["One", "Two", "Five"],
  "unionValList": [
    {
      "type": "testStruct1",
      "test": true
    },
    {
      "type": "testStruct2_invalid",
      "test": 33
    },
    {
      "type": "testStruct3",
      "test": "strVal"
    }
  ]
})json",
   false},    

    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [true, false, false],
  "floatValList": [1.05, 2, -99.999, 0.01],
  "intValList": [1, 2, 3],
  "enumValList": ["One", "Two", "Five", "not_a_val"],
  "unionValList": []
})json",
   false},
    
    {R"json(
{
  "boolVal": "notABool",
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [],
  "floatValList": [],
  "intValList": [],
  "enumValList": [],
  "unionValList": []
})json",
   false},

    {R"json(
{
  "boolVal": true,
  "floatVal": "notAFloat",
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [],
  "floatValList": [],
  "intValList": [],
  "enumValList": [],
  "unionValList": []
})json",
   false},

    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": "notAnInt",
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [],
  "floatValList": [],
  "intValList": [],
  "enumValList": [],
  "unionValList": []
})json",
   false},

    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": ["notAbool"],
  "floatValList": [],
  "intValList": [],
  "enumValList": [],
  "unionValList": []
})json",
   false},

    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [],
  "floatValList": [3.1, "notAFloat"],
  "intValList": [],
  "enumValList": [],
  "unionValList": []
})json",
   false},

    {R"json(
{
  "boolVal": true,
  "floatVal": 3.14,
  "intVal": 7,
  "enumVal": "Five",
  "unionVal": {
    "type": "testStruct1",
    "test": false
  },

  "boolValList": [],
  "floatValList": [],
  "intValList": ["notAnInt", 4],
  "enumValList": [],
  "unionValList": []
})json",
   false},

    }};

  Json::Reader reader;
  
  for( const auto& stringPair : jsonMap ) {
    Json::Value root;
    ASSERT(reader.parse(stringPair.first, root));

    JsonSerialization::testStructure_WithLists test;
    ASSERT_EQ(test.SetFromJSON(root), stringPair.second);
  }
  
  PASS();
}

TEST Enum_Complex() {
  ASSERT_EQ(FooEnum::foo1, 0);
  ASSERT_EQ(FooEnum::foo2, 8);
  ASSERT_EQ(FooEnum::foo3, 9);
  ASSERT_EQ(FooEnum::foo4, 10);
  ASSERT_EQ(FooEnum::foo5, 1280);
  ASSERT_EQ(FooEnum::foo6, 1281);
  ASSERT_EQ(FooEnum::foo7, 1000);
  
  ASSERT_EQ((unsigned int)BarEnum::bar1, 0);
  ASSERT_EQ((unsigned int)BarEnum::bar2, 8);
  ASSERT_EQ((unsigned int)BarEnum::bar3, 9);
  ASSERT_EQ((unsigned int)BarEnum::bar4, 1291);
  ASSERT_EQ((unsigned int)BarEnum::bar5, 16);
  ASSERT_EQ((unsigned int)BarEnum::bar6, 17);
  
  PASS();
}

TEST DefaultConstructor() {
  // HasDefaultConstructor should be constructible with no arguments or two arguments
  ASSERT_EQ((std::is_constructible<Constructor::HasDefaultConstructor>::value), true);
  ASSERT_EQ((std::is_constructible<Constructor::HasDefaultConstructor, double, int>::value), true);
  
  // HasNoDefaultConstructor should NOT be constructible with no arguments but should be constructible with two arguments
  ASSERT_EQ((std::is_constructible<Constructor::HasNoDefaultConstructor>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::HasNoDefaultConstructor, double, int>::value), true);

  ASSERT_EQ((std::is_constructible<Constructor::HasNoDefaultConstructorString>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::HasNoDefaultConstructorString, std::string>::value), true);

  ASSERT_EQ((std::is_constructible<Constructor::NestedString>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::NestedString, std::string>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::NestedString, Constructor::HasNoDefaultConstructorString>::value), true);

  ASSERT_EQ((std::is_constructible<Constructor::NoDefaultConstructorComplex>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::NoDefaultConstructorComplex,
                                   Constructor::HasDefaultConstructor,
                                   std::string,
                                   std::array<unsigned char, 20>>::value), true);
  
  ASSERT_EQ((std::is_constructible<Constructor::MessageWithStruct>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::MessageWithStruct,
                                   Constructor::HasNoDefaultConstructor,
                                   int,
                                   float,
                                   Constructor::HasDefaultConstructor>::value), true);
  
  ASSERT_EQ((std::is_constructible<Constructor::OtherMessageWithStruct>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::OtherMessageWithStruct,
                                   Constructor::NoDefaultConstructorComplex,
                                   Constructor::HasDefaultConstructor>::value), true);
  
  ASSERT_EQ((std::is_constructible<Constructor::NestedNoDefaults>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::NestedNoDefaults,
                                   Constructor::NoDefaultConstructorComplex,
                                   Constructor::HasDefaultConstructor,
                                   std::array<unsigned char, 20>,
                                   Constructor::HasNoDefaultConstructor,
                                   std::string>::value), true);
  
  ASSERT_EQ((std::is_constructible<Constructor::SuperComplex>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::SuperComplex,
                                   Constructor::NestedNoDefaults,
                                   Constructor::HasDefaultConstructor,
                                   Constructor::NoDefaultConstructorComplex>::value), true);
  
  ASSERT_EQ((std::is_constructible<Constructor::Nest1>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::Nest1,
             Constructor::HasNoDefaultConstructor>::value), true);
  
  ASSERT_EQ((std::is_constructible<Constructor::Nest2>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::Nest2,
             Constructor::Nest1>::value), true);
  
  ASSERT_EQ((std::is_constructible<Constructor::Nest3>::value), false);
  ASSERT_EQ((std::is_constructible<Constructor::Nest3,
             Constructor::Nest2>::value), true);
  
  PASS();
}

TEST FixedArray() {
  Arrays::s s;
  ASSERT_EQ(s.arr8.size() == Arrays::ArrSize::sizeTen, true);
  ASSERT_EQ(s.arr16.size() == Arrays::ArrSize::sizeTwenty, true);
  ASSERT_EQ(s.Size() == (Arrays::ArrSize::sizeTen * sizeof(uint8_t) + Arrays::ArrSize::sizeTwenty * sizeof(uint16_t)), true);
  
  Arrays::m m;
  ASSERT_EQ(m.arr8.size() == Arrays::ArrSize::sizeTen, true);
  ASSERT_EQ(m.arr16.size() == Arrays::ArrSize::sizeTwenty, true);
  ASSERT_EQ(m.Size() == (Arrays::ArrSize::sizeTen * sizeof(uint8_t) + Arrays::ArrSize::sizeTwenty * sizeof(uint16_t)), true);
  
  PASS();
}

TEST EnumConcept() {

  ASSERT_EQ(IsFooOdd(FooEnum::foo1, false), true);
  ASSERT_EQ(IsFooOdd(FooEnum::foo2, false), false);
  ASSERT_EQ(IsFooOdd(FooEnum::foo3, false), true);
  ASSERT_EQ(IsFooOdd(FooEnum::foo4, false), false);
  ASSERT_EQ(IsFooOdd(FooEnum::foo5, false), true);
  ASSERT_EQ(IsFooOdd(FooEnum::foo6, false), false);
  ASSERT_EQ(IsFooOdd(FooEnum::foo7, false), true);

  ASSERT_EQ(IsFoo4(FooEnum::foo1, false), false);
  ASSERT_EQ(IsFoo4(FooEnum::foo2, false), false);
  ASSERT_EQ(IsFoo4(FooEnum::foo3, false), false);
  ASSERT_EQ(IsFoo4(FooEnum::foo4, false), true);
  ASSERT_EQ(IsFoo4(FooEnum::foo5, false), false);
  ASSERT_EQ(IsFoo4(FooEnum::foo6, false), false);
  ASSERT_EQ(IsFoo4(FooEnum::foo7, false), false);

  ASSERT_EQ(BarEnumToFooEnum(BarEnum::bar1, FooEnum::foo7), FooEnum::foo1);
  ASSERT_EQ(BarEnumToFooEnum(BarEnum::bar2, FooEnum::foo7), FooEnum::foo2);
  ASSERT_EQ(BarEnumToFooEnum(BarEnum::bar3, FooEnum::foo7), FooEnum::foo3);
  ASSERT_EQ(BarEnumToFooEnum(BarEnum::bar4, FooEnum::foo7), FooEnum::foo4);
  ASSERT_EQ(BarEnumToFooEnum(BarEnum::bar5, FooEnum::foo7), FooEnum::foo5);
  ASSERT_EQ(BarEnumToFooEnum(BarEnum::bar6, FooEnum::foo7), FooEnum::foo6);

  ASSERT_EQ(BarToString(BarEnum::bar1, ""), "bar1");
  ASSERT_EQ(BarToString(BarEnum::bar2, ""), "bar2");
  ASSERT_EQ(BarToString(BarEnum::bar3, ""), "bar3");
  ASSERT_EQ(BarToString(BarEnum::bar4, ""), "bar4");
  ASSERT_EQ(BarToString(BarEnum::bar5, ""), "bar5");
  ASSERT_EQ(BarToString(BarEnum::bar6, ""), "bar6");

  // Check DoubleFoo returns a float
  ASSERT_EQ(typeid(DoubleFoo(FooEnum::foo1, 0)).name(), typeid(1.f).name());
  ASSERT_EQ(DoubleFoo(FooEnum::foo1, 0), FooEnum::foo1 * 2.f);
  ASSERT_EQ(DoubleFoo(FooEnum::foo2, 0), FooEnum::foo2 * 2.f);
  ASSERT_EQ(DoubleFoo(FooEnum::foo3, 0), FooEnum::foo3 * 2.f);
  ASSERT_EQ(DoubleFoo(FooEnum::foo4, 0), FooEnum::foo4 * 2.f);
  ASSERT_EQ(DoubleFoo(FooEnum::foo5, 0), FooEnum::foo5 * 2.f);
  ASSERT_EQ(DoubleFoo(FooEnum::foo6, 0), FooEnum::foo6 * 2.f);
  ASSERT_EQ(DoubleFoo(FooEnum::foo7, 0), FooEnum::foo7 * 2.f);

  // Make sure we call two enum concepts with the same name
  ASSERT_EQ(IsValid(FooEnum::foo1, false), true);
  ASSERT_EQ(IsValid(FooEnum::foo2, false), false);
  ASSERT_EQ(IsValid(BarEnum::bar1, false), true);
  ASSERT_EQ(IsValid(BarEnum::bar2, false), false);

  // Check default value is returned
  ASSERT_EQ(IsValid(static_cast<FooEnum>(100000000), true), true);

  PASS();
}

SUITE(CPP_Emitter) {

  // Enum Tests
  RUN_TEST(AnkiEnum_Basics);
  RUN_TEST(AnkiEnum_NoClass);
  RUN_TEST(Enum_VersionHash);
  RUN_TEST(Enum_Complex);
  RUN_TEST(Enum_NumEntries);

  // Message Tests
  RUN_TEST(Message_VersionHash);

  // RoundTripTests
  RUN_TEST(Foo_should_round_trip);
  RUN_TEST(Bar_should_round_trip);
  RUN_TEST(Dog_should_round_trip);
  RUN_TEST(Poodle_should_init);
  RUN_TEST(SoManyStrings_should_round_trip);
  RUN_TEST(od432_should_round_trip);
  RUN_TEST(od433_should_round_trip);
  RUN_TEST(union_should_round_trip_after_reuse);


  // Operator overrides
  RUN_TEST(CopyConstructors_should_round_trip);
  RUN_TEST(AssignmentOperators);

  // Union Tests
  RUN_TEST(Union_UnionOfUnion);
  RUN_TEST(Union_MessageOfUnion);
  RUN_TEST(Union_VersionHash);

  // Autounion
  RUN_TEST(Autounion_should_exist);

  // Union with dupes_allowed
  RUN_TEST(Union_DupesAllowed);
  // Autounion with dupes_allowed
  RUN_TEST(Autounion_DupesAllowed);

  // Template helpies
  RUN_TEST(Union_TagToType);
  RUN_TEST(Union_TemplatedAccessors);

  // Default Values
  RUN_TEST(DefaultValues_Ints);
  RUN_TEST(DefaultValues_Floats);
  
  // Default constructor generation
  RUN_TEST(DefaultConstructor);
  
  RUN_TEST(FixedArray);


  // Json Serialization
  RUN_TEST(JsonSerialization_BasicTypes);
  RUN_TEST(JsonSerialization_List);
  RUN_TEST(JsonSerialization_ListOfEnums);
  RUN_TEST(JsonSerialization_Nested);
  RUN_TEST(JsonSerialization_Unions);
  RUN_TEST(JsonSerialization_Enums);
  RUN_TEST(JsonSerialization_PartialJson);
  RUN_TEST(JsonSerialization_ReturnVal);

  // Enum Concept
  RUN_TEST(EnumConcept);
}

/* Add definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

/* Set up, run suite(s) of tests, report pass/fail/skip stats. */
int run_tests(void) {
    GREATEST_INIT();            /* init. greatest internals */
    /* List of suites to run. */
    RUN_SUITE(CPP_Emitter);
    GREATEST_REPORT();          /* display results */
    return greatest_all_passed();
}

/* main(), for a standalone command-line test runner.
 * This replaces run_tests above, and adds command line option
 * handling and exiting with a pass/fail status. */
int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* init & parse command-line args */
    RUN_SUITE(CPP_Emitter);
    GREATEST_MAIN_END();        /* display results */
}
