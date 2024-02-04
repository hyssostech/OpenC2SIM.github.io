/*******************************************************************************
** Copyright (c) 2014 MAK Technologies, Inc
** All rights reserved
** GMU C4I & Cyber Ceneter built on MAK code to implement C2SIM-VRF interface
** in normal context GMU code would be open source but MAK licenses are 
** required to run this
*******************************************************************************/

// additions by GMU C4I & Cyber Center 
// c2simVRF consistent with C2SIM_CWIX2022v1.2.xsd
// updated to VRForces5.0.2 by JMP 7Mar23 
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// from Doug Reece for reportCallback
#include "vrfmsgs/reportMessage.h"
#include "vrftasks/taskCompleteReport.h"
#include "vrftasks/textReport.h"
#include "vrfMsgTransport/radioMessageListener.h"

// from Doug Reece for spotReportCallback
#include <vrftasks/radioMessageTypes.h>
#include <vrftasks/simReportCollection.h>
#include <vrftasks/spotReport.h>
#include <vrfMsgTransport/radioMessageListener.h>
#include <vrfmsgs/reportMessage.h>

// original testIf.cxx
#include "textIf.h"
#include <vlutil/vlPrint.h>
#include <vl/hostStructs.h>
#include <vlpi/disEnums.h>
#include <vrfMsgTransport/eventManager.h> // new in 5.0.1a

#include "vrfutil/backendMap.h"
#include "vrfutil/backendMapEntry.h"

#include "vrfplan/planBuilder.h"
#include "vrfutil/ceResourceOperator.h"
#include "vrfutil/appPathResolver.h"
#include "vrftasks/setHeadingRequest.h"
#include "vrftasks/setSpeedRequest.h"
#include "vrftasks/setDestroyRequest.h"
#include "vrftasks/moveToTask.h"
#include "vrftasks/waitDurationTask.h"
#include "vrfMsgTransport/vrfMessageInterface.h"

#include "vrfmsgs/ifResourceMonitorRequest.h"
#include "vrfmsgs/simInterfaceMessage.h"
#include "vrfmsgs/ifResourceMonitorResponse.h"
#include "vrfmsgs/ifRequestSnapshots.h"
#include "vrfmsgs/ifSnapshotsResponse.h"

#include <vrfplan/ifStmt.h>
#include <vrfplan/ifBlock.h>

#include <vrfplan/triggerStmt.h>
#include <vrfplan/triggerBlock.h>

#include <vrfplan/whileStmt.h>
#include <vrfplan/whileBlock.h>

#include <vrfMsgTransport/simControlEvent.h>// new in 5.0.1a

#include <vl/startResumeInteraction.h> // not in 5.0.1a
#include <vl/stopFreezeInteraction.h> // not in 5.0.1a

#include "C2SIMinterface.h" // C2SIM
#include "C2SIMClientLib2.h"// C2SIM

#include <time.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if DT_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if !WIN32
#include <sys/fcntl.h>
#else
#include <fcntl.h>
#endif
#if WIN32
#include <io.h>
#include <conio.h>
#endif
#include "vrfcontrol/remoteEnvironmentController.h" // new in 5.0.1a

#if !WIN32
#define UNBLOCK() setBlock(0, 0)
#define RESTORE() setBlock(0, 1)
#else
#define UNBLOCK()
#define RESTORE()
#endif

bool textIfUseIbml = false;             // C2SIM
bool textIfSendRedTracking = false;     // C2SIM
bool textIfSendBlueTracking = false;    // C2SIM
bool textIfSendRedObservations = false; // C2SIM
bool textIfSendBlueObservations = false;// C2SIM
bool simStarted = false;                // C2SIM
int maxBundleSizeTextIf = 10240;        ;// C2SIM
DtTextInterface* callbackTextIf;
bool bundlereports = false;             //C2SIM
int maxReportsPerBundleTextIf = 10;// nominal; STOMP may balk at larger values
int maxBundlingTimeMs = 2000; // in ms
DtExerciseConn* exConn;

void DtTextInterface::setStarted(std::string value){// C2SIM
	simStarted = (value == "RUNNING");
}

const char* terrainDir = "..\\userData\\terrains\\";
const char* scenariosDir = "..\\userData\\scenarios\\";

static bool monitorResourcesCallbackAdded = false;

namespace {
	std::string restServerAddress; // C2SIM
	std::string restPortNumber;    // C2SIM
	std::string clientIDCode;      // C2SIM
	static bool sendReports;       // C2SIM
	static bool debugMode;         // C2SIM
}

//-------------------------------------------------------
//Initialize command chart
//-------------------------------------------------------

typedef void (*CmdFunc)(char *, DtTextInterface *a);

struct Cmd
{
    char *name;
    CmdFunc func;
};

Cmd allCmds[] = {
  "list",                     DtTextInterface::listCmd,
  "load",                     DtTextInterface::loadCmd,
  "new",                      DtTextInterface::newCmd,
  "save",                     DtTextInterface::saveCmd,
  "create",                   DtTextInterface::createCmd,
  "set",                      DtTextInterface::setCmd,
  "task",                     DtTextInterface::taskCmd,
  "monitor",                  DtTextInterface::monitorResourcesCmd,
  "unmonitor",                DtTextInterface::removeMonitorResourcesCmd,
  "aggregate",                DtTextInterface::aggregateCmd,
  "delete",                   DtTextInterface::deleteCmd,
  "close",                    DtTextInterface::closeCmd,
  "rewind",                   DtTextInterface::rewindCmd,
  "bigroute",                 DtTextInterface::bigRouteCmd,
  "run",                      DtTextInterface::runCmd,
  "simrun",                   DtTextInterface::simrunCmd,
  "simpause",                 DtTextInterface::simpauseCmd,
  "listsnapshots",            DtTextInterface::listSnapshotsCmd,
  "takesnapshot",             DtTextInterface::takeSnapshotCmd,
  "turnonsnapshots",          DtTextInterface::turnOnSnapshotsCmd,
  "turnoffsnapshots",         DtTextInterface::turnOffSnapshotsCmd,
  "rollback",                 DtTextInterface::rollbackToSnapshotCmd,
  "pause",                    DtTextInterface::pauseCmd,
  "timemultiplier",           DtTextInterface::timemultiplierCmd,
  "quit",                     DtTextInterface::quitCmd,
  "q",                        DtTextInterface::quitCmd,
  "help",                     DtTextInterface::helpCmd,
  "h",                        DtTextInterface::helpCmd,
  "?",                        DtTextInterface::helpCmd,
};

int nCmds = DtNUMBER(allCmds);

unsigned int DtTextInterface::theNextAggregateId = 1;
unsigned int DtTextInterface::theNextSubordinateId = 1;

//////////////////////////////////////////////////////////////////////////

// This section makes C2SIM reports
// Added by JMP with help from Doug Reece

// strings composed of observations
// used to avoid repeating report
std::list<std::string>observationList;
std::string c2simVersionTif;

// pieces of ibml general status report
std::string ibml09GSRpart1(
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
	"<bml:BMLReport xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
	"xmlns:jc3iedm=\"urn:int:nato:standard:mip:jc3iedm:3.1a:oo:2.0\" "
	"xmlns:bml=\"http://netlab.gmu.edu/IBML\" "
	"xmlns:msdl=\"http://netlab.gmu.edu/JBML/MSDL\">"
	"<bml:Report>"
	"<bml:CategoryOfReport>StatusReport</bml:CategoryOfReport>"
	"<bml:TypeOfReport>GeneralStatusReport</bml:TypeOfReport>"
	"<bml:StatusReport><bml:GeneralStatusReport>"
	"<bml:ReporterWho><bml:UnitID>");
std::string ibml09GSRpart2(
	"</bml:UnitID></bml:ReporterWho> "
	"<bml:Hostility>");
std::string ibml09GSRpart3(
	"</bml:Hostility> "
	"<bml:Executer>"
	"<bml:UnitID>");
std::string ibml09GSRpart4(
    "</bml:UnitID></bml:Executer>"
    "<bml:OpStatus>");
std::string ibml09GSRpart4a(
    "</bml:OpStatus>"
	"<bml:WhereLocation><bml:GDC>"
	"<bml:Latitude>");
std::string ibml09GSRpart5(
	"</bml:Latitude>"
	"<bml:Longitude>");
std::string ibml09GSRpart6(
	"</bml:Longitude>"
	"<bml:ElevationAGL>0</bml:ElevationAGL>"
	"</bml:GDC></bml:WhereLocation>\n"
	"<bml:When>20070101000000.000</bml:When>"
	"<bml:ReportID>");
std::string ibml09GSRpart7(
	"</bml:ReportID>"
	"<bml:Credibility>"
	"<bml:Source>HUMINT</bml:Source>"
	"<bml:Reliability>A</bml:Reliability>"
	"<bml:Certainty>RPTFCT</bml:Certainty>"
	"</bml:Credibility>"
	"</bml:GeneralStatusReport></bml:StatusReport></bml:Report></bml:BMLReport>");

// pieces of C2SIM report preamble and postamble
std::string c2simReportPart1(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<MessageBody xmlns=\"http://www.sisostds.org/schemas/C2SIM/1.1\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xsi:schemaLocation=\"http://www.sisostds.org/schemas/C2SIM/1.1/C2SIM"
    " http://www.sisostds.org/schemas/C2SIM/1.1/C2SIM_Experimental.xsd\">"
    "<DomainMessageBody>"
    "<ReportBody>"
    "<FromSender>00000000-0000-0000-0000-000000000000</FromSender>"
    "<ToReceiver>00000000-0000-0000-0000-000000000000</ToReceiver>");
    // TODO: determine who is sender for bumdle
std::string c2simReportPart8a(
    "<ReportID>");
std::string c2simReportPart9(
    "</ReportID>"
    "<ReportingEntity>");
std::string c2simReportPart10(
    "</ReportingEntity>"
    "</ReportBody>"
    "</DomainMessageBody>"
    "</MessageBody>");

// pieces of C2SIM position report
std::string c2simPositionPart1(
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
	"<MessageBody xmlns=\"http://www.sisostds.org/schemas/C2SIM/1.1\" "
	"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
	"xsi:schemaLocation=\"http://www.sisostds.org/schemas/C2SIM/1.1/C2SIM"
	" http://www.sisostds.org/schemas/C2SIM/1.1/C2SIM_Experimental.xsd\">"
	"<DomainMessageBody>"
	"<ReportBody>"
	"<FromSender>"
    "<FromSender>00000000-0000-0000-0000-000000000000</FromSender>"
    "<ToReceiver>00000000-0000-0000-0000-000000000000</ToReceiver>");
    // TODO: determine who is sender for bumdle
