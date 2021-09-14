#!/usr/bin/env python2
#
# Copyright 2015-2016 Anki Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
#  CLAD Unit test:
#  Python emitter unit test
#

from __future__ import absolute_import
from __future__ import print_function

import math
import sys

import unittest

from SimpleTest import AnkiTypes, Foo, Bar, Baz, Cat, SoManyStrings, Constructor, Arrays
from SimpleTest import ExplicitlyTaggedUnion, AnInt, AFloat, AListOfDoubles, AFixedListOfBytes
from SimpleTest import ExplicitlyTaggedAutoUnion, AnIntMessage, AFloatMessage, AListOfDoublesMessage, AFixedListOfBytesMessage, ABoolMessage
from aligned.AutoUnionTest import FunkyMessage, Funky, Monkey, Music
from DefaultValues import IntsWithDefaultValue, FloatsWithDefaultValue
from SimpleTest import FooEnum, BarEnum
from SimpleTest import BarEnumToFooEnum, BarToString, DoubleFoo, IsFoo4, IsFooOdd, IsValid

class TestSimpleMessage(unittest.TestCase):

    def test_EmptyMessageConstruction(self):
        foo1a = Foo()
        foo1b = Foo()
        self.assertEqual(foo1a, foo1b, "Empty Message construction is deterministic")

    def test_EmptyMessageRoundTrip(self):
        foo2a = Foo()
        foo2b = Foo.unpack(foo2a.pack())
        self.assertEqual(foo2a, foo2b, "Empty Message round-trip via constructor is broken")

    def test_PopulatedMessageRoundTrip(self):
        foo3a = Foo(isFoo=True, myByte=-5, byteTwo=203, myFloat=13231321,
                    myShort=32000, myNormal=0xFFFFFFFF,
                    myFoo=AnkiTypes.AnkiEnum.e3, myString=('laadlsjk' * 10))
        foo3b = Foo.unpack(foo3a.pack())
        self.assertEqual(foo3a, foo3b, "Popluated Message round-trip via constructor is broken")

    def test_PopulatedMessageAssignment(self):
        foo4a = Foo()
        foo4a.isFoo=True
        foo4a.myByte=31
        foo4a.byteTwo=128
        foo4a.myShort=0
        foo4a.myFloat=1323.12
        foo4a.myNormal=65536
        foo4a.myFoo=AnkiTypes.AnkiEnum.e1
        foo4a.myString=''

        foo4b = Foo.unpack(foo4a.pack())
        self.assertEqual(foo4a, foo4b, "Populated Message assignment is broken")

class TestMessageOfArrays(unittest.TestCase):

    def test_EmptyMessageConstruction(self):
        bar1a = Bar()
        bar1b = Bar()
        self.assertEqual(bar1a, bar1b, "Empty Message construction is deterministic")

    def test_EmptyMessageRoundTrip(self):
        bar2a = Bar()
        bar2b = Bar.unpack(bar2a.pack())
        self.assertEqual(bar2a, bar2b, "Empty Message round-trip via constructor is broken")

    def test_PopulatedMessageRoundTrip(self):
        bar3a = Bar(
            (True, True, True),    #boolBuff
            [-128, 127, 0, 1, -1], #byteBuff
            (-23201, 23201, 102),  #shortBuff
            [AnkiTypes.AnkiEnum.d1], #enumBuff
            [sys.float_info.epsilon, 0, -0, 1, -1, sys.float_info.max, sys.float_info.min,
             sys.float_info.radix, float('inf'), float('-inf')], #doubleBuff
            'long' * 256, #myLongerString
            (i ^ 0x1321 for i in range(20)), #fixedBuff
            (False for i in range(10)), #fixedBoolBuff
            (AnkiTypes.AnkiEnum.d1 for i in range(2)), #fixedEnumBuff
        )
        bar3b = Bar.unpack(bar3a.pack())
        self.assertEqual(bar3a, bar3b, "Populated Message round-trip via constructor is broken")

    def test_PopulatedMessageAssignment(self):
        bar4a = Bar()
        bar4a.boolBuff = (i % 3 == 0 for i in range(200))
        bar4a.byteBuff = ()
        bar4a.shortBuff = tuple(range(255))
        bar4a.enumBuff = [AnkiTypes.AnkiEnum.e3 for i in range(10)]
        bar4a.doubleBuff = ()
        bar4a.myLongerString = ''.join(str(i) for i in range(10))
        bar4a.fixedBuff = [-1] * 20
        bar4a.fixedBoolBuff = (i % 3 == 0 for i in range(10))

        bar4b = Bar.unpack(bar4a.pack())
        self.assertEqual(bar4a, bar4b, "Populated Message assignment is broken")

