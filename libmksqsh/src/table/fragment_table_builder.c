#include <mksqsh_table.h>
#include <sqsh_data_set.h>

int
mksqsh__fragment_table_init(
		struct MksqshFragmentTable *table, FILE *content_output,
		FILE *lookup_output) {
	const size_t entry_size = sizeof(struct SqshDataFragment);
	return mksqsh__table_init(
			&table->table, entry_size, content_output, lookup_output);
}

int
mksqsh__fragment_table_write(
		struct MksqshFragmentTable *table, uint64_t start, uint32_t size) {
	struct SqshDataFragment fragment = {0};

	sqsh__data_fragment_start_set(&fragment, start);
	sqsh__data_fragment_size_info_set(&fragment, size);

	return mksqsh__table_add(&table->table, &fragment, sizeof(fragment));
}

int
mksqsh__fragment_table_flush(struct MksqshFragmentTable *table) {
	return mksqsh__table_flush(&table->table);
}

int
mksqsh__fragment_table_cleanup(struct MksqshFragmentTable *table) {
	return mksqsh__table_cleanup(&table->table);
}
