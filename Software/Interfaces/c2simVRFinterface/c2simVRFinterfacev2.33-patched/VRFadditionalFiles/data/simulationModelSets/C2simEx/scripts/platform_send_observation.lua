-- Original script by Doug Reece of MAK
-- modified 18Jun21 by JMP to produce observation reports from both Friendly & Opposing
-- to be used with c2simVRFinterface v.16 and later 

-- This script causes the entity to periodically send an observation text message 
-- to its parent. The identity of the parent determined by a text message that is
-- received from the parent. When this message is received, the script begins sending
-- messages.
-- Each message is in the format
-- CONTACTLIST <senderForceType> <senderName> <count> <class1> <lat> <lon> <time> <opstat> <class2> ...
-- where count is the number of observations being sent, and
-- class is a class of entity such as "tank", "apc", "aircraft", etc. The mapping from
-- DIS category to vehicle class is given in the landCategory table.

-- Some basic VRF Utilities defined in a common module.
require "vrfutil"

-- Global Variables

-- Print out counts of contact lists, and the contents of OBSERVATION reports,
-- when Notify Level is Debug.
local DEBUG = true

local sendInterval = 10
local reportDestroyed = true 

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
local s = checkpointState
s.nextSendTime = 0

-- VRForces functions to retrieve aspects of objects 
-- shut down when object is killed; so to enable reports
-- of killed objects, keep a list of those found last cycle
s.oldList = {} -- Save list from last time, so destroyed entities are noted
s.oldAggList = {} -- same role, for aggregates

local landCategory = {"tank",
   "afv",
   "armoredutility",
   "spa",
   "towedarty",
   "wheeledutility",
   "wheeledutility",
   "trackedutility",
   "trackedutility"}


-- Called when the task first starts. Never called again.
function init()
   s.parent = nil
   s.nextSendTime = math.random(15) + vrf:getSimulationTime()
   
   -- Set the tick period for this script.
   vrf:setTickPeriod(2)
end

-- Called each tick while this task is active.
function tick()
   local curTime = vrf:getSimulationTime()

   if curTime > s.nextSendTime then
      
      s.nextSendTime = curTime + sendInterval
      
      if s.parent == nil then
         local myType = this:getEntityType()
         
         -- if this object is aircraft,  collect
         -- observatons on non-aggregated entities
         if vrf:entityTypeMatches(myType, EntityType.PlatformAir()) then
            sendObservations()
         end
         return
      end
      
      --------
      -- sending to parent (i.e. this is part of aggregated unit)
      local contacts = this:getAllContacts()
      local thisForceType = this:getForceType()
      reportString = "CONTACTLIST " .. thisForceType .. " "
      local count = 0
      local entityString = "" -- All entities: general type and (lat, lon) for each   
      local newAggList = {} -- new contact table for aggregates
      for _i, entity in ipairs(contacts) do
         local info = this:getContactInfo(entity)
         if thisForceType ~= entity:getForceType() and
            info.detectionLevel >= 2 and
            not entity:isDestroyed() then
            local typeInfo
            local contactType = info.entityType       
 
            -- This is an object type, with an extra "n:" prefix, so strip that off.
            local i1, i2
            i1, i2 = string.find(contactType, ":")
            contactType = string.sub(contactType, i2+1, -1)
            
            if vrf:entityTypeMatches(contactType, EntityType.PlatformAir()) then
               typeInfo = "aircraft"
            elseif vrf:entityTypeMatches(contactType,
               EntityType.Lifeform()) then
               typeInfo = "human"
            elseif vrf:entityTypeMatches(contactType,
               EntityType.PlatformSurface()) then
               typeInfo = "ship"
               
            elseif vrf:entityTypeMatches(contactType,
               EntityType.PlatformLand()) then
               
               local category = GetCategory(contactType)
               local catString = landCategory[category]
               if catString ~= nil then 
                  typeInfo = catString
               else
                  typeInfo = "vehicle"
               end
            end
            
            -- get segment for contacts string, entities and
            -- aggregates to which they belong (empty segment 
            -- returned here indicates part of aggregate that is 
            -- not the first entity)
            local segment
            local superior
            local aggTypeInfo

            segment, superior, aggTypeInfo = getSegment(entity)
            if segment ~= "" then
               entityString = entityString .. segment         
               count = count + 1

               -- save entity, mapped to type in newAggList
               -- clear same entity from oldAggList
               if reportDestroyed then
                  newAggList[superior] = aggTypeInfo
                  s.oldAggList[superior] = nil
               end
            end -- if segment
         end -- if thisForceType
      end -- for _i, entity 

      -- go back and report kills of objects found in
      -- former cycles but not in this one
      if reportDestroyed then
         -- Now see if entities on the old list were destroyed
         for entity, deadTypeInfo in pairs(s.oldAggList) do
            if entity:isValid() then
               if entity:isDestroyed() then
                  local loc = entity:getLocation3D()
                  local lat = math.deg(loc:getLat())
                  local lon = math.deg(loc:getLon())
                  entityString = entityString .. deadTypeInfo ..
                     string.format(" %f %f %d NotOp ", 
                        lat, lon, curTime)
                  count = count + 1
               end
            end -- if entity:isValid()
         end -- for entity
         s.oldAggList = newAggList

      end -- if reportDestroyed

      -- send the contacts in message to parent
      if count > 0 then
         reportString = reportString .. 
                        string.format("%d ", count) ..
                        entityString
         vrf:sendMessage(s.parent, reportString)
