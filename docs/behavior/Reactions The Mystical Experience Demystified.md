# Reactions: The Mystical Experience Demystified

Created by Kevin Karol Oct 19, 2017

__The colors red, blue, and green are real. The color yellow is a mystical experience shared by all of us.
- Tom Stoppard, Rosencrantz and Guildenstern are Dead__


## Part 1: What the Tortoise said to Achilles about Reactions 

Achilles: What is that strange robot on that table? It reminds me somehow of a Wall-E

Tortoise: That is a Victor.

Achilles: A Victor? 

Tortoise: Yes Achilles, he's a life-like pet robot made by Anki.

Achilles: That really is quite the creation. I particularly enjoy how he seems to be reacting to things in his environment.

Tortoise: Yes, that is a wonderful feature. See how he reacts to being picked up. And how when I feed him he reacts to the cube shaking

Achilles: But good tortoise, surely you must be joking.

Tortoise: I certainly don't see why you should say such a thing Achilles. I'm being perfectly serious.

Achilles: Don't you see my friend, when Victor is picked up THAT is a reaction. But when you shake the cube to feed him, that is simply part of his behavior.

Tortoise: Achilles, I'm afraid you've quite lost me. Both situations you've just described have exactly the same component parts. Victor receives a stimulus, either being picked up or the cube shaking, and then he reacts by playing an animation in response.

Achilles: Exactly right. But what you may not know is that I attended Central Macedonian University, so I have a number of friends who work at Anki. And they told me that being picked up is a REACTION, but when the cube shakes in feeding that's just part of a behavior.

Tortoise: While Central Macedonian University is quite a prestigious institution, that really seems like a rather pedantic distinction to me. As an individual interacting with Victor for the first time there seems to me no distinction between Victor responding to being picked up and responding to a cube shaking during feeding.

Achilles: And that, my good friend, is why you are simply someone playing with Victor, not someone creating Victor.

Tortoise: Perhaps, or perhaps this concept of "Reaction" is simply an out of date concept that had a place within a simpler version of the product but which has outgrown its usefulness as the complexity of the product has evolved.

Achilles: Nonsense. If that were true then surely someone at the company would have provided a written example of the absurdity of the distinction from a user perspective, identified the issues the legacy system of thought has introduced into the system, and laid out a more sensible way to think about how Victor responds to stimuli across the user experience.

Tortoise: Indeed. You are quite right.

## Part 2: Picasso and The Color Yellow

For the purposes of clarity within this document the legacy system employed by the Cozmo product to trigger "reactions" will be referred to as Global Reactions. The necessity of this distinction is laid out in part by the dialogue above the crux of which is that "Reactions" as the term is generally used within the Victor development team refers specifically to the set of globally runnable behaviors that supposedly encompass how "Victor the character" should respond to a common set of stimuli. These include stimuli like being "picked up" which Victor should almost always respond to in order to give a sense that he is alive. Global Reactions started under the premise that there was a set of stimuli that Victor should almost always respond to, and when those weren't running then behaviors would take control instead. Let's represent this idealized version of reactions be represented by this lovely painting of a woman by Pablo Picasso which has great background/foreground distinction:

![](images/Picasso%20Reactions%20Fantasy.png)

Figure 1: How we like to think reactions work

And now let's find a visual equivalent of what the true implementation of Global Reactions has evolved into:

![](images/Picasso%20Reactions%20Fantasy%202.png)

Figure 2: How Reactions actually work

Excellent. The unfortunate reality is that Victor's "expected responses" in any give scenario is generally NOT global, it's highly contextual. And to the end user there is no distinction between the Global Reactions which were intended to give Victor a sense of life and the specific contextual interactions such as shaking a cube during feeding. "Reactions" and keeping Victor alive are essentially a Ship of Theseus in which we constantly swap out the stimuli that make sense for Victor to respond to and the result is a mystical experience of "aliveness" and Victor "reacting" which is totally separate from the Global Reactions implementation.

## Part 3: But Seriously, How Do I Write a Reaction?

You don't. What you write is a behavior like any other. And you include it at the appropriate (almost always non-global) level of the behavior tree. To the user Victor is still "reacting" but if there are special cases that arise where that reaction is no longer appropriate the reaction can be moved further down the tree.

![](images/Hooking%20Up%20Reactions.png)

Figure 3: Figuring out where "reactions" should live


If you believe what you're writing is something which might break Victor's "sense of aliveness" if it's not included in every single branch of the behavior tree, consider Writing Design Enforcement Unit Tests.