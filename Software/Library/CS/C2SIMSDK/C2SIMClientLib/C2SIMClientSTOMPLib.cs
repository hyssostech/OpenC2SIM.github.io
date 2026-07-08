using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks.Dataflow;
using System.Xml.Linq;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace C2SimClientLib;

/// <summary>
///  STOMP server messaging
/// </summary>
public class C2SIMClientSTOMPLib : IDisposable
{
    #region Public constants
    /// <summary>
    /// Type of message
    /// </summary>
#pragma warning disable 1591
    public enum MessageType { EMPTY, MESSAGE, CONNECTED, ERROR };
#pragma warning restore 1591
    #endregion

    #region Private constants
    static string SISOSTD = "SISO-STD-C2SIM";
    const string XML_PREAMBLE = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    const string END_OF_FRAME = "\u0000";
    #endregion

    #region Private properties
    private TcpClient _client;
    private NetworkStream _networkStream;

    /// <summary>
    /// Logger to use - injected during construction
    /// </summary>
    private ILogger _logger;

    /// <summary> 
    /// host - Name of STOMP host
    /// </summary>
    private string _host;
    /// <summary>
    /// port - The TCP port used to communicate with the 
    /// STOMP host, normal default is 61613
    /// </summary>
    private int _port;
    /// <summary>
    /// destination - The topic being used for this 
    /// STOMP session - The only one being used currently 
    /// is "/topic/C2SIM"
    /// </summary>
    private string _destination;
    /// <summary>
    /// Identifies the subscription, and associated messages and unsubscribe
    /// </summary>
    private string _subscriptionId;
    /// <summary>
    /// adv_subscriptions - These are subscriptions that can address any 
    /// header in the message using SQL-like statements.
    /// </summary>
    private List<string> _adv_subscriptions;
    /// <summary>
    /// currentMsg - Reference to the XML message with the C2SIM Header removed.   
    /// </summary>
    private C2SIMSTOMPMessage _currentMsg;
    /// <summary>
    /// Server heart beat message frequency in milliseconds
    /// </summary>
    private int _serverHeartBeat;
    /// <summary>
    /// Commands the cancellation of async operations
    /// </summary>
    private CancellationTokenSource _cancellationSource;
    /// <summary>
    /// STOP message pump task
    /// </summary>
    private Task _messagePump;
    /// <summary>
    /// Incoming STOMP message queue
    /// </summary>
    /// <remarks>
    /// One queue per connection. This used to be a static shared by every instance in the
    /// process, which meant a second client silently stole the first client's frames -
    /// including its CONNECTED handshake response
    /// </remarks>
    private readonly BufferBlock<C2SIMSTOMPMessage> _queue;
    private bool _disposedValue;
    #endregion

    #region Construction / teardown
    /// <summary>
    /// Construct a library object
    /// </summary>
    /// <param name="logger">Logger to use</param>
    /// <param name="settings">STOMP service settings</param>
    public C2SIMClientSTOMPLib(ILogger logger, C2SIMClientSTOMPSettings settings)
    {
        _logger = logger;

        _queue = new BufferBlock<C2SIMSTOMPMessage>();

        _host = settings.Host;
        _port = Int32.Parse(settings.Port);
        _destination = settings.Destination;

        _adv_subscriptions = new List<string>();
        _subscriptionId = DateTime.Now.ToFileTime().ToString();

        _serverHeartBeat = settings.ServerHeartBeat;

        IsConnected = false;
    }

    /// <summary>
    /// Constructor taking IOption, to be used with service.AddOptions()
    /// </summary>
    /// <param name="logger"></param>
    /// <param name="options"></param>
    public C2SIMClientSTOMPLib(ILogger logger, IOptions<C2SIMClientSTOMPSettings> options)
    :this(logger, options.Value)
    {
    }

    /// <summary>
    /// Dispose object
    /// </summary>
    /// <param name="disposing"></param>
    protected virtual void Dispose(bool disposing)
    {
        _logger?.LogTrace("Entering method");
        if (!_disposedValue)
        {
            if (disposing)
            {
                if (_cancellationSource != null)
                {
                    // Trigger cancellation via the common token
                    _cancellationSource.Cancel();
                }
                if (_networkStream != null)
                {
                    _networkStream.Dispose();
                }
                if (_client != null)
                {
                    _client.Dispose();
                }
            }
            // TODO: free unmanaged resources (unmanaged objects) and override finalizer
            // TODO: set large fields to null
            _disposedValue = true;
        }
    }

