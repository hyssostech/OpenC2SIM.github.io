using C2SIM.Tests.Fakes;
using C2SimClientLib;
using Microsoft.Extensions.Logging.Abstractions;
using Xunit;

namespace C2SIM.Tests;

/// <summary>
/// Exercises C2SIMClientRESTLib against a fake HTTP server and inspects what it actually posted.
/// </summary>
public sealed class RestLibTests : IDisposable
{
    readonly FakeRestServer _server = new();

    const string Siso = "SISO-STD-C2SIM";

    /// <summary>A C2SIM document: root element is MessageBody, so DetermineProtocol keeps the configured protocol</summary>
    static string MessageBodyDoc(string id = "ORD-1") =>
        $"<MessageBody xmlns=\"{C2SIMMessages.Ns}\"><DomainMessageBody><OrderBody><OrderID>{id}</OrderID></OrderBody></DomainMessageBody></MessageBody>";

    C2SIMClientRESTLib NewClient(string protocol) => new(
        NullLogger.Instance,
        new C2SIMClientRESTSettings("TESTS", "http://127.0.0.1", _server.Port.ToString(), "C2SIMServer",
                                    "INFORM", protocol, "1.0.2"));

    public void Dispose() => _server.Dispose();

    [Fact]
    public async Task C2SIM_push_wraps_the_body_in_a_C2SIMHeader_envelope()
    {
        C2SIMClientRESTLib client = NewClient(Siso);

        await client.BmlRequest(MessageBodyDoc());

        CapturedRequest req = Assert.Single(_server.Requests);
        Assert.Equal("POST", req.Method);
        Assert.Equal("/C2SIMServer/c2sim", req.Path);
        Assert.Equal(Siso, req.QueryParam("protocol"));
        Assert.Contains("<C2SIMHeader", req.Body);
        Assert.Contains("<OrderID>ORD-1</OrderID>", req.Body);
    }

    [Fact]
    public async Task BML_push_sends_the_raw_document_with_no_C2SIM_header()
    {
        C2SIMClientRESTLib client = NewClient("BML");

        await client.BmlRequest(C2SIMMessages.BmlDocument);

        CapturedRequest req = Assert.Single(_server.Requests);
        Assert.Equal("BML", req.QueryParam("protocol"));
        Assert.DoesNotContain("C2SIMHeader", req.Body);
    }

    /// <summary>
    /// Regression: _protocol and _protocolVersion were static while C2SIMSDK constructs one
    /// C2SIMClientRESTLib per request. Constructing a BML client used to overwrite the shared
    /// _protocol, so an already-constructed C2SIM client would then take the BML branch of
    /// BmlRequest and post its document with no C2SIMHeader and protocol=BML - silently.
    /// </summary>
    [Fact]
    public async Task Constructing_a_BML_client_does_not_corrupt_an_existing_C2SIM_client()
    {
        C2SIMClientRESTLib c2sim = NewClient(Siso);
        C2SIMClientRESTLib bml = NewClient("BML");     // used to poison the shared static
        Assert.NotNull(bml);

        await c2sim.BmlRequest(MessageBodyDoc());

        CapturedRequest req = Assert.Single(_server.Requests);
        Assert.Equal(Siso, req.QueryParam("protocol"));
        Assert.Contains("<C2SIMHeader", req.Body);
    }

    /// <summary>
    /// The mirror of the above: a BML client must keep sending BML after a C2SIM client is built.
    /// Under the static field this threw NullReferenceException, because the BML client's _c2s
    /// header is null while the shared _protocol had been flipped back to C2SIM.
    /// </summary>
    [Fact]
    public async Task Constructing_a_C2SIM_client_does_not_corrupt_an_existing_BML_client()
    {
        C2SIMClientRESTLib bml = NewClient("BML");
        C2SIMClientRESTLib c2sim = NewClient(Siso);
        Assert.NotNull(c2sim);

        Exception ex = await Record.ExceptionAsync(() => bml.BmlRequest(MessageBodyDoc()));

        Assert.Null(ex);
        CapturedRequest req = Assert.Single(_server.Requests);
        Assert.Equal("BML", req.QueryParam("protocol"));
    }

