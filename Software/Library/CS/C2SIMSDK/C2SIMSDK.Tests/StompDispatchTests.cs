using C2SIM.Tests.Fakes;
using Microsoft.Extensions.Logging.Abstractions;
using Xunit;

namespace C2SIM.Tests;

/// <summary>
/// Drives the real STOMP message pump inside C2SIMSDK.Connect() via a fake broker,
/// and asserts which notification events each C2SIM message body raises.
/// </summary>
public sealed class StompDispatchTests : IAsyncLifetime
{
    FakeStompBroker _broker;
    C2SIMSDK _sdk;
    EventRecorder _events;

    public async Task InitializeAsync()
    {
        _broker = new FakeStompBroker();
        _sdk = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(_broker));
        _events = new EventRecorder(_sdk);
        await _sdk.Connect();
        await _broker.ClientConnected;
    }

    public Task DisposeAsync()
    {
        _sdk?.Dispose();
        _broker?.Dispose();
        return Task.CompletedTask;
    }

    [Fact]
    public async Task OrderBody_raises_OrderReceived()
    {
        _broker.SendMessage(C2SIMMessages.Order());
        await _events.WaitForRaw(1);

        Assert.Equal(1, _events.Order);
        Assert.Equal(0, _events.Report);
        Assert.Equal(0, _events.Initialization);
    }

    /// <summary>
    /// OderReceived is a deprecated misspelling. It must keep firing alongside
    /// OrderReceived until it is removed, or existing clients go silent.
    /// </summary>
    [Fact]
    public async Task OrderBody_also_raises_deprecated_OderReceived()
    {
        _broker.SendMessage(C2SIMMessages.Order());
        await _events.WaitForRaw(1);

        Assert.Equal(1, _events.Order);
        Assert.Equal(1, _events.Oder);
    }

    /// <summary>
    /// Regression: ObjectInitializationBody used to fall into the dispatcher's default
    /// arm and be logged and dropped. c2simVRF supplies Routes in this body.
    /// </summary>
    [Fact]
    public async Task ObjectInitializationBody_raises_ObjectInitializationReceived()
    {
        _broker.SendMessage(C2SIMMessages.ObjectInitialization());
        await _events.WaitForRaw(1);

        Assert.Equal(1, _events.ObjectInitialization);
        Assert.Equal(0, _events.Initialization);
        Assert.Contains("<Route>", _events.LastObjectInitializationBody);
    }

    [Fact]
    public async Task C2SIMInitializationBody_raises_InitializationReceived()
    {
        _broker.SendMessage(C2SIMMessages.Initialization());
        await _events.WaitForRaw(1);

        Assert.Equal(1, _events.Initialization);
        Assert.Equal(0, _events.ObjectInitialization);
    }

    [Fact]
    public async Task ReportBody_raises_ReportReceived()
    {
        _broker.SendMessage(C2SIMMessages.Report());
        await _events.WaitForRaw(1);

        Assert.Equal(1, _events.Report);
    }

    [Fact]
    public async Task SystemMessageBody_raises_StatusChangedReceived()
    {
        _broker.SendMessage(C2SIMMessages.SystemMessage());
        await _events.WaitForRaw(1);

        Assert.Equal(1, _events.StatusChanged);
    }

    [Fact]
    public async Task Every_message_raises_the_raw_C2SIMMessageReceived()
    {
        _broker.SendMessage(C2SIMMessages.Order());
        _broker.SendMessage(C2SIMMessages.Report());
        _broker.SendMessage(C2SIMMessages.SystemAcknowledgement());
        await _events.WaitForRaw(3);

        Assert.Equal(3, _events.Raw);
    }

    [Theory]
    [InlineData("SystemAcknowledgementBody")]
    [InlineData("PlanBody")]
    public async Task Unhandled_bodies_are_ignored_without_error(string which)
    {
        string msg = which == "PlanBody" ? C2SIMMessages.Plan() : C2SIMMessages.SystemAcknowledgement();
        _broker.SendMessage(msg);
        await _events.WaitForRaw(1);

        Assert.Equal(1, _events.Raw);
        Assert.Equal(0, _events.Order + _events.Report + _events.Initialization
                        + _events.ObjectInitialization + _events.StatusChanged);
        Assert.Equal(0, _events.Errors);
    }

    [Fact]
    public async Task Header_is_exposed_separately_from_the_body()
    {
        // The C++/Java lib left the C2SIMHeader lumped into MessageBody. The .NET port splits them.
        _broker.SendMessage(C2SIMMessages.Order());
        await _events.WaitForRaw(1);

        Assert.Equal("TESTS", _events.LastHeader?.FromSendingSystem);
        Assert.DoesNotContain("C2SIMHeader", _events.LastRawBody);
    }
}
