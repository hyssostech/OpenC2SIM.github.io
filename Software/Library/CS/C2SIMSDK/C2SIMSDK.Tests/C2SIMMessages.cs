namespace C2SIM.Tests;

/// <summary>
/// Single-line C2SIM message fixtures. Single line because C2SIMClientSTOMPLib
/// concatenates body lines without a separator when reading a STOMP frame.
/// </summary>
public static class C2SIMMessages
{
    public const string Ns = "http://www.sisostds.org/schemas/C2SIM/1.1";

    public static string Header(string from = "TESTS") =>
        $"<C2SIMHeader xmlns=\"{Ns}\">"
        + "<CommunicativeActTypeCode>Inform</CommunicativeActTypeCode>"
        + "<ConversationID>11111111-1111-1111-1111-111111111111</ConversationID>"
        + $"<FromSendingSystem>{from}</FromSendingSystem>"
        + "<MessageID>22222222-2222-2222-2222-222222222222</MessageID>"
        + "<Protocol>SISO-STD-C2SIM</Protocol>"
        + "<ProtocolVersion>1.0.2</ProtocolVersion>"
        + "<ToReceivingSystem>ALL</ToReceivingSystem>"
        + "</C2SIMHeader>";

    /// <summary>Wrap a body element in the Message/C2SIMHeader/MessageBody envelope the server sends</summary>
    public static string Wrap(string bodyInner) =>
        $"<Message xmlns=\"{Ns}\">{Header()}<MessageBody>{bodyInner}</MessageBody></Message>";

    public static string Order(string id = "ORD-1") =>
        Wrap($"<DomainMessageBody><OrderBody><OrderID>{id}</OrderID></OrderBody></DomainMessageBody>");

    public static string Report(string id = "REP-1") =>
        Wrap($"<DomainMessageBody><ReportBody><ReportID>{id}</ReportID></ReportBody></DomainMessageBody>");

    public static string Initialization(string name = "INIT-1") =>
        Wrap($"<C2SIMInitializationBody><Name>{name}</Name></C2SIMInitializationBody>");

    public static string ObjectInitialization(string route = "R1") =>
        Wrap($"<ObjectInitializationBody><ObjectDefinitions><Route><Name>{route}</Name></Route></ObjectDefinitions></ObjectInitializationBody>");

    public static string SystemMessage() =>
        Wrap("<SystemMessageBody><StartScenario/></SystemMessageBody>");

    /// <summary>A body the dispatcher has no event for - must be ignored, not fatal</summary>
    public static string SystemAcknowledgement() =>
        Wrap("<SystemAcknowledgementBody><AcknowledgementTypeCode>Accept</AcknowledgementTypeCode></SystemAcknowledgementBody>");

    /// <summary>A DomainMessageBody child the dispatcher has no event for</summary>
    public static string Plan() =>
        Wrap("<DomainMessageBody><PlanBody><PlanID>PLAN-1</PlanID></PlanBody></DomainMessageBody>");

    /// <summary>Message bodies used for the REST push tests (no envelope - the lib adds it)</summary>
    public const string BareOrderBody = "<OrderBody><OrderID>ORD-REST</OrderID></OrderBody>";

    /// <summary>A non-C2SIM (legacy BML) document - root element is not MessageBody</summary>
    public const string BmlDocument = "<BMLReport><ReportID>BML-1</ReportID></BMLReport>";
}