    // // TODO: override finalizer only if 'Dispose(bool disposing)' has code to free unmanaged resources
    // ~C2SIMClientSTOMPLib()
    // {
    //     // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
    //     Dispose(disposing: false);
    // }

    /// <summary>
    /// Dispose of object
    /// </summary>
    public void Dispose()
    {
        // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
        Dispose(disposing: true);
        GC.SuppressFinalize(this);
    }
    #endregion

    #region Public properties
    /// <summary>
    /// State of the connection to STOMP
    /// </summary>
    public bool IsConnected { get; private set; }
    #endregion

    #region Public methods
    /// <summary>
    /// Connect to Stomp host, checking for a CONNECTED response
    /// </summary>
    /// <returns>STOMPMessage - Response from host if connection - Response should be CONNECTED</returns>
    /// <exception cref="C2SIMClientException">Includes various exceptions</exception>
    public async Task<C2SIMSTOMPMessage> Connect()
    {
        _logger?.LogTrace("Entering method");
        // Clip previous connection if reconnecting
        if (IsConnected)
        {
            await Disconnect();
        }

        try
        {
            if (!IPAddress.TryParse(_host, out IPAddress ipAddress))
            {
                // Try to resolve from dns
                IPHostEntry hostEntry = Dns.GetHostEntry(_host); // the string can be an IP address
                if (hostEntry.AddressList.Length == 0)
                {
                    string emsg = "Could not resolve host '" + _host + "'";
                    _logger?.LogError(emsg);
                    throw new C2SIMClientException(emsg);
                }
                for (int h = 0; h < hostEntry.AddressList.Length; h++)
                {
                    if (hostEntry.AddressList[h].AddressFamily == AddressFamily.InterNetwork)
                    {
                        ipAddress = hostEntry.AddressList[h];
                        break;
                    }
                }
                if (ipAddress == null)
                {
                    string msg = "No IPv4 address found for " + _host;
                    _logger?.LogError(msg);
                    throw new C2SIMClientException(msg);
                }
            }

            // Create client, connect and get stream to read and write
            _logger.LogTrace($"Connecting to TCP host {ipAddress}:{_port}");
            _client = new TcpClient();
            await _client.ConnectAsync(ipAddress, _port);
            _networkStream = _client.GetStream();

            // Send connection message
            _logger.LogTrace("Sending CONNECT");
            string connectFrame = "CONNECT\n"
                    + "heart-beat: 0," + _serverHeartBeat.ToString() + "\n"
                    + "accept-version: 1.2\n"
                    + "login:\n"
                    + "passcode:\n"
                    + "\n"
                    + END_OF_FRAME;
            await SendFrame(connectFrame);

            // Send subscription
            _logger.LogTrace("Sending SUBSCRIBE");
            string subscribeFrame = "SUBSCRIBE\n";
            // Add any message selector
            if (_adv_subscriptions.Count() != 0)
            {
                subscribeFrame += "selector:";
                foreach (string adv in _adv_subscriptions)
                {
                    subscribeFrame += adv + "\n";
                }
            }
            subscribeFrame += $"destination:{_destination}\n";
            // Add message ID, blank line and null
            subscribeFrame += $"id:{_subscriptionId}\n"
                        + "\n" 
                        + END_OF_FRAME;
            // Send the SUBSCRIBE frame
            await SendFrame(subscribeFrame);
            // Start background thread so messages from host can be received.
            Start();
            // Get the response to connection request
            C2SIMSTOMPMessage resp = await GetNext_Block();
            // Are we connected?  If so return the message.  If not throw an exception
            if (resp is not null && resp.MessageType.Equals("CONNECTED"))
            {
                _logger?.LogTrace("Connected");
                IsConnected = true;
                return resp;
            }
            else
            {
                string emsg = "Expected 'CONNECTED' but received " + resp?.MessageType ?? "null";
                _logger?.LogError(emsg);
                throw new C2SIMClientException(emsg);
            }
        }
        catch (SocketException e)
        {
            string emsg = "Socket exception";
            _logger?.LogError(e, emsg);
            throw new C2SIMClientException(emsg, e);
        }
        catch (Exception e)
        {
            string emsg = "Exception";
            _logger?.LogError(e, emsg);
            throw new C2SIMClientException(emsg, e);
        }
    }

    /// <summary>
    /// Start a thread and invoke run to get the client going
    /// </summary>
    public void Start()
    {
        _logger?.LogTrace("Entering method");
        _cancellationSource = new CancellationTokenSource();
        _messagePump = Task.Run(() => Run(), _cancellationSource.Token);
    }

