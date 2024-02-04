REM Batch file to start C2SIM VRForces interface

REM Pitch RTI explicit path - ignores other RTIs that may be present in the PATH
set PATH=C:\Program Files\prti1516e\lib\vc141_64;C:\Program Files\prti1516e\lib;C:\Program Files\prti1516e\jre\bin\server;%PATH%

REM Assume that MAK is installed in the default location
REM Start VRF with the "HLA 1516 Evolved RPR 2.0 with MAK extensions configuration
REM Make sure to match the configuration parameters (Session Id, Backend Site Number, Federation Name )
cd C:\MAK\vrforces5.0.2\bin64

REM Start the Interface from the bin64 within the folder containing this script
REM if multiple VRForces are participating as separate federates, each should have a different siteId and sessionId
%~dp0\bin64\c2simVRFHLA1516e.exe 127.0.0.1 8080 61613 NPS 0 0 1 127.0.0.1  0 0 0 1 3201 1 0 0 1 CWIX-2023

REM Parameters optional; defaults given below
REM 1 C2SIM Server IP:10.2.10.30
REM 2 REST port:8080
REM 3 STOMP port:61613
REM 4 client ID:VRFORCES
REM 5 skipinit:0 
REM 6 IBML: 0
REM 7 red/blue tracking:0
REM 8 VRF address: 127.0.0.1 - always this for HLA
REM 9 internal report interval:0
REM 10 blue force name: ""
REM 11 debug output:0
REM 12 VRF session ID:1
REM 13 VRF app number:3201
REM 14 VRF site:1
REM 15 blue/red obs:0
REM 16 respond to C2SIM time mult:0
REM 17 bundle reports: 0
REM 18 HLA Federation:CWIX=2022 (0 if none)
REM 19 name for this order sender (0 if none)

pause