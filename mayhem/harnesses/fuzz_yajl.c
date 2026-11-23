/* Fuzzes the vendored yajl JSON parser (src/cdogs/yajl) that cdogs uses for every data/save
 * file it reads through YAJLReadFile: tree parse, tree walk, and a stream re-encode via yajl_gen.
 * Reaches the yajl fixes for CVE-2022-24795 / CVE-2023-33460. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "yajl/api/yajl_gen.h"
#include "yajl/api/yajl_parse.h"
#include "yajl/api/yajl_tree.h"

static void walk(yajl_val v, int depth)
{
	if (v == NULL || depth > 64)
		return;
	if (YAJL_IS_OBJECT(v))
	{
		for (size_t i = 0; i < v->u.object.len; i++)
		{
			const char *path[] = {v->u.object.keys[i], NULL};
			(void)yajl_tree_get(v, path, yajl_t_any);
			walk(v->u.object.values[i], depth + 1);
		}
	}
	else if (YAJL_IS_ARRAY(v))
	{
		for (size_t i = 0; i < v->u.array.len; i++)
			walk(v->u.array.values[i], depth + 1);
	}
	else if (YAJL_IS_STRING(v))
	{
		(void)strlen(YAJL_GET_STRING(v));
	}
}

/* yajl_parse callbacks that replay the event stream into a yajl_gen (the reformat path). */
static int cb_null(void *ctx) { return yajl_gen_null((yajl_gen)ctx) == yajl_gen_status_ok; }
static int cb_bool(void *ctx, int b) { return yajl_gen_bool((yajl_gen)ctx, b) == yajl_gen_status_ok; }
static int cb_num(void *ctx, const char *s, size_t l) { return yajl_gen_number((yajl_gen)ctx, s, l) == yajl_gen_status_ok; }
static int cb_str(void *ctx, const unsigned char *s, size_t l) { return yajl_gen_string((yajl_gen)ctx, s, l) == yajl_gen_status_ok; }
static int cb_smap(void *ctx) { return yajl_gen_map_open((yajl_gen)ctx) == yajl_gen_status_ok; }
static int cb_emap(void *ctx) { return yajl_gen_map_close((yajl_gen)ctx) == yajl_gen_status_ok; }
static int cb_sarr(void *ctx) { return yajl_gen_array_open((yajl_gen)ctx) == yajl_gen_status_ok; }
static int cb_earr(void *ctx) { return yajl_gen_array_close((yajl_gen)ctx) == yajl_gen_status_ok; }
static const yajl_callbacks callbacks = {cb_null, cb_bool, NULL, NULL, cb_num, cb_str,
                                         cb_smap, cb_str, cb_emap, cb_sarr, cb_earr};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* yajl_tree_parse takes a NUL-terminated string (yajl_tree.h). */
	char *buf = malloc(size + 1);
	if (buf == NULL)
		return 0;
	memcpy(buf, data, size);
	buf[size] = '\0';
	char err[1024];
	yajl_val v = yajl_tree_parse(buf, err, sizeof err);
	walk(v, 0);
	yajl_tree_free(v);

	yajl_gen g = yajl_gen_alloc(NULL);
	yajl_gen_config(g, yajl_gen_beautify, 1);
	yajl_gen_config(g, yajl_gen_validate_utf8, 1);
	yajl_handle h = yajl_alloc(&callbacks, NULL, g);
	yajl_config(h, yajl_allow_comments, 1);
	if (yajl_parse(h, data, size) == yajl_status_ok)
		yajl_complete_parse(h);
	unsigned char *e = yajl_get_error(h, 1, data, size);
	yajl_free_error(h, e);
	yajl_free(h);
	yajl_gen_free(g);
	free(buf);
	return 0;
}
