#include "SceneHider.h"
#include "LogoSwap.h"
#include "Settings.h"

namespace SceneHider
{
	namespace
	{
		constexpr auto LOGO_ROOT_NAME = "Logo";

		struct CulledObject
		{
			RE::NiPointer<RE::NiAVObject> object;
			bool                          wasCulled;
		};

		std::vector<CulledObject>              g_culled;
		std::vector<RE::NiAVObject*>           g_roots;
		std::vector<RE::NiPointer<RE::NiNode>> g_loadingLogos;
		bool                                   g_anyVisible{ true };
		std::recursive_mutex                   g_lock;  // the Mist Menu updates from more than one thread

		// MistMenu's layout is reverse-engineered, so read the vtable under SEH to report a bad offset instead of crashing.
		bool TryReadPointer(const void* a_address, void*& a_out) noexcept
		{
			__try
			{
				a_out = *static_cast<void* const*>(a_address);
				return true;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				return false;
			}
		}

		bool HasNodeVtable(const void* a_object)
		{
			void* vtbl = nullptr;
			if (!a_object || !TryReadPointer(a_object, vtbl))
			{
				return false;
			}
			static const std::array<std::uintptr_t, 8> nodeVtables{
				RE::VTABLE_NiNode[0].address(),
				RE::VTABLE_BSFadeNode[0].address(),
				RE::VTABLE_BSMultiBoundNode[0].address(),
				RE::VTABLE_BSLeafAnimNode[0].address(),
				RE::VTABLE_BSValueNode[0].address(),
				RE::VTABLE_BSOrderedNode[0].address(),
				RE::VTABLE_NiBillboardNode[0].address(),
				RE::VTABLE_NiSwitchNode[0].address(),
			};
			return std::ranges::find(nodeVtables, reinterpret_cast<std::uintptr_t>(vtbl)) != nodeVtables.end();
		}

		RE::NiAVObject* MenuSceneRoot()
		{
			auto* manager = RE::UI3DSceneManager::GetSingleton();
			if (!manager)
			{
				return nullptr;
			}
			void* node = nullptr;
			void* vtbl = nullptr;
			if (!TryReadPointer(static_cast<const void*>(&manager->shadowSceneNode), node) || !node || !TryReadPointer(node, vtbl))
			{
				return nullptr;
			}
			return reinterpret_cast<std::uintptr_t>(vtbl) == RE::VTABLE_ShadowSceneNode[0].address() ? static_cast<RE::NiAVObject*>(node) : nullptr;
		}

		std::string_view NameOf(const RE::NiAVObject* a_object)
		{
			const char* name = a_object->name.c_str();
			return name && *name ? std::string_view{ name } : "<unnamed>"sv;
		}

		std::string_view TypeOf(RE::NiAVObject* a_object)
		{
			const auto* const rtti = a_object->GetRTTI();
			return rtti && rtti->name ? std::string_view{ rtti->name } : "?"sv;
		}

		template <class F>
		void ForEachObject(RE::NiAVObject* a_object, const F& a_func, int a_depth = 0)
		{
			if (!a_object)
			{
				return;
			}
			a_func(a_object, a_depth);
			if (auto* node = a_object->AsNode())
			{
				for (auto& child : node->GetChildren())
				{
					ForEachObject(child.get(), a_func, a_depth + 1);
				}
			}
		}

		void LogTree(std::string_view a_title, RE::NiAVObject* a_root)
		{
			logger::info("{}:", a_title);
			ForEachObject(a_root, [](RE::NiAVObject* a_obj, int a_depth) {
				logger::info("  {:{}}{} \"{}\"{}", "", a_depth * 2, TypeOf(a_obj), NameOf(a_obj), a_obj->GetAppCulled() ? " (already culled)" : "");
			});
		}

		RE::NiAVObject* SceneRoot(RE::NiAVObject* a_object)
		{
			while (a_object && a_object->parent)
			{
				a_object = a_object->parent;
			}
			return a_object;
		}

