#include <cextras/endian.h>
#include <mksqsh_table.h>
#include <sqsh_data_set.h>

#define ENTRY_SIZE sizeof(uint32_t)

int
mksqsh__id_table_init(
		struct MksqshIdTable *table, FILE *content_output,
		FILE *lookup_output) {
	return mksqsh__table_init(
			&table->table, ENTRY_SIZE, content_output, lookup_output);
}

int
mksqsh__id_table_add(struct MksqshIdTable *table, uint32_t id) {
	uint32_t id_le = CX_CPU_2_LE32(id);

	return mksqsh__table_add(&table->table, &id_le, ENTRY_SIZE);
}

int
mksqsh__id_table_flush(struct MksqshIdTable *table) {
	return mksqsh__table_flush(&table->table);
}

int
mksqsh__id_table_cleanup(struct MksqshIdTable *table) {
	return mksqsh__table_cleanup(&table->table);
}
