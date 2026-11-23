/* Fuzzes the Wolfenstein 3D campaign conversion: map_new.c MapNewLoad on a directory holding Wolf3D
 * data files -> map_wolf.c MapWolfLoad, which parses them with cwolfmap and converts each level
 * into a cdogs campaign (tiles, entities, keys, sounds, music, pics). Goes past fuzz_cwolfmap's
 * parser into the 3.7k-line converter.
 *
 * Input: the fuzz_cwolfmap layout: up to five sections, each a 4-byte little-endian length then
 * that many bytes, for MAPHEAD, GAMEMAPS, AUDIOHED, AUDIOT and VSWAP (".WL1"), written to a fresh
 * directory under $TMPDIR and removed afterwards.
 *
 * State kept across inputs: the game's built-in data classes (fuzz_game_init.h) and the Wolf
 * module state the game sets up once (MapWolfInit: the emulated OPL chip, no default maps). Each
 * input loads into a fresh CampaignSetting that CampaignSettingTerminateAll clears afterwards. */
#define SDL_MAIN_HANDLED
#include "fuzz_common.h"
#include "fuzz_game_init.h"
#include <map_wolf.h>

static const char *names[] = {"MAPHEAD", "GAMEMAPS", "AUDIOHED", "AUDIOT", "VSWAP"};
#define NSECTIONS (sizeof names / sizeof names[0])

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;
	fuzz_game_init();
	MapWolfInit();
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const char *tmp = getenv("TMPDIR");
	if (tmp == NULL || *tmp == '\0')
		tmp = "/tmp";
	char dir[4096];
	snprintf(dir, sizeof dir, "%s/cwolfXXXXXX", tmp);
	if (mkdtemp(dir) == NULL)
		return 0;

	char path[4200];
	size_t off = 0;
	size_t written = 0;
	for (; written < NSECTIONS && off + 4 <= size; written++)
	{
		uint32_t len = (uint32_t)data[off] | (uint32_t)data[off + 1] << 8 |
		               (uint32_t)data[off + 2] << 16 | (uint32_t)data[off + 3] << 24;
		off += 4;
		if (len > size - off)
			len = (uint32_t)(size - off);
		snprintf(path, sizeof path, "%s/%s.WL1", dir, names[written]);
		FILE *f = fopen(path, "wb");
		if (f == NULL)
			break;
		fwrite(data + off, 1, len, f);
		fclose(f);
		off += len;
	}

	CampaignSetting c;
	CampaignSettingInit(&c);
	MapNewLoad(dir, &c);
	CampaignSettingTerminateAll(&c);

	for (size_t i = 0; i < NSECTIONS; i++)
	{
		snprintf(path, sizeof path, "%s/%s.WL1", dir, names[i]);
		unlink(path);
	}
	rmdir(dir);
	return 0;
}
