using System.Text;

namespace C2SimClientLib;

/// <summary>
///  Encapsulates a STOMP Message along with other data created during the processing of the message
/// </summary>
/// <remarks>
///  From original Java code implemented by Douglas Corner - George Mason University C4I and Cyber Center
/// </remarks>
public class C2SIMSTOMPMessage
{
    #region Private properties
    /// <summary>
    /// STOMP Message COMMAND received
    /// </summary>
    internal string _messageType;                   // CONNECTED, MESSAGE, etc
    /// <summary>
    /// Type of BML message as determined by XML matching
    /// </summary>
    internal string _messageSelector;               // IBMLReport, ..
    /// <summary>
    /// Raw unparsed STOMP message headers
    /// </summary>
    internal List<string> _headers;
    /// <summary>
    ///  Maps header to header value
    /// </summary>
    internal Dictionary<string, string> _headerMap;  // Header     
    /// <summary>
    /// The body of the message not including terminating null
    /// </summary>
    internal string _messageBody;
    /// <summary>
    /// Length of message body as received
    /// </summary>
    /// <remarks>
    /// Does not include terminating null.  Obtained from content-length header    
    /// </remarks>
    internal long _contentLength;
    /// <summary>
    /// Length of the message after removing the C2SIM header.  
    /// </summary>
    /// <remarks>
    /// For non C2SIM messages this is the same as the contentLength
    /// </remarks>
    internal long _messageLength;
    /// <summary>
    /// C2SIMHeader stripped from incoming Message
    /// </summary>
    internal C2SIMHeader _c2sim;
    /// <summary>
    /// Exception caught in background thread
    /// </summary>
    /// <remarks>
    /// Used to communicate exception to background.
    /// May be an otherwise empty message        
    /// </remarks>
    internal Exception _error;
    #endregion

    #region Construction / teardown
    /// <summary>
    /// Constructor
    /// </summary>
    public C2SIMSTOMPMessage()
    {
        _messageSelector = string.Empty;
        _messageType = string.Empty;
        _headers = new List<string>();       
        _headerMap = new Dictionary<string, string>();
        _messageBody = string.Empty;                  
        _messageLength = 0L;                
    }
    #endregion

    #region Public methods
    /// <summary>
    ///   Move the values from headers Vector creating a HashMap of header names and header values
    /// </summary>
    /// <returns>string - messageSelector if one was found </returns>
    public string CreateHeaderMap()
    {
        _headerMap = new Dictionary<string, string>();
        for (int i = 0; i < _headers.Count(); ++i)
        {
            int colomnIdx = _headers[i].IndexOf(':');
            if (colomnIdx < 0)
            {
                // Just ignore - some entries are not headers, e.g. 'MESSAGE'
                continue;
            }
            // Key/name before the ':'
            string header = _headers[i].Substring(0, colomnIdx);
            // Value after the ':' - may be empty
            string headerVal = _headers[i].Length > colomnIdx + 1 ? _headers[i].Substring(colomnIdx + 1) : string.Empty; 
            // Some headers may be repeated (e.g. 'destination'). Keeping the latest
            _headerMap[header] = headerVal;
            // The following java code seems - as the original comment indicates - never to be true
            // What is seem is a header message-selector: C2SIM_Order or similar now, not "true: <value>"
            //// A value of true is used with the particular message Selector for this message
            //// TODO: what can be seen in some of the actual headers are entries where the header _key_ is "true",
            //// for example "true:1636062688801"
            //// This test for headerVal is possibly a mistake
            //// if (headerVal.Equals("true", StringComparison.InvariantCultureIgnoreCase))
            if (header.Equals("message-selector", StringComparison.InvariantCultureIgnoreCase))
            {
                _messageSelector = headerVal;
            }
        }
        return _messageSelector;
    }

    /// <summary>
    /// Contents of a specific STOMP header
    /// </summary>
    /// <param name="header">Specific header e.g. "content-length"</param>
    /// <returns>- string - Value of header or string.Empty if header not set in incoming message</returns>
    public string GetHeader(string header)
    {
        return _headerMap.ContainsKey(header) ? _headerMap[header] : string.Empty;
    }
    #endregion

    #region Private methods
    /// <summary>
    /// Add a line to the list of headers
    /// </summary>
    /// <param name="s">Header to add</param>
    internal void AddHeader(string s)
    {
        _headers.Add(s);
    }

    /// <summary>
    /// Add line to message body
    /// </summary>
    /// <param name="s">Line to add</param>
    internal void AddToBody(string s)
    {
        _messageBody += s;
    }
    #endregion

    #region Public properties
    /// <summary>
    /// STOMP command for this message
    /// </summary>
    /// <remarks>
    /// Normally CONNECTED or MESSAGE
    /// </remarks>
    public string MessageType => _messageType;
    /// <summary>
    /// Message type determined when the server receives 
    /// the message from its creator 
    /// </summary>
    public string MessageSelector => _messageSelector;
    /// <summary>
    /// Body of the message
    /// </summary>
    /// <remarks>
    /// Part of the message following the headers.  Does not include the terminating NULL
    /// </remarks>
    public string MessageBody => _messageBody;
    /// <summary>
    /// Length of the message without the C2SIM Header
    /// </summary>
    /// <remarks>
    ///  For non C2SIM messages this is the same as the contentLength.
    /// </remarks>
    public long MessageLength => _messageLength;
    /// <summary>
    /// Length of the message as determined by the content-length header
    /// </summary>
    public long ContentLength => _contentLength;
    /// <summary>
    /// C2Sim header from this message
    /// </summary>
    public C2SIMHeader C2SIMHeader => _c2sim;

    /// <summary>
    /// Exception thrown in the stomp TCP thread, wrapped for delivery within this message
    /// </summary>
    public Exception Error => _error;
    #endregion

}
