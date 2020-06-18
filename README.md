# Audio Scripted Node Graph

## [Immersive Digital Experience Alliance](http://immersivealliance.org/)

A demonstration of the extensibility of ITMF through adding WAV audio playback on Windows. Through usage of Lua and LuaJIT's Foreign Function Interface (FFI), external C functions from a custom dynamically linked library (.dll) within Windows or shared objects (.so) in Linux written in C or C++ can be called from Lua. This solution is a wrapper around Windows PlaySound function to build a .dll that can be utilized in an ITMF Lua Scripted Node Graph.

## Table of Contents

* [Requirements](#requirements)
* [Building](#Building)
* [Playback in OctaneRender](#playback-in-octanerender)
* [Discussion](#discussion)

## Requirements

* Windows 10 64-bit
* Visual Studio 2019
* OctaneRender 4.05 or above
    * Lua 5.3 (built into OctaneRender)

## Building

The `AudioDLL` project was built with Visual Studio 2019. The .sln file provided should provide the necessary set up needed for a release build. A build is accomplished by `Ctrl+Shift+B` or selecting `Build > Build Solution`.

In the case of undefined references or many errors resulting from `winmm.lib` not being added as dependency. This is accomplished by opening `Project > AudioDLL Properties`, then navigating to `Linker > Input > Additional Dependencies`. `winmm.lib` must then be added. 

## Playback in OctaneRender

**IMPORTANT!**
Prior to adding the script to the scripted node graph, the absolute path to the DLL must first be defined in on line 5 and 12 within `AudioScriptedNodeGraph.lua`.

The script can then be added to a Scripted Node Graph. The first time the script is ran, the file input pin is created. Once the pin is created, a node defining the path to the WAV file can be input into the filename pin. Right clicking the node and selecting "Trigger" will play back the audio.

## Discussion

This is intended to show the extensibility of ITMF through simple audio playback. Solutions for an official immersive audio implementation within an ITMF Scene Graph are currently under consideration by IDEA.

Lua and LuaJIT FFI extensibiity in ITMF is powerful as it provides an interface for C functions and data structures enabling users to leverage both for custom functionality within an ITMF Scripted Node Graph. A collection of already existing Lua scripts used and created by Octane users can be found [here](https://render.otoy.com/forum/viewtopic.php?f=73&t=57252).

Lua Scripted Node Graphs in ITMF is only currently supported in OctaneRender and OctaneRender plug-ins though wider industry adoption will allow for this extensibility to be provided in all DCCs.

Original implementation included frame by frame playback support through feeding a buffer to the wavOut* API. This is still included within the `AudioDLL` project, though the Lua FFI does not utilize it. Further implementation and work on single frame playback as well as spatial and immersive audio via Windows `ISpatialAudioClient` may be possible in the future. The implementation provided has been simplified from the original implementation.
