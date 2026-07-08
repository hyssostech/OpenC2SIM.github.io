using System.Net;
using System.Net.Sockets;
using System.Text;

namespace C2SIM.Tests.Fakes;

/// <summary>
/// Minimal STOMP 1.2 broker, just enough to drive C2SIMClientSTOMPLib:
/// accepts one client, answers the CONNECT/SUBSCRIBE handshake with CONNECTED,
/// then pushes MESSAGE frames on demand.
/// </summary>
/// <remarks>
/// Binds port 0 so concurrently running tests never collide. Read <see cref="Port"/>
/// after construction to build the STOMP url.
/// </remarks>
public sealed class FakeStompBroker : IDisposable
{
    readonly TcpListener _listener;
    readonly TaskCompletionSource<bool> _clientConnected =
        new(TaskCreationOptions.RunContinuationsAsynchronously);
    TcpClient _client;
    NetworkStream _stream;

    /// <summary>Ephemeral port the broker is listening on</summary>
    public int Port { get; }

    /// <summary>Completes once the client has finished the CONNECT handshake</summary>
    public Task ClientConnected => _clientConnected.Task;

    public FakeStompBroker()
    {
        _listener = new TcpListener(IPAddress.Loopback, 0);
        _listener.Start();
        Port = ((IPEndPoint)_listener.LocalEndpoint).Port;
        _ = Task.Run(AcceptAsync);
    }

    async Task AcceptAsync()
    {
        try
        {
            _client = await _listener.AcceptTcpClientAsync();
            _stream = _client.GetStream();
            // Drain the client's CONNECT and SUBSCRIBE frames. A single read is enough:
            // C2SIMClientSTOMPLib writes both before it starts reading.
            var buf = new byte[8192];
            await _stream.ReadAsync(buf, 0, buf.Length);
            Send("CONNECTED\nversion:1.2\n\n\0\n");
            _clientConnected.TrySetResult(true);
        }
        catch (Exception e)
        {
            _clientConnected.TrySetException(e);
        }
    }

    /// <summary>Push a STOMP MESSAGE frame carrying <paramref name="xml"/> as its body.</summary>
    /// <remarks>
    /// C2SIMClientSTOMPLib reads the body line by line and concatenates without a separator,
    /// so the xml must be a single line.
    /// </remarks>
    public void SendMessage(string xml)
    {
        if (xml.Contains('\n'))
        {
            throw new ArgumentException("STOMP body must be a single line", nameof(xml));
        }
        Send("MESSAGE\n"
             + "destination:/topic/C2SIM\n"
             + $"content-length:{Encoding.UTF8.GetByteCount(xml)}\n"
             + "\n"
             + xml + "\n"
             + "\0\n");
    }

    void Send(string frame)
    {
        byte[] b = Encoding.UTF8.GetBytes(frame);
        _stream.Write(b, 0, b.Length);
        _stream.Flush();
    }

    public void Dispose()
    {
        try { _stream?.Dispose(); } catch { /* test teardown */ }
        try { _client?.Dispose(); } catch { /* test teardown */ }
        try { _listener.Stop(); } catch { /* test teardown */ }
    }
}
