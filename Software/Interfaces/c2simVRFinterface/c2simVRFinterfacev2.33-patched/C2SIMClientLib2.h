/*----------------------------------------------------------------*
|   Copyright 2009-2020 Networking and Simulation Laboratory      |
|         George Mason University, Fairfax, Virginia              |
|                                                                 |
| Permission to use, copy, modify, and distribute this            |
| software and its documentation for all purposes is hereby       |
| granted without fee, provided that the above copyright notice   |
| and this permission appear in all copies and in supporting      |
| documentation, and that the name of George Mason University     |
| not be used in advertising or publicity pertaining to           |
| distribution of the software without specific, written prior    |
| permission. GMU makes no representations about the suitability  |
| of this software for any purposes.  It is provided "AS IS"      |
| without express or implied warranties.  All risk associated     |
| with use of this software is expressly assumed by the user.     |
*----------------------------------------------------------------*/
#pragma once
#include <string>
#include <vector>
#include <map>
#include <exception>
#include <stdio.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SISOSTD "SISO-STD-C2SIM"

/**
*  version 4.8.0.0 converted from Doug Corner's Java
*/

class C2SIMClientException : public std::exception {

public:

	/// C2SIM Exception caused by another exception
	C2SIMClientException(std::string& m);

	/// C2SIM Exception caused by another exception
	C2SIMClientException(std::string& m, exception e);
	~C2SIMClientException();

	/**
	* Get message set in this exception when instantiated
	* @return String - Message included in constructor
	*/
	virtual const char* getMessage();

	/**
	* Get message from another exception thrown by underlying software and included in this exception
	* @return String - Underlying cause message
	*/
	virtual const char*  getCauseMessage() const throw();

};

/**
*
* \brief C2SIM Server Web Services REST Client
* This client does the following:
*       Open a connection with the server on specified port (Default is 8080)
*       Build an HTTP POST transaction from parameters and BML XML document
*       Submit the transaction
*       Read the result
*       Disconnect from the server
*       Return the result received from the server to the 
* @see C2SIMSTOMPMessage
*
* \author Douglas - George Mason University C4I and Cyber Center
* translated frpm Java by Aakarshika Priydarshi 
*
*/

class C2SIMHeader
{

public:

	C2SIMHeader();
	~C2SIMHeader();

	/********************/
	/*  insertC2SIM     */
	/********************/
	/**
	* Given an xml document with the XML header and content create and insert a C2SIM header into it
	@param xml - The xml string that header is to be inserted into
	@param sender - The C2SIM Sender
	@param receiver - The C2SIM Receiver
	@param performative - One of the C2SIM performatives <BR>
	Possible values for performative are: Inform, Confirm, Refuse, Accept, Agree, Request
	@return - The updated XML string with the header inserted
	*/
	std::string C2SIMHeader::insertC2SIM(
		std::string xml, 
		std::string sender, 
		std::string receiver, 
		std::string performative,
		std::string isoDateTime);

	/********************/
	/* removeC2SIM      */
	/********************/
	/**
	* Remove C2SIM message envelope and return core xml message with header
	@param xml - String - Input xml message with C2SIM envelop
	@return String - Reconstructed message without C23SIM components
	*/
	std::string C2SIMHeader::removeC2SIM(std::string xml);

	/*************/
	/*  xmlDoc   */
	/*************/
	/**
	*   xmlDoc - return a DOM Document representing this message header <BR>
	*   NOT IMPLEMENTED AT THIS TIME
	* @return null
	*/
	std::string C2SIMHeader::toDoc();

	/***********************/
	/* extractDataForTag   */
	/***********************/
	/**
	* Utility routine to pull data from XML string, given a tag
	* This does not properly parse the XML; it just assumes
	* the XMLwill be well structured accorind to C2SIM schema
	* and that tag requested is for a leaf node which occurs
	* only once in the XML;
	* ignores namespace prefix in xmlString but not in tagSought
	*
	* @param xmlString - the string to be examined
	* @param tagSought - the tag for which data is sought
	* @return the string of data from the tag or "" if not found
	*/
	std::string C2SIMHeader::extractDataForTag(std::string xmlString, std::string tagSought);

