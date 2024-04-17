# C2SIM SDK for .NET Release Notes

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