using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace C2SIM.Tests.Fakes;

/// <summary>A single captured HTTP request</summary>
public sealed record CapturedRequest(string Method, string Target, string Body)
{
    /// <summary>Query string portion of the request target, without the leading '?'</summary>
    public string Query => Target.Contains('?') ? Target[(Target.IndexOf('?') + 1)..] : string.Empty;

    /// <summary>Path portion of the request target</summary>
    public string Path => Target.Contains('?') ? Target[..Target.IndexOf('?')] : Target;

    /// <summary>Value of a query string parameter, or null</summary>
    public string QueryParam(string name) => Query
        .Split('&', StringSplitOptions.RemoveEmptyEntries)
        .Select(kv => kv.Split('=', 2))
        .Where(kv => kv[0] == name)
        .Select(kv => kv.Length > 1 ? Uri.UnescapeDataString(kv[1]) : string.Empty)
        .FirstOrDefault();
}

/// <summary>
/// Minimal HTTP/1.1 server over a raw socket, used to capture what C2SIMClientRESTLib posts.
/// </summary>
/// <remarks>
/// Deliberately not HttpListener: registering an HttpListener prefix needs an administrative
/// netsh url acl on Windows, which would make the test suite un-runnable on a developer box.
/// Binds port 0 so tests never collide.
/// </remarks>
public sealed class FakeRestServer : IDisposable
{
    readonly TcpListener _listener;
    readonly CancellationTokenSource _cts = new();
    readonly ConcurrentQueue<CapturedRequest> _requests = new();

    /// <summary>Ephemeral port the server is listening on</summary>
    public int Port { get; }

    /// <summary>Body returned for every request</summary>
    public string ResponseBody { get; set; } =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><result><status>OK</status><message>ok</message></result>";

    /// <summary>Every request received, in arrival order</summary>
    public IReadOnlyCollection<CapturedRequest> Requests => _requests.ToArray();

    public FakeRestServer()
    {
        _listener = new TcpListener(IPAddress.Loopback, 0);
        _listener.Start();
        Port = ((IPEndPoint)_listener.LocalEndpoint).Port;
        _ = Task.Run(AcceptLoopAsync);
    }

    async Task AcceptLoopAsync()
    {
        while (!_cts.IsCancellationRequested)
        {
            TcpClient client;
            try { client = await _listener.AcceptTcpClientAsync(); }
            catch { return; }   // listener stopped
            _ = Task.Run(() => HandleAsync(client));
        }
    }

    async Task HandleAsync(TcpClient client)
    {
        using (client)
        using (NetworkStream ns = client.GetStream())
        {
            try
            {
                // Read headers (terminated by a blank line), then exactly content-length bytes.
                // Read byte-at-a-time so the body is not swallowed into a buffered reader.
                string headerText = await ReadHeadersAsync(ns);
                if (string.IsNullOrEmpty(headerText)) return;

                string[] lines = headerText.Split("\r\n", StringSplitOptions.RemoveEmptyEntries);
                string[] requestLine = lines[0].Split(' ');
                string method = requestLine[0];
                string target = requestLine.Length > 1 ? requestLine[1] : "/";

                int contentLength = lines
                    .Where(l => l.StartsWith("Content-Length:", StringComparison.OrdinalIgnoreCase))
                    .Select(l => int.Parse(l.Split(':', 2)[1].Trim()))
                    .FirstOrDefault();

                string body = string.Empty;
                if (contentLength > 0)
                {
                    var buf = new byte[contentLength];
                    int read = 0;
                    while (read < contentLength)
                    {
                        int n = await ns.ReadAsync(buf, read, contentLength - read);
                        if (n == 0) break;
                        read += n;
                    }
                    body = Encoding.UTF8.GetString(buf, 0, read);
                }

                _requests.Enqueue(new CapturedRequest(method, target, body));

                byte[] payload = Encoding.UTF8.GetBytes(ResponseBody);
                string head = "HTTP/1.1 200 OK\r\n"
                            + "Content-Type: application/xml\r\n"
                            + $"Content-Length: {payload.Length}\r\n"
                            + "Connection: close\r\n\r\n";
                byte[] headBytes = Encoding.UTF8.GetBytes(head);
                await ns.WriteAsync(headBytes, 0, headBytes.Length);
                await ns.WriteAsync(payload, 0, payload.Length);
                await ns.FlushAsync();
            }
            catch
            {
                // A malformed or aborted request is not interesting to the tests
            }
        }
    }

    static async Task<string> ReadHeadersAsync(NetworkStream ns)
    {
        var sb = new StringBuilder();
        var one = new byte[1];
        while (!sb.ToString().EndsWith("\r\n\r\n"))
        {
            int n = await ns.ReadAsync(one, 0, 1);
            if (n == 0) return sb.ToString();
            sb.Append((char)one[0]);
            if (sb.Length > 65536) break;
        }
        return sb.ToString();
    }

    public void Dispose()
    {
        _cts.Cancel();
        try { _listener.Stop(); } catch { /* test teardown */ }
        _cts.Dispose();
    }
}
