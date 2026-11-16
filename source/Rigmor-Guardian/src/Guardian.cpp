#include "Guardian.h"

// Watches the scenes and quest stages of the Rigmor trilogy from native code and, when one of them hangs,
// does what the Papyrus "Rose Watcher" did by hand: stop the stuck scene and push the quest stage on.
//
//  * rules   : a quest stage range (+ optionally one scene) that, once it has held for maxSeconds, runs a list of actions
//  * observer: every scene defined by the Rigmor plugins is timed; one that stays in the same phase too long is written
//              to the log, so the next stall can be turned into a rule without guessing (and, if switched on, stopped)
//
// Nothing here polls from Papyrus: the state is read straight from the scene/quest objects once a second, and the
// actions are the same Papyrus calls the mod's own scripts make (Scene.Stop, Quest.SetStage).

namespace Guardian
{
	namespace
	{
		using json = nlohmann::json;

		constexpr auto kConfigPath = "Data/SKSE/Plugins/RigmorGuardian.json";

		struct Action
		{
			enum class Type
			{
				StopScene,
				SetStage,
				EvaluatePackage,
				Notify
			};
			Type         type{ Type::StopScene };
			int          stage{ 0 };
			std::string  form;  // EvaluatePackage: "XXXXXX:Plugin.esp" of the actor
			std::string  text;  // Notify
			RE::Actor*   actor{ nullptr };
		};

		struct Rule
		{
			std::string          name;
			bool                 enabled{ true };
			std::string          questKey;
			std::string          sceneKey;
			int                  stageMin{ 0 };
			int                  stageMax{ 65535 };
			double               maxSeconds{ 30.0 };
			double               cooldown{ 20.0 };
			int                  maxAttempts{ 3 };
			std::vector<Action>  actions;
			// resolved
			RE::TESQuest*        quest{ nullptr };
			RE::BGSScene*        scene{ nullptr };
			// state
			double               stuck{ 0.0 };
			double               cool{ 0.0 };
			int                  attempts{ 0 };
		};

		struct Settings
		{
			bool   enabled{ true };
			bool   act{ false };  // false = only write what would be done
			bool   notify{ false };
			bool   debugLog{ false };
			double tickSeconds{ 1.0 };
			double settleSeconds{ 10.0 };
		};

		struct Observer
		{
			bool                     enabled{ true };
			std::vector<std::string> plugins{ "Rigmor.esp", "RigmorCyrodiil.esm", "RigmorCyrodiilPatch.esp" };
			double                   phaseStallSeconds{ 45.0 };
			double                   actSeconds{ 0.0 };  // 0 = never stop a scene because of the observer alone
			int                      maxAttempts{ 2 };
		};

		struct Track
		{
			RE::BGSScene* scene{ nullptr };
			double        playing{ 0.0 };
			std::uint32_t phase{ 0xFFFFFFFFu };
			double        phaseTime{ 0.0 };
			bool          warned{ false };
			int           acted{ 0 };
		};

		Settings           g_settings;
		Observer           g_observer;
		std::vector<Rule>  g_rules;
		std::vector<Track> g_tracks;
		double             g_settle{ 0.0 };
		std::atomic<bool>  g_ready{ false };
		std::atomic<bool>  g_started{ false };

		bool IEquals(std::string_view a, std::string_view b)
		{
			return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
		}

		template <class T>
		T* Resolve(const std::string& a_key)
		{
			const auto pos = a_key.find(':');
			if (pos == std::string::npos) {
				return nullptr;
			}
			RE::FormID id = 0;
			try {
				id = static_cast<RE::FormID>(std::stoul(a_key.substr(0, pos), nullptr, 16));
			} catch (...) {
				return nullptr;
			}
			auto* dh = RE::TESDataHandler::GetSingleton();
			return dh ? dh->LookupForm<T>(id, std::string_view(a_key).substr(pos + 1)) : nullptr;
		}