	/********************/
	/*  populateC2SIM   */
	/********************/
	/**
	* Populate a new C2SIMHeader object from an XML string.
	* @param xmlString - source of data to popuate the header
	* @throws C2SIMClientException - Thrown for one of the following reasons <BR>
	*  IOException in SAXBuilder
	*  JDOMException
	*/
	C2SIMHeader* C2SIMHeader::populateC2SIM(std::string xmlString);

	/********************/
	/*  toXMLString     */
	/********************/
	/**
	* toXMLString
	* @return - XML String containing the contents of this C2SIM header object
	*/
	std::string C2SIMHeader::toXMLString();

	/***********************************/
	/*  findStartTag and findEndTag     /
	/***********************************/
	/**
	* toXMLString
	* these two functions only work when findEndTag is called
	* with same tag and immediately after findStartTag
	* also they ignore any namespace prefix that is present
	*
	* findStartTag
	* @param std:string XML string
	* @param std:string tag to find
	* @param size_t offset in XML to start search
	* @return - size_T position where opening tag starts in XML
	*
	* findEndTag
	* @param std:string XML string
	* @param std:string tag to find
	* @param size_t offset in XML to start search
	* @return - size_t position where closing tag ends in XML
	*/
	size_t C2SIMHeader::findStartTag(std::string xml, std::string tag, size_t offset);
	size_t C2SIMHeader::findEndTag(std::string xml, std::string tag, size_t offset);


	/****************************/
	/*  getters and setters     */
	/****************************/

	/**
	* getCommunicativeActTypeCode - Get the value of the CommunicativeActTypeCode property - Indicates processing requested for this message
	@return - String - Possible values: Inform, Confirm, Refuse, Accept, Agree, Request
	*/
	std::string C2SIMHeader::getCommunicativeActTypeCode();

	/**
	* setCommunicativeActTypeCode - Set value of CommunicativeActTypeCode property in current C2SIMHeader
	@param CommunicativeActTypeCode - String - Possible values see getPerformative()
	@see getCommunicativeActTypeCode#getCommunicativeActTypeCode
	*/
	void C2SIMHeader::setCommunicativeActTypeCode(std::string communicativeActTypeCode);

	/**
	* setSecurityClassificationCode - Set field used to connect a series of messages into one conversation
	@param securityClassificationCode - String UUID  C2SIM securityClassificationCode .
	*/
	void C2SIMHeader::setSecurityClassificationCode(std::string securityClassificationCode);

	/**
	* getSecurityClassificationCode - Get field used to connect a series of messages into one conversation
	@return - securityClassificationCode - String UUID
	*/
	std::string C2SIMHeader::getSecurityClassificationCode();

	/**
	* setConversationID - Set field used to connect a series of messages into one conversation
	* @param conversationID - String UUID  C2SIM conversationID .
	*/
	void C2SIMHeader::setConversationID(std::string cid);

	/**
	* getConversationID - Get field used to connect a series of messages into one conversation
	* @return - conversationID - String UUID
	*/
	std::string C2SIMHeader::getConversationID();

	/**
	* generateConversationID - Generate a new Conversation ID (UIID format) for this C2SIM Header
	*/
	void C2SIMHeader::generateConversationID();

	/**
	* getInReplyTo property indicating the system to reply to
	* @return InReplyTo - String - System identifier
	*/
	std::string C2SIMHeader::getInReplyTo();

	/**
	* setInReplyTo  Set inReplyTo property indicating identification of message being replied to
	* @param inReplyTo - String messageID - UUID
	*/
	void C2SIMHeader::setInReplyTo(std::string irt);

	/**
	* getMessageID - Get ID of message using current C2SIM header
	* @return messageID = String - UUID
	*/
	std::string C2SIMHeader::getMessageID();

	/**
	* setMessageID - Set the message ID of the message using this C2SIM Header
	* @param messageID String - UUID
	*/
	void C2SIMHeader::setMessageID(std::string mid);

	/**
	* generateMessageID - Generate ID for this C2SIM Message Header
	*/
	void C2SIMHeader::generateMessageID();

	/**
	* setProtocol - Set field used to indicate the protocol of the message
	@param protocol - String UUID  C2SIM protocol .
	*/
	void  C2SIMHeader::setProtocol(std::string protocolParm);

	/**
	* getProtocol - Get field used to indicate the protocol of the message
	@return - protocol - String UUID
	*/
	std::string C2SIMHeader::getProtocol();

