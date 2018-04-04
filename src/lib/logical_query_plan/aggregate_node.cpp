#include "aggregate_node.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "expression/abstract_column_expression.hpp"
#include "expression/lqp_column_expression.hpp"
#include "types.hpp"
#include "utils/assert.hpp"

namespace opossum {

AggregateNode::AggregateNode(const std::vector<LQPColumnReference>& groupby_column_references,
                             const std::vector<PlanColumnDefinition>& aggregate_column_definitions)
    : AbstractLQPNode(LQPNodeType::Aggregate),
      _aggregate_column_definitions(aggregate_column_definitions),
      _groupby_column_references(groupby_column_references) {
#if IS_DEBUG
  for (const auto& aggregate_column_definition : _aggregate_column_definitions) {
    DebugAssert(aggregate_column_definition.expression->type == ExpressionType::Aggregate, "Aggregate expression must be an Aggregate.");
  }
#endif
}

std::shared_ptr<AbstractLQPNode> AggregateNode::_deep_copy_impl(
    const std::shared_ptr<AbstractLQPNode>& copied_left_input,
    const std::shared_ptr<AbstractLQPNode>& copied_right_input) const {
  Assert(left_input(), "Can't clone without input, need it to adapt column references");

  std::vector<PlanColumnDefinition> aggregate_column_definitions;
  aggregate_column_definitions.reserve(_aggregate_column_definitions.size());
  for (const auto& aggregate_column_definition : _aggregate_column_definitions) {
    auto expression_copy = aggregate_column_definition.expression->deep_copy();
    adapt_expression_to_different_lqp(*expression_copy, *left_input(), *copied_left_input);
    aggregate_column_definitions.emplace_back(expression_copy, aggregate_column_definition.alias);
  }

  std::vector<LQPColumnReference> groupby_column_references;
  groupby_column_references.reserve(_groupby_column_references.size());
  for (const auto& groupby_column_reference : _groupby_column_references) {
    groupby_column_references.emplace_back(
        adapt_column_reference_to_different_lqp(groupby_column_reference, *left_input(), *copied_left_input));
  }

  return AggregateNode::make(groupby_column_references, aggregate_column_definitions);
}

const std::vector<PlanColumnDefinition>& AggregateNode::aggregate_column_definitions() const {
  return _aggregate_column_definitions;
}

const std::vector<LQPColumnReference>& AggregateNode::groupby_column_references() const {
  return _groupby_column_references;
}

std::string AggregateNode::description() const {
  std::ostringstream s;

  s << "[Aggregate] ";

  for (auto aggregate_idx = size_t{0}; aggregate_idx < _aggregate_column_definitions.size(); ++aggregate_idx) {
    s << _aggregate_column_definitions[aggregate_idx].description();
    if (aggregate_idx + 1 < _aggregate_column_definitions.size()) {
      s << ", ";
    }
  }

  if (!_groupby_column_references.empty()) {
    s << " GROUP BY [";
    for (size_t group_by_idx = 0; group_by_idx < _groupby_column_references.size(); ++group_by_idx) {
      s << _groupby_column_references[group_by_idx].description();
      if (group_by_idx + 1 < _groupby_column_references.size()) {
        s << ", ";
      }
    }
    s << "]";
  }

  return s.str();
}

std::string AggregateNode::get_verbose_column_name(ColumnID column_id) const {
  DebugAssert(left_input(), "Need input to generate name");

  if (column_id < _groupby_column_references.size()) {
    return _groupby_column_references[column_id].description();
  }

  const auto aggregate_column_id = column_id - _groupby_column_references.size();
  DebugAssert(aggregate_column_id < _aggregate_expressions.size(), "ColumnID out of range");

  return _aggregate_column_definitions[aggregate_column_id].description();
}

void AggregateNode::_on_input_changed() {
  DebugAssert(!right_input(), "AggregateNode can't have a right input.");

  _output_column_names.reset();
}

const std::vector<std::string>& AggregateNode::output_column_names() const {
  Assert(left_input(), "Input not set, can't know output column names without it");
  if (!_output_column_names) {
    _update_output();
  }
  return *_output_column_names;
}

const std::vector<LQPColumnReference>& AggregateNode::output_column_references() const {
  if (!_output_column_references) {
    _update_output();
  }
  return *_output_column_references;
}

const std::vector<std::shared_ptr<AbstractExpression>>& AggregateNode::output_column_expressions() const {
  if (!_output_column_expressions) {
    _output_column_expressions->emplace();
    _output_column_expressions->reserve(output_column_count());
    for (const auto& groupby_column : _groupby_column_references) {
      _output_column_expressions->emplace_back(std::make_shared<LQPColumnExpression>(groupby_column));
    }
    for (const auto& aggregate_column_definition : _aggregate_column_definitions) {
      _output_column_expressions->emplace_back(aggregate_column_definition.expression);
    }
  }
  return *_output_column_expressions;
}

bool AggregateNode::shallow_equals(const AbstractLQPNode& rhs) const {
  Assert(rhs.type() == type(), "Can only compare nodes of the same type()");
  const auto& aggregate_node = static_cast<const AggregateNode&>(rhs);

  Assert(left_input() && rhs.left_input(), "Can't compare column references without inputs");
  return _equals(*left_input(), _aggregate_column_definitions, *rhs.left_input(), aggregate_node.aggregate_column_definitions()) &&
         _equals(*left_input(), _groupby_column_references, *rhs.left_input(),
                 aggregate_node.groupby_column_references());
}

void AggregateNode::_update_output() const {
  /**
   * The output (column names and output-to-input mapping) of this node gets cleared whenever an input changed and is
   * re-computed on request. This allows LQPs to be in temporary invalid states (e.g. no left input in Join) and thus
   * allows easier manipulation in the optimizer.
   */

  DebugAssert(!_output_column_references, "No need to update, _update_output() shouldn't get called.");
  DebugAssert(!_output_column_names, "No need to update, _update_output() shouldn't get called.");
  DebugAssert(left_input(), "Can't set output without input");

  _output_column_names.emplace();
  _output_column_names->reserve(_groupby_column_references.size() + _aggregate_column_definitions.size());

  _output_column_references.emplace();
  _output_column_references->reserve(_groupby_column_references.size() + _aggregate_column_definitions.size());

  /**
   * Set output column references and names.
   *
   * The Aggregate operator will put all GROUP BY columns in the output table at the beginning,
   * so we first handle those, and afterwards add the column information for the aggregate functions.
   */
  for (const auto& groupby_column_reference : _groupby_column_references) {
    _output_column_references->emplace_back(groupby_column_reference);

    const auto input_column_id = left_input()->get_output_column_id(groupby_column_reference);
    _output_column_names->emplace_back(left_input()->output_column_names()[input_column_id]);
  }

  auto column_id = static_cast<ColumnID>(_groupby_column_references.size());
  for (const auto& aggregate_column_definition : _aggregate_column_definitions) {
    DebugAssert(aggregate_column_definition.expression->type == ExpressionType::Aggregate, "Expression must be a aggregate.");

    std::string column_name;

    if (aggregate_column_definition.alias) {
      column_name = *aggregate_column_definition.alias;
    } else {
      /**
       * If the aggregate function has no alias defined in the query, we simply parse the expression back to a string.
       * SQL in the standard does not specify a name to be given.
       * This might result in multiple output columns with the same name, but we accept that.
       * Other DBs behave similarly (e.g. MySQL).
       */
      column_name = aggregate_column_definition.expression->description();
    }

    _output_column_names->emplace_back(column_name);
    _output_column_references->emplace_back(shared_from_this(), column_id);
    column_id++;
  }
}

}  // namespace opossum
