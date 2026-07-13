using System.Net.Http.Headers;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Xml.Linq;
using System.Text;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace C2SimClientLib;

/// <summary>
/// C2Sim Server Web Services REST Client
/// </summary>
/// <remarks>
/// This client does the following:
///      Open a connection with the server on specified port (Default is 8080)
///      Build an HTTP POST transaction from parameters and BML XML document
///      Submit the transaction
///      Read the result
///      Disconnect from the server
///      Return the result received from the server to the caller
/// </remarks>
public class C2SIMClientRESTLib
{
    #region Constants
    const string SISOSTD = "SISO-STD-C2SIM";
    const string XML_PREAMBLE = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    #endregion

    #region Instance variables
    private static ILogger _logger;
    private static string _clientVersion = string.Empty;
#if NET5_0_OR_GREATER
    // One shared client (was: new HttpClient() per call, which stranded a socket in
    // TIME_WAIT per report push -> ephemeral-port exhaustion / SocketException 10048 at
    // ~20x report volume). Accept headers are now PER-REQUEST (a shared client's
    // DefaultRequestHeaders are not thread-safe to mutate). PooledConnectionLifetime keeps
    // long-lived pooled connections from going stale.
    private static readonly HttpClient _httpClient =
        new HttpClient(new SocketsHttpHandler { PooledConnectionLifetime = TimeSpan.FromMinutes(2) });
#else
    // netstandard2.0: SocketsHttpHandler/PooledConnectionLifetime are unavailable; a plain
    // shared client still fixes the exhaustion (connections are pooled and reused).
    private static readonly HttpClient _httpClient = new HttpClient();
#endif
    // These must be per-instance: BmlRequest() reassigns _protocol on every call, and
    // C2SIMSDK constructs a new C2SIMClientRESTLib per request. As statics, a concurrent
    // BmlRequest("BML") could leave _protocol != SISOSTD while another instance's
    // constructor tests it, leaving that instance with a null _c2s header
    private string _protocol = string.Empty;   // C2SIM
    private string _protocolVersion;
    private string _host = "localhost";
    private string _port = "8080";
    private string _path = "C2SIMServer/c2sim";
    private string _submitter = "NotSet";
    private string _firstForwarders = string.Empty;   // C2SIM
    private string _domain;      // Not used any more.  Older client code may still set it
    private C2SIMHeader _c2s;
    #endregion

    #region Construction
    /// <summary>
    /// Constructc REST request object
    /// </summary>
    /// <param name="logger">Logger to use</param>
    /// <param name="settings">REST service settings</param>
    public C2SIMClientRESTLib(ILogger logger, C2SIMClientRESTSettings settings)
    {
        _logger = logger;

        _submitter = settings.SubmitterId;
        _host = settings.Host;
        _port = settings.Port;
        _path = settings.Path;
        _protocol = settings.Protocol; 
        _protocolVersion = settings.ProtocolVersion;
        // Instantiate C2SIM Header if protocol is C2SIM
        _c2s = null;
        if (_protocol == SISOSTD)
        {
            _c2s = new C2SIMHeader()
            {
                FromSendingSystem = _submitter,
                ToReceivingSystem = _host,
                CommunicativeActTypeCode = settings.Performative,
                Protocol = SISOSTD,
                ProtocolVersion = _protocolVersion,
            };
            _c2s.GenerateConversationID();
            _c2s.GenerateMessageID();
        }
        // Make sure we have version of ClientLib 
        if (string.IsNullOrWhiteSpace(_clientVersion))
            _clientVersion = GetVersion();
    }

    /// <summary>
    /// Constructor taking IOption, to be used with service.AddOptions()
    /// </summary>
    /// <param name="logger"></param>
    /// <param name="options"></param>
    public C2SIMClientRESTLib(ILogger logger, IOptions<C2SIMClientRESTSettings> options)
    :this(logger, options.Value)
    {
    }
    #endregion