std::string c2simPositionPart2(
	"<ReportContent>"
	"<PositionReportContent>"
	"<TimeOfObservation>"
	"<DateTime><IsoDateTime>");
std::string c2simPositionPart3(
	"</IsoDateTime></DateTime>"
	"</TimeOfObservation>"
    "<EntityHealthStatus>"
	"<OperationalStatus>"
	"<OperationalStatusCode>");
std::string c2simPositionPart6(
	"</OperationalStatusCode>"
	"</OperationalStatus>"
    "</EntityHealthStatus>"
    "<EntityHealthStatus>"
	"<Strength>"
	"<StrengthPercentage>");
std::string c2simPositionPart7(
	"</StrengthPercentage>"
	"</Strength>"
    "</EntityHealthStatus>"
    "<Location>"
    "<GeodeticCoordinate>"
    "<Latitude>");
std::string c2simPositionPart4(
    "</Latitude>"
    "<Longitude>");
std::string c2simPositionPart5(
    "</Longitude>"
    "</GeodeticCoordinate>"
    "</Location>"
	"<SubjectEntity>");
std::string c2simPositionPart8(
    "</SubjectEntity>"
    "</PositionReportContent>"
    "</ReportContent>");
std::string c2simPositionPart8a(
	"<ReportID>");
std::string c2simPositionPart9(
	"</ReportID>"
	"<ReportingEntity>");
std::string c2simPositionPart10(
	"</ReportingEntity>"
	"</ReportBody>"
	"</DomainMessageBody>"
	"</MessageBody>");

// pieces of C2SIM Task Status report
std::string c2simTaskStatusPart1(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<MessageBody xmlns=\"http://www.sisostds.org/schemas/C2SIM/1.1\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xsi:schemaLocation=\"http://www.sisostds.org/schemas/C2SIM/1.1/C2SIM"
    " http://www.sisostds.org/schemas/C2SIM/1.1/C2SIM_Experimental.xsd\">"
    "<DomainMessageBody>"
    "<ReportBody>"
    "<FromSender>00000000-0000-0000-0000-000000000000</FromSender>"
    "<ToReceiver>00000000-0000-0000-0000-000000000000</ToReceiver>");
// TODO: determine who is sender for bundle
std::string c2simTaskStatusPart2(
	"<ReportContent>"
	"<TaskStatus>"
	"<TimeOfObservation>"
	"<DateTime><IsoDateTime>");
std::string c2simTaskStatusPart3(
	"</IsoDateTime></DateTime>"
	"</TimeOfObservation>"
	"<CurrentTask>");
std::string c2simTaskStatusPart4(
	"</CurrentTask>"
	"<TaskStatusCode>");
std::string c2simTaskStatusPart5(
    "</TaskStatusCode>"
    "</TaskStatus>"
    "</ReportContent>");
std::string c2simTaskStatusPart5a(
	"<ReportID>");
std::string c2simTaskStatusPart6(
	"</ReportID>"
	"<ReportingEntity>");
std::string c2simTaskStatusPart7(
	"</ReportingEntity>"
	"</ReportBody>"
	"</DomainMessageBody>"
	"</MessageBody>");

// pieces of C2SIM observation report
std::string c2simObservationPart1(
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
	"<MessageBody xmlns=\"http://www.sisostds.org/schemas/C2SIM/1.1\" "
	"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
	"xsi:schemaLocation=\"http://www.sisostds.org/schemas/C2SIM/1.1/C2SIM"
	" http://www.sisostds.org/schemas/C2SIM/1.1/C2SIM_Experimental.xsd\">"
	"<DomainMessageBody>"
	"<ReportBody>"
	"<FromSender>00000000-0000-0000-0000-000000000000</FromSender>"
    "<ToReceiver>00000000-0000-0000-0000-000000000000</ToReceiver>");
    // TODO: determine who is sender for bumdle
std::string c2simObservationPart2(
	"<ReportContent>"
	"<ObservationReportContent>"
	"<TimeOfObservation>"
    "<DateTime><IsoDateTime>");
std::string c2simObservationPart3(
    "</IsoDateTime></DateTime>"
    "</TimeOfObservation>"
    "<Observation>"
    "<HealthObservation>"
    "<ActorReference>");
std::string c2simObservationPart3a(
    "</ActorReference>"
    "<EntityHealthStatus>"
    "<OperationalStatus>"
    "<OperationalStatusCode>");
std::string c2simObservationPart3b(
    "</OperationalStatusCode>"
    "</OperationalStatus>"
    "</EntityHealthStatus>"
    "</HealthObservation>"
    "</Observation>"
	"<Observation>"
	"<LocationObservation>"
	"<ActorReference>");
std::string c2simObservationPart4(
	"</ActorReference>"
	"<Location>"
	"<GeodeticCoordinate>"
	"<Latitude>");
std::string c2simObservationPart5(
	"</Latitude>"
	"<Longitude>");
std::string c2simObservationPart5a(
    "</Longitude>"
    "</GeodeticCoordinate>"
    "</Location>"
    "</LocationObservation>"
    "</Observation>"
    "<Observation>"
    "<NameObservation>"
    "<ActorReference>");
std::string c2simObservationPart6(
    "</ActorReference>"
    "<HostilityStatusCode>");
std::string c2simObservationPart6a(
    "</HostilityStatusCode>" 
    "<Name>");
std::string c2simObservationPart6b(
    "</Name>"
    "</NameObservation>"
    "</Observation>"
    "</ObservationReportContent>"
    "</ReportContent>");
std::string c2simObservationPart6c(
	"<ReportID>");
std::string c2simObservationPart7(
    "</ReportID>"
	"<ReportingEntity>");
std::string c2simObservationPart8(
	"</ReportingEntity>"
	"</ReportBody>"
	"</DomainMessageBody>"
	"</MessageBody>");

// accept reference to C2SIMxmlHandler
C2SIMxmlHandler* xmlHandler = nullptr;
void DtTextInterface::setC2SIMxmlHandler(C2SIMxmlHandler* xmlHandlerRef) {
	xmlHandler = xmlHandlerRef;
}

// collect Reports into batches to send
// could use this approach with all three report types {Position, Observation, TaskStatus} 
// as parts are the same before and after ReportContent but only Position Reports
// generate enough volume to make bumdling worthwhile, and Observation/Task Status have
// other reasons not to bundle. Note that for PositionReports, the ReportID applies to the
// whole bundle and is created when the bundle is sent.
std::string reportBundle = "";
long reportBundleCreationTimeMs = 0L;
int numberReportsInBundle = 0;
void DtTextInterface::sendC2simReport(
    std::string uuid,
    std::string reportContent) {
 
    // keep track of Report arrival time
    SYSTEMTIME st;
    GetSystemTime(&st);
    long reportArrivalMs = 
        3600000 * st.wHour + 60000 * st.wMinute + 1000 * st.wSecond + st.wMilliseconds;
 
    // if no bundle exists, make one and save its creation time
    if (reportBundle == "") {
        reportBundle = c2simReportPart1;// report preamble
        reportBundleCreationTimeMs = reportArrivalMs;
        numberReportsInBundle = 0;
    }
   
    // check bundle count - add provided reportContent if this completes bundle
    if ((reportBundle.size() + reportContent.size() < maxBundleSizeTextIf) ||
        reportContent == "") {
        reportBundle += reportContent;
        if (reportContent != "")numberReportsInBundle++;

        // if bundle is now full or reportConent = "" is received 
        // due to reminder thread, send the bundle
        if ((numberReportsInBundle >= maxReportsPerBundleTextIf) ||
            (reportContent == "")) {

            // add end pieces of a report, following last ReportContent
            reportBundle += // reportPostamble
                c2simReportPart8a + DtTextInterface::makeReportID() +
                c2simReportPart9 + uuid +
                c2simReportPart10;

            // send the bundle
            if (reportBundle.size() > 0) {
                sendRest(
                    true,
                    restServerAddress,
                    restPortNumber,
                    clientIDCode,
                    uuid,
                    reportBundle);

                // null out bundle storage
                reportBundle = "";
                reportBundleCreationTimeMs = 0L;
                numberReportsInBundle = 0;
            }
        }

        // if this was first in bundle, start thread to send a reminder 
        // after reportBundleCreationTime
        if (numberReportsInBundle == 1) {
            std::thread t4(&DtTextInterface::waitForBundle,
                reportBundleCreationTimeMs,
                uuid);
            t4.detach();
        }
    }
    
    // bundle size case - does it fit in the bundle?
    // if not, send bundle then use this content to start new bundle
    else if (reportBundle.size() + reportContent.size() >= maxBundleSizeTextIf) {

        // add end pieces of a report, following last ReportContent
        reportBundle += // reportPostamble
            c2simReportPart8a + DtTextInterface::makeReportID() +
            c2simReportPart9 + uuid +
            c2simReportPart10;

        // send the bundle
        if (reportBundle.size() > 0) {
            sendRest(
                true,
                restServerAddress,
                restPortNumber,
                clientIDCode,
                uuid,
                reportBundle);
        }

        // null out bundle storage
        reportBundle = c2simReportPart1 + reportContent;
        reportBundleCreationTimeMs = reportArrivalMs;
        numberReportsInBundle = 1;


        // this is first in bundle, so start thread to send a reminder 
        // after reportBundleCreationTime
        std::thread t4(&DtTextInterface::waitForBundle,
            reportBundleCreationTimeMs,
            uuid);
        t4.detach();
    }

}// end sendC2simReport()

// call sendC2simReport() bundling time after reportBundleCreationTime
void DtTextInterface::waitForBundle(
    long reportBundleCreationTimeMs, 
    std::string uuid) {

    // find time now
    SYSTEMTIME st;
    GetSystemTime(&st);
    long systimeNowMs = 
        3600000 * st.wHour + 60000 * st.wMinute + 1000 * st.wSecond + st.wMilliseconds;

    // wait until maxBundlingTime after reportBundleCreationTime
    long deltaT = reportBundleCreationTimeMs + maxBundlingTimeMs - systimeNowMs;
    if(deltaT >0)Sleep(deltaT);
    sendC2simReport(uuid, "");

}// end waitForBundle