    /// <summary>
    /// Send message to STOMP host on an already established connection 
    /// </summary>
    /// <param name="cmd">STOMP Command to be used - should normally be MESSAGE</param>
    /// <param name="xml">The message to be sent</param>
    /// <param name="headers">A Vector Strings containing STOMP headers in the form  headerName:headerValue</param>
    /// <exception cref="C2SIMClientException">Thrown by sendFrame()</exception>
    public async Task Publish(string cmd, List<string> headers, string xml)
    {
        _logger?.LogTrace("Entering method");
        // Compute the content-length including the terminating NL and add a header
        headers.Add($"content-length:{xml.Length + 1}\n");
        headers.Add("content-type:text/plain"); // TODO: Should this be xml?
        string msg = cmd + "\n";
        foreach (string h in headers)
        {
            msg += h;
        }
        // Add blank line to mark end of headers
        msg += "\n";
        // Add message to be published  Make sure there is a terminating NL.
        msg += xml + "\n";
        // Add null to mark end of message
        msg += END_OF_FRAME;
        // Send the message
        await SendFrame(msg);
    }

    /// <summary>
    /// Returns the next message received from the STOMP messaging server.  
    /// </summary>
    /// <remarks>
    /// The calling thread will NOT be blocked if a STOMPMessage is not available - a null is immediately returned instead.
    /// Use await GetNext_Block() to interrupt execution until a message becomes available. 
    /// </remarks>
    /// <returns>STOMPMessage - The next STOMP message or NULL if no message is available at this time.  Message should be MESSAGE.</returns>
    /// <exception cref="C2SIMClientException">Encapsulates several specific exceptions</exception>
    public C2SIMSTOMPMessage GetNext_NoBlock()
    {
        _logger?.LogTrace("Entering method");
        if (_queue.TryReceive(out _currentMsg))
        {
            return ProcessSTOMPMessage(_currentMsg);
        }
        else
        {
            return null;
        }
    }

    /// <summary>
    /// Returns the message received from the STOMP messaging server.  
    /// </summary>
    /// <remarks>
    /// The original Java method blocks the calling thread. Here we make this method asynchronous
    /// so the user can wait for the result, but do that in an await that will _not_ block the thread
    /// </remarks>
    /// <returns>STOMPMessage - The next STOMP message.  Message should be MESSAGE.</returns>
    /// <exception cref="C2SIMClientException">Encapsulates several specific exceptions</exception>
    public async Task<C2SIMSTOMPMessage> GetNext_Block()
    {
        _logger?.LogTrace("Entering method");
        try
        {
            // Wait for next STOMP Message
            _currentMsg = await _queue.ReceiveAsync(_cancellationSource.Token);
        }
        catch (OperationCanceledException ie)
        {
            string emsg = "STOMP block reading cancelled";
            _logger?.LogError(emsg, ie);
            return null;
        }
        return ProcessSTOMPMessage(_currentMsg);
    }

    /// <summary>
    /// Create C2SIM header from a message content and cleans up the message body
    /// </summary>
    /// <remarks>
    /// Throw exceptions sent over as messages posted by the background thread
    /// </remarks>
    /// <param name="currentMsg"></param>
    /// <returns>Processed STOMPMessage</returns>
    /// <exception cref="C2SIMClientException">Multiple potential exceptions detected by the background thread and sent over within a message</exception>
    private C2SIMSTOMPMessage ProcessSTOMPMessage(C2SIMSTOMPMessage currentMsg)
    {
        _logger?.LogTrace("Entering method");
        // Background thread can't throw an exception as there is no caller.
        //   Check for a STOMPMsg object in the queue with something in error
        if (currentMsg.Error != null)
        {
            string emsg = "Error caught in background thread";
            _logger?.LogError(emsg, currentMsg.Error);
            throw new C2SIMClientException(emsg, currentMsg._error);
        }

        // NB: The original Java (v4.8.0.2) code parses the header and body after the message is dequeued
        // In addition, it tests for 'protocol' in the header map - that is not populated in that version
        // so in practice the code below never gets executed
        // We opt to parse the header and body before dequeing, as it is read from the STOMP stream - in Run()
        //// If this is a C2SIM Message:
        ////      Extract the original XML and return in messageBody
        ////      Extract the C2SIM information and build a CWIXHeader
        ////      Add the CWIXHeader to the currentMsg
        //if (currentMsg._headerMap.ContainsKey("protocol") &&
        //    (currentMsg._headerMap["protocol"].Equals(SISOSTD, StringComparison.InvariantCultureIgnoreCase)))
        //{
        //    // Fill out a new C2SIM Header with message
        //    C2SIMHeader c2s = C2SIMHeader.PopulateC2SIM(currentMsg.MessageBody);
        //    currentMsg._c2sim = c2s;
        //    // Remove C2SIM header and trailer
        //    string xml = C2SIMHeader.RemoveC2SIM(currentMsg.MessageBody);
        //    currentMsg._messageBody = xml;
        //    currentMsg._messageLength = xml.Length;
        //}
        // Return the STOMPMessage object to the caller
        return currentMsg;
    }

