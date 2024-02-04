/*******************************************************************************

        Copyright (c) 2002 MAK Technologies, Inc.
        All rights reserved.
        *******************************************************************************/
        /*******************************************************************************
        $RCSfile: main.cxx,v $ $Revision: 1.13 $ $State: Exp $
        *******************************************************************************/
        //** MODIFIED 31Aug2022 by GMU C4I & Cyber Lab for c2simVRF

// c2simVRFv2.33 updated to use C++ClientLibv4.8.3.1 31July2022
// compatible with VR-Forces 5.0.2

#include <iostream>
#include <cstring>
#include <string>
#include "textIf.h"
#include "remoteControlInit.h"

// VR-Forces headers
#include <vrfMsgTransport/vrfMessageInterface.h>
#include <vrfMsgTransport/communicationManager.h>
#include <vrlinkNetworkInterface/vrlinkVrfRemoteController.h>

// VR-Link headers
#include <vl/exerciseConn.h>
#include <vlutil/vlProcessControl.h>
#include <vrfmsgs/ifStatus.h>

#include <vrfMsgTransport/simControlEvent.h>

// path to the VRForces executable directory
std::string vrfBin64 = "C:\\MAK\\vrforces5.0.2\\bin64\\";

// current C2SIM and interface versions
std::string c2simVersion = "CWIX2023v1.0.2";
std::string c2simVRFversion = "2.33";

// IP address and port of C2SIM (BML) server
std::string serverAddress = "10.2.10.30";
std::string stompPort = "61613"; // standard server STOMP port
std::string restPort = "8080";   // normal C2SIM REST port

// local configuration defaults
int reportInterval = 0; // in seconds 
std::string clientId = "VRFORCES";
std::string skipInitialize = "0";
std::string useIbmlFlag = "0";
int sendTrackingCode = 0;
int sendObservationsCode = 0;
std::string vrfLocalAddress = "127.0.0.1";
std::string debugFlag = "0";
char* vrfAddr = new char[15];
std::string blueForceName = "";
int sessionId = 1;
std::string applicationNumber = "3201";
char* appNumber = new char[5];
std::string site = "1";
char* siteId = new char[2];
std::string federation = "0";
char* fed = new char[20];
bool repondToTimeMultipleC2SIM = false;
bool bundleReports = false;
std::string acceptedSendingSystem = "0";
char* compare1 = "1";

// establish max bundle size (where budling configured in command line)
int maxBundleSize = 30000;

// establsh aax creport count in bundle
int maxBundleReportCount = 20;

// provides STOMP interface and build VRForces commands
#include "C2SIMinterface.h"

// Thread functionality
#include <thread>

static int timesCalled = 0;

#include <vrlinkNetworkInterface/vrlinkNetworkInterface.h>

class MyDtVrlinkVrfRemoteController : public makVrf::DtVrlinkVrfRemoteController
{
public:
   MyDtVrlinkVrfRemoteController() : makVrf::DtVrlinkVrfRemoteController() {};

   void init(DtExerciseConn* ev,
      DtReflectedEnvironmentProcessList* rel, DtReflectedEntityList* reel, DtReflectedAggregateList* ral,
      const DtString& uuidMarkingForNonVrForces, bool disableRemoteDiscovery
   #if DtHLA
      , makVrfVrlinkExtToolkit::DtReflectedExtendedAttributesObjectList* eaol = nullptr
   #endif
   )
   {
      myOwnsCommunicationManager = true;
      myCommunicationManager = DtCommunicationManager::createCommunicationManager(ev->applicationId());
      myVrlinkNetworkInterface = makVrf::DtVrlinkNetworkInterface::createVrlinkNetworkInterface(ev);
      myVrlinkNetworkInterface->setDisableRemoteDiscovery(disableRemoteDiscovery);
      myCommunicationManager->addManager(myVrlinkNetworkInterface);

      if (!myMsgIf)    
      {   createOrSetMessageInterface(nullptr);   }

      if (!myVrfTDLMsgIf)
      {  createOrSetVrfTDLMessageInterface(nullptr);  }

      makVrf::DtVrlinkVrfRemoteController::init(myCommunicationManager, rel, reel, ral, uuidMarkingForNonVrForces
   #if DtHLA
      , eaol
   #endif
      );
   }
};

