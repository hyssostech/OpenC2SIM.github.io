using Xunit;

namespace C2SIM.Tests;

/// <summary>
/// A Fact that runs only when a live C2SIM server has been configured, so the
/// suite stays green on a machine (or CI agent) with no server.
/// </summary>
/// <remarks>
/// Set both environment variables to enable, for example against the reference
/// implementation container:
/// <code>
/// C2SIM_TEST_REST_URL  = http://127.0.0.1:8080/C2SIMServer
/// C2SIM_TEST_STOMP_URL = http://127.0.0.1:61613/topic/C2SIM
/// C2SIM_TEST_PASSWORD  = v0lgenau      (optional, defaults to v0lgenau)
/// </code>
/// </remarks>
public sealed class LiveServerFactAttribute : FactAttribute
{
    public LiveServerFactAttribute()
    {
        if (string.IsNullOrWhiteSpace(LiveServer.RestUrl) || string.IsNullOrWhiteSpace(LiveServer.StompUrl))
        {
            Skip = "Live C2SIM server not configured - set C2SIM_TEST_REST_URL and C2SIM_TEST_STOMP_URL";
        }
    }
}

/// <summary>Live server connection details, read from the environment</summary>
public static class LiveServer
{
    public static string RestUrl => Environment.GetEnvironmentVariable("C2SIM_TEST_REST_URL");
    public static string StompUrl => Environment.GetEnvironmentVariable("C2SIM_TEST_STOMP_URL");
    public static string Password => Environment.GetEnvironmentVariable("C2SIM_TEST_PASSWORD") ?? "v0lgenau";

    public static C2SIMSDKSettings Settings(string submitterId = "C2SIMSDK.Tests") => new()
    {
        SubmitterId = submitterId,
        RestUrl = RestUrl,
        RestPassword = Password,
        StompUrl = StompUrl,
        Protocol = "SISO-STD-C2SIM",
        ProtocolVersion = "CWIX2024v1.0.2",
    };
}
