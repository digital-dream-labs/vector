# Victor Debug Process

Created by Jordan Rivas 

## Start lldb server
Use 'start-lldb.sh' to connect to the robot and start an lldb server:

```
#!/usr/bin/env bash
./project/victor/scripts/start-lldb.sh
```

This will open victor's firewall for incoming connections and start an LLDB server at port 55001.

You can connect to the server with LLDB commands like this:

```
LLDB
platform select remote-linux
platform connect connect://victor:55001
```

## Debugging with Xcode
If you haven't built for xcode, or your xcode project needs an update, you'll need to build it with a command like this:

```
#!/usr/bin/env bash
victor_build_xcode
```

Use Xcode to open the project file created in 'victor/_build/mac/Debug-Xcode/victor.xcodeproj'.

You can use Xcode to debug unit tests and simulator processes, but it does not have good support for remote connections or cross-compiled targets.

## Debugging with VS Code
Open VS Code, goto File → Open and select victor repo root directory. Once the project loads click the Debug icon on the Side Bar. Select the configuration you want to run from the header and click the "play" button to run and attach the debugger.

You can find more information inside the victor repo:

https://github.com/anki/victor/blob/master/docs/development/debugging.md

You can find sample debug tasks inside the victor repo:

https://github.com/anki/victor/blob/master/templates/vscode/launch.json

## Victor Crash Reports
If you get a tombstone log from debuggerd, you can decode the stack trace as described here:

https://github.com/anki/victor/blob/master/docs/FAQ.md#how-do-i-decipher-this-crash-in-the-victor-log

Related Pages