// send REST message
bool skipSendingNextReport = false;
void DtTextInterface::sendRest(
	bool formatIsC2sim,
	std::string restServerAddress,
	std::string restPort,
	std::string clientID,
	std::string uuid,
	std::string report) {

    // if reports overrun channel
    if (skipSendingNextReport) {
        skipSendingNextReport = false;
        return;
    }
    
	try {
		// start a REST connection
		C2SIMClientREST_Lib* restClient;
		if (formatIsC2sim){
			std::string fromSender = xmlHandler->getReportFromSender(uuid);
			if (fromSender == "")fromSender = uuid;
			std::string toReceiver = xmlHandler->getReportToReceiver(uuid);
			if (toReceiver == "")toReceiver = xmlHandler->getSuperiorUnit(uuid);

            // reports from subordinate units may not have a C2SIM UUID - make them anonymous
            if (fromSender == "")fromSender = "00000000-0000-0000-0000-000000000000";
            if (toReceiver == "")toReceiver = "00000000-0000-0000-0000-000000000000";
			restClient = new C2SIMClientREST_Lib(fromSender, toReceiver, "Inform", c2simVersionTif);
		}
		else // use IBML constructor
			restClient = new C2SIMClientREST_Lib();
		restClient->setHost(restServerAddress);
		restClient->setPort(restPort);
		restClient->setSubmitter(clientIDCode);
		if (formatIsC2sim)
			restClient->setProtocol(SISOSTD);
		else
			restClient->setProtocol("BML");
		std::string restResponse = restClient->bmlRequest(report);
        if (debugMode) {
            if (report.find("ObservationReport") != string::npos)
                std::cout << "REPORT:" << report << "\n";
            std::cout << "RESPONSE:" << restResponse << "|\n";
        }
        // output the status from response
        std::string status, afterStatus;
        size_t afterStatusIndex = restResponse.find("<status>", 0);
        if (afterStatusIndex != std::string::npos) {
            afterStatus = restResponse.substr(afterStatusIndex);
            size_t statusIndex = afterStatus.find("</status>", 0);
            if (statusIndex != std::string::npos)
                status = afterStatus.substr(0, statusIndex + 9);
            else status = restResponse;
        }
        else status = restResponse;
    }
    catch (C2SIMClientException & e) {
        std::cout << "can't send report in textIf: " << e.getMessage() <<
            "\ncause.what():" << e.getCauseMessage() <<
            "\nreport text:\n" << report << "\n";
        skipSendingNextReport = true;
        return;
    }
	catch (const XMLException& e) {
		std::cout << "can't send XML in textIf: " << e.getMessage() << ":\n" <<
			report.substr(0,100) << "\n";
		return;
	}
	catch (...) {
		std::cout << "Report send failed in textIf: " << report.substr(0,100) << "\n";
		return;
	}	
}// end sendRest()


/**
*   internal function to capture current GMT date and time a strings
*/
std::string reportDateTime() {
	// use Microsoft system function
	SYSTEMTIME st;
	GetSystemTime(&st);
	char systime[13], sysdate[11];

	// format to Java SimpleDateFormat("yyyy-MM-dd HH:mm:ss,SSS")
	// example 2019-01-01T00:00:01Z
	//std::sprintf(systime, "%02d:%02d:%02d,%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::sprintf(systime, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
	std::sprintf(sysdate, "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
	std::string formatDate = sysdate;
	std::string formatTime = systime;
	std::string datePlusTime = formatDate.append("T").append(formatTime).append("Z");
	return datePlusTime;
}

// make a dummy UUID with unique reportID
static int reportIndex = 0;
std::string DtTextInterface::makeReportID() {
    /*	std::ostringstream ss;
        ss << std::setw(12) << std::setfill('0') << ++reportIndex;
        std::string reportCount = ss.str();
        std::string reportID = "00000000-0000-0000-0000-" + reportCount;
        return reportID; */
    return makeUuid();
}

// called by VRF when scenario closes (so we need to stop c2simVRFinterface)
void scenarioCloseCallback(const DtVrfObjectMessage* msg, void* usr)
{
	std::cout << "Scenario closed - shutting down c2simVRF interface\n";
	callbackTextIf->setTimeToQuit(true);
}

//////////////////////////////////////////////////////////////////////////
// Report callbacks from Doug Reece
//
// These two functions handle 3 report types; they could be factored into
// 3 functions, or combined into 1
void reportCallback(const DtVrfObjectMessage* msg, void* usr)
{
	if (!sendReports)return;
	if (msg)
	{
		DtReportMessage* reportMsg = (DtReportMessage*)msg;
		DtSimReportMessageType msgType = reportMsg->contentType();
		DtString completedTypeString("task-completed-report");
		DtString textTypeString("text-report");
		std::string msgString = msgType.string();

		// task completed comes here
		if (msgType.string() == completedTypeString)
        {
            //DtString printString; reportMsg->printDataToString(printString);
            //std::cout<<"TASK COMPLETE STRING:"<<printString.string() << "\n";
			DtTaskCompleteReport* taskCompleteReport =
				dynamic_cast<DtTaskCompleteReport*>(reportMsg->report());

			if (taskCompleteReport) // modified 15May21 by JMP
			{
				// the C2SIM task has two phases: travel along route then execute activity
				std::string reportUnitName = msg->transmitter().markingText(); 

                // lookup the task unit in unitMap; if not present try vrfUuid
                Unit* taskUnit = xmlHandler->getUnitByName(reportUnitName);
                if (taskUnit == nullptr) {

                    // look up the reportUnitName in the unitVrfUuidMap
                    taskUnit = xmlHandler->getUnitByVrfUuid("VRF_UUID:" + reportUnitName);
                    if (taskUnit == nullptr) {
                        std::cout << "CAN'T FIND UNIT TO MATCH THIS VRFUUID\n";
                        return;
                    }
                    reportUnitName = taskUnit->name;
                }
                std::cout << "TaskComplete received for VRFUUID:" << taskUnit->vrfUuid << " unit:" << 
                    taskUnit->name<< " task UUID:" << taskUnit->currentTaskUuid << "\n";

                // display result including C2SIM uuid
				std::string taskType = taskCompleteReport->taskCompleted().string();
				std::cout << "Task complete message from " << reportUnitName <<
					"  Task type completed:" << taskType << "  Unit UUID:" << taskUnit->uuid << "\n";

				// we observe that VRF task is complete when object arrives at destination
				xmlHandler->setTaskRouteIsComplete(taskUnit->uuid);
				std::string completedTaskUuid = xmlHandler->getCurrentTaskUuid(taskUnit->uuid);
                xmlHandler->setTaskIsComplete(completedTaskUuid);
                std::cout << "TASK COMPLETE:" << completedTaskUuid << "\n";
			}
		}
		else if (msgType.string() == textTypeString)
		{
			DtTextReport* textReport = dynamic_cast<DtTextReport*>(
				reportMsg->report());
			if (textReport)
			{
				char charReport[120];
                std::string hostilityCode = "", opStatusCode = "", strength = "";
				strncpy(charReport, textReport->text().c_str(), 119);
				charReport[119] = '\0';
				if (debugMode)
                //if(strstr(charReport,"OBSERVATION")!= nullptr)
					std::cout << "*****REPORT CALLBACK TEXT:" << charReport << "\n";
				char* keyword;
				char* name;
				char* categoryChars;
				char* latChars;
				char* lonChars;
				char* codeChars;
                char* opStatChars;
                char* sourceForceTypeChars;
			
				// The expected BLUFOR tracking report:
				// POSITION "entity name" <latitude in deg> <lon in deg>
				// Note that there must be 1 and only 1 space between 
				// POSITION and the quoted entity name.
                //
                // Reports are bundled for efficient transmit. As a result the 
                // ReportingEntity UUID is that of the last PositionReport
                // received. 
				keyword = strtok(charReport, " ");
				if (strcmp(keyword, "POSITION") == 0)
				{
					// extract name
					name = strtok(NULL, "\"");
					std::string objectName = std::string(name);
					latChars = strtok(NULL, " ");
					lonChars = strtok(NULL, " "); 

					// get hostility and OpStat from the unitMap
					hostilityCode = xmlHandler->getHostilityCode(objectName);
					opStatusCode = xmlHandler->getOpStatusCode(objectName);
					strength = xmlHandler->getStrength(objectName);

					// get UUID from the unitNameMap
					Unit* reportUnit = xmlHandler->getUnitByName(objectName);
					
					// components of Doug Reece's aggregates will not be in UnitMap
					// but we don't want to report on them - only their parent unit
					if (reportUnit == nullptr) return;

					// components of aggregate MobileIrregular are in the UnitMap
					// and the UnitMap entry createObjectName also has a
					// superiorUnit name - we want to report on aggregateName
					// and do it by emitting one report every time reports for
					// the count of components are received here
					std::string uuid = reportUnit->uuid;
					std::string aggregateName = reportUnit->aggregateName;
					
					// check whether this is part of an aggregate
					// if so report for one of the aggregate only and use parent name
					if (aggregateName != "") {
						Unit* parentUnit = xmlHandler->getUnitByName(reportUnit->aggregateName);
						if (++(parentUnit->reportCount) < parentUnit->numberOfVehicles)return;
						parentUnit->reportCount = 0;
						uuid = parentUnit->uuid;
						objectName = parentUnit->name;
					}

					// show report in output
					if(debugMode)
                       std::cout << "POSITION report for " << objectName << " " << hostilityCode <<
						"" << uuid << " " << latChars << "/" << lonChars << " " << "\n";
					
					// choose report format matching order received
					if ((hostilityCode == "HO" && textIfSendRedTracking) ||
                        ((hostilityCode == "FR" || hostilityCode == "AFR") && textIfSendBlueTracking))
					if (!textIfUseIbml) {// C2SIM
                        DtTextInterface::sendC2simReport(
                            uuid,
                            c2simPositionPart2 + reportDateTime() + // reporting time
                            c2simPositionPart3 + opStatusCode + // operational status
                            c2simPositionPart6 + strength + // strength percentage
                            c2simPositionPart7 + latChars +
                            c2simPositionPart4 + lonChars +
                            c2simPositionPart5 + uuid + 
                            c2simPositionPart8); //objectName 
					}
					else { // IBML09
						DtTextInterface::sendRest(false,
							restServerAddress,
							restPortNumber,
							clientIDCode,
							objectName,
							ibml09GSRpart1 + objectName +
							ibml09GSRpart2 + hostilityCode +
							ibml09GSRpart3 + objectName +
							ibml09GSRpart4 + latChars +
							ibml09GSRpart5 + lonChars +
							ibml09GSRpart6 + DtTextInterface::makeReportID() +
						    ibml09GSRpart7);
					}
				}// end if (strcmp(keyword, "POSITION")

				if (strcmp(keyword, "OBSERVATION") == 0) {
				
                    // we send these unbunled so the ReportID can be used as 
                    // ActorReference - necessary since we don't know OPFOR UUIDs

                    // extract forceType
                    sourceForceTypeChars = strtok(NULL, " ");
                    std::string sourceForceTypeString = string(sourceForceTypeChars);
                    std::string reportHostility = "";
                    
                    // extract name and find its UUID and hostility
					name = strtok(NULL, "\"");
					std::string objectName = std::string(name);
					std::string uuid = xmlHandler->getUuidByName(objectName);

                    // report hostility is opposite of reporter hostilityCode
                    // this is done from standpoint of red exercise adversary
                    // they see blue obervation reports or blue friendly
                    // who see red observation reports
                    if (sourceForceTypeString == "Friendly")reportHostility = "HO";
                    else if (sourceForceTypeString == "Opposing")reportHostility = "FR";
                    else return;

                    // filter out reports not configured
                    if (hostilityCode == "HO" && !textIfSendBlueObservations)return;
                    if (hostilityCode == "FR" && !textIfSendRedObservations)return;

					// extract object count
					int objectCount = atoi(strtok(NULL, " "));
                    
					// example textstring
					// OBSERVATION "AH-64" 2 vehicle  58.579000 16.206849 137 FullOp vehicle  58.579000 16.206924 137 NotOp 

					// loop through the data in text, generating reports
					for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex){

						// extract next set of observation parameters
						categoryChars = strtok(NULL, " ");
						if (categoryChars == NULL)break;
						std::string catString = string(categoryChars);
						latChars = strtok(NULL, " ");
						if (latChars == NULL)break;
						std::string latString = string(latChars);
						lonChars = strtok(NULL, " ");
						if (lonChars == NULL)break;
						std::string lonString = string(lonChars);
						codeChars = strtok(NULL, " ");
						if (codeChars == NULL)break;
						std::string codeString = string(codeChars);
                        opStatChars = strtok(NULL, " ");
                        if (opStatChars == NULL)break;
                        std::string opStatString = string(opStatChars);

						// catenate them and and add to list if first time seen
                        std::string listString = catString + " " + latString + " " + lonString +
                            " " + codeString + " " + opStatString;
						std::list<std::string>::iterator it;
						it = std::find(observationList.begin(), observationList.end(), listString);
						if (it != observationList.end())continue;
						observationList.insert(it, listString);
						std::cout << "OBSERVATION report from " << objectName << " hostility " << 
                            reportHostility << ": " << listString << "\n";
						std::string uniqueObjectName = catString + "_" + std::to_string(reportIndex);

						if (!textIfUseIbml) {// C2SIM

                            // translate OPSTAT to C2SIM
                            if (opStatString == "FullOp")opStatString = "FullyOperational";
                            else if (opStatString == "MostlyOp")opStatString = "MostlyOperational";
                            else if (opStatString == "PartlyOp")opStatString = "PartlyOperational";
                            else if (opStatString == "NotOp")opStatString = "NotOperational";
                            else opStatString = "";

                            //send the report - use reportID as ActorReference
                            std::string reportID = DtTextInterface::makeReportID();
                            DtTextInterface::DtTextInterface::sendRest(true,
                                restServerAddress,
                                restPortNumber,
                                clientIDCode,
                                uuid,
                                c2simObservationPart1 + uuid + // of reportingEntity
                                c2simObservationPart2 + reportDateTime() + // reporting time
                                c2simObservationPart3 + reportID + // used as ActorReference
                                c2simObservationPart3a + opStatString +
                                c2simObservationPart3b + reportID + // used as ActorReference
                                c2simObservationPart4 + latChars +
                                c2simObservationPart5 + lonChars +
                                c2simObservationPart5a + reportID + // used as ActorReference
                                c2simObservationPart6 + reportHostility +
                                c2simObservationPart6a + // uniqueObjectName
                                c2simObservationPart6b + reportID +
                                c2simObservationPart7 + uuid + // of reportingEntity
                                c2simObservationPart8);
                            std::cout << "SENT MAK OBSERVATION REPORT\n";
						}
                        else {// IBML09
                            // translate OPSTAT to IBML09
                            if (opStatString == "FullOp")opStatString = "OPR";
                            else if (opStatString == "MostlyOp")opStatString = "MOPS";
                            else if (opStatString == "PartlyOp")opStatString = "SOPS";
                            else if (opStatString == "NotOp")opStatString = "NOP";
                            
                            else opStatString = "";

                            // send the report
							DtTextInterface::sendRest(false,
								restServerAddress,
								restPortNumber,
								clientIDCode,
								objectName,
								ibml09GSRpart1 + uuid +
								ibml09GSRpart2 + reportHostility +
								ibml09GSRpart3 + uniqueObjectName +
								ibml09GSRpart4 + latChars +
								ibml09GSRpart5 + lonChars +
								ibml09GSRpart6);
						}
					}// end for(objectIndex...
				}// end if (strcmp(keyword, "OBSERVATION")
			}// end if (textReport)
		}// end if (msgType.string()
	}// end if (msg)
}// end reportCallback

