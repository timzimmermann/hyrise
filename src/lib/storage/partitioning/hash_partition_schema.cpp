#include <storage/partitioning/hash_partition_schema.hpp>
#include <storage/table.hpp>

namespace opossum {

HashPartitionSchema::HashPartitionSchema(ColumnID column_id, HashFunction hash_function,
                                         PartitionID number_of_partitions)
    : _column_id(column_id), _hash_function(hash_function), _number_of_partitions(number_of_partitions) {
  _partitions.reserve(number_of_partitions);

  for (PartitionID index{0}; index < _number_of_partitions; ++index) {
    _partitions.emplace_back(std::make_shared<Partition>(index));
  }
}

std::string HashPartitionSchema::name() const { return "HashPartition"; }

PartitionSchemaType HashPartitionSchema::get_type() const { return PartitionSchemaType::Hash; }

void HashPartitionSchema::append(const std::vector<AllTypeVariant>& values) {
  AbstractPartitionSchema::append(values, get_matching_partition_for(values));
}

PartitionID HashPartitionSchema::get_matching_partition_for(const std::vector<AllTypeVariant>& values) const {
  DebugAssert(values.size() > static_cast<size_t>(_column_id), "Can not determine partition, too few values given");
  const auto& value = values[_column_id];
  return get_matching_partition_for(value);
}

PartitionID HashPartitionSchema::get_matching_partition_for(const AllTypeVariant& value) const {
  const auto hash = _hash_function(value);
  PartitionID matching_partition = static_cast<PartitionID>(hash % _number_of_partitions);
  return matching_partition;
}

std::map<RowID, PartitionID> HashPartitionSchema::get_mapping_to_partitions(std::shared_ptr<const Table> table) const {
  std::map<RowID, PartitionID> partition_mapping;
  for (ChunkID chunkID = ChunkID{0}; chunkID < table->chunk_count(); ++chunkID) {
    const auto source_chunk = table->get_chunk(chunkID);
    auto column_with_partitioning_values = source_chunk->get_column(get_column_id());
    for (uint32_t rowID = 0; rowID < source_chunk->size(); ++rowID) {
      partition_mapping[{chunkID, rowID}] = get_matching_partition_for((*column_with_partitioning_values)[rowID]);
    }
  }
  return partition_mapping;
}

std::vector<ChunkID> HashPartitionSchema::get_chunk_ids_to_exclude(PredicateCondition condition,
                                                                   const AllTypeVariant& value) const {
  PartitionID matching_partition = get_matching_partition_for(value);
  std::vector<ChunkID> chunk_ids_to_exclude;

  if (condition != PredicateCondition::Equals) {
    return chunk_ids_to_exclude;
  }

  for (PartitionID partition_id{0}; partition_id < partition_count(); partition_id++) {
    if (partition_id != matching_partition) {
      auto chunks_of_partition = get_partition(partition_id)->get_chunks();
      std::transform(chunks_of_partition.cbegin(), chunks_of_partition.cend(), std::back_inserter(chunk_ids_to_exclude),
                     [](auto chunk) { return chunk->id(); });
    }
  }
  return chunk_ids_to_exclude;
}

ColumnID HashPartitionSchema::get_column_id() const { return _column_id; }

}  // namespace opossum