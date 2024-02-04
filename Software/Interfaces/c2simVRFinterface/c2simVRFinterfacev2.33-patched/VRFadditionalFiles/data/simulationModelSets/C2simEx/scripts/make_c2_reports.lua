-- Original script by Doug Reece of MAK
-- modified 18Jun21 by JMP to produce observation reports from both Friendly & Opposing
-- to be used with c2simVRFinterface v.16 and later 

-- A background process to send tracking reports.
-- Sent in a text report, with format
-- TRACKING "entity name" <lat in deg.> <lon in deg>
-- There must be 1 and only 1 space between TRACKING and the
-- quoted entity name.

local TICK = 2
local sendInterval = 200

-- Some basic VRF Utilities defined in a common module.
require "vrfutil"

vrf:setCheckpointMode(CheckpointStateOnly)
local s = checkpointState

s.nextSendTime = 0

-- List of subordinates. Indexed by subordinate name.
s.subs = {}
-- List of subs that have sent an observation.  Indexed by subordinate name.
s.subsNotReporting = {}

-- A table of observations, as reported from subordinates.
--  The table will be indexed by entity name--cheating, allows perfect fusion--
-- and each entry will contain:
--    entity or aggregate class, such as "tank", "apc", or "aircraft"
--    last observerd lat and lon
--    last time of observation
--    operational status
s.observations = {}
-- 

-- Called when the task first starts. Never called again.
function init() 
   -- Set the tick period for this script.
   
   vrf:setTickPeriod(TICK)
   local tmpList = this:getCapableSubordinates()
   for i, sub in ipairs(tmpList) do
      s.subs[sub] = true
      s.subsNotReporting[sub] = true
   end
   
   s.nextSendTime = vrf:getSimulationTime() + math.random(15)
end

-- Called each tick while this task is active.
function tick()
   local curTime = vrf:getSimulationTime()
   if curTime > s.nextSendTime then
      
      s.nextSendTime = curTime + sendInterval
   
      if next(s.subsNotReporting) then
         for sub, v in pairs(s.subsNotReporting) do
            vrf:sendMessage(sub, "parent")
         end
      end

      -- compare to this code in Tracking_Reports:      
      -- Enable this to send POSITION reports from this script.
--~       local reportString = string.format("POSITION \"%s\" %f %f",
--~           this:getName(), math.deg(this:getLocation3D():getLat()), 
--~             math.deg(this:getLocation3D():getLon())) .. 
--~       vrf:sendReport("text-report", {}, {text = reportString})
  
      reportString = "OBSERVATION " .. this:getForceType() .. " "
      local count = 0
      local entityString = "" -- All entities: general type, (lat, lon), and opStat for each
      for name, info in pairs(s.observations) do
         entityString = entityString .. info.type 
         entityString = entityString ..
            string.format(" %f %f %d %s ", 
               info.lat, info.lon, info.time, info.health)               
         count = count + 1
      end
      if count > 0 then
         reportString = reportString .. -- debugX s.observations["sourceForceType"]
                        string.format("\"%s\" ", this:getName()) .. 
                        string.format("%d ", count) ..
                        entityString 
         vrf:sendReport("text-report", {}, {text = reportString})
      end
   end
end

--=========================================================================

-- Split the input string into words, separate by spaces. Return in a table.
-- Treat quoted phrases as one word.
-- @param inputString The string to split
-- @return A table containing the words of the input string. Words are any contiguous
-- sequence of non-space characters.
function wordSplit(inputString)
   local out = {}
   local words = {}
   -- Pattern match contiguous characters that aren't spaces.
   local pat = "([^%s]+)" -- One or more of the set of characters that is the complement of space, ie all non-space characters; 
   for ss in string.gmatch(inputString, pat) do
      table.insert(words, ss)
   end
   
   -- Look for quotes
   local index = 1
   local outIndex = 1
   while index <= #words do
      if string.sub(words[index],1,1) == "\"" then -- Word starts with quote
         if string.sub(words[index],-1, -1) == "\"" then -- Same word ends with quote
            out[outIndex] = string.sub(words[index],2, -2)
         else
            -- A later word must end with a quote
            out[outIndex] = string.sub(words[index],2,-1) -- Copy, except the quote
            index = index + 1
            while string.sub(words[index], -1, -1) ~= "\"" and
               index <= #words do
               
               -- Assume a single space between words
               out[outIndex] = out[outIndex] .. " " .. words[index]
               index = index + 1
            end
            out[outIndex] = out[outIndex] .. " " ..
               string.sub(words[index],1,-2) -- Copy all but last quote char
         end
      else
         out[outIndex] = words[index]
      end
      index = index + 1
      outIndex = outIndex + 1
   end
   
   return out
end

function receiveTextMessage(message, sender)
   local found = false
   local senderName = sender:getName()

   for _s, _v in pairs(s.subs) do
      if _s:getName() == senderName then
         found = true
         break
      end
   end
   
   if found then
   -- if s.subs[sender:getName()] ~= nil then
      local words = wordSplit(message)
      if words[1] == "CONTACTLIST" then

         -- Make or update entries in the observation table for each observation
         local  numObs = 0 + words[3] -- string coercion to number
         local index = 3
         for _i = 1, numObs do
            local obsName = _i
            local obsInfo = {}
            obsInfo.type = words[index + 1]
            obsInfo.lat = 0. + words[index + 2] -- coercion to number
            obsInfo.lon = 0. + words[index + 3] -- coercion to number
            obsInfo.time = vrf:getSimulationTime()
            obsInfo.name = obsName
            obsInfo.health = words[index + 5]
            s.observations[obsName] = obsInfo
            index = index + 5
         end
         
         -- Update the list of subordinates that have not yet sent an observation
         for _s, _v in pairs(s.subsNotReporting) do
            if _s:getName() == senderName then
               s.subsNotReporting[_s] = nil
               break
            end
         end
      end
   end
end
