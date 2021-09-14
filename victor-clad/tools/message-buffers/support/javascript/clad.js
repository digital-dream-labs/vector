class Clad {
  constructor() {}

  pack() {
    return null;
  }

  unpack(buffer) {
    return null;
  }

  unpackFromClad(cladBuffer) {
    let buf = cladBuffer.buffer.slice(cladBuffer.index);
    this.unpack(buf);
    cladBuffer.index += this.size;
  }

  get size() { return 0; }
}

class CladBuffer {
  constructor(buffer) {
    this.index = 0;
    this.buffer = buffer;
  }

  readBool() {
    let ret = this.buffer.slice(this.index, this.index + 1) != 0;
    this.index++;
    return ret;
  }

  readUint8() {
    let ret = IntBuffer.BufferToUInt8(this.buffer.slice(this.index, this.index + 1));
    this.index += 1;
    return ret;
  }

  readInt8() {
    let ret = IntBuffer.BufferToInt8(this.buffer.slice(this.index, this.index + 1));
    this.index += 1;
    return ret;
  }

  readUint16() {
    let ret = IntBuffer.BufferToUInt16(this.buffer.slice(this.index, this.index + 2));
    this.index += 2;
    return ret;
  }

  readInt16() {
    let ret = IntBuffer.BufferToInt16(this.buffer.slice(this.index, this.index + 2));
    this.index += 2;
    return ret;
  }

  readUint32() {
    let ret = IntBuffer.BufferToUInt32(this.buffer.slice(this.index, this.index + 4));
    this.index += 4;
    return ret;
  }

  readInt32() {
    let ret = IntBuffer.BufferToInt32(this.buffer.slice(this.index, this.index + 4));
    this.index += 4;
    return ret;
  }

  readBigInt64() {
    let ret = IntBuffer.LE64ToBigInt(this.buffer.slice(this.index, this.index + 8));
    this.index += 8;
    return ret;
  }

  readBigUint64() {
    let ret = IntBuffer.LE64ToBigUInt(this.buffer.slice(this.index, this.index + 8));
    this.index += 8;
    return ret;
  }

  readFloat32() {
    let byteArray = this.buffer.slice(this.index, this.index + 4);
    
    if(IntBuffer.IsHostLittleEndian()) {
      byteArray.reverse();
    }

    let ret = IntBuffer.BufferToFloat32(byteArray);
    this.index += 4;
    return ret;
  }

  readFloat64() {
    let byteArray = this.buffer.slice(this.index, this.index + 8);
    
    if(IntBuffer.IsHostLittleEndian()) {
      byteArray.reverse();
    }

    let ret = IntBuffer.BufferToFloat64(byteArray);
    this.index += 8;
    return ret;
  }

  readFArray(isFloat, type, capacity, signed) {
    if(type == 1) {
      let ret = this.buffer.slice(this.index, this.index + capacity);
      this.index += capacity;
      return signed? new Int8Array(ret) : new Uint8Array(ret);
    } else {
      let ret = this.buffer.slice(this.index, this.index + (capacity * type));
      this.index += (capacity * type);

      let typedArray;

      if(isFloat) {
        switch(type) {
          case 4:
            typedArray = new Float32Array(capacity);
            break;
          case 8:
            typedArray = new Float64Array(capacity);
            break;
          default:
            console.error("Unhandled array type.")
            return null;
        }

        for(let i = 0; i < capacity; i++) {
          let buf = ret.slice(i*type, (i*type) + type);
          let numArr;

          if(IntBuffer.IsHostLittleEndian()) {
            buf.reverse();
          }

          switch(type) {
            case 4:
              numArr = [IntBuffer.BufferToFloat32(buf)];
              break;
            case 8:
              numArr = [IntBuffer.BufferToFloat64(buf)];
              break;
            default:
              return null;
          }

          typedArray.set(numArr, i);
        }
      } else {
        switch(type) {
          case 2:
            typedArray = signed? new Int16Array(capacity) : new Uint16Array(capacity);
            break;
          case 4:
            typedArray = signed? new Int32Array(capacity) : new Uint32Array(capacity);
            break;
          default:
            console.error("Unhandled array type.")
            return null;
        }

        for(let i = 0; i < capacity; i++) {
          let buf = ret.slice(i*type, (i*type) + type);
          let numArr;

          switch(type) {
            case 2:
              numArr = signed? [IntBuffer.BufferToInt16(buf)] : [IntBuffer.BufferToUInt16(buf)];
              break;
            case 4:
              numArr = signed? [IntBuffer.BufferToInt32(buf)] : [IntBuffer.BufferToUInt32(buf)];
              break;
            default:
              return null;
          }

          typedArray.set(numArr, i);
        }
      }

      return typedArray;
    }
  }