		bool IsUnder(RE::NiAVObject* a_object, RE::NiAVObject* a_ancestor)
		{
			for (; a_object; a_object = a_object->parent)
			{
				if (a_object == a_ancestor)
				{
					return true;
				}
			}
			return false;
		}

		bool HasVisibleGeometry(RE::NiAVObject* a_object)
		{
			if (!a_object || a_object->GetAppCulled())
			{
				return false;
			}
			if (netimmerse_cast<RE::BSGeometry*>(a_object) || netimmerse_cast<RE::NiParticleSystem*>(a_object))
			{
				return true;
			}
			auto* node = a_object->AsNode();
			return node && std::ranges::any_of(node->GetChildren(), [](const auto& a_child) { return HasVisibleGeometry(a_child.get()); });
		}

		void AddTarget(RE::NiAVObject* a_object, std::string_view a_reason)
		{
			for (const auto& entry : g_culled)
			{
				if (entry.object.get() == a_object)
				{
					return;
				}
			}
			g_culled.push_back({ RE::NiPointer<RE::NiAVObject>(a_object), a_object->GetAppCulled() });
			logger::info("Scene: hide {} \"{}\" ({})", TypeOf(a_object), NameOf(a_object), a_reason);
		}

		void RemoveTarget(RE::NiAVObject* a_object)
		{
			std::erase_if(g_culled, [&](const CulledObject& a_entry) {
				if (a_entry.object.get() != a_object)
				{
					return false;
				}
				a_entry.object->SetAppCulled(a_entry.wasCulled);
				logger::info("Scene: show {} \"{}\" again", TypeOf(a_object), NameOf(a_object));
				return true;
			});
		}

		// Culls the logo's children, not its root, so the engine stays in charge of the root.
		void SetLogoHidden(RE::NiNode* a_logo, bool a_hide, std::string_view a_reason)
		{
			auto& children = a_logo->GetChildren();
			for (auto& child : children)
			{
				if (child)
				{
					a_hide ? AddTarget(child.get(), a_reason) : RemoveTarget(child.get());
				}
			}
			if (children.empty())
			{
				a_hide ? AddTarget(a_logo, a_reason) : RemoveTarget(a_logo);
			}
		}

		// Collects the scene roots reachable from every model pointer the Mist Menu holds, so the logo is found before the smoke loads.
		std::vector<RE::NiAVObject*> FindRoots(RE::MistMenu* a_menu, RE::NiNode*& a_mist)
		{
			auto&      data       = a_menu->GetRuntimeData();
			const auto candidates = std::to_array<std::pair<void*, std::string_view>>({
				{ data.mistModel.get(), "mistModel" },
				{ data.loadScreenModel.get(), "loadScreenModel" },
				{ data.cameraPath.get(), "cameraPath" },
				{ data.cameraPathNode.get(), "cameraPathNode" },
			});
			a_mist                = HasNodeVtable(data.mistModel.get()) ? data.mistModel.get() : nullptr;

			std::vector<RE::NiAVObject*> roots;
			// The menu 3D root exists before any Mist Menu model loads, so the launch screen logo is found on its first frame.
			if (auto* shadow = MenuSceneRoot())
			{
				roots.push_back(shadow);
				if (std::ranges::find(g_roots, shadow) == g_roots.end())
				{
					LogTree("Scene: new scene root via UI3DSceneManager::shadowSceneNode", shadow);
				}
			}
			for (const auto& [pointer, name] : candidates)
			{
				if (!HasNodeVtable(pointer))
				{
					continue;
				}
				auto* root = SceneRoot(static_cast<RE::NiAVObject*>(pointer));
				if (std::ranges::find(roots, root) != roots.end())
				{
					continue;
				}
				roots.push_back(root);
				if (std::ranges::find(g_roots, root) == g_roots.end())
				{
					LogTree("Scene: new scene root via MistMenu::"s + std::string(name), root);
				}
			}
			return roots;
		}

