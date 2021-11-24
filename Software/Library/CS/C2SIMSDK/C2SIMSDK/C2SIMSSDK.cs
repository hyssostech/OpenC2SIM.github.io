﻿using System.Xml.Linq;
using System.Net.NetworkInformation;
using System.Text;
using System.Xml.Serialization;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using C2SimClientLib;

namespace C2SIM;

/// <summary>
/// Methods and events for interacting with a C2SIM environment, issuing commands and messages, and receiving notifications
/// </summary>
public class C2SIMSDK : IC2SIMSDK
{
    #region Public constants
    /// <summary>
    /// Commands accepted by the C2SIM server
    /// </summary>
#pragma warning disable 1591
    public enum C2SIMCommands { STOP, RESET, INITIALIZE, SHARE, START, PAUSE, STATUS, QUERYINIT };
#pragma warning restore 1591
    /// <summary>
    /// Server status
    /// </summary>
#pragma warning disable 1591
    public enum C2SIMServerStatus { UNKNOWN, UNINITIALIZED, INITIALIZING, INITIALIZED, RUNNING, PAUSED }
#pragma warning restore 1591
    #endregion

    #region Private properties
    private string _submitterId;
    private Uri _restUri;
    private string _password;
    private string _protocol;
    private string _protocolVersion;
    private Uri _stompUri;

    private C2SIMClientSTOMPLib _c2SimStompClient;
    private CancellationTokenSource _cancellationSource;

    internal static ILogger _logger;
    #endregion

    #region Construction / teardown
    /// <summary>
    /// Construct SDK object from IOptions - configured 
    /// </summary>
    /// <param name="logger">Logger to use</param>
    /// <param name="options">Configuration settings wrapped in IOptions</param>
    public C2SIMSDK(ILogger<C2SIMSDK> logger, IOptions<C2SIMSDKSettings> options)
    :this(logger, options.Value)
    {
    }
    
    /// <summary>
    /// Construct SDK object
    /// </summary>
    /// <param name="logger">Logger to use</param>
    /// <param name="settings">Configuration settings</param>
    public C2SIMSDK(ILogger<C2SIMSDK> logger, C2SIMSDKSettings settings)
    {
        _logger = logger;

        _submitterId = settings.SubmitterId;
        _restUri = new Uri(settings.RestUrl);
        _password = settings.RestPassword;

        _stompUri = new Uri(settings.StompUrl);

        string stompHost = _stompUri.Host;
        // Add scheme prefix if not an IP
        if (Uri.CheckHostName(_stompUri.Host) != UriHostNameType.IPv4 && Uri.CheckHostName(_stompUri.Host) != UriHostNameType.IPv6)
        {
            stompHost = _stompUri.Scheme + "://" + stompHost;
        }
        _c2SimStompClient = new C2SIMClientSTOMPLib(
            _logger,
            new C2SIMClientSTOMPSettings(
                stompHost,
                _stompUri.Port.ToString(),
                _stompUri.PathAndQuery
            )
        );

        _protocol = settings.Protocol;
        _protocolVersion = settings.ProtocolVersion;
    }
    #endregion

    #region Public properties
    /// <summary>
    /// C2SIM REST services endpoint
    /// </summary>
    public string RestEndpoint => _restUri.ToString();

    /// <summary>
    /// C2SIM Notification (STOMP) service endpoint
    /// </summary>
    public string StompEndpoint => _stompUri.ToString();

    /// <summary>
    /// C2SIM protocol (C2SIM, BML)
    /// </summary>
    public string Protocol => _protocol;

    /// <summary>
    /// Version of the protocol (1.0.0, 0.0.9, ...)
    /// </summary>
    public string ProtocolVersion => _protocolVersion;
    #endregion

    #region Public events
    /// <summary>
    /// Triggered when a Command message is received, signaling a change in the server status 
    /// </summary>
    /// <remarks>
    /// Event Body contains a serialized SystemCommandBodyType. To deserialize:
    /// <code>
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.CustomSchema.SystemCommandBodyType&gt;(e.Body);
    /// </code>
    /// </remarks>
    public event EventHandler<C2SIMNotificationEventParams> StatusChangedReceived;

