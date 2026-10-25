#include "Controller.h"
#include "SceneHider.h"
#include "Settings.h"
#include "UIHider.h"

namespace Controller
{
	namespace
	{
		std::atomic_bool g_launching{ true };  // game launch loading screen, until the first Main Menu opens
		std::atomic_bool g_mainMenuOpen{ false };
		std::atomic_bool g_hideEnabled{ false };

		bool Hiding()
		{
			if (g_mainMenuOpen)
			{
				return g_hideEnabled;
			}
			return g_launching && Settings::Get().hideOnStartup;
		}

		template <class F>
		void Guarded(const F& a_func)
		{
			try
			{
				a_func();
			}
			catch (const std::exception& e)
			{
				logger::error("Update failed: {}", e.what());
			}
		}

		// Each hook runs once per frame on the UI thread after the menu advanced, so our writes win over the menu's own.
		struct MainMenuHook
		{
			static void thunk(RE::MainMenu* a_this, float a_interval, std::uint32_t a_currentTime)
			{
				func(a_this, a_interval, a_currentTime);
				if (g_mainMenuOpen)
				{
					Guarded([&] {
						UIHider::UpdateMenuButtons(a_this->uiMovie.get());
						UIHider::Update(a_this->uiMovie.get(), g_hideEnabled);
					});
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct MistMenuHook
		{
			static void thunk(RE::MistMenu* a_this, float a_interval, std::uint32_t a_currentTime)
			{
				func(a_this, a_interval, a_currentTime);
				Guarded([&] { SceneHider::Update(a_this, Hiding(), g_launching); });
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Runs right before the Mist Menu draws so a newly attached logo is culled first, and skips the draw while nothing is visible.
		struct MistMenuPostDisplayHook
		{
			static void thunk(RE::MistMenu* a_this)
			{
				Guarded([&] { SceneHider::Update(a_this, Hiding(), g_launching); });
				if (SceneHider::ShouldDraw())
				{
					func(a_this);
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct SpinnerHook
		{
			static void thunk(RE::LoadWaitSpinner* a_this, float a_interval, std::uint32_t a_currentTime)
			{
				func(a_this, a_interval, a_currentTime);
				Guarded([&] { UIHider::UpdateSpinner(a_this->uiMovie.get(), Hiding()); });
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// IMenu vtable slots.
		constexpr std::size_t ADVANCE_MOVIE_SLOT = 0x5;
		constexpr std::size_t POST_DISPLAY_SLOT  = 0x6;

		template <class Hook>
		void HookVfunc(const REL::VariantID& a_vtable, std::size_t a_index, std::string_view a_name)
		{
			REL::Relocation<std::uintptr_t> vtbl{ a_vtable };
			Hook::func = vtbl.write_vfunc(a_index, Hook::thunk);
			logger::info("Hooked {} (vtable {:X})", a_name, vtbl.address());
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static MenuSink* GetSingleton()
			{
				static MenuSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* /*a_eventSource*/) override
			{
				if (!a_event || a_event->menuName != RE::MainMenu::MENU_NAME)
				{
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->opening)
				{
					g_hideEnabled  = Settings::Get().hideOnStartup;
					g_mainMenuOpen = true;
					g_launching    = false;
					logger::info("Main Menu opened - hiding {}", g_hideEnabled ? "ON" : "OFF");
				}
				else
				{
					g_mainMenuOpen = false;
					SceneHider::RestoreAll();
					UIHider::Reset();
					logger::info("Main Menu closed - everything restored");
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class InputSink final : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			static InputSink* GetSingleton()
			{
				static InputSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>* /*a_eventSource*/) override
			{
				const auto key = Settings::Get().toggleKey;
				if (!a_events || key == 0 || !g_mainMenuOpen)
				{
					return RE::BSEventNotifyControl::kContinue;
				}
				for (auto* event = *a_events; event; event = event->next)
				{
					auto* const button = event->AsButtonEvent();
					if (button && button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && button->GetIDCode() == key && button->IsDown())
					{
						g_hideEnabled = !g_hideEnabled;
						logger::info("Hotkey: hiding {}", g_hideEnabled ? "ON" : "OFF");
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void InstallHooks()
	{
		HookVfunc<MainMenuHook>(RE::VTABLE_MainMenu[0], ADVANCE_MOVIE_SLOT, "MainMenu::AdvanceMovie");
		HookVfunc<MistMenuHook>(RE::VTABLE_MistMenu[0], ADVANCE_MOVIE_SLOT, "MistMenu::AdvanceMovie");
		HookVfunc<MistMenuPostDisplayHook>(RE::VTABLE_MistMenu[0], POST_DISPLAY_SLOT, "MistMenu::PostDisplay");
		HookVfunc<SpinnerHook>(RE::VTABLE_LoadWaitSpinner[0], ADVANCE_MOVIE_SLOT, "LoadWaitSpinner::AdvanceMovie");
	}

	void RegisterEvents()
	{
		if (auto* ui = RE::UI::GetSingleton())
		{
			ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
			logger::info("Registered menu open/close sink");
			if (ui->IsMenuOpen(RE::MainMenu::MENU_NAME))
			{
				g_hideEnabled  = Settings::Get().hideOnStartup;
				g_mainMenuOpen = true;
				g_launching    = false;
				logger::info("Main Menu was already open - hiding {}", g_hideEnabled ? "ON" : "OFF");
			}
		}
		else
		{
			logger::error("UI singleton unavailable - plugin inactive");
		}

		if (Settings::Get().toggleKey != 0)
		{
			if (auto* input = RE::BSInputDeviceManager::GetSingleton())
			{
				input->AddEventSink(InputSink::GetSingleton());
				logger::info("Registered hotkey sink (scan code {})", Settings::Get().toggleKey);
			}
			else
			{
				logger::error("Input manager unavailable - hotkey disabled");
			}
		}
	}
}
