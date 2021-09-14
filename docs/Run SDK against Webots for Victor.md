# Run SDK against Webots for Victor

Created by Thanh Le Last updated Nov 26, 2018
Pre-condition:
* Have pulled the lasted version of Victor project from Github
* Have Webots installed in Macbook.

## Step-by-step guide
All step that I used to run SDK test against Victor Webots:

1. Build victor for mac by running './project/victor/build-victor.sh -p mac -f'.
2. Setting Up the SDK: Please refer to this page for the latest instruction on how to set up the SDK: Python Vector SDK - Getting Started
3. Open `simulator/worlds/cozmo2World.wbt` in victor project. 
4. Turn on behaviors in webots (tap blue background and tap ']' or '/' key)
5. Open new terminal, navigate to `tools/sdk/vector-sdk/examples/tutorials` and run: `./01_hello_world.py --serial Local`
6. Or you can navigate to: `$VICTOR_ROOT/tools/sdk/vector-sdk/tests`  and run one of these commands bellows:

```
$VICTOR_ROOT/tools/sdk/vector-sdk/tests
> ./test_say_text.py --serial Local
```

### Troubleshooting

For step 3: If we receive some error about webots can't connect to anim server. Try to install curl as follow:

```
$VICTOR_ROOT/tools/sdk/grpc_tools
> brew install curl --with-nghttp2
> brew link curl --force
```

For step 4: When running webots, if you get pop-ups asking "Do you want the application 'webotsCtrl...' to accept incoming network connections?", then you have to set up a firewall certificate.

1. Close Webots.
2. Follow the instructions in FirewallCertificateInstructions.md.
3. Run the Webots test script (which will automatically set up firewall exceptions for you) with the following command: `project/build-scripts/webots/webotsTest.py --setupFirewall`. You will need to enter your admin password.


