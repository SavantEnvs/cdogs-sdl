/* Fuzzes the campaign archive loader (map_new.c MapNewLoad -> map_archive.c MapNewLoadArchive) on a
 * "<name>.cdogscpn" directory, the format user-made campaigns are shared in. Besides the campaign
 * and mission JSON it loads each archive's custom data: bullets (bullet_class.c), guns
 * (weapon_class.c), ammo, pickups, map objects, particles, character classes and characters.
 * Reaches fix 9958625f90 (bullet hit sounds).
 *
 * Input: up to ten sections, each a 4-byte little-endian length followed by that many bytes, for
 * the files named in `files` below, in that order. A short final section is truncated; files for
 * missing sections are not created. They are written to a fresh directory under $TMPDIR.
 *
 * State kept across inputs: the game's built-in data classes (fuzz_game_init.h). The custom
 * classes an archive adds are cleared by CampaignSettingTerminateAll after each input, as the
 * game does before loading another campaign. */
#define SDL_MAIN_HANDLED
#include "fuzz_common.h"
#include "fuzz_game_init.h"

static const char *files[] = {
	"campaign.json", "missions.json", "characters.json", "bullets.json",
	"guns.json", "ammo.json", "pickups.json", "map_objects.json",
	"particles.json", "character_classes.json",
};
#define NFILES (sizeof files / sizeof files[0])

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;
	fuzz_game_init();
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const char *tmp = getenv("TMPDIR");
	if (tmp == NULL || *tmp == '\0')
		tmp = "/tmp";
	char base[4096];
	snprintf(base, sizeof base, "%s/archiveXXXXXX", tmp);
	if (mkdtemp(base) == NULL)
		return 0;
	char dir[4200];
	snprintf(dir, sizeof dir, "%s/fuzz.cdogscpn", base);
	if (mkdir(dir, 0700) != 0)
	{
		rmdir(base);
		return 0;
	}

	char path[4400];
	size_t off = 0;
	for (size_t i = 0; i < NFILES && off + 4 <= size; i++)
	{
		uint32_t len = (uint32_t)data[off] | (uint32_t)data[off + 1] << 8 |
		               (uint32_t)data[off + 2] << 16 | (uint32_t)data[off + 3] << 24;
		off += 4;
		if (len > size - off)
			len = (uint32_t)(size - off);
		snprintf(path, sizeof path, "%s/%s", dir, files[i]);
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

	for (size_t i = 0; i < NFILES; i++)
	{
		snprintf(path, sizeof path, "%s/%s", dir, files[i]);
		unlink(path);
	}
	rmdir(dir);
	rmdir(base);
	return 0;
}