int DtTextInterface::getMaxReportsPerBundle() {
    return maxReportsPerBundleTextIf;
}

void DtTextInterface::spotReportCallback(//const 
	DtVrfObjectMessage * msg, void * usr)
{
	const DtReportMessage* reportMsg = static_cast<const DtReportMessage*>(msg);
	// Make local copy so we can change the contact sources without messing with
	// the message that came though the callback.
	DtSimReportCollection* spotReports = static_cast<DtSimReportCollection*>(reportMsg->report()->clone(""));

	std::vector<DtSimReport*> reports = spotReports->reports();
	std::vector<DtSimReport*>::const_iterator iter = reports.begin();

	while (iter != reports.end())
	{
		DtSpotReport* spotRep = dynamic_cast<DtSpotReport*>(*iter);

		if (spotRep && spotRep->contactForce() == DtForceOpposing)
		{
			printf("Spotted: %s\n", spotRep->contact().markingText().c_str());
			DtEntityType contactType(spotRep->contactType());
			printf("  Kind: %d, domain: %d, category %d\n",
				contactType.kind(), contactType.domain(), contactType.category());
		}

		++iter;
	}

	delete spotReports;
}

// End report callbacks

// send a StatusReport to the server
void DtTextInterface::sendStatusReport(
	std::string senderUuid, 
	std::string statusCode,
	std::string taskUuid) {
	
	if (!textIfUseIbml) {// C2SIM

        // starting and end pieces of a report (exclusive of ReportContent
        // sent unbundled since it is critical to execution timing
        std::string taskReportPreamble = c2simTaskStatusPart1;
        std::string taskReportPostamble =
            c2simTaskStatusPart5a + DtTextInterface::makeReportID() +
            c2simTaskStatusPart6 + senderUuid + //objectName +
            c2simTaskStatusPart7;
		DtTextInterface::sendRest(true,
			restServerAddress,
			restPortNumber,
			clientIDCode,
			senderUuid,
			taskReportPreamble +
            c2simTaskStatusPart2 + reportDateTime() + // reporting time +
			c2simTaskStatusPart3 + taskUuid +         // taskID +
			c2simTaskStatusPart4 + statusCode +       // taskStatusCode +
			c2simTaskStatusPart5 + makeReportID() +   // unique idemtifier +
			c2simTaskStatusPart6 + senderUuid +       //objectName +
            taskReportPostamble);
        std::cout << "SENT TASK STATUS REPORT\n";
	}
}

// End of section added by JMP with help from Doug Reece

//////////////////////////////////////////////////////////////////////////

void DtTextInterface::helpCmd(char* s, DtTextInterface* a)
{
   if (!strncmp(s, "create", 6))
   {
      DtInfo("\n"
             "\tcreate waypoint <\"name\"> <p1>\n"
             "\tcreate route <\"name\"> <p1 p2 ... pn>\n"
             "\tcreate tank <p1>\n"
             "\tcreate aggregate <p1>\n"
             "\t\twhere px has form x,y,z\n");
   }
   else if (!strncmp(s, "set", 3))
   {
      DtInfo("\n"
             "\tset plan <\"name\">\n"
             "\tset label <objectId> <\"label\">\n"
             "\tset location <\"name\"> <pt>\n"
             "\tset fuel <\"name\"> <double>\n"
             "\tset target <\"name\"> <\"name\">\n"
             "\tset restore <\"name\"> \n");
   }
   else if (!strncmp(s, "task", 4))
   {
      DtInfo("\n"
             "\ttask patrolRoute <\"name\"> <\"route\">\n"
             "\ttask moveToPoint <\"name\"> <\"waypoint\">\n"
             "\ttask follow <\"name\"> <\"leader\">\n"
             "\ttask wait <\"name\"> <time>\n");
   }
   else if (!strncmp(s, "monitor", 4))
   {
      DtInfo("\nmonitor <\"resource\" | \"ALL\"> <\"entity name\"> [update time in seconds]\n");
   }
   else if (!strncmp(s, "unmonitor", 4))
   {
      DtInfo("\nunmonitor <\"resource\" | \"ALL\"> <\"entity name\">\n");
   }
   else if (!strncmp(s, "simrun", 4))
   {
      DtInfo("simrun [site:host]\n");
   }
   else if (!strncmp(s, "turnonsnapshots", 4))
   {
      DtInfo("turnonsnapshots <simulation time> <number to keep in memory>\n");
   }
   else if (!strncmp(s, "simpause", 4))
   {
      DtInfo("simpause [site:host]\n");
   }
   else if (!strncmp(s, "timemultiplier", 4))
   {
      DtInfo("timemultiplier number\n");
   }
   else if (!strncmp(s, "aggregate", 4))
   {
      DtInfo("\naggregate <name1> <name2> <name3>\n");
   }
   else
   {
      DtInfo("\n"
          "\tload <\"filename\">\n"
          "\tsave <\"filename\">\n"
          "\tnew <\"terrain db filename\">\n"
          "\tclose\n"
          "\trewind\n"
          "\trun\n"
          "\tpause\n"
          "\tsimrun\n"
          "\tsimpause\n"
          "\tlistsnapshots\n"
          "\ttakesnapshot\n"
          "\tturnonsnapshots <simulation time> <number to keep>\n"
          "\tturnoffsnapshots\n"
          "\trollback <simulation time>\n"
          "\ttimemultiplier <number>\n"
          "\tlist\n"
          "\tcreate [ waypoint | route | tank  | aggregate]\n"
          "\tdelete [ <object_name> | <object_id> ]\n"
          "\tset [ plan | label | location | fuel | target | restore ]\n"
          "\ttask [ patrolRoute | moveToPoint | follow | wait ]\n"
          "\taggregate <name1> <name2> <name3>\n"
          "\tmonitor <\"resource\" | \"ALL\"> <\"entity name\"> [update time in seconds]\n"
          "\tunmonitor <\"resource\" | \"ALL\"> <\"entity name\">\n"
          "\thelp <command_name>\n" 
          "\tquit\n");
   }
}

