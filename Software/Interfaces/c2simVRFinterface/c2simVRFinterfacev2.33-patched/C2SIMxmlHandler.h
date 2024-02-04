/*----------------------------------------------------------------*
|   Copyright 2021-2022 Networking and Simulation Laboratory      |
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
*-----------------------------------------------------------------*/

// c2simVRF 2.29s consistent with C2SIM_SMX_LOX_v11.xsd

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <iostream>
#include <map>

// xercesc
#include "xercesc/sax/HandlerBase.hpp"
#include "xercesc/sax/AttributeList.hpp"
#include "xercesc/sax/AttributeList.hpp"

// SaxParser
#include "xercesc/parsers/SAXParser.hpp"

// VRLink
#include <matrix/geodeticCoord.h>
#include <vrlinkNetworkInterface/vrlinkVrfRemoteController.h>

using namespace xercesc;

// size of strings and arrays
#define MAXPOINTS 100   // points in route

// initial tags found in parse

// data from an ObjectInitialization Unit
struct Unit {
	std::string uuid;
	std::string name;// NOTE: VRForces limits callback names to 10 chars
	std::string vrfUuid;
	std::string opStatusCode;
	std::string strengthPercent;
	std::string hostilityCode;
	std::string echelon;
	std::string superiorUnit;
	std::string latitude;
	std::string longitude;
	std::string elevationAgl;
	std::string directionPhi;
	std::string directionPsi;
	std::string directionTheta;
	std::string symbolId;
	std::string disEntityType;
	std::string forceSideUuid;
	std::string systemName;
	std::string aggregateName;
	std::string reportFromSender;
	std::string reportToReceiver;
	std::string currentTaskUuid;
	bool createdObject;
	bool vrfObjectHasBeenCreated;
	bool routeIsComplete;
	int vrfUuidInitCount;
	int numberOfVehicles;
	int reportCount;
	uint8_t DISKind;
	uint8_t DISDomain;
	uint16_t DISCountry;
	uint8_t DISCategory;
	uint8_t DISSubcategory;
	uint8_t DISSpecific;
	uint8_t DISExtra;
};

// data from an order (OrderBody or OrderPushIBML)
struct Task {
	std::string taskUuid;
	std::string taskName;
	std::string orderUuid;
	DtString taskVrfDtString;
	DtString taskRouteDtString;
	std::string performingEntity;
	std::string affectedEntity;
	std::string taskeeUuid;
	std::string actionTaskActivityCode;
	std::string ruleOfEngagementCode;
	std::string dateTime;
	int simulationStartMs;
	std::string startAfterTaskUuid;
	int relativeDelayMs;
	int locationPointCount;
	bool taskIsComplete;
	std::string pickupPointName;
	std::string dropoffPointName;
	std::string returnPointName;
	DtUUID scriptedTaskUuid;
	std::string mapGraphicUuid;
	std::string latitudes[MAXPOINTS];
	std::string longitudes[MAXPOINTS];
	std::string elevations[MAXPOINTS];
};

// various tactical graphics from PhysicaEntity
struct PhysicalLocation {
	std::string latitude;
	std::string longitude;
	std::string elevation;
};

struct PhysicalRoute {
	std::vector<PhysicalLocation*> locations;
};

struct Boundary {
	std::vector<PhysicalLocation*> locations;
};

struct Point {
	PhysicalLocation* pointLocation;
};

struct TacticalArea {
	PhysicalLocation* corner1;
	PhysicalLocation* corner2;
};

// data from ForceSideRelations
struct BlueForceSide {
	std::string uuid;
	std::string otherSide1HostilityCode;
	std::string otherSide1Uuid;
	std::string otherSide1Name;
	std::string otherSide2HostilityCode;
	std::string otherSide2Uuid;
	std::string otherSide2Name;
};

class C2SIMxmlHandler : public HandlerBase
{
	// globals

	// description of XML order
	std::string ibmlTaskersIntentTag = "TaskersIntent";
	std::string c2simDesiredEffectTag = "DesiredEffectCode";

public:
	C2SIMxmlHandler(bool useIbmlRef, bool displayDebugRef);
	~C2SIMxmlHandler();
	void C2SIMxmlHandler::displayError(std::string errorText);

