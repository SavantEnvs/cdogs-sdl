/* Fuzzes the autosave loader (src/autosave.c: AutosaveLoad -> JSON campaign/mission/player save
 * parsing), the file the game reads from the config dir at start-up. Reaches fix 9ade064d12. */
#define SDL_MAIN_HANDLED
#include "fuzz_common.h"
#include <autosave.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char path[4096];
	if (fuzz_write_tmp(data, size, "autosave", ".json", path, sizeof path) != 0)
		return 0;
	Autosave a;
	AutosaveInit(&a);
	AutosaveLoad(&a, path);
	(void)AutosaveGetLastCampaign(&a);
	AutosaveTerminate(&a);
	unlink(path);
	return 0;
}