class TestMessageOfStrings(unittest.TestCase):
    def test_EmptyMessageConstruction(self):
        somany1a = SoManyStrings()
        somany1b = SoManyStrings()
        self.assertEqual(somany1a, somany1b, "Empty Message construction is broken")

    def test_EmptyMessageRoundTrip(self):
        somany2a = SoManyStrings()
        somany2b = SoManyStrings.unpack(somany2a.pack())
        self.assertEqual(somany2a, somany2b, "Empty Message round-trip via constructor is broken")

    def test_PopulatedMessageRoundTrip(self):
        somany3a = SoManyStrings(
            (hex(x*x*x^3423) for x in range(2000)),
            ('na' * i for i in range(3)),
            ('ads', 'fhg', 'jlk'),
            ('Super', 'Troopers'))
        somany3b = SoManyStrings.unpack(somany3a.pack())
        self.assertEqual(somany3a, somany3b,
                         "Populated Message round-trip via constructor is broken")

    def test_PopulatedMessageAssignment(self):
        somany4a = SoManyStrings()
        somany4a.varStringBuff = (chr(32 + i) for i in range(80))
        somany4a.fixedStringBuff = 'abc'
        somany4a.anotherVarStringBuff = [u'\u1233\u1231 foo', 'asdas', '\xC2\xA2']
        somany4a.anotherFixedStringBuff = ['', '\0']

        somany4b = SoManyStrings.unpack(somany4a.pack())
        self.assertEqual(somany4a, somany4b,
                         "Populated Message assignment is broken")