		void CollectTargets(const std::vector<RE::NiAVObject*>& a_roots, RE::NiNode* a_mist, bool a_launching)
		{
			const auto& s = Settings::Get();

			if (a_mist && !s.showSmoke)
			{
				ForEachObject(a_mist, [](RE::NiAVObject* a_obj, int) {
					if (netimmerse_cast<RE::NiParticleSystem*>(a_obj))
					{
						AddTarget(a_obj, "ShowSmoke=false");
					}
				});
			}

			if (s.showDragonMenuLogo && s.showDragonLoadingLogo)
			{
				return;
			}
			// Every logo model is searched each frame because the launch screen and the main menu load different ones.
			for (auto* root : a_roots)
			{
				ForEachObject(root, [&](RE::NiAVObject* a_obj, int) {
					auto* logo = a_obj->AsNode();
					if (!logo || _stricmp(a_obj->name.c_str(), LOGO_ROOT_NAME) != 0)
					{
						return;
					}
					// A logo first seen before the main menu opened stays the loading logo, unless both screens share one model and so one dragon.
					const bool known   = std::ranges::any_of(g_loadingLogos, [&](const auto& a_known) { return a_known.get() == logo; });
					const bool loading = LogoSwap::SameModel() ? a_launching : known || a_launching;
					if (loading && !known)
					{
						g_loadingLogos.emplace_back(logo);
					}
					const bool show = loading ? s.showDragonLoadingLogo : s.showDragonMenuLogo;
					SetLogoHidden(logo, !show, loading ? "ShowDragonLoadingLogo=false"sv : "ShowDragonMenuLogo=false"sv);
				});
			}
		}

		// Objects that left the scene are restored and released so old models are not kept alive.
		void PruneDetached(const std::vector<RE::NiAVObject*>& a_roots)
		{
			std::erase_if(g_culled, [&](const CulledObject& a_entry) {
				const bool attached = std::ranges::any_of(a_roots, [&](RE::NiAVObject* a_root) { return IsUnder(a_entry.object.get(), a_root); });
				if (!attached)
				{
					a_entry.object->SetAppCulled(a_entry.wasCulled);
				}
				return !attached;
			});
			std::erase_if(g_loadingLogos, [&](const RE::NiPointer<RE::NiNode>& a_logo) {
				return std::ranges::none_of(a_roots, [&](RE::NiAVObject* a_root) { return IsUnder(a_logo.get(), a_root); });
			});
		}
	}

	void Update(RE::MistMenu* a_menu, bool a_hide, bool a_launching)
	{
		std::scoped_lock lock(g_lock);
		const auto&      s          = Settings::Get();
		const bool       wantsScene = !s.showSmoke || !s.showDragonMenuLogo || !s.showDragonLoadingLogo;
		if (!a_menu || !a_hide || !wantsScene)
		{
			RestoreAll();
			return;
		}

		RE::NiNode* mist  = nullptr;
		auto        roots = FindRoots(a_menu, mist);
		PruneDetached(roots);
		CollectTargets(roots, mist, a_launching);
		g_roots = std::move(roots);

		// Re-apply every frame in case the engine resets the flag.
		for (const auto& entry : g_culled)
		{
			if (!entry.object->GetAppCulled())
			{
				entry.object->SetAppCulled(true);
			}
		}

		const bool anyVisible = g_culled.empty() || std::ranges::any_of(g_roots, [](RE::NiAVObject* a_root) { return HasVisibleGeometry(a_root); });
		if (anyVisible != g_anyVisible)
		{
			g_anyVisible = anyVisible;
			logger::info("Scene: {}", anyVisible ? "visible geometry again - drawing the Mist Menu" : "every mesh is hidden - skipping the Mist Menu draw");
		}
	}

	void RestoreAll()
	{
		std::scoped_lock lock(g_lock);
		if (!g_culled.empty())
		{
			logger::info("Scene: restoring {} object(s)", g_culled.size());
		}
		for (const auto& entry : g_culled)
		{
			entry.object->SetAppCulled(entry.wasCulled);
		}
		g_culled.clear();
		g_roots.clear();
		g_loadingLogos.clear();
		g_anyVisible = true;
	}

	bool ShouldDraw()
	{
		std::scoped_lock lock(g_lock);
		return g_anyVisible;
	}
}
