# C2SIM SDK

Resources for controlling state and handling messages in a C2SIM environment. For an overview of C2SIM and its capabilities to support multiple connected Command and Control (C2) and Simulation systems, see the [C2SIM Server Reference Implementation Documentation](https://bit.ly/36E8Sb5)

The focus of the SDK is specifically on the C2SIM protocol, rather than legacy ones (e.g. CBML, IBML, IBML9, MSDL) which may be supported by C2SIM environments as well.

The SDK is built on top of generic capabilities offered by a [(ported) Client Library](https://github.com/hyssostech/OpenC2SIM.github.io/tree/master/Software/Library/CS/C2SIMSDK/C2SIMClientLib), configured with specific parameters obtained by examining the 
[C2SIM GUI Editor code](https://github.com/hyssostech/OpenC2SIM.github.io/tree/master/Software/Client/C2SIMGUIv2.10.9) 
use of the [Java C2SIMClientLib](https://github.com/hyssostech/OpenC2SIM.github.io/tree/master/Software/Library/Java/C2SIMClientLib).

NOTE: This SDK tracks the C2SIM standard as it evolves through multiple iterations driven primarily by CWIX. The current version supports the CWIX2024 ontology and schema. 
Documents complying to the official SISO v1.0.1 can be produced by selecting the appropriate namespace to deserialize messages (see the *C2SIM XSD object serialization* section below for details).  

In a nutshell, the SDK:

- Wraps a series of basic Library Commands required to make a C2SIM server transition to a state where it is ready to accept Initializations, or Orders, or to join an ongoing session
- Issues Library messages with required parameters for pushing specific messages containing Orders, Initializations and Reports
- Serializes and deserializes XML messages from/into plain old C# objects
- Exposes generic Library (STOMP) notification messages as finer grained events signaling the reception of Initialization, 
Orders, Reports, and server status changes


## Nuget Install

The [HyssosTeck.Sdk.C2SIM](https://www.nuget.org/packages/HyssosTech.Sdk.C2SIM/) nuget provides support for: 

* .NET 6 projects
* .NET Standard 2.0 - compatible with recent .NET Framework projects

As such, it should be compatible with Windows, macOs and Linux platforms, provided these have the required [.NET Runtime/SDK](https://dotnet.microsoft.com/download/dotnet/6.0) installed


## Quick Start

1. Create a `C2SIMSDK` object pointing to a C2SIM server

    ```CS
    // ... obtain reference to logger that should be used by the SDK
    ILogger logger = null; // Create an appropriate logger here
    C2SIMSDK c2SimSDK = new C2SIMSDK(
        logger,
        new C2SIMSDKSettings(
            // Id string of this app - use C2SIMSDK.GetMachineID() to get a unique id based on the client hardware
            <submitter id>, 
            // Full C2SIM server endpoint, including host:port/path, e.g. "http://10.2.10.30:8080/C2SIMServer"
            <rest endpoint>, 
            // C2SIM server password
            <rest password>        
            // Full STOMP service endpoint, including host:port/destination, e.g. "http://10.2.10.30:61613/topic/C2SIM"
            <stomp endpoint>, 
            // Protocol - could also be "BML" for example, but the SDK focuses on C2SIM
            "SISO-STD-C2SIM",
            // Version of the protocol - v1.0.1 is the latest published version
            "1.0.2"
        )
    );
    ```

    *NOTE*: the SDK supports Dependency Injection as part of a [.NET Generic Host](https://docs.microsoft.com/en-us/dotnet/core/extensions/generic-host), which simplifies handling of logging and configuration settings. See the [Sample App](https://github.com/hyssostech/OpenC2SIM.github.io/tree/master/Software/Library/CS/C2SIMSDK/C2SIMSDKSampleApp) for an example of how to implement that.


1. Subscribe to notification events

    ```CS
    c2SimSDK.StatusChangedReceived += C2SimSDK_StatusChangedReceived;
    c2SimSDK.InitializationReceived += C2SimSDK_InitializationReceived;
    c2SimSDK.OderReceived += C2SimSDK_OderReceived;
    c2SimSDK.ReportReceived += C2SimSDK_ReportReceived;
    ```

1. Establish the connection to the C2SIM notification service to start the message flow

    ```CS
    try
    {
        // Connect to the notification service to start receiving messages
        await c2SimSDK.Connect();
    }
    catch (Exception e)
    {
        string msg = e.InnerException != null ? e.InnerException.Message : e.Message;
        // Handle error
        // ...
    }
    ```

1. Send commands and messages as required

    See the [C2SIM .NET SDK Reference](<https://github.com/hyssostech/OpenC2SIM.github.io/tree/master/Software/Library/CS/C2SIMSDK/docs/C2SIM .NET SDK Reference.docx>) for details of the methods summarized below

	1. Change the state of the server
		- ResetToInitializing() - get the server to an `Initializing` state, issuing `STOP, RESET, INITIALIZE` individual commands as needed
		- SwitchToRunning() - get the server to a `Running` state, issuing `SHARE, START` individual commands as needed 
        - JoinSession() - get the server to issue a late join notification, eventually causing an `InitializationReceived` event to be triggered 
	1. Push messages
        - PushInitializationMessage()
        - PushOrderMessage() 
        - PushReportMessage()
    1. Access to raw Library functionality
		- PushCommand()
            C2SIM server commands: STOP, RESET, INITIALIZE, SHARE, START, PAUSE, STATUS, QUERYINIT (see [C2SIM Server Message Flow](https://github.com/hyssostech/OpenC2SIM.github.io/blob/master/Software/Server/C2SIM%20Server%20Message%20Flow_20200325.pdf) for details)
            Also supports commands added in v1.0.2 of the schema: 
                RESTART, 
                GETSIMMULT, SETSIMMULT <multiple>, 
                STARTPLAY, STOPPLAY, PAUSEPLAY, GETPLAYSTAT, 
                GETPLAYMULT, SETPLAYMULT <multiple>, 
                STARTREC, STOPREC, PAUSEREC, RESTARTREC, GETRECSTAT,
                MAGIC <entityUUIDreference> <latitude> <longitude> 
        - PushMessage() - client configured XML messages

## C2SIM XSD object serialization

### Main classes

The SDK includes classes for schema versions 1.0.0/1/2. These classes were generated from the 
[Schemas](./schemas) using the `xsd` tool, distinguished by the namespace - `C2SIM.Schema100`, or `C2SIM.Schema101`, or `C2SIM.Schema102`:

```
xsd schemas\C2SIM_SMX_LOX_v1.0.0.xsd /c /l:CS /n:C2SIM.Schema100
xsd schemas\C2SIM_SMX_LOX_v1.0.1.xsd /c /l:CS /n:C2SIM.Schema101
xsd schemas\C2SIM_SMX_LOX_CWIX2024.xsd /c /l:CS /n:C2SIM.Schema102
```

### Messages returned by the server as response to commands

There are differences in the messages returned by older versions of the server (at least up to v4.8.1.1),
which accepted v1.0.0 and v1.0.1 of the standard, and the newer v1.0.2

**Older versions**

Actual responses included elements which were not present in the published 1.0.0. and 1.0.1  xsds. 
A separate augmented `C2SIM_SMX_LOX_v1.0.x_Command.xsd` is provided, that includes these missing elements:
* `SystemCommandBodyType` includes an additional element - `SessionStateCode`
* `SystemCommandTypeCodeType` includes an additional `ResetScenario` enumeration 

The class for this specific type is then generated by `xsd` using the following command:

```
xsd schemas\C2SIM_SMX_LOX_v1.0.x_Command.xsd /c /l:CS /n:C2SIM.CustomSchema
```

**Recent versions**

Version 1.0.2 introduces a breaking change, renaming `SystemCommandBodyType` to `SystemMessageBodyType`,
`SystemCommandBody` to `SystemMessageBody`,
and embedding the type choices previously defined in `SystemCommandTypeCodeType` directly into `SystemMessageBodyType`. 
The embedded choices are expanded to cover the additional recording, replay, replay speed and magic move commands.

There is no longer a need for a separate customized class, as the extraneous elements have been removed from
the responses produced by more recent version sof the server (v4.8.3.3+).

### Classes per message type

To be able to support toolsets that have not been updated to v1.0.1, the SDK opts to deliver message bodies as 
XML strings, rather than already serialized object, so that the client app can decide which version to apply.

Push messages and Notifications are expected to emply the following types respectivelly:

* Push messages expect XML strings serialized from the following object types:
    * `PushInitializationMessage` - `C2SIMInitializationBodyType`
    * `PushOrderMessage` - `OrderBodyType`
    * `PushReportMessage` - `ReportBodyType`
    XML containing `MessageBody` and `MessageBody/DomainMessageBody` elements wrapping the types above are also accepted by the
    push methods. If these elements are missing, they are automatically added 

* Notification event handlers are invoked by the SDK with XML representations of the message bodies provided as parameters:
    * `StatusChangedReceived` - `SystemCommandBodyType`
    * `InitializationReceived` - `C2SIMInitializationBodyType`
    * `OderReceived` - `OrderBodyType`
    * `ReportReceived` - `ReportBodyType`
    * A generic `C2SIMMessageReceived` notification is issued for every message. The parameter contains the unparsed content of the received message body. If the Header indicates that the message is C2SIM-compliant (rather than say CBML or other format), then it can be deserialized using a `MessageBodyType` wrapper type

For convenience, the following utility methods are provided to handle de/serialization:
* `C2SIMSDK.ToC2SIMObject<T>(string xml)` - returns an object of type `T` deserialized from the string parameter
* `C2SIMSDK.FromC2SIMObject<T>(object o)` - returns a string with XML serialized from an object of type `T`

These methods can be used to to handle the desired version of the C2SIM schema by picking the appropriate namespace:

```CSHARP
// Serialize a .NET order object into the corresponding C2SIM xml
// If using v1.0.0
string xmlOrder = C2SIMSDK.FromC2SIMObject<C2SIM.Schema100.OrderBodyType>(orderObject);
...
// If using v1.0.1
string xmlOrder = C2SIMSDK.FromC2SIMObject<C2SIM.Schema101.OrderBodyType>(orderObject);
...
// If using v1.0.2 - CWIX2024 version
string xmlOrder = C2SIMSDK.FromC2SIMObject<C2SIM.Schema102.OrderBodyType>(orderObject);
```

```CSHARP
// If using v1.0.0 or v1.0.1, SystemCommand requires the use of the CustomSchema
var xmlCommand = C2SIMSDK.FromC2SIMObject<C2SIM.CustomSchema.SystemCommandBodyType>(commandObject);
...
// If using v1.0.2, use SystemMessage (new SystemCommand name) as defined in the standard v1.0.2 schema
var xmlCommand = C2SIMSDK.FromC2SIMObject<C2SIM.Schema102.SystemMessageBodyType>(commandObject);
```

```CSHARP
// Convert received (C2SIM xml) notification body into a .NET object
// If using v1.0.0
var orderObject  = C2SIMSDK.ToC2SIMObject<C2SIM.Schema100.OrderBodyType>(e.Body);
...
// If using v1.0.1
var orderObject = C2SIMSDK.ToC2SIMObject<C2SIM.Schema101.OrderBodyType>(e.Body);
...
// If using v1.0.2 - CWIX2024 version
var orderObject = C2SIMSDK.ToC2SIMObject<C2SIM.Schema102.OrderBodyType>(e.Body);
```

```CSHARP
// If using v1.0.0 or v1.0.1, SystemCommand requires the use of the CustomSchema
var command = C2SIMSDK.ToC2SIMObject<C2SIM.CustomSchema.SystemCommandBodyType>(e.Body);
...
// If using v1.0.2, use SystemMessage (new SystemCommand name) as defined in the standard v1.0.2 schema
var command = C2SIMSDK.ToC2SIMObject<C2SIM.Schema102.SystemMessageBodyType>(e.Body);
```

## Sample app

The methods above are used in a [sample app](https://github.com/hyssostech/OpenC2SIM.github.io/tree/master/Software/Library/CS/C2SIMSDK/C2SIMSDKSampleApp), which accepts commands and messages interactively and sends them to a server.
The app also displays notifications received from a server, and can provide insight into the traffic generated by the sample itself or other apps
connected to the same server, e.g. the [Sketch-Thru-Plan Planning Workstation](http://www.hyssos.com), as can be seen in this [demo](https://vimeo.com/641689328).
