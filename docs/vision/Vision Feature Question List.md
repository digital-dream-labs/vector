# Vision Feature Question List
Created by Andrew Stein Last updated Mar 18, 2019

## Context:

User-facing vision features*, especially those related to object recognition/teaching, have historically been plagued by early technical development going on for too long before running into a wall of some combination of unanswerable product/design/tech questions preventing full featurization. This document seeks to capture the types of questions that would ideally all have answers before going too far down the path of technical development. This is meant to be less of a process-oriented “checklist to be completed before moving forward” and more of a useful set of questions to make sure we cover in kickoff / brainstorm meetings.

Of course, some of these questions are hard to answer without some idea of the technical capabilities, so limited technical prototypes and some iteration will still be useful. But the earlier and more completely we can consider these questions, the better. Prototyping is fine, but longer term development of tech that’s going nowhere is something we want to avoid (especially optimization to run on-robot).

*This document is focused on features which directly expose vision tech to the user, as opposed to features that use some computer vision “under the hood” for another purpose, such as image compositing to find the charger in low light.

## Question Summary:

1. What will the user see / experience? What will the robot do?
2. How do we handle/hide/embrace unavoidable failures?
3. [For objects:] Do we need “instance recognition” or “category recognition” (or both/neither)?
4. Do we need to localize the object? (Know where it is in the image)
5. When does this run?
6. Where will we get training and test data?
7. How do QA a data-driven feature to test/verify it?

## Questions Explained:

1. What will the user see? What will the robot actually do?
    * To get “credit”, it must be ultra clear that the robot has recognized or understood something visually. This cannot just feel “slightly better than random behavior” in terms of its perceived reaction.
    * Subtle reactions like “the robot kinda prefers A over B” have historically failed; It’s not worth the development effort, nor the computation, to reveal these features subtly.
    * How do we express semantic understanding of what the robot sees, still in a characterful way? Do we show something on the face? Does the robot speak an object’s name? Examples:
        * Cozmo’s pet detection feature uses barking or meowing when he thinks he sees a cat or dog.
        * Vector’s hand detection feature is a bit more subtle, but he plays a hand-specific “nuzzle and climb-on” animation that also uses a purring sound to tie it to his petting reaction (which also involves direct interaction with hands).
    * If we have lots of “robot sees X, reacts by playing animation Y” situations, we should consider whether we can use a general Json-defined map of SalientPoints to animations.
        * This has benefits for scaling content with little Behavior dev involvement.
        * A major difficulty here is that it might necessitate that we can just run a single large-scale detector for “all the things” all the time, which is unlikely. See also question (5).

2. Visual recognition will never be “perfect”. How do we handle/hide/embrace failures?
    * Must acknowledge and address up front the fact that the robot will not always identify the thing we care about (false negatives) and that he will sometimes mistakenly see it (false positives).
    * Given the desire to “get credit” and “express intelligence”, this leads to competing desires, which should be explicitly considered in answering (1) above:
        * Avoiding subtlety (see above), so as to clearly and adequately express the robot’s intelligence and internal understanding.
        * Making things more subtle/abstract in order to avoid revealing the mistakes -- which will likely be more frequent than we’d like! -- which can make the robot seem “dumb”.
    * Testing plan must also take this into account. See (7) below.

3. Specifically for objects, do we need to recognize instances or categories?
    * Instance recognition: the robot remembers this specific object (without necessarily even knowing what it is), with the implication that his response to it is consistent over time.
    * Category recognition: the robot knows what kind of object this is. In this scenario, it is expected that the robot knows, for example, what a coffee mug is, and therefore can successfully recognize “any” coffee mug he sees.
    * These are not necessarily mutually exclusive: Some features could require both remembering a specific object instance and knowing what it is. (“This coffee mug.”)
    * “Neither” might also be an answer, as in the case of detecting “objectness”. There may be features for which we simply want to know there is an object but neither remember it nor know what it is by name.
    * In either case, it is helpful to constrain the number of categories/objects the robot needs to differentiate or recognize. For example, a feature to ask “Hey Vector, what is this fruit?” is far easier than the generic “Hey Vector, what is this?”.