int main(int argc, char** argv)
{
    // extract up to 82 optional command line arguments;
    // can be omitted unless providing one of the later ones:
    // 1. server IP address
    // 2. REST port number
    // 3. STOMP port number
    // 4. clientID name 
    // 5. 1 to skip initialize, 0 otherwise (default 0)
    // 6. 1 to use IBML, 0 otherwise (default 0)
    // 7. 0 to send blue tracking, 1 to send red and blue tracking, 2 to send only red tracking, 3 to send none (default 0)
    // 8. VRForces Local IP Address (defaults to loopback)
    // 9. c2simVRF report generation interval in seconds (0 starts VRForces lua report generation script)
    // 19. blue force name for initialization
    // 11. 1 to print debug data, 0 otherwise (default 0)
    // 12. VRForces Session ID (default 1)
    // 13. remote control interface application number (default 3201)
    // 14. VRForces Site ID (default 1)
    // 15. 0 to send blue observations, 1 to send red and blue observations, 2 to send only red observations, 3 to send none (default 0)
    // 16. 1 to respond to C2SIM SetSimulationRelatimeMultiple; 0 not to respond (default 0)
    // 17. 1 to bundle reports in 2-second window for more efficient transmssion
    // 18. name of HLA federation, 0 for DIS (default 0)
    // 19. name of SendingSystem from C2SIMHeader; if missing or 0, orders from any SendingSystem will be accepted

    std::cout <<
        "Starting VR-Forces C2SIM Interface v" << c2simVRFversion <<
        " compatible << c2simVersion << using C2SIMv" <<
        c2simVersion << "\n";
 
    // server address parameter
    if (argc < 2)
        std::cout << "using default server IP address: " << serverAddress << "\n";
    else
    {
        serverAddress = argv[1];         
        std::cout << "using server address: " << serverAddress << "\n";
    }

    // REST port parameter
    if (argc < 3)
        std::cout << "using default REST port: " << restPort << "\n";
    else
    {
        restPort = argv[2];         
        std::cout << "using REST port: " << restPort << "\n";
    }

    // STOMP port parameter
    if (argc < 4)
        std::cout << "using default STOMP port: " << stompPort << "\n";
    else
    {
        stompPort = argv[3];         
        std::cout << "using STOMP port: " << stompPort << "\n";
    }

    // client ID parameter
    if (argc < 5)
        std::cout << "using default client ID: " << clientId << "\n";
    else
    {
        clientId = argv[4];         
        std::cout << "using client ID: " << clientId << "\n";
    }

    // initialization parameter (skip or not)
    if (argc < 6)
        std::cout << "defaulting to require initialization sequence\n";
    else
    {
        skipInitialize = argv[5];         
        if (skipInitialize == "1")             
            std::cout << "skipping initialization sequence\n";
    }

    // use IBML09 in place of C2SIM parameter 
    if (argc < 7)
        std::cout << "defaulting to C2SIM_SMX_LOXv" << c2simVersion << " schema\n";
    else
    {
        useIbmlFlag = argv[6];         
        if (useIbmlFlag == "1")std::cout << "expecting IBML09 input\n";         
        else std::cout << "expecting C2SIMv" << c2simVersion << " input\n";
    }

    // tracking reports parameter
    if (argc < 8)
        std::cout << "defaulting to sending blue and not red tracking reports\n";
    else
    {
        sendTrackingCode = std::stoi(argv[7]);         
        if (sendTrackingCode == 0 || sendTrackingCode == 1)
            std::cout << "sending blue tracking reports\n";         
        else std::cout << "not sending blue tracking reports\n";         
        if (sendTrackingCode == 1 || sendTrackingCode == 2)
            std::cout << "sending red tracking reports\n";         
        else std::cout << "not sending red tracking reports\n";
    }

    // address for VRForces parameter
    if (argc < 9)
        std::cout << "VRForces address defaulting to " << vrfLocalAddress << "\n";
    else
    {
        vrfLocalAddress = argv[8];         
        std::cout << "VRForces address set to:" << vrfLocalAddress << "\n";
    }

    std::strcpy(vrfAddr, vrfLocalAddress.c_str());

    // report interval (if reports generated in c2simVRF rather than lua scripts
    if (argc < 10)
        std::cout << "C2SIM report interval defaulting to " << reportInterval << "sec\n";
    else
    {
        reportInterval = std::stoi(argv[9]); 
        if (reportInterval == 0)
            std::cout << "using VRF lua script report generator\n";
        else
            std::cout << "internal position report send time interval:" << 
            reportInterval << " seconds\n";
    }

    // blue force name parameter
    if (argc >= 11) {
        // zero means command line is not provide blue force name
        // (this is needed if not providing and debugFlag is required)
        blueForceName = argv[10];
        if (blueForceName != "0")
        {
            std::cout << "blue force name: " << blueForceName << "\n";
        }

        else blueForceName = "";
        if (blueForceName == "")
            std::cout << "blue force name defaulting to first XML ForceSide entry\n";
    }

    // print debug data parameter
    if (argc >= 12)
    { // 1 to dispay debug info         
        debugFlag = argv[11];         
        if (debugFlag == "1")std::cout << "displaying debug output\n";     
    }

    // VRForces Session ID parameter
    if (argc >= 13) { // integer sessionId}
        sessionId = atoi(argv[12]);
        std::cout << "setting Session ID to: " << sessionId << "\n";
    }
    else std::cout << "Session ID defaulting to: " << sessionId << "\n";

    // VRForces remote interface application number parameter
    if (argc >= 14) { // integer in char[2] Application number}
        applicationNumber = argv[13];
        std::cout << "setting remote interface application number to:" << applicationNumber << "\n";
    }
    else
    {
        std::cout << "remote interface application number defaulting to " << applicationNumber << "\n";
    }

    std::strcpy(appNumber, applicationNumber.c_str());

    // VRForces Site ID parameter
    if (argc >= 15)
        // integer in char[2] Site ID
    {
        site = argv[14];
        std::cout << "setting remote interface site number to:" << site << "\n";
    }

    else std::cout << "VRForces Site ID defaulting to " << site << "\n";
    std::strcpy(siteId, site.c_str());

    // observation reports parameter
    if (argc >= 16)
    {
        sendObservationsCode = std::stoi(argv[15]);         
        if (sendObservationsCode == 0 || sendObservationsCode == 1)std::cout << "sending blue observation reports\n";         
        else std::cout << "not sending blue obervation reports\n";         
        if (sendObservationsCode == 1 || sendObservationsCode == 2)std::cout << "sending red observation reports\n";         
        else std::cout << "not sending red observation reports\n";
    }

    else std::cout << "defaulting to sending blue and not red tracking reports\n";

    // C2SIM SetSimulationRealtimeMultiple - report or not? (default not)
    if (argc >= 17)
        if (strncmp(argv[16], compare1, 1) == 0)repondToTimeMultipleC2SIM = true;
    std::cout << "RepondToTimeMultipleC2SIM:" << repondToTimeMultipleC2SIM << "\n";

    // whether or not to bundle reports for more efficient transmision 
    if (argc >= 18)
        if(strncmp(argv[17],compare1,1)==0)bundleReports = true;
    if (bundleReports)std::cout << "bundling positions reports for more efficient transmission\n";
    else std::cout << "not bundling reports\n";

    // HLA Federation default MAK-RPR-2.0 mX 19 chars
    if (argc >= 19)
    {
        federation = argv[18];
        std::strcpy(fed, federation.c_str());
        std::cout << "HLA federation:" << federation << "\n";
    }
#if DtHLA
    if (federation == "0") {
        std::cout << "required parameters HLA federation not set - cannot execute\n";
        return 0;
    }
#endif
    
    // SendingSystem from C2SIM header
    if (argc >= 20) {
        acceptedSendingSystem = argv[19];
        if(acceptedSendingSystem != "0")
            std::cout << "accepting C2SIM Orders from:" << acceptedSendingSystem << "\n";
        else std::cout << "accepting C2SIM Orders from any sending system\n";
    }
    else std::cout << "accepting C2SIM Orders from any sending system\n";

    // arguments for appInitializer command line
#if DtHLA 
    char* fomFile6 = new char[50];
    std::strcpy(fomFile6, (vrfBin64 + "MAK-VRFExt-6_evolved.xml").c_str());
    char* fomFile1 = new char[50];
    std::strcpy(fomFile1, (vrfBin64 + "MAK-VRFExt-1_evolved.xml").c_str());
    char* rprFomFile = new char[50];
    std::strcpy(rprFomFile, (vrfBin64 + "RPR_FOM_v2.0_1516-2010.xml").c_str());

    char* lgrFomFile = new char[50];
    std::strcpy(lgrFomFile, (vrfBin64 + "MAK-LgrControl-2_evolved.xml").c_str());
    char* aggFomFile = new char[50];
    std::strcpy(aggFomFile, (vrfBin64 + "MAK-VRFAggregate-3_evolved.xml").c_str());
    char* dynFomFile = new char[50];
    std::strcpy(dynFomFile, (vrfBin64 + "MAK-DynamicTerrain-2_evolved.xml").c_str());
    char* diFomFile = new char[50];
    std::strcpy(diFomFile, (vrfBin64 + "MAK-DIGuy-7_evolved.xml").c_str());
   
    char* vrfArgv[] =
    {   "bin64\\c2simVRFHLA1516e", 
        "--rprFomVersion", "2.0", 
        //"--rprFomRevision", "3",
        "--fomModules", fomFile6,       
        "--fomModules", diFomFile,
        //"--fomModules", fomFile1,        
        //"--fomModules", lgrFomFile,            
        //"--fomModules", aggFomFile,            
        //"--fomModules", dynFomFile,         
        "-a", appNumber,          
        "-s", siteId,          
        "--execName", fed,         
        "--fedFileName", rprFomFile,   
        "-n", "1"
    };
#else
    char* vrfArgv[] =
    { "bin64\\c2simVRF", "--disVersion", "7", "--deviceAddress", vrfAddr,         
        "--disPort", "3000", "-a", appNumber, "-s", siteId, "-x", "1", "-n", "2" };
#endif
    int argCount = sizeof(vrfArgv) / sizeof(vrfArgv[0]);
    std::cout << "VR-Forces arguments:";
    for (int i = 1; i < argCount; i += 2)std::cout << vrfArgv[i] << " " << vrfArgv[i + 1] << "|";
    std::cout << "\n";
    
    // Create initializer object used to provide initialization data to the
    // exercise connection, configured through command line arguments.
    DtRemoteControlInitializer appInitializer(argCount, vrfArgv);
    appInitializer.parseCmdLine();
    
    // Create the controller
    MyDtVrlinkVrfRemoteController* controller = new MyDtVrlinkVrfRemoteController();
 
    //Create an exercise connection
    DtExerciseConn* exConn = new DtExerciseConn(appInitializer);
    controller->init(exConn, nullptr, nullptr, nullptr, "entity-identifier", true);

    // set the session ID & host IP address
    controller->eventManager()->setProcessEventsImmediately(true);
    controller->vrfMessageInterface()->setSessionId(sessionId);
    std::cout << "SessionId set to:" << controller->vrfMessageInterface()->sessionId() << "\n";

    // Create our text interface so we can enter remote commands
    DtTextInterface* textIf =  
        new DtTextInterface(controller, serverAddress, restPort, clientId,
            useIbmlFlag == "1", sendTrackingCode, reportInterval == 0,
            debugFlag == "1", c2simVersion, sendObservationsCode, bundleReports, 
            maxBundleSize, maxBundleReportCount, exConn);

    // add local host address to the controller
    controller->setHostInetAddr(vrfAddr);
    std::cout << "HostInetAddr set to:" << controller->hostInetAddr().toDtString() << "\n";
    DtList currentBackends = textIf->controller()->backends();
    std::cout << "BACKEND COUNT:" << currentBackends.count() << "\n";
    // Create a C2SIM interface to read orders and translate them for VRF
    C2SIMinterface* c2simInterface =
        new C2SIMinterface(textIf, serverAddress, stompPort, restPort, clientId,
            useIbmlFlag == "1", sendTrackingCode, debugFlag == "1",
            reportInterval, c2simVersion, repondToTimeMultipleC2SIM,
            acceptedSendingSystem, c2simVRFversion,
#if DtHLA
            true
#else
            false
#endif        
);
    // Start a thread to read from STOMP and generate VRForces commands
    std::thread t1(&C2SIMinterface::readStomp, c2simInterface,
        skipInitialize == "1", clientId, reportInterval);

    // start a thread to generate reports (must start after objects are created)
    std::thread t3(&C2SIMinterface::reportGenerator);

    // Processing: read stdin and call drainInput.
    //    Calling drainInput() ensures that the controller receives
    //    necessary feedback from VR-Forces backend applications.

    while (!textIf->timeToQuit())
    {
        textIf->readCommand();         
        exConn->clock()->setSimTime(exConn->clock()->elapsedRealTime());         
        exConn->drainInput();         
        controller->tick();        
        DtSleep(0.1);
    }

    // shutdown
    t3.join();
    t1.join();
    delete textIf;
    delete controller;
    delete exConn;

    return 0;
}
// end main()