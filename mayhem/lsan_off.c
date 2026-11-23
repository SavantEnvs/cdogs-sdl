/* Linked into every fuzz and -standalone binary: turns LeakSanitizer off at build time. */
int __lsan_is_turned_off(void) { return 1; }