	/**
	* setProtocol - Set field used to indicate the protocol of the message
	@param protocol - String UUID  C2SIM protocol .
	*/
	void  C2SIMHeader::setProtocolVersion(std::string protocolVersionParm);

	/**
	* getProtocolVersion - Get field used to indicate the version of the protocol of the message
	@return - protocolVersion - String
	*/
	std::string C2SIMHeader::getProtocolVersion();

	/**
	* getSendingTime - ISO Format time when C2SIMHeader object was instantiated
	*/
	std::string C2SIMHeader::getSendingTime();

	/**
	* setSendingTime - Set sendingTime = ISO Format
	@param sendingTime
	*/
	void C2SIMHeader::setSendingTime(std::string sendingTime);

	/**
	* getReplyToSystem = Get replyTo property indicating what system is to be replied to
	@return - System Identification - String
	*/
	std::string C2SIMHeader::getReplyToSystem();

	/**
	* setReplyToSystem - Set replyTo property indicating what system is to be replied to
	@param replyTo
	*/
	void C2SIMHeader::setReplyToSystem(std::string replyTo);

	/**
	* getPerformative - Get the value of the performative property - Indicates processing requested for this message
	* @return - String - Possible values: Inform, Confirm, Refuse, Accept, Agree, Request
	*/
	std::string C2SIMHeader::getPerformative();

	/**
	* setPerformative - Set value of performative property in current C2SIMHeader
	* @param performative - String - Possible values see getPerformative()
	* @see getPerformative
	*/
	void C2SIMHeader::setPerformative(std::string p);

	/**
	* getToReceivingSystem - Get value of intended receiver for this message
	* @return receiver - String
	*/
	std::string C2SIMHeader::getToReceivingSystem();

	/**
	* setToReceivingSystem - Set the receiver property indicating the indented recipient for this C2SIM message
	* @param receiver - String
	*/
	void C2SIMHeader::setToReceivingSystem(std::string r);

	/**
	* getReplyBy - Specifies date/time by which reply is needed
	@return DateTime as - yyyy-mm-ddThh:mm:ssZ
	*/
	std::string C2SIMHeader::getReplyBy();

	/**
	* setReplyBy - Set date/time by which reply is requested
	* @param replyBy - Date/Time as yyyy-mm-ddThh:mm:ssZ
	*/
	void C2SIMHeader::setReplyBy(std::string rb);

	/**
	* getReplyTo = Get replyTo property indicating what system is to be replied to
	* @return - System Identification - String
	*/
	std::string C2SIMHeader::getReplyTo();

	/**
	* setReplyTo - Set replyTo property indicating what system is to be replied to
	* @param replyTo
	*/
	void C2SIMHeader::setReplyTo(std::string rt);

	/**
	* getReplyWith - Get replyWith property indicating response required from receiver
	* @return - Performative - String
	*/
	std::string C2SIMHeader::getReplyWith();

	/**
	* setReplyWith - Set replyWith property indicating response required from receiver
	* @param replyWith - Performative - String
	*/
	void C2SIMHeader::setReplyWith(std::string rw);

	/**
	* ggetFromSendingSystem - Get the identification of the sender of thie message
	* @return System ID - String
	*/
	std::string C2SIMHeader::getFromSendingSystem();

	/**
	* setFromSendingSystem - Set identification of the sender of this message
	* @param sender - System identifier - String
	*/
	void C2SIMHeader::setFromSendingSystem(std::string s);

	/**
	* getInReplyToMessageID - Get ID message being replied to
	@return - String System ID UUID format
	*/
	std::string C2SIMHeader::getInReplyToMessageID();

	/**
	* setInReplyToMessageID - Set ID message being replied to
	@param inReplyToMessageID- ID in UUID format
	*/
	void setInReplyToMessageID(std::string inReplyToMessageID);

};// end Class C2SIMHeader


class C2SIMClientREST_Lib
{
public:

	/****************************************/
	/* C2SIMClientREST_Lib() - Constructors   */
	/****************************************/
	/**
	* C2SIMClientREST_Lib Constructor w/o parameters
	*/
	C2SIMClientREST_Lib();
	~C2SIMClientREST_Lib();

	/**
	* getVersion
	* @return current version of C2SIMClientLib
	*/
	std::string C2SIMClientREST_Lib::getVersion();