class TestUnion(unittest.TestCase):
    def test_EmptyUnionConstruction(self):
        msg1a = Cat.MyMessage()
        msg1b = Cat.MyMessage()
        self.assertEqual(msg1a, msg1b, "Empty union construction is broken")

    def test_EmptyUnionRoundTrip(self):
        msg2a = Cat.MyMessage(myFoo=Foo()) # can't pack untagged
        msg2b = Cat.MyMessage.unpack(msg2a.pack())
        self.assertEqual(msg2a, msg2b, "Empty union roundtrip is broken")

    def test_PopulatedUnionRoundTrip(self):
        msg3a = Cat.MyMessage(myFoo=Foo(
            False,
            -128,
            127,
            -23201,
            float('inf'),
            102030,
            0,
            'z' * 255))
        msg3b = Cat.MyMessage.unpack(msg3a.pack())
        self.assertEqual(msg3a, msg3b, "Populated union round-trip is broken")

    def test_PopulatedUnionAssignment(self):
        msg4a = Cat.MyMessage()
        msg4a.myBar = Bar()
        msg4a.myBar.boolBuff = (True, False)
        msg4a.myBar.byteBuff = ()
        msg4a.myBar.shortBuff = (i ^ 0x1321 for i in range(20))
        msg4a.myBar.enumBuff = [AnkiTypes.AnkiEnum.d3 for i in range(10)]
        msg4a.myBar.doubleBuff = [math.sqrt(x**3) for x in range(100)]
        msg4a.myBar.myLongerString = ''.join(str(i) for i in range(100))
        msg4a.myBar.fixedBuff = [sum(range(i)) for i in range(20)]
        msg4a.myBar.fixedBoolBuff = (True,) * 10

        msg4b = Cat.MyMessage.unpack(msg4a.pack())
        self.assertEqual(msg4a, msg4b, "Populated union assignment is broken")

    def test_explicitUnionValues(self):
        testUnion = ExplicitlyTaggedUnion()
        testUnion.anInt = AnInt(42)
        self.assertEqual(0x01, testUnion.tag)
        testUnion.aFloat = AFloat(10.0)
        self.assertEqual(0x02, testUnion.tag)
        testUnion.dList = AListOfDoubles([100.1, 200.2])
        self.assertEqual(0x80, testUnion.tag)
        testUnion.bList = AFixedListOfBytes( (0x00, 0x01, 0x02, 0x03) )
        self.assertEqual(0x81, testUnion.tag)

    def verify_autounion_tag(self, testUnion, autoTags):
        self.assertIn(testUnion.tag, autoTags)
        if testUnion.tag in autoTags:
            autoTags.remove(testUnion.tag)
            
    def test_explicitAutoUnionValues(self):
        autoTags = [3,4,5]
        testUnion = ExplicitlyTaggedAutoUnion()
        testUnion.AnIntMessage = AnIntMessage(42)
        self.assertEqual(0x01, testUnion.tag)
        testUnion.AFloatMessage = AFloatMessage(10.0)
        self.verify_autounion_tag(testUnion, autoTags)
        testUnion.ABoolMessage = ABoolMessage(False)
        self.verify_autounion_tag(testUnion, autoTags)
        testUnion.AListOfDoublesMessage = AListOfDoublesMessage([100.1, 200.2])
        self.verify_autounion_tag(testUnion, autoTags)
        testUnion.AFixedListOfBytesMessage = AFixedListOfBytesMessage( (0x00, 0x01, 0x02, 0x03) )
        self.assertEqual(0x02, testUnion.tag)
        # Every autotag should have been used once and once only, so list should now be empty
        self.assertEqual(autoTags, [])
        
    def test_introspection(self):
        kind = 'myDog'
        self.assertTrue(hasattr(Cat.MyMessage, kind))
        tag = getattr(Cat.MyMessage.Tag, kind)
        msgType = Cat.MyMessage.typeByTag(tag)
        self.assertIsInstance(msgType(), Baz.Dog)
        subMessage = msgType()
        constructedMessage = Cat.MyMessage(**{kind: subMessage})
        self.assertEqual(constructedMessage.tag, tag)
        self.assertEqual(getattr(constructedMessage, constructedMessage.tag_name), subMessage)


class TestAutoUnion(unittest.TestCase):
    def test_Creation(self):
        # this test totally sucks, it needs to assert stuff.
        msg = FunkyMessage()
        funky = Funky(AnkiTypes.AnkiEnum.e1, 3)
        aMonkey = Monkey(1331232132, funky)
        msg.Monkey = aMonkey
        music = Music((131,), funky)
        msg.Music = music

