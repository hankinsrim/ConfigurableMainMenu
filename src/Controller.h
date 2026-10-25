#pragma once

// Owns the hiding state and wires the game hooks and events to SceneHider and UIHider.
namespace Controller
{
	// Plugin load: hook AdvanceMovie of the Main Menu, Mist Menu and LoadWaitSpinner.
	void InstallHooks();

	// kInputLoaded: register menu open/close and hotkey event sinks.
	void RegisterEvents();
}