	/**
	* Constructor used when the intention is to send a C2SIM document. <BR>
	* The supplied parameters will be saved and used to create the C2SIM message header 
	*	when c2simRequest is called to make the submission
	* @param sender   -   C2SIMHeader field - Sender of document
	* @param receiver -   C2SIMHeader field - Receiver of document
	* @param performative - C2SIMHeader field - Action that receiver is to  
	*	perform as specified by the C2SIM specification
	*/
	C2SIMClientREST_Lib(std::string sender, std::string receiver, 
		std::string performative, std::string protocolVersion);

	/********************/
	/* serverStatus     */
	/********************/
	/**
	* Get status of C2SIM Server. - Confirm that server is running and return initialization status<BR>
	* setHost()  and setSubmitter() must have must have been executed before calling this method.<BR>
	@return - XML Document indicating current status of the server.<BR>
	@throws C2SIMClientException - Primary and secondary causes will be available in
	Sample output:
		<pre>{@code
		<?xml version="1.0" encoding="UTF-8"?>
		<result>
		<status>OK</status>
		<message>Server is operating</message>
		<serverInitialized>false</serverInitialized>
		<sessionInitialized>false</sessionInitialized>
		<unitDatabaseName>defaultDB</unitDatabaseName>
		<unitDatabaseSize>0</unitDatabaseSize>
		<msgNumber>0</msgNumber>
		<time> 0.000</time>
		</result>
		}</pre>
	*/
	std::string C2SIMClientREST_Lib::serverStatus();

	/************************/
	/* c2simCommand         */
	/************************/
	/**
	* c2simCommand pass a command to the C2SIM Server <BR>
	* Current commands are NEW, LOAD, SAVE, SAVEAS, DELETE, SHARE, QUERYUNIT, QUERYINIT<BR>
	* See the latest version of <i>C2SIM Server Reference Implementation</i> for details.<BR>
	* Result is an XML document which may contain {@code<status>OK</status>} or may be actual 
	*	data depending on the command submitted.<BR>
	*
	@param cmd      Command to be processed.
	@param parm1    Optional first parameter
	@param parm2    Optional second parameter
	@return String result - XML Document giving results of command and server status similar to serverStatus method.
	@throws C2SIMClientException = Primary and secondary causes are transmitted within the C2SIMClientException object.
	*/
	std::string C2SIMClientREST_Lib::c2simCommand(
		std::string cmd,
		std::string parm1,
		std::string parm2);

	/********************/
	/* c2simRequest     */
	/********************/
	/**
	* Submit a request to a C2SIM Server <BR>
	* This method performs the same function as the legacy bmlRequest method 
	@param xml - The xml document being submitted
	@return - Indication of success of operation along with server status.  See serverStatus method.
	@throws C2SIMClientException - Primary and secondary causes will be included in C2SIMClientException object
	*/
	std::string C2SIMClientREST_Lib::c2simRequest(std::string xml);

	/********************/
	/*  bmlRequest()    */
	/********************/
	/**
	* Submit a C2SIM transaction to the host specified
	* Request string using default is:
	* {@code "http://10.2.10.30::8080/C2SIMServer/bml?submitterID=jmp&forwarder=" }
	* @param xml - An XML string containing the C2SIM
	* @return  XML - The response returned by the host C2SIM server
	* @see edu.gmu.c4i.c2simclientlib2.C2SIMClientException
	* @throws edu.gmu.c4i.c2simclientlib2.C2SIMClientException - Various causes
	*/
	std::string C2SIMClientREST_Lib::bmlRequest(std::string xmlTransaction);

	/****************************/
	/*  getters and setters     */
	/****************************/

	/**
	* Set buffer length (default is 50000)
	*/
	void C2SIMClientREST_Lib::setBufferLength(size_t size);

	/**
	* Set the port number (as a string)
	* @param port
	*/
	void C2SIMClientREST_Lib::setPort(std::string port);

	/**
	* Set the host name or address
	* @param host
	*/
	void C2SIMClientREST_Lib::setHost(std::string host);

	/**
	* Set the port number (as a string)
	* @param path
	*/
	void C2SIMClientREST_Lib::setPath(std::string path);

	/**
	* Set the domain property.  Used to discriminate between BML dialects
	* @param domain
	*/
	void C2SIMClientREST_Lib::setDomain(std::string domain);