luaPrint("OBS2:" .. reportString)
      end

   -- terminate if curTime > s.nextSendTime 
   end -- if curTime > s.nextSendTime
end -- tick()

-- to collect observations on non-aggregated entities
function sendObservations()
   local reportString = "OBSERVATION " 
   local contacts = this:getAllContacts()
   local count = 0
   local entityString = "" -- All entities: general type and (lat, lon) for each
   local curTime = vrf:getSimulationTime()
   local newList = {} -- New contact table
   
   local debug = this:getNotifyLevel() == 4 and DEBUG
   if debug then
      local curCount = 0
      for k, v in pairs(contacts) do curCount = curCount + 1 end
      local oldCount = 0
      for k, v in pairs(s.oldList) do oldCount = oldCount + 1 end
      luaPrint(string.format("%.3f # contacts: %d; old contacts: %d",
            vrf:getSimulationTime(), curCount, oldCount))
   end
         
   for _i, entity in ipairs(contacts) do
      local info = this:getContactInfo(entity)

      if info.detectionLevel >= 2 and
         not entity:isDestroyed() then
         
         local typeInfo
         local contactType = info.entityType
         -- This is an object type, with an extra "n:" prefix, so strip that off.
         local i1, i2
         i1, i2 = string.find(contactType, ":")
         contactType = string.sub(contactType, i2+1, -1)
         
         if vrf:entityTypeMatches(contactType, EntityType.PlatformAir()) then
            typeInfo = "aircraft"
         elseif vrf:entityTypeMatches(contactType,
            EntityType.Lifeform()) then
            typeInfo = "human"
         elseif vrf:entityTypeMatches(contactType,
            EntityType.PlatformSurface()) then
            typeInfo = "ship"
            
         elseif vrf:entityTypeMatches(contactType,
            EntityType.PlatformLand()) then
            
            local category = GetCategory(contactType)
            local catString = landCategory[category]
            if catString ~= nil then 
               typeInfo = catString
            else
               typeInfo = "vehicle"
            end
         end

         entityString = entityString .. 
            typeInfo .. " "
      
         local loc = entity:getLocation3D()
         local lat = math.deg(loc:getLat())
         local lon = math.deg(loc:getLon())
         entityString = entityString .. 
            string.format(" %f %f %d FullOp ", 
               lat, lon, curTime)
            
         count = count + 1
         --  
         -- keep entity, mapped to type
         -- clear same entity from old list
         if reportDestroyed then
            newList[entity] = typeInfo
            s.oldList[entity] = nil
         end
         
      end
   end
   
   if reportDestroyed then
      -- Now see if entities on the old list were destroyed
      -- if they were, make a report that they are dead
      for entity, typeInfo in pairs(s.oldList) do
         if entity:isValid() then
            if entity:isDestroyed() then
               local loc = entity:getLocation3D()
               local lat = math.deg(loc:getLat())
               local lon = math.deg(loc:getLon())
               entityString = entityString .. typeInfo..
                  string.format(" %f %f %d NotOp ", 
                     lat, lon, curTime)
               count = count + 1
            end
         end
      end
      s.oldList = newList 
   end
            
   if count > 0 then
      printInfo("Sending OBSERVATION text report")
      reportString = reportString ..
                     this:getForceType() .. " " .. 
                     string.format("\"%s\" ", this:getName()) ..                   
                     string.format("%d ", count) ..
                     entityString
      if debug then
         luaPrint(reportString)
      end
      vrf:sendReport("text-report", {}, {text = reportString})
   end

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

