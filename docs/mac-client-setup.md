# Victor Setup Using mac-client

## Description

The `mac-client` program is used to connect to a robot over BLE to perform initial setup of a 
Victor robot. It will securely pair to the robot, and provide an encrypted/authenticated 
channel for you to complete tasks such as getting Victor connected to a WiFi network, or 
having him complete an OTA (over the air) update.

`mac-client` is built and run on a computer running `Mac OS X`.

## Building 

To build `mac-client`, simply run the [build command](/apps/demos/ble-pairing/mac-client/build_client.sh) from the root of your `victor` repo:
```
./apps/demos/ble-pairing/mac-client/build_client.sh
```

## Quick Start - OTAing with Ease

After you are paired once with your robot (see `Running` section), you can run this oneline command to update your robot `"Vector A2B2"` (if your robot is not already connected to the internet, this command will try to connect your robot to `AnkiRobits` for you):
`./mac-client -f A2B2 --ota-update http://example.com/update.ota`

If you want to speed things up and perform an OTA with the robot in AP mode, you can simply do:
`./mac-client -f A2B2 --ota-update-ap http://example.com/update.ota`

You can specify `--local (-l)` with the `--ota-update-ap` command to specify a local file instead of a URL.

This command might not work 100% of the time because it involves using MacOS api to try to connect your Mac to Victor's wifi network, which is a little tempermental.

## Running

To run `mac-client`, also from your repo root, do:
```
./apps/demos/ble-pairing/mac-client/_build/mac-client
```

However, you will probably want to run with a filter so the `mac-client` only connects to
your robot:

```
# I will only connect to Victor with name "Vector C3P0"
./apps/demos/ble-pairing/mac-client/_build/mac-client --filter C3P0
```

The first time you connect to your Victor, you will need to put Victor in pairing mode.
To do this, double-press his backpack button. You should see the `mac-client` show an 
`> Enter pin:` prompt:

```
./mac-client --filter C3P0
* Connecting to Vector C3P0
* Speaking to Vector with protocol version [2].
> Enter pin:
```

At this point, type in the 6 digit number you see on Victor's face, and hit `enter`. If 
everything worked, you should see the Victor cli prompt appear:

```
vector-R1A4#
```

## Performing Tasks Synchronously

You can add the `-s` arg followed by a string to perform the CLI commands non-interactively.

For example, you can do:

`./mac-client -f A2B2 -s 'wifi-scan; wifi-connect:MyWifiSsid|MyPassword; wifi-ip'`.

This command will 1) scan for wifi, and then when finished will 2) connect to "MyWifiSsid" with password "MyPassword". 3) After the connection request is successful or fails, the app will request the robot's IP.

## Using the Victor Cli

The cli provides a set of commands for you to securely get Victor connected to a wifi network
and optionally update his OS.

Type `help` and enter to see the full list of commands.

### Exiting the Shell

Type `exit` or hold `ctrl` + `d`.

### Connecting to WiFi

1. Scan for wifi-networks by using the command `wifi-scan`. (Required!)
2. Connect to a network by using `wifi-connect ssid password`.

You can check the status of WiFi connection by typing `status` and you can get his IP address
by typing `wifi-ip`.

### Updating Victor OTA

Type `ota-start url`, where `url` is a valid URL with an Anki-signed OTA.

You can cancel an in-progress OTA by typing `ota-cancel`.

You can get a live-updating progress bar by typing `ota-progress`. If you do this, you will
have to use `ctrl` + `d` if you want to exit the shell prematurely.

### Use Victor as Access point 

If you can't get Victor to connect to a WiFi network, you might be saved by placing Victor in 
access point mode and having one of your devices (e.g., your Macbook) connect to Victor's 
network. 

To put him in access point mode, type `wifi-ap true`. (Note: If you want to try connecting 
Victor to another wifi network after this, make sure you perform a `wifi-scan` first!).

The `wifi-ap true` command will print out a wifi ssid and password for you to connect to. It 
should look like `Vector C3P0` with a password that is 8 numerical digits.

## Connecting through SSH

Victor robots have a shared public key. In order to SSH, you will need to download the shared 
private key. You can find the instructions from [this confluence page](https://ankiinc.atlassian.net/wiki/spaces/ATT/pages/368476326/Victor+DVT3+ssh+connection).

Once you have the private key installed and permissions set properly, you should be able to
follow the instructions from the confluence page to start SSH session. For convenience, you
can also use the `mac-client` Victor cli. Simply type `ssh-start`, and a new terminal window
will be loaded and will run the command `ssh root@127.0.0.1`.