	/**
	* Set the Requestor property indicating the identity of the client
	* @param requestor
	*/
	void C2SIMClientREST_Lib::setSubmitter(std::string submitter);

	/**
	* Set the protocol property
	* @param protocol
	*/
	void C2SIMClientREST_Lib::setProtocol(std::string protocolParm);

	/**
	* Set the protocolVersion property
	* @param protocolVersion
	*/
	void C2SIMClientREST_Lib::setProtocolVersion(std::string protocolVersionParm);

	/**
	* Set the FirstForwarder property indicating first server to
	* forward the XML
	* @param firstForwarder
	*/
	void C2SIMClientREST_Lib::setFirstForwarder(std::string firstForwarder);

	/**
	* Set the value of C2SIMHeader to be used with submission of C2SIM transaction
	@param c C2SIMHeader
	*/
	void C2SIMClientREST_Lib::setC2SIMHeader(C2SIMHeader* ch);

	/**
	* Set the stompServer property indicating STOMP Server host name (or IP)

	* @param stompServer    String - Host name (or IP) of server providing stomp pub/sub services
	*/
	void  C2SIMClientREST_Lib::setStompServer(std::string ss);

	/**
	* Get the current setting of the port property
	* @return port
	*/
	std::string C2SIMClientREST_Lib::getPort();
	/**
	* Get the current setting of the path property
	* @return
	*/
	std::string C2SIMClientREST_Lib::getPath();

	/**
	* Get current setting of host property
	* @return
	*/
	std::string C2SIMClientREST_Lib::getHost();

	/**
	*
	* Returns the current setting of the domain property
	* @return  the current setting of the domain property
	*/
	std::string C2SIMClientREST_Lib::getDomain();

	/**
	* Get current setting of Requestor property
	* @return
	*/
	std::string C2SIMClientREST_Lib::getSubmitter();

	/**
	* Get the protocol property
	*/
	std::string C2SIMClientREST_Lib::getProtocol();

	/**
	* Get the FirstForwarder property indicating first server to
	* forward the XML
	*/
	std::string C2SIMClientREST_Lib::getFirstForwarder();

	/**
	* Return value of C2SIM Header
	@return C2SIMHeader the current C2SIM header.
	*/
	C2SIMHeader* C2SIMClientREST_Lib::getC2SIMHeader();

	/**
	* Get the stompServer property indicating name of host providing STOMP pub/sub services
	* @return  String - Name (or IP)  identifying STOMP Server
	*/
	std::string  C2SIMClientREST_Lib::getStompServer();
};

class C2SIMSTOMPMessage
{
public:

	C2SIMSTOMPMessage();
	~C2SIMSTOMPMessage();

	/**
	 * Add header to message
	 */
	void C2SIMSTOMPMessage::addHeader(std::string s);

	/**
	 * import reference to the StompClient frame.properties
	 * for use as headerMap
	 */
	void C2SIMSTOMPMessage::setHeaderMap(std::map<std::string, std::string> frameProperties);

	/* 
	 * Add line to message body
	 */
	void C2SIMSTOMPMessage::addToBody(std::string s);

	/**
	* error Description of Exception caught in foreground thread.  Used to communicate exception to background.
	*      May be an otherwise empty message
	*/
	std::string error;

	/**
	* getMessageType - Returns the STOMP command for this message.  Normally CONNECTED or MESSAGE
	* @return String - The STOMP COMMAND for this message
	*/
	std::string C2SIMSTOMPMessage::getMessageType();

	/**
	* setMessageType - Saves the STOMP command for this message.  Normally CONNECTED or MESSAGE
	* @param type e.g. - The STOMP COMMAND for this message
	*/
	void C2SIMSTOMPMessage::setMessageType(std::string type);

	/**
	* Return the C2SIM message type determined when the server receives the message from its creator
	* @return String - BML Message Selector e.g. IBML09GSR
	*/
	std::string C2SIMSTOMPMessage::getMessageSelector();

	/**
	* Return the body of the message, i.e. the part of the message following the headers.  
	* Does not include the terminating NULL
	* @return String - The message body from the STOMP Message
	*/
	std::string C2SIMSTOMPMessage::getMessageBody();

	/**
	* Sets value of body of the message, i.e. the part of the message following the headers.  
	* Does not include the terminating NULL
	* @param body - message content
	*/
	void C2SIMSTOMPMessage::setMessageBody(std::string body);

