/*******************************************************************************
** Copyright (c) 2014 MAK Technologies, Inc.
** All rights reserved.
*******************************************************************************/

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma once

#include <vlutil/vlUtil.h>
#include <vrlinkNetworkInterface/vrlinkVrfRemoteController.h>

#include "C2SIMxmlHandler.h"

#include <vl/exerciseConn.h>

#include <vrlinkNetworkInterface/UUIDNetworkManager.h>

//d#include "MyDtVrlinkVrfRemoteController.h"


//This class provides a text interface for controlling VR-Forces over the network.
//This interface currently supports the following actions:
//
//1. load <filename> 
//     load a scenario file

//2. save <filename>
//     save the current scenario

//3. new <db filename>
//     create a new scenario

//4. close
//     close the current scenario

//5. rewind
//     rewind the current scenario to its start state

//6. run
//     sends a message interface request to run the current scenario

//6a. simrun
//     sends a sim mgmt start PDU to run the current scenario

//7. simpause
//     sends a sim mgmt stop PDU to pause the current scenario

//7a. pause
//     sends a sim mgmt pause request to pause the current scenario

//8. list
//     list all the backends that are currently running

//9. create [ waypoint | route | tank | aggregate]
//     create a VR-Forces waypoint, route, tank, or tank aggregate

//10. delete [ <object_name> | <object_id> ]
//     delete a VR-Forces object by name or by object ID

//11. set [ plan | label | location | fuel | target | restore ]
//     set a VR-Forces object attribute

//12. task [ patrolRoute | moveToPoint | follow | wait ]
//     task a VR-Forces entity

//13. aggregate "name1" "name2" "name3"
//     aggregate 3 VR-Forces entities together

//14. timemultiplier
//     sets the time multiplier on the exercise clock

//15. quit
//     quit the application

// global callback from Doug Reece for C2SIM reports
static void reportCallback(const DtVrfObjectMessage* msg, void* usr);

class DtTextInterface
{
public:
  
  //constructor
  DtTextInterface(
	  makVrf::DtVrlinkVrfRemoteController* controller,// C2SIM
	  std::string serverAddress,
      std::string restport,
	  std::string clientID,
	  bool useIbmlRef,            //C2SIM
	  int sendTrackingRef,        //C2SIM
	  bool sendReportsRef,        //C2SIM
	  bool debugModeRef,          //C2SIM
      std::string c2simVersionRef,//C2SIM
      int sendObservationsRef,    //C2SIM
      bool bundleReportsRef,      //C2SIM
      int maxBundleSizeref,       //C2SIM
      int maxBundleReportCountRef,//C2SIM
      DtExerciseConn* exConnRef); //C2SIM

  //destructor
  virtual ~DtTextInterface();

  void readCommand();// C2SIM

  void setC2SIMxmlHandler(C2SIMxmlHandler* xmlhandlerRef);// C2SIM

  //Process entered text
  void readStdin();

  //Set/Get time to quit
  virtual void setTimeToQuit(bool yesNo);
  virtual bool timeToQuit() const;

  //Get remote vrf controller pointer
  virtual makVrf::DtVrlinkVrfRemoteController* controller() const;

public:

  //Functions for parsing command strings and calling
  //the corresponding methods on  DtVrfRemoteController.

  virtual void createWaypoint(char* str);
  virtual void createRoute(char* str);
  virtual void createTank(char* str);
  virtual void createAggregate(char* str);
  virtual void setPlan(char* str);
  virtual void setLabel(char* str);
  virtual void setLocation(char* str);
  virtual void setFuel(char* str);
  virtual void setTarget(char* str);
  virtual void setRestore(char* str);
  virtual void patrolRoute(char* str);
  virtual void moveToPoint(char* str);
  virtual void follow(char* str);
  virtual void wait(char* str);
  virtual void bigRoute(int iPoints);
  virtual void listSnapshots();

public:

   void DtTextInterface::spotReportCallback(//const // C2SIM
		DtVrfObjectMessage* msg, void * usr);
   void DtTextInterface::sendStatusReport(          // C2SIM
	   std::string senderUuid, std::string statusCode, std::string taskUuid);

   //-------------------------------------------------------
   //Text callback functions
   //-------------------------------------------------------

   //Called in response to "list"
   static void listCmd(char* s, DtTextInterface* a);

   //Called in response to "load"
   static void loadCmd(char* s, DtTextInterface* a);
   //Called by DtVrfRemoteController::loadScenario when back-ends are missing
   static bool missingBackends(DtBackendMap* map, const std::set<DtSimulationAddress>& currentBackends, void* usr);

   //Called in response to "new"
   static void newCmd(char* s, DtTextInterface* a);

   //Called in response to "save"
   static void saveCmd(char* s, DtTextInterface* a);

   //Called in response to "create"
   static void createCmd(char* s, DtTextInterface* a);

   //Called in response to "set"
   static void setCmd(char* s, DtTextInterface* a);

   //Called in response to "setspeed"
   static void setSpeedCmd(char* s, DtTextInterface* a);

   //Called in response to "setsensor"
   static void setSensorCmd(char* s, DtTextInterface* a);

   //Called in response to "task"
   static void taskCmd(char* s, DtTextInterface* a);