    /// <summary>
    /// Triggered when an Initialization message is received - provides serialized C2SIMInitializationBodyType content
    /// </summary>
    /// <remarks>
    /// Event Body contains a serialized C2SIMInitializationBodyType. To deserialize:
    /// <code>
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.Schema100.C2SIMInitializationBodyType&gt;(e.Body);
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.Schema101.C2SIMInitializationBodyType&gt;(e.Body);
    /// </code>
    /// </remarks>
    public event EventHandler<C2SIMNotificationEventParams> InitializationReceived;

    /// <summary>
    /// Triggered when an Order message is received - provides serialized OrderBodyType content
    /// </summary>
    /// <remarks>
    /// Event Body contains a serialized OrderBodyType. To deserialize:
    /// <code>
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.Schema100.OrderBodyType&gt;(e.Body);
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.Schema101.OrderBodyType&gt;(e.Body);
    /// </code>
    /// </remarks>
    public event EventHandler<C2SIMNotificationEventParams> OderReceived;

    /// <summary>
    /// Triggered when a Report message is received - provides serialized ReportBodyType content
    /// </summary>
    /// <remarks>
    /// Event Body contains a serialized ReportBodyType. To deserialize:
    /// <code>
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.Schema100.ReportBodyType&gt;(e.Body);
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.Schema101.ReportBodyType&gt;(e.Body);
    /// </code>
    /// </remarks>
    public event EventHandler<C2SIMNotificationEventParams> ReportReceived;

    /// <summary>
    /// Triggered for every message received - provides unparsed serialized MessageBodyType content
    /// </summary>
    /// <remarks>
    /// Messages in formats other than C2SIM, for example CBML, can be contained in the body.
    /// If the Header shows it is a C2SIM message, then the content is a serialized MessageBodyType.
    /// To deserialize:
    /// <code>
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.Schema100.MessageBodyType&gt;(e.Body);
    /// var body = C2SIMSDK.ToC2SIMObject&lt;C2SIM.Schema101.MessageBodyType&gt;(e.Body);
    /// </code>
    /// </remarks>
    public event EventHandler<C2SIMNotificationEventParams> C2SIMMessageReceived;

    /// <summary>
    /// Triggered when an SDK processing error occurs
    /// </summary>
    public event EventHandler<Exception> Error;

    /// <summary>
    /// Notification parameter: Header and Body of the message
    /// </summary>
    public class C2SIMNotificationEventParams
    {
        /// <summary>
        /// C2SIM standard message header
        /// </summary>
        public NotificationHeader Header { get; set; }

        /// <summary>
        /// Message body
        /// </summary>
        /// <remarks>
        /// Can be one of the following depending on the type of message:
        /// SystemCommandBodyType, C2SIMInitializationBodyType, OrderBodyType, ReportBodyType
        /// </remarks>
        public string Body { get; set; }

        /// <summary>
        /// Construct a notification parameter object
        /// </summary>
        /// <param name="header"></param>
        /// <param name="body"></param>
        public C2SIMNotificationEventParams(NotificationHeader header, string body)
        {
            Header = header;
            Body = body;
        }
    }
    #endregion

    #region Public C2SIM REST methods
    /// <summary>
    /// Reset the server to a state where it accepts initialization messages
    /// </summary>
    /// <exception cref="C2SIMClientException">Thrown if there is a failure during the setup or if the final state is not Initializing</exception>
    public async Task ResetToInitializing()
    {
        // Send stop + reset + initialize
        var status = await GetStatus();
        if (status == C2SIMServerStatus.INITIALIZING)
        {
            // Already in the desired state
            return;
        }
        // Stop may fail if not in Running or Paused, but we just ignore the error
        await PushCommand(C2SIMCommands.STOP);
        await PushCommand(C2SIMCommands.RESET);
        await PushCommand(C2SIMCommands.INITIALIZE);
        await AssertStatus(C2SIMServerStatus.INITIALIZING);
    }

