#pragma once

#include "jit_abstract_sink.hpp"

namespace opossum {

/* Base class for all column writers.
 * We need this class, so we can store a number of JitColumnWriters with different template
 * specializations in a common data structure.
 */
class BaseJitColumnWriter {
 public:
  using Ptr = std::shared_ptr<const BaseJitColumnWriter>;

  virtual void write_value(JitRuntimeContext& ctx) const = 0;
};

/* JitColumnWriters provide a template-free interface to store tuple values in ValueColumns in the output table.
 *
 * All ValueColumns have BaseValueColumn as their template-free super class. This allows us to store shared pointers
 * to all output columns in vector in the runtime context.
 * We then use JitColumnWriter instances to access these columns. JitColumnWriters are templated with the
 * type of ValueColumn they are accessing. They are initialized with an output_index and a tuple value.
 * When requested to store a value, they will access the column from the runtime context corresponding to their
 * output_index and copy the value from their JitTupleValue.
 *
 * All column writers have a common template-free base class. That allows us to store the column writers in a
 * vector as well and access all types of columns with a single interface.
 */
template <typename ValueColumn, typename DataType, bool Nullable>
class JitColumnWriter : public BaseJitColumnWriter {
 public:
  JitColumnWriter(const size_t output_index, const JitTupleValue& tuple_value)
      : _output_index{output_index}, _tuple_value{tuple_value} {}

  void write_value(JitRuntimeContext& ctx) const {
    const auto value = _tuple_value.materialize(ctx).template get<DataType>();
    _value_column(ctx).values().push_back(value);
    // clang-format off
    if constexpr (Nullable) {
      const auto is_null = _tuple_value.materialize(ctx).is_null();
      _value_column(ctx).null_values().push_back(is_null);
    }
    // clang-format on
  }

 private:
  ValueColumn& _value_column(JitRuntimeContext& ctx) const {
    return *std::static_pointer_cast<ValueColumn>(ctx.outputs[_output_index]);
  }

  const size_t _output_index;
  const JitTupleValue _tuple_value;
};

struct JitOutputColumn {
  std::string column_name;
  JitTupleValue tuple_value;
};

/* JitWriteTuple must be the last operator in any chain of jit operators.
 * It is responsible for
 * 1) adding column definitions to the output table
 * 2) appending the current tuple to the current output chunk
 * 3) creating a new output chunks and adding output chunks to the output table
 */
class JitWriteTuple : public JitAbstractSink {
 public:
  std::string description() const final;

  void before_query(Table& out_table, JitRuntimeContext& ctx) final;
  void after_chunk(Table& out_table, JitRuntimeContext& ctx) const final;

  void add_output_column(const std::string& column_name, const JitTupleValue& tuple_value);

 private:
  void next(JitRuntimeContext& ctx) const final;

  void _create_output_chunk(JitRuntimeContext& ctx) const;

  std::vector<JitOutputColumn> _output_columns;
  std::vector<BaseJitColumnWriter::Ptr> _column_writers;
};

}  // namespace opossum