  readVArray(isFloat, type, sizeType, signed) {
    let vArrayLength = 0;

    switch(sizeType) {
      case 1:
        vArrayLength = IntBuffer.BufferToUInt8(this.buffer.slice(this.index, this.index + 1));
        this.index++;
        break;
      case 2:
        vArrayLength = IntBuffer.BufferToUInt16(this.buffer.slice(this.index, this.index + 2));
        this.index += 2;
        break;
      case 4:
        vArrayLength = IntBuffer.BufferToUInt32(this.buffer.slice(this.index, this.index + 4));
        this.index += 4;
        break;
      case 8:
        vArrayLength = IntBuffer.BufferToUInt32(this.buffer.slice(this.index, this.index + 4));

        let bigNumber = false;
        for(let i = 0; i < 4; i++) {
          bigNumber |= (this.buffer[this.index + i] != 0);
        }

        if(bigNumber) {
          console.log("Warning! readVArray is reading a uint_64 type that is larger than uint_32, which is unsupported in JS_emitter.py");
        }

        this.index += 8;
        break;
    }

    return this.readFArray(isFloat, type, vArrayLength, signed);
  }

  readString(type) {
    let buffer = this.readVArray(false, 1, type, false);
    return String.fromCharCode.apply(String, buffer);
  }

  readStringFArray(isFloat, type, capacity) {
    let array = [];

    for(let i = 0; i < capacity; i++) {
      array.push(this.readString(type));
    }

    return array;
  }

  readStringVArray(isFloat, type, sizeType) {
    let vArrayLength = 0;
    let array = [];

    switch(sizeType) {
      case 1:
        vArrayLength = IntBuffer.BufferToUInt8(this.buffer.slice(this.index, this.index + 1));
        this.index++;
        break;
      case 2:
        vArrayLength = IntBuffer.BufferToUInt16(this.buffer.slice(this.index, this.index + 2));
        this.index += 2;
        break;
      case 4:
        vArrayLength = IntBuffer.BufferToUInt32(this.buffer.slice(this.index, this.index + 4));
        this.index += 4;
        break;
      case 8:
        vArrayLength = IntBuffer.BufferToUInt32(this.buffer.slice(this.index, this.index + 4));
        this.index += 8;
        break;
    }

    for(let i = 0; i < vArrayLength; i++) {
      array.push(this.readString(type));
    }

    return array;
  }

  ///
  /// Write operations
  ///
  write(array) {
    this.buffer.set(array, this.index);
    this.index += array.length;
  }

  writeBool(value) {
    this.buffer.set([value], this.index);
    this.index++;
  }

  writeUint8(value) {
    this.buffer.set([value], this.index);
    this.index++;
  }

  writeInt8(value) {
    this.buffer.set([value], this.index);
    this.index++;
  }

  writeUint16(value) {
    this.buffer.set(IntBuffer.Int16ToLE(value), this.index);
    this.index += 2;
  }

  writeInt16(value) {
    this.buffer.set(IntBuffer.Int16ToLE(value), this.index);
    this.index += 2;
  }

  writeUint32(value) {
    this.buffer.set(IntBuffer.Int32ToLE(value), this.index);
    this.index += 4;
  }

  writeInt32(value) {
    this.buffer.set(IntBuffer.Int32ToLE(value), this.index);
    this.index += 4;
  }

  writeBigUint64(value) {
    this.write(IntBuffer.BigIntToLE64(value));
  }

  writeBigInt64(value) {
    this.write(IntBuffer.BigIntToLE64(value));
  }

  writeFloat32(value) {
    this.buffer.set(IntBuffer.Float32ToLE(value), this.index);
    this.index += 4;
  }

  writeFloat64(value) {
    this.buffer.set(IntBuffer.Float64ToLE(value), this.index);
    this.index += 8;
  }

