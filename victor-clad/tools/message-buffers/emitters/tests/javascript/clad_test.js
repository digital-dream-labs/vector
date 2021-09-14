const { TestClad } = require('./build/simple/Javascript.js');
const assert = require('assert');
const fs = require('fs');

// Tests for Javascript CLAD emitter 

let testSubEnum = TestClad.EnumT.One;
let testSubBool = false;

let testSubMessage = new TestClad.SubMessage(testSubBool, testSubEnum);

// cladMessage fields
let testBool = true;
let testUint8 = 147;
let testUint16 = 35149;
let testUint32 = 1290345;
let testUint64 = 5239140128n;
let testFloat32 = 343.2;
let testFloat64 = 12999.1234;
let testString = "Hello, world!";
let testString16 = "Hi, a string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. A string of length longer than 256 characters. The End.";
let testFArray8 = new Uint8Array([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 ]);
let testVArray8 = new Uint8Array([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]);
let testVArrayFloat32 = new Float32Array([1.23, 2.34, 3.45, 4.56, 5.67, 6.78, 7.89]);
let testStringArray = [ "First", "Second", "Third", "Fourth", "Fifth", "Sixth" ];

let cladMessage = new TestClad.Message(
  testBool, 
  testUint8,
  testUint16,
  testUint32, 
  testUint64,
  testFloat32, 
  testFloat64, 
  testSubMessage, 
  testString, 
  testString16, 
  testFArray8, 
  testVArray8, 
  testVArrayFloat32,
  testStringArray);

let buffer = cladMessage.pack();

// write buffer to file for interop testing
fs.writeFileSync("./buffer.tmp", Buffer.from(buffer), function(err) {
  //
  console.log("write file");
});

// scramble data
cladMessage.testBool = null;
cladMessage.testUint8 = null;
cladMessage.testUint16 = null;
cladMessage.testUint32 = null;
cladMessage.testUint64 = null;
cladMessage.testFloat32 = null;
cladMessage.testFloat64 = null;
cladMessage.testSubMessage = null;
cladMessage.testString = null;
cladMessage.testString16 = null;
cladMessage.testFArray8 = null;
cladMessage.testVArray8 = null;
cladMessage.testVArrayFloat32 = null;
cladMessage.testStringArray = null;

cladMessage.unpack(buffer);

assert(testBool == cladMessage.testBool, "bool test failed.");
assert(testUint8 == cladMessage.testUint8, "uint_8 test failed.");
assert(testUint16 == cladMessage.testUint16, "uint_16 test failed.");
assert(testUint32 == cladMessage.testUint32, "uint_32 test failed.");
assert(testUint64 == cladMessage.testUint64, "uint_64 test failed.");
assert(Math.abs(testFloat32 - cladMessage.testFloat32) < 0.001, "float_32 test failed.");
assert(Math.abs(testFloat64 - cladMessage.testFloat64) < 0.001, "float_64 test failed.");
assert(testSubMessage.testSubBool == cladMessage.testSubMessage.testSubBool, "Bool field of sub-message test failed.");
assert(testSubMessage.testSubEnum == cladMessage.testSubMessage.testSubEnum, "Enum field of sub-message test failed.");
assert(testString == cladMessage.testString, "String with max length MAX_UINT8 test failed.");
assert(testString16 == cladMessage.testString16, "String with max length MAX_UINT16 test failed.");

function checkArrayEquality(a, b) {
  if(a === b) return true;
  if(a == null || b == null) return false;
  if(a.length != b.length) return false;

  for(let i = 0; i < a.length; i++) {
    if(a[i] != b[i]) return false;
  }

  return true;
}

function checkArrayFloatProximity(a, b) {
  if(a === b) return true;
  if(a == null || b == null) return false;
  if(a.length != b.length) return false;

  for(let i = 0; i < a.length; i++) {
    if(Math.abs(a[i] - b[i]) >= 0.001) return false;
  }

  return true;
}

assert(checkArrayEquality(testFArray8, cladMessage.testFArray8), "Fixed array of uint8s test failed.");
assert(checkArrayEquality(testVArray8, cladMessage.testVArray8), "Variable array of uint8s test failed.");
assert(checkArrayFloatProximity(testVArrayFloat32, cladMessage.testVArrayFloat32), "Array of floats test failed.");
assert(checkArrayEquality(testStringArray, cladMessage.testStringArray), "Array of strings test failed.");

console.log("All tests passed!");