    /// <summary>
    /// Send a C2SIM Ack response to an incoming C2SIM request
    /// </summary>
    /// <returns>STOMPMessage - The next STOMP message.  Message should be MESSAGE.</returns>
    /// <param name="oldMsg">Message that is being responded to</param>
    /// <param name="c2sResp">Response code to be sent</param>
    /// <param name="ackCode">Code describing the acknowledgment</param>
    public async Task SendC2SIM_Response(
        C2SIMSTOMPMessage oldMsg,
        string c2sResp,
        string ackCode)
    {
        _logger?.LogTrace("Entering method");
        C2SIMHeader c2s;
        C2SIMHeader oldc2s;
        string xml = string.Empty;
        string header = string.Empty;
        List<string> headers = new List<string>();
        string msg = string.Empty;
        if (oldMsg._headerMap["protocol"].Equals(SISOSTD, StringComparison.InvariantCultureIgnoreCase))
        {
            oldc2s = oldMsg._c2sim;
            c2s = new C2SIMHeader();
            // Use the conversationID from the incoming message
            c2s._conversationID = oldc2s._conversationID;
            // Set performative for this message
            c2s._communicativeActTypeCode = oldc2s._communicativeActTypeCode;
            // inReplyTo is the request message;
            c2s._inReplyToMessageID = oldc2s._messageID;
            // Swap sender and receiver from inoming message
            c2s._fromSendingSystem = oldc2s._toReceivingSystem;
            c2s._toReceivingSystem = oldc2s._fromSendingSystem;
            // Convert header to xml
            header = c2s.ToXMLString();
            // Build the acknowledgment
            xml = "<MessageBody><AcknowledgementBody><AcknowledgementTypeCode>" +
                ackCode + "</AcknowledgementTypeCode></AcknowledgementBody></MessageBody>";
            headers.Add("protocol:C2SIM");
            // Build the full message
            msg = XML_PREAMBLE +
                "<Message xmlns=\"http://www.sisostds.org/schemas/c2sim/1.0\">" +
                header + xml + "</Message>";
        }
        // Build Vector of headers for this message
        // Use most headers from incoming message
        headers.Add("destination:" + oldMsg.GetHeader("destination") + "\n");
        headers.Add("content-type:text/plain\n");
        headers.Add("submitter:" + oldMsg.GetHeader("submitterID") + "\n");
        headers.Add("message-time:" + oldMsg.GetHeader("msgTime") + "\n");
        headers.Add("message-type:" + oldMsg.GetHeader("msgType") + "\n");
        headers.Add("message-number:" + oldMsg.GetHeader("msgNumber") + "\n");
        headers.Add("conversationid:" + oldMsg.GetHeader("conversationID") + "\n");
        headers.Add("protocol" + SISOSTD + "\n");
        // Publish the message    
        await Publish("SEND", headers, msg);
    }

    /// <summary>
    /// Add a general selector expression to be used with SUBSCRIBE
    /// </summary>
    /// <remarks>
    ///   Host will only publish messages matching one of the selectors.
    ///   If no addSubscriptions are submitted then all messages will be received.
    /// </remarks>
    /// <param name="subString">string - Expression to be added to subscription list.  Expression will provide a header value to be used as a filter. 
    ///  If specified  the only messages
    ///  that will be received on the current connection will be those Satisfying the expression or those msgSelectors specified in addSubscription.
    ///  If no subscriptions are submitted then this system will receive all messages published to the topic
    ///  </param>
    public void AddAdvSubscription(string subString)
    {
        _logger?.LogTrace("Entering method");
        _adv_subscriptions.Add(subString);
    }

