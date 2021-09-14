# Connecting to Cubes

## CubeCommsComponent
  * Provides low level connection management and interface to BLE services.
  * Can be accessed via the web interface at Engine->WebViz->Cubes
  * Can be used to directly manage cube connections for debug and development
  * DO NOT MANAGE CONNECTIONS FROM HERE INSIDE THE ENGINE UNLESS YOU ARE CERTAIN ITS THE CORRECT COURSE! Directly creating/killing connections from the CubeCommsComponent will be perceived by other systems as unexpected connections/connection-losses. Instead, use the CubeConnectionCoordinator to subscribe to the connection you need. The Coordinator will communicate the aggregate connection need to the CubeCommsComponent.

## CubeConnectionCoordinator
  * Takes subscriptions for cube connections and manages the resultant connection state as appropriate
  * Coordinator state is visible on the Engine->WebViz->Cubes tab along with some exposed test subscription functions

### Connection Types:
Background: Connection/Status lights are NOT shown upon connect/disconnect. Localization status lights are DISABLED

Interactable: Connection/Status lights ARE shown upon connect/disconnect. Localization status lights are ENABLED

#### Background<->Interactable connection flow
  * Background subscriptions will cause a connection to be made, but no automatic lights will trigger, allowing the connection to be silently held as long as there are background subscribers.
  * Interactable subscriptions take precedence over background subscriptions, so new Interactable subscriptions will upgrade an existing background connection to an interactable one but nothing will happen if a new background subscription is submitted with an existing Interactable connection.
 
    NOTE: when a  connection is upgraded, connection lights will be shown as though the connection is newly established.

  * When the last Interactable subscription has been cancelled, after a "standby" timeout, the connection will fall back to a background subscription. "Disconnect" lights are shown during this transition. If a new interactable subscription is received while the "Disconnect" lights are playing, we will transition back to an Interactable connection WITHOUT showing the connection lights again.
  * The background connection will be held open as long as there are background subscribers. Once the last background subscription has been cancelled, after a "disconnect" timeout, the connection to the cube will be dropped. There will be no outward sign of this disconnection

### ICubeConnectionSubscriber
Any class which wishes to subscribe to a cube connection must implement this interface so it can be tracked. There are a couple of pure virtual methods which require overriding. This is masked in the behavior system by ICozmoBehaviors implementation, but pay special attention to the `ConnectionLostCallback`. This method will be called in the event of an unexpected connection loss after which all subscriptions will be internally dropped by the CubeConnectionCoordinator. Plan appropriately.

## Behavior System Structures
ICozmoBehavior implements the ICubeConnectionSubscriber interface and overrides its methods with empty implementations so that behaviors can override them locally. This means any behavior can also subscribe to a cube connection directly without specifying any cube connection requirements, but must also unsubscribe to prevent the cube connection from persisting indefinitely and killing our cube battery over the course of a single day, so... take this seriously.

A system for defining cube connection requirements is available within iCozmoBehavior::BehaviorOperationModifiers. There are (at time of writing) 5 distinct type of connection requirement:
1. None - the uninteresting case
1. OptionalLazy
   * Has no impact on WantsToBeActivated
   * Will subscribe to a connection ONLY IF A CONNECTION ALREADY EXISTS. This has the effect that Lazy subscriptions will prevent an existing connection from expiring, but will not generate a new one.
   * if it subscribes, causes ICozmoBehavior to subscribe to a connection in OnActivated and unsubscribe in OnDeactivated
1. OptionalActive
   * Has no impact on WantsToBeActivated
   * Will subscribe to a connection. This has the effect that Active subscriptions will generate new cube connections if none exists.
   * Causes ICozmoBehavior to subscribe to a connection in OnActivated and unsubscribe in OnDeactivated
1. RequiredLazy (opportunistic)
   * WantsToBeActivated will return false if there is NOT CURRENTLY an active connection
   * Will subscribe to a connection. This has the effect that a RequiredLazy connection will prevent an existing connection from expiring, but will not generate a new one.
   * Causes ICozmoBehavior to subscribe to a connection in OnActivated and unsubscribe in OnDeactivated
   * WILL STILL RUN if the existing connection is a background connection. This allows flexibility in our opportunistic employment of existing connections. If `connectInBackground` is set, will generate a background connection, regular connection hierarchy rules will apply.
   * Should be thought of as a behavior which will ONLY run if something else recently required a connection and left the connection in standby after it finished doing its thing
1. RequiredManaged
   * WantsToBeActivated will:
      1. Test the present behavior's ancestry in dev builds checking for a delegation ancestor which has `ensuresCubeConnectionAtDelegation` set to true. It will generate an error if it doesn't find one and return false. PLEASE DO NOT ABUSE THIS AND GO SETTING THIS VALUE TO TRUE IF YOUR BEHAVIOR DOES NOT ACTUALLY ENSURE A CUBE CONNECTION AT DELEGATION!!!
      1. return false if there is NOT CURRENTLY an active connection
   * Will subscribe to a connection. This has the effect that a RequiredManaged connection will prevent an existing connection from expiring, but will not generate a new one. 

### Typical Implementation
1. A behavior which requires a visible connection (e.g. `VectorPlaysCubeSpinner`) will specify `RequiredManaged`. This means you will need to place it as the descendant (child, grand-child, etc) of a behavior which specifies `ensuresCubeConnectionAtDelegation`
1. A behavior which specifies `ensuresCubeConnectionAtDelegation` (e.g. `ConnectToCube`) will most probably want to subscribe to a connection via `OptionalActive` and subsequently listen for callbacks from the `CubeConnectionCoordinator`, delegating only after the connection is confirmed.