	// parts of the parser
	int C2SIMxmlHandler::getMessageType();
	void C2SIMxmlHandler::parseC2SIMOrder();
	void C2SIMxmlHandler::parseC2SIMReport();
	void C2SIMxmlHandler::parseIBMLOrder();
	void C2SIMxmlHandler::parseC2simInitialize();
	void C2SIMxmlHandler::parseSystemCommand();
	double C2SIMxmlHandler::getSimMultiple();
	bool C2SIMxmlHandler::isC2simOrder();
	bool C2SIMxmlHandler::isIbmlOrder();
	bool C2SIMxmlHandler::isC2simReport();
	bool C2SIMxmlHandler::isC2simTaskStatusReport();
	bool C2SIMxmlHandler::isIbmlReport();
	std::string C2SIMxmlHandler::getOrderUuid();
	std::string C2SIMxmlHandler::getOrderSender();
	std::string C2SIMxmlHandler::getOrderReceiver();
	std::string C2SIMxmlHandler::getSystemState();
	std::string C2SIMxmlHandler::getSystemCommandTypeCode();
	std::string C2SIMxmlHandler::getTaskersIntent();
	std::string C2SIMxmlHandler::getDateTime();
	std::string C2SIMxmlHandler::getTaskeeUuid(Task* task);
	std::string C2SIMxmlHandler::getAffectedEntity(Task* task);
	std::string C2SIMxmlHandler::getActionTaskActivityCode(Task* task);
	std::string C2SIMxmlHandler::getMapGraphicID(Task* task);
	std::string C2SIMxmlHandler::getRuleOfEngagementCode(Task* task);
	std::string C2SIMxmlHandler::getRootTag();
	int C2SIMxmlHandler::getNumberOfUnits();
	std::string C2SIMxmlHandler::getOrderID();
	int C2SIMxmlHandler::getNumberOfTasksThisOrder();
	std::string C2SIMxmlHandler::getUuidByName(std::string unitName);
	std::string C2SIMxmlHandler::getHostilityCode(std::string unitName);
	std::string C2SIMxmlHandler::getOpStatusCode(std::string unitName);
	std::string C2SIMxmlHandler::getStrength(std::string unitName);
	std::string C2SIMxmlHandler::getAggregateUnitName(std::string unitName);
	Unit* C2SIMxmlHandler::getUnitByUuid(std::string uuid);
	Unit* C2SIMxmlHandler::getUnitByName(std::string name);
	Unit* C2SIMxmlHandler::getUnitByVrfUuid(std::string vrfUuid);
	bool C2SIMxmlHandler::unitMapIsEmpty();
	bool C2SIMxmlHandler::addUnit(Unit* newUnit);
	bool C2SIMxmlHandler::addUnitByName(Unit* newUnit);
	bool C2SIMxmlHandler::addUnitByVrfUuid(Unit* newUnit);
	std::string C2SIMxmlHandler::getSuperiorUnit(std::string uuid);
	std::string C2SIMxmlHandler::getReportFromSender(std::string uuid);
	std::string C2SIMxmlHandler::getReportToReceiver(std::string uuid);
	std::string C2SIMxmlHandler::getTaskStatusCode();
	std::map<std::string, Unit*>::iterator C2SIMxmlHandler::getUnitMapBegin();
	std::map<std::string, Unit*>::iterator C2SIMxmlHandler::getUnitMapEnd();
	Task* C2SIMxmlHandler::makeNewTask();
	bool C2SIMxmlHandler::addTask(Task*);
	bool C2SIMxmlHandler::addOrderId(std::string newOrderId);
	bool C2SIMxmlHandler::getTaskIsComplete(std::string taskUuid);
	Task* C2SIMxmlHandler::getTaskByUuid(std::string taskUuid);
	Task* C2SIMxmlHandler::getTask(std::map<std::string, Task*>::iterator);
	Task* getTaskByName(std::string name);
	std::string getTaskNameByUuid(std::string taskUuid);
	std::map<std::string, Task*>::iterator C2SIMxmlHandler::getTaskMapBegin();
	std::map<std::string, Task*>::iterator C2SIMxmlHandler::getTaskMapEnd();
	std::string C2SIMxmlHandler::getStartAfterTaskUuid(std::string taskUuid);
	bool C2SIMxmlHandler::getTaskRouteIsComplete(std::string taskUuid);
	void C2SIMxmlHandler::setTaskRouteIsComplete(std::string taskUuid);
	void C2SIMxmlHandler::setTaskIsComplete(std::string taskUuid);
	void C2SIMxmlHandler::setUnitCurrentTaskUuid(std::string unitUuid, std::string taskUuid);
	void C2SIMxmlHandler::setCurrentTaskUuid(std::string unitUuid, std::string newCurrentTaskUuid);
	std::string getCurrentTaskUuid(std::string unitUuid);
	void startElement(const XMLCh* const, AttributeList&);
	void fatalError(const SAXParseException&);
	void startDocument();
	void endDocument();
	void characters(const XMLCh* const, const XMLSize_t);
	void endElement(const XMLCh* const name);
	void startC2SIMParse(std::string xmlString);
	Unit* makeEmptyUnit(std::string UUID);
	int findTotalIsoMs(std::string isoTimeDuration);
	BlueForceSide* makeEmptyForceSide();
	std::string getScenarioDateTimeString();
	void insertTacticalGraphicType(std::string graphicType, std::string Uuid);
	std::string C2SIMxmlHandler::retrieveGraphicType(std::string graphicUUID);
	void C2SIMxmlHandler::retrieveMagic(std::string &uuid, std::string &lat, std::string &lon);