    /// <summary>
    /// Set the server to a state where it accepts Order/Report messages
    /// </summary>
    /// <exception cref="C2SIMClientException">Thrown if there is a failure during the setup or if the final state is not Running</exception>
    public async Task SwitchToRunning()
    {
        var status = await GetStatus();
        if (status == C2SIMServerStatus.RUNNING)
        {
            // Already in the desired state
            return;
        }
        // Send share + start
        var resp = await PushCommand(C2SIMCommands.SHARE);
        if (! resp.IsSuccess || await GetStatus() != C2SIMServerStatus.INITIALIZED)
        {
            // Abort with the response produced by Share - likely indicating that no initialization data was present
            throw new C2SIMClientException($"Failed to SHARE: {resp.Message}");
        }
        await PushCommand(C2SIMCommands.START);
        // Assert will fail if attempting to transition when no initialization data was present
        await AssertStatus(C2SIMServerStatus.RUNNING);
    }

    /// <summary>
    /// Request to join a potentially ongoing session, where initialization messages have already been published
    /// </summary>
    /// <remarks>
    /// The C2SIM Initialize message content is returned indirectly via an InitializationReceived notification
    /// </remarks>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<C2SIMServerResponse> JoinSession()
    {
        // What is the payload returned?
        if (await GetStatus() == C2SIMServerStatus.INITIALIZING)
        {
            // Nothing to do - no initialization published yet
            return null;
        }
        // Issue command to get already published initialization
        return await PushCommand(C2SIMCommands.QUERYINIT);
    }

    /// <summary>
    /// Get the current server status
    /// </summary>
    /// <returns>Server status - UNKNOWN, UNINITIALIZED, INITIALIZING, INITIALIZED, RUNNING, PAUSED</returns>
    /// <exception cref="C2SIMClientException">Thrown if unable to retrieve the status from a server response</exception>
    public async Task<C2SIMServerStatus> GetStatus()
    {
        /*
        <?xml version="1.0" encoding="UTF-8"?>
            <result>
                    <status>OK</status>
                    <message>Server is up</message>
                    <serverInitialized>true</serverInitialized>
                    <serverVersion>4.8.0.11</serverVersion>
                    <sessionState>UNINITIALIZED</sessionState>
                    <unitDatabaseName>default</unitDatabaseName>
                    <unitDatabaseSize>0</unitDatabaseSize>
                    <msgNumber>140</msgNumber>
                    <time> 0.000</time>
                    <collectResponseTime>T</collectResponseTime>
            </result>
        */
        // Get the current status
        C2SIMServerResponse resp = await PushCommand(C2SIMCommands.STATUS);
        if (resp.Status != C2SIMServerResponse.ResponseStatus.OK ||  !Enum.TryParse<C2SIMServerStatus>(resp.SessionState, out C2SIMServerStatus status))
        {
            string emsg = $"Failed to get the server's status: {resp.Message}";
            _logger?.LogError(emsg + " " + resp.ToString());
            throw new C2SIMClientException(emsg);
        }
        return status;
    }

    /// <summary>
    /// Checks that the current server status is the expected one
    /// </summary>
    /// <param name="postCondition">Expected server status</param>
    /// <exception cref="C2SIMClientException">If current status does not match the post condition</exception>
    public async Task AssertStatus(C2SIMServerStatus postCondition)
    {
        var status = await GetStatus();
        if (status != postCondition)
        {
            string emsg = $"Server status {status} does not match the required post condition {postCondition}";
            _logger?.LogWarning(emsg);
            throw new C2SIMClientException(emsg);
        }
    }

    /// <summary>
    /// Send an Initialization message to the server
    /// </summary>
    /// <param name="xmlMessage">Serialized C2SIMInitializationBodyType or full MessageBodyType</param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<C2SIMServerResponse> PushInitializationMessage(string xmlMessage)
    {
        _logger?.LogTrace($"Pushing C2SIM Initialization message");
        return await PushMessage(WrappedMessage(xmlMessage), "INFORM");
    }

