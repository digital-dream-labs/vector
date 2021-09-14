package test

// these tests were copied from the C# emitter and Go-ified

import (
	"bytes"
	"math"
	"reflect"

	"build"
	"testing"
)

func printCompareError(t *testing.T, name1, name2 string, var1, var2 interface{}) {
	t.Error(name1, "doesn't equal", name2, "after unpacking")
	t.Error(name1, "is:", var1)
	t.Error(name2, "is:", var2)
}

func TestFoo(t *testing.T) {
	myFoo := build.Foo{true, 0x7f, 0xfe, 0xafe, -123123,
		0xdeadbeef, build.AnkiEnum_myReallySilly_EnumVal,
		"Blah Blah Blah"}
	otherFoo := build.Foo{}
	t.Log("myFoo =", myFoo.Size(), "bytes")

	var buf bytes.Buffer
	if err := myFoo.Pack(&buf); err != nil {
		t.Fatal("error packing myFoo:", err)
	}
	if err := otherFoo.Unpack(&buf); err != nil {
		t.Fatal("error unpacking otherFoo:", err)
	}

	if myFoo != otherFoo {
		printCompareError(t, "myFoo", "otherFoo", myFoo, otherFoo)
	}
}

func TestBar(t *testing.T) {
	myBar := build.Bar{[]bool{true, false, false},
		[]int8{0, 1, 2, 3, 4},
		[]int16{5, 6, 7},
		[]build.AnkiEnum{build.AnkiEnum_d1, build.AnkiEnum_e1},
		[]float64{math.MaxFloat64, math.Inf(1),
			-1 * math.MaxFloat64, -0, +0, math.Inf(-1), 1, -1},
		"Foo Bar Baz",
		[20]int16{1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
			11, 12, 13, 14, 15, 16, 17, 18, 19, 20},
		[10]bool{false, false, false, false, false,
			true, true, true, true, true},
		[2]build.AnkiEnum{build.AnkiEnum_e1, build.AnkiEnum_e3}}

	t.Log("myBar =", myBar.Size(), "bytes")

	var buf bytes.Buffer
	if err := myBar.Pack(&buf); err != nil {
		t.Fatal("error packing myBar:", err)
	}

	var otherBar build.Bar
	if err := otherBar.Unpack(&buf); err != nil {
		t.Fatal("error unpacking otherBar:", err)
	}

	if !reflect.DeepEqual(myBar, otherBar) {
		printCompareError(t, "myBar", "otherBar", myBar, otherBar)
	}
}

func TestDog(t *testing.T) {
	myDog := build.Dog{build.AnkiEnum_e1, 5}
	var otherDog build.Dog

	t.Log("my =", myDog.Size(), "bytes")

	var buf bytes.Buffer
	if err := myDog.Pack(&buf); err != nil {
		t.Fatal("error packing myDog:", err)
	}
	if err := otherDog.Unpack(&buf); err != nil {
		t.Fatal("error unpacking otherDog:", err)
	}

	if myDog != otherDog {
		printCompareError(t, "myDog", "otherDog", myDog, otherDog)
	}
}

func TestOd432(t *testing.T) {

	myFoo := build.Foo{true, 0x7f, 0xfe, 0xafe, -123123, 0xdeadbeef,
		build.AnkiEnum_myReallySilly_EnumVal,
		"Blah Blah Blah"}

	my := build.Od432{myFoo, 5, build.LEDColor_CurrentColor}
	t.Log("my =", my.Size(), "bytes")

	var buf bytes.Buffer
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	var other build.Od432
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if my != other {
		printCompareError(t, "my", "other", my, other)
	}
}

