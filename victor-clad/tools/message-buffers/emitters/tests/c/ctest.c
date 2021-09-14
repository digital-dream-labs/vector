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

#include "aligned/CTest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_Foo()
{
  printf("Test Foo:\n");
  Foo foo1;
  foo1.isFoo = 0;
  foo1.myByte = 0x0f;
  foo1.myShort = 0x0c0a;
  foo1.myFloat = 1.0f;
  foo1.myNormal = 0x0eadbeef;
  foo1.myFoo = AnkiEnum_d2;
  //foo1.myString = "Blah Blah Blah";
  size_t length = Foo_Size(&foo1);

  Foo foo2;

  memcpy(&foo2, &foo1, length);

  if (foo1.isFoo == foo2.isFoo &&
      foo1.myByte == foo2.myByte &&
      foo1.myShort == foo2.myShort &&
      foo1.myFloat == foo2.myFloat &&
      foo1.myNormal == foo2.myNormal &&
      foo1.myFoo == foo2.myFoo) {
    printf("PASS foo1 == foo2\n");
    return 1;
  }
  else {
    printf("FAIL foo1 != foo2\n");
    return 0;
  }
}

int test_MyMessage()
{
  printf("Test MyMessage:\n");
  MyMessage message;
  message.tag = MyMessageTag_foo;
  message.foo.isFoo = 0x1;
  message.foo.myByte = 0x0f;
  message.foo.myShort = 0x0c0a;
  message.foo.myFloat = -0.0f;
  message.foo.myNormal = 0x0eadbeef;
  message.foo.myFoo = AnkiEnum_d2;

  MyMessage message2;
  memcpy(MyMessage_Cast(&message2), MyMessage_Cast(&message), MyMessage_Size(&message));

  if (message.foo.isFoo == message.foo.isFoo &&
      message.foo.myByte == message2.foo.myByte &&
      message.foo.myShort == message2.foo.myShort &&
      message.foo.myNormal == message2.foo.myNormal &&
      message.foo.myFoo == message2.foo.myFoo) {
    printf("PASS message.foo == message2.foo\n");
  }
  else {
    printf("FAIL message.foo != message2.foo\n");
    return 0;
  }

  Bar temp = {
    { 1, 1, 0, 0, 1, 0, 1, 0 },
    { 0, 1},
    { 5, 6, 7 },
    1000000000000000,
    { 3.1415926535897932, -22.0e-123, 1.0 / 0.0 },
    { AnkiEnum_d1, AnkiEnum_e1, AnkiEnum_d2, AnkiEnum_e2 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }
  };

  message.tag = MyMessageTag_bar;
  message.bar = temp;

  memcpy(MyMessage_Cast(&message2), MyMessage_Cast(&message), MyMessage_Size(&message));

  if (memcmp(&message.bar.byteBuff, &message2.bar.byteBuff, sizeof(message.bar.byteBuff)) == 0 &&
      memcmp(&message.bar.shortBuff, &message2.bar.shortBuff, sizeof(message.bar.shortBuff)) == 0 &&
      memcmp(&message.bar.enumBuff, &message2.bar.enumBuff, sizeof(message.bar.enumBuff)) == 0 &&
      memcmp(&message.bar.fixedBuff, &message2.bar.fixedBuff, sizeof(message.bar.fixedBuff)) == 0) {
    printf("PASS message.bar == message2.bar\n");
    return 1;
  }
  else {
    printf("FAIL message.foo != message2.foo\n");
    return 0;
  }
}

int main(int argc, char** argv)
{
  // fix warnings
  (void)argc;
  (void)argv;

  if(!test_Foo()) {
    return 1;
  }
  if(!test_MyMessage()) {
    return 3;
  }
  return 0;
}