    /// <summary>
    /// Send an Order message to the server
    /// </summary>
    /// <param name="xmlMessage">Serialized OrderBodyType or full MessageBodyType</param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<C2SIMServerResponse> PushOrderMessage(string xmlMessage)
    {
        _logger?.LogTrace($"Pushing C2SIM Order message");
        return await PushMessage(WrappedMessage(xmlMessage), "ORDER");
    }

    /// <summary>
    /// Send a Report message to the server
    /// </summary>
    /// <param name="xmlMessage">Serialized ReportBodyType or full MessageBodyType</param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<C2SIMServerResponse> PushReportMessage(string xmlMessage)
    {
        _logger?.LogTrace($"Pushing C2SIM Report message");
        return  await PushMessage(WrappedMessage(xmlMessage), "REPORT");
    }

    /// <summary>
    /// Add MesageBody and DomainMessageBody wrappers around C2SIMInitializationBody, OrderBody or ReportBody if needed
    /// </summary>
    /// <param name="xmlMessage">Message that may need to be wrapped</param>
    /// <returns>Message that is guaranteed to be wrapped in a MessageBody element</returns>
    /// <exception cref="ArgumentException">Message could not be parsed or did not contain a C2SIMInitializationBody, OrderBody or ReportBody root</exception>
    private string WrappedMessage(string xmlMessage)
    {
        XNamespace ns = "http://www.sisostds.org/schemas/C2SIM/1.1";
        XElement root = XElement.Parse(xmlMessage);
        if (root?.Name.LocalName == "MessageBody")
        {
            // Nothing to do - already wrapped
            return xmlMessage;
        }
        if (root?.Name.LocalName == "OrderBody" || root?.Name.LocalName == "ReportBody")
        {
            // Add Domain and Message wrappers
            root = new XElement("MessageBody", new XElement("DomainMessageBody", root));
            return root.ToString();
        }
        else if (root?.Name.LocalName == "C2SIMInitializationBody" || root?.Name.LocalName == "DomainMessageBody")
        {
            // Add just a Message wrapper
            root = new XElement("MessageBody", root);
            return root.ToString();
        }
        throw new ArgumentException("Expected C2SIM message with MessageBody, C2SIMInitializationBody, OrderBody or ReportBody");
    }

    /// <summary>
    /// Send a message to the server
    /// </summary>
    /// <remarks>
    /// This is a generic version of the specialized PushInitializeMessage,
    /// PushOrderMessage and PushReportMessage, which should be preferred
    /// </remarks>
    /// <param name="xmlMessage">Serialized MessageBodyType</param>
    /// <param name="performative">INFORM, ORDER, REPORT - need to match the type of xmlMessage - Initialization, Order, or Report</param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<C2SIMServerResponse> PushMessage(string xmlMessage, string performative)
    {
        _logger?.LogTrace($"Pushing C2SIM message");
        var c2SimRestClient = CreateClientRestService(performative);
        try
        {
            string resp = await c2SimRestClient.C2SimRequest(xmlMessage);
            return ToC2SIMObject<C2SIMServerResponse>(resp);
        }
        catch (C2SIMClientException e)
        {
            _logger?.LogError($"Error pushing message: {e}");
            throw;
        }
    }

    /// <summary>
    /// Issue a command
    /// </summary>
    /// <param name="command"></param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task<C2SIMServerResponse> PushCommand(C2SIMSDK.C2SIMCommands command)
    {
        _logger?.LogTrace($"Pushing C2SIM command {command}");
        var c2SimRestClient = CreateClientRestService("INFORM");
        string resp = null;
        try
        {
            resp = await c2SimRestClient.C2SimCommand(command.ToString(), _password, "");
        }
        catch (C2SIMClientException e)
        {
            _logger?.LogError($"Error pushing {command} command: {e}");
        }
        return ToC2SIMObject<C2SIMServerResponse>(resp);
    }
    #endregion