  writeFArray(array) {
    this.buffer.set(IntBuffer.TypedArrayToByteArray(array), this.index);
    this.index += (array.length * array.BYTES_PER_ELEMENT);
  }

  writeVArray(array, sizeType) {
    switch(sizeType) {
      case 1:
        this.buffer.set([array.length], this.index);
        break;
      case 2:
        this.buffer.set(IntBuffer.Int16ToLE(array.length), this.index);
        break;
      case 4:
        this.buffer.set(IntBuffer.Int32ToLE(array.length), this.index);
        break;
      case 8:
        this.buffer.set(IntBuffer.Int32ToLE(array.length), this.index);
        break;
      default:
        console.error("Unsupported size type.");
        break;
    }

    this.index += sizeType;

    this.buffer.set(IntBuffer.TypedArrayToByteArray(array), this.index);
    this.index += (array.length * array.BYTES_PER_ELEMENT);
  }

  writeString(value, sizeType) {
    let stringBuffer = new Uint8Array(value.length);

    for(let i = 0; i < value.length; i++) {
      stringBuffer.set([value.charCodeAt(i)], i);
    }

    this.writeVArray(stringBuffer, sizeType);
  }

  writeStringFArray(value, capacity, sizeType) {
    for(let i = 0; i < capacity; i++) {
      this.writeString(value[i], sizeType);
    }
  }

  writeStringVArray(array, arrayType, sizeType) {
    switch(arrayType) {
      case 1:
        this.buffer.set([array.length], this.index);
        break;
      case 2:
        this.buffer.set(IntBuffer.Int16ToLE(array.length), this.index);
        break;
      case 4:
        this.buffer.set(IntBuffer.Int32ToLE(array.length), this.index);
        break;
      case 8:
        this.buffer.set(IntBuffer.Int32ToLE(array.length), this.index);
        break;
      default:
        console.error("Unsupported size type.");
        break;
    }
    
    this.index += arrayType;

    for(let i = 0; i < array.length; i++) {
      this.writeString(array[i], sizeType);
    }
  }
}

class IntBuffer {
  static IsHostLittleEndian() {
    let buffer = new ArrayBuffer(2);
    let byteArray = new Uint8Array(buffer);
    let shortArray = new Uint16Array(buffer);
    byteArray[0] = 0x11;
    byteArray[1] = 0x22;

    return shortArray[0] == 0x2211;
  }

  static Int32ToLE(number) {
    let buffer = new Array(4);
    buffer[0] = number & 0x000000FF;
    buffer[1] = (number >> 8) & 0x0000FF;
    buffer[2] = (number >> 16) & 0x00FF;
    buffer[3] = number >> 24;
    return buffer;
  }

  static Int16ToLE(number) {
    let buffer = new Array(2);
    buffer[0] = number & 0x00FF;
    buffer[1] = number >> 8;
    return buffer;
  }

  static Float32ToLE(number) {
    let buffer = new Float32Array(1);
    buffer[0] = number;
    let byteArray = new Int8Array(buffer.buffer);

    if(!IntBuffer.IsHostLittleEndian()) {
      byteArray.reverse();
    }
    
    return Array.from(byteArray);
  }

  static Float64ToLE(number) {
    let buffer = new Float64Array(1);
    buffer[0] = number;
    let byteArray = new Int8Array(buffer.buffer);

    if(!IntBuffer.IsHostLittleEndian()) {
      byteArray.reverse();
    }
    
    return Array.from(byteArray);
  }

  static BufferToInt8(buffer) {
    var buf = new ArrayBuffer(1);
    var view = new DataView(buf);
  
    buffer.forEach(function (b, i) {
        view.setUint8(i, b);
    });
  
    return view.getInt8(0);
  }

  static BufferToUInt8(buffer) {
    var buf = new ArrayBuffer(1);
    var view = new DataView(buf);
  
    buffer.forEach(function (b, i) {
        view.setUint8(i, b);
    });
  
    return view.getUint8(0);
  }

  static BufferToInt16(buffer) {
    var buf = new ArrayBuffer(2);
    var view = new DataView(buf);
  
    buffer.forEach(function (b, i) {
        view.setUint8(i, b);
    });
  
    return view.getInt16(0, true);
  }
  
