#include "UIHider.h"
#include "Settings.h"

namespace UIHider
{
	namespace
	{
		constexpr auto MENU_PATH    = "_root.MenuHolder.Menu_mc";
		constexpr auto SPINNER_PATH = "_root.Menu_mc";

		// AS2 places timeline depth d at script depth d - 16384.
		constexpr double PRESS_START_DEPTH = 75.0 - 16384.0;

		enum class Restore : std::uint8_t
		{
			kSnapshot,     // value observed before we first hid it
			kVersionRule,  // StartMenu.ShowDeleteButtonHelp: visible iff delete button hidden
		};

		struct Element
		{
			std::string label;
			std::string path;  // relative to Menu_mc; empty = PRESS_START_DEPTH lookup
			Restore     restore{ Restore::kSnapshot };
			bool        hidden{ false };
			bool        snapshot{ true };
			bool        reportedMissing{ false };
			bool        reportedFound{ false };
		};

		std::vector<Element> g_elements;
		RE::GFxMovieView*    g_movie{ nullptr };
		bool                 g_reportedNoMenu{ false };
		bool                 g_cursorHidden{ false };
		bool                 g_spinnerHidden{ false };
		std::recursive_mutex g_lock;  // the Main Menu updates from more than one thread

		void BuildElements()
		{
			const auto& s = Settings::Get();
			g_elements.clear();
			const auto add = [](bool a_show, std::string a_label, std::string a_path, Restore a_restore = Restore::kSnapshot) {
				if (!a_show)
				{
					g_elements.push_back({ std::move(a_label), std::move(a_path), a_restore });
				}
			};
			add(s.showMenuButtons, "ShowMenuButtons", "MainListHolder");
			// Menu text that only appears briefly or rarely on PC is hidden together with the buttons.
			add(s.showMenuButtons, "ShowMenuButtons", "");
			add(s.showMenuButtons, "ShowMenuButtons", "VersionText", Restore::kVersionRule);
			add(s.showMenuButtons, "ShowMenuButtons", "BottomButtons_mc");
			add(s.showMenuButtons, "ShowMenuButtons", "LoadingContentMessage");
			add(s.showBethesdaLogo, "ShowBethesdaLogo", "Logo_mc");
			// StartMenu.SetMotd re-shows MessageOfTheDay_mc itself, so its two text fields are hidden instead.
			add(s.showMOTD, "ShowMOTD", "MessageOfTheDay_mc.Header_tf");
			add(s.showMOTD, "ShowMOTD", "MessageOfTheDay_mc.Motd_tf");
		}

		// Same property the SWF's own scripts toggle, and unlike _alpha it does not detach the clip from its timeline.
		bool IsVisible(const RE::GFxValue& a_clip)
		{
			RE::GFxValue visible;
			return a_clip.GetMember("_visible", &visible) && visible.IsBool() ? visible.GetBool() : true;
		}

		void SetVisible(RE::GFxValue& a_clip, bool a_visible)
		{
			a_clip.SetMember("_visible", RE::GFxValue(a_visible));
		}

		bool GetPath(RE::GFxMovieView* a_movie, const std::string& a_path, RE::GFxValue& a_out)
		{
			const auto full = std::string(MENU_PATH) + "." + a_path;
			return a_movie->GetVariable(&a_out, full.c_str()) && a_out.IsDisplayObject();
		}

		bool Lookup(RE::GFxMovieView* a_movie, RE::GFxValue& a_menu, Element& a_element, RE::GFxValue& a_out)
		{
			if (!a_element.path.empty())
			{
				return GetPath(a_movie, a_element.path, a_out);
			}

			RE::GFxValue depth(PRESS_START_DEPTH);
			if (!a_menu.Invoke("getInstanceAtDepth", &a_out, &depth, 1) || !a_out.IsDisplayObject())
			{
				return false;
			}
			// Unnamed timeline instances are auto-named "instanceN", anything else here belongs to a replacer SWF.
			RE::GFxValue name;
			if (a_out.GetMember("_name", &name) && name.IsString() && std::string_view{ name.GetString() }.starts_with("instance"))
			{
				if (!a_element.reportedFound)
				{
					logger::info("UI: press-start prompt resolved as Menu_mc.{} (depth 75)", name.GetString());
				}
				return true;
			}
			return false;
		}