func TestOd433(t *testing.T) {
	fooA := build.Foo{true, 0x7f, 0xfe, 0xafe, -123123, 0xdeadbeef,
		build.AnkiEnum_myReallySilly_EnumVal,
		"Blah Blah Blah"}
	fooB := build.Foo{false, 0x1f, 0xfc, 0x123, -26874, 0xbeefdead,
		build.AnkiEnum_e3,
		"Yeah Yeah Yeah"}
	fooC := build.Foo{true, 0x3f, 0xae, 0xbfe, -143123, 0xdeedbeef,
		build.AnkiEnum_d1,
		"Doh Doh Doh"}
	fooD := build.Foo{true, 0xf, 0xfd, 0x143, -25874, 0xbeefdeed,
		build.AnkiEnum_e2,
		"Yo Yo Yo"}

	my := build.Od433{[]build.Foo{fooA, fooB}, [2]build.Foo{fooC, fooD}, 6}
	t.Log("my =", my.Size(), "bytes")

	var buf bytes.Buffer
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	var other build.Od433
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}
}

func TestSoManyStrings(t *testing.T) {
	my := build.SoManyStrings{[]string{"one, two"},
		[3]string{"uno, dos, tres"},
		[]string{"yi", "ar", "san", "si"},
		[2]string{"un", "deux"}}

	t.Log("my =", my.Size(), "bytes")

	var buf bytes.Buffer
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	var other build.SoManyStrings
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}
}

func TestOd434(t *testing.T) {
	my := build.Od434{[]uint64{1, 2, 3}}

	t.Log("my =", my.Size(), "bytes")

	var buf bytes.Buffer
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	var other build.Od434
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}
}

func TestUnion(t *testing.T) {
	var myFoo build.Foo
	myFoo.MyByte = 0x7f
	myFoo.ByteTwo = 0xfe
	myFoo.MyShort = 0x0afe
	myFoo.MyFloat = 123.123123e12
	myFoo.MyNormal = 0x0eadbeef
	myFoo.MyFoo = build.AnkiEnum_myReallySilly_EnumVal
	myFoo.MyString = "Whatever"

	var my build.MyMessage
	my.SetMyFoo(&myFoo)
	t.Log("my =", my.Size(), "bytes")

	var buf bytes.Buffer
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	var other build.MyMessage
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}

	var myBar build.Bar
	myBar.BoolBuff = []bool{true, false, true}
	myBar.ByteBuff = []int8{0x0f, 0x0e, 0x0c, 0x0a}
	myBar.ShortBuff = []int16{0x0fed, 0x0caf, 0x0a2f, 0x0a12}
	myBar.EnumBuff = []build.AnkiEnum{build.AnkiEnum_myReallySilly_EnumVal,
		build.AnkiEnum_e2}
	myBar.DoubleBuff = []float64{0.01}
	myBar.MyLongerString = "SomeLongerStupidString"
	myBar.FixedBuff = [20]int16{1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
		11, 12, 13, 14, 15, 16, 17, 18, 19, 20}
	myBar.FixedBoolBuff = [10]bool{false, false, false, false, false,
		true, true, true, true, true}

	my.SetMyBar(&myBar)
	t.Log("my =", my.Size(), "bytes")

	buf.Reset()
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}

	var myDog build.Dog
	myDog.A = build.AnkiEnum_e3
	myDog.B = 9

	my.SetMyDog(&myDog)
	t.Log("my =", my.Size(), "bytes")

	buf.Reset()
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}

	var myStrings build.SoManyStrings
	myStrings.VarStringBuff = []string{"one", "two"}
	myStrings.FixedStringBuff = [3]string{"uno", "dos", "tres"}
	myStrings.AnotherVarStringBuff = []string{"yi", "ar", "san", "si"}
	myStrings.AnotherFixedStringBuff = [2]string{"un", "deux"}

	my.SetMySoManyStrings(&myStrings)
	t.Log("my =", my.Size(), "bytes")

	buf.Reset()
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}
}

func TestUnionOfUnion(t *testing.T) {
	var myFoo build.Foo
	myFoo.MyByte = 0x7f
	myFoo.ByteTwo = 0xfe
	myFoo.MyShort = 0x0afe
	myFoo.MyFloat = 123.123123e12
	myFoo.MyNormal = 0x0eadbeef
	myFoo.MyFoo = build.AnkiEnum_myReallySilly_EnumVal
	myFoo.MyString = "Whatever"

	var myUnion build.FooBarUnion
	myUnion.SetMyFoo(&myFoo)

	var my build.UnionOfUnion
	my.SetMyFooBar(&myUnion)

	t.Log("foo struct =", myFoo.Size(), "bytes")
	t.Log("inner union =", myUnion.Size(), "bytes")
	t.Log("union of union =", my.Size(), "bytes")

	var buf bytes.Buffer
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	var other build.UnionOfUnion
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}
}