  static BufferToUInt16(buffer) {
    var buf = new ArrayBuffer(2);
    var view = new DataView(buf);
  
    buffer.forEach(function (b, i) {
        view.setUint8(i, b);
    });
  
    return view.getUint16(0, true);
  }

  static BufferToInt32(buffer) {
    var buf = new ArrayBuffer(4);
    var view = new DataView(buf);
  
    buffer.forEach(function (b, i) {
        view.setUint8(i, b);
    });
  
    return view.getInt32(0, true);
  } 

  static BufferToUInt32(buffer) {
    var buf = new ArrayBuffer(4);
    var view = new DataView(buf);
  
    buffer.forEach(function (b, i) {
        view.setUint8(i, b);
    });
  
    return view.getUint32(0, true);
  }

  static BufferToFloat32(buffer) {
    // Create a buffer
    var buf = new ArrayBuffer(4);
    // Create a data view of it
    var view = new DataView(buf);
  
    // set bytes
    buffer.forEach(function (b, i) {
        view.setUint8(i, b);
    });
  
    // Read the bits as a float; note that by doing this, we're implicitly
    // converting it from a 32-bit float into JavaScript's native 64-bit double
    return view.getFloat32(0);
  }

  static BufferToFloat64(buffer) {
    // Create a buffer
    var buf = new ArrayBuffer(8);
    // Create a data view of it
    var view = new DataView(buf);
  
    // set bytes
    buffer.forEach(function (b, i) {
        view.setUint8(i, b);
    });
  
    // Read the bits as a float; note that by doing this, we're implicitly
    // converting it from a 32-bit float into JavaScript's native 64-bit double
    return view.getFloat64(0);
  }

  static TypedArrayToByteArray(typedArray) {
    let elementSize = typedArray.BYTES_PER_ELEMENT;
    let buffer = new Uint8Array(elementSize * typedArray.length);

    if(typedArray.constructor.name.indexOf('Float') == 0) {
      // type is float array
      switch(elementSize) {
        case 4:
          for(let i = 0; i < typedArray.length; i++) {
            buffer.set(IntBuffer.Float32ToLE(typedArray[i]), i*4);
          }
          break;
        case 8:
          for(let i = 0; i < typedArray.length; i++) {
            buffer.set(IntBuffer.Float64ToLE(typedArray[i]), i*8);
          }
          break;
      }
    } else {
      switch(elementSize) {
        case 1:
          for(let i = 0; i < typedArray.length; i++) {
            buffer.set([typedArray[i]], i);
          }
          break;
        case 2:
          for(let i = 0; i < typedArray.length; i++) {
            buffer.set(IntBuffer.Int16ToLE(typedArray[i]), i*2);
          }
          break;
        case 4:
          for(let i = 0; i < typedArray.length; i++) {
            buffer.set(IntBuffer.Int32ToLE(typedArray[i]), i*4);
          }
          break;
        case 8:
          // not yet supported
          break;
      }
    }
    
    return buffer;
  }

  static BigIntToLE64(bigInt) {
    let big64bit = BigInt.asUintN(64, bigInt);
    let high = Number(big64bit & (0xFFFFFFFFn));
    let low = Number(big64bit >> (32n));

    let bufferHigh = IntBuffer.Int32ToLE(high);
    let bufferLow = IntBuffer.Int32ToLE(low);

    return bufferHigh.concat(bufferLow);
  }

  static LE64ToBigInt(buffer) {
    let low = buffer.slice(0, 4);
    let high = buffer.slice(4, 8);

    let lowInt = IntBuffer.BufferToUInt32(low);
    let highInt = IntBuffer.BufferToUInt32(high);

    let bigInt = (BigInt(highInt) << 32n) | BigInt(lowInt);

    return BigInt.asIntN(64, bigInt);
  }

  static LE64ToBigUInt(buffer) {
    let low = buffer.slice(0, 4);
    let high = buffer.slice(4, 8);

    let lowInt = IntBuffer.BufferToUInt32(low);
    let highInt = IntBuffer.BufferToUInt32(high);

    let bigInt = (BigInt(highInt) << 32n) | BigInt(lowInt);

    return BigInt.asUintN(64, bigInt);
  }
}

module.exports = { Clad, CladBuffer, IntBuffer };