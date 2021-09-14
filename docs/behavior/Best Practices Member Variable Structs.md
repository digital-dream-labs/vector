# Best Practices: Member Variable Structs
Created by Kevin Karol  Feb 26, 2018

Behaviors generally have two types of member variables: those which "define" how a specific instance operates, and those which "track" aspects of behavior state in order to make decisions. To make it clear which variables belong to each category it's recommended that member variables be placed within structs that group them together and make it clear the role a variable is playing in the decision making process.

The two structs privately defined by most behaviors are:

  1) InstanceConfig - contains member variables that "define" how the instance operates and are loaded from JSON or set in code per instance

  2) DynamicVariables - contains member variables that "track" behavior state or properties that are updated throughout the behavior lifecycle. Generally this struct is re-set OnBehvaiorActivated to ensure that a behavior doesn't get stuck with bad state for the full robot runtime. 

These two structs should have a default constructor defined in the .cpp file so that the behavior can be easily reset. Occasionally InstanceConfig will have a second constructor that initializes the struct from JSON.

### Example

Within most behaviors you will see the following defined in the private scope:

```
struct InstanceConfig{
  InstanceConfig();
  InstanceConfig(const Json::Value& config);
};

struct DynamicVariables{
  DynamicVariables();
};


InstanceConfig _iConfig;
DynamicVariables _dVars;
``