	/**
	* Get the length of the message determined internally
	* @return Long - The message length
	*/
	long C2SIMSTOMPMessage::getMessageLength();

	/**
	* set messageLength for this message
	*/
	void C2SIMSTOMPMessage::setMessageLength(long messageMengthParm);

	/**
	* set contentLength for this message
	*/
	void C2SIMSTOMPMessage::setContentLengthFromStompHeader(long contentLengthParm);

	/**
	* Get the length of the message as determined by the content-length header
	* @return Long - The content length
	*/
	long C2SIMSTOMPMessage::getContentLength();

	/**
	* getC2SIMHeader
	* @return C2SIHeader the c2sim header from this essage
	*/
	C2SIMHeader* C2SIMSTOMPMessage::getC2SIMHeader();

	/**
	* setC2SIMHeader
	* @paramn C2SIHeader the c2sim header from this essage
	*/
	void C2SIMSTOMPMessage::setC2SIMHeader(C2SIMHeader* c2simHeaderParm);

	/**
	* Get the contents of a specific STOMP header
	* @param header e.g. "content-length"
	* @return - String - Value of header or "" if header not set
	*/
	std::string C2SIMSTOMPMessage::getHeader(std::string header);

};

class C2SIMClientSTOMP_Lib
{

public:
	/**************************************/
	/* C2SIMClientSTOMP_Lib() Constructor   */
	/**************************************/
	/**
	* Constructor - No parameters
	*/
	C2SIMClientSTOMP_Lib();

	/**************************************/
	/* C2SIMClientSTOMP_Lib() Destructor   */
	/**************************************/
	/*
	* Destructor - Default - No parameters
	*/
	~C2SIMClientSTOMP_Lib();

	/****************/
	/* connect      */
	/****************/
	// Connect to STOMP Host
	//  Wait for CONNECTED Message
	/**
	* Connect to Stomp host
	* @return STOMPMessage - Response from host if connection made otherwise throw an exception.  Response should be CONNECTED.
	* @see edu.gmu.c4i.c2simclientlib2.C2SIMClientException
	* @see edu.gmu.c4i.c2simclientlib2.C2SIMSTOMPMessage
	* @throws C2SIMClientException - Includes various exceptions
	*/
	C2SIMSTOMPMessage* C2SIMClientSTOMP_Lib::connect();

	/****************/
	/* publish      */
	/****************/
	/**
	* Send message to STOMP host on connection already established
	* @param xml - The message to be sent
	* @param headers - A Vector Strings containing STOMP headers in the form  headerName:headerValue
	*@throws C2SIMClientException
	*/
	void C2SIMClientSTOMP_Lib::publish(std::string cmd, std::vector<std::string> headers, std::string xml);

	/*************************/
	/* getNext_NoBlock()     */
	/*************************/
	/**
	* Returns the next message received from the STOMP messaging server.
	* The calling thread will NOT be blocked if a STOMPMessage is not available; .
	* @return STOMPMessage - The next STOMP message or NULL if no message is available at this time.  Message should be MESSAGE.
	* @see edu.gmu.c4i.c2simclientlib2.C2SIMSTOMPMessage
	* @see edu.gmu.c4i.c2simclientlib2.C2SIMClientException
	* @throws C2SIMClientException - Encapsulates several different exceptions.
	*/
	C2SIMSTOMPMessage* C2SIMClientSTOMP_Lib::getNext_NoBlock();

	/*************************/
	/* getNext_Block()       */
	/*************************/
	/**
	* Returns the message received from the STOMP messaging server.  The calling thread
	* will be blocked until a message has been received.
	* @return STOMPMessage - The next STOMP message.  Message should be MESSAGE.
	* @see edu.gmu.c4i.c2simclientlib2.C2SIMSTOMPMessage
	* @see edu.gmu.c4i.c2simclientlib2.C2SIMClientException
	* @throws C2SIMClientException - Encapsulates various exceptions
	*/
	C2SIMSTOMPMessage* C2SIMClientSTOMP_Lib::getNext_Block();

