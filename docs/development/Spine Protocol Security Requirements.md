# Spine Protocol Security Requirements

Created by Daniel Casner Last updated May 16, 2017

Android is too large of an attack surface so we have to assume that the head has been compromised and is attacking the system controller. We are relying on the integrity of the security controller in order to maintain privacy features. Hence the system controller must not trust anything send by the head that could allow it to be exploited. Specifically:

* Fixed format: the data in each frame (slug) shall be the same structure each time (within a given firmware version)
    * Tagged types are not allowed
    * If a piece of data is needed, include it in every frame
        * For example, instead of including a debug print or trace from the hardware processor, put the variables needed for debugging in the frame.
* No variable length arrays
* No data from the head should ever be used as a length, index or pointer of any kind
    * Unless explicitly checked in exactly one place before being used
* All parameters set by the head need to have their bounds checked and enforced on the system controller
    * Should we clamp?
    * Should we discard entire slugs?