		std::string Describe(const RE::TESForm* a_form)
		{
			if (!a_form) {
				return "<none>";
			}
			const auto* file = a_form->GetFile(0);
			const char* editor = a_form->GetFormEditorID();
			return std::format("{}{}[{:08X}]{}", editor && *editor ? editor : "", editor && *editor ? " " : "", a_form->GetFormID(), file ? std::format(" ({})", file->GetFilename()) : std::string{});
		}

		bool CallVM(RE::TESForm* a_form, const char* a_class, const char* a_function, RE::BSScript::IFunctionArguments* a_args)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm || !a_form) {
				return false;
			}
			auto* policy = vm->GetObjectHandlePolicy();
			if (!policy) {
				return false;
			}
			const auto                                               handle = policy->GetHandleForObject(static_cast<std::uint32_t>(a_form->GetFormType()), a_form);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
			return vm->DispatchMethodCall(handle, a_class, a_function, a_args, none);
		}

		void StopScene(RE::BGSScene* a_scene)
		{
			const bool ok = CallVM(a_scene, "Scene", "Stop", RE::MakeFunctionArguments());
			logger::info("  Scene.Stop on {} -> {}", Describe(a_scene), ok ? "dispatched" : "FAILED");
		}

		void SetStage(RE::TESQuest* a_quest, int a_stage)
		{
			const bool ok = CallVM(a_quest, "Quest", "SetStage", RE::MakeFunctionArguments(static_cast<std::int32_t>(a_stage)));
			logger::info("  Quest.SetStage({}) on {} -> {}", a_stage, Describe(a_quest), ok ? "dispatched" : "FAILED");
		}

		void Notify(const std::string& a_text)
		{
			if (!a_text.empty()) {
				RE::SendHUDMessage::ShowHUDMessage(a_text.c_str());
			}
		}

		void Fire(Rule& a_rule)
		{
			logger::warn("RULE '{}' fired: quest {} at stage {}, held {:.0f}s{}", a_rule.name, Describe(a_rule.quest), a_rule.quest->currentStage, a_rule.maxSeconds,
				a_rule.scene ? std::format(", scene {} phase {}", Describe(a_rule.scene), a_rule.scene->currentPhaseIndex) : std::string{});
			if (!g_settings.act) {
				logger::warn("  observe mode (settings.act = false): not acting");
				return;
			}
			for (auto& a : a_rule.actions) {
				switch (a.type) {
				case Action::Type::StopScene:
					if (a_rule.scene) {
						StopScene(a_rule.scene);
					}
					break;
				case Action::Type::SetStage:
					SetStage(a_rule.quest, a.stage);
					break;
				case Action::Type::EvaluatePackage:
					if (a.actor) {
						const bool ok = CallVM(a.actor, "Actor", "EvaluatePackage", RE::MakeFunctionArguments(false));
						logger::info("  Actor.EvaluatePackage on {} -> {}", Describe(a.actor), ok ? "dispatched" : "FAILED");
					}
					break;
				case Action::Type::Notify:
					Notify(a.text);
					break;
				}
			}
			if (g_settings.notify) {
				Notify(std::format("Rigmor Guardian: unstuck '{}'", a_rule.name));
			}
		}

		void Tick(double a_dt)
		{
			if (!g_ready || !g_settings.enabled) {
				return;
			}
			auto* ui = RE::UI::GetSingleton();
			auto* pc = RE::PlayerCharacter::GetSingleton();
			if (!ui || !pc || !pc->Is3DLoaded()) {
				return;
			}
			if (ui->GameIsPaused() || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
				return;
			}
			a_dt = std::min(a_dt, 5.0);
			if (g_settle > 0.0) {
				g_settle -= a_dt;
				return;
			}

			for (auto& r : g_rules) {
				if (!r.enabled || !r.quest) {
					continue;
				}
				if (r.cool > 0.0) {
					r.cool -= a_dt;
				}
				const int  stage = r.quest->currentStage;
				const bool inRange = r.quest->IsRunning() && stage >= r.stageMin && stage <= r.stageMax;
				const bool hold = inRange && (!r.scene || r.scene->isPlaying);
				if (!inRange) {
					r.attempts = 0;
				}
				if (!hold) {
					r.stuck = 0.0;
					continue;
				}
				if (r.cool > 0.0 || r.attempts >= r.maxAttempts) {
					continue;
				}
				r.stuck += a_dt;
				if (r.stuck >= r.maxSeconds) {
					Fire(r);
					r.stuck = 0.0;
					r.cool = r.cooldown;
					++r.attempts;
				}
			}

			if (!g_observer.enabled) {
				return;
			}
			for (auto& t : g_tracks) {
				auto* s = t.scene;
				if (!s->isPlaying) {
					if (t.playing > 0.0 && g_settings.debugLog) {
						logger::debug("scene {} ended after {:.0f}s", Describe(s), t.playing);
					}
					t = Track{ s };
					continue;
				}
				t.playing += a_dt;
				const auto phase = s->currentPhaseIndex;
				if (phase != t.phase) {
					t.phase = phase;
					t.phaseTime = 0.0;
					t.warned = false;
				} else {
					t.phaseTime += a_dt;
				}
				if (t.phaseTime >= g_observer.phaseStallSeconds && !t.warned) {
					t.warned = true;
					const auto* q = s->parentQuest;
					logger::warn("STALL? scene {} stayed in phase {} for {:.0f}s (playing {:.0f}s); quest {} stage {}", Describe(s), phase, t.phaseTime, t.playing, Describe(q), q ? q->currentStage : 0);
				}
				if (g_observer.actSeconds > 0.0 && t.phaseTime >= g_observer.actSeconds && t.acted < g_observer.maxAttempts) {
					++t.acted;
					logger::warn("OBSERVER stop: scene {} phase {} for {:.0f}s{}", Describe(s), phase, t.phaseTime, g_settings.act ? "" : " (observe mode: not acting)");
					if (g_settings.act) {
						StopScene(s);
					}
					t.phaseTime = 0.0;
				}
			}
		}

		void StartTicker()
		{
			if (g_started.exchange(true)) {
				return;
			}
			std::thread([] {
				auto last = std::chrono::steady_clock::now();
				while (true) {
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(std::max(0.25, g_settings.tickSeconds) * 1000.0)));
					const auto now = std::chrono::steady_clock::now();
					const double dt = std::chrono::duration<double>(now - last).count();
					last = now;
					SKSE::GetTaskInterface()->AddTask([dt] { Tick(dt); });
				}
			}).detach();
		}

		void LoadConfig()
		{
			std::ifstream in(kConfigPath);
			if (!in) {
				logger::warn("{} not found: observer only, no rules", kConfigPath);
				return;
			}
			const json doc = json::parse(in, nullptr, false);
			if (doc.is_discarded() || !doc.is_object()) {
				logger::error("{} is not valid JSON: nothing loaded", kConfigPath);
				return;
			}
			if (const auto it = doc.find("settings"); it != doc.end() && it->is_object()) {
				g_settings.enabled = it->value("enabled", g_settings.enabled);
				g_settings.act = it->value("act", g_settings.act);
				g_settings.notify = it->value("notify", g_settings.notify);
				g_settings.debugLog = it->value("debugLog", g_settings.debugLog);
				g_settings.tickSeconds = it->value("tickSeconds", g_settings.tickSeconds);
				g_settings.settleSeconds = it->value("settleSeconds", g_settings.settleSeconds);
			}
			if (const auto it = doc.find("observer"); it != doc.end() && it->is_object()) {
				g_observer.enabled = it->value("enabled", g_observer.enabled);
				g_observer.phaseStallSeconds = it->value("phaseStallSeconds", g_observer.phaseStallSeconds);
				g_observer.actSeconds = it->value("actSeconds", g_observer.actSeconds);
				g_observer.maxAttempts = it->value("maxAttempts", g_observer.maxAttempts);
				if (const auto p = it->find("plugins"); p != it->end() && p->is_array()) {
					g_observer.plugins.clear();
					for (const auto& e : *p) {
						if (e.is_string()) {
							g_observer.plugins.push_back(e.get<std::string>());
						}
					}
				}
			}
			if (const auto it = doc.find("rules"); it != doc.end() && it->is_array()) {
				for (const auto& j : *it) {
					if (!j.is_object()) {
						continue;
					}
					Rule r;
					r.name = j.value("name", "unnamed rule");
					r.enabled = j.value("enabled", true);
					r.questKey = j.value("quest", "");
					r.sceneKey = j.value("scene", "");
					r.stageMin = j.value("stageMin", 0);
					r.stageMax = j.value("stageMax", 65535);
					r.maxSeconds = j.value("maxSeconds", 30.0);
					r.cooldown = j.value("cooldown", 20.0);
					r.maxAttempts = j.value("maxAttempts", 3);
					if (const auto a = j.find("actions"); a != j.end() && a->is_array()) {
						for (const auto& ja : *a) {
							Action act;
							const std::string type = ja.value("type", "");
							if (type == "stopScene") {
								act.type = Action::Type::StopScene;
							} else if (type == "setStage") {
								act.type = Action::Type::SetStage;
								act.stage = ja.value("stage", 0);
							} else if (type == "evaluatePackage") {
								act.type = Action::Type::EvaluatePackage;
								act.form = ja.value("actor", "");
							} else if (type == "notify") {
								act.type = Action::Type::Notify;
								act.text = ja.value("text", "");
							} else {
								logger::warn("rule '{}': unknown action type '{}' ignored", r.name, type);
								continue;
							}
							r.actions.push_back(std::move(act));
						}
					}
					g_rules.push_back(std::move(r));
				}
			}
		}

		void ResolveAll()
		{
			for (auto& r : g_rules) {
				r.quest = Resolve<RE::TESQuest>(r.questKey);
				if (!r.sceneKey.empty()) {
					r.scene = Resolve<RE::BGSScene>(r.sceneKey);
				}
				if (!r.quest || (!r.sceneKey.empty() && !r.scene)) {
					logger::warn("rule '{}' disabled: {} not found (plugin missing or wrong FormID)", r.name, !r.quest ? r.questKey : r.sceneKey);
					r.enabled = false;
					continue;
				}
				for (auto& a : r.actions) {
					if (a.type == Action::Type::EvaluatePackage) {
						a.actor = Resolve<RE::Actor>(a.form);
						if (!a.actor) {
							logger::warn("rule '{}': actor {} not found, that action is skipped", r.name, a.form);
						}
					}
				}
				logger::info("rule '{}' ready: quest {} stages {}-{}, {}", r.name, Describe(r.quest), r.stageMin, r.stageMax, r.scene ? "scene " + Describe(r.scene) : "stage only");
			}
		}

		void CollectScenes()
		{
			auto* dh = RE::TESDataHandler::GetSingleton();
			if (!dh) {
				return;
			}
			std::size_t total = 0;
			for (auto* scene : dh->GetFormArray<RE::BGSScene>()) {
				if (!scene) {
					continue;
				}
				const auto* file = scene->GetFile(0);
				if (!file) {
					continue;
				}
				for (const auto& p : g_observer.plugins) {
					if (IEquals(file->GetFilename(), p)) {
						g_tracks.push_back(Track{ scene });
						++total;
						break;
					}
				}
			}
			logger::info("observer: {} scenes of {} plugin(s) are timed", total, g_observer.plugins.size());
		}
	}

	void Init()
	{
		LoadConfig();
		if (g_settings.debugLog) {
			spdlog::set_level(spdlog::level::debug);
			spdlog::flush_on(spdlog::level::debug);
		}
		ResolveAll();
		if (g_observer.enabled) {
			CollectScenes();
		}
		logger::info("settings: enabled={} act={} tick={}s settle={}s", g_settings.enabled, g_settings.act, g_settings.tickSeconds, g_settings.settleSeconds);
		g_settle = g_settings.settleSeconds;
		g_ready = true;
		StartTicker();
	}

	void OnGameStart()
	{
		for (auto& r : g_rules) {
			r.stuck = 0.0;
			r.cool = 0.0;
			r.attempts = 0;
		}
		for (auto& t : g_tracks) {
			t = Track{ t.scene };
		}
		g_settle = g_settings.settleSeconds;
	}
}
