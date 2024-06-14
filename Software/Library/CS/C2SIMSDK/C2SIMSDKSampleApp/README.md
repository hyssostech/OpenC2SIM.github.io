# C2SIM SDK Sample App

This app illustrates the use of the [C2SIM SDK](..). It provides a simple command line interface for interacting with C2SIM servers:

- Issue commands - send `stop`, `reset`, `initialize`, `share`, `start`, `queryinit`, `status` 
- Issue v1.0.2 commands
    `restart`, 
    `getsimmult`, `setsimmult <multiple>`,
    `startplay`, `stopplay`, `pauseplay`, `getplaystat`, `getplaymult`, `setplaymult <multiple>`,
    `startrec`, `stoprec`, `pauserec`, `restartrec`, `getrecstat`,
    `magic <entityUUIDreference> <latitude> <longitude>` (the C2SIM GUI Editor v2.11.1+ implementation is currently the best source of information ont he required parameters for each of these commands)
- Push messages - `push <type> <path to xml>` - where type is "init", "order", or "report"; the XML must be properly formatted according to the 
[C2SIM standard](https://github.com/hyssostech/OpenC2SIM.github.io/tree/master/Standard)
- Observe server notifications - the STOMP message stream is displayed

To exit, enter `quit`

For a description of commands and messages, see the [C2SIM Server Reference Implementation Documentation](https://bit.ly/36E8Sb5)

*NOTE*: default logging is set to display on the Console. This may result in some clash between the app's messages and logs. Configure log to save to file instead to avoid that.

## Configuration settings

Default parameters are set in [appsettings.json](./appsettings.json), within an `C2SIM` section:

* SubmitterId - Id string of the submitter
* RestUrl - Full C2SIM server endpoint, including host:port/path, e.g. "http://10.2.10.30:8080/C2SIMServer
* RestPassword - C2SIM server password
* StompUrl - Full notification service (STOMP) endpoint, including host:port/destination, e.g. "http://10.2.10.30:dotnet run "
* Protocol - "SISO-STD-C2SIM" (or "BML")
* ProtocolVersion - "1.0.0" for published standard, or legacy version (e.g. v9="0.0.9")
* DisplayReports - Turns the display of C2SIM Report messages ON/OFF - other types of messages are still displayed.

These settings can be overridden via command line parameters (described further down)

## Building and Running

Install the [.NET 6 runtime](https://dotnet.microsoft.com/download/dotnet/6.0) for the required OS (Linux, macOS, Windows) if not yet installed

```
cd C2SIMSDKSampleApp
dotnet build
dotnet run 
```
Or using command line parameters to override the default `appsettings.json` settings:

```
dotnet run C2SIM:RestUrl="http://10.2.10.70:8080/C2SIMServer" C2SIM:DisplayReports=OFF
```

Notice that the name of the `appsettings.json` section containing the application parameters - `C2SIM` - needs to be used as a prefix, as shown in the example above