using C2SIM.Tests.Fakes;
using Microsoft.Extensions.Logging.Abstractions;
using Xunit;

namespace C2SIM.Tests;

public sealed class LifecycleAndErrorTests
{
    static C2SIMSDKSettings Unreachable() => new()
    {
        SubmitterId = "TESTS",
        RestUrl = "http://127.0.0.1:1/C2SIMServer",
        RestPassword = "pw",
        // Port 1 is never listening; Connect() is not called in these tests anyway
        StompUrl = "http://127.0.0.1:1/topic/C2SIM",
    };

    /// <summary>
    /// Regression: Dispose() guarded on _c2SimStompClient (always non-null after the ctor)
    /// but dereferenced _cancellationSource, which only Connect() creates.
    /// </summary>
    [Fact]
    public void Dispose_without_Connect_does_not_throw()
    {
        var sdk = new C2SIMSDK(NullLoggerFactory.Instance, Unreachable());
        Exception ex = Record.Exception(() => sdk.Dispose());
        Assert.Null(ex);
    }

    [Fact]
    public void Dispose_is_idempotent()
    {
        var sdk = new C2SIMSDK(NullLoggerFactory.Instance, Unreachable());
        sdk.Dispose();
        Exception ex = Record.Exception(() => sdk.Dispose());
        Assert.Null(ex);
    }

    [Fact]
    public async Task Disconnect_without_Connect_does_not_throw_on_the_cancellation_source()
    {
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, Unreachable());
        // Disconnect() reaches the STOMP client, which will fault on a dead socket and wrap
        // in C2SIMClientException. What must NOT happen is a NullReferenceException from
        // _cancellationSource.
        Exception ex = await Record.ExceptionAsync(() => sdk.Disconnect());
        Assert.IsNotType<NullReferenceException>(ex);
    }

    /// <summary>
    /// Regression: OnError had no call site, so a failure inside the message pump
    /// (very commonly, a subscriber that throws) was logged and swallowed.
    /// </summary>
    [Fact]
    public async Task Throwing_subscriber_surfaces_on_the_Error_event()
    {
        using var broker = new FakeStompBroker();
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(broker));

        var recorder = new EventRecorder(sdk);
        sdk.ReportReceived += (_, __) => throw new InvalidOperationException("boom");

        await sdk.Connect();
        await broker.ClientConnected;

        broker.SendMessage(C2SIMMessages.Report());
        await recorder.WaitForRaw(1);

        Assert.Equal(1, recorder.Errors);
        Assert.IsType<InvalidOperationException>(recorder.LastError);
        Assert.Equal("boom", recorder.LastError.Message);
    }

    /// <summary>A throwing subscriber must not take the pump down with it.</summary>
    [Fact]
    public async Task Pump_survives_a_throwing_subscriber()
    {
        using var broker = new FakeStompBroker();
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(broker));

        var recorder = new EventRecorder(sdk);
        sdk.ReportReceived += (_, __) => throw new InvalidOperationException("boom");

        await sdk.Connect();
        await broker.ClientConnected;

        broker.SendMessage(C2SIMMessages.Report());   // subscriber throws here
        broker.SendMessage(C2SIMMessages.Order());    // must still be dispatched
        await recorder.WaitForRaw(2);

        Assert.Equal(1, recorder.Errors);
        Assert.Equal(1, recorder.Order);
    }

    /// <summary>A subscriber that throws on the Error event itself must not take the pump down.</summary>
    [Fact]
    public async Task Pump_survives_a_throwing_Error_subscriber()
    {
        using var broker = new FakeStompBroker();
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(broker));

        var recorder = new EventRecorder(sdk);
        sdk.ReportReceived += (_, __) => throw new InvalidOperationException("boom");
        sdk.Error += (_, __) => throw new InvalidOperationException("error handler boom");

        await sdk.Connect();
        await broker.ClientConnected;

        broker.SendMessage(C2SIMMessages.Report());
        broker.SendMessage(C2SIMMessages.Order());
        await recorder.WaitForRaw(2);

        Assert.Equal(1, recorder.Order);
    }

    /// <summary>Shutdown is not an error - Disconnect() cancels the token before closing the socket.</summary>
    [Fact]
    public async Task Disconnect_raises_no_spurious_Error()
    {
        using var broker = new FakeStompBroker();
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(broker));

        var recorder = new EventRecorder(sdk);
        await sdk.Connect();
        await broker.ClientConnected;

        broker.SendMessage(C2SIMMessages.Order());
        await recorder.WaitForRaw(1);

        await sdk.Disconnect();
        await Task.Delay(300);

        Assert.Equal(0, recorder.Errors);
    }
}
