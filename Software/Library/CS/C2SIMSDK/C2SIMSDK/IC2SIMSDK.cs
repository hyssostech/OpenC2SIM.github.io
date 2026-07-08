namespace C2SIM;

/// <summary>
/// C2SIMSDK Interface
/// </summary>
public interface IC2SIMSDK
{
    /// <summary>
    /// C2SIM REST endpoint
    /// </summary>
    string RestEndpoint { get; }
    /// <summary>
    /// C2SIM notification (STOMP) endpoint
    /// </summary>
    string StompEndpoint { get; }
    /// <summary>
    /// Protocol 
    /// </summary>
    string Protocol { get; }
    /// <summary>
    /// Protocol version
    /// </summary>
    string ProtocolVersion { get; }
    /// <summary>
    /// Server changed status
    /// </summary>
    event EventHandler<C2SIMSDK.C2SIMNotificationEventParams> StatusChangedReceived;
    /// <summary>
    /// Initializaiton message received
    /// </summary>
    event EventHandler<C2SIMSDK.C2SIMNotificationEventParams> InitializationReceived;
    /// <summary>
    /// ObjectInitialization message received
    /// </summary>
    event EventHandler<C2SIMSDK.C2SIMNotificationEventParams> ObjectInitializationReceived;
    /// <summary>
    /// Order message received
    /// </summary>
    [Obsolete("Misspelled. Use OrderReceived instead. This member is raised in addition to OrderReceived and will be removed in a future release.")]
    event EventHandler<C2SIMSDK.C2SIMNotificationEventParams> OderReceived;
    /// <summary>
    /// Order message received
    /// </summary>
    event EventHandler<C2SIMSDK.C2SIMNotificationEventParams> OrderReceived;
    /// <summary>
    /// Report message received
    /// </summary>
    event EventHandler<C2SIMSDK.C2SIMNotificationEventParams> ReportReceived;
    /// <summary>
    /// Provides raw XML for all (unparsed) received messages 
    /// </summary>
    event EventHandler<C2SIMSDK.C2SIMNotificationEventParams> C2SIMMessageReceived;
    /// <summary>
    /// Error notification received
    /// </summary>
    event EventHandler<Exception> Error;

    /// <summary>
    /// Checks that the current server status is the expected one
    /// </summary>
    /// <param name="postCondition">Expected server status</param>
    Task AssertStatus(C2SIMSDK.C2SIMServerStatus postCondition);
    /// <summary>
    /// Connect to a STOMP server
    /// </summary>
    Task Connect();
    /// <summary>
    /// Disconnect from the notification service (STOMP)
    /// </summary>
    Task Disconnect();
    /// <summary>
    /// Get the current server status
    /// </summary>
    Task<C2SIMSDK.C2SIMServerStatus> GetStatus();
    /// <summary>
    /// Join a potentially ongoing session, where initialization messages have already been published
    /// </summary>
    /// <returns>Null if no initialization was shared, or the C2SIM Initialize message content</returns>
    Task<string> JoinSession();
    /// <summary>
    /// Issue a command
    /// </summary>
    /// <param name="command"></param>
    /// <param name="tokens">Parameter array</param>
    /// <returns>Server response - formats vary depending on the command</returns>
    /// <exception cref="C2SimClientLib.C2SIMClientException"></exception>
    Task<string> PushCommand(C2SIMSDK.C2SIMCommands command, string[] tokens);
    /// <summary>
    /// Issue a command
    /// </summary>
    /// <param name="command"></param>
    /// <param name="parm1">Optional parameter - varies depending on command</param>
    /// <param name="parm2">Optional parameter - varies depending on command</param>
    /// <param name="parm3">Optional parameter - varies depending on command</param>
    /// <returns>Server response - formats vary depending on the command</returns>
    /// <exception cref="C2SimClientLib.C2SIMClientException"></exception>
    Task<string> PushCommand(C2SIMSDK.C2SIMCommands command, string parm1 = null, string parm2 = null, string parm3 = null);
    /// <summary>
    /// Send an Initialization message to the server
    /// </summary>
    /// <param name="xmlMessage">C2SIM message to send - formatted according to the standard</param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    Task<C2SIMServerResponse> PushInitializationMessage(string xmlMessage);
    /// <summary>
    /// Send an Order message to the server
    /// </summary>
    /// <param name="xmlMessage">C2SIM message to send - formatted according to the standard</param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    Task<C2SIMServerResponse> PushOrderMessage(string xmlMessage);
    /// <summary>
    /// Send a Report message to the server
    /// </summary>
    /// <param name="xmlMessage">C2SIM message to send - formatted according to the standard</param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    Task<C2SIMServerResponse> PushReportMessage(string xmlMessage);
    /// <summary>
    /// Send a message to the server
    /// </summary>
    /// <remarks>
    /// This is a generic version of the specialized PushInitializeMessage,
    /// PushOrderMessage and PushReportMessage, which should be preferred
    /// </remarks>
    /// <param name="xmlMessage">C2SIM message to send - formatted according to the standard</param>
    /// <param name="performative">INFORM, ORDER, REPORT - need to match the type of xmlMessage - Initialization, Order, or Report</param>
    /// <returns>Server response - Status OK if success, ERROR otherwise</returns>
    Task<C2SIMServerResponse> PushMessage(string xmlMessage, string performative);
    /// <summary>
    /// Reset the server to a state where it accepts initialization messages
    /// </summary>
    Task ResetToInitializing();
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
    Task StompPublish(string cmd, List<string> headers, string xml);
    /// <summary>
    /// Set the server to a state where it accepts Order/Report messages
    /// </summary>
    Task SwitchToRunning();
}
