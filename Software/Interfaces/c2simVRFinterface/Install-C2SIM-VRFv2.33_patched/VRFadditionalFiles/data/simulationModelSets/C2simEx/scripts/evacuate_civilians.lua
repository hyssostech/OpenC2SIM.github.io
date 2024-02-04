-- This script template has each of the script entry point functions.
-- They are described in detail in VR-Forces Configuration Guide.

-- Some basic VRF Utilities defined in a common module.
require "vrfutil"

-- Global Variables
--
-- Global variables get saved when a scenario gets checkpointed in one of the folowing way:
-- 1) If the checkpoint mode is AllGlobals all global variables defined will be saved as part of the save stat
-- 2) In setting CheckpointStateOnly, this means that the script will *only* save variables that are part of the checkpointState table.  If you remove this value, it will then
--    default to the behavior of sabing all globals
--
-- If you wish to change the mode, call setCheckpointMode(AllGlobals) to save all globals or setCheckpointMode(CheckpointStateOnly)
-- to save only those variables in the checkpointState table
-- They get re-initialized when a checkpointed scenario is loaded.
vrf:setCheckpointMode(CheckpointStateOnly)

-- For convenience
s = checkpointState

-- Task Parameters Available in Script
--  taskParameters.pickupPoint Type: SimObject - Location to fly to and pick up passengers.
--  taskParameters.numPassengers Type: Integer - Number of passengers to embark
--  taskParameters.maxRangeToPoint Type: Real Unit: meters - The maximum distance from the pickup point to potential passengers.
--  taskParameters.dropoffPoint Type: SimObject - Place where passengers will be disembarked
--  taskParameters.returnPoint Type: SimObject - Point to which entity flies after dropping off passengers.
--  taskParameters.altitude Type: Real Unit: meters - Altitude for flying to points.

s.state = nil
-- Task states:
-- fly-to-pickup - Flying to the pickup point.
-- waiting-for-embark - Passengers have been identified and given tasks to embark
-- fly-to-dropoff - Passengers have all embarked and entity is flying to drop off
-- waiting-for-disembark - Passengers have been given disembark
-- rtb - entity is flying back to return point.

s.passengerTasks = {} -- table of subtask ID for each passenger

local EMBARK_TIMEOUT = 300 -- How long to wait for embarkation
s.embarkStartTime = 0

s.subtaskId = -1

-- Called when the task first starts. Never called again.
function init()
   if taskParameters.pickupPoint:isValid() and
      taskParameters.dropoffPoint:isValid() and
      taskParameters.returnPoint:isValid() then
      
      s.subtaskId = vrf:startSubtask("move-to",
         {control_point = taskParameters.pickupPoint})
      s.state = "fly-to-pickup"
   else
      printWarn("One of the control points is invalid; ending task")
   end
end

-- Called each tick while this task is active.
function tick()
   printDebug(string.format("%.3f Evac: state %s",
      vrf:getSimulationTime(), s.state))
      
   if s.state == "fly-to-pickup" then
      if not vrf:isSubtaskRunning(s.subtaskId) then
         if this:getLocation3D():distanceToLocation3D(
            taskParameters.pickupPoint:getLocation3D()) > 10.0 then
            
            printWarn("Could not reach pickup point.")
            vrf:endTask(false)
            return
         else
            local foundPassengers = findPassengers()
            printVerbose(string.format("  Found %d neutral lifeforms; starting embark tasks.",
               #foundPassengers))
            for _i, p in ipairs(foundPassengers) do
--~                s.passengerTasks[p] = vrf:sendTask(p, "embark",
--~                   {parent = this}, true)
               -- There's a bug in the human movement actuator related to
               -- embarking, so just teleport them into the aircraft:
               vrf:sendSetData(p, "set-load-entity",
                  {parent = this})
            end
            s.state = "waiting-for-embark"
            s.embarkStartTime = vrf:getSimulationTime()
         end
      end
   
   elseif s.state == "waiting-for-embark" then
   -- This code is used if the passengers are given embark tasks above, instead of sets
--~       local done = true
--~       if vrf:getSimulationTime() > s.embarkStartTime + EMBARK_TIMEOUT then
--~          printWarn(string.format("  Embarkation timed out. Leaving with current passengers."))
--~          for p, id in pairs(s.passengerTasks) do
--~             vrf:stopSubtask(id)
--~          end
--~       else
--~          
--~          for p, id in pairs(s.passengerTasks) do
--~             if vrf:isSubtaskRunning(id) then
--~                done = false
--~             else
--~                s.passengerTasks[p] = nil
--~                printVerbose("  Picked up passenger ", p:getName())
--~             end
--~          end
--~       end
--~       if done then
         printVerbose("Got all passengers. Flying to dropoff point.")
         s.subtaskId = vrf:startSubtask("move-to",
            {control_point = taskParameters.dropoffPoint})
         s.state = "fly-to-dropoff"
--~       end
   
   elseif s.state == "fly-to-dropoff" then
      if not vrf:isSubtaskRunning(s.subtaskId) then
         printVerbose("At dropoff point. Disembarking passengers")
         s.subtaskId = vrf:startSubtask("disembark-all-entity", {})
         s.state = "waiting-for-disembark"
      end
      
   elseif s.state == "waiting-for-disembark" then
      if not vrf:isSubtaskRunning(s.subtaskId) then
         printVerbose("Passengers disembarked. Flying to return point.")
         s.subtaskId = vrf:startSubtask("move-to",
            {control_point = taskParameters.returnPoint})
         s.state = "rtb"
      end
      
   elseif s.state == "rtb" then
      if not vrf:isSubtaskRunning(s.subtaskId) then
         vrf:endTask(true)
      end

   end
end

--------------------------------------------------------------
-- Finds nearby civilians to embark. Returns a list of sim objects.
function findPassengers()
   local nearObjects = vrf:getSimObjectsNearWithFilter(this:getLocation3D(),
      taskParameters.maxRangeToPoint,
      {types = {EntityType.Lifeform()}})
   local nearCivilians = {}
   local count = 0
   printVerbose(string.format("  Found %d nearby lifeforms; looking for neutrals",
      #nearObjects))
   for i, o in ipairs(nearObjects) do
      if o:getForceType() == "Neutral" then
         table.insert(nearCivilians, o)
         count = count + 1
         if count >= taskParameters.numPassengers then
            return nearCivilians
         end
      end
   end
   return nearCivilians
end
--------------------------------------------------------------
-- Called when this task is being suspended, likely by a reaction activating.
function suspend()
   -- By default, halt all subtasks and other entity tasks started by this task when suspending.
   vrf:stopAllSubtasks()
   vrf:stopAllTasks()
end

-- Called when this task is being resumed after being suspended.
function resume()
   -- By default, simply call init() to start the task over.
   init()
end

-- Called immediately before a scenario checkpoint is saved when
-- this task is active.
-- It is typically not necessary to add code to this function.
function saveState()
end

-- Called immediately after a scenario checkpoint is loaded in which
-- this task is active.
-- It is typically not necessary to add code to this function.
function loadState()
end


-- Called when this task is ending, for any reason.
-- It is typically not necessary to add code to this function.
function shutdown()
end

-- Called whenever the entity receives a text report message while
-- this task is active.
--   message is the message text string.
--   sender is the SimObject which sent the message.
function receiveTextMessage(message, sender)
end