class TestDefaultValues(unittest.TestCase):
    def test_defaultValueInts(self):
      # This will break if the default values specified in DefaultValues.clad change
      firstData = IntsWithDefaultValue()
      self.assertEqual(firstData.a, 42)
      self.assertEqual(firstData.b, 0xff)
      self.assertEqual(firstData.c, -2)
      self.assertEqual(firstData.d, True)

      # Ensure we can still fully specify the data
      otherData = IntsWithDefaultValue(1, 1, 1, False)
      self.assertEqual(otherData.a, 1)
      self.assertEqual(otherData.b, 1)
      self.assertEqual(otherData.c, 1)
      self.assertEqual(otherData.d, False)

      # Ensure we can still partially specify the data
      lastData = IntsWithDefaultValue()
      lastData.c = -10
      lastData.d = False
      self.assertEqual(lastData.a, 42)
      self.assertEqual(lastData.b, 0xff)
      self.assertEqual(lastData.c, -10, repr(lastData))
      self.assertEqual(lastData.d, False)

    def test_defaultValuesFloats(self):
      #This will break if the default values specified in DefaultValues.clad change
      firstData = FloatsWithDefaultValue()
      self.assertAlmostEqual(firstData.a, 0.42)
      self.assertAlmostEqual(firstData.b, 12.0)
      self.assertAlmostEqual(firstData.c, 10.0101)
      self.assertAlmostEqual(firstData.d, -2.0)

      # Ensure we can still fully specify the data
      otherData = FloatsWithDefaultValue(1.0, 1.0, 1.0, 1.0)
      self.assertAlmostEqual(otherData.a, 1.0)
      self.assertAlmostEqual(otherData.b, 1.0)
      self.assertAlmostEqual(otherData.c, 1.0)
      self.assertAlmostEqual(otherData.d, 1.0)

      # Ensure we can still partially specify the data
      lastData = FloatsWithDefaultValue()
      lastData.c = -10
      lastData.d = False
      self.assertAlmostEqual(lastData.a, 0.42)
      self.assertAlmostEqual(lastData.b, 12.0)
      self.assertAlmostEqual(lastData.c, -10)
      self.assertAlmostEqual(lastData.d, False)

    def test_defaultValuesVerbatimEnum(self):
      myPoodle = Baz.Poodle()
      self.assertEqual(myPoodle.a, AnkiTypes.AnkiEnum.d1)

class TestEnumComplex(unittest.TestCase):
    def test_enumComplex(self):
      self.assertEqual(FooEnum.foo1, 0)
      self.assertEqual(FooEnum.foo2, 8)
      self.assertEqual(FooEnum.foo3, 9)
      self.assertEqual(FooEnum.foo4, 10)
      self.assertEqual(FooEnum.foo5, 1280)
      self.assertEqual(FooEnum.foo6, 1281)
      self.assertEqual(FooEnum.foo7, 1000)

      self.assertEqual(BarEnum.bar1, 0)
      self.assertEqual(BarEnum.bar2, 8)
      self.assertEqual(BarEnum.bar3, 9)
      self.assertEqual(BarEnum.bar4, 1291)
      self.assertEqual(BarEnum.bar5, 16)
      self.assertEqual(BarEnum.bar6, 17)

class TestDefaultConstructor(unittest.TestCase):
    def test_defaultConstructor(self):
      self.assertEqual(Constructor.HasDefaultConstructor.__init__.__defaults__, (0.0,0))
      self.assertEqual(Constructor.HasNoDefaultConstructor.__init__.__defaults__, None)
      self.assertEqual(Constructor.NoDefaultConstructorComplex.__init__.__defaults__, None)
      self.assertEqual(Constructor.MessageWithStruct.__init__.__defaults__, None)
      self.assertEqual(Constructor.OtherMessageWithStruct.__init__.__defaults__, None)
      self.assertEqual(Constructor.NestedNoDefaults.__init__.__defaults__, None)
      self.assertEqual(Constructor.SuperComplex.__init__.__defaults__, None)
      self.assertEqual(Constructor.Nest1.__init__.__defaults__, None)
      self.assertEqual(Constructor.Nest2.__init__.__defaults__, None)
      self.assertEqual(Constructor.Nest3.__init__.__defaults__, None)

class TestFixedArray(unittest.TestCase):
    def test_fixedArray(self):
      s = Arrays.s()
      self.assertEqual(len(s.arr8) == Arrays.ArrSize.sizeTen, True)
      self.assertEqual(len(s.arr16) == Arrays.ArrSize.sizeTwenty, True)
      self.assertEqual(len(s) == Arrays.ArrSize.sizeTen*1 + Arrays.ArrSize.sizeTwenty*2, True)

      m = Arrays.m()
      self.assertEqual(len(m.arr8) == Arrays.ArrSize.sizeTen, True)
      self.assertEqual(len(m.arr16) == Arrays.ArrSize.sizeTwenty, True)
      self.assertEqual(len(m) == Arrays.ArrSize.sizeTen*1 + Arrays.ArrSize.sizeTwenty*2, True)