    /// <summary>
    /// Disconnect from STOMP server and close client
    /// </summary>
    /// <returns>string - "OK" indicating successful completion of disconnect or else throws an exception</returns>
    /// <exception cref="C2SIMClientException">Encapsulates various exceptions</exception>
    public async Task<string> Disconnect()
    {
        _logger?.LogTrace("Entering method");
        string disconnectFrame = "DISCONNECT\n"
                    + "\n"
                    + END_OF_FRAME;
        try
        {
            await SendFrame (disconnectFrame);
            _client.Close();
        }
        catch (Exception e)
        {
            string emsg = "Exception thrown in call to disconnect";
            _logger?.LogError(e, emsg);
            throw new C2SIMClientException(emsg, e);
        }
        // Terminate the message pump
        if (_cancellationSource != null)
        {
            // Trigger cancellation via the common token
            _cancellationSource.Cancel();
        }
        // Disconnect was successful return OK
        _logger?.LogTrace("Disconnected");
        IsConnected = false;
        return "OK";
    }
    #endregion

    #region Private methods
    /// <summary>
    /// Send frame on current client
    /// </summary>
    private async Task SendFrame(string data)
    {
        _logger?.LogTrace("Entering method");
        try
        {
            // Use leaveOpen so the next frames can be sent over the networkStream
            // NB: Encoding.UTF8 causes a Byte Order Mark to be added, which STOMP does not like
            // use new UTF8Encoding(false) to avoid that
            // https://stackoverflow.com/a/52187936
            using (var streamWriter = new StreamWriter(_networkStream, new UTF8Encoding(false), 1024, leaveOpen: true))
            {
                streamWriter.AutoFlush = true;
                await streamWriter.WriteAsync(data);
            }
        }
        catch (Exception e)
        {
            string emsg = "Exception while sending frame";
            _logger?.LogError(e, emsg);
            throw new C2SIMClientException(emsg, e);
        }
    }

