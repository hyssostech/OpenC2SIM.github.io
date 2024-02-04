/*----------------------------------------------------------------*
|     Copyright 2023 Networking and Simulation Laboratory         |
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

VERSION 2.33 uses C++ClientLibv4.8.3.1 with libboost-*-vc140-*

This directory contains all the code developed by GMU as open source
to provide a prototype C2SIM interface for VT-MAK VR-Forces.
Open source supporting code also is provided.

This version works with VRForces5.0.2 and VRLink5.8 and is compiled
with Microsoft Visual Studio 16.4.6.

The project file provided assumes that VRForces5.0.2 (including 
toolkit) is installed in C:\MAK\vrforces5.0.2 and VRLink5.8 is 
installed C:\MAK\vrlink5.8. These are MAK's default install 
directories. They can be adjusted by changing VrfDir and VrlDir
in the project file c2simVRFDIS.vcxproj and c2simVRFHLA1516e.vcxproj.

You must add to your Windows Path Environment Variable:
C:/MAK/vrforces5.0.2/bin64 and C:/MAK/vrlink5.8/bin64

The top-level directory c2simVRFinterfacev2.33 can be installed in C:\
or elsewhere. 

v2.33 includes binaries for C++C2SIMClientLibv4.8.3.1, which is 
compatible with C2SIM Reference Implemenation Server v4.8.3 and 
is *not* necessarily compatible with earlier versions of the server.

v2.33 is compatible with C2SIM CWIX2023v1.0.2. It accepts multiple
Routes in C2SIMInitializationBody and ObjectinitializationBody
messages. To provide routes after initialization, use an
Entity containing a Route in the C2SIM Order.

see main.cxx for list of c2simVRF command-line parameters
(not to be confused with VRForces command-line parameters that
are listed in main.cxx and previously were in the VRForces config 
panel)

c2simVRFv2.33 accepts a ninth command line parameter that
gives a value in seconds for the interval between Position Reports.
If this parameter has value greater than zero the scripted reports
procedures are ignored and Position Reports (only - no Observation
Reports) are generated at this interval. If this parameter is zero
(the default value) the scripted approach to reports is used. 

See folder VRFadditionalFiles for files to be added to VRForces to
support the scenario used by NATO MSG-201 in CWIX 2023.
Beginning with c2simVRFv2.17, it is possible to use the C2SIM 
interface with either DIS or HLA. To run with HLA start the 
interface using runc2simVRFHLA1516e.bat.

For HLA you must start VRForces using the vrfLauncher as shown in
vrfLauncher.pdf in this directory. SessionID and Site Number (both 
Back-end and Front-end) must match the numbers in the .bat file used
to start c2simVRF interface, and also must be different from those
used with any other instance of VRForces in the HLA federation. This
includes the three FOM Modules indicated in the launcher.

The VRForces IP address must be 127.0.0.1 in all HLA .bat files
(for DIS the IP address must be that of the broadcast network being 
used, for example a VPN address).

Also be aware the these Windows Path environment variable must be set
at start of the Path list when using Pitch RTI:
C:\Program Files\prti1516e\lib\vc141_64
C:\Program Files\prti1516e\lib
C:\Program Files\prti1516e\jre\bin\server

Also for Pitch RTI these must be added to the CLASSPATH:
C:\Program Files\prti1516e\lib\prti1516e.jar
C:\Program Files\prti1516e\lib\prti1516.jar
C:\Program Files\prti1516e\lib\prti.jar

There are also example batch files for starting the c2simVRFinterface 
for DIS (runc2simVRFDIS.bat) and HLA (runc2simVRFHLA1516e.bat). You might 
want to make desktop shortcuts for these batch files.

c2simVRFv2.33 uses these command line parameters;
any can be omitted to use default unless needed to enable providing 
one of the later ones:
1. server IP address
2. REST port number
3. STOMP port number
4. clientID name 
5. 1 to skip initialize, 0 otherwise (default 0)
6. 1 to use IBML instead of C2SIM, 0 otherwise (default 0)
7. 0 to send blue tracking (default)
   1 to send red and blue tracking, 
   2 to send only red tracking
   3 to send no tracking
8. VRForces Local IP Address (defaults to loopback)
9. report generation interval in seconds
10. blue force name for initialization
11. 1 to print debug data, 0 otherwise (default 0)
12. VRForces Session ID (default 1)
13. remote control interface application number (default 3201)
14. VRForces Site ID (default 1)
15. 0 to send blue observations (default)
    1 to send red and blue observations, 
    2 to send only red observations 
    3 to send no observations
16. 1 to respond to C2SIM SetSimulationRelatimeMultiple in v1.0.2
    0 not to respond (default 0)
17. 1 to bundle reports for more efficient transmission
    0 not to bundle (default 0)
18. name of HLA Federation if using HLA (default MAK-RPR-2.0) 
19. name of SendingSystem per C2SIMHeader; if missing or 0, 
    orders from any SendingSystem will be accepted (NOTE: the
    C2SIMGUI inserts the configured value of SubmitterID in
    FromSendingSystem when forwarding XML documents such as
    Order and Report.)

Treatment of reports described above assumes VRForces hostile
forces will be under command of an exercise OPFOR who are
giving orders to red units and see observations about blue units
as their own opposition. Thus red observations of the blue force
are reported as "FR" because that is the blue force role in the
exercise.

Version 2.33 includes support for batching multiple <ReportContent>
per the C2SIM standard for <ReportBody>.

NOTE: The DIS version has not been load-tested; HLA version is believed 
to work properly with VRForces 5.0.2 running task-follows-task, 
no matter which order the Tasks are submitted in C2SIM Orders.