    #region Public methods
    /// <summary>
    /// Get status of C2SIM Server. 
    /// </summary>
    /// <remarks>
    /// Confirm that server is running and return initialization statussetHost()  and setSubmitter() must 
    /// have must have been executed before calling this method.
    /// Sample output:
    /// &lt;?xml clientVersion = &quot;1.0&quot; encoding=&quot;UTF-8&quot;?&gt;
    /// &lt;result&gt;
    ///    &lt;status&gt;OK&lt;/status&gt;
    ///    &lt;message&gt;Server is operating&lt;/message&gt;
    ///    &lt;serverInitialized&gt;false&lt;/serverInitialized&gt;
    ///    &lt;sessionInitialized&gt;false&lt;/sessionInitialized&gt;
    ///    &lt;unitDatabaseName&gt;defaultDB&lt;/unitDatabaseName&gt;
    ///    &lt;unitDatabaseSize&gt;0&lt;/unitDatabaseSize&gt;
    ///    &lt;msgNumber&gt;0&lt;/msgNumber&gt;
    ///    &lt;time&gt; 0.000&lt;/time&gt;
    /// &lt;/result&gt;
    /// </remarks>
    /// <returns>>XML Document indicating current status of the server</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<string> ServerStatus()
    {
        _logger?.LogTrace("Entering method");
        Uri url;
        string result;
        try
        {
            url = new Uri(BuildC2SIMEndpoint("status"));
            using (var request = new HttpRequestMessage(HttpMethod.Get, url))
            {
                request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("text/plain"));
                using (var resp = await _httpClient.SendAsync(request))
                {
                    resp.EnsureSuccessStatusCode();
                    result = await resp.Content.ReadAsStringAsync();
                }
            }
        }
        catch (HttpRequestException e)
        {
            string emsg = "HTTP Request Exception";
            _logger?.LogError(e, emsg);
            throw new C2SIMClientException(emsg, e);
        }
        // Did we get an error from the server? (The server returns XML)
        if (result.Contains("<status>Error</status>"))
        {
            string msg = "Error received from server " + result;
            _logger?.LogError(msg);
            throw new C2SIMClientException(msg);
        }
        _logger?.LogTrace($"Result = {result}");
        return result;
    }

    /// <summary>
    /// Execute a 2SIM Server command
    /// </summary>
    /// <remarks>
    /// Current commands are NEW, LOAD, SAVE, SAVEAS, DELETE, SHARE, QUERYUNIT, QUERYINIT,
    /// MAGIC, RESTART, GETSIMMULT, SETSIMMULT, GETPLAYSTAT, PAUSEPLAY, STARTPLAY, STOPPLAY, 
    /// GETPLAYMULT, SETPLAYMULT, STARTREC, STOPREC, GETRECSTAT, PAUSEREC, RESTARTREC
    /// See the  <see href="https://github.com/OpenC2SIM/OpenC2SIM.github.io/blob/master/C2SIM%20Server%20Reference%20Implementation%20Documentation%204.8.0.X%20.pdf">C2SIM Server Reference Implementation</see> 
    /// for details.
    /// Result is an XML document which may contain a 'status' xml element
    /// </remarks>
    /// <param name="cmd">Command to be processed.  </param>
    /// <param name="parm1">Optional first parameter</param>
    /// <param name="parm2">Optional second parameter</param>
    /// <param name="parm3">Optional third parameter</param>
    /// <returns>string result - XML Document giving results of command and server status similar to serverStatus method.</returns>
    /// <exception cref="C2SIMClientException">Primary and secondary causes are transmitted within the C2SIMClientException object</exception>
    public async Task<string> C2SimCommand(string cmd, string parm1, string parm2, string parm3=null)
    {
        _logger?.LogTrace("Entering method");
        string xml = "<C2SIM_Statistics xmlns=\"http://www.sisostds.org/schemas/c2sim/1.0\"/>";
        // Make sure we have version of ClientLib 
        if (string.IsNullOrWhiteSpace(_clientVersion))
            _clientVersion = GetVersion();
        // Make sure the required parameters have been provided
        if (string.IsNullOrWhiteSpace(_submitter))
        {
            string emsg = "Error - Submitter not specified";
            _logger?.LogError(emsg);
            throw new C2SIMClientException(emsg);
        }
        if ((cmd == null) || (string.IsNullOrWhiteSpace(cmd)))
        {
            string emsg = "No command specified";
            _logger?.LogError(emsg);
            throw new C2SIMClientException(emsg);
        }
        if (parm1 == null)
            parm1 = string.Empty;
        if (parm2 == null)
            parm2 = string.Empty;
        if (parm3 == null)
            parm3 = string.Empty;
        if (_submitter == null)
            _submitter = string.Empty;
        // Build the parameter string to include
        //       try {
        string u = BuildC2SIMEndpoint("command", $"submitterID={_submitter}"
            + $"&command={cmd}"
            + $"&parm1={parm1}"
            + $"&parm2={parm2}"
            + $"&parm3={parm3}"
            + $"&version={_clientVersion}");
        string result = await SendTrans(u, xml);
        return result;
    }

    /// <summary>
    /// Submit a request to a BML/C2SIM Server
    /// </summary>
    /// <remarks>
    /// This method performs the same function as the bmlRequest method and is included as part of the migration from BML to C2SIM 
    /// </remarks>
    /// <param name="xml">The xml document being submitted</param>
    /// <returns>Indication of success of operation along with server status.  See serverStatus method.</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<string> C2SimRequest(string xml)
    {
        _logger?.LogTrace("Entering method");
        return await BmlRequest(xml);
    }

    /// <summary>
    /// Submit a BML transaction to a BML/C2SIM Server host
    /// </summary>
    /// <remarks>
    /// As a minimum setHost() and setSubmitter() must have been executed before 
    /// calling this method.
    /// If the document is C2SIM the C2SIM message envelope should not be 
    /// included it will be generated by this method
    /// </remarks>
    /// <param name="xml">An XML string containing a BML or C2SIM xml document.</param>
    /// <returns>XML - The response received from the host BML server</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<string> BmlRequest(string xml)
    {
        _logger?.LogTrace("Entering method");
        long startTime;
        long endTime;
        DateTime startDate;
        DateTime endDate;
        string header;
        string body;
        string msg = string.Empty;
        string sdf = "yyyy-MM-dd HH:mm:ss,SSS";
        // Make sure the required parameters have been provided
        if (string.IsNullOrWhiteSpace(_submitter))
        {
            string emsg = "Error - Submitter not specified";
            _logger?.LogError(emsg);
            throw new C2SIMClientException(emsg);
        }
        // Determine the protocol for this message based on the root element (MessageBody is C2SIM, else BML),
        // overriding whatever the user may have set in the constructor
        _protocol = DetermineProtocol(xml);
        string u = BuildC2SIMEndpoint("c2sim", $"submitterID={_submitter}"
            + $"&protocol={_protocol}"
            + $"&version={_clientVersion}");

        // If protocol is BML then use the message as received
        if (_protocol.Equals("BML", StringComparison.InvariantCultureIgnoreCase))
        {
            // No extra headers
            msg = xml;
        }
        else
        {
            System.Diagnostics.Debug.Assert(_protocol == "C2SIM" || _protocol.Equals(SISOSTD, StringComparison.InvariantCultureIgnoreCase));
            header = _c2s.ToXMLString();
            // Locate the message body (After the xml preamble)
            body = LocateXmlBody(xml);
            // Now build the message and frame with with C2SIM_Message
            msg = XML_PREAMBLE + "<Message xmlns=\"http://www.sisostds.org/schemas/C2SIM/1.1\">" + header + body + "</Message>";
            // C2SIM takes extra querystring parameters
            u += $"&sender={_c2s.FromSendingSystem}"
                    + $"&receiver={_c2s.ToReceivingSystem}"
                    + $"&conversationid={_c2s.ConversationID}";
        }
        // If first forwarder is set add it
        if (!string.IsNullOrWhiteSpace(_firstForwarders))
            u += $"&forwarders={_firstForwarders}";
        // Record the start time
        startTime = DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond;
        startDate = DateTime.Now;
        _logger?.LogTrace(u);
        _logger?.LogTrace(msg);
        string result = await SendTrans(u, msg);
        // If the server indicates that response time statistics should be collected, send them.
        if (GetElementValue(result, "collectResponseTime").Equals("T", StringComparison.InvariantCultureIgnoreCase))
        {
            // Record the end time for the transaction
            endTime = DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond;
            endDate = DateTime.Now;
            Double elapsedTime = (1.0 * endTime - startTime) / 1000;
            // Send the response time to the stats collector on the BML Server
            u = BuildC2SIMEndpoint("stats", "submitterID=" + _submitter);
            // Set up the xml with the response time of the first transaction
            string responseTimeResult = XML_PREAMBLE
                    + "<C2SIM_Statistics xmlns=\"http://www.sisostds.org/schemas/c2sim/1.0\">"
                    + "<REST_ResponseTime>"
                    + "<submitterID>" + _submitter + "</submitterID>"
                    + "<msgNumber>" + GetElementValue(result, "msgNumber") + "</msgNumber>"
                    + "<startTime>" + startDate.ToString(sdf) + "</startTime>"
                    + "<endTime>" + endDate.ToString(sdf) + "</endTime>"
                    + "<elapsedTime>" + elapsedTime + "</elapsedTime>"
                    + "<serverTime>" + GetElementValue(result, "time") + "</serverTime>"
                    + "</REST_ResponseTime></C2SIM_Statistics>";
            // Send the response time to be recorded on the server
            await SendTrans(u, responseTimeResult);
        }
        // Did we get an error from the server, (The server returns XML)
        if (result.Contains("<status>Error</status>"))
        {
            string emsg ="Error received from server\n" + result;
            _logger?.LogError(emsg);
            throw new C2SIMClientException(emsg);
        }
        return result;
    }

    /// <summary>
    ///   Search an xml string looking for the first instance of a tag.
    /// </summary>
    /// <remarks>
    ///   If found, return the value associated with that tag
    /// </remarks>
    /// <param name="xml">The xml string to be searched</param>
    /// <param name="target">The string (Tag) being searched for</param>
    /// <returns>The value of the element named by that tag</returns>
    public static string GetElementValue(string xml, string target)
    {
        _logger?.LogTrace("Entering method");
        if (string.IsNullOrWhiteSpace(xml))
        {
            return string.Empty;
        }
        try
        {
            // Parsed on every call rather than cached: the previous static cache was keyed on
            // xml.GetHashCode(), so a hash collision returned the wrong document, and the static
            // was mutated from every thread that issued a REST request
            XDocument xdoc = XDocument.Parse(xml);
            IEnumerable<XElement> result = xdoc.Descendants().Where(p => p.Name.LocalName == target);
            return result?.First().Value ?? string.Empty;
        }
        catch (Exception)
        {
            // Failure means it is not there
            return string.Empty;
        }
    }
    #endregion


    #region Private methods
    /// <summary>
    /// HTTP post
    /// </summary>
    /// <param name="u"></param>
    /// <param name="xml"></param>
    /// <returns></returns>
    /// <exception cref="C2SIMClientException"></exception>
    private async Task<string> SendTrans(string u, string xml)
    {
        _logger?.LogTrace("Entering method");
        string result;
        try
        {
            Uri url = new Uri(u);
            // Set up parameters to do a POST of the xml BML transaction
            using (var request = new HttpRequestMessage(HttpMethod.Post, url))
            {
                request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/xml"));
                request.Content = new StringContent(xml, Encoding.UTF8, "application/xml"); // CONTENT-TYPE
                using (HttpResponseMessage resp = await _httpClient.SendAsync(request))
                {
                    result = await resp.Content.ReadAsStringAsync();
                }
            }
        }
        catch (HttpRequestException e)
        {
            string emsg = $"Connection error: {e.Message}";
            _logger?.LogError(e, emsg);
            throw new C2SIMClientException(emsg, e);
        }
        return result;
    }

    /// <summary>
    /// Assembly version info
    /// </summary>
    /// <returns></returns>
    internal string GetVersion()
    {
        _logger?.LogTrace("Entering method");
        string ver = "UNKNOWN";
        try
        {
            ver = this.GetType()
            .GetTypeInfo()
            .Assembly
            .GetCustomAttribute<AssemblyFileVersionAttribute>()
            .Version;
        }
        catch (Exception e)
        {
            // Just log, but ignore
            _logger?.LogWarning($"Failed to extract version from assembly: {e}");
        }
        return ver;
    }

    /// <summary>
    /// Build C2SIM service request string
    /// </summary>
    /// <param name="cmdPath"></param>
    /// <param name="queryString"></param>
    /// <returns></returns>
    private string BuildC2SIMEndpoint(string cmdPath, string queryString = null)
    {
        _logger?.LogTrace("Entering method");
        string u = string.Empty;
        if (!_host.StartsWith("http", StringComparison.InvariantCultureIgnoreCase))
            u += "http://";
        u += _host;
        if (!string.IsNullOrWhiteSpace(_port))
            u += $":{_port}";
        if (!string.IsNullOrWhiteSpace(_path))
            u += $"/{_path.Trim('/')}";
        u += $"/{cmdPath}";
        if (queryString != null)
            u += $"?{queryString}";
        return u;
    }

    /// <summary>
    /// Determine the protocol code to use with a root element
    /// </summary>
    /// <param name="xml"></param>
    /// <returns></returns>
    string DetermineProtocol(string xml)
    {
        _logger?.LogTrace("Entering method");
        string temp = LocateXmlBody(xml);
        string root = string.Empty;
        string prot = string.Empty;
        // Locate the root tag accounting for possible namespace prefix.
        Regex p = new Regex("\\s*<(\\w+:)?(\\w+)");
        Match m = p.Match(temp);
        if (m.Success && m.Groups.Count > 2)
        {
            // Tag is second match.  First match is prefix: which is optional
            root = m.Groups[2].Value;
        }
        switch (root)
        {
            case "MessageBody":
                prot = _protocol;
                break;
            default:
                prot = "BML";
                break;
        }
        // Return the protocol
        return prot;
    }

    /// <summary>
    /// Locate the xml body following the xml preamble
    /// </summary>
    static string LocateXmlBody(string xml)
    {
        _logger?.LogTrace("Entering method");
        string body = string.Empty;
        // Is the XML preamble present?
        if (xml.StartsWith("<?xml version"))
        {
            // Locate the actual beginning of the xml (Find the second "<")
            int start = xml.IndexOf("<", 1);
            // Get the xml after the XML preamble
            body = xml.Substring(start);
        }
        else
            body = xml;
        return body;
    }
    #endregion


    #region Public properties
    /// <summary>
    /// Domain
    /// </summary>
    [Obsolete]
    public string Domain { get => _domain; set => this._domain = value; }

    /// <summary>
    /// Name or IP address of BML/C2SIM server host
    /// </summary>
    public string Host { get => _host; set => this._host = value; }

    /// <summary>
    /// Server TCP port number to use (defaults to 8080)
    /// </summary>
    public string Port { get => _port; set => this._port = value; }

    /// <summary>
    /// C2SIM service server path
    /// </summary>
    public string Path { get => _path; set => this._path = value; }

    /// <summary>
    /// Set the Requestor property indicating the identity of the client
    /// </summary>
    /// <remarks>
    /// This is the same as Submitter and provides compatibility with earlier versions of the library.
    /// </remarks>
    public string Requestor { get => _submitter; set => this._submitter = value; }

    /// <summary>
    /// Indicates the identity of the client
    /// </summary>
    public string Submitter { get => _submitter; set => this._submitter = value; }

    /// <summary>
    /// Indicates the first server to handle the XML document
    /// </summary>
    public string FirstForwarders { get => _firstForwarders; set => this._firstForwarders = value; }

    /// <summary>
    /// C2SIMHeader to be used with submission of C2SIM transaction
    /// </summary>
    /// <remarks>
    /// Should not normally be used as the header is automatically generated by the client code.
    /// </remarks>
    public C2SIMHeader C2SIMHeader => _c2s;

    /// <summary>
    /// Protocol to use: BML and C2SIM
    /// </summary>
    /// <exception cref="C2SIMClientException"></exception>
    public string Protocol
    {
        get => _protocol;
        set
        {
            _protocol = value;
            if (_c2s != null)
            {
                _c2s.Protocol = value;
            }
        }
    }

    /// <summary>
    /// Version of the protocol, e.g. 1.0.0 for the C2SIM published standard or 0.0.9 for pre-publication/legacy v9
    /// </summary>
    /// <exception cref="C2SIMClientException"></exception>
    public string ProtocolVersion
    {
        get => _protocolVersion;
        set
        {
            _protocolVersion = value;
            if (_c2s != null)
            {
                _c2s.ProtocolVersion = value;
            }
        }
    }
    #endregion


}