		bool RestoreValue(RE::GFxMovieView* a_movie, const Element& a_element)
		{
			RE::GFxValue value;
			switch (a_element.restore)
			{
			case Restore::kVersionRule:
				if (GetPath(a_movie, "DeleteSaveButton", value))
				{
					return !IsVisible(value);
				}
				break;
			default:
				break;
			}
			return a_element.snapshot;
		}

		void SetCursorHidden(bool a_hidden)
		{
			if (!a_hidden && !g_cursorHidden)
			{
				return;
			}
			auto* ui     = RE::UI::GetSingleton();
			auto  cursor = ui ? ui->GetMenu(RE::CursorMenu::MENU_NAME) : nullptr;
			if (!cursor || !cursor->uiMovie)
			{
				return;  // not open yet; retried next frame
			}
			// Enforced every frame because the Cursor Menu can be recreated with a fresh movie.
			if (cursor->uiMovie->GetVisible() == a_hidden)
			{
				cursor->uiMovie->SetVisible(!a_hidden);
			}
			if (g_cursorHidden != a_hidden)
			{
				g_cursorHidden = a_hidden;
				logger::info("UI: mouse cursor {}", a_hidden ? "hidden" : "restored");
			}
		}

		void RestoreElements(RE::GFxMovieView* a_movie)
		{
			for (auto& element : g_elements)
			{
				if (!element.hidden)
				{
					continue;
				}
				element.hidden = false;
				RE::GFxValue menu;
				RE::GFxValue target;
				if (a_movie && a_movie->GetVariable(&menu, MENU_PATH) && menu.IsDisplayObject() && Lookup(a_movie, menu, element, target))
				{
					const bool visible = RestoreValue(a_movie, element);
					SetVisible(target, visible);
					logger::info("UI: restored {} ({}) -> _visible={}", element.label, element.path.empty() ? "press-start prompt" : element.path, visible);
				}
			}
		}
	}

	void Update(RE::GFxMovieView* a_movie, bool a_hide)
	{
		std::scoped_lock lock(g_lock);
		if (a_movie != g_movie)
		{
			// New movie instance (first open, or returned to the main menu from in-game).
			g_movie          = a_movie;
			g_reportedNoMenu = false;
			BuildElements();
		}

		SetCursorHidden(a_hide && !Settings::Get().showMouseCursor);

		if (!a_movie || g_elements.empty())
		{
			return;
		}
		if (!a_hide)
		{
			RestoreElements(a_movie);
			return;
		}

		RE::GFxValue menu;
		if (!a_movie->GetVariable(&menu, MENU_PATH) || !menu.IsDisplayObject())
		{
			if (!g_reportedNoMenu)
			{
				logger::warn("UI: {} not found (replaced startmenu.swf?) - UI options have no effect.", MENU_PATH);
				g_reportedNoMenu = true;
			}
			return;
		}

		for (auto& element : g_elements)
		{
			RE::GFxValue target;
			if (!Lookup(a_movie, menu, element, target))
			{
				if (!element.reportedMissing)
				{
					logger::warn("UI: {} target \"{}\" not found in startmenu.swf - skipped.", element.label, element.path.empty() ? "press-start prompt (depth 75)" : element.path);
					element.reportedMissing = true;
				}
				continue;
			}
			if (!element.reportedFound)
			{
				logger::info("UI: found {} -> hiding (was _visible={})", element.path.empty() ? "press-start prompt"s : element.path, IsVisible(target));
				element.reportedFound = true;
			}
			if (!element.hidden)
			{
				element.snapshot = IsVisible(target);
				element.hidden   = true;
			}
			// The SWF re-shows several of these on state changes, so enforce every frame.
			if (IsVisible(target))
			{
				SetVisible(target, false);
			}
		}
	}

