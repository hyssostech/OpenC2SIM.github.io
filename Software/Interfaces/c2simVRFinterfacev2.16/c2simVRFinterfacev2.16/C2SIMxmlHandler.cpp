/*----------------------------------------------------------------*
|     Copyright 2021 Networking and Simulation Laboratory         |
|         George Mason University, Fairfax, Virginia              |
|                                                                 |
| Permission to use, copy, modify, and distribute this            |
| software and its documentation for academic purposes is hereby  |
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

// c2simVRF 2.16 consistent with C2SIM_SMX_LOX_v1.0.0.xsd

#include "C2SIMxmlHandler.h"

using namespace xercesc;

// conversion factor
const static double degreesToRadians = 57.2957795131L;

// root of the XML document
std::string rootTag = "";

// storage for order data

// strings to manipulate XML parsing
std::string latestTag;
std::string startTag;
std::string endTag;
std::string previousTag;
std::string oneBeforePreviousTag;
std::string dataText;
extern std::string blueForceName;

// type of message we are parsing
enum parsingType {
	NONE,
	INIT,
	SYSTEM,
	DOMAINTYPE,
	C2SIMORDER,
	C2SIMREPORT,
	IBMLORDER,
	IBMLREPORT,
	NOTIFICATION
} parsingType_t;

// flags to control parsing flow
bool skipThisDocument = false;
bool parsingC2simTask = false;
bool parsingC2simTaskStatusReport = false;
bool parsingManeuverWarfareTask = false;

// strings extracted from XML
std::string taskersIntent;
std::string dateTime;
std::string actionTemporalAssociationCode;
std::string latitude;
std::string longitude;
std::string elevation;
std::string latitudes[MAXPOINTS]; // each element is a latitude string
std::string longitudes[MAXPOINTS];// each element is a longitude string
std::string elevations[MAXPOINTS];// each element is an elevation string
std::string lastOrderId = "";
std::string xmlToParse;

// flags to indicate we've found a particular order string
bool foundRootTag = false;
bool foundIbmlOrderRootTag = false;
bool foundFinalTag = false;
bool parsingEntity = false;
bool parsingForceSide = false;
bool parsingBlueForceSide = false;
bool parsingSystemEntityList = false;
bool parsingSystemCommandTypeCode = false;
bool foundTaskTag = false;
bool foundPerformingEntity = false;
int locationPointCount = 0;
parsingType currentMessageType = NONE;
bool parsingC2simOrder = false;
bool parsingIbmlOrder = false;
bool parsingC2simReport = false;
bool parsingIbmlReport = false;
bool parsingC2simInitialize = false;
bool parsingSystemMessage = false;
bool parsingDISEntityType = false;
bool parseIbml, displayDebug;
int actorCount = 0; // actors in SystemEntityList
std::string orderFromSender = "";
std::string orderToReceiver = "";
std::string currentTaskStatusCode = "";

// data from the SystemEntityList of a C2SIMInitialize message
std::vector<std::string> actors;

// Tasks for Orders
// array of all tasks received in orders
std::string thisOrderUuid = "";
Task* reportTask = nullptr;
int tasksThisOrder;
Task* thisTask = nullptr;
Unit* thisUnit = nullptr;
BlueForceSide* blueForceSide = nullptr;
std::string otherForceSideAUuid = "";
std::string otherForceSideBUuid = "";
std::string otherForceSideAName = "";
std::string otherForceSideBName = "";

// Military Organization data for Units
static Unit* militaryOrgUnits;

// Simulation Control data
// UNINITIALIZED, INITIALIZING, INITIALIZED, RUNNING, PAUSED
static std::string systemState = "";

// constructor
C2SIMxmlHandler::C2SIMxmlHandler(bool useIbmlRef, bool displayDebugRef)
{
	// copy parameter value
	parseIbml = useIbmlRef;
	displayDebug = displayDebugRef;

	// initialize strings to zero length
	latestTag = "";
	startTag = "";
	endTag = "";
	previousTag = "";
	oneBeforePreviousTag = "";
	dataText = "";
	taskersIntent = "";
	dateTime = "";
	actionTemporalAssociationCode = "";
	latitude = "";
	longitude = "";
	elevation = "";
	for (int i = 0; i < MAXPOINTS; ++i) {
		latitudes[i] = "";
		longitudes[i] = "";
		elevations[i] = "";
	}
	orderFromSender = "";
	orderToReceiver = "";

	// initialize unit and Task maps
	unitMap; unitNameMap; unitVrfUuidMap;
	thisUnit = nullptr;
	actorCount = 0;
	taskMap; taskNameMap;
	orderIdMap;

	// initialize task index
	tasksThisOrder = 0;

}// end constructor

// destructor
C2SIMxmlHandler::~C2SIMxmlHandler()
{
}// end destructor


// print an error important enough to catch attention
// with a line of * on each side
void C2SIMxmlHandler::displayError(std::string errorText) {
	std::string output = "ERROR - " + errorText;
	std::cout << std::string(output.length(), '*') << "\n";
	std::cout << errorText << "\n";
	std::cout << std::string(output.length(), '*') << "\n";
}

// decode an IsoTimeDuration string to a number of seconds
// format is P00Y00M00DT00H00M00S where 00 is 1 or 2 digits
// returns -1 if format not valid
int C2SIMxmlHandler::findTotalIsoMs(std::string duration)
{
	int result = 0;
	try {
		if (duration.substr(0, 1) != "P")return -1;

		// parse and add years
		std::string remain = duration.substr(1);
		int yPos = remain.find("Y");
		if (yPos < 0)return -1;
		result = 31536000 * stoi(remain.substr(0,yPos));// 31536000 = 365*24*60*60
		
		// parse and add months
		remain = remain.substr(yPos + 1);
		int monthPos = remain.find("M");
		if (monthPos < 0)return -1;
		result += 108000 * stoi(remain.substr(0, monthPos));//108000 = 30*60*60
		
		// parse and add days
		remain = remain.substr(monthPos + 1);
		int dtPos = remain.find("DT");
		if (dtPos < 0)return -1;
		result += 86400 * stoi(remain.substr(0, dtPos));//86400 = 24*60*60
		
		// parse and add hours
		remain = remain.substr(dtPos + 2);
		int hPos = remain.find("H");
		if (hPos < 0)return -1;
		result += 3600 * stoi(remain.substr(0, hPos));//3600 = 60*60
		
		// parse and add minutes
		remain = remain.substr(hPos + 1);
		int minPos = remain.find("M");
		if (minPos < 0)return -1;
		result += 60 * stoi(remain.substr(0, minPos));

		// parse and add seconds
		remain = remain.substr(minPos + 1);
		int sPos = remain.find("S");
		if (sPos < 0)return -1;
		result += stoi(remain.substr(0, sPos));

		// return the sum in ms
		return 1000*result;
	}
	catch (...) { return -1; }
}

std::string C2SIMxmlHandler::getRootTag()
{
	return rootTag;
}

// returns root tag found in latest parse
// if no root tag found this will be empty string
int C2SIMxmlHandler::getMessageType()
{
	return currentMessageType;
}

// returns Uuid of order just parsed
std::string C2SIMxmlHandler::getOrderUuid()
{
	return thisOrderUuid;
}

// returns FromSender from order
std::string C2SIMxmlHandler::getOrderSender() {
	return orderFromSender;
}

// returns ToReceiver from order
std::string C2SIMxmlHandler::getOrderReceiver() {
	return orderToReceiver;
}

// returns superiorUnit for unit
std::string C2SIMxmlHandler::getSuperiorUnit(std::string uuid) {
	Unit* unit = unitMap[uuid];
	if (unit == nullptr) return "";
	return unit->superiorUnit;
}

// returns reportFromSender for unit
std::string C2SIMxmlHandler::getReportFromSender(std::string uuid) {
	Unit* unit = unitMap[uuid];
	if (unit == nullptr) return "";
	return unit->reportFromSender;
}

// returns reportToReceiver for unit
std::string C2SIMxmlHandler::getReportToReceiver(std::string uuid) {
	Unit* unit = unitMap[uuid];
	if (unit == nullptr) return "";
	return unit->reportToReceiver;
}

// returns TaskStatusCode when TaskStatus report has been parsed; else ""
std::string C2SIMxmlHandler::getTaskStatusCode() {
	return currentTaskStatusCode;
}

// returns pointer to the Unit associated with a UUID
Unit* C2SIMxmlHandler::getUnit(std::string uuid){
	return unitMap[uuid];
}

// returns pointer to to Unit associated with a name
Unit* C2SIMxmlHandler::getUnitByName(std::string name){
	Unit* response = unitNameMap[name];
	return response;
}

// returns pointer to to Unit associated with a vrfUuid
Unit* C2SIMxmlHandler::getUnitByVrfUuid(std::string vrfUuid) {
	return unitVrfUuidMap[vrfUuid];
}

// adds a new Unit to unitMap
// returns false if unit already eists; true otherwise
// Xerces is giving Entities twice so duplicate is two matches
bool C2SIMxmlHandler::addUnit(Unit* newUnit){
	if (unitMap[newUnit->uuid] != nullptr){
		newUnit->addInitCount += 1;
		if (newUnit->addInitCount > 1) {
			displayError("UNIT UUID:" + newUnit->uuid +
				" ALREADY ENTERED - DUPLICATES NOT ALLOWED");
			return false;
		}
	}
	unitMap[newUnit->uuid] = newUnit;
	return true;
}

// adds a unit pointer to the the unitNameMap using name in the unit as key
// returns false if unit already exists; true otherwise
// Xerces is giving Entities twice so duplicate is two matches
bool C2SIMxmlHandler::addUnitByName(Unit* newUnit) {
	if (unitNameMap[newUnit->name] != nullptr) {
		newUnit->nameInitCount += 1;
		if (newUnit->nameInitCount > 1) {
			displayError("UNIT UUID:" + newUnit->uuid +
				" ALREADY ENTERED - DUPLICATES NOT ALLOWED");
			return false;
		}
	}
	unitNameMap[newUnit->name] = newUnit;
	return true;
}

// adds a unit pointer to the the unitVrfUuidMap using name in the unit as key
// returns false if unit already exists; true otherwise
// Xerces is giving Entities twice so duplicate is two matches
bool C2SIMxmlHandler::addUnitByVrfUuid(Unit* newUnit) {
	if (unitVrfUuidMap[newUnit->vrfUuid] != nullptr) {
		newUnit->vrfUuidInitCount += 1;
		if (newUnit->vrfUuidInitCount > 1) {
			displayError("UNIT UUID:" + newUnit->uuid +
				" ALREADY ENTERED - DUPLICATES NOT ALLOWED");
			return false;
		}
	}
	unitVrfUuidMap[newUnit->vrfUuid] = newUnit;
	return true;
}

// returns true if the unitMap is empty
bool C2SIMxmlHandler::unitMapIsEmpty(){
	return unitMap.empty();
}

// builds an empty Unit with UUID from parameter
Unit* C2SIMxmlHandler::makeEmptyUnit(std::string UUID) {
	Unit* unit = new Unit;
	unit->uuid = UUID;
	unit->name = "";
	unit->vrfUuid = "";
	unit->opStatusCode = "";
	unit->strengthPercent = "";
	unit->hostilityCode = "";
	unit->echelon = "";
	unit->superiorUnit = "";
	unit->latitude = "";
	unit->longitude = "";
	unit->elevationAgl = "";
	unit->directionPhi = "";
	unit->directionPsi = "";
	unit->directionTheta = "";
	unit->symbolId = "";
	unit->disEntityType = "";
	unit->forceSideUuid = "";
	unit->systemName = "";
	unit->aggregateName = "";
	unit->vrfObjectHasBeenCreated = false;
	unit->routeIsComplete = false;
	unit->taskIsComplete = false;
	unit->reportFromSender = "";
	unit->reportToReceiver = "";
	unit->currentTaskUuid = "";
	unit->createdObject = false;
	unit->addInitCount = 0;
	unit->nameInitCount = 0;
	unit->vrfUuidInitCount = 0;
	unit->numberOfVehicles = 0;
	unit->reportCount = 0;
	unit->DISKind = 0;
	unit->DISDomain = 0;
	unit->DISCountry = 0;
	unit->DISCategory = 0;
	unit->DISSubcategory = 0;
	unit->DISSpecific = 0;
	unit->DISExtra = 0;
	return unit;
}

// returns iterator begin() for unitMap
std::map<std::string, Unit*>::iterator C2SIMxmlHandler::getUnitMapBegin(){
	return unitMap.begin();
}

// returns iterator end() for unitMap
std::map<std::string, Unit*>::iterator C2SIMxmlHandler::getUnitMapEnd(){
	return unitMap.end();
}

// builds an empty ForceSide with UUID from parameter
// blueForceName is in a global variable
BlueForceSide* C2SIMxmlHandler::makeEmptyForceSide() {
	BlueForceSide* blueForceSide = new BlueForceSide;
	blueForceSide->uuid = "";
	blueForceSide->otherSide1HostilityCode = "";
	blueForceSide->otherSide1Uuid = "";
	blueForceSide->otherSide1Name = "";
	blueForceSide->otherSide2HostilityCode = "";
	blueForceSide->otherSide2Uuid = "";
	blueForceSide->otherSide2Name = "";
	return blueForceSide;
}

// build an empty Task with UUID from parameter
Task* C2SIMxmlHandler::makeNewTask(){
	Task* newTask = new Task;
	newTask->taskUuid = "";
	newTask->taskName = "";
	newTask->orderUuid;
	newTask->taskRouteDtString = "";
	newTask->performingEntity;
	newTask->affectedEntity;
	newTask->taskeeUuid;
	newTask->actionTaskActivityCode;
	newTask->ruleOfEngagementCode = "";
	newTask->dateTime = "";
	newTask->simulationStartMs = 0;
	newTask->relativeDelayMs = 0;
	newTask->startAfterTaskUuid = "";
	newTask->locationPointCount = 0;
	newTask->pickupPointName = "";
	newTask->dropoffPointName = "";
	newTask->returnPointName = "";
	newTask->scriptedTaskUuid = NULL;
	return newTask;
}

// adds a Task to the taskMap
// returns false if Task already exists; true otherwise
bool C2SIMxmlHandler::addTask(Task* newTask){
	if (taskMap[newTask->taskUuid] != nullptr) {
		displayError("TASKID:" + newTask->taskUuid + 
			" ALREADY ENTERED - DUPLICATES NOT ALLOWED");
		return false;
	}
	taskMap[newTask->taskUuid] = newTask;
	if (newTask->taskName != "")
		taskNameMap[newTask->taskName] = newTask;
	newTask->orderUuid = thisOrderUuid;
	return true;
}

// adds a orderId to the orderIdMap
// returns false if Task already exists; true otherwise
bool C2SIMxmlHandler::addOrderId(std::string newOrderId) {
	if (orderIdMap[newOrderId] != "") {
		displayError("ORDERID:" + newOrderId +
			" ALREADY ENTERED - DUPLICATES NOT ALLOWED");
		skipThisDocument = true;
		return false;
	}
	orderIdMap[newOrderId] = newOrderId;
	return true;
}

// returns the task with parameter UUID associated with thisOrderUUID
Task* C2SIMxmlHandler::getTask(std::string taskUuid){
	return taskMap[taskUuid];
}

// returns the task with iterator taskIt associated with thisOrderUUID
Task* C2SIMxmlHandler::getTask(std::map<std::string, Task*>::iterator taskIt){
	return taskIt->second;
}

// returns the task with parameter name associated with thisOrderUUID
Task* C2SIMxmlHandler::getTaskByName(std::string name){
	return taskNameMap[name];
}

// returns the taskName of referenced task; if none returns ""
std::string C2SIMxmlHandler::getTaskNameByUuid(std::string taskUuid) {
	Task* getTask = taskMap[taskUuid];
	if (getTask != nullptr)return getTask->taskName;
	return "";
}

// returns iterator begin() for taskMap
std::map<std::string, Task*>::iterator C2SIMxmlHandler::getTaskMapBegin(){
	return taskMap.begin();
}

// returns iterator end() for taskMap
std::map<std::string, Task*>::iterator C2SIMxmlHandler::getTaskMapEnd(){
	return taskMap.end();
}

// return startAfterTaskUuid for referenced Task
std::string C2SIMxmlHandler::getStartAfterTaskUuid(std::string taskUuid) {
	Task* startAfterTask = taskMap[taskUuid];
	if(startAfterTask != nullptr)return startAfterTask->startAfterTaskUuid;
	return "";
}

// return route completion state for referenced Unit
bool C2SIMxmlHandler::getTaskRouteIsComplete(std::string unitUuid) {
	Unit* getUnit = unitMap[unitUuid];
	if(getUnit != nullptr)return getUnit->routeIsComplete;
	return false;
}

// return task completion state for referenced Unit
bool C2SIMxmlHandler::getTaskIsComplete(std::string unitUuid) {
	Unit* getUnit = unitMap[unitUuid];
	if(getUnit != nullptr)return getUnit->taskIsComplete;
	else return false;
}

// set routeIsComplete true for referenced Task
void C2SIMxmlHandler::setTaskRouteIsComplete(std::string unitUuid) {
	Unit* setUnit = unitMap[unitUuid];
	if (setUnit != nullptr)setUnit->routeIsComplete = true;
	else std::cerr << "ERROR SETTING ROUTE COMPLETION FOR UNIT; UUID:" << unitUuid << "\n";
}

// set taskIsComplete true for referenced Task
void C2SIMxmlHandler::setTaskIsComplete(std::string unitUuid) {
	Unit* setUnit = unitMap[unitUuid];
	if(setUnit != nullptr)setUnit->taskIsComplete = true;
	else std::cerr << "ERROR SETTING TASK COMPLETION FOR UNIT; UUID:" << unitUuid << " NOT FOUND\n";
}

// set currentTaskUuid in a Unit and clear Unit's routeIsComplete and  taskIsComplete
void C2SIMxmlHandler::setCurrentTaskUuid(std::string unitUuid, std::string newCurrentTask) {
	Unit* setUnit = unitMap[unitUuid];
	if (setUnit != nullptr) {
		setUnit->currentTaskUuid = newCurrentTask;
		setUnit->routeIsComplete = false;
		setUnit->taskIsComplete = false;
		return;
	}
	std::cerr << "ERROR SETTING CURRENT TASK FOR UNIT; UUID:" << unitUuid << " NOT FOUND\n";
}

// return currentTaskUuid for referenced Unit
std::string C2SIMxmlHandler::getCurrentTaskUuid(std::string unitUuid) {
	Unit* getUnit = unitMap[unitUuid];
	if (getUnit == nullptr)return "";
	return getUnit->currentTaskUuid;
}

// returns true if C2SIM OrderBody tag has been found
bool C2SIMxmlHandler::isC2simOrder(){
	return parsingC2simOrder;
}

// returns true if IBML Order tag has been found
bool C2SIMxmlHandler::isIbmlOrder(){
	return parsingIbmlOrder;
}

// returns true if C2SIM ReportBody tag has been found in this message
bool C2SIMxmlHandler::isC2simReport(){
	return parsingC2simReport;
}

// returns true if C2SIM TaskStatus tag has been found in message with ReportBody
bool C2SIMxmlHandler::isC2simTaskStatusReport() {
	return parsingC2simTaskStatusReport;
}

// returns true if IBML Report tag has been found
bool C2SIMxmlHandler::isIbmlReport(){
	return parsingIbmlReport;
}

// returns UUID associated with unitName in the unitMap
// returns "" if unit not found
std::string C2SIMxmlHandler::getUuidByName(std::string unitName) {
	Unit* unit = getUnitByName(unitName);
	std::string returnCode = "";
	if (unit != nullptr)
		returnCode = unit->uuid;
	return returnCode;
}

// returns hostility associated with unitName in the unitMap
std::string C2SIMxmlHandler::getHostilityCode(std::string unitName) {
	Unit* unit = getUnitByName(unitName);
	static std::string returnCode = "";
	if (unit != nullptr)
		returnCode = unit->hostilityCode;
	return returnCode;
}

// returns opStatusCode associated with unitName in the unitMap
std::string C2SIMxmlHandler::getOpStatusCode(std::string unitName){
	Unit* unit = getUnitByName(unitName);
	static std::string returnCode = "";
	if (unit != nullptr)
		returnCode = unit->opStatusCode;
	return returnCode;
}

// returns strengthPercent associated with unitName in the unitMap
std::string C2SIMxmlHandler::getStrength(std::string unitName){
	Unit* unit = getUnitByName(unitName);
	static std::string returnCode = "";
	if (unit != nullptr)
		returnCode = unit->strengthPercent;
	return returnCode;
}

// returns aggregate name associated with unit name in the unitMap
std::string C2SIMxmlHandler::getAggregateUnitName(std::string unitName) {
	Unit* unit = unitNameMap[unitName];
	if (unit == nullptr)return "";
	return unit->aggregateName;
}

// returns value of taskersIntent
std::string C2SIMxmlHandler::getTaskersIntent()
{
	return taskersIntent;
}

// returns value of dataTime
std::string C2SIMxmlHandler::getDateTime()
{
	return dateTime;
}

// returns value of taskeeUuid
std::string C2SIMxmlHandler::getTaskeeUuid()
{
	return thisTask->taskeeUuid;
}

// returns value of affectedEntity UUID
std::string C2SIMxmlHandler::getAffectedEntity(Task* task)
{
	return task->affectedEntity;
}

// returns value of actionTaskActivityCode
std::string C2SIMxmlHandler::getActionTaskActivityCode()
{
	return thisTask->actionTaskActivityCode;
}

// returns value of ruleOfEngagementCode
std::string C2SIMxmlHandler::getRuleOfEngagementCode()
{
	return thisTask->ruleOfEngagementCode;
}
 
// returns the count of Units from Military_Organization message
int C2SIMxmlHandler::getNumberOfUnits() {
	return unitMap.size();
}// end C2SIMxmlHandler::getNumberOfUnits() 


// returns the count of tasks for an order
int C2SIMxmlHandler::getNumberOfTasksThisOrder() {
	return tasksThisOrder;
}// end C2SIMxmlHandler::getNumberOfTasksThisOrder()


// return controlAction from last C2SIM_Simulation_Control message
std::string C2SIMxmlHandler::getSystemState() {
	return systemState;
}


// called by C2SIMinterface to start parse 
// resets for a new C2SIM document
void C2SIMxmlHandler::startC2SIMParse(std::string xmlStringRef)
{
	xmlToParse = xmlStringRef;

	// reset root tag
	rootTag = "";
	
	// reset logical flags and counts
	skipThisDocument = false;
	parsingC2simTask = false;
	parsingManeuverWarfareTask = false;
	skipThisDocument = false;
	foundRootTag = false;
	foundFinalTag = false;
	parsingEntity = false;
	parsingForceSide = false;
	parsingBlueForceSide = false;
	parsingSystemEntityList = false;
	parsingSystemCommandTypeCode = false;
	parsingC2simOrder = false;
	parsingIbmlOrder = false;
	parsingC2simReport = false;
	parsingIbmlReport = false;
	parsingC2simInitialize = false;
	parsingSystemMessage = false;
	parsingDISEntityType = false;
	currentMessageType = NONE;
	actorCount = 0;
	tasksThisOrder = 0;
	orderFromSender = "";
	orderToReceiver = "";
	currentTaskStatusCode = "";
	thisTask = nullptr;
	thisUnit = nullptr;
}

// transcode XMLCh* to char*, copy that to string output 
// then release the transcoded copy
// this seems inefficient but it's too hard to
// keep track of and release multiple encodings
void C2SIMxmlHandler::copyXMLChLessNs(std::string &output, const XMLCh* const input) {
	char* handoff = XMLString::transcode(input);
	output = handoff;
	int colonPosition = output.find(':');
	if (colonPosition != std::string::npos)
		output = output.substr(colonPosition + 1);
	XMLString::release(&handoff);
}

// called at beginning of new XML doc
void C2SIMxmlHandler::startDocument()
{
}

// called at end of parsing XML doc
void C2SIMxmlHandler::endDocument()
{
}

// called with tag at beginning of an element
void C2SIMxmlHandler::startElement(const XMLCh* const name,
	AttributeList& attributes)
{
	if (skipThisDocument)return; 

	// copy tag name parameter to startTag
	copyXMLChLessNs(startTag, name);
	//std::cout << "STARTTAG:" << startTag << "\n";
	
	// copy startTag to latestTag and ripple older ones
	oneBeforePreviousTag = previousTag;
	previousTag = latestTag;
	latestTag = startTag;

	// check for root tag
	if (!foundRootTag){

		rootTag = startTag;

		// could be IBML09
		if (parseIbml) {
			if (latestTag == "OrderPushIBML") {
				parsingIbmlOrder = true;
				foundRootTag = true;
				tasksThisOrder = 0;
				std::cout << "parsing IBML09 order\n";
				return;
			}
			if (latestTag == "BMLReport") {
				parsingIbmlReport = true;
				foundRootTag = true;
				std::cout << "parsing IBML09 report\n";
				return;
			}
			displayError("FOUND UNEXPECTED ROOT TAG:" + latestTag);
			skipThisDocument = true;
			return;
		}

		// all C2SIM root tags should be MessageBody
		if (latestTag != "MessageBody") {
			displayError("FOUND UNEXPECTED ROOT TAG:" + latestTag);
			skipThisDocument = true;
			return;
		}
		foundRootTag = true;
		return;
	}

	// next level tags we care about are:
	// - C2SIMInitializationBody
	// - SystemCommandBody
	// - DomainMessageBody with next level:
	//   - OrderBody
	//   - ReportBody
	// 
	// if at this point we are not in process of parsing one of the 
	// four types above, the startElement should be one of the first
	// three; if so, set state to the type we are parsing
	// if not, print the tag and ignore it
	//
	// if last tag was DomainMessageBody, this tag should be one
	// of OrderBody or ReportBody; if so, set the state to the 
	// type we are parsing; if not, print the tag and ignore it
	switch (currentMessageType) {

		// beginning to parse new message
		case NONE:
			if (startTag == "C2SIMInitializationBody" || startTag == "ObjectInitializationBody") {
				currentMessageType = INIT;
				parsingC2simInitialize = true;
				std::cout << "parsing C2SIM Initialization message\n";
			}
			else if (startTag == "SystemCommandBody") {
				currentMessageType = SYSTEM;
				parsingSystemCommandTypeCode = true;
				std::cout << "received C2SIM System Command message\n";
			}
			else if (startTag == "DomainMessageBody") {
				currentMessageType = DOMAINTYPE;
			}
			else if (startTag == "OrderPushIBML") {
				currentMessageType = IBMLORDER;
				parsingIbmlOrder = true;
			}
			else if (startTag == "BMLReport") 
				currentMessageType = IBMLREPORT;
			else if (startTag == "C2SIM_Simulation_Notification") 
				currentMessageType = NOTIFICATION;

			// no match, print error and quit parsing
			else if (currentMessageType == NONE){
				std::cout << "ERROR - CAN'T PARSE UNEXPECTED MESSAGE TYPE:" << startTag << "\n";
				skipThisDocument = true;
				return;
			}
			break;

		// parsing C2SIMInitialization message
		case INIT:
			C2SIMxmlHandler::parseC2simInitialize();
			break;

		// parsing SystemCommand message
		case SYSTEM:
			C2SIMxmlHandler::parseSystemCommand();
			break;

		// just started parsing DomainMessageBody
		// next determine whether Order or Report
		case DOMAINTYPE:
			if (startTag == "OrderBody") {
				currentMessageType = C2SIMORDER;
				parsingC2simOrder = true;
				thisTask = nullptr;
				std::cout << "parsing C2SIM Order\n";
				thisOrderUuid = "";
			}
			else if (startTag == "ReportBody") {
				currentMessageType = C2SIMREPORT;
				parsingC2simReport = true;
				std::cout << "parsing C2SIM Report\n";
			}
			else {
				displayError("CAN'T PARSE UNEXPECTED DOMAINMESSAGEBODY TYPE:" + startTag);
				skipThisDocument = true;
				return;
			}
			break;

		// parsing C2SIM Order message
		case C2SIMORDER:
			C2SIMxmlHandler::parseC2SIMOrder();
			break;

		// parsing C2SIM Report message
		case C2SIMREPORT:
			C2SIMxmlHandler::parseC2SIMReport();
			break;
			
		// parsing IBML09 Order
		case IBMLORDER:
			C2SIMxmlHandler::parseIBMLOrder();
			break;

		// parsing IBML09 Report
		case IBMLREPORT:
			skipThisDocument = true;
			break;

		// parsing Notification
		case NOTIFICATION:
			skipThisDocument = true;
			break;

		// lost track
		default:
			displayError("UNEXPECTED STATE PARSING XML : " + currentMessageType);

	}// end switch(currentMessageType)
}// end startElement()


// parse a C2SIM Order
void C2SIMxmlHandler::parseC2SIMOrder() {

	// already found <DomainMessage><OrderBody>
	// here we parse higher-level tags under that which have no data
	// (leaf tags with data are parsed under characters()

	// Task tag starts a new task
	if (latestTag == "Task"){
		parsingC2simTask = true;
			
		// increment Tasks count	
		tasksThisOrder++;
		if (thisTask == nullptr)
			thisTask = makeNewTask();
		foundTaskTag = true;
		actionTemporalAssociationCode = "";

		// fill the lat/lon string with zeros 
		// and elev with 100 (triggers ground clamping)
		for (int point = 0; point < MAXPOINTS; ++point) {
			thisTask->latitudes[point] = "0";
			thisTask->longitudes[point] = "0";
			thisTask->elevations[point] = "30";
		}
		return;
	
	}// end Task parse

	// parse ManeuverWarfareTask
	if (latestTag == "ManeuverWarfareTask"){
		if (!parsingC2simTask){
			displayError("MANEUVERWARFARETASK MUST FOLLOW TASK");
			skipThisDocument = true;
			return;
		}
		parsingManeuverWarfareTask = true;
	}

}// end void C2SIMxmlHandler::parseC2SIMOrder()

// parse a C2SIM Report
void C2SIMxmlHandler::parseC2SIMReport() {

	// only ReportBody we care about is TaskStatus
	parsingC2simTaskStatusReport = false;
	if (latestTag == "TaskStatus")
		parsingC2simTaskStatusReport = true;
	else if (latestTag == "PositionReportContent" || latestTag == "ObservationReportContent")
		skipThisDocument = true;
}

void C2SIMxmlHandler::parseIBMLOrder() {

	// IBML Task
	if (parsingIbmlOrder && startTag == "Task") {

		// increment Tasks count	
		tasksThisOrder++;
		if (thisTask == nullptr)
			thisTask = makeNewTask();
		foundTaskTag = true;
		foundPerformingEntity = false;

		// fill the lat/lon/elev string with zeros
		for (int point = 0; point < MAXPOINTS; ++point) {
			thisTask->latitudes[point] = "0";
			thisTask->longitudes[point] = "0";
			thisTask->elevations[point] = "0";
		}
		return;

	}// end IBML Task parse

	// parser may swallow tags without data - force the issue
	if (latestTag == "Unit") {
		characters(L"", 0);
	}
}// end C2SIMxmlHandler::parseIBMLOrder()

// parse a C2SIMInitialize
void C2SIMxmlHandler::parseC2simInitialize() {
	
	// ForceSide tag
	if (latestTag == "ForceSide") {
		parsingForceSide = true;
		return;
	}

	// Entity tag
	if (latestTag == "Entity") {
		parsingEntity = true;
		thisUnit = makeEmptyUnit(std::string(""));
		return;
	}

	// DISEntityType tag
	if (latestTag == "DISEntityType") {
		parsingDISEntityType = true;
		return;
	}

	// SystemEntityList tag
	if (latestTag == "SystemEntityList") {
		parsingSystemEntityList = true;
		actors.clear();
		return;
	}
	return;
}

// parse a SystemCommand
void C2SIMxmlHandler::parseSystemCommand() {

	// SystemCommandTypeCode tag
	if (latestTag == "SystemCommandTypeCode")
		parsingSystemCommandTypeCode = true;
	return;
}


// called when last tag of an element of XML has been parsed
void C2SIMxmlHandler::endElement(const XMLCh* const name)
{
	if (skipThisDocument)return;

	// copy name parameter to endTag
	copyXMLChLessNs(endTag, name);
	//std::cout << "ENDTAG:" << endTag << "|\n";
	
	// check whether it is the closing mirror of root tag
	// if so, skip any other input (e.g. translated order from server)
	if (!foundFinalTag) {
		if (parsingC2simInitialize) {
			if(endTag == "C2SIMInitializationBody") {
				foundFinalTag = true;
				skipThisDocument = true;
				parsingC2simInitialize = false;

				// confirm we found a blueForceSide and show
				// all (three?) ForceSides on the console 
				if (blueForceSide == nullptr) {
					displayError("BLUE FORCESIDE COULD NOT BE DETECTED");
					skipThisDocument = true;
					return;
				}
				else 
				{
					// match up ForceSide UUIDs with blueForceSide to get names
					if (blueForceSide->otherSide1Uuid == otherForceSideAUuid)
						blueForceSide->otherSide1Name = otherForceSideAName;
					else if (blueForceSide->otherSide1Uuid == otherForceSideBUuid)
						blueForceSide->otherSide1Name = otherForceSideBName;
					if (blueForceSide->otherSide2Uuid == otherForceSideAUuid)
						blueForceSide->otherSide2Name = otherForceSideAName;
					else if (blueForceSide->otherSide2Uuid == otherForceSideBUuid)
						blueForceSide->otherSide2Name = otherForceSideBName;

					// output ForceSides to console
					std::cout << "\nFORCESIDES:\n";
					std::cout << "BLUE: " << blueForceName << " FR " << blueForceSide->uuid << "\n";
					if (blueForceSide->otherSide1Uuid != "")
						std::cout << "OTHER1: " << blueForceSide->otherSide1Name << " " <<
						blueForceSide->otherSide1HostilityCode << " UUID:" << blueForceSide->otherSide1Uuid << "\n";
					if (blueForceSide->otherSide2Uuid != "")
						std::cout << "OTHER2: " << blueForceSide->otherSide2Name << " " <<
						blueForceSide->otherSide2HostilityCode << " UUID:" << blueForceSide->otherSide2Uuid << "\n\n";
				}// end else/if (blueForceSide == nullptr)

				// copy HostilityCode from blueForceSide to Unit on that ForceSide
				// scan all Units in the unitMap inserting hostility
				std::map<std::string, Unit*>::iterator unitIter;
				for (unitIter = unitMap.begin();
					unitIter != unitMap.end();
					++unitIter) {
					Unit* nextUnit = unitIter->second;
					if (nextUnit != nullptr) {
						if (blueForceSide->uuid == nextUnit->forceSideUuid)
							nextUnit->hostilityCode = "FR";
						if (blueForceSide->otherSide1Uuid == nextUnit->forceSideUuid)
							nextUnit->hostilityCode = blueForceSide->otherSide1HostilityCode;
						if (blueForceSide->otherSide2Uuid == nextUnit->forceSideUuid)
							nextUnit->hostilityCode = blueForceSide->otherSide2HostilityCode;
					}// end if(nextUnit != nullptr)
				}// end for(unitIter...
			}// end if(endTag == "C2SIMInitializationBody")

			// confirm we found a Blue ForceSide
			if (endTag == "ForceSide") {
				parsingForceSide = false;
				parsingBlueForceSide = false;
				if (blueForceSide != nullptr) {
					if (blueForceSide->uuid == "") {
						displayError("FORCESIDE IN C2SIMINITIALIZATION MISSING UUID");
						return;
						if (blueForceName == "") {
							displayError("FORCESIDE MISSING NAME");
							return;
						}
					}
				}
				else
				{
					displayError("BLUE FORCESIDE COULD NOT BE DETECTED");
					skipThisDocument = true;
					return;
				}
			}// end if (endTag == "ForceSide")

			// finish parsing an Entity
			if (endTag == "Entity") {
				parsingEntity = false;

				// end of a unit - map it
				if (thisUnit != nullptr) {
					if (thisUnit->uuid == "") {
						displayError("UNIT IN C2SIMINITIALIZATION MISSING UUID");
						return;
					}
					
					// these will print error and not 
					// add unit if it alrady exists
					addUnit(thisUnit);
					addUnitByName(thisUnit);
				}
			}// end if (endTag == "Entity")

			if (endTag == "DISEntityType") {
				parsingDISEntityType = false;
				thisUnit->disEntityType =
					std::to_string(thisUnit->DISKind) + "." +
					std::to_string(thisUnit->DISDomain) + "." +
					std::to_string(thisUnit->DISCountry) + "." +
					std::to_string(thisUnit->DISCategory) + "." +
					std::to_string(thisUnit->DISSubcategory) + "." +
					std::to_string(thisUnit->DISSpecific) + "." +
					std::to_string(thisUnit->DISExtra);
			}

			if (endTag == "SystemEntityList")
				parsingSystemEntityList = false;

		}// end if (parsingC2simInitialize)

		else if (parsingIbmlOrder) {
			if (endTag == "OrderPushIBML") {

				// if this ends a coordinate point, increment point index
				if (endTag == "WhereLocation") {
					if (thisTask == nullptr) {
						std::cout << "ERROR - Location tag outsside of Task\n";
						skipThisDocument = true;
						return;
					}
					if (locationPointCount >= MAXPOINTS) {
						std::cout << "ERROR - TOO MANY POINTS IN ROUTE (MAX " << MAXPOINTS << ")\n";
						skipThisDocument = true;
						return;
					}
					thisTask->locationPointCount++;
				}
				foundFinalTag = true;
				skipThisDocument = true;
			}
		}// end if (parsingIbmlOrder)

		else if (parsingC2simOrder) {
			if (endTag == "OrderBody") {
				lastOrderId = "OrderID>" + thisOrderUuid + "<";;
				foundFinalTag = true;
				skipThisDocument = true;
				if (thisOrderUuid == "") {
					displayError("ORDER WITH TASK:" + thisTask->taskName +
						" MISSING ORDERID");
					return;
				}

				// insert this OrderID in its Tasks
				for (std::map<std::string, Task*>::iterator taskIt = taskMap.begin();
					taskIt != taskMap.end();
					taskIt++) {
					Task* nextTask = taskIt->second;
					if (nextTask == nullptr)continue;
					if (nextTask->orderUuid == "")
						nextTask->orderUuid = thisOrderUuid;
				}
			}// end if (parsingC2simOrder) 

			if (endTag == "Task") {
				// save the sender & receiver for reporting
				Unit* performingUnit = unitMap[thisTask->taskeeUuid];
				if (performingUnit == nullptr) {
					displayError("TASK:" + thisTask->taskName +
						" PERFORMINGENTITY NOT INITIALIZED");
					skipThisDocument = true;
					delete thisTask;
					thisTask = nullptr;
					tasksThisOrder = 0;
					foundTaskTag = false;
					return;
				}
				if (thisTask->taskName == "") {
					displayError("TASK WITH UUID:" + thisTask->taskUuid +
						" MUST HAVE TASKNAME");
					skipThisDocument = true;
					delete thisTask;
					thisTask = nullptr;
					tasksThisOrder = 0;
					foundTaskTag = false;
					return;
				}
				if (thisTask->taskName.find(' ') != std::string::npos) {
					displayError("TASK NAME:" + thisTask->taskName +
						" CONTAINS A BLANK");
					skipThisDocument = true;
					delete thisTask;
					thisTask = nullptr;
					tasksThisOrder = 0;
					foundTaskTag = false;
					return;
				}
				// make sure the taskname os unique in order set
				for (std::map<std::string, Task*>::iterator tasknameIter = getTaskMapBegin();
					tasknameIter != getTaskMapEnd();
					++tasknameIter) {
					Task* iterTask = getTask(tasknameIter);
					if (iterTask == nullptr)break;
					if (getTask(tasknameIter)->taskName == thisTask->taskName) {
						displayError("TASK NAME:" + thisTask->taskName + " IS NOT UNIQUE");
						tasksThisOrder = 0;
						foundTaskTag = false;
						return;
					}
				}// end for(std::map<std""string...

				performingUnit->reportToReceiver = orderFromSender;
				performingUnit->reportFromSender = orderToReceiver;

				// enter task into taskMap
				if (!addTask(thisTask))
					skipThisDocument = true;
				foundTaskTag = false;
				thisTask = nullptr;
			}// end if (endTag == "Task")

			// if this ends a coordinate point, increment point index
			if (endTag == "Location") {
				if (thisTask == nullptr) {
					std::cout << "ERROR - Location tag outside of Task\n";
					skipThisDocument = true;
					return;
				}
				if (locationPointCount >= MAXPOINTS) {
					std::cout << "ERROR - TOO MANY POINTS IN ROUTE (MAX " << MAXPOINTS << ")\n";
					skipThisDocument = true;
					return;
				}
				thisTask->locationPointCount++;

			}// end if (endTag == "Location")
		}// end if (parsingC2simOrder)
	}// end if (!foundFinalTag) {

	// delete latestTag
	latestTag = "";

}// end C2SIMxmlHandler::endElement


 // character data
 // TODO: per documentation, this could come in pieces; 
 // all pieces should be collected before acting on it
void C2SIMxmlHandler::characters(
	const XMLCh* const data,
	const XMLSize_t dataLength) {

	copyXMLChLessNs(dataText, data);
	if (skipThisDocument)return;

	// to see tags and data parsed uncomment next line
	//std::cout << "TAG:" << latestTag << "|DATA:" << dataText << "|\n";
	
	// if parse conditions not met just return
	if (!foundRootTag || foundFinalTag || latestTag.empty())return;
	
	// look for order element tags and copy their data one tag at a time

	// starting here we parse the various possible XML inputs
	// for our purposes C2SIM and IBML09 are semantically equivalent
	// so we store the results in a single set of task variables
	
	// C2SIM Order
	if (parsingC2simOrder) {

		// look for sender and receiver
		if (orderFromSender == "") {
			if (latestTag == "FromSender")
				orderFromSender = dataText;
		}
		if (orderToReceiver == "") {
			if (latestTag == "ToReceiver")
				orderToReceiver = dataText;
		}

		// look for OrderID
		if (thisOrderUuid == "") {
			if (latestTag == "OrderID") {
				if (!addOrderId(dataText))return;
				thisOrderUuid = dataText;
				return;
			}
		}

		// don't start parsing Order before Task tag
		if (!foundTaskTag)return;

		// look for the TaskID
		if (thisTask->taskUuid == "") {
			if (latestTag == "UUID") {
				thisTask->taskUuid = dataText;
				return;
			}
		}

		// look for the task Name
		if (thisTask->taskName == "") {
			if (previousTag != "DateTime" && latestTag == "Name") {
				thisTask->taskName = dataText;
				return;
			}
		}

		// look for the PerformingEntity
		if (thisTask->taskeeUuid == "") {
			if (latestTag == "PerformingEntity") {
				thisTask->taskeeUuid = dataText;
				thisTask->performingEntity = dataText;
				return;
			}
		}

		// look for the AffectedEntity
		if (thisTask->affectedEntity == "") {
			if (latestTag == "AffectedEntity") {
				thisTask->affectedEntity = dataText;
				return;
			}
		}

		// look for TaskActionCode
		if (thisTask->actionTaskActivityCode == "") {
			if (latestTag == "TaskActionCode" || latestTag == "ManeuverWarfareTaskActionCode") {
				thisTask->actionTaskActivityCode = dataText;
				return;
			}
		}

		// look for WeaponRuleOfEngagementCode
		if (thisTask->ruleOfEngagementCode == "") {
			if (latestTag == "WeaponRuleOfEngagementCode") {
				thisTask->ruleOfEngagementCode = dataText;
				return;
			}
		}

		// look for DateTime
		if (thisTask->dateTime == "") {
			if (latestTag == "IsoDateTime") {
				thisTask->dateTime = dataText;
				return;
			}
		}

		// look for simulationStartTime
		if (thisTask->simulationStartMs == 0) {
			if (oneBeforePreviousTag == "SimulationTime" &&
				previousTag == "DelayTimeAmount" &&
				latestTag == "IsoTimeDuration") {
				thisTask->simulationStartMs = findTotalIsoMs(dataText);
				return;
			}
		}

		// look for relativeDelayTime
		if (thisTask->relativeDelayMs == 0) {
			if (oneBeforePreviousTag == "RelativeTime" &&
				previousTag == "DelayTimeAmount" &&
				latestTag == "IsoTimeDuration") {
				thisTask->relativeDelayMs = findTotalIsoMs(dataText);
				return;
			}
		}

		// look for the actionTemporalAssociationCode
		if (actionTemporalAssociationCode.length() == 0)
			if (latestTag == "ActionTemporalAssociationCode") {
		        actionTemporalAssociationCode = dataText;
				return;
			}

		// look for the startAfterTaskUuid
		if (actionTemporalAssociationCode == "STREND" && latestTag == "TemporalAssociationWithAction") {
			thisTask->startAfterTaskUuid = dataText; 
			return;
		}

		// look for latitude of next point
		if (latestTag == "Latitude") {
			thisTask->latitudes[thisTask->locationPointCount] = dataText;
			return;
		}

		// look for longitude of next point
		if (latestTag == "Longitude") {
			thisTask->longitudes[thisTask->locationPointCount] = dataText;
			return;
		}

		// look for elevation of next point
		if (latestTag == "AltitudeAGL") {
			thisTask->elevations[thisTask->locationPointCount] = dataText;
			return;
		}

		// must be some tag we don't care about
		dataText = "";

		return;

	}// end parsingC2simOrder

	// C2SIM Report
	else if (parsingC2simReport)
	{
		if (latestTag == "CurrentTask") {
			reportTask = getTask(dataText);

			// if task is unknown make an entry for it in the taskMap
			if (reportTask == nullptr) {
				reportTask = makeNewTask();
				reportTask->taskUuid = dataText;
			}
		}

		// extract Task status
		else if(reportTask != nullptr) {
			if(latestTag == "TaskStatusCode"){
				currentTaskStatusCode = dataText;

				// for TaskComplete report set Task completion flag in executing Unit
				if (currentTaskStatusCode == "TASKCMPLT") {
					setTaskIsComplete(reportTask->taskeeUuid);
					if (displayDebug)
						std::cout << "Task complete:" << reportTask->taskeeUuid << "\n";
				}
			}
		}
	}

	// IBML09 order
	else if (parsingIbmlOrder) {

		// don't start parsing Order before Task tag
		if (!foundTaskTag)return;
		if(thisTask == nullptr)thisTask = makeNewTask();

		// look for the TaskId
		if (thisTask->taskUuid == "") {
			if (latestTag == "TaskID") {
				thisTask->taskUuid = dataText;
				thisTask->taskName = dataText;
				return;
			}
		}

		// look for the taskeeUuid 
		if (thisTask->taskeeUuid == "") {
			if (latestTag == "TaskeeWho") {
				thisTask->taskeeUuid = dataText;
				return;
			}
		}
		
		// look for actionTaskActivityCode
		if (thisTask->actionTaskActivityCode == "") {
			if (latestTag == "WhatCode") {
				thisTask->actionTaskActivityCode = dataText;
				return;
			}
		}

		// look for dateTime
		if (thisTask->dateTime == "") {
			if (latestTag == "DateTime") {
				thisTask->dateTime = dataText;
				return;
			}
		}

		// look for latitude of next point
		if (latestTag == "Latitude") {
			thisTask->latitudes[thisTask->locationPointCount] = dataText;
			return;
		}

		// look for longitude of next point
		if (latestTag == "Longitude") {
			thisTask->longitudes[thisTask->locationPointCount] = dataText;
			return;
		}

		// look for elevation of next point
		if (latestTag == "ElevationAGL") {
			thisTask->elevations[thisTask->locationPointCount] = dataText;
			return;
		}

		// must be some tag we don't care about
		dataText = "";
		return;

	}// end else if (parsingIbmlOrder)

	// C2SIMInitialization message - only one instance per run
	else if (parsingC2simInitialize) {

		// ForceSide
		if (parsingForceSide) {

			if (blueForceSide == nullptr)blueForceSide = makeEmptyForceSide();

			// look for name - this is the blue ForceSide
			// if blueForceName is empty or matches this name
			if (latestTag == "Name") {
				if (blueForceName == "") {
					blueForceName = dataText;
					std::cout <<
						"using default first blue for name from first ForceSide in C2SIMInitializationBody:" <<
						dataText << "\n";
				}
				if (blueForceName == dataText)
					parsingBlueForceSide = true;
			}

			if (parsingBlueForceSide) {

				// look for UUID (it follows Nam ein schema)
				if (latestTag == "UUID")
						blueForceSide->uuid = dataText;

				// look for otherSideHostilityCodes
				if (latestTag == "HostilityStatusCode") {
					if (blueForceSide->otherSide1HostilityCode == "")
						blueForceSide->otherSide1HostilityCode = dataText;
					else if (blueForceSide->otherSide2HostilityCode == "")
						blueForceSide->otherSide2HostilityCode = dataText;
					else {
						displayError("THIS IMPLEMENTAION DOES NOT ALLOW > 2 FORCESIDERELATIONSS");
						skipThisDocument = true;
						return;
					}
				}

				// look for otherSideUuids
				if (latestTag == "OtherSide") {
					if (blueForceSide->otherSide1Uuid == "")
						blueForceSide->otherSide1Uuid = dataText;
					else if (blueForceSide->otherSide2Uuid == "")
						blueForceSide->otherSide2Uuid = dataText;
					else {
						displayError("THIS IMPLEMENTAION DOES NOT ALLOW > 2 FORCESIDERELATIONSS");
						skipThisDocument = true;
						return;
					}
				}
			} 
			else 
			{
				// ForceSide but not blue
				
				// get names and UUIDs of other ForceSides
				if (latestTag == "UUID") {
					if (otherForceSideAUuid == "")
						otherForceSideAUuid = dataText;
					else if (otherForceSideBUuid == "")
						otherForceSideBUuid = dataText;
				}
				// names will arrive in same order as UUIDs
				if (latestTag == "Name") {
					if (otherForceSideAName == "")
						otherForceSideAName = dataText;
					else if (otherForceSideBName == "")
						otherForceSideBName = dataText; 
				}
			}// end else/if(parsingBlueForceSide)

		}// end if(parsingForceSide)

		// Entity and all of Unit data
		else if (parsingEntity) {

			// look for UUID
			if (thisUnit->uuid == "") {
				if (latestTag == "UUID") {
					thisUnit->uuid = dataText;
					return;
				}
			}

			// look for Name
			if (thisUnit->name == "") {
				if (latestTag == "Name") {
					thisUnit->name = dataText;
					return;
				}
			} 

			// look for Side (this is a ForceSide, needed for HostilityCode)
			if (thisUnit->forceSideUuid == "") {
				if (latestTag == "Side") {
					thisUnit->forceSideUuid = dataText;
					return;
				}
			}

			// look for OperationalStatusCode
			if (thisUnit->opStatusCode == "") {
				if (latestTag == "OperationalStatusCode") {
					thisUnit->opStatusCode = dataText;
					return;
				}
			}

			// look for StrengthPercentage
			if (thisUnit->strengthPercent == "") {
				if (latestTag == "StrengthPercentage") {
					thisUnit->strengthPercent = dataText;
					return;
				}
			}

			// look for echelon
			if (thisUnit->echelon == "") {
				if (latestTag == "Echelon") {
					thisUnit->echelon = dataText;
					return;
				}
			}

			// look for SuperiorUnit
			if (thisUnit->superiorUnit == "") {
				if (latestTag == "Superior") {
					thisUnit->superiorUnit = dataText;
					return;
				}
			}

			// look for Latitude
			if (thisUnit->latitude == "") {
				if (latestTag == "Latitude") {
					thisUnit->latitude = dataText;
					return;
				}
			}

			// look for Longitude
			if (thisUnit->longitude == "") {
				if (latestTag == "Longitude") {
					thisUnit->longitude = dataText;
					return;
				}
			}

			// look for ElevationAGL
			if (thisUnit->elevationAgl == "") {
				if (latestTag == "ElevationAGL") {
					thisUnit->elevationAgl = dataText;
					return;
				}
			}

			// print error message if ElevationMSL is found
			if (latestTag == "ElevationMSL") {
				std::cout << "ERROR IN TASK: WE DO NOT USE ElevationMSL\n";
				skipThisDocument = true;
				return;
			}

			// look for Euler angle Phi
			if (thisUnit->directionPhi == "") {
				if (latestTag == "Phi") {
					thisUnit->directionPhi = dataText;
					return;
				}
			}

			// look for Euler angle Psi
			if (thisUnit->directionPsi == "") {
				if (latestTag == "Psi") {
					thisUnit->directionPsi = dataText;
					return;
				}
			}

			// look for Euler angle Theta
			if (thisUnit->directionTheta == "") {
				if (latestTag == "Theta") {
					thisUnit->directionTheta = dataText;
					return;
				}
			}

			// look for SymbolIdentifier
			if (thisUnit->symbolId == "") {
				if (latestTag == "SIDCString") {
					thisUnit->symbolId = dataText;
					return;
				}
			}

			// DISEntityType components
			if (parsingDISEntityType) {

				// look for DISKind
				if (thisUnit->DISKind == 0) {
					if (latestTag == "DISKind") {
						thisUnit->DISKind = std::stoi(dataText);
					}
				}

				// look for DISDomain
				if (thisUnit->DISDomain == 0) {
					if (latestTag == "DISDomain") {
						thisUnit->DISDomain = std::stoi(dataText);
					}
				}

				// look for DIScountry
				if (thisUnit->DISCountry == 0) {
					if (latestTag == "DISCountry") {
						thisUnit->DISCountry = std::stoi(dataText);
					}
				}

				// look for DISCategory
				if (thisUnit->DISCategory == 0) {
					if (latestTag == "DISCategory") {
						thisUnit->DISCategory = std::stoi(dataText);
					}
				}

				// look for DISSubcategory
				if (thisUnit->DISSubcategory == 0) {
					if (latestTag == "DISSubcategory") {
						thisUnit->DISSubcategory = std::stoi(dataText);
					}
				}

				// look for DISSpecific
				if (thisUnit->DISSpecific == 0) {
					if (latestTag == "DISSpecific") {
						thisUnit->DISSpecific = std::stoi(dataText);
					}
				}

				// look for DISExtra
				if (thisUnit->DISExtra == 0) {
					if (latestTag == "DISExtra") {
						thisUnit->DISExtra = std::stoi(dataText);
					}
				}

				// look for DISEntityType
				if (thisUnit->disEntityType == "") {
					if (latestTag == "EntityTypeString") {
						thisUnit->disEntityType = dataText;
						return;
					}
				}
			}// end if(parsingDISEntityType
		}//end if(parsingEntity)

        // SystemEntityList
		else if(parsingSystemEntityList) {

			// look for ActorReference
			if (latestTag == "ActorReference"){
				actors.push_back(dataText);
				return;
			}

			// look for SystemName
			if (latestTag == "SystemName") {

				// copy the SystemName to all associated actor units
				for (auto &actor : actors) {
					Unit* unitToUpdate = unitMap[actor];
					if (unitToUpdate == nullptr) {
						displayError("BAD SYSTEM ENTITY LIST- NO UNIT MATCHING ACTOR REFERENCE:"
							+ actor);
						skipThisDocument = true;
						rootTag = "";
					}
					else
						unitToUpdate->systemName = dataText;
				}
				return;
			}
		}// end else if (parsingSystemEntityList)

		// must be some tag we don't care about
		dataText = "";
		return;

	}// end else if(parsingC2simInitialization)
	
	// control message 
	else if (parsingSystemCommandTypeCode) {
		if (latestTag == "SessionStateCode") {
			systemState = dataText;
			return;
		}
		if (latestTag == "SystemCommandTypeCode") {
			systemState = dataText;
		}
	}//end else if(parsingSystemCommandTypeCode)

}// end C2SIMxmlHandler::characters

void C2SIMxmlHandler::fatalError(const SAXParseException& exception)
{
	char* message = XMLString::transcode(exception.getMessage());
	std::cerr << "Fatal Error: " << message <<
		" at line: " << exception.getLineNumber() << 
		" column:" << exception.getColumnNumber() << "\n";
	XMLString::release(&message);
}