void DtTextInterface::quitCmd(char* s, DtTextInterface* a)
{
   a->setTimeToQuit(true);
}

void DtTextInterface::listSnapshotsCmd(char* s, DtTextInterface* a)
{
   a->listSnapshots();
}

void DtTextInterface::takeSnapshotCmd(char* s, DtTextInterface* a)
{
   a->controller()->takeSnapshot();
   DtInfo << "Snapshot taken" << std::endl;
}

void DtTextInterface::turnOnSnapshotsCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif

   if (!strcmp(str, "")) 
   {
      helpCmd("turnonsnapshots", NULL);
   }
   else
   {
      char *t1, *t2;
      t1 = strtok(str, " ");

      if (t1)
      {
         t2 = strtok(NULL, " ");

         if (t2)
         {
            DtInfo << "Turning on snapshots every " << std::max(1, atoi(t1)) << " seconds.  Storing " << std::max(1, atoi(t2)) << " snapshots." << std::endl;
            a->controller()->setPeriodicSnapshotSettings(true, std::max(1, atoi(t1)),
               std::max(1, atoi(t2)));
         }
         else
         {
            helpCmd("turnonsnapshots", NULL);
         }
      }
      else
      {
         helpCmd("turnonsnapshots", NULL);
      }
   }
}

void DtTextInterface::turnOffSnapshotsCmd(char* s, DtTextInterface* a)
{
   a->controller()->setPeriodicSnapshotSettings(false, -1, -1);
}

void DtTextInterface::rollbackToSnapshotCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif
   if (!strcmp(str, "")) return;

   a->controller()->rollbackToSnapshot(atof(str));
}

void DtTextInterface::listCmd(char* s, DtTextInterface* a)
{
   int i = 0;
   DtInfo("Listing known backends\n");
   DtList backends = a->controller()->backends();
   for (DtListItem* item = backends.first(); item; item = item->next())
   {
      DtSimulationAddress* addr = (DtSimulationAddress*) item->data();
      DtInfo("\t%d) %s\n", ++i, addr->string());
      if (i == 5) DtInfo("\n");
   }

   DtInfo("Current state: %s\n", DtControlTypeString(
      a->controller()->backendsControlState()));
}

void DtTextInterface::loadCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif
   if (!strcmp(str, "")) return;

   char *name;
   name = strtok(str, "\"");
   a->controller()->loadScenario(
      DtFilename(DtString(scenariosDir) + DtString(name)), missingBackends, 
      a);
}

bool DtTextInterface::missingBackends(DtBackendMap* map, 
   const std::set<DtSimulationAddress>& currentBackends, void* usr)
{
   DtWarn("Cannot load scenario due to missing backends:\n");
   DtListItem* item;
   for (item = map->first(); item; item = item->next())
   {
      DtBackendMapEntry* entry = (DtBackendMapEntry*)item->data();
      if (entry)
      {
         DtWarn("\t%s\n", entry->fromAddress().string());
      }
   }
   
   return false;
}

void DtTextInterface::bigRouteCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif

   int iPoints = 0;

   if (strlen(str) == 0)
   {
      iPoints = 800;
   }
   else
   {
      iPoints = atoi(str);
   }

   a->bigRoute(iPoints);
}

void DtTextInterface::bigRoute(int iPoints)
{
   srand(time(NULL));
   DtList verts;

   for (int i = 0; i < iPoints; i++)
   {
      verts.add(new DtVector((double)((double)rand() / (double)RAND_MAX) * 2000.,
         (double)((double)rand() / (double)RAND_MAX) * 2000., 100.));
   }

   controller()->createRoute(verts);

   DtListItem* item = verts.first();

   while (item)
   {
      delete (DtVector*)item->data();

      item = item->next();
   }
}

void DtTextInterface::saveCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif
   if (!strcmp(str, "")) return;

   if (!strstr(str, ".scn"))
      strcat(str, ".scn");

   char *name;
   name = strtok(str, "\"");
   a->controller()->saveScenario(
      DtFilename(DtString(scenariosDir) + DtString(name)));
}

void DtTextInterface::rewindCmd(char* str, DtTextInterface* a)
{
   a->controller()->rewind();
}

void DtTextInterface::closeCmd(char* s, DtTextInterface* a)
{
   a->controller()->unsubscribeAllPlans();
   a->controller()->removeAllPlanStatementCallbacks();
   a->controller()->removeAllPlanCompleteCallbacks();
   a->controller()->closeScenario();
}

void DtTextInterface::newCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif

   if (!strcmp(str, "")) 
   {
      DtInfo("Give me a terrain database, please!\n> ");
   }
   else
   {
      a->controller()->unsubscribeAllPlans();
      a->controller()->removeAllPlanStatementCallbacks();
      a->controller()->removeAllPlanCompleteCallbacks();
      char *name;
      name = strtok(str, "\"");
      DtFilename terrainFilename(DtString(terrainDir) + DtString(name));

      a->controller()->newScenario(terrainFilename, terrainFilename);
   }
}

void DtTextInterface::createCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif

   if (!strcmp(str, "")) 
   {
      DtInfo("Create what?!\n");
      helpCmd("create", NULL);
   }
   else if (!strncmp(str, "waypoint", 8))
   {
      a->createWaypoint(str);
   }
   else if (!strncmp(str, "route", 5))
   {
      a->createRoute(str);
   }
   else if (!strncmp(str, "tank", 4))
   {
      a->createTank(str);
   }
   else if (!strncmp(str,"aggregate",9))
   {
      a->createAggregate(str);
   }
}

void DtTextInterface::createAggregate(char* str)
{   
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.
   char *point;
   point = strtok(&str[9], " ");

   if (!point)
   {
      DtInfo("\tusage: aggregate <geocentric_location>\n"); 
      return;
   }

   DtVector vec;
   sscanf(point, "%lf,%lf,%lf", &vec[0], &vec[1], &vec[2]);
   //Here we are creating the indivual parts. We register with the vrfObjectCreatedCb callback because
   //we can't add the subordinates to our aggregate until they've actually been created. Timing can be tricky here if the indivual is 
   //created before the aggregate. In this simple example we create the aggregate entity first then it's subordinates.
   
   //When manaully creating aggregates and their components this timing must be considered. One means is to check if the aggregate has been created
   //and if not, keep a list of the individuals and in a callback once the desired aggregate has been created then add them to the aggregate's 
   //organization. 

   //Create the aggregate entity at desired position with name "tankPlt"
   DtString pltName = "Tank_Plt_" + DtString(theNextAggregateId++);
 
  controller()->createAggregate(
	   vrfObjectCreatedCb, 
	   (void*)myController,
	   DtEntityType(11, 1, 225, 3, 0, 0, 0), vec,
	   DtForceFriendly, 0, pltName);
  
   //Now create the individual tanks that will make up this aggregate
   DtString subName = "Plt_Sub_";

   controller()->createEntity(
      vrfObjectCreatedCb, (void*)myController,
      DtEntityType(1, 1, 225, 1, 1, 3, 0), vec,
      DtForceFriendly, 90.0,subName + DtString(theNextSubordinateId++));
   
   //Change the offet from each other to create an intial triangle formation
   vec[DtX]+=10;
   controller()->createEntity(
	   vrfObjectCreatedCb, (void*)myController,
	   DtEntityType(1, 1, 225, 1, 1, 3, 0), vec,
	   DtForceFriendly, 90.0,subName + DtString(theNextSubordinateId++));
   
   vec[DtY]+=10;
   controller()->createEntity(
	   vrfObjectCreatedCb, (void*)myController,
	   DtEntityType(1, 1, 225, 1, 1, 3, 0), vec,
	   DtForceFriendly, 90.0,subName + DtString(theNextSubordinateId++));
}

void DtTextInterface::resourcesProcessedCb(DtSimMessage* msg, void* usr)
{
   DtIfServerRequestResponse* s = (DtIfServerRequestResponse*) 
      ((DtSimInterfaceMessage*) msg)->interfaceContent();

   DtString str;
   if (s->type() == DtResourceMonitorResponseMessageType)
   {
      s->printDataToString(str);
      DtInfo("%s\n", str.string());
   }
}

void DtTextInterface::monitorResourcesCmd(char * str, DtTextInterface* a)
{
   if (!strcmp(str, "")) 
   {
      DtInfo("Monitor what?!\n");
      helpCmd("monitor", NULL);
   }
   else
   {
      char *res;
      char *entity;
      char *time;
      char* temp;
      int iTime = 10;

      res = strtok(str, "\"");
      temp = strtok(NULL, "\"");
      entity = strtok(NULL, "\"");
      time = strtok(NULL, "\"");

      DtIfResourceMonitorRequest req;

      req.setUUID(DtUUID(entity));

      if (time)
      {
         iTime = atoi(time);
      }

      if (iTime < 5)
      {
         DtInfo << "Time setting of " << iTime << " will cause unnecessary output which can hamper data entry.  Will set to 5 seconds\n";
         iTime = 5;
      }

      req.setMonitorPeriod(iTime);
      req.setMonitorBy(DtCondExprSimTimeType);

      if (strcmp(res, "ALL"))
      {
         req.addMonitoredItem(res);
      }

      if (!monitorResourcesCallbackAdded)
      {
         a->controller()->vrfMessageInterface()->addMessageCallback(
            DtResourceMonitorResponseMessageType, resourcesProcessedCb, a);

         monitorResourcesCallbackAdded = true;
      }

      a->controller()->vrfMessageInterface()->createAndDeliverMessage(DtSimSendToAll, req);
   }
}

void DtTextInterface::removeMonitorResourcesCmd(char * str, DtTextInterface* a)
{
   if (!strcmp(str, "")) 
   {
      DtInfo("unmonitor what?!\n");
      helpCmd("unmonitor", NULL);
   }
   else
   {
      char *res;
      char *entity;
      char* temp;

      res = strtok(str, "\"");
      temp = strtok(NULL, "\"");
      entity = strtok(NULL, "\"");
      temp = strtok(NULL, "\"");

      DtIfResourceMonitorRequest req;

      req.setUUID(DtUUID(entity));
      req.setMonitorPeriod(10);
      req.setMonitorBy(DtCondExprSimTimeType);

      if (strcmp(res, "ALL"))
      {
         req.addMonitoredItem(res);
      }

      req.setStopMonitoring(true);

      a->controller()->vrfMessageInterface()->createAndDeliverMessage(DtSimSendToAll, req);
   } 
}

