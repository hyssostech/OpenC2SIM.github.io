using Microsoft.Extensions.Logging.Abstractions;
using Xunit;

namespace C2SIM.Tests;

/// <summary>
/// Smoke tests against a real C2SIM reference implementation server.
/// Skipped unless C2SIM_TEST_REST_URL / C2SIM_TEST_STOMP_URL are set - see LiveServerFactAttribute.
/// </summary>
/// <remarks>
/// These are deliberately read-only with respect to server state: they issue STATUS
/// and connect to the notification topic. They do not INITIALIZE / START / RESET,
/// because a C2SIM server is typically shared and those commands would disrupt other clients.
/// </remarks>
[Collection("LiveServer")]
public sealed class LiveServerTests
{
    [LiveServerFact]
    public async Task Server_reports_a_status()
    {
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, LiveServer.Settings());

        C2SIMSDK.C2SIMServerStatus status = await sdk.GetStatus();

        Assert.NotEqual(C2SIMSDK.C2SIMServerStatus.UNKNOWN, status);
    }

    [LiveServerFact]
    public async Task Status_command_round_trips_and_reports_the_server_version()
    {
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, LiveServer.Settings());

        string xml = await sdk.PushCommand(C2SIMSDK.C2SIMCommands.STATUS);

        var resp = C2SIMSDK.ToC2SIMObject<C2SIMServerResponse>(xml);
        Assert.True(resp.IsSuccess, $"server returned: {xml}");
        Assert.False(string.IsNullOrWhiteSpace(resp.ServerVersion));
    }

    [LiveServerFact]
    public async Task Can_connect_and_disconnect_from_the_notification_service()
    {
        using var sdk = new C2SIMSDK(NullLoggerFactory.Instance, LiveServer.Settings());
        int errors = 0;
        sdk.Error += (_, __) => Interlocked.Increment(ref errors);

        await sdk.Connect();
        Assert.True(sdk.IsConnected());

        await Task.Delay(500);
        await sdk.Disconnect();
        await Task.Delay(300);

        Assert.Equal(0, Volatile.Read(ref errors));
    }
}
