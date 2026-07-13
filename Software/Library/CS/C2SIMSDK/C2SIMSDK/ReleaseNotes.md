# C2SIM SDK for .NET Release Notes

## Version 1.4.0
* Added a test project, `C2SIMSDK.Tests`. `dotnet test` runs it with no server needed; the live server tests are skipped unless `C2SIM_TEST_REST_URL` and `C2SIM_TEST_STOMP_URL` are set
* Multiple `C2SIMSDK` instances can now coexist in one process. The underlying `C2SIMClientSTOMPLib` held its incoming message queue in a `static` field, so a second client stole the first client's frames and its `Connect()` failed with `Expected 'CONNECTED' but received MESSAGE`. Requires C2SIMClientLib v4.8.3.2
* `IsConnected()` now returns false after `Disconnect()` - `C2SIMClientSTOMPLib.Disconnect()` was setting the flag to true
* Added `ObjectInitializationReceived` event - `ObjectInitializationBody` messages were previously logged and dropped, and were only reachable via the raw `C2SIMMessageReceived` event
* Added `OrderReceived` event. The misspelled `OderReceived` is now `[Obsolete]`, but is still raised alongside `OrderReceived` and will be removed in a future release
* The `Error` event is now actually raised. Previously `OnError` had no call site, so failures in the STOMP message pump were logged but never surfaced to subscribers
* The STOMP message pump now exits cleanly on cancellation instead of treating shutdown as an error
* Fixed a `NullReferenceException` when disposing an SDK instance on which `Connect()` was never called
* `C2SIMClientRESTLib`: `_protocol` and `_protocolVersion` are no longer `static`. `BmlRequest` reassigns `_protocol` on every call while `C2SIMSDK` constructs one instance per request, so a concurrent BML request could leave a C2SIM instance without its `C2SIMHeader`
* `C2SIMClientRESTLib.GetElementValue`: removed the static document cache, which was keyed on `string.GetHashCode()` (a hash collision returned the wrong document) and was mutated from every requesting thread
* `C2SIMClientRESTLib`: uses one shared static `HttpClient` instead of creating and disposing one per call. The per-call client stranded a socket in TIME_WAIT on every request, exhausting the ephemeral port range (SocketException 10048) under high report volume. Accept headers are now set per-request, since a shared client's `DefaultRequestHeaders` are not thread-safe to mutate. Requires C2SIMClientLib v4.8.3.3

## Version 1.3.1
* Improved reporting issued when there is an unexpected message returned from the server
* Changed server response `time` property to string to accoomodate the 'nul' response returned by the server

## Version 1.3.0
* Updated to CWIX2024 schema

## Version 1.2.15
* Fixed message-selector extraction (original java code looked for deprecated header name)
* Parsing quoted paths in sample app

## Version 1.2.14
* Fixed a bug that caused STOMP connection to fail when domain names were used as the host parameter, instead of IP

## Version 1.2.13
* JoinSession() returns just the C2SIMINitializationBody xml content
* Fixed a bug on STOMP disconnection that happened when the client app did not provide a cancellation token

## Version 1.2.12
* JoinSession() now always returns content - would return null if state was "Initializing". 
If interested in restricting the state the invoker should query the status.  

## Version 1.2.11
* C2SIMHeader elements re-ordered to match changes seen on the Java Library code v4.8.3.1.
That overcomes issues seen in some instances of the server that may be configured with stricter validation.
* Setting header SendingTIme to UTC to match the Zulu format of the string representation sent to the server (was local time, as used in the Java version)

## Version 1.2.10
* Updated to C2SIM v1.0.2 schema (CWIX2023)

## Version 1.2.9
* Safer termination when client code fails to call Disconnect() before disposing

## Version 1.2.8
* Added a heart-beat option to STOMP connections to enhance the longevity of the subscriptions - defaults to server messages every 10 seconds
* Refactored the Library settings classes - C2SIMClientRESTSettings and C2SIMClientSTOMPSettings - into separate files
* Added debug logging listing content of messages and commands

## Version 1.2.7
* Fixed issue with switching connections to another server midrun (connection confirmation got swallowed by previous message pump)
* Making C2SIMSDK disposable

## Version 1.2.6
* Extended list of commands to comply with the schema version 1.0.2

## Version 1.2.5
* Injecting ILoggerFactory to simplify logger propagation when nested libraries are used
* Documented Status changed events that signal that the server is waiting for Initialization, or for Orders / Reports

## Version 1.2.4
* Improved reporting on connection error
* Fixed issue with larger message numbers in C2SIM server response object

## Version 1.2.3
*  More detailed logging
    
## Version 1.2.2
* Fixed issue with server responses, which may sometimes be just strings rather than xml d

## Version 1.2.1

* Wrapping Initialization, Order and Report messages in MessageBody if not already present
* Returning push message server results as strongly typed  objects

## Version 1.2.0

* Changes to make it possible to handle both versions of the schema - v1.0.0 and v1.0.1

## Version 1.1.0

* Updated to C2SIM schema v1.0.1

## Version 1.0.0

* Initial release