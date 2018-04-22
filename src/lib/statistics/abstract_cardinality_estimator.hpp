#pragma once

#include <memory>
#include <vector>

namespace opossum {

class AbstractExpression2;
class AbstractLQPNode;

using Cardinality = float;

class AbstractCardinalityEstimator {
 public:
  virtual ~AbstractCardinalityEstimator() = default;

  /**
   * @param relations   Relations that the predicates work on.
   * @param predicates  Expressions of ExpressionType2 Column, Predicate and Logical.
   * @return            The estimated Cardinality of the evaluating `predicates` on `relations`
   */
  virtual Cardinality estimate(const std::vector<std::shared_ptr<AbstractLQPNode>>& relations,
                               const std::vector<std::shared_ptr<AbstractExpression2>>& predicates);
};

}  // namespace opossum