    /// <summary>
    /// C2SIMSDK creates a client per push and the SDK is used from a report timer thread,
    /// so concurrent pushes are the normal case, not an exotic one.
    /// </summary>
    [Fact]
    public async Task Concurrent_pushes_each_keep_their_own_protocol()
    {
        const int n = 24;
        var tasks = new List<Task>();
        for (int i = 0; i < n; i++)
        {
            int k = i;
            tasks.Add(Task.Run(async () =>
            {
                if (k % 2 == 0)
                {
                    await NewClient(Siso).BmlRequest(MessageBodyDoc($"ORD-{k}"));
                }
                else
                {
                    await NewClient("BML").BmlRequest(C2SIMMessages.BmlDocument);
                }
            }));
        }
        await Task.WhenAll(tasks);

        List<CapturedRequest> reqs = _server.Requests.ToList();
        Assert.Equal(n, reqs.Count);

        List<CapturedRequest> c2simReqs = reqs.Where(r => r.QueryParam("protocol") == Siso).ToList();
        List<CapturedRequest> bmlReqs = reqs.Where(r => r.QueryParam("protocol") == "BML").ToList();

        Assert.Equal(n / 2, c2simReqs.Count);
        Assert.Equal(n / 2, bmlReqs.Count);
        Assert.All(c2simReqs, r => Assert.Contains("<C2SIMHeader", r.Body));
        Assert.All(bmlReqs, r => Assert.DoesNotContain("C2SIMHeader", r.Body));
    }

    [Fact]
    public async Task Server_error_response_raises_C2SIMClientException()
    {
        _server.ResponseBody = "<result><status>Error</status><message>nope</message></result>";
        C2SIMClientRESTLib client = NewClient(Siso);

        await Assert.ThrowsAsync<C2SIMClientException>(() => client.BmlRequest(MessageBodyDoc()));
    }

    [Fact]
    public async Task C2SimCommand_posts_to_the_command_endpoint_with_its_parameters()
    {
        C2SIMClientRESTLib client = NewClient(Siso);

        await client.C2SimCommand("MAGIC", "uuid-1", "12.5", "-3.25");

        CapturedRequest req = Assert.Single(_server.Requests);
        Assert.Equal("/C2SIMServer/command", req.Path);
        Assert.Equal("MAGIC", req.QueryParam("command"));
        Assert.Equal("uuid-1", req.QueryParam("parm1"));
        Assert.Equal("12.5", req.QueryParam("parm2"));
        Assert.Equal("-3.25", req.QueryParam("parm3"));
        Assert.Equal("TESTS", req.QueryParam("submitterID"));
    }
}

/// <summary>
/// GetElementValue is public and static. It used to memoise the parsed XDocument in a static
/// field keyed on string.GetHashCode(), which is both racy and wrong on a hash collision.
/// </summary>
public sealed class GetElementValueTests
{
    [Fact]
    public void Returns_the_named_element_value()
    {
        string xml = "<result><status>OK</status><msgNumber>7</msgNumber></result>";
        Assert.Equal("OK", C2SIMClientRESTLib.GetElementValue(xml, "status"));
        Assert.Equal("7", C2SIMClientRESTLib.GetElementValue(xml, "msgNumber"));
    }

    [Fact]
    public void Returns_empty_for_a_missing_element()
    {
        string xml = "<result><status>OK</status></result>";
        Assert.Equal(string.Empty, C2SIMClientRESTLib.GetElementValue(xml, "collectResponseTime"));
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData("   ")]
    [InlineData("not xml at all")]
    public void Returns_empty_for_unusable_input(string xml)
    {
        Assert.Equal(string.Empty, C2SIMClientRESTLib.GetElementValue(xml, "status"));
    }

    [Fact]
    public void Interleaved_documents_do_not_bleed_into_each_other()
    {
        string a = "<result><status>OK</status></result>";
        string b = "<result><status>Error</status></result>";
        Assert.Equal("OK", C2SIMClientRESTLib.GetElementValue(a, "status"));
        Assert.Equal("Error", C2SIMClientRESTLib.GetElementValue(b, "status"));
        Assert.Equal("OK", C2SIMClientRESTLib.GetElementValue(a, "status"));
    }

    /// <summary>
    /// With the static cache, _cachedXDoc and _cachedXDocHash were written non-atomically
    /// from every calling thread, so a parallel sweep would occasionally read one document's
    /// hash with another document's content.
    /// </summary>
    [Fact]
    public void Parallel_callers_each_get_their_own_document()
    {
        const int n = 500;
        var results = new string[n];

        Parallel.For(0, n, i =>
        {
            string xml = $"<result><status>S{i}</status></result>";
            results[i] = C2SIMClientRESTLib.GetElementValue(xml, "status");
        });

        for (int i = 0; i < n; i++)
        {
            Assert.Equal($"S{i}", results[i]);
        }
    }
}
