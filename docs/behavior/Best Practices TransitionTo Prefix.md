# Best Practices: "TransitionTo" Prefix

Created by Kevin Karol Jan 31, 2018

Most behaviors have some concept of "state" even if it's not explicitly enumerated. In order to distinguish between member functions which are helpers (e.g. GetBestObject(), ShouldReactToStimulus()) and those which alter the behavior's "state" (e.g. what behavior/action to delegate to, what backpack lights to set) a prefix is applied to "state" related functions. The prefix TransitionTo____ indicates that a member function is transitioning the behavior's state. There is no explicit prefix required for helper functions.