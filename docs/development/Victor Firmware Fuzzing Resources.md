# Victor Firmware Fuzzing Resources

Created by Henry Weller Sep 15, 2017


General fuzzing resources page: https://github.com/secfigo/Awesome-Fuzzing 
* Has wide-ranging resources on fuzzing 
* American Fuzzy Lop github page with setup instructions: https://github.com/mirrorer/afl

End-to-end fuzzing with AFL: https://foxglovesecurity.com/2016/03/15/fuzzing-workflows-a-fuzz-job-from-start-to-finish/

* Helpful for getting familiar with AFL using a real example. Also includes a tutorial on triaging crash data +1

Compiling AFL in LLVM Mode: https://reverse.put.as/2017/07/10/compiling-afl-osx-llvm-mode/

AFL status screen debugging page: http://lcamtuf.coredump.cx/afl/status_screen.txt

* Go-to page for interpreting/troubleshooting the AFL readout UI
AFL Google Group: https://groups.google.com/forum/#!forum/afl-users

* 2+ years of AFL project queries that is moderated by Michael Zalewski, creator of AFL, and his team of engineers


Project overview:

* Generate random test cases to expose vulnerabilities within the Victor firmware, specifically the syscon bootloader comms.cpp function


Set-up instructions (above links are incredible resources but this is exactly what I did to configure my fuzzer)

* git clone github.com/mirrorer/afl
* Install latest version of clang http://releases.llvm.org/4.0.1/clang+llvm-4.0.1-x86_64-apple-darwin.tar.xz. 
* extract clang and rename the extracted folder something managable
* cd afl, cd llvm_mode
* export LLVM_MODE=/pathtoclang/bin/llvm-config
* make (this should build the afl-clang-fast++ and afl-clang-fast binaries in your afl directory)
* Now when you build your projects to be fuzzed, target the correct compiler binary for your application using CC/CXX flags, and set AFL_CC/CXX flags to the clang compiler in the same directory as the llvm-config file


Powerpoint Presentation:

[Fuzzing the Syscon Bootloader](Fuzzing%20the%20Syscon%20Bootloader.pptx)


Email any other fuzzing questions you may have to hweller@stanford.edu (smile)