	// NOTE: Some of next 12 functions have side-effects!

	// makes an empty PhysicalRoute; 
	// leaves pointer to the PhysicalRoute in thisPhysicalRoute
	void C2SIMxmlHandler::makeRouteGraphic();
	// inserts thisPhysicalRoute in routesMap
	void C2SIMxmlHandler::insertThisRoute(std::string Uuid);
	// retrieves PhysicalRoute given its UUID
	PhysicalRoute* C2SIMxmlHandler::retrieveRoute(std::string Uuid);

	// makes an empty Boundary line; 
	// leaves pointer to the Boundary line in thisBoundary
	void C2SIMxmlHandler::makeBoundaryGraphic();
	// inserts thisBoundary in boundariesMap
	void C2SIMxmlHandler::insertThisBoundary(std::string Uuid);
	// retrieves Boundary given its UUID
	Boundary* C2SIMxmlHandler::retrieveBoundary(std::string Uuid);


	// makes  an empty Point; 
	// and pointer to the Point in thisPoint
	void C2SIMxmlHandler::makePointGraphic();
	// inserts thisPoint in pointsMap
	void C2SIMxmlHandler::insertThisPoint(std::string Uuid);
	// retrieves Point given its UUID
	Point* C2SIMxmlHandler::retrievePoint(std::string Uuid);

	// makes an empty TacticalArea; 
	// leaves pointer to the TacticalArea in thisTacticalArea
	void C2SIMxmlHandler::makeAreaGraphic();
	// inserts thisTacticalArea in tacticalAreasMap
	void C2SIMxmlHandler::insertThisArea(std::string Uuid);
	// retrieves TacticalArea given its UUID
	TacticalArea* C2SIMxmlHandler::retrieveTacticalArea(std::string Uuid);

private:

	// collection of all units received in C2SIMInitialization by UUID
	std::map<std::string, Unit*> unitMap;

	// version of unitMap that is mapped by name not UUID
	std::map<std::string, Unit*> unitNameMap;

	// map for VRForces UUIDs mapped to units
	std::map<std::string, Unit*> unitVrfUuidMap;

	// collection of all tasks received by UUID
	// (also those in Order.Task if skipping Initialization)
	std::map<std::string, Task*>taskMap;

	// version of taskMap that is mapped by name not UUID
	std::map<std::string, Task*> taskNameMap;

	// collection of all orderID received
	std::map<std::string, std::string>orderIdMap;

	// copies an XML char* to std::string after stripping off namespace:
	void C2SIMxmlHandler::copyXMLChLessNs(std::string &output, const XMLCh* const input);

	// index of tactical graphics types for lookup by UUID
	std::map<std::string, std::string> tacticalGraphicsMap;

	// collected routes for lookup by UUID
	std::map<std::string, PhysicalRoute*> routesMap;

	// collected boundaries for looukup by UUID
	std::map<std::string, Boundary*> boundariesMap;

	// collected points for lookup by UUID
	std::map<std::string, Point*> pointsMap;

	// collected tactical areas for lookup by UUID 
	std::map <std::string, TacticalArea*> tacticalAreasMap;
};
