# Dependency Managed Component

The [entity/component model](https://en.wikipedia.org/wiki/Entity%E2%80%93component%E2%80%93system) consists of classes where an entity (in gaming this is traditionally a character, a weapon, etc) can accept an arbitrary set of components (movement component, AI component, shield component) that it operates on in a uniform way. The Dependency Managed Entity/Component utility classes build upon this paradigm to allow components to declare explicit ordering requirements for Initialization and Update, as well as to receive direct access to their fellow components without the need to pass around the entity (e.g. passing robot throughout the system to access its components).

When Init or Update are called on an Entity, the entity automatically:
  1) performs a topological sort on its components
  2) builds the dependency/additional access list of components to be passed into each component
  2) calls the init/update function on each sub component in sorted order, passing in the components it requested

Most of the functions present in the entity/component base classes are to support these three automated steps.
  * GetInitDependencies/AdditionalInitAccessibleComponents/GetUpdateDependencies/AdditionalUpdateAccessibleComponents: These functions provide the list of dependencies (step 1) and components to include in the init and update calls (step 2) to the component
  * The templated base class helps with tying together enum IDs and type safe casts (see below)
  * The has/get component checks allow entities to be built with different components - currently most components exist for the entire lifetime of the entity, but this may not always be the case


Engine currently has three main dependency managed entities: Robot, AI and Behavior. Each of these entities has a components_fwd and components_impl file which list the components available in each entity.  These lists consist of both 1) an enum value for the component 2) the c++ class associated with that component. The enum value is almost exclusively used internally by the entity - in most cases component access should happen via the templated GetComponent function. This automatically handles type safe component access/casting.  Calling entity.GetComponent<ClassType>() will fail at compile time if the class is not present within the entity. For an example of how this syntax can make component access susinct see iCozmoBehavior's GetAIComp() and GetBehaviorComp() functions.


## Unreliable Components
Since dependency managed components are associated with a specific entity there is occasionally a need to share a component across multiple levels of the system (e.g. Face World is owned by robot, but many AI components need easy access to it). Declaring the component as an Unreliable Component allows it to be accessed through the normal templated class accessors, but errors will be thrown if a component at the unmanaged level declares a dependency on it. This is because the Init/Update order is determined by a different entity's ordering.