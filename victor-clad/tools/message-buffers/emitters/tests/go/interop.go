package test

// #cgo CFLAGS: -I ./build/cpp -I ../../../support/cpp/include
// #include "cbridge.h"
import "C"
import (
	"anki/clad"
	"build"
	"bytes"
	"fmt"
	"math"
	"reflect"
	"testing"
	"unsafe"
)

// pack the given struct into a buffer to pass to C - C will unpack it into their version
// of the object, re-pack it into a buffer to send back to us
func doRT(objType C.int, obj clad.Struct) *bytes.Buffer {
	var buf bytes.Buffer
	if err := obj.Pack(&buf); err != nil {
		return nil
	}

	inBytes := buf.Bytes()
	retBuf := make([]byte, 512)

	written := C.RoundTrip(objType,
		(*C.uint8_t)(unsafe.Pointer(&inBytes[0])),
		(C.size_t)(len(inBytes)),
		(*C.uint8_t)(unsafe.Pointer(&retBuf[0])),
		(C.size_t)(512))
	fmt.Println("Packed", len(inBytes), "bytes and received", written, "bytes back")
	return bytes.NewBuffer(retBuf[:written])
}

func testCFunky(t *testing.T) {
	m := build.Funky{A: build.AnkiEnum_d1, B: 122}

	buf := doRT(C.Type_Funky, &m)
	var m2 build.Funky
	if err := m2.Unpack(buf); err != nil {
		t.Fatal("Unpack error:", err)
	}

	if !reflect.DeepEqual(m, m2) {
		printCompareError(t, "m", "m2", m, m2)
	}
}

func testCMonkey(t *testing.T) {
	var m build.Monkey
	m.A = build.Funky{A: build.AnkiEnum_e1, B: 126}
	m.B = 3.0

	buf := doRT(C.Type_Monkey, &m)
	var m2 build.Monkey
	if err := m2.Unpack(buf); err != nil {
		t.Fatal("Unpack error:", err)
	}

	if !reflect.DeepEqual(m, m2) {
		printCompareError(t, "m", "m2", m, m2)
	}
}

func testCMusic(t *testing.T) {
	var m build.Music
	m.C = build.Funky{A: build.AnkiEnum_d3, B: 119}
	m.D = [1]int64{math.MaxInt32 + (math.MaxInt32 / 2)}

	buf := doRT(C.Type_Monkey, &m)
	var m2 build.Music
	if err := m2.Unpack(buf); err != nil {
		t.Fatal("Unpack error:", err)
	}

	if !reflect.DeepEqual(m, m2) {
		printCompareError(t, "m", "m2", m, m2)
	}
}

func testCFunkyMessage(t *testing.T) {
	var mu build.Music
	mu.C = build.Funky{A: build.AnkiEnum_d3, B: 119}
	mu.D = [1]int64{math.MaxInt32 + (math.MaxInt32 / 2)}

	m := build.NewFunkyMessageWithMusic(&mu)

	buf := doRT(C.Type_FunkyMessage, m)
	var m2 build.FunkyMessage
	if err := m2.Unpack(buf); err != nil {
		t.Fatal("Unpack error:", err)
	}

	if !reflect.DeepEqual(m, &m2) {
		printCompareError(t, "m", "m2", m, m2)
	}
}

func testCUnionOfUnion(t *testing.T) {
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

	var m build.UnionOfUnion
	m.SetMyFooBar(&myUnion)

	buf := doRT(C.Type_UnionOfUnion, &m)
	var m2 build.UnionOfUnion
	if err := m2.Unpack(buf); err != nil {
		t.Fatal("Unpack error:", err)
	}

	if !reflect.DeepEqual(m, m2) {
		printCompareError(t, "m", "m2", m, m2)
	}
}

func testCMessageOfUnion(t *testing.T) {
	var myFoo build.Foo
	myFoo.MyByte = 0x7f
	myFoo.ByteTwo = 0xfe
	myFoo.MyShort = 0x0afe
	myFoo.MyFloat = 123.123123e12
	myFoo.MyNormal = 0x0eadbeef
	myFoo.MyFoo = build.AnkiEnum_myReallySilly_EnumVal
	myFoo.MyString = "Whatever"

	myUnion := build.NewFooBarUnionWithMyFoo(&myFoo)

	var m build.MessageOfUnion
	m.AnInt = 11
	m.MyFooBar = *myUnion
	m.ABool = true

	buf := doRT(C.Type_MessageOfUnion, &m)
	var m2 build.MessageOfUnion
	if err := m2.Unpack(buf); err != nil {
		t.Fatal("Unpack error:", err)
	}

	if !reflect.DeepEqual(m, m2) {
		printCompareError(t, "m", "m2", m, m2)
	}
}
