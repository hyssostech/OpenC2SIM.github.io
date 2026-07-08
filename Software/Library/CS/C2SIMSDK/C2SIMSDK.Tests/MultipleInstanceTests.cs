using C2SIM.Tests.Fakes;
using Microsoft.Extensions.Logging.Abstractions;
using Xunit;

namespace C2SIM.Tests;

/// <summary>
/// Two SDK instances in one process must not see each other's traffic.
/// </summary>
/// <remarks>
/// Regression: C2SIMClientSTOMPLib held its incoming BufferBlock queue in a static field
/// ("There is only one queue (It is a static variable)"), so a second client dequeued the
/// first client's frames. The visible symptoms were a Connect() that failed with
/// "Expected 'CONNECTED' but received MESSAGE", and messages delivered to the wrong SDK.
/// This also means the whole xunit suite - which runs test classes in parallel in one
/// process - cannot pass until the queue is per-instance.
/// </remarks>
public sealed class MultipleInstanceTests
{
    /// <summary>
    /// Every message goes to B's broker. A must see none of them.
    /// </summary>
    /// <remarks>
    /// Uses a batch rather than a single message on purpose: with a shared static queue,
    /// whether A's pump or B's pump wins any one message is a race, so a single message
    /// would make this test a coin flip. Over a batch, A stealing at least one is close to certain.
    /// </remarks>
    [Fact]
    public async Task A_connected_SDK_never_sees_another_SDKs_messages()
    {
        const int batch = 10;

        using var brokerA = new FakeStompBroker();
        using var brokerB = new FakeStompBroker();
        using var sdkA = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(brokerA));
        using var sdkB = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(brokerB));

        var eventsA = new EventRecorder(sdkA);
        var eventsB = new EventRecorder(sdkB);

        await sdkA.Connect();
        await brokerA.ClientConnected;
        await sdkB.Connect();
        await brokerB.ClientConnected;

        for (int i = 0; i < batch; i++)
        {
            brokerB.SendMessage(C2SIMMessages.Report($"REP-{i}"));
        }
        await eventsB.WaitForRaw(batch);

        Assert.Equal(batch, eventsB.Report);
        Assert.Equal(0, eventsA.Raw);
    }

    /// <summary>Each SDK sees only the body type its own broker sent</summary>
    [Fact]
    public async Task Two_connected_SDKs_each_receive_only_their_own_traffic()
    {
        const int batch = 10;

        using var brokerA = new FakeStompBroker();
        using var brokerB = new FakeStompBroker();
        using var sdkA = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(brokerA));
        using var sdkB = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(brokerB));

        var eventsA = new EventRecorder(sdkA);
        var eventsB = new EventRecorder(sdkB);

        await sdkA.Connect();
        await brokerA.ClientConnected;
        await sdkB.Connect();
        await brokerB.ClientConnected;

        for (int i = 0; i < batch; i++)
        {
            brokerA.SendMessage(C2SIMMessages.Order($"ORD-{i}"));
            brokerB.SendMessage(C2SIMMessages.Report($"REP-{i}"));
        }

        await eventsA.WaitForRaw(batch);
        await eventsB.WaitForRaw(batch);

        Assert.Equal(batch, eventsA.Order);
        Assert.Equal(0, eventsA.Report);

        Assert.Equal(batch, eventsB.Report);
        Assert.Equal(0, eventsB.Order);
    }

    /// <summary>
    /// The second Connect() used to fail outright, because the first client's pump had
    /// already drained the shared queue - or worse, handed it the wrong frame.
    /// </summary>
    [Fact]
    public async Task A_second_SDK_can_connect_while_the_first_is_connected()
    {
        using var brokerA = new FakeStompBroker();
        using var brokerB = new FakeStompBroker();
        using var sdkA = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(brokerA));
        using var sdkB = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(brokerB));

        await sdkA.Connect();
        await brokerA.ClientConnected;

        Exception ex = await Record.ExceptionAsync(() => sdkB.Connect());

        Assert.Null(ex);
        Assert.True(sdkA.IsConnected());
        Assert.True(sdkB.IsConnected());
    }

    /// <summary>Disconnect must clear IsConnected - it used to set it to true.</summary>
    [Fact]
    public async Task Disconnect_clears_IsConnected()
    {
        using var broker = new FakeStompBroker();
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, TestSettings.For(broker));

        await sdk.Connect();
        await broker.ClientConnected;
        Assert.True(sdk.IsConnected());

        await sdk.Disconnect();

        Assert.False(sdk.IsConnected());
    }
}
