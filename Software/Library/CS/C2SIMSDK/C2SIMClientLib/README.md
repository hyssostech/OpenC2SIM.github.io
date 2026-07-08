# C2SIM Client Library for .NET

Library providing means for client applications to interact with C2SIM environments, ported from the
[Java OpenC2SIM Client Library v4.8.0.2](https://github.com/hyssostech/OpenC2SIM.github.io/tree/master/Software/Library/Java/C2SIMClientLib)

It provides methods for performing REST endpoint calls and interact with the STOMP notification service. See the [C2SIM Server Reference Implementation Documentation 4.8.1.X](https://bit.ly/36E8Sb5) for details

Below the differences between the Java version and .NET's are described.

## Versioning

The first three components track the Java library / server generation being targeted. The fourth
component is the .NET revision and is incremented independently when this port changes.
The current version is recorded in [VERSION.md](VERSION.md) and is reported to the server as the
`version` query string parameter of every REST call.

### v4.8.3.2
* `C2SIMClientSTOMPLib`: the incoming message queue (`_queue`) is no longer `static`. A single
  `BufferBlock` was shared by every instance in the process, so a second client dequeued the first
  client's frames. Two `C2SIMSDK` instances in one process could not both connect - the second
  `Connect()` failed with `Expected 'CONNECTED' but received MESSAGE`, or messages were delivered
  to the wrong client. `_logger` is likewise per-instance now
* `C2SIMClientSTOMPLib.Disconnect()` set `IsConnected = true` on success. It now clears the flag,
  so `IsConnected` and the reconnect path in `Connect()` behave correctly
* `C2SIMClientRESTLib`: `_protocol` and `_protocolVersion` are no longer `static`. `BmlRequest` reassigns
  `_protocol` on every call while callers construct one instance per request, so a concurrent BML request
  could leave a C2SIM instance without its `C2SIMHeader`. `DetermineProtocol` is now an instance method
* `C2SIMClientRESTLib.GetElementValue`: removed the static `XDocument` cache. It was keyed on
  `string.GetHashCode()`, so a hash collision returned the wrong document, and the static was mutated
  from every thread issuing a REST request

## Promises

Promises are used throughout, to support the use of `async/await`.

In particular, `GetNext_Block()`, which as the name indicates blocked until the next STOMP message becomes available no 
longer blocks in the strict sense of the term. Clients can `await` this call, which for all purposes causes execution
of that call to be suspended until the method returns, but does not block the thread itself.

`GetNext_NoBlock(`) retains the `TryGet` style of the original, returning a null immediately if a message is not available. 
Given that `GetNext_Block(`) implements a more efficient way of suspending the execution without a thread lock penalty, there 
is no strong reason for using `GetNext_NoBlock()` in most cases.

## Constructors

`C2SIMClientREST_Lib()` and `C2SIMClientSTOMP_Lib` constructors now take parameters compatible with [Dependency Injection](https://docs.microsoft.com/en-us/dotnet/core/extensions/dependency-injection) and the [Options](https://docs.microsoft.com/en-us/dotnet/core/extensions/options)
patterns, taking an ILogger parameter, and a structured record packaging the different settings ([C2SIMClientRESTSettings](C2SIMClientRESTSettings.cs) and [C2SIMClientSTOMPSettings](C2SIMClientSTOMPSettings.cs) respectively)

## STOMP heart-beat

STOMP connection adds a "heart-beat" element that configures the STOMP Server to send (empty) heart-beat messages at a certain frequency. This reduces the opportunities for connection timeouts.

The default setting is `10000` (a message every 10 seconds), but can be changed via a [C2SIMClientSTOMPSettings](C2SIMClientSTOMPSettings.cs) parameter if desired. Setting that to zero prevents the STOMP Server from sending any heart-beat messages.

## Exceptions

C2SIMException, used to wrap exceptions, does that now by packing the wrapped exception as a standard `InnerException`. 
The Java code used a property for that. 

## Parsing of STOMP messages' C2SIM header and C2SIM body

Java's `getNext_Block/NoBlock()` return a [C2SIMSTOMPMessage](C2SIMSTOMPMessage.cs) where the `C2SIMHeader` property is always null. The `MessageBody` property
contains the XML for both the messages' C2SIM header a well as the C2SIM body. 

Code in `getNext_Block()` that would parse things out, populating the C2SIMHeader and removing the corresponding XML from MessageBody exists, 
but is in practice never executed, since it relies on a test for a "protocol" headerMap key. But headerMap is never instantiated, so the
C2SIMHeader population never takes place.

The .NET version opts to do the parsing, given that this appears to have been the original intention. This is noted here because
client code that might potentially serve as the basis for a .NET port will likely have extra code handling this parsing, and may fail because
that is already being handled in the .NET code.


## Style 

Standard .NET style was used for:

- Names of Methods, and public Properties - Capitalized first letters replacing Java's lowercase
- Getters and Setters - get_* and set_* are replaced by (initial upper cased) .NET properties

## Logging

Generic .NET (ILogger) logging was added, which can be configured to use a variety of different providers and configurations injected by the 
client making use of the library.