4. Do we need to localize the object? (Know where it is in the image)
    * In vision, detection and classification generally mean two different things:
        * “Detection” implies knowing where in the image the object is.
        * “Classification” is simply realizing the object is present.
    * Classification is generally far simpler overall, easier to label, and computationally cheaper than detection. If a problem can be framed as one of classification, it may be easier to achieve.
    * Detection will be required if the robot needs to turn towards the object. If this is required, it is useful to think about how accurate this really needs to be. Limiting the required accuracy of the detection may make the feature more achievable.
    * Detection only tells you the object’s location in the image. Without some other cue or source of information, the robot will not know the distance to the object just from vision. (For example, we use relatively consistent distance between human eyes to compute the distance to a face, and we use known physical marker sizes on the cube to figure out their 3D pose.)

5. When do we run the detector/recognizer?
    * “All the time” is probably the wrong answer! Thanks to computational constraints, we can’t be looking for anything and everything the robot might be able to see all the time, in every frame.
    * Any feature definition must take into account how the vision system will know to even look for the relevant cue or object(s) it requires. (This is very “chicken-and-egg”!)
    * It is worth considering how to configure the behavior system to require the least frequent detection of the “right” objects as possible to expose the feature.   

6. Where do we get the training data?
    * Any feature based on machine learning requires data to train, and data held out to evaluate performance (and to use for QA/regression testing -- see (7) below).
    * The data should be as representative of the real world as possible. Given our robots’ unique viewpoint and camera, the more data we can actually collect from our robots, the better.
    * Given our limited ability to collect data from our robots, however, the more we can we leverage existing data sets, be clever about mining various data sources (YouTube?), and/or synthesize additional data, the better.
    * How can we collect data from willing users who opt in (hopefully using forthcoming tools in the app)?
        * Note that this must be viewed as a way to merely augment other data collection. It does not replace other approaches.
        * Until we have user participation that suggests otherwise, this is unlikely to result in enough data to “solve all our data collection problems”.
    * If we have no idea how we’d ever get a reasonable volume of data for the feature, it should not proceed.

7. How do we enable QA to test and verify data-driven features?
    * Two ends of the spectrum:
    * Blindly trust test dataset:
        * Statistical performance on a held-out test set says we are X% accurate. If X > some threshold, ship it! (Irrespective of “one-off” failures reported by QA / employees.)
        * Assumes our test dataset is fully representative of reality!
        * How do we know if we have enough data to trust our tests?
    * “Typical” QA testing:
        * Feature didn’t work for me X out of Y attempts (for small Y, like 10).
        * Not really actionable. Only options:
            * Add more data (ideally, the error cases they actually saw?)
            * Maybe adjust a threshold (but again, should probably be based on data)
        * Note that our QA test environments are likely even less representative of the real distribution of data, so putting too much weight on them could bias our tuning (i.e. overfitting).
            * Ideally the QA team would contribute images of their testing scenarios and environments for a feature before we reach the testing phase, so that training includes some data from those environments as well.
    * We (the Vision Team) generally lean towards “trusting the data” more, but how do we (with the help of the QA team) also sanity check that the feature isn’t totally broken with more “typical” testing?
    * What degree of accuracy do we expect? Can we define/bucket scenarios to define acceptable vs. unacceptable failures?
        * E.g. Do we require always detecting a cat, even from just a paw, or is it ok to only detect a cat when the face is occupying ¾ of the frame.
        * Can we use additional “attribute” labels to help bucket data into easy/hard examples with different required performance levels on each?
    * Tools we have, which we can use as part of the plan:
        * DevImageCapture, which has a mode for saving FalseNegatives / FalsePositives that can useful for mining hard examples.
        * If the behavior uses a WaitForImagesAction to enable the corresponding VisionMode, we can use its image-saving feature (enabled with a console var) to store images seen while testing.