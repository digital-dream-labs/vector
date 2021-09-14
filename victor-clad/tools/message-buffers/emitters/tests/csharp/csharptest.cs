// Copyright 2015-2016 Anki Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// CSharp message packing test
//

public class HelloWorld {
  public static bool Test_Foo() {
    Foo myFoo = new Foo(true, 0x7f, 0xfe, 0xafe, -123123,
                        0xdeadbeef, AnkiTypes.AnkiEnum.myReallySilly_EnumVal,
                        "Blah Blah Blah");
    Foo otherFoo = new Foo();

    System.Console.WriteLine("myFoo = " + myFoo.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    myFoo.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherFoo.Unpack(stream);

    return myFoo.Equals(otherFoo);
  }

  public static bool Test_Bar() {
    Bar myBar;
    Bar otherBar;

    myBar = new Bar(new bool[3] { true, false, false },
                    new sbyte[5] { 0, 1, 2, 3, 4 },
                    new short[3] { 5, 6, 7 },
                    new AnkiTypes.AnkiEnum[2] { AnkiTypes.AnkiEnum.d1,
                                                AnkiTypes.AnkiEnum.e1 },
                    new double[] { double.MaxValue, double.PositiveInfinity,
                                   double.MinValue, -0, +0, double.NegativeInfinity, 1, -1 },
                    "Foo Bar Baz",
                    new short[20] {1,2,3,4,5,6,7,8,9,10,
                                   11,12,13,14,15,16,17,18,19,20},
                    new bool[10] {false, false, false, false, false,
                                  true, true, true, true, true},
                    new AnkiTypes.AnkiEnum[2] { AnkiTypes.AnkiEnum.e1,
                                                AnkiTypes.AnkiEnum.e3 });


    System.Console.WriteLine("myBar = " + myBar.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    myBar.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherBar = new Bar(stream);

    return myBar.Equals(otherBar);
  }

  public static bool Test_BarInitialize() {
    Bar myBar;
    Bar otherBar;

    myBar = new Bar(new bool[3] { true, false, false },
      new sbyte[5] { 0, 1, 2, 3, 4 },
      new short[3] { 5, 6, 7 },
      new AnkiTypes.AnkiEnum[2] { AnkiTypes.AnkiEnum.d1,
        AnkiTypes.AnkiEnum.e1 },
      new double[] { double.MaxValue, double.PositiveInfinity,
        double.MinValue, -0, +0, double.NegativeInfinity, 1, -1 },
      "Foo Bar Baz",
      new short[20] {1,2,3,4,5,6,7,8,9,10,
        11,12,13,14,15,16,17,18,19,20},
      new bool[10] {false, false, false, false, false,
        true, true, true, true, true},
      new AnkiTypes.AnkiEnum[2] { AnkiTypes.AnkiEnum.e1,
        AnkiTypes.AnkiEnum.e3 });

    otherBar = new Bar();

    otherBar.Initialize(new bool[3] { true, false, false },
      new sbyte[5] { 0, 1, 2, 3, 4 },
      new short[3] { 5, 6, 7 },
      new AnkiTypes.AnkiEnum[2] { AnkiTypes.AnkiEnum.d1,
        AnkiTypes.AnkiEnum.e1 },
      new double[] { double.MaxValue, double.PositiveInfinity,
        double.MinValue, -0, +0, double.NegativeInfinity, 1, -1 },
      "Foo Bar Baz",
      new short[20] {1,2,3,4,5,6,7,8,9,10,
        11,12,13,14,15,16,17,18,19,20},
      new bool[10] {false, false, false, false, false,
        true, true, true, true, true},
      new AnkiTypes.AnkiEnum[2] { AnkiTypes.AnkiEnum.e1,
        AnkiTypes.AnkiEnum.e3 });

    return myBar.Equals(otherBar);
  }

  public static bool Test_Dog() {
    Baz.Dog myDog;
    Baz.Dog otherDog;

    myDog = new Baz.Dog(AnkiTypes.AnkiEnum.e1, 5);
    System.Console.WriteLine("myDog = " + myDog.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    myDog.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherDog = new Baz.Dog(stream);

    return myDog.Equals(otherDog);
  }

  public static bool Test_Poodle() {
    Baz.Poodle myPoodle = new Baz.Poodle();
    return (myPoodle.a == AnkiTypes.AnkiEnum.d1);
  }
    
  public static bool Test_od432() {
    od432 myOD432;
    od432 otherOD432;
    Foo myFoo = new Foo(true, 0x7f, 0xfe, 0xafe, -123123, 0xdeadbeef,
                        AnkiTypes.AnkiEnum.myReallySilly_EnumVal,
                        "Blah Blah Blah");
    myOD432 = new od432(myFoo, 5, LEDColor.CurrentColor);
    System.Console.WriteLine("myOD432 = " + myOD432.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    myOD432.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherOD432 = new od432(stream);

    return myOD432.Equals(otherOD432);
  }

  public static bool Test_od433() {
    od433 myOD433;
    od433 otherOD433;
    Foo fooA = new Foo(true, 0x7f, 0xfe, 0xafe, -123123, 0xdeadbeef,
                       AnkiTypes.AnkiEnum.myReallySilly_EnumVal,
                       "Blah Blah Blah");
    Foo fooB = new Foo(false, 0x1f, 0xfc, 0x123, -26874, 0xbeefdead,
                       AnkiTypes.AnkiEnum.e3,
                       "Yeah Yeah Yeah");
    Foo fooC = new Foo(true, 0x3f, 0xae, 0xbfe, -143123, 0xdeedbeef,
                       AnkiTypes.AnkiEnum.d1,
                       "Doh Doh Doh");
    Foo fooD = new Foo(true, 0xf, 0xfd, 0x143, -25874, 0xbeefdeed,
                       AnkiTypes.AnkiEnum.e2,
                       "Yo Yo Yo");
    myOD433 = new od433(new Foo[2] { fooA, fooB }, new Foo[2] { fooC, fooD }, 6);
    System.Console.WriteLine("myOD433 = " + myOD433.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    myOD433.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherOD433 = new od433(stream);

    return myOD433.Equals(otherOD433);
  }

  public static bool Test_SoManyStrings() {
    SoManyStrings mySoManyStrings;
    SoManyStrings otherSoManyStrings;

    mySoManyStrings = new SoManyStrings(new string[2] { "one", "two" },
                                        new string[3] { "uno", "dos", "tres" },
                                        new string[4] { "yi", "ar", "san", "si" },
                                        new string[2] { "un", "deux" });
    System.Console.WriteLine("mySoManyStrings = " + mySoManyStrings.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    mySoManyStrings.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherSoManyStrings = new SoManyStrings(stream);

    return mySoManyStrings.Equals(otherSoManyStrings);
  }

  public static bool Test_od434() {
    od434 myOD434;
    od434 otherOD434;

    myOD434 = new od434(new ulong[3] { 1UL, 2UL, 3UL });

    System.Console.WriteLine("myOD434 = " + myOD434.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    myOD434.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherOD434 = new od434(stream);

    return myOD434.Equals(otherOD434);
  }

  public static bool Test_Union() {
    Cat.MyMessage msg = new Cat.MyMessage();
    Cat.MyMessage otherMsg = new Cat.MyMessage();

    Foo myFoo = new Foo();

    myFoo.myByte = 0x7f;
    myFoo.byteTwo = 0xfe;
    myFoo.myShort = 0x0afe;
    myFoo.myFloat = 123.123123e12f;
    myFoo.myNormal = 0x0eadbeef;
    myFoo.myFoo = AnkiTypes.AnkiEnum.myReallySilly_EnumVal;
    myFoo.myString = "Whatever";

    msg.myFoo = myFoo;

    System.Console.WriteLine("msg = " + msg.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    msg.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherMsg.Unpack(stream);

    if (!msg.Equals(otherMsg)) {
      return false;
    }

    Bar myBar = new Bar();

    myBar.boolBuff = new bool[3] { true, false, true };
    myBar.byteBuff = new sbyte[4] { 0x0f, 0x0e, 0x0c, 0x0a };
    myBar.shortBuff = new short[4] { 0x0fed, 0x0caf, 0x0a2f, 0x0a12 };
    myBar.enumBuff = new AnkiTypes.AnkiEnum[2] { AnkiTypes.AnkiEnum.myReallySilly_EnumVal,
                                                 AnkiTypes.AnkiEnum.e2 };
    myBar.doubleBuff = new double[1] { double.NaN };
    myBar.myLongerString = "SomeLongerStupidString";
    myBar.fixedBuff = new short[20] {1,2,3,4,5,6,7,8,9,10,
                                     11,12,13,14,15,16,17,18,19,20};
    myBar.fixedBoolBuff = new bool[10] {false, false, false, false, false,
                                        true, true, true, true, true};


    msg.myBar = myBar;

    System.Console.WriteLine("msg = " + msg.Size + " bytes");
    stream = new System.IO.MemoryStream();
    msg.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherMsg.Unpack(stream);

    if (!msg.Equals(otherMsg)) {
      return false;
    }

    Baz.Dog myDog = new Baz.Dog();

    myDog.a = AnkiTypes.AnkiEnum.e3;
    myDog.b = 9;

    msg.myDog = myDog;
    System.Console.WriteLine("msg = " + msg.Size + " bytes");
    stream = new System.IO.MemoryStream();
    msg.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherMsg.Unpack(stream);

    if (!msg.Equals(otherMsg)) {
      return false;
    }

    SoManyStrings mySoManyStrings = new SoManyStrings();

    mySoManyStrings.varStringBuff = new string[2] { "one", "two" };
    mySoManyStrings.fixedStringBuff = new string[3] { "uno", "dos", "tres" };
    mySoManyStrings.anotherVarStringBuff = new string[4] { "yi", "ar", "san", "si" };
    mySoManyStrings.anotherFixedStringBuff = new string[2] { "un", "deux" };

    msg.mySoManyStrings = mySoManyStrings;
    System.Console.WriteLine("msg = " + msg.Size + " bytes");
    stream = new System.IO.MemoryStream();
    msg.Pack(stream);
    stream.Seek(0, System.IO.SeekOrigin.Begin);
    otherMsg.Unpack(stream);

    if (!msg.Equals(otherMsg)) {
      return false;
    }

    return true;
  }

  public static bool Test_UnionOfUnion() {
    Foo myFoo = new Foo();

    myFoo.myByte = 0x7f;
    myFoo.byteTwo = 0xfe;
    myFoo.myShort = 0x0afe;
    myFoo.myFloat = 123.123123e12f;
    myFoo.myNormal = 0x0eadbeef;
    myFoo.myFoo = AnkiTypes.AnkiEnum.myReallySilly_EnumVal;
    myFoo.myString = "Whatever";

    FooBarUnion myFooBarUnion = new FooBarUnion();
    myFooBarUnion.myFoo = myFoo;
    UnionOfUnion myUnionOfUnion = new UnionOfUnion();
    myUnionOfUnion.myFooBar = myFooBarUnion;
    System.Console.WriteLine("\nStruct = " + myFoo.Size + " bytes");
    System.Console.WriteLine("Union = " + myFooBarUnion.Size + " bytes");
    System.Console.WriteLine("UnionOfUnion = " + myUnionOfUnion.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    myUnionOfUnion.Pack(stream);

    stream.Seek(0, System.IO.SeekOrigin.Begin);
    UnionOfUnion myOtherUnionOfUnion = new UnionOfUnion(stream);
    if (!myOtherUnionOfUnion.Equals(myUnionOfUnion)) {
      return false;
    }

    return true;
  }

  public static bool Test_MessageOfUnion() {
    Foo myFoo = new Foo();

    myFoo.myByte = 0x7f;
    myFoo.byteTwo = 0xfe;
    myFoo.myShort = 0x0afe;
    myFoo.myFloat = 123.123123e12f;
    myFoo.myNormal = 0x0eadbeef;
    myFoo.myFoo = AnkiTypes.AnkiEnum.myReallySilly_EnumVal;
    myFoo.myString = "Whatever";

    FooBarUnion myFooBarUnion = new FooBarUnion();
    myFooBarUnion.myFoo = myFoo;
    MessageOfUnion myMessageOfUnion = new MessageOfUnion();
    myMessageOfUnion.anInt = 11;
    myMessageOfUnion.myFooBar = myFooBarUnion;
    myMessageOfUnion.aBool = true;


    System.Console.WriteLine("\nStruct = " + myFoo.Size + " bytes");
    System.Console.WriteLine("Union = " + myFooBarUnion.Size + " bytes");
    System.Console.WriteLine("MessageOfUnion = " + myMessageOfUnion.Size + " bytes");
    System.IO.MemoryStream stream = new System.IO.MemoryStream();
    myMessageOfUnion.Pack(stream);

    stream.Seek(0, System.IO.SeekOrigin.Begin);
    MessageOfUnion myOtherMessageOfUnion = new MessageOfUnion(stream);
    if (!myOtherMessageOfUnion.Equals(myMessageOfUnion)) {
      return false;
    }

    return true;
  }

  public static bool Test_UnionInitialize() {
    Foo myFoo = new Foo();

    myFoo.myByte = 0x7f;
    myFoo.byteTwo = 0xfe;
    myFoo.myShort = 0x0afe;
    myFoo.myFloat = 123.123123e12f;
    myFoo.myNormal = 0x0eadbeef;
    myFoo.myFoo = AnkiTypes.AnkiEnum.myReallySilly_EnumVal;
    myFoo.myString = "Whatever";

    FooBarUnion myFooBarUnion = new FooBarUnion();

    myFooBarUnion.Initialize(myFoo);

    if (myFooBarUnion.myFoo != myFoo) {
      return false;
    }

    return true;
  }

  public static bool Test_MessageOfUnionInitialize() {
    Foo myFoo = new Foo();

    myFoo.myByte = 0x7f;
    myFoo.byteTwo = 0xfe;
    myFoo.myShort = 0x0afe;
    myFoo.myFloat = 123.123123e12f;
    myFoo.myNormal = 0x0eadbeef;
    myFoo.myFoo = AnkiTypes.AnkiEnum.myReallySilly_EnumVal;
    myFoo.myString = "Whatever";

    MessageOfUnion myMessageOfUnion = new MessageOfUnion();
    myMessageOfUnion.Initialize(11, myFoo, true);

    if (myMessageOfUnion.myFooBar.myFoo != myFoo) {
      return false;
    }

    return true;
  }

  public static bool Test_DefaultValuesInt() {
    // This will break if the default values specified in DefaultValues.clad change
    IntsWithDefaultValue firstData = new IntsWithDefaultValue();
    if (firstData.a != 42) return false;
    if (firstData.b != 0xff) return false;
    if (firstData.c != -2) return false;
    if (firstData.d != true) return false;

    // Ensure we can still fully specify the data
    IntsWithDefaultValue otherData = new IntsWithDefaultValue(1, 1, 1, false);
    if (otherData.a != 1) return false;
    if (otherData.b != 1) return false;
    if (otherData.c != 1) return false;
    if (otherData.d != false) return false;

    // Ensure we can still partially specify the data
    IntsWithDefaultValue lastData = new IntsWithDefaultValue();
    lastData.c = -10;
    lastData.d = false;
    if (lastData.a != 42) return false;
    if (lastData.b != 0xff) return false;
    if (lastData.c != -10) return false;
    if (lastData.d != false) return false;

    return true;
  }

  public static bool Test_DefaultValuesFloat() {
    // This will break if the default values specified in DefaultValues.clad change
    FloatsWithDefaultValue firstData = new FloatsWithDefaultValue();
    if (firstData.a != 0.42f) return false;
    if (firstData.b != 12.0f) return false;
    if (firstData.c != 10.0101) return false;
    if (firstData.d != -2.0f) return false;

    // Ensure we can still fully specify the data
    FloatsWithDefaultValue otherData = new FloatsWithDefaultValue(1.0f, 1.0f, 1.0, 1.0f);
    if (otherData.a != 1.0f) return false;
    if (otherData.b != 1.0f) return false;
    if (otherData.c != 1.0) return false;
    if (otherData.d != 1.0f) return false;

    // Ensure we can still partially specify the data
    FloatsWithDefaultValue lastData = new FloatsWithDefaultValue();
    lastData.c = -10;
    lastData.d = 0.0f;
    if (lastData.a != 0.42f) return false;
    if (lastData.b != 12.0f) return false;
    if (lastData.c != -10) return false;
    if (lastData.d != 0.0f) return false;

    return true;
  }

  public static bool Test_Enum_Complex() {
    // For some reason doing "if ((uint)FooEnum.foo1 != 0) return false;" 
    // resulted in unreachable code errrors so I had to do this
    uint i = (uint)FooEnum.foo1;
    if (i != 0) return false;

    i = (uint)FooEnum.foo2;
    if (i != 8) return false;

    i = (uint)FooEnum.foo3;
    if (i != 9) return false;

    i = (uint)FooEnum.foo4;
    if (i != 10) return false;

    i = (uint)FooEnum.foo5;
    if (i != 1280) return false;

    i = (uint)FooEnum.foo6;
    if (i != 1281) return false;

    i = (uint)FooEnum.foo7;
    if (i != 1000) return false;

    i = (uint)BarEnum.bar1;
    if (i != 0) return false;

    i = (uint)BarEnum.bar2;
    if (i != 8) return false;

    i = (uint)BarEnum.bar3;
    if (i != 9) return false;

    i = (uint)BarEnum.bar4;
    if (i != 1291) return false;

    i = (uint)BarEnum.bar5;
    if (i != 16) return false;

    i = (uint)BarEnum.bar6;
    if (i != 17) return false;

    return true;
  }

  public static bool Test_DefaultConstructor() {
    // Test that HasDefaultConstructor has a constructor that takes no types (the empty type?)
    Constructor.HasDefaultConstructor defaultConstructor = new Constructor.HasDefaultConstructor(4.2f, 3);
    System.Type type1 = defaultConstructor.GetType();
    if (type1.GetConstructor(System.Type.EmptyTypes) == null) return false;

    // Test that HasNoDefaultConstructor does NOT have a constructor that takes no types
    Constructor.HasNoDefaultConstructor noDefaultConstructor = new Constructor.HasNoDefaultConstructor(4.2f, 3);
    System.Type type2 = noDefaultConstructor.GetType();
    if (type2.GetConstructor(System.Type.EmptyTypes) != null) return false;

    Constructor.NoDefaultConstructorComplex noDefaultConstructorComplex = new Constructor.NoDefaultConstructorComplex(defaultConstructor, "wow", new byte[20]);
    System.Type type3 = noDefaultConstructorComplex.GetType();
    if (type3.GetConstructor(System.Type.EmptyTypes) != null) return false;

    Constructor.MessageWithStruct messageWithStruct = new Constructor.MessageWithStruct(noDefaultConstructor, 4, 3.5f, defaultConstructor);
    System.Type type4 = messageWithStruct.GetType();
    if (type4.GetConstructor(System.Type.EmptyTypes) != null) return false;

    Constructor.OtherMessageWithStruct otherMessageWithStruct = new Constructor.OtherMessageWithStruct(noDefaultConstructorComplex, defaultConstructor);
    System.Type type5 = otherMessageWithStruct.GetType();
    if (type5.GetConstructor(System.Type.EmptyTypes) != null) return false;

    Constructor.NestedNoDefaults nestedNoDefaults = new Constructor.NestedNoDefaults(noDefaultConstructorComplex, defaultConstructor, new byte[20], noDefaultConstructor, "bye");
    System.Type type6 = nestedNoDefaults.GetType();
    if (type6.GetConstructor(System.Type.EmptyTypes) != null) return false;

    Constructor.SuperComplex superComplex = new Constructor.SuperComplex(nestedNoDefaults, defaultConstructor, noDefaultConstructorComplex);
    System.Type type7 = superComplex.GetType();
    if (type7.GetConstructor(System.Type.EmptyTypes) != null) return false;

    Constructor.Nest1 nest1 = new Constructor.Nest1(noDefaultConstructor);
    System.Type type8 = nest1.GetType();
    if (type8.GetConstructor(System.Type.EmptyTypes) != null) return false;

    Constructor.Nest2 nest2 = new Constructor.Nest2(nest1);
    System.Type type9 = nest2.GetType();
    if (type9.GetConstructor(System.Type.EmptyTypes) != null) return false;

    Constructor.Nest3 nest3 = new Constructor.Nest3(nest2);
    System.Type type10 = nest3.GetType();
    if (type10.GetConstructor(System.Type.EmptyTypes) != null) return false;

    return true;
  }

  public static bool Test_FixedArray() {
    Arrays.s s = new Arrays.s();
    if (s.arr8.Length != (int)Arrays.ArrSize.sizeTen) return false;
    if (s.arr16.Length != (int)Arrays.ArrSize.sizeTwenty) return false;
    if (s.Size != ((int)Arrays.ArrSize.sizeTen * sizeof(byte) + (int)Arrays.ArrSize.sizeTwenty * sizeof(ushort))) return false;

    Arrays.m m = new Arrays.m();
    if (m.arr8.Length != (int)Arrays.ArrSize.sizeTen) return false;
    if (m.arr16.Length != (int)Arrays.ArrSize.sizeTwenty) return false;
    if (m.Size != ((int)Arrays.ArrSize.sizeTen * sizeof(byte) + (int)Arrays.ArrSize.sizeTwenty * sizeof(ushort))) return false;

    return true;
  }

  public static bool Test_EnumConcept() {

    if (EnumConcept.IsFooOdd(FooEnum.foo1, false) == false) return false;
    if (EnumConcept.IsFooOdd(FooEnum.foo2, false) == true) return false;
    if (EnumConcept.IsFooOdd(FooEnum.foo3, false) == false) return false;
    if (EnumConcept.IsFooOdd(FooEnum.foo4, false) == true) return false;
    if (EnumConcept.IsFooOdd(FooEnum.foo5, false) == false) return false;
    if (EnumConcept.IsFooOdd(FooEnum.foo6, false) == true) return false;
    if (EnumConcept.IsFooOdd(FooEnum.foo7, false) == false) return false;

    if (EnumConcept.IsFoo4(FooEnum.foo1, false) == true) return false;
    if (EnumConcept.IsFoo4(FooEnum.foo2, false) == true) return false;
    if (EnumConcept.IsFoo4(FooEnum.foo3, false) == true) return false;
    if (EnumConcept.IsFoo4(FooEnum.foo4, false) == false) return false;
    if (EnumConcept.IsFoo4(FooEnum.foo5, false) == true) return false;
    if (EnumConcept.IsFoo4(FooEnum.foo6, false) == true) return false;
    if (EnumConcept.IsFoo4(FooEnum.foo7, false) == true) return false;

    if (EnumConcept.BarEnumToFooEnum(BarEnum.bar1, FooEnum.foo7) != FooEnum.foo1) return false;
    if (EnumConcept.BarEnumToFooEnum(BarEnum.bar2, FooEnum.foo7) != FooEnum.foo2) return false;
    if (EnumConcept.BarEnumToFooEnum(BarEnum.bar3, FooEnum.foo7) != FooEnum.foo3) return false;
    if (EnumConcept.BarEnumToFooEnum(BarEnum.bar4, FooEnum.foo7) != FooEnum.foo4) return false;
    if (EnumConcept.BarEnumToFooEnum(BarEnum.bar5, FooEnum.foo7) != FooEnum.foo5) return false;
    if (EnumConcept.BarEnumToFooEnum(BarEnum.bar6, FooEnum.foo7) != FooEnum.foo6) return false;

    if (EnumConcept.BarToString(BarEnum.bar1, "") != "bar1") return false;
    if (EnumConcept.BarToString(BarEnum.bar2, "") != "bar2") return false;
    if (EnumConcept.BarToString(BarEnum.bar3, "") != "bar3") return false;
    if (EnumConcept.BarToString(BarEnum.bar4, "") != "bar4") return false;
    if (EnumConcept.BarToString(BarEnum.bar5, "") != "bar5") return false;
    if (EnumConcept.BarToString(BarEnum.bar6, "") != "bar6") return false;

    if (EnumConcept.DoubleFoo(FooEnum.foo1, 0) != ((uint)FooEnum.foo1 * 2)) return false;
    if (EnumConcept.DoubleFoo(FooEnum.foo2, 0) != ((uint)FooEnum.foo2 * 2)) return false;
    if (EnumConcept.DoubleFoo(FooEnum.foo3, 0) != ((uint)FooEnum.foo3 * 2)) return false;
    if (EnumConcept.DoubleFoo(FooEnum.foo4, 0) != ((uint)FooEnum.foo4 * 2)) return false;
    if (EnumConcept.DoubleFoo(FooEnum.foo5, 0) != ((uint)FooEnum.foo5 * 2)) return false;
    if (EnumConcept.DoubleFoo(FooEnum.foo6, 0) != ((uint)FooEnum.foo6 * 2)) return false;
    if (EnumConcept.DoubleFoo(FooEnum.foo7, 0) != ((uint)FooEnum.foo7 * 2)) return false;

    if (EnumConcept.IsValid(FooEnum.foo1, false) == false) return false;
    if (EnumConcept.IsValid(FooEnum.foo2, false) == true) return false;
    if (EnumConcept.IsValid(BarEnum.bar1, false) == false) return false;
    if (EnumConcept.IsValid(BarEnum.bar2, false) == true) return false;

    if (EnumConcept.IsValid((FooEnum)(100000000), true) == false) return false;

    return true;
  }

  public static void Main() {
    System.Console.Write("Test_Foo: ");
    System.Console.WriteLine(Test_Foo() ? "PASS" : "FAIL");

    System.Console.Write("Test_Bar: ");
    System.Console.WriteLine(Test_Bar() ? "PASS" : "FAIL");

    System.Console.Write("Test_BarInitialize: ");
    System.Console.WriteLine(Test_BarInitialize() ? "PASS" : "FAIL");

    System.Console.Write("Test_Dog: ");
    System.Console.WriteLine(Test_Dog() ? "PASS" : "FAIL");
      
    System.Console.Write("Test_Poodle: ");
    System.Console.WriteLine(Test_Poodle() ? "PASS" : "FAIL");

    System.Console.Write("Test_od432: ");
    System.Console.WriteLine(Test_od432() ? "PASS" : "FAIL");

    System.Console.Write("Test_od433: ");
    System.Console.WriteLine(Test_od433() ? "PASS" : "FAIL");

    System.Console.Write("Test_SoManyStrings: ");
    System.Console.WriteLine(Test_SoManyStrings() ? "PASS" : "FAIL");

    System.Console.Write("Test_od434: ");
    System.Console.WriteLine(Test_od434() ? "PASS" : "FAIL");

    System.Console.Write("Test_Union: ");
    System.Console.WriteLine(Test_Union() ? "PASS" : "FAIL");

    System.Console.Write("Test_UnionOfUnion: ");
    System.Console.WriteLine(Test_UnionOfUnion() ? "PASS" : "FAIL");

    System.Console.Write("Test_MessageOfUnion: ");
    System.Console.WriteLine(Test_MessageOfUnion() ? "PASS" : "FAIL");

    System.Console.Write("Test_UnionInitialize: ");
    System.Console.WriteLine(Test_UnionInitialize() ? "PASS" : "FAIL");

    System.Console.Write("Test_MessageOfUnionInitialize: ");
    System.Console.WriteLine(Test_MessageOfUnionInitialize() ? "PASS" : "FAIL");

    System.Console.Write("Test_DefaultValuesInt: ");
    System.Console.WriteLine(Test_DefaultValuesInt() ? "PASS" : "FAIL");

    System.Console.Write("Test_DefaultValuesFloat: ");
    System.Console.WriteLine(Test_DefaultValuesFloat() ? "PASS" : "FAIL");

    System.Console.Write("Test_Enum_Complex: ");
    System.Console.WriteLine(Test_Enum_Complex() ? "PASS" : "FAIL");

    System.Console.Write("Test_DefaultConstructor: ");
    System.Console.WriteLine(Test_DefaultConstructor() ? "PASS" : "FAIL");

    System.Console.Write("Test_FixedArray: ");
    System.Console.WriteLine(Test_FixedArray() ? "PASS" : "FAIL");

    System.Console.Write("Test_EnumConcept: ");
    System.Console.WriteLine(Test_EnumConcept() ? "PASS" : "FAIL");
  }
}