void DtTextInterface::aggregateCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif

   if (!strcmp(str, "")) 
   {
      DtInfo("Aggregate what?!\n");
      helpCmd("aggregate", NULL);
   }
   else
   {
      char *e1, *e2, *e3, *temp;
      e1 = strtok(str, "\"");
      temp = strtok(NULL, "\"");
      e2 = strtok(NULL, "\"");
      temp = strtok(NULL, "\"");
      e3 = strtok(NULL, "\"");
      temp = strtok(NULL, "\"");
      
      DtList entities;
      if (e1) entities.add(new DtString(e1));
      if (e2) entities.add(new DtString(e2));
      if (e3) entities.add(new DtString(e3));

      a->controller()->createAggregate(
         DtEntityType(11, 1, 0, 3, 2, 0, 0), 
         DtVector(10.0, 10.0, 10.0),
         DtForceFriendly, 0.0, &entities);

      //empty list of entities
      DtListItem* next = nullptr;
      for (DtListItem* item = entities.first(); item; item = next)
      {
         next = item->next();
         delete (DtString*) entities.remove(item);      
      }
   }
}

void DtTextInterface::setCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif

   if (!strcmp(str, "")) 
   {
      DtInfo("Set what?!\n");
      helpCmd("set", NULL);
   }
   else if (!strncmp(str, "plan", 4))
   {
      a->setPlan(str);
   }
   else if (!strncmp(str, "label", 5))
   {
      a->setLabel(str);
   }
   else if (!strncmp(str, "location", 8))
   {
      a->setLocation(str);
   }
   else if (!strncmp(str, "fuel", 4))
   {
      a->setFuel(str);
   }
   else if (!strncmp(str, "target", 6))
   {
      a->setTarget(str);
   }
   else if (!strncmp(str, "restore", 7))
   {
      a->setRestore(str);
   }
}

void DtTextInterface::taskCmd(char* str, DtTextInterface* a)
{
#if WIN32
   str[strlen(str)]='\0';
#else
   str[strlen(str)-1]='\0';
#endif

   if (!strcmp(str, "")) 
   {
      DtInfo("Task what?!\n");
      helpCmd("task", NULL);
   }
   else if (!strncmp(str, "patrolRoute", 11))
   {
      a->patrolRoute(str);
   }
   else if (!strncmp(str, "moveToPoint", 11))
   {
      a->moveToPoint(str);
   }
   else if (!strncmp(str, "follow", 6))
   {
      a->follow(str);
   }
   else if (!strncmp(str, "wait", 4))
   {
      a->wait(str);
   }
}   

void DtTextInterface::deleteCmd(char* s, DtTextInterface* a)
{
#if WIN32
   s[strlen(s)]='\0';
#else
   s[strlen(s)-1]='\0';
#endif
 
   if (strchr(s, ':'))
   {
      a->controller()->deleteObject(DtUUID(s));
   }
   else
   {
      char* strname = strtok(s, "\"");
      a->controller()->deleteObject(DtUUID(strname));
   }
}

void DtTextInterface::runCmd(char* s, DtTextInterface* a)
{
   a->controller()->run();
}

void DtTextInterface::simrunCmd(char* s, DtTextInterface* a)
{   
   DtStartResumeInteraction ds;
   
#if WIN32
   s[strlen(s)]='\0';
#else
   s[strlen(s)-1]='\0';
#endif

   //if no arguments, send it to all backends
   if (strcmp(s, "") == 0) 
   {
      ds.setReceiverId(DtEntityIdentifier(DtSimSendToAll,0));
   }
   else
   {
      //get the backend id
      if (strchr(s, ':'))
      {         
         ds.setReceiverId(DtEntityIdentifier(DtSimulationAddress(s),0));
      }
      else
      {
         DtWarn << "Start who? (use site:host)";
         return;
      }
   }   
   ds.setSenderId(DtEntityIdentifier(a->controller()->simulationAddress(),0));
   exConn->sendStamped(ds);
}

void DtTextInterface::pauseCmd(char* s, DtTextInterface* a)
{    
   a->controller()->pause();
}

void DtTextInterface::simpauseCmd(char* s, DtTextInterface* a)
{ 
   DtStopFreezeInteraction ds;

#if WIN32
   s[strlen(s)]='\0';
#else
   s[strlen(s)-1]='\0';
#endif

   //if no arguments, send it to all backends
   if (strcmp(s, "") == 0) 
   {
      ds.setReceiverId(DtEntityIdentifier(DtSimSendToAll,0));
   }
   else
   {
      //get the backend id site:host
      if (strchr(s, ':'))
      {         
         ds.setReceiverId(DtEntityIdentifier(DtSimulationAddress(s),0));
      }
      else
      {
         DtWarn << "Stop who? (use site:host)";
         return;
      }
   }   
   ds.setSenderId(DtEntityIdentifier(a->controller()->simulationAddress(),0));
   exConn->sendStamped(ds);
}

void DtTextInterface::timemultiplierCmd(char* str, DtTextInterface* a)
{ 
   char* t;
   t = strtok(str, " ");
   double multiplier = atof(t);

   if (multiplier == 0.0)
   {  
      DtInfo("\tusage: timemultiplier <number>\n"); 
      return;
   }
   a->controller()->setTimeMultiplier(multiplier);
}

#ifdef _PowerUX
int strcasecmp(const char *s1, const char *s2)
{
     char *s1Temp = strdup(s1);
     char *s2Temp = strdup(s2);

     for (int strIndex= 0; strIndex< strlen(s1Temp); strIndex++)
        if ((64 < s1Temp[strIndex]) && (s1Temp[strIndex] < 91))
             s1Temp[strIndex] += 32;
     for (int strIndex= 0; strIndex< strlen(s2Temp); strIndex++)
        if ((64 < s2Temp[strIndex]) && (s2Temp[strIndex] < 91))
             s2Temp[strIndex] += 32;
     int retVal = (strcmp(s1Temp,s2Temp));
     free (s1Temp);
     free (s2Temp);
     return retVal;

}

#endif

#if WIN32
static char nbdf_buff[128];
static int nbdf_ptr=0;    
char *non_blocking_dos_fgets(char *buff, int size, FILE *f)
{
   while (_kbhit())
   {
      nbdf_buff[nbdf_ptr] = _getche();
      if (  nbdf_buff[nbdf_ptr] == '\r'
         || nbdf_buff[nbdf_ptr] == '\n'
         || nbdf_ptr >= size )
      {
         nbdf_buff[nbdf_ptr] = 0; //terminate string
         strcpy(buff,nbdf_buff);
         nbdf_ptr=0;
         DtInfo("\n");
         return(buff);
      }
      else if ( nbdf_buff[nbdf_ptr] == '\b')
      {
         if (nbdf_ptr > 0)
         {
            //Write out a blank space to erase whatever was there
            _putch(' ');
            //Move back before the space
            _putch('\b');
            --nbdf_ptr;
         }
      }
      else
      {
         ++nbdf_ptr;            
      }
    }
    return NULL;
}
#endif

#define LASTCHAR(s) (s[strlen(s)-1])
#define STERMINATE(s) do{ char *t=(s); if (*t) (s)++, *t='\0';} while(0)

/* returns pointer to next token separated by a sep character
 * it's similar to strtok, but does not use internal static memory,
 * and leaves nextp pointing to end of word
 * note that gettok modifies the contents of the
 * strings passed to it by replacing separators with '\0' */
char *gettok(char **nextp, char *sep)
{
    char *start, *end;

    start = *nextp;
    while (*start && strchr(sep, *start))
        start++;
    if (!*start)
        return NULL;
    end = start;
    while (*end && !strchr(sep, *end) )
        end++;
    *nextp = end;

    STERMINATE(*nextp);
    return start;
}


#if !WIN32
void setBlock(int fd, int on)
{
    static int blockf, nonblockf;
    static int first = 1;

    if (first) {
        first = 0;
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
            DtWarnPerror("fcntl(F_GETFL) failed");
        /* could use FNDELAY instead of FNONBLK, but then feof()
           will be confused */
        blockf = flags & ~O_NONBLOCK;
        nonblockf = flags | O_NONBLOCK;

    }

    if (fcntl(0, F_SETFL, on ? blockf : nonblockf) < 0)
        DtWarnPerror("fcntl(F_SETFL) failed");
}
#endif

Cmd *lookupCmd(char *cmdname)
{
    if (!cmdname)
        return NULL;
    for (int j=0; j<nCmds; j++)
#if !WIN32
        if (!strcasecmp(cmdname, allCmds[j].name))
#else
        if (!_stricmp(cmdname, allCmds[j].name))
#endif
            return allCmds+j;
    return NULL;
}

//constructor
DtTextInterface::DtTextInterface(// C2SIM
    makVrf::DtVrlinkVrfRemoteController* controller,
    std::string serverAddressRef,
    std::string restPortRef,
    std::string clientIDRef,
    bool useIbmlRef,
    int sendTrackingRef,
    bool sendReportsRef,
    bool debugModeRef,
    std::string c2simVersionRef,
    int sendObservationsRef,
    bool bundleReportsRef,
    int maxBundleSizeRef,
    int maxBundleReportCountRef,
    DtExerciseConn* exConnRef) :
 kbInterest(0), myTimeToQuit(0), 
    myController(controller),
  myRemoveListCallback(false)
{
   callbackTextIf = this;
   clientIDCode = clientIDRef;
   restServerAddress = serverAddressRef;
   restPortNumber = restPortRef;
   sendReports = sendReportsRef;
   debugMode = debugModeRef;
   exConn = exConnRef;
   textIfSendRedTracking = sendTrackingRef == 1 || sendTrackingRef== 2;
   textIfSendBlueTracking = sendTrackingRef == 0 || sendTrackingRef == 1;
   textIfSendRedObservations = sendObservationsRef == 1 || sendObservationsRef == 2;
   textIfSendBlueObservations = sendObservationsRef == 0 || sendObservationsRef == 1;
   c2simVersionTif = c2simVersionRef;
   maxBundleSizeTextIf = maxBundleSizeRef;
   maxReportsPerBundleTextIf = maxBundleReportCountRef;
   if (!bundleReportsRef)maxReportsPerBundleTextIf = 1; 
  
   DtInfo("Scenario directory: %s\n", scenariosDir); 
   DtInfo("Terrain directory: %s\n", terrainDir); 

   myController->addBackendDiscoveryCallback(backendAddedCb, NULL);
   myController->addBackendRemovalCallback(backendRemovedCb, NULL);

   myController->addBackendLoadedCallback(backendLoadedCb, NULL);
   myController->addScenarioLoadedCallback(ScenarioLoadedCb, NULL);

   myController->addBackendSavedCallback(backendSavedCb, NULL);
   myController->addScenarioSavedCallback(ScenarioSavedCb, NULL);

   kbInterest++;
   //prompt();

   //from Doug Reece
   DtVrfObjectMessageExecutive* msgExec = myController->objectMessageExecutive();
   // This is necessary to capture the taskComplete report messages, which go 
   // through a VRF internal message passing mechanism, but not the radio simulation
   msgExec->addMessageCallbackByCategory(
	   DtReportMessageType,
	   (DtVrfObjectMessageCallbackFcn)reportCallback,
	   this);

   // And this is necessary to get the text and spot reports, which go through the radio
   // mechanism. 
   myController->radioMessageListener()->
	   addMessageCallback(//DtTextReportType, 
	   (DtVrfObjectMessageCallbackFcn)reportCallback, (void*)0);
   //////// end Doug Reece code

   // Arrange callback when VRF scenario closes so we can close c2simVRFinterface
   msgExec->addMessageCallbackByCategory(
	   DtCloseScenarioMessageType,
	   (DtVrfObjectMessageCallbackFcn)scenarioCloseCallback,
	   this);
   
}

