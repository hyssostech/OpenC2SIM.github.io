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

// c2simVRF 2.29 consistent with C2SIM standard schema

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma once
#include "C2SIMxmlHandler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <vector>
#include <cmath>
#include <string>
#include <condition_variable>
#include <thread>
#include <queue>
#include <math.h>
#include <stdexcept>
#include <tchar.h>

// VRLink and VRForces
#include <vlutil/vlProcessControl.h>
#include "textIf.h"
#include <windows.h>

// xercesc
#include "xerces_utils.h"
#include "xercesc/framework/MemBufInputSource.hpp"
#include "xercesc/parsers/XercesDOMParser.hpp"
#include "xercesc/dom/DOMException.hpp"	
#include "xercesc/dom/DOMElement.hpp"
#include "xercesc/util/OutOfMemoryException.hpp"

// SAX parser
#include "xercesc/parsers/SAXParser.hpp"
#include "xercesc/sax/HandlerBase.hpp"
#include "xercesc/util/XMLString.hpp"

// Boost
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

// STOMP & REST
#include "C2SIMClientLib2.h"

class C2SIMinterface
{
public:
	C2SIMinterface(
		DtTextInterface* textIf,
		std::string serverAddressRef,
		std::string stompPortRef,
		std::string restPortRef,
		std::string clientIDRef,
		bool useIbmlRef,
		int sendTrackingRef,
		bool debugMode,
		int reportInterval,
		std::string c2simVersionRef,
		bool repondToTimeMultipleC2SIM,
		std::string acceptedSendingSystemRef,
		std::string c2simVRFversionRef,
		bool usingHLA 
	);
	~C2SIMinterface();

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

	// export BML functions
	static void reportGenerator();
	static void stopReports();
	std::string C2SIMinterface::getOrderSender();
	std::string C2SIMinterface::getOrderReceiver();
	static void readStomp(
		C2SIMinterface* c2simInterface, 
		bool skipInitialize,
		std::string clientId,
		int reportIntervalRef);
	static std::string readAnXmlFile(std::string contents);
	static void setScenarioStart();
	static void writeAnXmlFile(std::string filename, std::string content);
	static bool getUnitGeodeticFromSim(
		Unit* unit,
		std::string &latString,
		std::string &lonString,
		std::string &elAglString);
	static void C2SIMinterface::executeTask(
		std::string taskId,
		DtTextInterface* textIf,
		bool skipInitialize,
		C2SIMinterface* c2simInterface,
		Task* thisTask); 
};
std::string doubleToString(double d);