-- Split the input string into substrings, using the given character as a delimiter.<br/>
-- @param inputString The string to split
-- @param delimiter The single character, or substring, used to split the input. 
-- @return A table containing the substrings of the input string in each entry. The substrings
-- will not include the delimiter character or substring. Leading spaces in each substring 
-- are removed.
function split(inputString, delimiter)
   local out = {}
   -- Add the delimiter to the end to get the tail of the string
   local str = inputString..delimiter
   -- The pattern matches 0 or more spaces, followed by 1 or more non-delimiter
   -- characters, followed by the delimiter. The captured substring is everything
   -- between the leading spaces and the final delimiter.
   local pat = "%s*([^"..delimiter.."]+)"..delimiter
   for ss in string.gmatch(str, pat) do
      table.insert(out, ss)
   end
   return out
end


function receiveTextMessage(message, sender)
   if message == "parent" then
      s.parent = sender
      printVerbose("Send observation: parent is ", s.parent)
   else
      if string.find(message, "parent") then
         printVerbose(string.format("  Rcd \"%s\" ",
            message))
      end
   end
end

------------------------------------------------------------------------------------------------
-- Returns the nth field in the entity type. Generally not used directly, but by other
-- utility functions defined below.
function GetNthField(entityType, n)
   if n < 1 or n > 7 then
      return nil
   end
   
   local i = 1
   for val in string.gmatch(entityType, "%-*%d+") do      
      if i == n then
         return tonumber(val)
      end
      
      i = i + 1
   end
   
   return 0 -- field was not specified in entityType, so must be 0
end
-- Returns the category field from the given entity type
function GetCategory(entityType)
   return GetNthField(entityType, 4)
end

-- returns segment of contact list for operational status
-- for aggregates, calculates percent of subordinates
-- surviving and uses that to derive status
-- VRForces4.9 does not offer this info for aggregates,
-- so we do it by lookup every time we see first subordinate
-- for other subordinates we return empty string 
-- by JMP 23 June 2021
function getSegment(object)

   -- VRForces4.9 does not provide opStat for aggregates
   -- so we fudge opStat whenever we see first entity in aggregate
   -- by assigning opStat of aggregate based on all its entities
   
   -- find object's superior if it has one
   local subordinate
   local superior = object:getSuperior()

   if superior == nil then 
      luaPrint("ERROR - getSegment expects entities in aggregate")
      return ""
   end
   if superior:isAggregate() ~= true then 
      luaPrint("ERROR - getSegment expects entities in aggregate")
      return ""
   end
   
   -- find first subordinate of the superior
   local allSubordinates = superior:getSubordinates()
   for _k, firstSubordinate in ipairs(allSubordinates) do
      -- arbitrarily take first subordinate
      -- (there must be a better way to get first element in lua!)
      subordinate = firstSubordinate
      break
   end

   -- for other than first element return empty string
   if subordinate ~= object then return "" end
   if subordinate == nil then return "" end
                 
   -- if we found a first entity calculate opstat of superior
         
   -- count superior's defunct subordinates     
   local totalCount = 0
   local deadCount = 0  
   for _j, subentity in ipairs(allSubordinates) do
      totalCount = totalCount + 1
      if subentity:isDestroyed() then deadCount = deadCount + 1 end
   end

   -- assign opstat by Jim Ruth's algorithm
   local deadFraction = deadCount/totalCount
   local derivedOpStat
   if deadFraction > .6 then derivedOpStat = "NotOp"
      elseif deadFraction > .4 then derivedOpStat = "PartlyOp"
      elseif deadFraction > .20 then derivedOpStat = "MostlyOp" 
      else derivedOpStat = "FullOp"
   end 
   
   -- collect the other parameters for segment
   local loc = superior:getLocation3D()
   local lat = math.deg(loc:getLat())
   local lon = math.deg(loc:getLon())
   local disType = superior:getEntityType();

   -- match an aggregate name to entityType
   -- TODO: many more DIStypes
   local typeInfo = "AggregateUnit"
   if disType == "11:1:225:5:20:0:0" then typeInfo = "USA_ArmorHQ"
   elseif disType == "11:1:225:3:2:0:0" then typeInfo = "USA_ArmorHQ"
   elseif disType == "11:1:0:13:34:0:1" then typeInfo = "MobileIrregular"
   end 

   -- make the segment and save values we found
   local curTime = vrf:getSimulationTime()
   local segment = typeInfo .. string.format(" %f %f %d ", lat, lon, curTime) ..
      derivedOpStat .. " "

   return segment, superior, typeInfo

end -- function getSegment(object)