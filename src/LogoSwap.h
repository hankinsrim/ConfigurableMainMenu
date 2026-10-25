#pragma once

// Chooses which dragon model the launch loading screen and the main menu load, by redirecting the game's own model paths.
namespace LogoSwap
{
	inline constexpr auto CLASSIC_PATH     = "interface/logo/logo.nif";
	inline constexpr auto ANNIVERSARY_PATH = "interface/logo/logo01ae.nif";

	// Plugin load: patch the paths before the launch screen and main menu load their logos.
	void Install();

	// True when both screens load the same model, in which case the game keeps the launch screen's dragon for the main menu.
	[[nodiscard]] bool SameModel();
}
