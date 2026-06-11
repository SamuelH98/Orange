#include "orange/desktop_entry.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	struct orange_desktop_entry entry;
	assert(orange_desktop_entry_load("../assets/desktop/pdf-document.desktop", &entry));
	assert(strcmp(entry.name, "PDF Documents") == 0);
	assert(strcmp(entry.icon, "notes") == 0);
	assert(strstr(entry.exec, "Documents") != NULL);

	struct orange_desktop_entry entries[4];
	size_t count = 0;
	assert(orange_desktop_entry_load_all("../assets/desktop", entries, 4, &count));
	assert(count >= 2);

	puts("desktop entry tests passed");
	return 0;
}
