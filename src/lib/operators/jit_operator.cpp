#include "jit_operator.hpp"

namespace opossum {

JitOperator::JitOperator(const std::shared_ptr<const AbstractOperator> left) : AbstractReadOnlyOperator{left} {}

const std::string JitOperator::name() const { return "JitOperator"; }

const std::string JitOperator::description(DescriptionMode description_mode) const {
  std::stringstream desc;
  const auto separator = description_mode == DescriptionMode::MultiLine ? "\n" : " ";
  desc << "[JitOperator]" << separator;
  for (const auto& op : _operators) {
    desc << op->description() << separator;
  }
  return desc.str();
}

void JitOperator::add_jit_operator(const JitAbstractOperator::Ptr& op) { _operators.push_back(op); }

const JitReadTuple::Ptr JitOperator::_source() const {
  return std::dynamic_pointer_cast<JitReadTuple>(_operators.front());
}

const JitAbstractSink::Ptr JitOperator::_sink() const {
  return std::dynamic_pointer_cast<JitAbstractSink>(_operators.back());
}

std::shared_ptr<const Table> JitOperator::_on_execute() {
  // Connect operators to a chain
  for (auto it = _operators.begin(); it != _operators.end() && it + 1 != _operators.end(); ++it) {
    (*it)->set_next_operator(*(it + 1));
  }

  DebugAssert(_source(), "JitOperator does not have a valid source node.");
  DebugAssert(_sink(), "JitOperator does not have a valid sink node.");

  const auto& in_table = *input_left()->get_output();
  auto out_table = std::make_shared<opossum::Table>(in_table.max_chunk_size());

  JitRuntimeContext ctx;
  _source()->before_query(in_table, ctx);
  _sink()->before_query(*out_table, ctx);

  for (opossum::ChunkID chunk_id{0}; chunk_id < in_table.chunk_count(); ++chunk_id) {
    const auto& in_chunk = *in_table.get_chunk(chunk_id);

    ctx.chunk_size = in_chunk.size();
    ctx.chunk_offset = 0;

    _source()->before_chunk(in_table, in_chunk, ctx);
    _source()->execute(ctx);
    _sink()->after_chunk(*out_table, ctx);
  }

  return out_table;
}

}  // namespace opossum