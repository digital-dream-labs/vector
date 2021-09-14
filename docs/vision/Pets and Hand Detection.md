# Pets and Hand Detection

Created by Andrew Stein Last updated Dec 07, 2018

Current Feature Summary:
* Hand Detection is slated for Vector R1.2.0 (Holiday release), Pet Detection is deferred to R1.3 (post-holiday)
* Initially formulate as image classification:
    * Avoids additional significant challenge of localizing hands or pets in the image
    * Massively simplifies data collection and labeling
* Longer term, can add localization 
* Neural Net Training Plan:
    * MobileNet classification network with 4 classes: hand, cat, dog, <none>  (Can combine cat+dog -> “pet” if needed / easier)
    * Start with data mined from online datasets (COCO + OpenImages), using fairly tight crops based on bounding box labels to simulate our scenario
    * Capture initial data with Vector for testing
    * Get initial network trained by Friday, October 5 for initial gut check
* User-Facing Behavior Plan:
    * Classification approach assumes we are already looking at the object
    * For hands:
        * In exploring, drive up to object, ask vision system "is this a hand?"
        * If yes, play hand-specific animation (e.g. purring, fist bump, nuzzling)
        * If no, continue as before (scan and poke animations)
        * Also check for a hand after reacting to an introduced obstacle. If hand, do the same hand-specific behavior.
    * For pets:
        * During exploring (question), periodically (question) ask vision system if a pet is present right in front of the robot.
        * If yes, play pet-specific animation (e.g. meow/bark/sneeze, to make it clear robot sees a pet)
        * If no, continue exploring as before.
* Data Collection:
    * Start with COCO+OpenImages
    * Add data from Vector, with help from employees, using DevImageCapture behavior
    * Idea: Ask Kickstarter backers for help. Have them use Take a Picture feature to capture images and upload to a DropBox folder (or something similar)
    * Idea: Partner with Animal Shelters. Have employees / TaskRabbits / Local KS Backers gather data at local SPCA or Animal Shelter and make a donation from Anki. 