DtTextInterface::~DtTextInterface()
{
   myController->removeBackendDiscoveryCallback(backendAddedCb, NULL);
   myController->removeBackendRemovalCallback(backendRemovedCb, NULL);

   myController->removeBackendLoadedCallback(backendLoadedCb, NULL);
   myController->removeScenarioLoadedCallback(ScenarioLoadedCb, NULL);

   myController->removeBackendSavedCallback(backendSavedCb, NULL);
   myController->removeScenarioSavedCallback(ScenarioSavedCb, NULL);
}

void DtTextInterface::setTimeToQuit(bool yesNo)
{
   myTimeToQuit = yesNo;
}

bool DtTextInterface::timeToQuit() const
{
   return myTimeToQuit;
}


makVrf::DtVrlinkVrfRemoteController* DtTextInterface::controller() const
{
   return myController;
}

void DtTextInterface::readCommand(){}// C2SIM

void DtTextInterface::readStdin()
{
   char buff[256];
   int discardNext = 0;

   UNBLOCK();
#if !WIN32
   while(fgets(buff, sizeof(buff), stdin))
#else
   while (non_blocking_dos_fgets(buff, sizeof(buff), stdin)) 
#endif
   {
      if (myRemoveListCallback)
      {
         controller()->vrfMessageInterface()->removeMessageCallback(
            DtSnapshotsResponseMessageType, scenarioSnapshotResponseCb, this);
         myRemoveListCallback = false;
      }

   int discardThis = discardNext;
      discardNext = (LASTCHAR(buff) != 10);
      if(!discardThis)
      {
         /* RESTORE/UNBLOCK are not necessary if you know you
            won't terminate from within processCmd */
         RESTORE();
         processCmd(buff);
         if(!kbInterest)
         {
            return;
         }
         UNBLOCK();
      }
   }
   RESTORE();
   if(feof(stdin))
   {
      DtInfo("EOF\n");
      exit(0);
   }
}

void DtTextInterface::prompt()
{
  if (kbInterest)
    DtInfo("> ");
  fflush(stdout);
}

void DtTextInterface::processCmd(char *buff)
{
    char *cmdname = gettok(&buff, "\t \n");
    Cmd *cmd = lookupCmd(cmdname);
    if (cmd)
      (*cmd->func)(buff, this);
    else if (cmdname)
      DtWarn("VRF Remote controller: no such command: \"%s\"\n", cmdname);
    prompt();
}

void DtTextInterface::backendAddedCb(const DtSimulationAddress& addr, void* usr)
{
  DtInfo("\nBackend %s Added\n> ", addr.string());
}

void DtTextInterface::backendRemovedCb(const DtSimulationAddress& addr, void* usr)
{
  DtInfo("\nBackend %s Removed\n> ", addr.string());
}

void DtTextInterface::backendLoadedCb(const DtSimulationAddress& addr, void* usr)
{
  // DtInfo("\nBackend %s Loaded\n> ", addr.string());
}

void DtTextInterface::ScenarioLoadedCb(void* usr)
{
   DtInfo("\nScenario Loaded\n> ");
}

void DtTextInterface::backendSavedCb(const DtSimulationAddress& addr, DtSaveResult result, void* usr)
{
   DtInfo("\nBackend %s Saved (%s)\n> ", 
      addr.string(), result == DtSuccess? "success" : "failure");
}

void DtTextInterface::ScenarioSavedCb(void* usr)
{
   DtInfo("\nScenario Saved\n> ");
}

void DtTextInterface::vrfObjectCreatedCb(
   const DtString& name, const DtEntityIdentifier& id, const DtUUID& uuid, void* usr)
{
   static DtUUID aggId;
   std::cout << "OBJECT CREATED CALLBACK RECEIVED FOR:" << name << "\n";
   // NOTE: VRForces limits callback names to 10 chars

   //We want to check for the creation of the aggregate entity and keep a hold of its entity id so that
   //when it's subordinates are created we can add them to the aggregate's organization
   DtString search = name; 
   if( search.findString("Tank_Plt") >=0 )
   {
      DtInfo("\n aggregate has been created with name and id: \"%s\", %s\n> ", 
         name.string(), id.string());
		aggId = uuid;
   }
   else
   {
      DtInfo("\n object has been created with name and id: \"%s\", %s\n> ", 
         name.string(), id.string());
   }

   //If the name of our new object is one of tank platoon desired subordinates add it
   DtString sub = name;
   if ( sub.findString("Plt_Sub") >= 0 )
   {
      //We grab hold of the remote controller and add our individual to the aggregate
      DtVrfRemoteController* pVrfRemoteCtrl = (DtVrfRemoteController*)(usr);
      //This entity belongs to our aggregate, add it to the platoon
      if ( pVrfRemoteCtrl )
      {
		   pVrfRemoteCtrl->addToOrganization(uuid, aggId);
         DtInfo("\n %s has been added to aggregate with id:  %s\n> ",name.string(),aggId.string());
      }
   }

   // capture the VRF UUID associated with this name

   // we make Task object name by appending blank + descriptive string to taskName
   std::string strName(name), strTaskName(name);
   int firstBlank = strName.find(' ');
   if (firstBlank > 0)strTaskName = strName.substr(0, firstBlank);
   DtString vrfDtString = uuid.uuidString();

   // all callback objects come through here - we want only Unit entities
   Unit* updateUnit = xmlHandler->getUnitByName(strName);
   
   // keep a reference from the vrfUuid to the unit, using unit's unique name
   if (updateUnit)
   {
       updateUnit->vrfUuid = vrfDtString;
       if (strName.length() > 0) {
           xmlHandler->addUnitByVrfUuid(updateUnit);
       }
       std::cout << "controller created object with name:" << strName << "|" << vrfDtString << "|\n";

       // signal C2SIMinterface->create*() that VRF has created unit
	   // save the Unit UUID under VRF name key and mark as created
	   updateUnit->vrfUuid = vrfDtString;
	   std::string aggregateUuid = xmlHandler->getUuidByName(strName);
       updateUnit->createdObject = true;
   }

   // or the routes associated with Tasks
   else {
	   Task* updateTask = xmlHandler->getTaskByName(strTaskName);
	   if (updateTask != nullptr) {
 
		   // save the task route UUID
		   updateTask->taskVrfDtString = vrfDtString;
           updateTask->taskRouteDtString = vrfDtString;
	   }
   }
}// end vrfObjectCreatedCb()

void DtTextInterface::createWaypoint(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *point, *name;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");
   point = strtok(NULL, " ");

   if (!point || !name)
   {
      DtInfo("\tusage: create waypoint <\"name\"> <pt>\n"); 
      return;
   }

  DtVector vec;
  sscanf(point, "%lf,%lf,%lf", &vec[0], &vec[1], &vec[2]);

   controller()->createWaypoint(vrfObjectCreatedCb, (void*)"waypoint", vec, 
      name);
}

void DtTextInterface::createRoute(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name, *point;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");

   if (!name)
   {
      DtInfo("\tusage: create route <\"name\"> <p1 p2 ... pn>\n"); 
      return;
   }

   //create list of vertices
   DtList vertices;
   while ((point = strtok(NULL, " ")) != NULL)
   {        
      double x, y, z;
      sscanf(point, "%lf,%lf,%lf", &x, &y, &z);
      DtVector* vec = new DtVector(x, y, z);
      vertices.add(vec);
   }

   controller()->createRoute(vrfObjectCreatedCb, (void*)"route", vertices, 
      name);

   //empty list of vertices
   DtListItem* next = NULL;
   for (DtListItem* item = vertices.first(); item; item = next)
   {
      next = item->next();
      delete (DtVector*) vertices.remove(item);      
   }
}

void DtTextInterface::createTank(char* str)
{   
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *point;
   point = strtok(&str[4], " ");

   if (!point)
   {
      DtInfo("\tusage: tank <geocentric_location>\n"); 
      return;
   }

   DtVector vec;
   sscanf(point, "%lf,%lf,%lf", &vec[0], &vec[1], &vec[2]);

   controller()->createEntity(
      vrfObjectCreatedCb, (void*)"tank",
      DtEntityType(1, 1, 225, 1, 1, 3, 0), vec,
      DtForceFriendly, 90.0);
}

void DtTextInterface::patrolRoute(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name, *route;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");
   route = strtok(NULL, "\"");  //get past whitespace
   route = strtok(NULL, "\"");

   if (!name || !route)
   {  
      DtInfo("\tusage: task patrolRoute <\"name\"> <\"route\">\n"); 
      return;
   }

   controller()->patrolAlongRoute(DtUUID(name), DtUUID(route));
}

void DtTextInterface::moveToPoint(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name, *point;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");
   point = strtok(NULL, "\"");  //get past whitespace
   point = strtok(NULL, "\"");

   if (!name || !point)
   {  
      DtInfo("\tusage: task moveToPoint <\"name\"> <\"waypoint\">\n"); 
      return;
   }
   controller()->moveToWaypoint(DtUUID(name), DtUUID(point));
}

void DtTextInterface::follow(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name, *leader;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");
   leader = strtok(NULL, "\""); //get past white space
   leader = strtok(NULL, "\"");

   if (!name || !leader)
   {  
      DtInfo("\tusage: task follow <name> <leader>\n"); 
      return;
   }
   DtVector offset(10.0, 0.0, 0.0);
   controller()->followEntity(DtUUID(name), DtUUID(leader), offset);
}

void DtTextInterface::wait(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name, *t;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");
   t = strtok(NULL, " ");

   if (!name || !t)
   {  
      DtInfo("\tusage: task wait <\"name\"> <time>\n"); 
      return;
   }

   DtTime time = atof(t);
   controller()->waitDuration(DtUUID(name), time);
}


