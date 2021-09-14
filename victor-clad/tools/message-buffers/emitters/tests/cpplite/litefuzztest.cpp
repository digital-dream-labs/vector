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
 
#include "ComplexUnion.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>

#include "fuzzdef.h"

int main(int argc, char **argv)
{
    MessageType message;
    std::string correct_type_name = MessageTypeName;

    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }
    std::string filename = argv[1];

    std::ifstream input(filename, std::ios::binary);
    input.seekg(0, std::ios::end);
    std::streampos size = input.tellg();
    input.seekg(0, std::ios::beg);

    std::cout << "Got file " << filename << " with size " << size << std::endl;

    std::vector<char> contents;
    if (size >= 0) {
        contents.reserve(size);
    }
    contents.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());

    // format:
    // fully::qualified::type
    // BASE64HASH==
    // 0#RJXJBINARYSTUFF@_\x23...

    std::vector<char>::const_iterator iter = contents.cbegin();
    std::vector<char>::const_iterator end = contents.cend();
    for (; iter != end; ++iter) {
        if (*iter == '\n') break;
    }
    if (iter == end) {
        std::cerr << "Expected newline in file " << filename << std::endl;
        return 1;
    }
    std::string type_name(contents.cbegin(), iter);
    if (type_name != correct_type_name) {
        std::cerr << "State is for wrong type. Expected " << correct_type_name + ", but got " << type_name << std::endl;
        return 1;
    }

    for (++iter; iter != end; ++iter) {
        if (*iter == '\n') break;
    }
    if (iter == end) {
        std::cerr << "Expected two newlines in file " << filename << std::endl;
        return 1;
    }

    ++iter;
    uint8_t *data = reinterpret_cast<uint8_t *>(const_cast<char *>(&*iter));
    size_t dataSize = contents.cend() - iter;

    if (MessageType::MAX_SIZE < dataSize) {
        std::cerr << "Buffer size is greater than maximum!" << std::endl;
        return 1;
    }

    // initialize it up with random crap
    std::srand(std::time(0));
    std::generate_n(reinterpret_cast<uint8_t *>(&message), sizeof(MessageType), std::rand);

    std::cout << "Unpacking " << correct_type_name << " from " << filename << "..." << std::endl;

    uint8_t* buffer = message.GetBuffer();
    memcpy(buffer, data, dataSize);

    if (!message.IsValid()) {
        std::cerr << "Invalid message: failed to parse (and size is " << message.Size() << " vs expected " << dataSize << ")" << std::endl;
        return 1;
    }

    if (message.Size() != dataSize) {
        std::cerr << "Valid message, wrong size: size is " << message.Size() << " vs expected " << dataSize << std::endl;
        return 1;
    }

    // nothing really to compare it against...

    std::cout << correct_type_name << " test passed!" << std::endl;
    return 0;
}
