/* Fuzzes the config loader (src/cdogs/config_io.c: ConfigLoad -> config_json.c versioned JSON
 * config, or config_old.c for legacy files), which the game reads from the config dir. */
#define SDL_MAIN_HANDLED
#include "fuzz_common.h"
#include <config_io.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char path[4096];
	if (fuzz_write_tmp(data, size, "options", ".cnf", path, sizeof path) != 0)
		return 0;
	Config c = ConfigLoad(path);
	ConfigDestroy(&c);
	unlink(path);
	return 0;
}