class TestEnumConcept(unittest.TestCase):
    def test_enumConcept(self):
      self.assertEqual(IsFooOdd(FooEnum.foo1, False), True)
      self.assertEqual(IsFooOdd(FooEnum.foo2, False), False)
      self.assertEqual(IsFooOdd(FooEnum.foo3, False), True)
      self.assertEqual(IsFooOdd(FooEnum.foo4, False), False)
      self.assertEqual(IsFooOdd(FooEnum.foo5, False), True)
      self.assertEqual(IsFooOdd(FooEnum.foo6, False), False)
      self.assertEqual(IsFooOdd(FooEnum.foo7, False), True)

      self.assertEqual(IsFoo4(FooEnum.foo1, False), False)
      self.assertEqual(IsFoo4(FooEnum.foo2, False), False)
      self.assertEqual(IsFoo4(FooEnum.foo3, False), False)
      self.assertEqual(IsFoo4(FooEnum.foo4, False), True)
      self.assertEqual(IsFoo4(FooEnum.foo5, False), False)
      self.assertEqual(IsFoo4(FooEnum.foo6, False), False)
      self.assertEqual(IsFoo4(FooEnum.foo7, False), False)

      self.assertEqual(BarEnumToFooEnum(BarEnum.bar1, FooEnum.foo7), FooEnum.foo1)
      self.assertEqual(BarEnumToFooEnum(BarEnum.bar2, FooEnum.foo7), FooEnum.foo2)
      self.assertEqual(BarEnumToFooEnum(BarEnum.bar3, FooEnum.foo7), FooEnum.foo3)
      self.assertEqual(BarEnumToFooEnum(BarEnum.bar4, FooEnum.foo7), FooEnum.foo4)
      self.assertEqual(BarEnumToFooEnum(BarEnum.bar5, FooEnum.foo7), FooEnum.foo5)
      self.assertEqual(BarEnumToFooEnum(BarEnum.bar6, FooEnum.foo7), FooEnum.foo6)

      self.assertEqual(BarToString(BarEnum.bar1, ""), "bar1")
      self.assertEqual(BarToString(BarEnum.bar2, ""), "bar2")
      self.assertEqual(BarToString(BarEnum.bar3, ""), "bar3")
      self.assertEqual(BarToString(BarEnum.bar4, ""), "bar4")
      self.assertEqual(BarToString(BarEnum.bar5, ""), "bar5")
      self.assertEqual(BarToString(BarEnum.bar6, ""), "bar6")

      self.assertEqual(DoubleFoo(FooEnum.foo1, 0), FooEnum.foo1 * 2)
      self.assertEqual(DoubleFoo(FooEnum.foo2, 0), FooEnum.foo2 * 2)
      self.assertEqual(DoubleFoo(FooEnum.foo3, 0), FooEnum.foo3 * 2)
      self.assertEqual(DoubleFoo(FooEnum.foo4, 0), FooEnum.foo4 * 2)
      self.assertEqual(DoubleFoo(FooEnum.foo5, 0), FooEnum.foo5 * 2)
      self.assertEqual(DoubleFoo(FooEnum.foo6, 0), FooEnum.foo6 * 2)
      self.assertEqual(DoubleFoo(FooEnum.foo7, 0), FooEnum.foo7 * 2)

      self.assertEqual(IsValid(FooEnum.foo1, False), True)
      self.assertEqual(IsValid(FooEnum.foo2, False), False)
      self.assertEqual(IsValid(BarEnum.bar1, False), True)
      self.assertEqual(IsValid(BarEnum.bar2, False), False)

      self.assertEqual(IsValid(100000000, True), True)

# Required unittest.main
if __name__ == '__main__':
    unittest.main()