	void UpdateMenuButtons(RE::GFxMovieView* a_movie)
	{
		std::scoped_lock lock(g_lock);
		const auto&      wanted = Settings::Get().menuButtons;
		RE::GFxValue     list;
		RE::GFxValue     entries;
		if (!a_movie || wanted.empty() || !GetPath(a_movie, "MainListHolder.List_mc", list) || !list.GetMember("entryList", &entries) || !entries.IsArray())
		{
			return;
		}

		std::vector<RE::GFxValue>  current;
		std::vector<std::uint32_t> currentIndices;
		for (std::uint32_t i = 0; i < entries.GetArraySize(); ++i)
		{
			RE::GFxValue entry;
			RE::GFxValue index;
			entries.GetElement(i, &entry);
			current.push_back(entry);
			currentIndices.push_back(entry.IsObject() && entry.GetMember("index", &index) && index.IsNumber() ? static_cast<std::uint32_t>(index.GetNumber()) : UINT32_MAX);
		}

		// setupMainMenu rebuilds the list whenever the game refreshes it, so the order is checked every frame.
		std::vector<RE::GFxValue>  desired;
		std::vector<std::uint32_t> desiredIndices;
		for (const auto item : wanted)
		{
			if (const auto it = std::ranges::find(currentIndices, item); it != currentIndices.end())
			{
				desired.push_back(current[it - currentIndices.begin()]);
				desiredIndices.push_back(item);
			}
		}
		if (desired.empty() || desiredIndices == currentIndices)
		{
			return;
		}

		RE::GFxValue centered;
		RE::GFxValue centeredIndex;
		std::size_t  position = 0;
		if (list.GetMember("centeredEntry", &centered) && centered.IsObject() && centered.GetMember("index", &centeredIndex) && centeredIndex.IsNumber())
		{
			const auto it = std::ranges::find(desiredIndices, static_cast<std::uint32_t>(centeredIndex.GetNumber()));
			position      = it != desiredIndices.end() ? static_cast<std::size_t>(it - desiredIndices.begin()) : 0;
		}

		entries.RemoveElements(0);
		for (const auto& entry : desired)
		{
			entries.PushBack(entry);
		}
		const std::array<RE::GFxValue, 2> scrollArgs{ RE::GFxValue(static_cast<double>(position)), RE::GFxValue(false) };
		list.Invoke("RestoreScrollPosition", scrollArgs);
		list.Invoke("InvalidateData");
		logger::info("UI: main menu buttons set to {} of {} (MenuButtons)", desired.size(), current.size());
	}

	void UpdateSpinner(RE::GFxMovieView* a_movie, bool a_hide)
	{
		std::scoped_lock lock(g_lock);
		const bool       hide = a_hide && !Settings::Get().showLoadingSpinner;
		if (!a_movie || (!hide && !g_spinnerHidden))
		{
			return;
		}
		RE::GFxValue clip;
		if (!a_movie->GetVariable(&clip, SPINNER_PATH) || !clip.IsDisplayObject())
		{
			return;
		}
		// loadwaitspinner.swf only fades Menu_mc through its parent timeline and never touches _visible, so true is the vanilla value.
		if (IsVisible(clip) == hide)
		{
			SetVisible(clip, !hide);
		}
		if (g_spinnerHidden != hide)
		{
			g_spinnerHidden = hide;
			logger::info("UI: loading spinner {}", hide ? "hidden" : "restored");
		}
	}

	void Reset()
	{
		std::scoped_lock lock(g_lock);
		SetCursorHidden(false);
		g_elements.clear();
		g_movie          = nullptr;
		g_reportedNoMenu = false;
	}
}
