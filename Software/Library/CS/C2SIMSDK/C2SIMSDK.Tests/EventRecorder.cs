using C2SIM.Tests.Fakes;

namespace C2SIM.Tests;

/// <summary>Settings pointing the SDK at a fake broker / fake rest server</summary>
public static class TestSettings
{
    public static C2SIMSDKSettings For(FakeStompBroker broker, int restPort = 1) => new()
    {
        SubmitterId = "TESTS",
        RestUrl = $"http://127.0.0.1:{restPort}/C2SIMServer",
        RestPassword = "pw",
        StompUrl = $"http://127.0.0.1:{broker.Port}/topic/C2SIM",
        Protocol = "SISO-STD-C2SIM",
        ProtocolVersion = "1.0.2",
    };
}

/// <summary>
/// Subscribes to every C2SIMSDK notification event and counts what arrives.
/// Counters are updated from the SDK's message pump thread, hence Interlocked / volatile.
/// </summary>
public sealed class EventRecorder
{
    int _raw, _order, _oder, _objInit, _init, _report, _status, _errors;

    public int Raw => Volatile.Read(ref _raw);
    public int Order => Volatile.Read(ref _order);
    public int Oder => Volatile.Read(ref _oder);
    public int ObjectInitialization => Volatile.Read(ref _objInit);
    public int Initialization => Volatile.Read(ref _init);
    public int Report => Volatile.Read(ref _report);
    public int StatusChanged => Volatile.Read(ref _status);
    public int Errors => Volatile.Read(ref _errors);

    public volatile string LastRawBody;
    public volatile string LastObjectInitializationBody;
    public volatile Exception LastError;
    public NotificationHeader LastHeader { get; private set; }

    public EventRecorder(C2SIMSDK sdk)
    {
        sdk.C2SIMMessageReceived += (_, e) =>
        {
            LastRawBody = e.Body;
            LastHeader = e.Header;
            Interlocked.Increment(ref _raw);
        };
        sdk.OrderReceived += (_, __) => Interlocked.Increment(ref _order);
#pragma warning disable CS0618 // deliberately observing the deprecated event
        sdk.OderReceived += (_, __) => Interlocked.Increment(ref _oder);
#pragma warning restore CS0618
        sdk.ObjectInitializationReceived += (_, e) =>
        {
            LastObjectInitializationBody = e.Body;
            Interlocked.Increment(ref _objInit);
        };
        sdk.InitializationReceived += (_, __) => Interlocked.Increment(ref _init);
        sdk.ReportReceived += (_, __) => Interlocked.Increment(ref _report);
        sdk.StatusChangedReceived += (_, __) => Interlocked.Increment(ref _status);
        sdk.Error += (_, e) => { LastError = e; Interlocked.Increment(ref _errors); };
    }

    /// <summary>
    /// Wait until at least <paramref name="count"/> messages have been seen by the raw event.
    /// The raw event fires before dispatch, so callers must also allow the dispatch to complete;
    /// a short settle delay covers that.
    /// </summary>
    public async Task WaitForRaw(int count, int timeoutMs = 5000)
    {
        var deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
        while (Raw < count && DateTime.UtcNow < deadline)
        {
            await Task.Delay(20);
        }
        if (Raw < count)
        {
            throw new TimeoutException($"Expected {count} raw messages, saw {Raw} within {timeoutMs} ms");
        }
        // Raw fires first; give the switch statement below it a moment to run
        await Task.Delay(100);
    }
}
