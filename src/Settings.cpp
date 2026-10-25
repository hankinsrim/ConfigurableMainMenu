#include "Settings.h"
#include "LogoSwap.h"

namespace
{
	constexpr auto INI_PATH = "Data/SKSE/Plugins/MainMenuToggles.ini";

	struct BoolOption
	{
		const char* section;
		const char* key;
		bool Settings::* member;
	};

	constexpr auto BOOL_OPTIONS = std::to_array<BoolOption>({
		{ "General", "HideOnStartup", &Settings::hideOnStartup },
		{ "Visibility", "ShowMenuButtons", &Settings::showMenuButtons },
		{ "Visibility", "ShowSmoke", &Settings::showSmoke },
		{ "Visibility", "ShowDragonMenuLogo", &Settings::showDragonMenuLogo },
		{ "Visibility", "ShowDragonLoadingLogo", &Settings::showDragonLoadingLogo },
		{ "Visibility", "ShowLoadingSpinner", &Settings::showLoadingSpinner },
		{ "Visibility", "ShowMouseCursor", &Settings::showMouseCursor },
		{ "Visibility", "ShowBethesdaLogo", &Settings::showBethesdaLogo },
		{ "Visibility", "ShowMOTD", &Settings::showMOTD },
	});

	// Names and StartMenu.as *_INDEX values of the main menu buttons shown on PC.
	constexpr auto MENU_BUTTONS = std::to_array<std::pair<std::string_view, std::uint32_t>>({
		{ "Continue", 0 },
		{ "New", 1 },
		{ "Load", 2 },
		{ "Creations", 5 },
		{ "Credits", 8 },
		{ "Quit", 9 },
	});

	bool EqualsNoCase(std::string_view a, std::string_view b)
	{
		return std::ranges::equal(a, b, [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
	}

	std::vector<std::uint32_t> ParseMenuButtons(std::string_view a_value)
	{
		std::vector<std::uint32_t> items;
		while (!a_value.empty())
		{
			const auto comma = a_value.find(',');
			auto       name  = a_value.substr(0, comma);
			a_value          = comma == std::string_view::npos ? std::string_view{} : a_value.substr(comma + 1);
			while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
			{
				name.remove_prefix(1);
			}
			while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
			{
				name.remove_suffix(1);
			}
			if (name.empty())
			{
				continue;
			}
			const auto item = std::ranges::find_if(MENU_BUTTONS, [&](const auto& a_item) { return EqualsNoCase(a_item.first, name); });
			if (item == MENU_BUTTONS.end())
			{
				logger::warn("  [Visibility] MenuButtons: unknown button \"{}\" - ignored (check spelling).", name);
			}
			else if (std::ranges::find(items, item->second) == items.end())
			{
				items.push_back(item->second);
			}
		}
		return items;
	}

	bool IsKnownKey(std::string_view a_section, std::string_view a_key)
	{
		const auto eq = EqualsNoCase;
		if (eq(a_section, "General") && (eq(a_key, "ToggleKey") || eq(a_key, "EnableLogging")))
		{
			return true;
		}
		if (eq(a_section, "Visibility") && (eq(a_key, "MenuButtons") || eq(a_key, "DragonMenuLogo") || eq(a_key, "DragonLoadingLogo")))
		{
			return true;
		}
		return std::ranges::any_of(BOOL_OPTIONS, [&](const BoolOption& a_opt) { return eq(a_section, a_opt.section) && eq(a_key, a_opt.key); });
	}

	std::string ParseDragonLogo(CSimpleIniA& a_ini, const char* a_key)
	{
		std::string_view value = a_ini.GetValue("Visibility", a_key, "");
		std::string      path;
		if (EqualsNoCase(value, "Classic"))
		{
			path = LogoSwap::CLASSIC_PATH;
		}
		else if (EqualsNoCase(value, "Anniversary"))
		{
			path = LogoSwap::ANNIVERSARY_PATH;
		}
		else if (!value.empty())
		{
			logger::warn("  [Visibility] {}: unknown value \"{}\" (use Classic or Anniversary) - vanilla kept.", a_key, value);
		}
		logger::info("  [Visibility] {} = {}", a_key, path.empty() ? "(vanilla)"s : path);
		return path;
	}
}

bool Settings::ReadEnableLogging()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	return ini.LoadFile(INI_PATH) < 0 || ini.GetBoolValue("General", "EnableLogging", true);
}

void Settings::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	if (const auto rc = ini.LoadFile(INI_PATH); rc < 0)
	{
		logger::warn("Could not read {} (error {}); using defaults (vanilla look, nothing hidden).", INI_PATH, static_cast<int>(rc));
		return;
	}

	// Options are logged in the order they appear in the shipped INI.
	const auto loadBools = [&](std::string_view a_section) {
		for (const auto& opt : BOOL_OPTIONS)
		{
			if (opt.section == a_section)
			{
				this->*opt.member = ini.GetBoolValue(opt.section, opt.key, this->*opt.member);
				logger::info("  [{}] {} = {}", opt.section, opt.key, this->*opt.member);
			}
		}
	};

	loadBools("General");
	toggleKey = static_cast<std::uint32_t>(ini.GetLongValue("General", "ToggleKey", 0));
	logger::info("  [General] ToggleKey = {} ({})", toggleKey, toggleKey ? "hotkey enabled" : "no hotkey");
	enableLogging = ini.GetBoolValue("General", "EnableLogging", enableLogging);
	logger::info("  [General] EnableLogging = {}", enableLogging);

	const std::string menuButtonsValue = ini.GetValue("Visibility", "MenuButtons", "");
	menuButtons                        = ParseMenuButtons(menuButtonsValue);
	logger::info("  [Visibility] MenuButtons = {}", menuButtons.empty() ? "(vanilla)"s : menuButtonsValue);
	loadBools("Visibility");
	dragonMenuLogo    = ParseDragonLogo(ini, "DragonMenuLogo");
	dragonLoadingLogo = ParseDragonLogo(ini, "DragonLoadingLogo");

	// Typos in option names would otherwise be silently ignored.
	CSimpleIniA::TNamesDepend sections;
	ini.GetAllSections(sections);
	for (const auto& section : sections)
	{
		CSimpleIniA::TNamesDepend keys;
		ini.GetAllKeys(section.pItem, keys);
		for (const auto& key : keys)
		{
			if (!IsKnownKey(section.pItem, key.pItem))
			{
				logger::warn("  Unknown option [{}] {} in INI - ignored (check spelling).", section.pItem, key.pItem);
			}
		}
	}
}

bool Settings::HidesAnything() const noexcept
{
	for (const auto& opt : BOOL_OPTIONS)
	{
		if (opt.member != &Settings::hideOnStartup && !(this->*opt.member))
		{
			return true;
		}
	}
	return !menuButtons.empty() || !dragonMenuLogo.empty() || !dragonLoadingLogo.empty();
}
