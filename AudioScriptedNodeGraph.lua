local AudioScriptedNodeGraph = {}

-- Update paths to packages to point to system32 dlls and location of AudioDLL.dll (for windows playback)
package.path = package.path.. ";C:\\Windows\\System32\\?.dll"
package.path = package.path.. ";absolute\\path\\to\\audio-scripted-node-graph\\AudioDLL\\x64\\Release\\?.dll" -- IMPORTANT! This must be changed before playback can occur

local ffi = require("ffi") -- import FFI library
-- C Function declarations for usage in Lua
ffi.cdef[[ 
void Sleep(int ms);
void PlayWholeWavAsync(const char* fileName);
]]
local audio = ffi.load("absolute\\path\\to\\audio-scripted-node-graph\\AudioDLL\\x64\\Release\\AudioDLL") -- IMPORTANT! This must be changed before playback
local playing = false

function AudioScriptedNodeGraph.onInit(self, graph)
    -- Setting up linker nodes
    local inputInfos = {
        {type=octane.PT_STRING, label="File Name", defaultNodeType=octane.NT_FILE}
    }

    inputs = graph:setInputLinkers(inputInfos)
end

function AudioScriptedNodeGraph.onTrigger(self, graph)
    -- When triggered this plays back the entire wav audio file unless triggered again while playing
    local fileName = self:getInputValue(inputs[1])
    fileName = fileName:gsub([[\]], [[\\]])
    playing = not playing
    if playing then
        audio.PlayWholeWavAsync(fileName)
    else
        audio.PlayWholeWavAsync(nil)
    end    
end

return AudioScriptedNodeGraph

