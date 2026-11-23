/* Fuzzes the campaign loader MapNewLoad (src/cdogs/map_new.c) on a single .cpn file: the classic
 * binary format (files.c LoadCampaignOld + mission_convert.c ConvertCampaignSetting, chosen by the
 * CAMPAIGN_MAGIC header) and the JSON format (MapNewLoadCampaignJSON / LoadMissions / character
 * loading). Campaign files are user-shared content. Reaches fixes 7c1d0fe131 and b56c88125a.
 *
 * State kept across inputs: the game's built-in data classes (fuzz_game_init.h). Each input loads
 * into a fresh CampaignSetting that CampaignSettingTerminateAll clears afterwards. */
#define SDL_MAIN_HANDLED
#include "fuzz_common.h"
#include "fuzz_game_init.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;
	fuzz_game_init();
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char path[4096];
	if (fuzz_write_tmp(data, size, "campaign", ".cpn", path, sizeof path) != 0)
		return 0;
	CampaignSetting c;
	CampaignSettingInit(&c);
	MapNewLoad(path, &c);
	CampaignSettingTerminateAll(&c);
	unlink(path);
	return 0;
}
