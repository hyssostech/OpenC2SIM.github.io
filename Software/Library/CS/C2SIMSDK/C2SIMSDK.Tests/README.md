# C2SIM SDK Tests

```
dotnet test C2SIMSDK.Tests\C2SIMSDK.Tests.csproj
```

No C2SIM server is required. The suite runs in about a second.

## What is covered

| File | Covers |
|---|---|
| `StompDispatchTests.cs` | Which notification event each C2SIM message body raises, driven through the real `C2SIMSDK.Connect()` message pump |
| `LifecycleAndErrorTests.cs` | `Dispose`/`Disconnect` without `Connect`, the `Error` event, pump survival when a subscriber throws |
| `MultipleInstanceTests.cs` | Two SDK instances in one process do not see each other's traffic |
| `RestLibTests.cs` | What `C2SIMClientRESTLib` actually posts: endpoint, query string, `C2SIMHeader` envelope, protocol selection, concurrency |
| `LiveServerTests.cs` | Smoke tests against a real server. Skipped unless configured - see below |

## Fakes rather than mocks

`Fakes/FakeStompBroker.cs` is a minimal STOMP 1.2 broker and `Fakes/FakeRestServer.cs` is a minimal
HTTP server, both on `TcpListener` bound to port 0. Tests therefore drive the production socket code
end to end, and never collide on a fixed port.

`FakeRestServer` deliberately avoids `HttpListener`: registering an `HttpListener` prefix needs an
administrative `netsh http add urlacl`, which would make the suite un-runnable on a normal developer
machine.

Note that `FakeStompBroker.SendMessage` requires single-line XML, because `C2SIMClientSTOMPLib` reads
a frame body line by line and concatenates the lines with no separator.

## Live server tests

Skipped by default. To run them against a C2SIM server - for example the reference implementation
container, which publishes 8080 and 61613:

```powershell
$env:C2SIM_TEST_REST_URL  = "http://127.0.0.1:8080/C2SIMServer"
$env:C2SIM_TEST_STOMP_URL = "http://127.0.0.1:61613/topic/C2SIM"
$env:C2SIM_TEST_PASSWORD  = "v0lgenau"     # optional, this is the default
dotnet test C2SIMSDK.Tests\C2SIMSDK.Tests.csproj
```

These tests are read-only with respect to server state. They issue `STATUS` and connect to the
notification topic; they do not `INITIALIZE`, `START` or `RESET`, because a C2SIM server is usually
shared and those commands would disrupt other clients.