func TestMessageOfUnion(t *testing.T) {
	var myFoo build.Foo
	myFoo.MyByte = 0x7f
	myFoo.ByteTwo = 0xfe
	myFoo.MyShort = 0x0afe
	myFoo.MyFloat = 123.123123e12
	myFoo.MyNormal = 0x0eadbeef
	myFoo.MyFoo = build.AnkiEnum_myReallySilly_EnumVal
	myFoo.MyString = "Whatever"

	myUnion := build.NewFooBarUnionWithMyFoo(&myFoo)

	var my build.MessageOfUnion
	my.AnInt = 11
	my.MyFooBar = *myUnion
	my.ABool = true

	t.Log("foo struct =", myFoo.Size(), "bytes")
	t.Log("inner union =", myUnion.Size(), "bytes")
	t.Log("message of union =", my.Size(), "bytes")

	var buf bytes.Buffer
	if err := my.Pack(&buf); err != nil {
		t.Fatal("error packing my:", err)
	}
	var other build.MessageOfUnion
	if err := other.Unpack(&buf); err != nil {
		t.Fatal("error unpacking other:", err)
	}

	if !reflect.DeepEqual(my, other) {
		printCompareError(t, "my", "other", my, other)
	}
}

func TestEnumComplex(t *testing.T) {
	i := uint(build.FooEnum_foo1)
	if i != 0 {
		t.Fatal("unexpected value")
	}

	i = uint(build.FooEnum_foo2)
	if i != 8 {
		t.Fatal("unexpected value")
	}

	i = uint(build.FooEnum_foo3)
	if i != 9 {
		t.Fatal("unexpected value")
	}

	i = uint(build.FooEnum_foo4)
	if i != 10 {
		t.Fatal("unexpected value")
	}

	i = uint(build.FooEnum_foo5)
	if i != 1280 {
		t.Fatal("unexpected value")
	}

	i = uint(build.FooEnum_foo6)
	if i != 1281 {
		t.Fatal("unexpected value")
	}

	i = uint(build.FooEnum_foo7)
	if i != 1000 {
		t.Fatal("unexpected value")
	}

	i = uint(build.BarEnum_bar1)
	if i != 0 {
		t.Fatal("unexpected value")
	}

	i = uint(build.BarEnum_bar2)
	if i != 8 {
		t.Fatal("unexpected value")
	}

	i = uint(build.BarEnum_bar3)
	if i != 9 {
		t.Fatal("unexpected value")
	}

	i = uint(build.BarEnum_bar4)
	if i != 1291 {
		t.Fatal("unexpected value")
	}

	i = uint(build.BarEnum_bar5)
	if i != 16 {
		t.Fatal("unexpected value")
	}

	i = uint(build.BarEnum_bar6)
	if i != 17 {
		t.Fatal("unexpected value")
	}
}

func TestFixedArray(t *testing.T) {
	var s build.S
	if len(s.Arr8) != int(build.ArrSize_sizeTen) {
		t.Fatal("unexpected array size")
	}
	if len(s.Arr16) != int(build.ArrSize_sizeTwenty) {
		t.Fatal("unexpected array size")
	}
	if s.Size() != uint32(build.ArrSize_sizeTen)+uint32(build.ArrSize_sizeTwenty)*2 {
		t.Fatal("unexpected array serialization size")
	}

	var m build.M
	if len(m.Arr8) != int(build.ArrSize_sizeTen) {
		t.Fatal("unexpected array size")
	}
	if len(m.Arr16) != int(build.ArrSize_sizeTwenty) {
		t.Fatal("unexpected array size")
	}
	if m.Size() != uint32(build.ArrSize_sizeTen)+uint32(build.ArrSize_sizeTwenty)*2 {
		t.Fatal("unexpected array serialization size")
	}
}