void DtTextInterface::setPlan(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name = strtok(str, " ");
   name = strtok(NULL, "\"");
   if (!name)
   {
      DtInfo("\tusage: set plan <\"entity name\">\n"); 
      return;
   }

   //Create a plan
   //This plan will contain four statements:
   //1. Set heading to 2.094395 radians
   //2. Set speed to 5 m/s
   //3. Wait 20 seconds
   //4. Move to waypoint "alpha"

   DtPlanBuilder planBuilder;

   DtWaitDurationTask waitTask;
   waitTask.setSecondsToWait(20.0);

   DtMoveToTask moveTask;

   //! Marking text will work as well as the UUID for the alpha waypoint.  To find the UUID
   //! in code:
   //!
   //! DtUUID alphaUUID = DtUUID::markingTextResolutionManager().mapMarkingTextToUUID("alpha");
   moveTask.setControlPoint(DtUUID("alpha"));

   DtSetHeadingRequest headingReq;
   headingReq.setHeading(2.094395);

   DtSetSpeedRequest speedReq;
   speedReq.setSpeed(5);

   //Create a conditional expression.
   //This is just a simple example of a condition that will always be
   //true when the entity is first created -- it checks to see if the fuel
   //is non-zero.
   DtCeResourceOperator conditionalExpression;
   conditionalExpression.setResource("fuel");
   conditionalExpression.setOper(">");
   conditionalExpression.setComparisonValue(0.0);

   DtPlanBuilder::SharedPlanBlockBuilder thenBlockBuilder, elseBlockBuilder;

   planBuilder.addIfCondition(conditionalExpression, thenBlockBuilder, elseBlockBuilder);

   thenBlockBuilder->addStatement(headingReq);
   thenBlockBuilder->addStatement(speedReq);
   thenBlockBuilder->addStatement(waitTask);
   thenBlockBuilder->addStatement(moveTask);

   DtSetDestroyRequest dest;

   elseBlockBuilder->addStatement(dest);

   //Assign the plan to the given entitiy
   controller()->assignPlanByName(DtUUID(name), planBuilder.plan());

   //Let's subscribe to the given entity's plan messages. 
   //This will let us know whether or "assignPlan" command 
   //was successfull.  Also, it will also notify us if any
   //changes were made to the plan.
   controller()->subscribePlan(DtUUID(name), planCb, NULL);

   //Let's register a callback for current statement
   //messages.  Our callback will be invoked whenever the entity
   //starts a new statement in the plan.
   controller()->addPlanStatementCallback(DtUUID(name), planStmtCb, NULL);

   //Let's also register a callback for when the entity completes
   //the plan.  Inside our callback, we unsubscribe to this entity's
   //plan messages and unregister our current statement callback.
   controller()->addPlanCompleteCallback(DtUUID(name), planCompleteCb, controller());
}

void printIndent(int i)
{
   i = i * 4;

   while (i--)
   {
     DtInfo(" ");
   }
}

void DtTextInterface::printBlock(const DtSimBlock* block, int indent)
{
   DtSimStatement* stmt = block->firstStmt();

   while (stmt)
   {
      printStatement(stmt, indent + 1);
      stmt = stmt->nextStmt();
   }
}

void DtTextInterface::printStatement(const DtSimStatement* stmt, int indent)
{
   const DtIfStmt* ifStmt = dynamic_cast<const DtIfStmt*>(stmt);

   printIndent(indent);
   if (ifStmt)
   {
      DtInfo("%s\n", stmt->stringRep(0).string());
      if (ifStmt->thenBlock())
      {
         printBlock(ifStmt->thenBlock());
      }

      if (ifStmt->elseBlock())
      {
         DtInfo("\n");
         printIndent(indent);
         DtInfo("else\n");
         printBlock(ifStmt->elseBlock());
      }

      printIndent(indent);
      DtInfo("%s\n", stmt->stringRep().string());
      return;
   }
   else
   {
      const DtTriggerStmt* trigger = dynamic_cast<const DtTriggerStmt*>(stmt);

      if (trigger)
      {
         DtInfo("%s\n", stmt->stringRep(0).string());
         printBlock(trigger->triggerBlock());
         DtInfo("\n");
         return;
      }
      else
      {
         const DtWhileStmt* whileStmt = dynamic_cast<const DtWhileStmt*>(stmt);
         
         if (whileStmt)
         {
            DtInfo("%s\n", stmt->stringRep(0).string());
            printBlock(whileStmt->whileBlock());
            DtInfo("\n");
            return;
         }
      }
   }

   DtInfo("%s\n", stmt->stringRep().string());
}

void DtTextInterface::planCb(const DtUUID& name, 
   const std::vector<DtSimStatement*>& stmts, bool append, void* usr)
{
   //Let's print the statements in the plan.
   //Because plans can be large, their statements may arrive in separate plan 
   //messages.  The append flag let's us know whether these statements belong to 
   //a plan we already have.

   if (append)
   {
      DtInfo("\nAdditional plan statements for %s.\n", name.string());
   }
   else
   {
      DtInfo("\nNew plan statements for %s.\n", name.string());
   }

   int i = 0;
   std::vector<DtSimStatement*>::const_iterator iter = stmts.begin();

   while (iter != stmts.end())
   {
      DtSimStatement* statement = *iter;
	   DtInfo("%d)\n", ++i);
      
      printStatement(statement);

      ++iter;
   }
   DtInfo("\n> ");
}

void DtTextInterface::planStmtCb(const DtUUID& id, DtPlanStatus status,
   void* usr)
{
   //Let's print the statement that 'id' is executing.  A statementId = 0
   //indicates that the 'id' hasn't started its plan yet.
   if (status.currentStatementId > 0) 
   {
      DtInfo("\n%s executing statement %d\n> ", id.string(),
        status.currentStatementId);
   }
}

void DtTextInterface::planCompleteCb(const DtUUID& id, void* usr)
{
   //Let's print that 'id' has completed the plan.
   DtInfo("\n%s has completed its plan!\n> ", id.string());

   //Because the plan we assigned is now complete, we no longer need the
   //plan callbacks we registered.  Let's unregister them here.
   DtVrfRemoteController* controller = (DtVrfRemoteController*) usr;
   controller->unsubscribePlan(id, planCb, NULL);
   controller->removePlanCompleteCallback(id, planCompleteCb, NULL);
   controller->removePlanStatementCallback(id, planStmtCb, NULL);
}

void DtTextInterface::setLabel(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *objId, *label;
   objId = strtok(str, " ");
   objId = strtok(NULL, " ");
   label = strtok(NULL, "\"");

   if (!objId || !label)
   {
      DtInfo("\tusage: set label <object id> <\"label\">\n"); 
      return;
   }

   controller()->setLabel(DtUUID(objId), label);
}

void DtTextInterface::setLocation(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name, *point;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");
   point = strtok(NULL, " ");

   if (!name || !point)
   {
      DtInfo("\tusage: set location <\"name\"> <pt>\n"); 
      return;
   }

   DtVector vec;
   sscanf(point, "%lf,%lf,%lf", &vec[0], &vec[1], &vec[2]);

   controller()->setLocation(DtUUID(name), vec);
}

void DtTextInterface::setFuel(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name, *amount;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");
   amount = strtok(NULL, " ");

   if (!name || !amount)
   {  
      DtInfo("\tusage: set fuel <\"name\"> <double>\n"); 
      return;
   }

   double fa = atof(amount);
   controller()->setResource(DtUUID(name), "fuel", fa);
}

void DtTextInterface::setTarget(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name, *target;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");
   target = strtok(NULL, "\"");  //get past whitespace
   target = strtok(NULL, "\"");

   if (!name || !target)
   {
      DtInfo("\tusage: set target <\"name\"> <\"name\">\n"); 
      return ;
   }

   controller()->setTarget(DtUUID(name), DtUUID(target));
}

void DtTextInterface::setRestore(char* str)
{
   //Let's parse the command string and then call
   //the corresponding method on DtVrfRemoteController.

   char *name;
   name = strtok(str, " ");
   name = strtok(NULL, "\"");

   if (!name)
   {
      DtInfo("\tusage: set restore <\"name\"> \n"); 
      return;
   }
   controller()->restore(DtUUID(name));
}

void DtTextInterface::scenarioSnapshotResponseCb(DtSimMessage* msg, void* usr)
{
   static_cast<DtTextInterface*>(usr)->setRemoveListCallback(true);
   DtSimInterfaceMessage* iface = dynamic_cast<DtSimInterfaceMessage*>(msg);
   DtIfSnapshotsResponse* rsp = dynamic_cast<DtIfSnapshotsResponse*>(
      iface->interfaceContent());

   DtIfSnapshotsResponse::SnapshotStatus::const_iterator iter = rsp->snapshots().begin();

   DtInfo << "\nSnapshots Available: ";

   if (!rsp->snapshots().size())
   {
      DtInfo << "None" << std::endl;
      return;
   }

   DtInfo << std::endl;

   while (iter != rsp->snapshots().end())
   {
      DtInfo << "   Simulation Time: " << floor(iter->second.simTime) << std::endl;
      ++iter;
   }
}

// Get the snapshots only from the first available back-end.  Should (hopefully) be representative of what all back-ends
// have
void DtTextInterface::listSnapshots()
{
   if (controller()->backendListener()->backends().count())
   {
      controller()->vrfMessageInterface()->removeMessageCallback(
         DtSnapshotsResponseMessageType, scenarioSnapshotResponseCb, this);
      controller()->vrfMessageInterface()->addMessageCallback(
         DtSnapshotsResponseMessageType, scenarioSnapshotResponseCb, this);

      DtIfRequestSnapshots request;
      controller()->vrfMessageInterface()->createAndDeliverMessage(static_cast<DtBackend*>
         (controller()->backendListener()->backendList()->first()->data())->address(), request);
   }
}

// callback to receive C2SIM Scenario load
void DtTextInterface::c2simScenarioLoadedCb(void* usr)
{
	DtInfo("\n***********Scenario Loaded\n> ");
}

// thread function to check periodically for shutdown of simulators
void DtTextInterface::pollForSimShutdown(
	DtVrfRemoteController* controller,
	DtTextInterface* textIf)
{
	// wait until backends ready
	while (!controller->allBackendsReady()) {
		DtSleep(1.0);
		std::cout << "POLLING 0\n";
	}

	// set shutdown state when backends stop
	while (controller->allBackendsReady()) {
		DtSleep(1.0);
		std::cout << "POLLING 1\n";
	}
	std::cout << "POLL QUITTING\n";
	textIf->setTimeToQuit(true);
}

/**
 *  generate a valid new UUID
 */
std::string DtTextInterface::makeUuid() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}