    /// <summary>
    /// STOMP server message pump
    /// </summary>
    /// <remarks>
    /// Read command
    ///     Only commands we should get are CONNECTED and MESSAGE
    /// Read headers, one per line
    ///     Blank line makes end of headers
    /// Get content-length header and extract the length
    ///     Use content-length to read message content
    ///     Read NULL NL marking end of message
    /// Build STOMPMessage object and add
    ///     Command
    ///     Message headers (Vector)
    ///     Message content as single string
    /// Add STOMPMessage to queue for background processing
    /// </remarks>
    private async Task Run()
    {
        _logger?.LogTrace("Entering method");
        // Main Foreground Loop 
        //     Read messages from the STOMP server
        //     Process the message building a STOMPMessage object
        //     Add each message to a thread safe queue
        // Message structure
        //     Command nl
        //     Multiple headers as header_name:header_value nl
        //     nl  (Blank line)
        //     Multiple lines of text
        //     Null (0x0)
        //     If content-length header is present if will provided the length of the text
        //     This code was tested with STOMP 1.2 on Apache-Apollo server and assumes that
        //         the content length header will be present in all MESSAGE messages
        // Loop forever
        using (StreamReader clientReader = new StreamReader(_networkStream, Encoding.UTF8, detectEncodingFromByteOrderMarks: false, 1024, leaveOpen: true))
        {
            while (!_cancellationSource.Token.IsCancellationRequested && ! clientReader.EndOfStream)
            {
                C2SIMSTOMPMessage msg = new C2SIMSTOMPMessage();
                try
                {
                    // Read command - there may be an extra '\0' preceding it
                    MessageType msgType = MessageType.EMPTY;
                    while (!clientReader.EndOfStream)
                    {
                        string cmd = await clientReader.ReadLineAsync();
                        if (cmd == "\0")
                        {
                            continue;
                        }
                        // Set message type to the first non-null line
                        if (Enum.TryParse<MessageType>(cmd, ignoreCase: true, out msgType))
                        {
                            msg._messageType = cmd.ToUpperInvariant();
                        }
                        else if (string.IsNullOrWhiteSpace(cmd))
                        {
                            msg._messageType = "EMPTY";
                        }
                        else
                        {
                            msg._messageType = $"INVALID {cmd}";
                        }
                        break;
                    }

                    // content-length header may not have been used (e.g. CONNECTED message)  
                    //  Provide a default value of 0 indicating no message body
                    long contentLength = 0L;
                    // Read the STOMP message headers into a Vector and look for a content-length header
                    // Headers are terminated by a blank line
                    string line;
                    while (!clientReader.EndOfStream)
                    {
                        line = await clientReader.ReadLineAsync();
                        if (string.IsNullOrWhiteSpace(line))
                        {
                            break;
                        }
                        msg.AddHeader(line);
                        // Is this a content-length header?
                        if (line.StartsWith("content-length"))
                        {
                            string cL = line.Substring(line.IndexOf(":") + 1);
                            contentLength = long.Parse(cL);
                        }
                    }
                    // Skip empty heart-beat messages
                    if (msgType == MessageType.EMPTY && contentLength == 0)
                    {
                        continue;
                    }

                    // Use the value from content-length header (or default value if we didn't find one)
                    msg._contentLength = contentLength;
                    msg._messageLength = contentLength;          // This may be modified later if this is a C2SIM message

                    // Populate the header map
                    // NOTE: the original Java code (v4.8.0.2) does not do that, and the map remains empty throughout
                    msg.CreateHeaderMap();

                    // Read message content if any
                    // NB: what ends up being read into MessageBody is the full content of the message, including
                    // a (C2SIM) MessageHeader and a MessageBody
                    string msgContent = string.Empty;
                    if (contentLength > 0)
                    {
                        while (!clientReader.EndOfStream)
                        {
                            line = await clientReader.ReadLineAsync();
                            if (string.IsNullOrWhiteSpace(line) || line == "\0")
                            {
                                break;
                            }
                            msgContent += line;
                        }
                    }

                    // NB: The original Java (v4.8.0.2) code places the full message content, including the header
                    // into _messageBody
                    // The C2SIMHeader would then be populated in post processing as part of GetNext_No/Block()
                    // That parsing depends on the header map though, that as noted above is not populated in Java
                    // The result is that the content including C2SIM header and message body stays lumped in _messageBody
                    // and the C2SIMHeader property remains null
                    // We opt in .Net to get the message fixed here before it is posted
                    if (!string.IsNullOrWhiteSpace(msgContent))
                    {
                        XElement xm = XElement.Parse(msgContent);
                        XNamespace ns = "http://www.sisostds.org/schemas/C2SIM/1.1";
                        XElement mh = xm.Descendants(ns + "C2SIMHeader").FirstOrDefault();
                        if (mh != null)
                        {
                            // This is a C2SIM message - extract header and body
                            msg._c2sim = C2SIMHeader.PopulateC2SIM(mh?.ToString());
                            XElement mb = xm.Descendants(ns + "MessageBody").FirstOrDefault();
                            msg._messageBody = mb != null ? mb.ToString() : string.Empty;
                        }
                        else
                        {
                            // Other type of message - load the full content into _messageBody for someone else to parse
                            msg._messageBody = msgContent;
                        }
                    }

                    // Turn an ERROR message into an exception that will be wrapped below
                    if (msgType == MessageType.ERROR)
                    {
                        // Find server message in headers
                        const string ErrorHeader = "message:";
                        string em = msg._headers.Where(p => p.StartsWith(ErrorHeader))?.First().Substring(ErrorHeader.Length);
                        string emsg = $"Server exception {em ?? string.Empty}";
                        throw new ApplicationException(emsg);
                    }
                    else
                    {
                        // Add the message to the queue
                        _logger?.LogTrace("Adding message to the queue");
                        await _queue.SendAsync(msg);
                    }
                }
                catch (Exception e)
                {
                    // Exception thrown.  Can't just throw or pass back as we are in a thread and there is no caller.
                    // Add the exception to an otherwise blank STOMPMesaege and add it to the queue
                    C2SIMSTOMPMessage sm = new C2SIMSTOMPMessage();
                    sm._error = e;
                    _logger?.LogError(e , "Error in STOMP message pump");
                    await _queue.SendAsync(sm);
                    // Exit loop if there was a communication error - the connection is likely dead at this point
                    if (e.InnerException is SocketException)
                        return;
                }
            }
            _logger?.LogTrace("Exiting STOMP message pump");
            await _queue.SendAsync(new C2SIMSTOMPMessage() { _messageBody = "Disconnected from STOMP" });
        }
    }
    #endregion

    #region Public properties
    /// <summary>
    ///  STOMP server port
    /// </summary>
    public int Port { get => _port; set => _port = value; }

    /// <summary>
    /// STOMP server host name or IP address
    /// </summary>
    public string Host { get => _host; set => _host = value; }

    /// <summary>
    /// Destination queue or topic
    /// </summary>
    /// <remarks>
    ///  If there is a trailing slash it will be removed.
    /// </remarks>
    public string Destination { set => _destination = value.Trim('/'); }
    #endregion
}
