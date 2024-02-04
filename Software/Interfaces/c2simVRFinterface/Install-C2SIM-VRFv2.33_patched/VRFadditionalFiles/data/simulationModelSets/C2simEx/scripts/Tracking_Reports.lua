-- A background process to send tracking reports.
-- Sent in a text report, with format
-- TRACKING "entity name" <lat in deg.> <lon in deg>
-- There must be 1 and only 1 space between TRACKING and the
-- quoted entity name.

-- Some basic VRF Utilities defined in a common module.
require "vrfutil"

local sendInterval = 60

checkpointState.nextSendTime = 0

-- Called when the task first starts. Never called again.
function init()
   checkpointState.nextSendTime = vrf:getSimulationTime() + math.random(15)
   -- Set the tick period for this script.
   vrf:setTickPeriod(2)
end

-- Called each tick while this task is active.
function tick()
   local curTime = vrf:getSimulationTime()
--debugx   if this:getForceType() == "Friendly" and
if curTime > checkpointState.nextSendTime then
      
      checkpointState.nextSendTime = curTime + sendInterval
      
      reportString = string.format("POSITION \"%s\" %f %f",
          this:getName(), math.deg(this:getLocation3D():getLat()), 
            math.deg(this:getLocation3D():getLon()))
   --   printWarn(reportString)
      vrf:sendReport("text-report", {text = reportString})
   end
end
