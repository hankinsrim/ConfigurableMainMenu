#pragma once

// Values from Data\SKSE\Plugins\MainMenuToggles.ini; every Show* option defaults to true (vanilla look).
struct Settings
{
	// [General]
	bool          hideOnStartup{ true };
	std::uint32_t toggleKey{ 0 };  // DirectInput scan code; 0 = no hotkey
	bool          enableLogging{ true };

	// [Visibility]
	std::vector<std::uint32_t> menuButtons;  // StartMenu.as button indices in display order; empty = vanilla
	bool                       showMenuButtons{ true };
	bool                       showSmoke{ true };
	bool                       showDragonMenuLogo{ true };
	bool                       showDragonLoadingLogo{ true };
	std::string                dragonMenuLogo;     // model path; empty = vanilla
	std::string                dragonLoadingLogo;  // model path; empty = vanilla
	bool                       showLoadingSpinner{ true };
	bool                       showMouseCursor{ true };
	bool                       showBethesdaLogo{ true };
	bool                       showMOTD{ true };

	[[nodiscard]] static Settings& Get() noexcept
	{
		static Settings singleton;
		return singleton;
	}

	// Read before SKSE::Init, which creates the log file.
	[[nodiscard]] static bool ReadEnableLogging();

	void Load();

	[[nodiscard]] bool HidesAnything() const noexcept;
};
