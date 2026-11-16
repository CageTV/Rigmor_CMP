/*
 * Rigmor Guardian
 * Copyright (c) 2026 CageTV
 *
 * GPL-3.0-or-later (see LICENSE).
 */
#include "Guardian.h"

namespace
{
	void InitializeLog()
	{
		auto path = logger::log_directory();
		if (!path) {
			return;
		}
		*path /= "RigmorGuardian.log"sv;
		auto log = std::make_shared<spdlog::logger>("global log"s, std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v"s);
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			Guardian::Init();
			break;
		case SKSE::MessagingInterface::kNewGame:
		case SKSE::MessagingInterface::kPostLoadGame:
			Guardian::OnGameStart();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	InitializeLog();
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	logger::info("Rigmor Guardian loaded");
	return true;
}
