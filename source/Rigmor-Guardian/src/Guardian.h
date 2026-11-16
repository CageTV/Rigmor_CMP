#pragma once

namespace Guardian
{
	// kDataLoaded: read the rules file, look the forms up, start the one-second ticker
	void Init();
	// kNewGame / kPostLoadGame: forget every running timer and wait a few seconds before judging anything
	void OnGameStart();
}