    #region Public notification service (STOMP) methods
    /// <summary>
    /// Connect to a STOMP server
    /// </summary>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task Connect()
    {
        try
        {
            // TODO: the following can be found in the C2SIM GUI app, but using it results in no messages being received
            // The default parameter in the GUI is set to ALL, and these are therefore not added
            //_c2SimStompClient.AddAdvSubscription("protocol = 'C2SIM'"); // "protocol = 'BML'"
            // Connect to the server
            C2SIMSTOMPMessage resp = await _c2SimStompClient.Connect();
            if (resp is null)
            {
                throw new C2SIMClientException("STOMP connection returned and empty response");
            }
            // Start to process messages
            _cancellationSource = new CancellationTokenSource();
            Task _stompMessagePump = Task.Run(async () =>
            {
                while (!_cancellationSource.Token.IsCancellationRequested)
                {
                    try
                    {
                        var stompMsg = await _c2SimStompClient.GetNext_Block();
                        if (stompMsg != null && !string.IsNullOrWhiteSpace(stompMsg.MessageBody))
                        {
                            // Expose the C2SIM header as an SDK object to avoid client having to add another reference just for this
                            NotificationHeader header = new NotificationHeader(stompMsg.C2SIMHeader);

                            // Notify subscribers interested in the raw message
                            OnC2SIMMessageReceived(new C2SIMNotificationEventParams(header, stompMsg.MessageBody));

                            // Parse message and dispatch specific events: status changes, initializations, orders, reports 
                            // From C2SIM_SMX_LOX_v1.0.0.xsd:
                            // <xs:complexType name="MessageBodyType">
                            //	<xs:choice>
                            //		<xs:element ref="C2SIMInitializationBody"/>
                            //		<xs:element ref="DomainMessageBody"/> 
                            //            <xs:choice>
                            //                <xs:element ref="AcknowledgementBody"/>
                            //                <xs:element ref="OrderBody"/>
                            //                <xs:element ref="PlanBody"/>
                            //                <xs:element ref="ReportBody"/>
                            //                <xs:element ref="RequestBody"/>
                            //            </xs:choice>
                            //		<xs:element ref="ObjectInitializationBody"/>
                            //		<xs:element ref="SystemAcknowledgementBody"/>
                            //		<xs:element ref="SystemCommandBody"/>
                            //      ReportBody is also a type, even though it is not listed in C2SIM_SMX_LOX_v1.0.0.xsd
                            //	</xs:choice>
                            //</xs:complexType>
                            // Get the name of the first element - that is what tells us what kind of message we got
                            XNamespace ns = "http://www.sisostds.org/schemas/C2SIM/1.1";
                            XElement mb = XElement.Parse(stompMsg.MessageBody);
                            if (mb?.Name.LocalName != "MessageBody")
                            {
                                // Look for nested element
                                mb = mb?.Descendants(ns + "MessageBody").FirstOrDefault();
                            }
                            if (mb is null)
                            {
                                // TODO: Should this throw an exception instead?
                                _logger?.LogWarning($"Could not find MessageBody in notification message: {stompMsg.MessageBody}. Ignoring");
                                continue;
                            }
                            XElement bodyElement = mb.FirstNode as XElement;
                            string bodyName = bodyElement?.Name.LocalName.ToString();
                            switch (bodyName)
                            {
                                case "SystemCommandBody":
                                    OnStatusChangeReceived(new C2SIMNotificationEventParams(header, bodyElement.ToString()));
                                    break;
                                case "C2SIMInitializationBody":
                                    OnInitializationReceived(new C2SIMNotificationEventParams(header,bodyElement.ToString()));
                                    break;
                                case "DomainMessageBody":
                                    // The actual body is the next node down
                                    bodyElement = bodyElement.FirstNode as XElement;
                                    bodyName = bodyElement?.Name.LocalName.ToString();
                                    switch (bodyName)
                                    {
                                        case "OrderBody":
                                            OnOderReceived(new C2SIMNotificationEventParams(header, bodyElement.ToString()));
                                            break;
                                        case "ReportBody":
                                            OnReportReceived(new C2SIMNotificationEventParams(header, bodyElement.ToString()));
                                            break;
                                        default:
                                            _logger?.LogWarning($"Ignoring C2SIM notification message with DomainMessageBody item of type {bodyName ?? "empty name"}");
                                            break;
                                    }
                                    break;
                                default:
                                    // Ignore others - ObjectInitializationBody (IBML9?) and SystemAcknowledgementBody, perhaps others
                                    _logger?.LogInformation($"Ignoring C2SIM {bodyName ?? "empty name"} notification message");
                                    break;
                            }
                        }
                    }
                    catch (Exception e)
                    {
                        // May result from message types we are not interested in, so just log
                        string emsg = $"Error processing notification {e.Message}";
                        _logger?.LogWarning(emsg + " " + e.ToString());
                    }
                }
            }, _cancellationSource.Token);
        }
        catch (TaskCanceledException)
        {
            // Throws in the course of app shutdown
        }
        catch (Exception e)
        {
            string emsg = $"Error connecting to notification service {e.Message}";
            _logger?.LogError(emsg + " " + e.ToString());
            throw new C2SIMClientException(emsg, e);
        }
    }
    /// <summary>
    /// Disconnect from the notification service (STOMP)
    /// </summary>
    /// <exception cref="C2SIMClientException"></exception>
    public async Task Disconnect()
    {
        try
        {
            // Cancel the message pump
            _cancellationSource.Cancel();
            // Disconnect from server
            await _c2SimStompClient.Disconnect();
        }
        catch (Exception e)
        {
            string emsg = $"Error disconnecting from STOMP {e.Message}";
            _logger?.LogError(emsg + " " + e.ToString());
            throw new C2SIMClientException(emsg, e);
        }
    }