   //Called in response to "aggregate"
   static void aggregateCmd(char* s, DtTextInterface* a);

   //Called in response to "delete"
   static void deleteCmd(char* s, DtTextInterface* a);

   //Called in response to "close"
   static void closeCmd(char* s, DtTextInterface* a);

   //Called in response to "monitor"
   static void monitorResourcesCmd(char* s, DtTextInterface* a);

   //Called in response to "unmonitor"
   static void removeMonitorResourcesCmd(char* s, DtTextInterface* a);

   //Called in response to "rewind"
   static void rewindCmd(char* s, DtTextInterface* a);

   //Called in response to "run"
   static void runCmd(char* s, DtTextInterface* a);

   //Called in response to "pause"
   static void pauseCmd(char* s, DtTextInterface* a);

   //Called in response to "simrun"
   static void simrunCmd(char* s, DtTextInterface* a);

   //Called in response to "simpause"
   static void simpauseCmd(char* s, DtTextInterface* a);

   //Called in response to "timemultipler" 
   static void timemultiplierCmd(char* s, DtTextInterface* a);

   //Called in response to "quit" or "q"
   static void quitCmd(char* s, DtTextInterface* a);

   //Called in response to "help" or "?" or "h"
   static void helpCmd(char* s, DtTextInterface* a);

   //Called in response to "help" or "?" or "h"
   static void bigRouteCmd(char* s, DtTextInterface* a);

   //Called in response to "listsnapshots"
   static void listSnapshotsCmd(char* s, DtTextInterface* a);

   //Called in response to "takesnapshot"
   static void takeSnapshotCmd(char* s, DtTextInterface* a);

   //Called in response to "turnonsnapshots"
   static void turnOnSnapshotsCmd(char* s, DtTextInterface* a);

   //Called in response to "turnoffsnapshots"
   static void turnOffSnapshotsCmd(char* s, DtTextInterface* a);

   //Called in response to "rollback"
   static void rollbackToSnapshotCmd(char* s, DtTextInterface* a);

   //-------------------------------------------------------
   //DtVrfRemoteController callback functions
   //-------------------------------------------------------

   //Backend arrival/departure callbacks
   static void backendAddedCb(const DtSimulationAddress& addr, void* usr);
   static void backendRemovedCb(const DtSimulationAddress& addr, void* usr);

   //Backend loaded/saved callbacks
   static void backendLoadedCb(const DtSimulationAddress& addr, void* usr);
   static void backendSavedCb(const DtSimulationAddress& addr, 
      DtSaveResult result, void* usr);

   //Scenario loaded/saved callbacks
   static void ScenarioLoadedCb(void* usr);
   static void ScenarioSavedCb(void* usr);
   static void c2simScenarioLoadedCb(void* usr);

   //VR-Forces object created callback
   static void vrfObjectCreatedCb(
      const DtString& name, const DtEntityIdentifier& id, const DtUUID& uuid, void* usr);

   //Plan callbacks 
   static void printStatement(const DtSimStatement* stmt, int indent = 1);
   static void printBlock(const DtSimBlock* stmt, int indent = 1);
   static void planCb(const DtUUID& name, 
      const std::vector<DtSimStatement*>& statements, bool append, void* usr);
   static void planStmtCb(const DtUUID& id, DtPlanStatus status, void* usr);
   static void planCompleteCb(const DtUUID& id, void* usr);

   //Callback used to process monitor resource responses
   static void resourcesProcessedCb(DtSimMessage* msg, void* usr);

   //Callback for list of current snapshots
   static void scenarioSnapshotResponseCb(DtSimMessage* msg, void* usr);

   //Callback used to receive object console messages from the remote control
   //API.
   static void objectConsoleMessageCb(const DtEntityIdentifier& id,
      int notifyLevel, const DtString& message, void* usr);

   void setRemoveListCallback(bool b) { myRemoveListCallback = b; };

protected:

   //Present commmand line interface.
   void prompt();

public:

   //Parse user input, call corresponding function
   void processCmd(char *buff);

   // set flag to enable report messaging
   void setStarted(std::string value);// C2SIM

   // send a REST message// C2SIM
   static void sendRest(
	   bool formatIsC2sim,
	   std::string restServerAddress,
	   std::string restPort,
	   std::string clientID,
	   std::string unitNameString,
	   std::string report);

   // make a unique ID
   static std::string makeReportID();

   // thread function to poll simulation status
   static void pollForSimShutdown(
	   DtVrfRemoteController* controller,
	   DtTextInterface* textIf);

   // make a valid UUID
   static std::string DtTextInterface::makeUuid();

   // provide nuber of reports in a bundle
   int getMaxReportsPerBundle();

   // collect Reports into batches to send
   static void sendC2simReport(
       std::string uuid,
       std::string reportContent);

   // call sendC2simReport() maxBundlingTimeMs 
   // after reportBundleCreationTime
   static void waitForBundle(
       long reportBundleCreationTime, 
       std::string uuid);

protected:

   bool myTimeToQuit;
   makVrf::DtVrlinkVrfRemoteController* myController;
   DtSelectParam sparam;
   static unsigned int theNextAggregateId;
   static unsigned int theNextSubordinateId;
   bool myRemoveListCallback;

private:
   int kbInterest;
};
