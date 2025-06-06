namespace C2SIM;

/// <summary>
/// Object returned by a C2SIM server as a response to a command/pushed message
/// </summary>
/// <remarks>
/// <code>
/// <result>
/// 	<status>OK</status>
/// 	<message>Server is up</message>
/// 	<serverInitialized>true</serverInitialized>
/// 	<serverVersion>4.8.0.11</serverVersion>
/// 	<sessionState>RUNNING</sessionState>
/// 	<unitDatabaseName>default</unitDatabaseName>
/// 	<unitDatabaseSize>4</unitDatabaseSize>
/// 	<msgNumber>188</msgNumber>
/// 	<time> 0.000</time>
/// 	<collectResponseTime>T</collectResponseTime>
/// </result>
/// </code>
/// </remarks>
[System.SerializableAttribute()]
[System.Diagnostics.DebuggerStepThroughAttribute()]
[System.ComponentModel.DesignerCategoryAttribute("code")]
[System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
[System.Xml.Serialization.XmlRootAttribute("result", Namespace = "", IsNullable = true)]
public partial class C2SIMServerResponse
{

    #region Private properties
    private ResponseStatus statusField;

    private string messageField;

    private bool serverInitializedField;

    private string serverVersionField;

    private string sessionStateField;

    private string unitDatabaseNameField;

    private long unitDatabaseSizeField;

    private long msgNumberField;

    // Newer versions of the server may return "nul" literal, so cannot be decimal
    private string timeField;

    private string collectResponseTimeField;
    #endregion
    
    #region Public constants
    /// <summary>
    /// Status indicating outcome of the operation
    /// </summary>
    public enum ResponseStatus { 
        /// <summary>
        /// Operation was successful
        /// </summary>
        OK, 
        /// <summary>
        /// Operation failed
        /// </summary>
        ERROR }
    #endregion

    #region Public properties
    /// <summary>
    /// Indicates whether the operation was successful 
    /// </summary>
    [System.Xml.Serialization.XmlIgnoreAttribute]
    public Boolean IsSuccess => Status == ResponseStatus.OK;

    /// <summary>
    /// Status - OK or ERROR
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("status")]
    public ResponseStatus Status { get => this.statusField; set => this.statusField = value; }

    /// <summary>
    /// Message detailing the error or operation outcome
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("message")]
    public string Message { get => this.messageField; set => this.messageField = value; }

    /// <summary>
    /// Server initialization - "true" if it is initialized
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("serverInitialized")]
    public bool ServerInitialized { get => this.serverInitializedField; set => this.serverInitializedField = value; }

    /// <summary>
    /// Server version, e.g. 4.8.0.11
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("serverVersion")]
    public string ServerVersion{ get => this.serverVersionField; set => this.serverVersionField = value; }

    /// <summary>
    /// Server session state - UNKNOWN, UNINITIALIZED, INITIALIZING, INITIALIZED, RUNNING, PAUSED
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("sessionState")]
    public string SessionState{ get => this.sessionStateField; set => this.sessionStateField = value; }

    /// <summary>
    /// Unit database used
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("unitDatabaseName")]
    public string UnitDatabaseName { get => this.unitDatabaseNameField; set => this.unitDatabaseNameField = value; }

    /// <summary>
    /// Size of the Unit database
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("unitDatabaseSize")]
    public long UnitDatabaseSize { get => this.unitDatabaseSizeField; set => this.unitDatabaseSizeField = value; }

    /// <summary>
    /// Message identifier / index
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("msgNumber")]
    public long MsgNumber { get => this.msgNumberField; set => this.msgNumberField = value; }

    /// <summary>
    /// Execution time?
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("time")]
    public string Time { get => this.timeField; set => this.timeField = value; }

    /// <summary>
    /// Indicates whether profiling/response times are bening collected - true if "T"
    /// </summary>
    [System.Xml.Serialization.XmlElementAttribute("collectResponseTime")]
    public string CollectResponseTime{ get => this.collectResponseTimeField; set => this.collectResponseTimeField = value; }
    #endregion
}
