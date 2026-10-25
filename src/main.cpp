#include "Controller.h"
#include "LogoSwap.h"
#include "Settings.h"

namespace
{
	constexpr std::size_t TRAMPOLINE_SIZE = 64;  // room for the swapped logo path

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (a_message->type == SKSE::MessagingInterface::kInputLoaded)
		{
			Controller::RegisterEvents();
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::InitInfo info;
	info.log            = Settings::ReadEnableLogging();
	info.trampoline     = true;
	info.trampolineSize = TRAMPOLINE_SIZE;
	SKSE::Init(a_skse, info);
	if (!info.log)
	{
		spdlog::set_level(spdlog::level::off);
	}

	const auto plugin = SKSE::GetPluginVersion();
	logger::info("{} v{}.{} loading on Skyrim {}", SKSE::GetPluginName(), plugin.major(), plugin.minor(), REL::Module::get().version().string("."sv));

	logger::info("Settings:");
	auto& settings = Settings::Get();
	settings.Load();
	if (!settings.HidesAnything())
	{
		logger::info("No element is set to be hidden - no hooks installed, the main menu will look vanilla.");
		return true;
	}

	LogoSwap::Install();
	Controller::InstallHooks();

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage))
	{
		logger::error("Could not register SKSE message listener");
		return false;
	}
	return true;
}
