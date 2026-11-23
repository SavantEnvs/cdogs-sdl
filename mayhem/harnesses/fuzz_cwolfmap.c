/* Fuzzes the vendored Wolfenstein 3D map reader cwolfmap (src/cdogs/cwolfmap: CWLoad -> MAPHEAD,
 * GAMEMAPS Carmack/RLEW expansion, AUDIOHED/AUDIOT, VSWAP), which cdogs runs on any campaign
 * directory holding Wolf3D data files (map_new.c -> map_wolf.c).
 *
 * Input: up to five sections, each a 4-byte little-endian length followed by that many bytes, for
 * MAPHEAD, GAMEMAPS, AUDIOHED, AUDIOT and VSWAP (".WL1"). A short final section is truncated; files
 * for missing sections are not created. They are written to a fresh directory under $TMPDIR and
 * the directory is loaded and walked like map_wolf.c does, then removed.
 *
 * State kept across inputs: the emulated OPL (AdLib) chip that the game creates once at start-up
 * (map_wolf.c MapWolfInit -> CWAudioInit); CWAudioLoadHead resets all its registers per load. */
#define SDL_MAIN_HANDLED
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cwolfmap/audio.h>
#include <cwolfmap/cwolfmap.h>

static const char *names[] = {"MAPHEAD", "GAMEMAPS", "AUDIOHED", "AUDIOT", "VSWAP"};
#define NSECTIONS (sizeof names / sizeof names[0])

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;
	if (!CWAudioInit())
		abort();
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

	CWolfMap map;
	memset(&map, 0, sizeof map);
	if (CWLoad(&map, dir, 1) == 0)
	{
		unsigned sum = 0;
		for (int i = 0; i < map.nLevels; i++)
		{
			const CWLevel *level = &map.levels[i];
			for (int p = 0; p < NUM_PLANES; p++)
				for (int y = 0; y < level->header.height; y++)
					for (int x = 0; x < level->header.width; x++)
						sum += CWLevelGetCh(level, p, x, y);
		}
		const char *desc = CWGetDescription(&map, 1);
		if (desc != NULL)
			sum += (unsigned)strlen(desc);
		(void)sum;
	}
	CWFree(&map);

	for (size_t i = 0; i < NSECTIONS; i++)
	{
		snprintf(path, sizeof path, "%s/%s.WL1", dir, names[i]);
		unlink(path);
	}
	rmdir(dir);
	return 0;
}
