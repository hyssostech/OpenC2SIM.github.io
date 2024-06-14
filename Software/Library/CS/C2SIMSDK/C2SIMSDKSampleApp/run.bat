REM Replace the IPs to match the C2SIM Server in use
pushd %~dp0\bin\debug\net6.0\
C2SIMSDKSampleApp.exe C2SIM:RestUrl="http://127.0.0.1:8080/C2SIMServer" C2SIM:StompUrl="http://127.0.0.1:61613/topic/C2SIM" C2SIM:DisplayReports=OFF
popd