	/************************/
	/* sendC2SIM_Response   */
	/************************/
	/**
	* sendC2SIM_Response - Send a C2SIM response to an incoming C2SIM request.  
	*	Response will be sent via STOMP
	* @param oldMsg - Message that is being responsed to
	* @param c2sResp - Response code to be sent*
	* @param ackCode - Code describing the acknowledgement
	* @throws edu.gmu.c4i.c2simclientlib2.C2SIMClientException 
	*	May throw a C2SIMClientException for several reasons <BR>
	*      IOException during send or close
	*      UnknownHost exception
	*      Received something other that "CONNECTED" during connection process
	*      InterruptedException while waiting for queue
	*      Error caught in foreground thread
	*/
	void C2SIMClientSTOMP_Lib::sendC2SIM_Response(
		C2SIMSTOMPMessage* oldMsg,
		std::string c2sResp,
		std::string ackCode);

	/*******************/
	/* disconnect()    */
	/*******************/
	/**
	* Disconnect from STOMP server and close socket.
	* @return std::string - "OK" indicating successful completion of disconnect or else throws an exception
	* @throws C2SIMClientException - Encapsulates various exceptions
	*/
	std::string C2SIMClientSTOMP_Lib::disconnect();

	/***********************/
	/* getters and setters  */
	/************************/
	// Getters and setters

	/**
	* setPort (std::string) for STOMP connection
	* @param port - std::string
	*/
	void C2SIMClientSTOMP_Lib::setPort(std::string port);

	/**
	* setPort int for STOMP connection
	* @param port - int
	*/
	void C2SIMClientSTOMP_Lib::setPort(int port);

	/**
	* @return int - Current port setting
	*/
	int C2SIMClientSTOMP_Lib::getPort();

	/**
	* Set the host name or IP address
	* @param host std::string name or IP address of STOMP server
	*/
	void C2SIMClientSTOMP_Lib::setHost(std::string host);

	/**
	* Get the address of the STOMP Messaging server
	* @return std::string - Host name/address
	*/
	std::string C2SIMClientSTOMP_Lib::getHost();

	/**
	* Set the destination queue or topic
	* @param destination - std::string Name of topic.  The default is /topic/C2SIM/
	*/
	void C2SIMClientSTOMP_Lib::setDestination(std::string destination);

	/**
	* Set the MessageSelector
	* @param messageSelector - std::string MessageSelector used for subscribing to a particular message
	*/
	void C2SIMClientSTOMP_Lib::setMessageSelector(std::string messageSelector);

	/**
	* Get the MessageSelector
	* @param messageSelector - std::string MessageSelector used for subscribing to a particular message
	*/
	std::string C2SIMClientSTOMP_Lib::getMessageSelector();

	/**
	* addSubscription - Add a Message Selector to list of selectors submitted with SUBSCRIBE
	*   This is deprecated.
	*   Host will only publish messages matching one of the selectors.
	*   If no addSubscriptions are submitted then all messages will be received.
	* @param msgSelector std::string - Name of a BML Message Type to be added to subscription list.  
	*	If the list contains at least one Message Selector then the only messages
	*   that will be received on the current connection will be those on the list.  
	*	If no subscriptions are submitted then this system will receive all messages published to the topic
	* @see setDestination
	*/
	void C2SIMClientSTOMP_Lib::addSubscription(std::string msgSelector);

	/**
	* addAdvSubscription - Add a general selector expression to be used with SUBSCRIBE
	*	This must be called before connect().
	*   Host will only publish messages matching one of the selectors.
	*   If no addSubscriptions are submitted then all messages will be received.
	* @param msgSelector std::string - Name of a BML Message Type to be added to subscription list.
	*	If the list contains at least one Message Selector then the only messages
	*   that will be received on the current connection will be those on the list.
	*	If no subscriptions are submitted then this system will receive all messages published to the topic
	* @see setDestination
	*/
	void C2SIMClientSTOMP_Lib::addAdvSubscription(std::string substring);
	// End of Getters/Setters

	/********************************************/
	/*  Foreground message receive thread run() */
	/********************************************/
	/**
	* run method is used internally to create a foreground thread for receiving and queuing messages
	* from STOMP server <BR>
	* This is required to be public to make a thread and should not be called except by internal code.
	*/
	void C2SIMClientSTOMP_Lib::run();

	/*****************/
	/* sendFrame()    */
	/******************/
	// Send frame on current socket
	static void sendFrame(std::ostream& outputSink, std::string data);

	void receiveProperties(std::map<std::string, std::string> &props);

	std::pair<std::string, std::string> receiveProperty();
};