    /// <summary>
    /// Publish a message to STOMP
    /// </summary>
    /// <remarks>
    /// Low level control over the server
    /// Use with care, as this may interfere with the state that is set by the C2SIM procedures
    /// embedded in the library
    /// See <see  href="https://stomp.github.io/stomp-specification-1.2.html#Connecting">for additional details</see>
    /// SEND, SUBSCRIBE, UNSUBSCRIBE, BEGIN, COMMIT, ABORT, ACK,NACK, DISCONNECT
    /// </remarks>
    /// <param name="cmd"></param>
    /// <param name="headers"></param>
    /// <param name="xml"></param>
    /// <returns></returns>
    public async Task StompPublish(string cmd, List<string> headers, string xml)
    {
        // TODO: what commands are supported?? headeres?? Make this more high level
        await _c2SimStompClient.Publish(cmd, headers, xml);
    }

    /// <summary>
    /// Deserialize object T from xml string
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <param name="xml"></param>
    /// <returns></returns>
    public static T ToC2SIMObject<T>(string xml)
    {
        if (string.IsNullOrWhiteSpace(xml))
        {
            return default(T);
        }
        try
        {
            XmlSerializer serializer = new XmlSerializer(typeof(T));
            using (TextReader reader = new StringReader(xml))
            {
                T obj = (T)serializer.Deserialize(reader);
                return obj;
            }
        }
        catch (System.Exception e)
        {
            _logger.LogError($"Failed to deserialize xml to type {typeof(T).ToString()}: {e.Message}", e);
            throw;
        }
    }
    #endregion

    #region Public utility methods
    /// <summary>
    /// Serialize object T to xml string
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <param name="obj"></param>
    /// <returns></returns>
    public static string FromC2SIMObject<T>(T obj)
    {
        try
        {
            XmlSerializer xmlSerializer = new XmlSerializer(typeof(T));
            // Write to text, UTF8-encoded
            using (Utf8StringWriter textWriter = new Utf8StringWriter())
            {
                xmlSerializer.Serialize(textWriter, obj);
                return textWriter.ToString();
            }
        }
        catch (System.Exception e)
        {
            _logger.LogError($"Failed to serialize obj type {typeof(T).ToString()} to xml string: {e.Message}", e);
            throw;
        }
    }

