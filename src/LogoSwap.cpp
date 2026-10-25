#include "LogoSwap.h"
#include "Settings.h"

namespace LogoSwap
{
	namespace
	{
		// AE Address Library IDs and offsets of the `lea rcx, [path]` instructions that load the main menu logo.
		struct LeaSite
		{
			std::uint64_t id;
			std::size_t   offset;
		};

		constexpr auto MENU_LOGO_SITES = std::to_array<LeaSite>({
			{ 52135, 0x12 },
			{ 416420, 0x1B },
		});

		// AE Address Library ID of the BSFixedString the launch loading screen reads its logo path from.
		constexpr std::uint64_t LOADING_LOGO_PATH_ID = 406372;

		constexpr std::array<std::uint8_t, 3> LEA_RCX_RIP{ 0x48, 0x8D, 0x0D };
		constexpr std::size_t                 LEA_SIZE = LEA_RCX_RIP.size() + sizeof(std::int32_t);

		// Model each screen loads once Install has run; swapped paths point into the Settings singleton.
		const char* g_menuPath    = ANNIVERSARY_PATH;
		const char* g_loadingPath = CLASSIC_PATH;

		bool EqualsNoCase(const char* a_lhs, const char* a_rhs)
		{
			return a_lhs && a_rhs && _stricmp(a_lhs, a_rhs) == 0;
		}

		void SwapMenuLogo(const std::string& a_path)
		{
			// The new path must sit within 2 GB of the game code, which trampoline memory guarantees.
			auto& trampoline = SKSE::GetTrampoline();
			auto* text       = static_cast<char*>(trampoline.allocate(a_path.size() + 1));
			std::memcpy(text, a_path.c_str(), a_path.size() + 1);

			bool patchedAll = true;
			for (const auto& site : MENU_LOGO_SITES)
			{
				const auto* const instruction = REL::Relocation<const std::uint8_t*>{ REL::ID(site.id), static_cast<std::ptrdiff_t>(site.offset) }.get();
				const auto* const operand     = std::next(instruction, LEA_RCX_RIP.size());
				const auto* const next        = std::next(instruction, LEA_SIZE);  // RIP-relative displacements count from the next instruction
				std::int32_t      disp        = 0;
				std::memcpy(&disp, operand, sizeof(disp));
				const auto* const current = reinterpret_cast<const char*>(std::next(next, disp));
				if (std::memcmp(instruction, LEA_RCX_RIP.data(), LEA_RCX_RIP.size()) != 0 || !EqualsNoCase(current, ANNIVERSARY_PATH))
				{
					logger::warn("LogoSwap: unexpected code at ID {} +0x{:X} (different game version?) - DragonMenuLogo not applied there.", site.id, site.offset);
					patchedAll = false;
					continue;
				}
				const auto newDisp = reinterpret_cast<std::intptr_t>(text) - reinterpret_cast<std::intptr_t>(next);
				if (newDisp < INT32_MIN || newDisp > INT32_MAX)
				{
					logger::warn("LogoSwap: path memory out of range for ID {} - DragonMenuLogo not applied there.", site.id);
					patchedAll = false;
					continue;
				}
				const auto value = static_cast<std::int32_t>(newDisp);
				REL::safe_write(reinterpret_cast<std::uintptr_t>(operand), &value, sizeof(value), &disp, sizeof(disp));
			}
			if (patchedAll)
			{
				g_menuPath = a_path.c_str();
			}
			logger::info("LogoSwap: main menu logo -> {}", a_path);
		}

		void SwapLoadingLogo(const std::string& a_path)
		{
			REL::Relocation<RE::BSFixedString*> path{ REL::ID(LOADING_LOGO_PATH_ID) };
			if (!EqualsNoCase(path->c_str(), CLASSIC_PATH))
			{
				logger::warn("LogoSwap: launch logo path is \"{}\", not the vanilla one (different game version?) - DragonLoadingLogo not applied.", path->c_str());
				return;
			}
			*path         = a_path.c_str();
			g_loadingPath = a_path.c_str();
			logger::info("LogoSwap: loading screen logo -> {}", a_path);
		}
	}

	void Install()
	{
		const auto& s = Settings::Get();
		if (s.dragonMenuLogo.empty() && s.dragonLoadingLogo.empty())
		{
			return;
		}
		if (!REL::Module::IsAE())
		{
			logger::warn("LogoSwap: DragonMenuLogo / DragonLoadingLogo need Skyrim 1.6 or newer - not applied.");
			return;
		}
		if (!s.dragonMenuLogo.empty())
		{
			SwapMenuLogo(s.dragonMenuLogo);
		}
		if (!s.dragonLoadingLogo.empty())
		{
			SwapLoadingLogo(s.dragonLoadingLogo);
		}
	}

	bool SameModel()
	{
		return EqualsNoCase(g_menuPath, g_loadingPath);
	}
}
