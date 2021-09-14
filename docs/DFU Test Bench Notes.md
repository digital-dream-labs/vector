# DFU Test Bench Notes

Created by Henry Weller Last updated Aug 01, 2017


Documentation on spine communication protocol: https://github.com/anki/cozmo-one/blob/master/robot2/clad/src/clad/spine/spine_protocol.clad 

Objectives

* Simulate the Hardware Processor Bootloader to test whether the DFU sends and acknowledges messages properly
    * What are all the possible ways that somebody could get through one layer of authentication and mess up data on another
* This being my first test bench, I thought it would be a good idea to document the process of writing my first few tests should anyone else on hardware decide to give Test-Driven Development a chance
    * A great place to start is James W. Grenning's Test-Driven Development for Embedded C. Adam has a hard copy and you should be able to get a PDF online.
    * Another gift from Adam is an introductory makefile for tests which supports the cpputest library
    * One positive thing about these test scripts is that they can be pretty easily integrated into directories with test.cpp and main.cpp being the only files that needed to be added. All of the tests are self contained within the former, and executed by the latter. 
* Created an alternative platform specifically for simulations
    * This allows me to bypass any platform-specific or hardware-dependent function calls (e.g. open(devicename,...)) to test individual modules within dfu.c

Three weeks in update:

* Version request and DFU Image sending tests have all been written and validated
* Some refactoring will need to take place in the future to refine some of the data handling but the data pipeline structure is complete 

![](images/image2017-8-1_17-1-46.png)
