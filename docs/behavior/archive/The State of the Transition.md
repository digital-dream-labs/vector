# ARCHIVE: The State of the Transition

Created by Kevin Karol Mar 08, 2018

##"Behavior Helpers" are still a thing

All documentation for the victor behavior system only discusses Behaviors and makes no mention of behavior helpers or activities. This is because with the new power instilled in behaviors to delegate to other behaviors there is no need for these distinctions. For the time being behavior helpers haven't been subsumed into the true BSM stack but soon they will simply be a class of "Self managing" behaviors that only return success or failure. So use behavior helpers all you like, but don't be confused by the name/what the distinction is between them and normal behaviors.

## Don't Inject Delegates into GetAllDelegates()

The GetAllDelegates function is used by tests to build a full behavior tree. When declaring your delegates please do it in such a way that they don't change after Init(). This is currently not enforced, but for sanity's sake just declare everything you might delegate to in GetAllDelegates so that the behavior tree is stable.