    /// <summary>
    /// Force encoding on a writer - the Encoding property is readonly so requires <see href="https://stackoverflow.com/a/3862106">this trick</see>
    /// </summary>
    private class Utf8StringWriter : StringWriter
    {
        public override Encoding Encoding => Encoding.UTF8;
    }


    /// <summary>
    /// Get a unique id for a machine
    /// </summary>
    /// <returns>Unique Id (MAC address-based)</returns>
    public static string GetMachineID()
    {
        string macAddress = null;
        try
        {
            List<System.Net.NetworkInformation.NetworkInterface> nics =
                new List<NetworkInterface>(NetworkInterface.GetAllNetworkInterfaces());
            foreach (NetworkInterface nic in nics)
            {
                string thisMacAddress = "";
                if (null == nic.GetPhysicalAddress() || !nic.OperationalStatus.Equals(OperationalStatus.Up))
                    continue;
                byte[] bytes = nic.GetPhysicalAddress().GetAddressBytes();
                for (int i = 0; i < bytes.Length; i++)
                    thisMacAddress += string.Format("{0:X2}{1}", bytes[i],
                            (i < bytes.Length - 1) ? "-" : "");
                if (thisMacAddress.CompareTo(macAddress) > 0)
                    macAddress = thisMacAddress;
            }
        }
        catch (Exception)
        {
            macAddress = null;
        }
        return macAddress;
    }

    /// <summary>
    /// Get the original / innermost exception wrapped within the C2SIMClientException
    /// </summary>
    /// <remarks>
    /// The actual original exception may be nested within multiple Exception layers
    /// </remarks>
    /// <returns>Original exception</returns>
    public static Exception GetRootException(Exception e)
    {
        // Drill down to the innermost (original/root) exception
        while (e.InnerException != null)
        {
            e = e.InnerException;
        }
        return e;
    }
    #endregion

    #region Protected handler invokers
    // Overridable event invokers in case this class gets extended
    // Super-class events cannot be invoked directly by sub-classes - error CS0070
    // Call base.Onxxxx() or override these instead
    // See https://docs.microsoft.com/en-us/dotnet/csharp/programming-guide/events/how-to-raise-base-class-events-in-derived-classes

    /// <summary>
    /// Command message received - indicates server status changes
    /// </summary>
    protected void OnStatusChangeReceived(C2SIMNotificationEventParams e)
    {
        StatusChangedReceived?.Invoke(this, e);
    }

    /// <summary>
    /// Initialization message received
    /// </summary>
    protected void OnInitializationReceived(C2SIMNotificationEventParams e)
    {
        InitializationReceived?.Invoke(this, e);
    }

    /// <summary>
    /// Order message received
    /// </summary>
    protected void OnOderReceived(C2SIMNotificationEventParams e)
    {
        OderReceived?.Invoke(this, e);
    }

    /// <summary>
    /// Report message received
    /// </summary>
    protected void OnReportReceived(C2SIMNotificationEventParams e)
    {
        ReportReceived?.Invoke(this, e);
    }

    /// <summary>
    /// Message received - provides raw (unparsed) XML for every message
    /// </summary>
    protected void OnC2SIMMessageReceived(C2SIMNotificationEventParams e)
    {
        C2SIMMessageReceived?.Invoke(this, e);
    }

    /// <summary>
    /// Error was issued
    /// </summary>
    protected void OnError(Exception e)
    {
        Error?.Invoke(this, e);
    }
    #endregion

    #region Private methods
    /// <summary>
    /// Creates a new rest endpoint service object supporting a specific type of message performative
    /// </summary>
    /// <param name="performative">"INFORM - initializations; ORDER - orders; REPORT - reports</param>
    /// <returns></returns>
    private C2SIMClientRESTLib CreateClientRestService(string performative)
    {
        return new C2SIMClientRESTLib(
            _logger,
            new C2SIMClientRESTSettings(
                _submitterId,
                $"{_restUri.Scheme}://{_restUri.Host}",
                _restUri.Port.ToString(),
                _restUri.PathAndQuery,
                performative,
                _protocol,
                 _protocolVersion
            )
        );
    }
    #endregion
}
