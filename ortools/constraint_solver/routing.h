// Copyright 2010-2017 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The vehicle routing library lets one model and solve generic vehicle routing
// problems ranging from the Traveling Salesman Problem to more complex
// problems such as the Capacitated Vehicle Routing Problem with Time Windows.
//
// The objective of a vehicle routing problem is to build routes covering a set
// of nodes minimizing the overall cost of the routes (usually proportional to
// the sum of the lengths of each segment of the routes) while respecting some
// problem-specific constraints (such as the length of a route). A route is
// equivalent to a path connecting nodes, starting/ending at specific
// starting/ending nodes.
//
// The term "vehicle routing" is historical and the category of problems solved
// is not limited to the routing of vehicles: any problem involving finding
// routes visiting a given number of nodes optimally falls under this category
// of problems, such as finding the optimal sequence in a playlist.
// The literature around vehicle routing problems is extremely dense but one
// can find some basic introductions in the following links:
// - http://en.wikipedia.org/wiki/Travelling_salesman_problem
// - http://www.tsp.gatech.edu/history/index.html
// - http://en.wikipedia.org/wiki/Vehicle_routing_problem
//
// The vehicle routing library is a vertical layer above the constraint
// programming library (ortools/constraint_programming:cp).
// One has access to all underlying constrained variables of the vehicle
// routing model which can therefore be enriched by adding any constraint
// available in the constraint programming library.
//
// There are two sets of variables available:
// - path variables:
//   * "next(i)" variables representing the immediate successor of the node
//     corresponding to i; use IndexToNode() to get the node corresponding to
//     a "next" variable value; note that node indices are strongly typed
//     integers (cf. ortools/base/int_type.h);
//   * "vehicle(i)" variables representing the vehicle route to which the
//     node corresponding to i belongs;
//   * "active(i)" boolean variables, true if the node corresponding to i is
//     visited and false if not; this can be false when nodes are either
//     optional or part of a disjunction;
//   * The following relationships hold for all i:
//      active(i) == 0 <=> next(i) == i <=> vehicle(i) == -1,
//      next(i) == j => vehicle(j) == vehicle(i).
// - dimension variables, used when one is accumulating quantities along routes,
//   such as weight or volume carried, distance or time:
//   * "cumul(i,d)" variables representing the quantity of dimension d when
//     arriving at the node corresponding to i;
//   * "transit(i,d)" variables representing the quantity of dimension d added
//     after visiting the node corresponding to i.
//   * The following relationship holds for all (i,d):
//       next(i) == j => cumul(j,d) == cumul(i,d) + transit(i,d).
// Solving the vehicle routing problems is mainly done using approximate methods
// (namely local search,
// cf. http://en.wikipedia.org/wiki/Local_search_(optimization) ), potentially
// combined with exact techniques based on dynamic programming and exhaustive
// tree search.
// TODO(user): Add a section on costs (vehicle arc costs, span costs,
//                disjunctions costs).
//
// Advanced tips: Flags are available to tune the search used to solve routing
// problems. Here is a quick overview of the ones one might want to modify:
// - Limiting the search for solutions:
//   * routing_solution_limit (default: kint64max): stop the search after
//     finding 'routing_solution_limit' improving solutions;
//   * routing_time_limit (default: kint64max): stop the search after
//     'routing_time_limit' milliseconds;
// - Customizing search:
//   * routing_first_solution (default: select the first node with an unbound
//     successor and connect it to the first available node): selects the
//     heuristic to build a first solution which will then be improved by local
//     search; possible values are GlobalCheapestArc (iteratively connect two
//     nodes which produce the cheapest route segment), LocalCheapestArc (select
//     the first node with an unbound successor and connect it to the node
//     which produces the cheapest route segment), PathCheapestArc (starting
//     from a route "start" node, connect it to the node which produces the
//     cheapest route segment, then extend the route by iterating on the last
//     node added to the route).
//   * Local search neighborhoods:
//     - routing_no_lns (default: false): forbids the use of Large Neighborhood
//       Search (LNS); LNS can find good solutions but is usually very slow.
//       Refer to the description of PATHLNS in the LocalSearchOperators enum
//       in constraint_solver.h for more information.
//     - routing_no_tsp (default: true): forbids the use of exact methods to
//       solve "sub"-traveling salesman problems (TSPs) of the current model
//       (such as sub-parts of a route, or one route in a multiple route
//       problem). Uses dynamic programming to solve such TSPs with a maximum
//       size (in number of nodes) up to cp_local_search_tsp_opt_size (flag with
//       a default value of 13 nodes). It is not activated by default because it
//       can slow down the search.
//   * Meta-heuristics: used to guide the search out of local minima found by
//     local search. Note that, in general, a search with metaheuristics
//     activated never stops, therefore one must specify a search limit.
//     Several types of metaheuristics are provided:
//     - routing_guided_local_search (default: false): activates guided local
//       search (cf. http://en.wikipedia.org/wiki/Guided_Local_Search);
//       this is generally the most efficient metaheuristic for vehicle
//       routing;
//     - routing_simulated_annealing (default: false): activates simulated
//       annealing (cf. http://en.wikipedia.org/wiki/Simulated_annealing);
//     - routing_tabu_search (default: false): activates tabu search (cf.
//       http://en.wikipedia.org/wiki/Tabu_search).
//
// Code sample:
// Here is a simple example solving a traveling salesman problem given a cost
// function callback (returns the cost of a route segment):
//
// - Define a custom distance/cost function from an index to another; in this
//   example just returns the sum of the indices:
//
//     int64 MyDistance(int64 from, int64 to) {
//       return from + to;
//     }
//
// - Create a routing model for a given problem size (int number of nodes) and
//   number of routes (here, 1):
//
//     RoutingIndexManager manager(...number of nodes..., 1);
//     RoutingModel routing(manager);
//
// - Set the cost function by registering an std::function<int64(int64, int64)>
// in the model and passing its index as the vehicle cost.
//
//    const int cost = routing.RegisterTransitCallback(MyDistance);
//    routing.SetArcCostEvaluatorOfAllVehicles(cost);
//
// - Find a solution using Solve(), returns a solution if any (owned by
//   routing):
//
//    const Assignment* solution = routing.Solve();
//    CHECK(solution != nullptr);
//
// - Inspect the solution cost and route (only one route here):
//
//    LOG(INFO) << "Cost " << solution->ObjectiveValue();
//    const int route_number = 0;
//    for (int64 node = routing.Start(route_number);
//         !routing.IsEnd(node);
//         node = solution->Value(routing.NextVar(node))) {
//      LOG(INFO) << routing.IndexToNode(node);
//    }
//
//
// Keywords: Vehicle Routing, Traveling Salesman Problem, TSP, VRP, CVRPTW, PDP.

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_

#include <stddef.h>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/adjustable_priority_queue.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/hash.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/graph/graph.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/theta_tree.h"
#include "ortools/util/range_query_function.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {

class IntVarFilteredDecisionBuilder;
class LocalSearchOperator;
class RoutingDimension;
#ifndef SWIG
using util::ReverseArcListGraph;
class SweepArranger;
#endif
struct SweepIndex;

class RoutingModel {
 public:
  // Status of the search.
  enum Status {
    // Problem not solved yet (before calling RoutingModel::Solve()).
    ROUTING_NOT_SOLVED,
    // Problem solved successfully after calling RoutingModel::Solve().
    ROUTING_SUCCESS,
    // No solution found to the problem after calling RoutingModel::Solve().
    ROUTING_FAIL,
    // Time limit reached before finding a solution with RoutingModel::Solve().
    ROUTING_FAIL_TIMEOUT,
    // Model, model parameters or flags are not valid.
    ROUTING_INVALID
  };

#ifndef SWIG
  // Types of precedence policy applied to pickup and delivery pairs.
  enum class PickupAndDeliveryPolicy {
    // Any precedence is accepted.
    ANY,
    // Deliveries must be performed in reverse order of pickups.
    LIFO,
    // Deliveries must be performed in the same order as pickups.
    FIFO
  };
#endif  // SWIG
  typedef RoutingCostClassIndex CostClassIndex;
  typedef RoutingDimensionIndex DimensionIndex;
  typedef RoutingDisjunctionIndex DisjunctionIndex;
  typedef RoutingVehicleClassIndex VehicleClassIndex;
  typedef RoutingTransitCallback1 TransitCallback1;
  typedef RoutingTransitCallback2 TransitCallback2;

// TODO(user): Remove all SWIG guards by adding the @ignore in .i.
#if !defined(SWIG)
  typedef RoutingIndexPair IndexPair;
  typedef RoutingIndexPairs IndexPairs;
#endif  // SWIG

#if !defined(SWIG)
  // What follows is relevant for models with time/state dependent transits.
  // Such transits, say from node A to node B, are functions f: int64->int64
  // of the cumuls of a dimension. The user is free to implement the abstract
  // RangeIntToIntFunction interface, but it is expected that the implementation
  // of each method is quite fast. For performance-related reasons,
  // StateDependentTransit keeps an additional pointer to a
  // RangeMinMaxIndexFunction, with similar functionality to
  // RangeIntToIntFunction, for g(x) = f(x)+x, where f is the transit from A to
  // B. In most situations the best solutions are problem-specific, but in case
  // of doubt the user may use the MakeStateDependentTransit function from the
  // routing library, which works out-of-the-box, with very good running time,
  // but memory inefficient in some situations.
  struct StateDependentTransit {
    RangeIntToIntFunction* transit;                   // f(x)
    RangeMinMaxIndexFunction* transit_plus_identity;  // g(x) = f(x) + x
  };
  typedef std::function<StateDependentTransit(int64, int64)>
      VariableIndexEvaluator2;
#endif  // SWIG

#if !defined(SWIG)
  struct CostClass {
    // Index of the arc cost evaluator, registered in the RoutingModel class.
    int evaluator_index = 0;

    // SUBTLE:
    // The vehicle's fixed cost is skipped on purpose here, because we
    // can afford to do so:
    // - We don't really care about creating "strict" equivalence classes;
    //   all we care about is to:
    //   1) compress the space of cost callbacks so that
    //      we can cache them more efficiently.
    //   2) have a smaller IntVar domain thanks to using a "cost class var"
    //      instead of the vehicle var, so that we reduce the search space.
    //   Both of these are an incentive for *fewer* cost classes. Ignoring
    //   the fixed costs can only be good in that regard.
    // - The fixed costs are only needed when evaluating the cost of the
    //   first arc of the route, in which case we know the vehicle, since we
    //   have the route's start node.

    // Only dimensions that have non-zero cost evaluator and a non-zero cost
    // coefficient (in this cost class) are listed here. Since we only need
    // their transit evaluator (the raw version that takes var index, not Node
    // Index) and their span cost coefficient, we just store those.
    // This is sorted by the natural operator < (and *not* by DimensionIndex).
    struct DimensionCost {
      int64 transit_evaluator_class;
      int64 cost_coefficient;
      const RoutingDimension* dimension;
      bool operator<(const DimensionCost& cost) const {
        if (transit_evaluator_class != cost.transit_evaluator_class) {
          return transit_evaluator_class < cost.transit_evaluator_class;
        }
        return cost_coefficient < cost.cost_coefficient;
      }
    };
    std::vector<DimensionCost>
        dimension_transit_evaluator_class_and_cost_coefficient;

    explicit CostClass(int evaluator_index)
        : evaluator_index(evaluator_index) {}

    // Comparator for STL containers and algorithms.
    static bool LessThan(const CostClass& a, const CostClass& b) {
      if (a.evaluator_index != b.evaluator_index) {
        return a.evaluator_index < b.evaluator_index;
      }
      return a.dimension_transit_evaluator_class_and_cost_coefficient <
             b.dimension_transit_evaluator_class_and_cost_coefficient;
    }
  };

  struct VehicleClass {
    // The cost class of the vehicle.
    CostClassIndex cost_class_index;
    // Contrarily to CostClass, here we need strict equivalence.
    int64 fixed_cost;
    // Vehicle start and end equivalence classes. Currently if two vehicles have
    // different start/end nodes which are "physically" located at the same
    // place, these two vehicles will be considered as non-equivalent unless the
    // two indices are in the same class.
    // TODO(user): Find equivalent start/end nodes wrt dimensions and
    // callbacks.
    int start_equivalence_class;
    int end_equivalence_class;
    // Bounds of cumul variables at start and end vehicle nodes.
    // dimension_{start,end}_cumuls_{min,max}[d] is the bound for dimension d.
    gtl::ITIVector<DimensionIndex, int64> dimension_start_cumuls_min;
    gtl::ITIVector<DimensionIndex, int64> dimension_start_cumuls_max;
    gtl::ITIVector<DimensionIndex, int64> dimension_end_cumuls_min;
    gtl::ITIVector<DimensionIndex, int64> dimension_end_cumuls_max;
    gtl::ITIVector<DimensionIndex, int64> dimension_capacities;
    // dimension_evaluators[d]->Run(from, to) is the transit value of arc
    // from->to for a dimension d.
    gtl::ITIVector<DimensionIndex, int64> dimension_evaluator_classes;
    // Fingerprint of unvisitable non-start/end nodes.
    uint64 unvisitable_nodes_fprint;

    // Comparator for STL containers and algorithms.
    static bool LessThan(const VehicleClass& a, const VehicleClass& b);
  };
#endif  // defined(SWIG)

  // Constant used to express a hard constraint instead of a soft penalty.
  static const int64 kNoPenalty;

  // Constant used to express the "no disjunction" index, returned when a node
  // does not appear in any disjunction.
  static const DisjunctionIndex kNoDisjunction;

  // Constant used to express the "no dimension" index, returned when a
  // dimension name does not correspond to an actual dimension.
  static const DimensionIndex kNoDimension;

  // Constructor taking an index manager. The version which does not take
  // RoutingModelParameters is equivalent to passing
  // DefaultRoutingModelParameters().
  explicit RoutingModel(const RoutingIndexManager& index_manager);
  RoutingModel(const RoutingIndexManager& index_manager,
               const RoutingModelParameters& parameters);
  ~RoutingModel();

  // Registers 'callback' and returns its index.
#ifndef SWIG
  int RegisterTransitCallback(TransitCallback1 callback);
#endif
  int RegisterTransitCallback(TransitCallback2 callback);
  int RegisterStateDependentTransitCallback(VariableIndexEvaluator2 callback);
  const TransitCallback2& TransitCallback(int callback_index) const {
    CHECK_LT(callback_index, transit_evaluators_.size());
    return transit_evaluators_[callback_index];
  }
  const TransitCallback1& UnaryTransitCallbackOrNull(int callback_index) const {
    CHECK_LT(callback_index, unary_transit_evaluators_.size());
    return unary_transit_evaluators_[callback_index];
  }
  const VariableIndexEvaluator2& StateDependentTransitCallback(
      int callback_index) const {
    CHECK_LT(callback_index, state_dependent_transit_evaluators_.size());
    return state_dependent_transit_evaluators_[callback_index];
  }

  // Model creation

  // Methods to add dimensions to routes; dimensions represent quantities
  // accumulated at nodes along the routes. They represent quantities such as
  // weights or volumes carried along the route, or distance or times.
  // Quantities at a node are represented by "cumul" variables and the increase
  // or decrease of quantities between nodes are represented by "transit"
  // variables. These variables are linked as follows:
  // if j == next(i), cumul(j) = cumul(i) + transit(i) + slack(i)
  // where slack is a positive slack variable (can represent waiting times for
  // a time dimension).
  // Setting the value of fix_start_cumul_to_zero to true will force the "cumul"
  // variable of the start node of all vehicles to be equal to 0.

  // Creates a dimension where the transit variable is constrained to be
  // equal to evaluator(i, next(i)); 'slack_max' is the upper bound of the
  // slack variable and 'capacity' is the upper bound of the cumul variables.
  // 'name' is the name used to reference the dimension; this name is used to
  // get cumul and transit variables from the routing model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  // Takes ownership of the callback 'evaluator'.
  bool AddDimension(int evaluator_index, int64 slack_max, int64 capacity,
                    bool fix_start_cumul_to_zero, const std::string& name);
  bool AddDimensionWithVehicleTransits(
      const std::vector<int>& evaluator_indices, int64 slack_max,
      int64 capacity, bool fix_start_cumul_to_zero, const std::string& name);
  bool AddDimensionWithVehicleCapacity(int evaluator_index, int64 slack_max,
                                       std::vector<int64> vehicle_capacities,
                                       bool fix_start_cumul_to_zero,
                                       const std::string& name);
  bool AddDimensionWithVehicleTransitAndCapacity(
      const std::vector<int>& evaluator_indices, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'value'; 'capacity' is the upper bound of the cumul variables.
  // 'name' is the name used to reference the dimension; this name is used to
  // get cumul and transit variables from the routing model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddConstantDimensionWithSlack(int64 value, int64 capacity,
                                     int64 slack_max,
                                     bool fix_start_cumul_to_zero,
                                     const std::string& name);
  bool AddConstantDimension(int64 value, int64 capacity,
                            bool fix_start_cumul_to_zero,
                            const std::string& name) {
    return AddConstantDimensionWithSlack(value, capacity, 0,
                                         fix_start_cumul_to_zero, name);
  }
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddVectorDimension(std::vector<int64> values, int64 capacity,
                          bool fix_start_cumul_to_zero,
                          const std::string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i][next(i)]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddMatrixDimension(std::vector<std::vector<int64>> values,
                          int64 capacity, bool fix_start_cumul_to_zero,
                          const std::string& name);
  // Creates a dimension with transits depending on the cumuls of another
  // dimension. 'pure_transits' are the per-vehicle fixed transits as above.
  // 'dependent_transits' is a vector containing for each vehicle an index to a
  // registered state dependent transit callback. 'base_dimension' indicates the
  // dimension from which the cumul variable is taken. If 'base_dimension' is
  // nullptr, then the newly created dimension is self-based.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      const std::vector<int>& pure_transits,
      const std::vector<int>& dependent_transits,
      const RoutingDimension* base_dimension, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name) {
    return AddDimensionDependentDimensionWithVehicleCapacityInternal(
        pure_transits, dependent_transits, base_dimension, slack_max,
        std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
  }
  // As above, but pure_transits are taken to be zero evaluators.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      const std::vector<int>& transits, const RoutingDimension* base_dimension,
      int64 slack_max, std::vector<int64> vehicle_capacities,
      bool fix_start_cumul_to_zero, const std::string& name);
  // Homogeneous versions of the functions above.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      int transit, const RoutingDimension* base_dimension, int64 slack_max,
      int64 vehicle_capacity, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      int pure_transit, int dependent_transit,
      const RoutingDimension* base_dimension, int64 slack_max,
      int64 vehicle_capacity, bool fix_start_cumul_to_zero,
      const std::string& name);

  // Creates a cached StateDependentTransit from an std::function.
  static RoutingModel::StateDependentTransit MakeStateDependentTransit(
      const std::function<int64(int64)>& f, int64 domain_start,
      int64 domain_end);

  // Outputs the names of all dimensions added to the routing engine.
  // TODO(user): rename.
  std::vector<::std::string> GetAllDimensionNames() const;
  // Returns true if a dimension exists for a given dimension name.
  bool HasDimension(const std::string& dimension_name) const;
  // Returns a dimension from its name. Dies if the dimension does not exist.
  const RoutingDimension& GetDimensionOrDie(
      const std::string& dimension_name) const;
  // Returns a dimension from its name. Returns nullptr if the dimension does
  // not exist.
  RoutingDimension* GetMutableDimension(
      const std::string& dimension_name) const;
  // Set the given dimension as "primary constrained". As of August 2013, this
  // is only used by ArcIsMoreConstrainedThanArc().
  // "dimension" must be the name of an existing dimension, or be empty, in
  // which case there will not be a primary dimension after this call.
  void SetPrimaryConstrainedDimension(const std::string& dimension_name) {
    DCHECK(dimension_name.empty() || HasDimension(dimension_name));
    primary_constrained_dimension_ = dimension_name;
  }
  // Get the primary constrained dimension, or an empty std::string if it is
  // unset.
  const std::string& GetPrimaryConstrainedDimension() const {
    return primary_constrained_dimension_;
  }
  // Adds a disjunction constraint on the indices: exactly 'max_cardinality' of
  // the indices are active. Start and end indices of any vehicle cannot be part
  // of a disjunction.
  // If a penalty is given, at most 'max_cardinality' of the indices can be
  // active, and if less are active, 'penalty' is payed per inactive index.
  // This is equivalent to adding the constraint:
  //     p + Sum(i)active[i] == max_cardinality
  // where p is an integer variable, and the following cost to the cost
  // function:
  //     p * penalty.
  // 'penalty' must be positive to make the disjunction optional; a negative
  // penalty will force 'max_cardinality' indices of the disjunction to be
  // performed, and therefore p == 0.
  // Note: passing a vector with a single index will model an optional index
  // with a penalty cost if it is not visited.
  DisjunctionIndex AddDisjunction(const std::vector<int64>& indices,
                                  int64 penalty = kNoPenalty,
                                  int64 max_cardinality = 1);
  // Returns the indices of the disjunctions to which an index belongs.
  const std::vector<DisjunctionIndex>& GetDisjunctionIndices(
      int64 index) const {
    return index_to_disjunctions_[index];
  }
  // Calls f for each variable index of indices in the same disjunctions as the
  // node corresponding to the variable index 'index'; only disjunctions of
  // cardinality 'cardinality' are considered.
  template <typename F>
  void ForEachNodeInDisjunctionWithMaxCardinalityFromIndex(
      int64 index, int64 max_cardinality, F f) const {
    for (const DisjunctionIndex disjunction : GetDisjunctionIndices(index)) {
      if (disjunctions_[disjunction].value.max_cardinality == max_cardinality) {
        for (const int64 d_index : disjunctions_[disjunction].indices) {
          f(d_index);
        }
      }
    }
  }
#if !defined(SWIGPYTHON)
  // Returns the variable indices of the nodes in the disjunction of index
  // 'index'.
  const std::vector<int64>& GetDisjunctionIndices(
      DisjunctionIndex index) const {
    return disjunctions_[index].indices;
  }
#endif  // !defined(SWIGPYTHON)
  // Returns the penalty of the node disjunction of index 'index'.
  int64 GetDisjunctionPenalty(DisjunctionIndex index) const {
    return disjunctions_[index].value.penalty;
  }
  // Returns the maximum number of possible active nodes of the node disjunction
  // of index 'index'.
  int64 GetDisjunctionMaxCardinality(DisjunctionIndex index) const {
    return disjunctions_[index].value.max_cardinality;
  }
  // Returns the number of node disjunctions in the model.
  int GetNumberOfDisjunctions() const { return disjunctions_.size(); }
  // Returns the list of all perfect binary disjunctions, as pairs of variable
  // indices: a disjunction is "perfect" when its variables do not appear in
  // any other disjunction. Each pair is sorted (lowest variable index first),
  // and the output vector is also sorted (lowest pairs first).
  std::vector<std::pair<int64, int64>> GetPerfectBinaryDisjunctions() const;
  // SPECIAL: Makes the solver ignore all the disjunctions whose active
  // variables are all trivially zero (i.e. Max() == 0), by setting their
  // max_cardinality to 0.
  // This can be useful when using the BaseBinaryDisjunctionNeighborhood
  // operators, in the context of arc-based routing.
  void IgnoreDisjunctionsAlreadyForcedToZero();

  // Adds a soft contraint to force a set of variable indices to be on the same
  // vehicle. If all nodes are not on the same vehicle, each extra vehicle used
  // adds 'cost' to the cost function.
  void AddSoftSameVehicleConstraint(const std::vector<int64>& indices,
                                    int64 cost);

  // Notifies that index1 and index2 form a pair of nodes which should belong
  // to the same route. This methods helps the search find better solutions,
  // especially in the local search phase.
  // It should be called each time you have an equality constraint linking
  // the vehicle variables of two node (including for instance pickup and
  // delivery problems):
  //     Solver* const solver = routing.solver();
  //     int64 index1 = manager.NodeToIndex(node1);
  //     int64 index2 = manager.NodeToIndex(node2);
  //     solver->AddConstraint(solver->MakeEquality(
  //         routing.VehicleVar(index1),
  //         routing.VehicleVar(index2)));
  //     routing.AddPickupAndDelivery(index1, index2);
  //
  // TODO(user): Remove this when model introspection detects linked nodes.
  void AddPickupAndDelivery(int64 pickup, int64 delivery);
  // Same as AddPickupAndDelivery but notifying that the performed node from
  // the disjunction of index 'pickup_disjunction' is on the same route as the
  // performed node from the disjunction of index 'delivery_disjunction'.
  void AddPickupAndDeliverySets(DisjunctionIndex pickup_disjunction,
                                DisjunctionIndex delivery_disjunction);
  // clang-format off
  // Returns pairs for which the node is a pickup; the first element of each
  // pair is the index in the pickup and delivery pairs list in which the pickup
  // appears, the second element is its index in the pickups list.
  const std::vector<std::pair<int, int> >&
  GetPickupIndexPairs(int64 node_index) const;
  // Same as above for deliveries.
  const std::vector<std::pair<int, int> >&
      GetDeliveryIndexPairs(int64 node_index) const;
  // clang-format on

#ifndef SWIG
  // Returns pickup and delivery pairs currently in the model.
  const IndexPairs& GetPickupAndDeliveryPairs() const {
    return pickup_delivery_pairs_;
  }
  const std::vector<std::pair<DisjunctionIndex, DisjunctionIndex>>&
  GetPickupAndDeliveryDisjunctions() const {
    return pickup_delivery_disjunctions_;
  }
  // Returns the number of non-start/end nodes which do not appear in a
  // pickup/delivery pair.
  int GetNumOfSingletonNodes() const;
  void SetPickupAndDeliveryPolicyOfVehicle(PickupAndDeliveryPolicy policy,
                                           int vehicle) {
    vehicle_pickup_delivery_policy_[vehicle] = policy;
  }
  PickupAndDeliveryPolicy GetPickupAndDeliveryPolicyOfVehicle(
      int vehicle) const {
    return vehicle_pickup_delivery_policy_[vehicle];
  }
#endif  // SWIG
  // Set the node visit types and incompatibilities between the types.
  // Two nodes with incompatible types cannot be visited by the same vehicle.
  // TODO(user): Forbid incompatible types from being on the same route at
  // the same time (instead of at any time).
  // The visit type of a node must be positive.
  // TODO(user): Support multiple visit types per node?
  void SetVisitType(int64 index, int type);
  int GetVisitType(int64 index) const;
  void AddTypeIncompatibility(int type1, int type2);
  // Returns visit types incompatible to a given type.
  const std::unordered_set<int>& GetTypeIncompatibilities(int type) const;
  int GetNumberOfVisitTypes() const { return num_visit_types_; }
  // Get the "unperformed" penalty of a node. This is only well defined if the
  // node is only part of a single Disjunction involving only itself, and that
  // disjunction has a penalty. In all other cases, including forced active
  // nodes, this returns 0.
  int64 UnperformedPenalty(int64 var_index) const;
  // Same as above except that it returns default_value instead of 0 when
  // penalty is not well defined (default value is passed as first argument to
  // simplify the usage of the method in a callback).
  int64 UnperformedPenaltyOrValue(int64 default_value, int64 var_index) const;
  // Returns the variable index of the first starting or ending node of all
  // routes. If all routes start  and end at the same node (single depot), this
  // is the node returned.
  int64 GetDepot() const;

  // Sets the cost function of the model such that the cost of a segment of a
  // route between node 'from' and 'to' is evaluator(from, to), whatever the
  // route or vehicle performing the route.
  void SetArcCostEvaluatorOfAllVehicles(int evaluator_index);
  // Sets the cost function for a given vehicle route.
  void SetArcCostEvaluatorOfVehicle(int evaluator_index, int vehicle);
  // Sets the fixed cost of all vehicle routes. It is equivalent to calling
  // SetFixedCostOfVehicle on all vehicle routes.
  void SetFixedCostOfAllVehicles(int64 cost);
  // Sets the fixed cost of one vehicle route.
  void SetFixedCostOfVehicle(int64 cost, int vehicle);
  // Returns the route fixed cost taken into account if the route of the
  // vehicle is not empty, aka there's at least one node on the route other than
  // the first and last nodes.
  int64 GetFixedCostOfVehicle(int vehicle) const;

  // The following methods set the linear and quadratic cost factors of vehicles
  // (must be positive values). The default value of these parameters is zero
  // for all vehicles.
  // When set, the cost_ of the model will contain terms aiming at reducing the
  // number of vehicles used in the model, by adding the following to the
  // objective for every vehicle v:
  // INDICATOR(v used in the model) *
  //   [linear_cost_factor_of_vehicle_[v]
  //    - quadratic_cost_factor_of_vehicle_[v]*(square of length of route v)]
  // i.e. for every used vehicle, we add the linear factor as fixed cost, and
  // subtract the square of the route length multiplied by the quadratic factor.
  // This second term aims at making the routes as dense as possible.
  //
  // Sets the linear and quadratic cost factor of all vehicles.
  void SetAmortizedCostFactorsOfAllVehicles(int64 linear_cost_factor,
                                            int64 quadratic_cost_factor);
  // Sets the linear and quadratic cost factor of the given vehicle.
  void SetAmortizedCostFactorsOfVehicle(int64 linear_cost_factor,
                                        int64 quadratic_cost_factor,
                                        int vehicle);

  const std::vector<int64>& GetAmortizedLinearCostFactorOfVehicles() const {
    return linear_cost_factor_of_vehicle_;
  }
  const std::vector<int64>& GetAmortizedQuadraticCostFactorOfVehicles() const {
    return quadratic_cost_factor_of_vehicle_;
  }

// Search
// Gets/sets the evaluator used during the search. Only relevant when
// RoutingSearchParameters.first_solution_strategy = EVALUATOR_STRATEGY.
#ifndef SWIG
  const Solver::IndexEvaluator2& first_solution_evaluator() const {
    return first_solution_evaluator_;
  }
#endif
  // Takes ownership of evaluator.
  void SetFirstSolutionEvaluator(Solver::IndexEvaluator2 evaluator) {
    first_solution_evaluator_ = std::move(evaluator);
  }
  // Adds a local search operator to the set of operators used to solve the
  // vehicle routing problem.
  void AddLocalSearchOperator(LocalSearchOperator* ls_operator);
  // Adds a search monitor to the search used to solve the routing model.
  void AddSearchMonitor(SearchMonitor* const monitor);
  // Adds a callback called each time a solution is found during the search.
  // This is a shortcut to creating a monitor to call the callback on
  // AtSolution() and adding it with AddSearchMonitor.
  void AddAtSolutionCallback(std::function<void()> callback);
  // Adds a variable to minimize in the solution finalizer. The solution
  // finalizer is called each time a solution is found during the search and
  // allows to instantiate secondary variables (such as dimension cumul
  // variables).
  void AddVariableMinimizedByFinalizer(IntVar* var);
  // Adds a variable to maximize in the solution finalizer (see above for
  // information on the solution finalizer).
  void AddVariableMaximizedByFinalizer(IntVar* var);
  // Closes the current routing model; after this method is called, no
  // modification to the model can be done, but RoutesToAssignment becomes
  // available. Note that CloseModel() is automatically called by Solve() and
  // other methods that produce solution.
  // This is equivalent to calling
  // CloseModelWithParameters(DefaultRoutingSearchParameters()).
  void CloseModel();
  // Same as above taking search parameters (as of 10/2015 some the parameters
  // have to be set when closing the model).
  void CloseModelWithParameters(
      const RoutingSearchParameters& search_parameters);
  // Solves the current routing model; closes the current model.
  // This is equivalent to calling
  // SolveWithParameters(DefaultRoutingSearchParameters())
  // or
  // SolveFromAssignmentWithParameters(assignment,
  //                                   DefaultRoutingSearchParameters()).
  const Assignment* Solve(const Assignment* assignment = nullptr);
  // Solves the current routing model with the given parameters. If 'solutions'
  // is specified, it will contain the k best solutions found during the search
  // (from worst to best, including the one returned by this method), where k
  // corresponds to the 'number_of_solutions_to_collect' in 'search_parameters'.
  // Note that the Assignment returned by the method and the ones in solutions
  // are owned by the underlying solver and should not be deleted.
  const Assignment* SolveWithParameters(
      const RoutingSearchParameters& search_parameters,
      std::vector<const Assignment*>* solutions = nullptr);
  const Assignment* SolveFromAssignmentWithParameters(
      const Assignment* assignment,
      const RoutingSearchParameters& search_parameters,
      std::vector<const Assignment*>* solutions = nullptr);
  // Given a "source_model" and its "source_assignment", resets
  // "target_assignment" with the IntVar variables (nexts_, and vehicle_vars_
  // if costs aren't homogeneous across vehicles) of "this" model, with the
  // values set according to those in "other_assignment".
  // The objective_element of target_assignment is set to this->cost_.
  void SetAssignmentFromOtherModelAssignment(
      Assignment* target_assignment, const RoutingModel* source_model,
      const Assignment* source_assignment);
  // Computes a lower bound to the routing problem solving a linear assignment
  // problem. The routing model must be closed before calling this method.
  // Note that problems with node disjunction constraints (including optional
  // nodes) and non-homogenous costs are not supported (the method returns 0 in
  // these cases).
  // TODO(user): Add support for non-homogeneous costs and disjunctions.
  int64 ComputeLowerBound();
  // Returns the current status of the routing model.
  Status status() const { return status_; }
  // Applies a lock chain to the next search. 'locks' represents an ordered
  // vector of nodes representing a partial route which will be fixed during the
  // next search; it will constrain next variables such that:
  // next[locks[i]] == locks[i+1].
  // Returns the next variable at the end of the locked chain; this variable is
  // not locked. An assignment containing the locks can be obtained by calling
  // PreAssignment().
  IntVar* ApplyLocks(const std::vector<int64>& locks);
  // Applies lock chains to all vehicles to the next search, such that locks[p]
  // is the lock chain for route p. Returns false if the locks do not contain
  // valid routes; expects that the routes do not contain the depots,
  // i.e. there are empty vectors in place of empty routes.
  // If close_routes is set to true, adds the end nodes to the route of each
  // vehicle and deactivates other nodes.
  // An assignment containing the locks can be obtained by calling
  // PreAssignment().
  bool ApplyLocksToAllVehicles(const std::vector<std::vector<int64>>& locks,
                               bool close_routes);
  // Returns an assignment used to fix some of the variables of the problem.
  // In practice, this assignment locks partial routes of the problem. This
  // can be used in the context of locking the parts of the routes which have
  // already been driven in online routing problems.
  const Assignment* const PreAssignment() const { return preassignment_; }
  Assignment* MutablePreAssignment() { return preassignment_; }
  // Writes the current solution to a file containing an AssignmentProto.
  // Returns false if the file cannot be opened or if there is no current
  // solution.
  bool WriteAssignment(const std::string& file_name) const;
  // Reads an assignment from a file and returns the current solution.
  // Returns nullptr if the file cannot be opened or if the assignment is not
  // valid.
  Assignment* ReadAssignment(const std::string& file_name);
  // Restores an assignment as a solution in the routing model and returns the
  // new solution. Returns nullptr if the assignment is not valid.
  Assignment* RestoreAssignment(const Assignment& solution);
  // Restores the routes as the current solution. Returns nullptr if the
  // solution cannot be restored (routes do not contain a valid solution). Note
  // that calling this method will run the solver to assign values to the
  // dimension variables; this may take considerable amount of time, especially
  // when using dimensions with slack.
  Assignment* ReadAssignmentFromRoutes(
      const std::vector<std::vector<int64>>& routes,
      bool ignore_inactive_indices);
  // Fills an assignment from a specification of the routes of the
  // vehicles. The routes are specified as lists of variable indices that appear
  // on the routes of the vehicles. The indices of the outer vector in
  // 'routes' correspond to vehicles IDs, the inner vector contains the
  // variable indices on the routes for the given vehicle. The inner vectors
  // must not contain the start and end indices, as these are determined by the
  // routing model.  Sets the value of NextVars in the assignment, adding the
  // variables to the assignment if necessary. The method does not touch other
  // variables in the assignment. The method can only be called after the model
  // is closed.  With ignore_inactive_indices set to false, this method will
  // fail (return nullptr) in case some of the route contain indices that are
  // deactivated in the model; when set to true, these indices will be
  // skipped.  Returns true if routes were successfully
  // loaded. However, such assignment still might not be a valid
  // solution to the routing problem due to more complex constraints;
  // it is advisible to call solver()->CheckSolution() afterwards.
  bool RoutesToAssignment(const std::vector<std::vector<int64>>& routes,
                          bool ignore_inactive_indices, bool close_routes,
                          Assignment* const assignment) const;
  // Converts the solution in the given assignment to routes for all vehicles.
  // Expects that assignment contains a valid solution (i.e. routes for all
  // vehicles end with an end index for that vehicle).
  void AssignmentToRoutes(const Assignment& assignment,
                          std::vector<std::vector<int64>>* const routes) const;
  // Returns a compacted version of the given assignment, in which all vehicles
  // with id lower or equal to some N have non-empty routes, and all vehicles
  // with id greater than N have empty routes. Does not take ownership of the
  // returned object.
  // If found, the cost of the compact assignment is the same as in the
  // original assignment and it preserves the values of 'active' variables.
  // Returns nullptr if a compact assignment was not found.
  // This method only works in homogenous mode, and it only swaps equivalent
  // vehicles (vehicles with the same start and end nodes). When creating the
  // compact assignment, the empty plan is replaced by the route assigned to the
  // compatible vehicle with the highest id. Note that with more complex
  // constraints on vehicle variables, this method might fail even if a compact
  // solution exists.
  // This method changes the vehicle and dimension variables as necessary.
  // While compacting the solution, only basic checks on vehicle variables are
  // performed; if one of these checks fails no attempts to repair it are made
  // (instead, the method returns nullptr).
  Assignment* CompactAssignment(const Assignment& assignment) const;
  // Same as CompactAssignment() but also checks the validity of the final
  // compact solution; if it is not valid, no attempts to repair it are made
  // (instead, the method returns nullptr).
  Assignment* CompactAndCheckAssignment(const Assignment& assignment) const;
  // Adds an extra variable to the vehicle routing assignment.
  void AddToAssignment(IntVar* const var);
  void AddIntervalToAssignment(IntervalVar* const interval);
#ifndef SWIG
  // TODO(user): Revisit if coordinates are added to the RoutingModel class.
  void SetSweepArranger(SweepArranger* sweep_arranger) {
    sweep_arranger_.reset(sweep_arranger);
  }
  // Returns the sweep arranger to be used by routing heuristics.
  SweepArranger* sweep_arranger() const { return sweep_arranger_.get(); }
#endif
  // Adds a custom local search filter to the list of filters used to speed up
  // local search by pruning unfeasible variable assignments.
  // Calling this method after the routing model has been closed (CloseModel()
  // or Solve() has been called) has no effect.
  // The routing model does not take ownership of the filter.
  void AddLocalSearchFilter(LocalSearchFilter* filter) {
    CHECK(filter != nullptr);
    if (closed_) {
      LOG(WARNING) << "Model is closed, filter addition will be ignored.";
    }
    extra_filters_.push_back(filter);
  }

  // Model inspection.
  // Returns the variable index of the starting node of a vehicle route.
  int64 Start(int vehicle) const { return starts_[vehicle]; }
  // Returns the variable index of the ending node of a vehicle route.
  int64 End(int vehicle) const { return ends_[vehicle]; }
  // Returns true if 'index' represents the first node of a route.
  bool IsStart(int64 index) const;
  // Returns true if 'index' represents the last node of a route.
  bool IsEnd(int64 index) const { return index >= Size(); }
  // Assignment inspection
  // Returns the variable index of the node directly after the node
  // corresponding to 'index' in 'assignment'.
  int64 Next(const Assignment& assignment, int64 index) const;
  // Returns true if the route of 'vehicle' is non empty in 'assignment'.
  bool IsVehicleUsed(const Assignment& assignment, int vehicle) const;
// Variables
#if !defined(SWIGPYTHON)
  // Returns all next variables of the model, such that Nexts(i) is the next
  // variable of the node corresponding to i.
  const std::vector<IntVar*>& Nexts() const { return nexts_; }
  // Returns all vehicle variables of the model,  such that VehicleVars(i) is
  // the vehicle variable of the node corresponding to i.
  const std::vector<IntVar*>& VehicleVars() const { return vehicle_vars_; }
#endif  // !defined(SWIGPYTHON)
  // Returns the next variable of the node corresponding to index. Note that
  // NextVar(index) == index is equivalent to ActiveVar(index) == 0.
  IntVar* NextVar(int64 index) const { return nexts_[index]; }
  // Returns the active variable of the node corresponding to index.
  IntVar* ActiveVar(int64 index) const { return active_[index]; }
  // Returns the vehicle variable of the node corresponding to index. Note that
  // VehicleVar(index) == -1 is equivalent to ActiveVar(index) == 0.
  IntVar* VehicleVar(int64 index) const { return vehicle_vars_[index]; }
  // Returns the global cost variable which is being minimized.
  IntVar* CostVar() const { return cost_; }
  // Returns the cost of the transit arc between two nodes for a given vehicle.
  // Input are variable indices of node. This returns 0 if vehicle < 0.
  int64 GetArcCostForVehicle(int64 from_index, int64 to_index, int64 vehicle);
  // Whether costs are homogeneous across all vehicles.
  bool CostsAreHomogeneousAcrossVehicles() const {
    return costs_are_homogeneous_across_vehicles_;
  }
  // Returns the cost of the segment between two nodes supposing all vehicle
  // costs are the same (returns the cost for the first vehicle otherwise).
  int64 GetHomogeneousCost(int64 from_index, int64 to_index) {
    return GetArcCostForVehicle(from_index, to_index, /*vehicle=*/0);
  }
  // Returns the cost of the arc in the context of the first solution strategy.
  // This is typically a simplification of the actual cost; see the .cc.
  int64 GetArcCostForFirstSolution(int64 from_index, int64 to_index);
  // Returns the cost of the segment between two nodes for a given cost
  // class. Input are variable indices of nodes and the cost class.
  // Unlike GetArcCostForVehicle(), if cost_class is kNoCost, then the
  // returned cost won't necessarily be zero: only some of the components
  // of the cost that depend on the cost class will be omited. See the code
  // for details.
  int64 GetArcCostForClass(int64 from_index, int64 to_index,
                           int64 /*CostClassIndex*/ cost_class_index);
  // Get the cost class index of the given vehicle.
  CostClassIndex GetCostClassIndexOfVehicle(int64 vehicle) const {
    DCHECK(closed_);
    return cost_class_index_of_vehicle_[vehicle];
  }
  // Returns true iff the model contains a vehicle with the given
  // cost_class_index.
  bool HasVehicleWithCostClassIndex(CostClassIndex cost_class_index) const {
    DCHECK(closed_);
    if (cost_class_index == kCostClassIndexOfZeroCost) {
      return has_vehicle_with_zero_cost_class_;
    }
    return cost_class_index < cost_classes_.size();
  }
  // Returns the number of different cost classes in the model.
  int GetCostClassesCount() const { return cost_classes_.size(); }
  // Ditto, minus the 'always zero', built-in cost class.
  int GetNonZeroCostClassesCount() const {
    return std::max(0, GetCostClassesCount() - 1);
  }
  VehicleClassIndex GetVehicleClassIndexOfVehicle(int64 vehicle) const {
    DCHECK(closed_);
    return vehicle_class_index_of_vehicle_[vehicle];
  }
  // Returns the number of different vehicle classes in the model.
  int GetVehicleClassesCount() const { return vehicle_classes_.size(); }
  // Returns variable indices of nodes constrained to be on the same route.
  const std::vector<int>& GetSameVehicleIndicesOfIndex(int node) const {
    DCHECK(closed_);
    return same_vehicle_groups_[same_vehicle_group_[node]];
  }
  // Returns whether the arc from->to1 is more constrained than from->to2,
  // taking into account, in order:
  // - whether the destination node isn't an end node
  // - whether the destination node is mandatory
  // - whether the destination node is bound to the same vehicle as the source
  // - the "primary constrained" dimension (see SetPrimaryConstrainedDimension)
  // It then breaks ties using, in order:
  // - the arc cost (taking unperformed penalties into account)
  // - the size of the vehicle vars of "to1" and "to2" (lowest size wins)
  // - the value: the lowest value of the indices to1 and to2 wins.
  // See the .cc for details.
  // The more constrained arc is typically preferable when building a
  // first solution. This method is intended to be used as a callback for the
  // BestValueByComparisonSelector value selector.
  // Args:
  //   from: the variable index of the source node
  //   to1: the variable index of the first candidate destination node.
  //   to2: the variable index of the second candidate destination node.
  bool ArcIsMoreConstrainedThanArc(int64 from, int64 to1, int64 to2);
  // Print some debugging information about an assignment, including the
  // feasible intervals of the CumulVar for dimension "dimension_to_print"
  // at each step of the routes.
  // If "dimension_to_print" is omitted, all dimensions will be printed.
  std::string DebugOutputAssignment(
      const Assignment& solution_assignment,
      const std::string& dimension_to_print) const;

  // Returns the underlying constraint solver. Can be used to add extra
  // constraints and/or modify search algoithms.
  Solver* solver() const { return solver_.get(); }

  // Returns true if the search limit has been crossed.
  bool CheckLimit() { return limit_->Check(); }

  // Sizes and indices
  // Returns the number of nodes in the model.
  int nodes() const { return nodes_; }
  // Returns the number of vehicle routes in the model.
  int vehicles() const { return vehicles_; }
  // Returns the number of next variables in the model.
  int64 Size() const { return nodes_ + vehicles_ - start_end_count_; }

  // Returns statistics on first solution search, number of decisions sent to
  // filters, number of decisions rejected by filters.
  int64 GetNumberOfDecisionsInFirstSolution(
      const RoutingSearchParameters& search_parameters) const;
  int64 GetNumberOfRejectsInFirstSolution(
      const RoutingSearchParameters& search_parameters) const;

  // Returns true if a vehicle/node matching problem is detected.
  bool IsMatchingModel() const;

  // Internal only: initializes the builders used to build a solver model from
  // CpModels.
  static void InitializeBuilders(Solver* solver);

#ifndef SWIG
  // Sets the callback returning the variable to use for the Tabu Search
  // metaheuristic.
  using GetTabuVarsCallback =
      std::function<std::vector<operations_research::IntVar*>(RoutingModel*)>;

  void SetTabuVarsCallback(GetTabuVarsCallback tabu_var_callback);
#endif  // SWIG

  // The next few members are in the public section only for testing purposes.
  // TODO(user): Find a way to test and restrict the access at the same time.
  //
  // MakeGuidedSlackFinalizer creates a DecisionBuilder for the slacks of a
  // dimension using a callback to choose which values to start with.
  // The finalizer works only when all next variables in the model have
  // been fixed. It has the following two characteristics:
  // 1. It follows the routes defined by the nexts variables when choosing a
  //    variable to make a decision on.
  // 2. When it comes to choose a value for the slack of node i, the decision
  //    builder first calls the callback with argument i, and supposingly the
  //    returned value is x it creates decisions slack[i] = x, slack[i] = x + 1,
  //    slack[i] = x - 1, slack[i] = x + 2, etc.
  DecisionBuilder* MakeGuidedSlackFinalizer(
      const RoutingDimension* dimension,
      std::function<int64(int64)> initializer);
#ifndef SWIG
  // TODO(user): MakeGreedyDescentLSOperator is too general for routing.h.
  // Perhaps move it to constraint_solver.h.
  // MakeGreedyDescentLSOperator creates a local search operator that tries to
  // improve the initial assignment by moving a logarithmically decreasing step
  // away in each possible dimension.
  static std::unique_ptr<LocalSearchOperator> MakeGreedyDescentLSOperator(
      std::vector<IntVar*> variables);
#endif  // __SWIG__
  // MakeSelfDependentDimensionFinalizer is a finalizer for the slacks of a
  // self-dependent dimension. It makes an extensive use of the caches of the
  // state dependent transits.
  // In detail, MakeSelfDependentDimensionFinalizer returns a composition of a
  // local search decision builder with a greedy descent operator for the cumul
  // of the start of each route and a guided slack finalizer. Provided there are
  // no time windows and the maximum slacks are large enough, once the cumul of
  // the start of route is fixed, the guided finalizer can find optimal values
  // of the slacks for the rest of the route in time proportional to the length
  // of the route. Therefore the composed finalizer generally works in time
  // O(log(t)*n*m), where t is the latest possible departute time, n is the
  // number of nodes in the network and m is the number of vehicles.
  DecisionBuilder* MakeSelfDependentDimensionFinalizer(
      const RoutingDimension* dimension);

 private:
  // Local search move operator usable in routing.
  enum RoutingLocalSearchOperator {
    RELOCATE = 0,
    RELOCATE_PAIR,
    LIGHT_RELOCATE_PAIR,
    RELOCATE_NEIGHBORS,
    EXCHANGE,
    EXCHANGE_PAIR,
    CROSS,
    CROSS_EXCHANGE,
    TWO_OPT,
    OR_OPT,
    RELOCATE_EXPENSIVE_CHAIN,
    LIN_KERNIGHAN,
    TSP_OPT,
    MAKE_ACTIVE,
    RELOCATE_AND_MAKE_ACTIVE,
    MAKE_ACTIVE_AND_RELOCATE,
    MAKE_INACTIVE,
    MAKE_CHAIN_INACTIVE,
    SWAP_ACTIVE,
    EXTENDED_SWAP_ACTIVE,
    NODE_PAIR_SWAP,
    PATH_LNS,
    FULL_PATH_LNS,
    TSP_LNS,
    INACTIVE_LNS,
    EXCHANGE_RELOCATE_PAIR,
    LOCAL_SEARCH_OPERATOR_COUNTER
  };

  // Structure storing a value for a set of variable indices. Is used to store
  // data for index disjunctions (variable indices, max_cardinality and penalty
  // when unperformed).
  template <typename T>
  struct ValuedNodes {
    std::vector<int64> indices;
    T value;
  };
  struct DisjunctionValues {
    int64 penalty;
    int64 max_cardinality;
  };
  typedef ValuedNodes<DisjunctionValues> Disjunction;

  // Storage of a cost cache element corresponding to a cost arc ending at
  // node 'index' and on the cost class 'cost_class'.
  struct CostCacheElement {
    // This is usually an int64, but using an int here decreases the RAM usage,
    // and should be fine since in practice we never have more than 1<<31 vars.
    // Note(user): on 2013-11, microbenchmarks on the arc costs callbacks
    // also showed a 2% speed-up thanks to using int rather than int64.
    int index;
    CostClassIndex cost_class_index;
    int64 cost;
  };

  // Internal methods.
  void Initialize();
  void AddNoCycleConstraintInternal();
  bool AddDimensionWithCapacityInternal(
      const std::vector<int>& evaluator_indices, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool AddDimensionDependentDimensionWithVehicleCapacityInternal(
      const std::vector<int>& pure_transits,
      const std::vector<int>& dependent_transits,
      const RoutingDimension* base_dimension, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool InitializeDimensionInternal(
      const std::vector<int>& evaluator_indices,
      const std::vector<int>& state_dependent_evaluator_indices,
      int64 slack_max, bool fix_start_cumul_to_zero,
      RoutingDimension* dimension);
  DimensionIndex GetDimensionIndex(const std::string& dimension_name) const;
  // Returns dimensions with soft and vehicle span costs.
  std::vector<RoutingDimension*> GetDimensionsWithSoftAndSpanCosts() const;
  void ComputeCostClasses(const RoutingSearchParameters& parameters);
  void ComputeVehicleClasses();
  int64 GetArcCostForClassInternal(int64 from_index, int64 to_index,
                                   CostClassIndex cost_class_index);
  void AppendHomogeneousArcCosts(const RoutingSearchParameters& parameters,
                                 int node_index,
                                 std::vector<IntVar*>* cost_elements);
  void AppendArcCosts(const RoutingSearchParameters& parameters, int node_index,
                      std::vector<IntVar*>* cost_elements);
  Assignment* DoRestoreAssignment();
  static const CostClassIndex kCostClassIndexOfZeroCost;
  int64 SafeGetCostClassInt64OfVehicle(int64 vehicle) const {
    DCHECK_LT(0, vehicles_);
    return (vehicle >= 0 ? GetCostClassIndexOfVehicle(vehicle)
                         : kCostClassIndexOfZeroCost)
        .value();
  }
  int64 GetDimensionTransitCostSum(int64 i, int64 j,
                                   const CostClass& cost_class) const;
  // Returns nullptr if no penalty cost, otherwise returns penalty variable.
  IntVar* CreateDisjunction(DisjunctionIndex disjunction);
  // Sets up pickup and delivery sets.
  void AddPickupAndDeliverySetsInternal(const std::vector<int64>& pickups,
                                        const std::vector<int64>& deliveries);
  // Returns the cost variable related to the soft same vehicle constraint of
  // index 'vehicle_index'.
  IntVar* CreateSameVehicleCost(int vehicle_index);
  // Returns the first active variable index in 'indices' starting from index
  // + 1.
  int FindNextActive(int index, const std::vector<int64>& indices) const;

  // Checks that all nodes on the route starting at start_index (using the
  // solution stored in assignment) can be visited by the given vehicle.
  bool RouteCanBeUsedByVehicle(const Assignment& assignment, int start_index,
                               int vehicle) const;
  // Replaces the route of unused_vehicle with the route of active_vehicle in
  // compact_assignment. Expects that unused_vehicle is a vehicle with an empty
  // route and that the route of active_vehicle is non-empty. Also expects that
  // 'assignment' contains the original assignment, from which
  // compact_assignment was created.
  // Returns true if the vehicles were successfully swapped; otherwise, returns
  // false.
  bool ReplaceUnusedVehicle(int unused_vehicle, int active_vehicle,
                            Assignment* compact_assignment) const;

  void QuietCloseModel();
  void QuietCloseModelWithParameters(
      const RoutingSearchParameters& parameters) {
    if (!closed_) {
      CloseModelWithParameters(parameters);
    }
  }

  // Solve matching problem with min-cost flow and store result in assignment.
  bool SolveMatchingModel(Assignment* assignment);
#ifndef SWIG
  // Append an assignment to a vector of assignments if it is feasible.
  bool AppendAssignmentIfFeasible(
      const Assignment& assignment,
      std::vector<std::unique_ptr<Assignment>>* assignments);
#endif
  // Log a solution.
  void LogSolution(const std::string& description, int64 solution_cost,
                   int64 start_time_ms);
  // See CompactAssignment. Checks the final solution if
  // check_compact_assignement is true.
  Assignment* CompactAssignmentInternal(const Assignment& assignment,
                                        bool check_compact_assignment) const;
  // Checks that the current search parameters are valid for the current model's
  // specific settings. This assumes that FindErrorInSearchParameters() from
  // ./routing_flags.h caught no error.
  std::string FindErrorInSearchParametersForModel(
      const RoutingSearchParameters& search_parameters) const;
  // Sets up search objects, such as decision builders and monitors.
  void SetupSearch(const RoutingSearchParameters& search_parameters);
  // Set of auxiliary methods used to setup the search.
  // TODO(user): Document each auxiliary method.
  Assignment* GetOrCreateAssignment();
  Assignment* GetOrCreateTmpAssignment();
  SearchLimit* GetOrCreateLimit();
  SearchLimit* GetOrCreateLocalSearchLimit();
  SearchLimit* GetOrCreateLargeNeighborhoodSearchLimit();
  LocalSearchOperator* CreateInsertionOperator();
  LocalSearchOperator* CreateMakeInactiveOperator();
  void CreateNeighborhoodOperators(const RoutingSearchParameters& parameters);
  LocalSearchOperator* GetNeighborhoodOperators(
      const RoutingSearchParameters& search_parameters) const;
  const std::vector<LocalSearchFilter*>& GetOrCreateLocalSearchFilters();
  const std::vector<LocalSearchFilter*>& GetOrCreateFeasibilityFilters();
  DecisionBuilder* CreateSolutionFinalizer();
  void CreateFirstSolutionDecisionBuilders(
      const RoutingSearchParameters& search_parameters);
  DecisionBuilder* GetFirstSolutionDecisionBuilder(
      const RoutingSearchParameters& search_parameters) const;
  IntVarFilteredDecisionBuilder* GetFilteredFirstSolutionDecisionBuilderOrNull(
      const RoutingSearchParameters& parameters) const;
  LocalSearchPhaseParameters* CreateLocalSearchParameters(
      const RoutingSearchParameters& search_parameters);
  DecisionBuilder* CreateLocalSearchDecisionBuilder(
      const RoutingSearchParameters& search_parameters);
  void SetupDecisionBuilders(const RoutingSearchParameters& search_parameters);
  void SetupMetaheuristics(const RoutingSearchParameters& search_parameters);
  void SetupAssignmentCollector(
      const RoutingSearchParameters& search_parameters);
  void SetupTrace(const RoutingSearchParameters& search_parameters);
  void SetupSearchMonitors(const RoutingSearchParameters& search_parameters);
  bool UsesLightPropagation(
      const RoutingSearchParameters& search_parameters) const;
  GetTabuVarsCallback tabu_var_callback_;

  int GetVehicleStartClass(int64 start) const;

  void InitSameVehicleGroups(int number_of_groups) {
    same_vehicle_group_.assign(Size(), 0);
    same_vehicle_groups_.assign(number_of_groups, {});
  }
  void SetSameVehicleGroup(int index, int group) {
    same_vehicle_group_[index] = group;
    same_vehicle_groups_[group].push_back(index);
  }

  // Model
  std::unique_ptr<Solver> solver_;
  int nodes_;
  int vehicles_;
  Constraint* no_cycle_constraint_ = nullptr;
  // Decision variables: indexed by int64 var index.
  std::vector<IntVar*> nexts_;
  std::vector<IntVar*> vehicle_vars_;
  std::vector<IntVar*> active_;
  // is_bound_to_end_[i] will be true iff the path starting at var #i is fully
  // bound and reaches the end of a route, i.e. either:
  // - IsEnd(i) is true
  // - or nexts_[i] is bound and is_bound_to_end_[nexts_[i].Value()] is true.
  std::vector<IntVar*> is_bound_to_end_;
  RevSwitch is_bound_to_end_ct_added_;
  // Dimensions
  std::unordered_map<std::string, DimensionIndex> dimension_name_to_index_;
  gtl::ITIVector<DimensionIndex, RoutingDimension*> dimensions_;
  std::string primary_constrained_dimension_;
  // Costs
  IntVar* cost_ = nullptr;
  std::vector<int> vehicle_to_transit_cost_;
  std::vector<int64> fixed_cost_of_vehicle_;
  std::vector<CostClassIndex> cost_class_index_of_vehicle_;
  bool has_vehicle_with_zero_cost_class_;
  std::vector<int64> linear_cost_factor_of_vehicle_;
  std::vector<int64> quadratic_cost_factor_of_vehicle_;
  bool vehicle_amortized_cost_factors_set_;
#ifndef SWIG
  gtl::ITIVector<CostClassIndex, CostClass> cost_classes_;
#endif  // SWIG
  bool costs_are_homogeneous_across_vehicles_;
  bool cache_callbacks_;
  std::vector<CostCacheElement> cost_cache_;  // Index by source index.
  std::vector<VehicleClassIndex> vehicle_class_index_of_vehicle_;
#ifndef SWIG
  gtl::ITIVector<VehicleClassIndex, VehicleClass> vehicle_classes_;
#endif  // SWIG
  std::function<int(int64)> vehicle_start_class_callback_;
  // Disjunctions
  gtl::ITIVector<DisjunctionIndex, Disjunction> disjunctions_;
  std::vector<std::vector<DisjunctionIndex>> index_to_disjunctions_;
  // Same vehicle costs
  std::vector<ValuedNodes<int64>> same_vehicle_costs_;
  // Pickup and delivery
  IndexPairs pickup_delivery_pairs_;
  std::vector<std::pair<DisjunctionIndex, DisjunctionIndex>>
      pickup_delivery_disjunctions_;
  // clang-format off
  // If node_index is a pickup, index_to_pickup_index_pairs_[node_index] is the
  // vector of pairs {pair_index, pickup_index} such that
  // (pickup_delivery_pairs_[pair_index].first)[pickup_index] == node_index
  std::vector<std::vector<std::pair<int, int> > > index_to_pickup_index_pairs_;
  // Same as above for deliveries.
  std::vector<std::vector<std::pair<int, int> > >
      index_to_delivery_index_pairs_;
  // clang-format on
#ifndef SWIG
  std::vector<PickupAndDeliveryPolicy> vehicle_pickup_delivery_policy_;
#endif  // SWIG
  // Same vehicle group to which a node belongs.
  std::vector<int> same_vehicle_group_;
  // Same vehicle node groups.
  std::vector<std::vector<int>> same_vehicle_groups_;
  // Node visit types
  // Variable index to visit type index.
  std::vector<int> index_to_visit_type_;
  // clang-format off
  std::vector<std::unordered_set<int> > incompatible_types_per_type_index_;
  // clang-format on
  // Empty set used in GetTypeIncompatibilities() when the given type has no
  // incompatibilities.
  const std::unordered_set<int> empty_incompatibilities_;
  int num_visit_types_;
  // Two indices are equivalent if they correspond to the same node (as given to
  // the constructors taking a RoutingIndexManager).
  std::vector<int> index_to_equivalence_class_;
  std::vector<int> index_to_vehicle_;
  std::vector<int64> starts_;
  std::vector<int64> ends_;
  // TODO(user): b/62478706 Once the port is done, this shouldn't be needed
  //                  anymore.
  RoutingIndexManager manager_;
  int start_end_count_;
  // Model status
  bool closed_ = false;
  Status status_ = ROUTING_NOT_SOLVED;
  bool enable_deep_serialization_ = true;

  // Search data
  std::vector<DecisionBuilder*> first_solution_decision_builders_;
  std::vector<IntVarFilteredDecisionBuilder*>
      first_solution_filtered_decision_builders_;
  Solver::IndexEvaluator2 first_solution_evaluator_;
  std::vector<LocalSearchOperator*> local_search_operators_;
  std::vector<SearchMonitor*> monitors_;
  SolutionCollector* collect_assignments_ = nullptr;
  SolutionCollector* collect_one_assignment_ = nullptr;
  DecisionBuilder* solve_db_ = nullptr;
  DecisionBuilder* improve_db_ = nullptr;
  DecisionBuilder* restore_assignment_ = nullptr;
  DecisionBuilder* restore_tmp_assignment_ = nullptr;
  Assignment* assignment_ = nullptr;
  Assignment* preassignment_ = nullptr;
  Assignment* tmp_assignment_ = nullptr;
  std::vector<IntVar*> extra_vars_;
  std::vector<IntervalVar*> extra_intervals_;
  std::vector<LocalSearchOperator*> extra_operators_;
  std::vector<LocalSearchFilter*> filters_;
  std::vector<LocalSearchFilter*> feasibility_filters_;
  std::vector<LocalSearchFilter*> extra_filters_;
  std::vector<IntVar*> variables_maximized_by_finalizer_;
  std::vector<IntVar*> variables_minimized_by_finalizer_;
#ifndef SWIG
  std::unique_ptr<SweepArranger> sweep_arranger_;
#endif

  SearchLimit* limit_ = nullptr;
  SearchLimit* ls_limit_ = nullptr;
  SearchLimit* lns_limit_ = nullptr;

  typedef std::pair<int64, int64> CacheKey;
  typedef std::unordered_map<CacheKey, int64> TransitCallbackCache;
  typedef std::unordered_map<CacheKey, StateDependentTransit>
      StateDependentTransitCallbackCache;

  std::vector<TransitCallback1> unary_transit_evaluators_;
  std::vector<TransitCallback2> transit_evaluators_;
  std::vector<VariableIndexEvaluator2> state_dependent_transit_evaluators_;
  std::vector<std::unique_ptr<StateDependentTransitCallbackCache>>
      state_dependent_transit_evaluators_cache_;

  friend class RoutingDimension;
  friend class RoutingModelInspector;

  DISALLOW_COPY_AND_ASSIGN(RoutingModel);
};

// Routing model visitor.
class RoutingModelVisitor : public BaseObject {
 public:
  // Constraint types.
  static const char kLightElement[];
  static const char kLightElement2[];
  static const char kRemoveValues[];
};

// This class acts like a CP propagator: it takes a set of tasks given by
// their start/duration/end features, and reduces the range of possible values.
class DisjunctivePropagator {
 public:
  // A structure to hold tasks described by their features.
  // The first num_chain_tasks are considered linked by a chain of precedences,
  // i.e. if i < j < num_chain_tasks, then end(i) <= start(j).
  // This occurs frequently in routing, and can be leveraged by
  // some variants of classic propagators.
  struct Tasks {
    int num_chain_tasks = 0;
    std::vector<int64> start_min;
    std::vector<int64> duration_min;
    std::vector<int64> end_max;
    std::vector<bool> is_preemptible;
    std::vector<const SortedDisjointIntervalList*> forbidden_intervals;
  };

  // Computes new bounds for all tasks, returns false if infeasible.
  // This does not compute a fixed point, so recalling it may filter more.
  bool Propagate(Tasks* tasks);

  // Propagates the deductions from the chain of precedences, if there is one.
  bool Precedences(Tasks* tasks);
  // Transforms the problem with a time symmetry centered in 0. Returns true for
  // convenience.
  bool MirrorTasks(Tasks* tasks);
  // Does edge-finding deductions on all tasks.
  bool EdgeFinding(Tasks* tasks);
  // Does detectable precedences deductions on tasks in the chain precedence,
  // taking the time windows of nonchain tasks into account.
  bool DetectablePrecedencesWithChain(Tasks* tasks);
  // Tasks might have holes in their domain, this enforces such holes.
  bool ForbiddenIntervals(Tasks* tasks);

 private:
  // The main algorithm uses Vilim's theta tree data structure.
  // See Petr Vilim's PhD thesis "Global Constraints in Scheduling".
  sat::ThetaLambdaTree<int64> theta_lambda_tree_;
  // Mappings between events and tasks.
  std::vector<int> tasks_by_start_min_;
  std::vector<int> tasks_by_end_max_;
  std::vector<int> event_of_task_;
  std::vector<int> nonchain_tasks_by_start_max_;
};

// GlobalVehicleBreaksConstraint ensures breaks constraints are enforced on
// all vehicles in the dimension passed to its constructor.
// It is intended to be used for dimensions representing time.
// A break constraint ensures break intervals fit on the route of a vehicle.
// For a given vehicle, it forces break intervals to be disjoint from visit
// intervals, where visit intervals start at CumulVar(node) and last for
// node_visit_transit[node]. Moreover, it ensures that there is enough time
// between two consecutive nodes of a route to do transit and vehicle breaks,
// i.e. if Next(nodeA) = nodeB, CumulVar(nodeA) = tA and CumulVar(nodeB) = tB,
// then SlackVar(nodeA) >= sum_{breaks \subseteq [tA, tB)} duration(break).
// TODO(user): This does not enforce vehicle breaks to be nonoverlapping,
//   and supposes travel/service times to be feasible (e.g. with a PathCumul).
//   This is probably the desired behaviour, because vehicle breaks will most
//   likely be constrained with precedence relations that are stronger than
//   a resource constraint.
class GlobalVehicleBreaksConstraint : public Constraint {
 public:
  explicit GlobalVehicleBreaksConstraint(const RoutingDimension* dimension);

  void Post() override;
  void InitialPropagate() override;

 private:
  void PropagateNode(int node);
  void PropagateVehicle(int vehicle);
  const RoutingModel* model_;
  const RoutingDimension* const dimension_;
  std::vector<Demon*> vehicle_demons_;

  // This translates pruning information to solver variables.
  // This class should have been an interface + subclasses,
  // but that would force pointers in the tasks_ vector,
  // which means dynamic allocation. Here tasks_'s reserved size will
  // adjust to usage and eventually no more dynamic allocation will be made.
  class TaskTranslator {
   public:
    TaskTranslator(IntVar* start, int64 duration_min)
        : start_(start), duration_min_(duration_min) {}
    explicit TaskTranslator(IntervalVar* interval) : interval_(interval) {}
    TaskTranslator() {}

    void SetStartMin(int64 value) {
      if (start_ != nullptr) {
        start_->SetMin(value);
      } else if (interval_ != nullptr) {
        interval_->SetStartMin(value);
      }
    }
    void SetEndMax(int64 value) {
      if (start_ != nullptr) {
        start_->SetMax(value - duration_min_);
      } else if (interval_ != nullptr) {
        interval_->SetEndMax(value);
      }
    }

   private:
    IntVar* start_ = nullptr;
    int64 duration_min_;
    IntervalVar* interval_ = nullptr;
  };

  // Route and interval variables are normalized to the following values.
  std::vector<TaskTranslator> task_translators_;

  // This is used to restrict bounds of tasks.
  DisjunctivePropagator disjunctive_propagator_;
  DisjunctivePropagator::Tasks tasks_;
};

// Dimensions represent quantities accumulated at nodes along the routes. They
// represent quantities such as weights or volumes carried along the route, or
// distance or times.
//
// Quantities at a node are represented by "cumul" variables and the increase
// or decrease of quantities between nodes are represented by "transit"
// variables. These variables are linked as follows:
//
// if j == next(i),
// cumuls(j) = cumuls(i) + transits(i) + slacks(i) + state_dependent_transits(i)
//
// where slack is a positive slack variable (can represent waiting times for
// a time dimension), and state_dependent_transits is a non-purely functional
// version of transits_. Favour transits over state_dependent_transits when
// possible, because purely functional callbacks allow more optimisations and
// make the model faster and easier to solve.
// TODO(user): Break constraints need to know the service time of nodes
// for a given vehicle, it is passed as an external vector, it would be better
// to have this information here.
class RoutingDimension {
 public:
  ~RoutingDimension();
  // Returns the model on which the dimension was created.
  RoutingModel* model() const { return model_; }
  // Returns the transition value for a given pair of nodes (as var index);
  // this value is the one taken by the corresponding transit variable when
  // the 'next' variable for 'from_index' is bound to 'to_index'.
  int64 GetTransitValue(int64 from_index, int64 to_index, int64 vehicle) const;
  // Same as above but taking a vehicle class of the dimension instead of a
  // vehicle (the class of a vehicle can be obtained with vehicle_to_class()).
  int64 GetTransitValueFromClass(int64 from_index, int64 to_index,
                                 int64 vehicle_class) const {
    return model_->TransitCallback(class_evaluators_[vehicle_class])(from_index,
                                                                     to_index);
  }
  // Get the cumul, transit and slack variables for the given node (given as
  // int64 var index).
  IntVar* CumulVar(int64 index) const { return cumuls_[index]; }
  IntVar* TransitVar(int64 index) const { return transits_[index]; }
  IntVar* FixedTransitVar(int64 index) const { return fixed_transits_[index]; }
  IntVar* SlackVar(int64 index) const { return slacks_[index]; }
#if !defined(SWIGPYTHON)
  // Like CumulVar(), TransitVar(), SlackVar() but return the whole variable
  // vectors instead (indexed by int64 var index).
  const std::vector<IntVar*>& cumuls() const { return cumuls_; }
  const std::vector<IntVar*>& transits() const { return transits_; }
  const std::vector<IntVar*>& slacks() const { return slacks_; }
#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
  // Returns forbidden intervals for each node.
  const std::vector<SortedDisjointIntervalList>& forbidden_intervals() const {
    return forbidden_intervals_;
  }
  // Returns the capacities for all vehicles.
  const std::vector<int64>& vehicle_capacities() const {
    return vehicle_capacities_;
  }
  // Returns the callback evaluating the transit value between two node indices
  // for a given vehicle.
  const RoutingModel::TransitCallback2& transit_evaluator(int vehicle) const {
    return model_->TransitCallback(
        class_evaluators_[vehicle_to_class_[vehicle]]);
  }
  int vehicle_to_class(int vehicle) const { return vehicle_to_class_[vehicle]; }
#endif  // !defined(SWIGCSHARP) && !defined(SWIGJAVA)
#endif  // !defined(SWIGPYTHON)
  // Sets an upper bound on the dimension span on a given vehicle. This is the
  // preferred way to limit the "length" of the route of a vehicle according to
  // a dimension.
  void SetSpanUpperBoundForVehicle(int64 upper_bound, int vehicle);
  // Sets a cost proportional to the dimension span on a given vehicle,
  // or on all vehicles at once. "coefficient" must be nonnegative.
  // This is handy to model costs proportional to idle time when the dimension
  // represents time.
  // The cost for a vehicle is
  //   span_cost = coefficient * (dimension end value - dimension start value).
  void SetSpanCostCoefficientForVehicle(int64 coefficient, int vehicle);
  void SetSpanCostCoefficientForAllVehicles(int64 coefficient);
  // Sets a cost proportional to the *global* dimension span, that is the
  // difference between the largest value of route end cumul variables and
  // the smallest value of route start cumul variables.
  // In other words:
  // global_span_cost =
  //   coefficient * (Max(dimension end value) - Min(dimension start value)).
  void SetGlobalSpanCostCoefficient(int64 coefficient);

#ifndef SWIG
  // Sets a piecewise linear cost on the cumul variable of a given variable
  // index. If f is a piecewise linear function, the resulting cost at 'index'
  // will be f(CumulVar(index)). As of 3/2017, only non-decreasing positive cost
  // functions are supported.
  void SetCumulVarPiecewiseLinearCost(int64 index,
                                      const PiecewiseLinearFunction& cost);
  // Returns true if a piecewise linear cost has been set for a given variable
  // index.
  bool HasCumulVarPiecewiseLinearCost(int64 index) const;
  // Returns the piecewise linear cost of a cumul variable for a given variable
  // index. The returned pointer has the same validity as this class.
  const PiecewiseLinearFunction* GetCumulVarPiecewiseLinearCost(
      int64 index) const;
#endif

  // Sets a soft upper bound to the cumul variable of a given variable index. If
  // the value of the cumul variable is greater than the bound, a cost
  // proportional to the difference between this value and the bound is added to
  // the cost function of the model:
  // cumulVar <= upper_bound -> cost = 0
  // cumulVar > upper_bound -> cost = coefficient * (cumulVar - upper_bound)
  // This is also handy to model tardiness costs when the dimension represents
  // time.
  void SetCumulVarSoftUpperBound(int64 index, int64 upper_bound,
                                 int64 coefficient);
  // Returns true if a soft upper bound has been set for a given variable index.
  bool HasCumulVarSoftUpperBound(int64 index) const;
  // Returns the soft upper bound of a cumul variable for a given variable
  // index. The "hard" upper bound of the variable is returned if no soft upper
  // bound has been set.
  int64 GetCumulVarSoftUpperBound(int64 index) const;
  // Returns the cost coefficient of the soft upper bound of a cumul variable
  // for a given variable index. If no soft upper bound has been set, 0 is
  // returned.
  int64 GetCumulVarSoftUpperBoundCoefficient(int64 index) const;

  // Sets a soft lower bound to the cumul variable of a given variable index. If
  // the value of the cumul variable is less than the bound, a cost proportional
  // to the difference between this value and the bound is added to the cost
  // function of the model:
  // cumulVar > lower_bound -> cost = 0
  // cumulVar <= lower_bound -> cost = coefficient * (lower_bound - cumulVar).
  // This is also handy to model earliness costs when the dimension represents
  // time.
  // Note: Using soft lower and upper bounds or span costs together is, as of
  // 6/2014, not well supported in the sense that an optimal schedule is not
  // guaranteed.
  void SetCumulVarSoftLowerBound(int64 index, int64 lower_bound,
                                 int64 coefficient);
  // Returns true if a soft lower bound has been set for a given variable index.
  bool HasCumulVarSoftLowerBound(int64 index) const;
  // Returns the soft lower bound of a cumul variable for a given variable
  // index. The "hard" lower bound of the variable is returned if no soft lower
  // bound has been set.
  int64 GetCumulVarSoftLowerBound(int64 index) const;
  // Returns the cost coefficient of the soft lower bound of a cumul variable
  // for a given variable index. If no soft lower bound has been set, 0 is
  // returned.
  int64 GetCumulVarSoftLowerBoundCoefficient(int64 index) const;
  // Sets the breaks for a given vehicle. Breaks are represented by
  // IntervalVars. They may interrupt transits between nodes and increase
  // the value of corresponding slack variables. However a break interval cannot
  // overlap the transit interval of a node, which is
  // [CumulVar(node), CumulVar(node) + node_visit_transits[node]), i.e. the
  // break interval must either end before CumulVar(node) or start after
  // CumulVar(node) + node_visit_transits[node].
  void SetBreakIntervalsOfVehicle(std::vector<IntervalVar*> breaks, int vehicle,
                                  std::vector<int64> node_visit_transits);
#if !defined(SWIGPYTHON)
  // Returns the break intervals set by SetBreakIntervalsOfVehicle().
  const std::vector<IntervalVar*>& GetBreakIntervalsOfVehicle(
      int vehicle) const;
  // Returns the amount of visit transit set by SetBreakIntervalsOfVehicle().
  const std::vector<int64>& GetNodeVisitTransitsOfVehicle(int vehicle) const;
#endif  // !defined(SWIGPYTHON)

  // Returns the parent in the dependency tree if any or nullptr otherwise.
  const RoutingDimension* base_dimension() const { return base_dimension_; }
  // It makes sense to use the function only for self-dependent dimension.
  // For such dimensions the value of the slack of a node determines the
  // transition cost of the next transit. Provided that
  //   1. cumul[node] is fixed,
  //   2. next[node] and next[next[node]] (if exists) are fixed,
  // the value of slack[node] for which cumul[next[node]] + transit[next[node]]
  // is minimized can be found in O(1) using this function.
  int64 ShortestTransitionSlack(int64 node) const;

  // Returns the name of the dimension.
  const std::string& name() const { return name_; }

  // Accessors.
#ifndef SWIG
  const ReverseArcListGraph<int, int>& GetPrecedenceGraph() const {
    return precedence_graph_;
  }
#endif

  // Limits, in terms of maximum difference between the cumul variables, between
  // the pickup and delivery alternatives belonging to a single pickup/delivery
  // pair in the RoutingModel.
  // The indices passed to the function respectively correspond to the position
  // of the pickup in the vector of pickup alternatives, and delivery position
  // in the delivery alternatives for this pickup/delivery pair.
  // These limits should only be set when each node index appears in at most one
  // pickup/delivery pair, i.e. each pickup (delivery) index is in a single
  // pickup/delivery pair.first (pair.second).
  typedef std::function<int64(int, int)> PickupToDeliveryLimitFunction;

  void SetPickupToDeliveryLimitFunctionForPair(
      PickupToDeliveryLimitFunction limit_function, int pair_index);

  bool HasPickupToDeliveryLimits() const;
#ifndef SWIG
  int64 GetPickupToDeliveryLimitForPair(int pair_index, int pickup,
                                        int delivery) const;
#endif  // SWIG

  int64 GetSpanUpperBoundForVehicle(int vehicle) const {
    return vehicle_span_upper_bounds_[vehicle];
  }
#ifndef SWIG
  const std::vector<int64>& vehicle_span_upper_bounds() const {
    return vehicle_span_upper_bounds_;
  }
#endif  // SWIG
  int64 GetSpanCostCoefficientForVehicle(int vehicle) const {
    return vehicle_span_cost_coefficients_[vehicle];
  }
#ifndef SWIG
  const std::vector<int64>& vehicle_span_cost_coefficients() const {
    return vehicle_span_cost_coefficients_;
  }
#endif  // SWIG
  int64 global_span_cost_coefficient() const {
    return global_span_cost_coefficient_;
  }

 private:
  struct SoftBound {
    IntVar* var;
    int64 bound;
    int64 coefficient;
  };

  struct PiecewiseLinearCost {
    PiecewiseLinearCost() : var(nullptr), cost(nullptr) {}
    IntVar* var;
    std::unique_ptr<PiecewiseLinearFunction> cost;
  };

  class SelfBased {};
  RoutingDimension(RoutingModel* model, std::vector<int64> vehicle_capacities,
                   const std::string& name,
                   const RoutingDimension* base_dimension);
  RoutingDimension(RoutingModel* model, std::vector<int64> vehicle_capacities,
                   const std::string& name, SelfBased);
  void Initialize(const std::vector<int>& transit_evaluators,
                  const std::vector<int>& state_dependent_transit_evaluators,
                  int64 slack_max);
  void InitializeCumuls();
  void InitializeTransits(
      const std::vector<int>& transit_evaluators,
      const std::vector<int>& state_dependent_transit_evaluators,
      int64 slack_max);
  void InitializeTransitVariables(int64 slack_max);
  // Sets up the cost variables related to cumul soft upper bounds.
  void SetupCumulVarSoftUpperBoundCosts(
      std::vector<IntVar*>* cost_elements) const;
  // Sets up the cost variables related to cumul soft lower bounds.
  void SetupCumulVarSoftLowerBoundCosts(
      std::vector<IntVar*>* cost_elements) const;
  void SetupCumulVarPiecewiseLinearCosts(
      std::vector<IntVar*>* cost_elements) const;
  // Sets up the cost variables related to the global span and per-vehicle span
  // costs (only for the "slack" part of the latter).
  void SetupGlobalSpanCost(std::vector<IntVar*>* cost_elements) const;
  void SetupSlackAndDependentTransitCosts(
      std::vector<IntVar*>* cost_elements) const;
  // Finalize the model of the dimension.
  void CloseModel(bool use_light_propagation);

  std::vector<IntVar*> cumuls_;
  std::vector<SortedDisjointIntervalList> forbidden_intervals_;
  std::vector<IntVar*> capacity_vars_;
  const std::vector<int64> vehicle_capacities_;
  std::vector<IntVar*> transits_;
  std::vector<IntVar*> fixed_transits_;
  // Values in class_evaluators_ correspond to the evaluators in
  // RoutingModel::transit_evaluators_ for each vehicle class.
  std::vector<int> class_evaluators_;
  std::vector<int64> vehicle_to_class_;
#ifndef SWIG
  ReverseArcListGraph<int, int> precedence_graph_;
#endif

  // The transits of a dimension may depend on its cumuls or the cumuls of
  // another dimension. There can be no cycles, except for self loops, a typical
  // example for this is a time dimension.
  const RoutingDimension* const base_dimension_;

  // Values in state_dependent_class_evaluators_ correspond to the evaluators in
  // RoutingModel::state_dependent_transit_evaluators_ for each vehicle class.
  std::vector<int> state_dependent_class_evaluators_;
  std::vector<int64> state_dependent_vehicle_to_class_;

  // For each pickup/delivery pair_index for which limits have been set,
  // pickup_to_delivery_limits_per_pair_index_[pair_index] contains the
  // PickupToDeliveryLimitFunction for the pickup and deliveries in this pair.
  std::vector<PickupToDeliveryLimitFunction>
      pickup_to_delivery_limits_per_pair_index_;

  // Used if some vehicle has breaks in this dimension, typically time.
  // clang-format off
  std::vector<std::vector<IntervalVar*> > vehicle_break_intervals_;
  std::vector<std::vector<int64> > vehicle_node_visit_transits_;
  // clang-format on

  std::vector<IntVar*> slacks_;
  std::vector<IntVar*> dependent_transits_;
  std::vector<int64> vehicle_span_upper_bounds_;
  int64 global_span_cost_coefficient_;
  std::vector<int64> vehicle_span_cost_coefficients_;
  std::vector<SoftBound> cumul_var_soft_upper_bound_;
  std::vector<SoftBound> cumul_var_soft_lower_bound_;
  std::vector<PiecewiseLinearCost> cumul_var_piecewise_linear_cost_;
  RoutingModel* const model_;
  const std::string name_;

  friend class RoutingModel;
  friend class RoutingModelInspector;

  DISALLOW_COPY_AND_ASSIGN(RoutingDimension);
};

#ifndef SWIG
// Class to arrange indices by by their distance and their angles from the
// depot. Used in the Sweep first solution heuristic.
class SweepArranger {
 public:
  explicit SweepArranger(const std::vector<std::pair<int64, int64>>& points);
  virtual ~SweepArranger() {}
  void ArrangeIndices(std::vector<int64>* indices);
  void SetSectors(int sectors) { sectors_ = sectors; }

 private:
  std::vector<int> coordinates_;
  int sectors_;

  DISALLOW_COPY_AND_ASSIGN(SweepArranger);
};
#endif

// A decision builder which tries to assign values to variables as close as
// possible to target values first.
DecisionBuilder* MakeSetValuesFromTargets(Solver* solver,
                                          std::vector<IntVar*> variables,
                                          std::vector<int64> targets);

// Routing Search

// Decision builders building a solution using local search filters to evaluate
// its feasibility. This is very fast but can eventually fail when the solution
// is restored if filters did not detect all infeasiblities.
// More details:
// Using local search filters to build a solution. The approach is pretty
// straight-forward: have a general assignment storing the current solution,
// build delta assigment representing possible extensions to the current
// solution and validate them with filters.
// The tricky bit comes from using the assignment and filter APIs in a way
// which avoids the lazy creation of internal hash_maps between variables
// and indices.

// Generic filter-based decision builder applied to IntVars.
// TODO(user): Eventually move this to the core CP solver library
// when the code is mature enough.
class IntVarFilteredDecisionBuilder : public DecisionBuilder {
 public:
  IntVarFilteredDecisionBuilder(Solver* solver,
                                const std::vector<IntVar*>& vars,
                                const std::vector<LocalSearchFilter*>& filters);
  ~IntVarFilteredDecisionBuilder() override {}
  Decision* Next(Solver* solver) override;
  // Virtual method to redefine to build a solution.
  virtual bool BuildSolution() = 0;
  // Returns statistics on search, number of decisions sent to filters, number
  // of decisions rejected by filters.
  int64 number_of_decisions() const { return number_of_decisions_; }
  int64 number_of_rejects() const { return number_of_rejects_; }

 protected:
  // Commits the modifications to the current solution if these modifications
  // are "filter-feasible", returns false otherwise; in any case discards
  // all modifications.
  bool Commit();
  // Returns true if the search must be stopped.
  virtual bool StopSearch() { return false; }
  // Modifies the current solution by setting the variable of index 'index' to
  // value 'value'.
  void SetValue(int64 index, int64 value) {
    if (!is_in_delta_[index]) {
      delta_->FastAdd(vars_[index])->SetValue(value);
      delta_indices_.push_back(index);
      is_in_delta_[index] = true;
    } else {
      delta_->SetValue(vars_[index], value);
    }
  }
  // Returns the value of the variable of index 'index' in the last committed
  // solution.
  int64 Value(int64 index) const {
    return assignment_->IntVarContainer().Element(index).Value();
  }
  // Returns true if the variable of index 'index' is in the current solution.
  bool Contains(int64 index) const {
    return assignment_->IntVarContainer().Element(index).Var() != nullptr;
  }
  // Returns the number of variables the decision builder is trying to
  // instantiate.
  int Size() const { return vars_.size(); }
  // Returns the variable of index 'index'.
  IntVar* Var(int64 index) const { return vars_[index]; }

 private:
  // Synchronizes filters with an assignment (the current solution).
  void SynchronizeFilters();
  // Checks if filters accept a given modification to the current solution
  // (represented by delta).
  bool FilterAccept();

  const std::vector<IntVar*> vars_;
  Assignment* const assignment_;
  Assignment* const delta_;
  std::vector<int> delta_indices_;
  std::vector<bool> is_in_delta_;
  const Assignment* const empty_;
  LocalSearchFilterManager filter_manager_;
  // Stats on search
  int64 number_of_decisions_;
  int64 number_of_rejects_;
};

// Filter-based decision builder dedicated to routing.
class RoutingFilteredDecisionBuilder : public IntVarFilteredDecisionBuilder {
 public:
  RoutingFilteredDecisionBuilder(
      RoutingModel* model, const std::vector<LocalSearchFilter*>& filters);
  ~RoutingFilteredDecisionBuilder() override {}
  RoutingModel* model() const { return model_; }
  // Initializes the current solution with empty or partial vehicle routes.
  bool InitializeRoutes();
  // Returns the end of the start chain of vehicle,
  int GetStartChainEnd(int vehicle) const { return start_chain_ends_[vehicle]; }
  // Returns the start of the end chain of vehicle,
  int GetEndChainStart(int vehicle) const { return end_chain_starts_[vehicle]; }
  // Make nodes in the same disjunction as 'node' unperformed. 'node' is a
  // variable index corresponding to a node.
  void MakeDisjunctionNodesUnperformed(int64 node);
  // Make all unassigned nodes unperformed.
  void MakeUnassignedNodesUnperformed();

 protected:
  bool StopSearch() override { return model_->CheckLimit(); }

 private:
  RoutingModel* const model_;
  std::vector<int64> start_chain_ends_;
  std::vector<int64> end_chain_starts_;
};

class CheapestInsertionFilteredDecisionBuilder
    : public RoutingFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  CheapestInsertionFilteredDecisionBuilder(
      RoutingModel* model, std::function<int64(int64, int64, int64)> evaluator,
      std::function<int64(int64)> penalty_evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  ~CheapestInsertionFilteredDecisionBuilder() override {}

 protected:
  typedef std::pair<int64, int64> ValuedPosition;
  struct StartEndValue {
    int64 distance;
    int vehicle;

    bool operator<(const StartEndValue& other) const {
      return std::tie(distance, vehicle) <
             std::tie(other.distance, other.vehicle);
    }
  };
  typedef std::pair<StartEndValue, /*seed_node*/ int> Seed;

  // Computes and returns the distance of each uninserted node to every vehicle
  // in "vehicles" as a std::vector<std::vector<StartEndValue>>,
  // start_end_distances_per_node.
  // For each node, start_end_distances_per_node[node] is sorted in decreasing
  // order.
  // clang-format off
  std::vector<std::vector<StartEndValue> >
      ComputeStartEndDistanceForVehicles(const std::vector<int>& vehicles);

  // Initializes the priority_queue by inserting the best entry corresponding
  // to each node, i.e. the last element of start_end_distances_per_node[node],
  // which is supposed to be sorted in decreasing order.
  // Queue is a priority queue containing Seeds.
  template <class Queue>
  void InitializePriorityQueue(
      std::vector<std::vector<StartEndValue> >* start_end_distances_per_node,
      Queue* priority_queue);
  // clang-format on

  // Inserts 'node' just after 'predecessor', and just before 'successor',
  // resulting in the following subsequence: predecessor -> node -> successor.
  // If 'node' is part of a disjunction, other nodes of the disjunction are made
  // unperformed.
  void InsertBetween(int64 node, int64 predecessor, int64 successor);
  // Helper method to the ComputeEvaluatorSortedPositions* methods. Finds all
  // possible insertion positions of node 'node_to_insert' in the partial route
  // starting at node 'start' and adds them to 'valued_position', a list of
  // unsorted pairs of (cost, position to insert the node).
  void AppendEvaluatedPositionsAfter(
      int64 node_to_insert, int64 start, int64 next_after_start, int64 vehicle,
      std::vector<ValuedPosition>* valued_positions);
  // Returns the cost of unperforming node 'node_to_insert'. Returns kint64max
  // if penalty callback is null or if the node cannot be unperformed.
  int64 GetUnperformedValue(int64 node_to_insert) const;

  std::function<int64(int64, int64, int64)> evaluator_;
  std::function<int64(int64)> penalty_evaluator_;
};

// Filter-based decision builder which builds a solution by inserting
// nodes at their cheapest position on any route; potentially several routes can
// be built in parallel. The cost of a position is computed from an arc-based
// cost callback. The node selected for insertion is the one which minimizes
// insertion cost. If a non null penalty evaluator is passed, making nodes
// unperformed is also taken into account with the corresponding penalty cost.
class GlobalCheapestInsertionFilteredDecisionBuilder
    : public CheapestInsertionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluators.
  GlobalCheapestInsertionFilteredDecisionBuilder(
      RoutingModel* model, std::function<int64(int64, int64, int64)> evaluator,
      std::function<int64(int64)> penalty_evaluator,
      const std::vector<LocalSearchFilter*>& filters, bool is_sequential,
      double farthest_seeds_ratio, double neighbors_ratio);
  ~GlobalCheapestInsertionFilteredDecisionBuilder() override {}
  bool BuildSolution() override;

 private:
  class PairEntry;
  class NodeEntry;
  typedef std::unordered_set<PairEntry*> PairEntries;
  typedef std::unordered_set<NodeEntry*> NodeEntries;

  // Inserts all non-inserted pickup and delivery pairs. Maintains a priority
  // queue of possible pair insertions, which is incrementally updated when a
  // pair insertion is committed. Incrementality is obtained by updating pair
  // insertion positions on the four newly modified route arcs: after the pickup
  // insertion position, after the pickup position, after the delivery insertion
  // position and after the delivery position.
  void InsertPairs();

  // Inserts non-inserted individual nodes on the given routes (or all routes if
  // "vehicles" is an empty vector), by constructing routes in parallel.
  // Maintains a priority queue of possible insertions, which is incrementally
  // updated when an insertion is committed.
  // Incrementality is obtained by updating insertion positions on the two newly
  // modified route arcs: after the node insertion position and after the node
  // position.
  void InsertNodesOnRoutes(const std::vector<int>& nodes,
                           const std::vector<int>& vehicles);

  // Inserts non-inserted individual nodes on routes by constructing routes
  // sequentially.
  // For each new route, the vehicle to use and the first node to insert on it
  // are given by calling InsertSeedNode(). The route is then completed with
  // other nodes by calling InsertNodesOnRoutes({vehicle}).
  void SequentialInsertNodes(const std::vector<int>& nodes);

  // Goes through all vehicles in the model to check if they are already used
  // (i.e. Value(start) != end) or not.
  // Updates the three passed vectors accordingly.
  void DetectUsedVehicles(std::vector<bool>* is_vehicle_used,
                          std::vector<int>* used_vehicles,
                          std::vector<int>* unused_vehicles);

  // Inserts the (farthest_seeds_ratio_ * model()->vehicles()) nodes farthest
  // from the start/ends of the available vehicle routes as seeds on their
  // closest route.
  void InsertFarthestNodesAsSeeds();

  // Inserts a "seed node" based on the given priority_queue of Seeds.
  // A "seed" is the node used in order to start a new route.
  // If the Seed at the top of the priority queue cannot be inserted,
  // (node already inserted in the model, corresponding vehicle already used, or
  // unsuccessful Commit()), start_end_distances_per_node is updated and used
  // to insert a new entry for that node if necessary (next best vehicle).
  // If a seed node is successfully inserted, updates is_vehicle_used and
  // returns the vehice of the corresponding route. Returns -1 otherwise.
  template <class Queue>
  int InsertSeedNode(
      std::vector<std::vector<StartEndValue>>* start_end_distances_per_node,
      Queue* priority_queue, std::vector<bool>* is_vehicle_used);
  // clang-format on

  // Initializes the priority queue and the pair entries with the current state
  // of the solution.
  void InitializePairPositions(
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  // Updates all pair entries inserting a node after node "insert_after" and
  // updates the priority queue accordingly.
  void UpdatePairPositions(int vehicle, int64 insert_after,
                           AdjustablePriorityQueue<PairEntry>* priority_queue,
                           std::vector<PairEntries>* pickup_to_entries,
                           std::vector<PairEntries>* delivery_to_entries) {
    UpdatePickupPositions(vehicle, insert_after, priority_queue,
                          pickup_to_entries, delivery_to_entries);
    UpdateDeliveryPositions(vehicle, insert_after, priority_queue,
                            pickup_to_entries, delivery_to_entries);
  }
  // Updates all pair entries inserting their pickup node after node
  // "insert_after" and updates the priority queue accordingly.
  void UpdatePickupPositions(int vehicle, int64 pickup_insert_after,
                             AdjustablePriorityQueue<PairEntry>* priority_queue,
                             std::vector<PairEntries>* pickup_to_entries,
                             std::vector<PairEntries>* delivery_to_entries);
  // Updates all pair entries inserting their delivery node after node
  // "insert_after" and updates the priority queue accordingly.
  void UpdateDeliveryPositions(
      int vehicle, int64 delivery_insert_after,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  // Deletes an entry, removing it from the priority queue and the appropriate
  // pickup and delivery entry sets.
  void DeletePairEntry(PairEntry* entry,
                       AdjustablePriorityQueue<PairEntry>* priority_queue,
                       std::vector<PairEntries>* pickup_to_entries,
                       std::vector<PairEntries>* delivery_to_entries);
  // Initializes the priority queue and the node entries with the current state
  // of the solution on the given vehicle routes.
  void InitializePositions(const std::vector<int>& nodes,
                           AdjustablePriorityQueue<NodeEntry>* priority_queue,
                           std::vector<NodeEntries>* position_to_node_entries,
                           const std::vector<int>& vehicles);
  // Updates all node entries inserting a node after node "insert_after" and
  // updates the priority queue accordingly.
  void UpdatePositions(const std::vector<int>& nodes, int vehicle,
                       int64 insert_after,
                       AdjustablePriorityQueue<NodeEntry>* priority_queue,
                       std::vector<NodeEntries>* node_entries);
  // Deletes an entry, removing it from the priority queue and the appropriate
  // node entry sets.
  void DeleteNodeEntry(NodeEntry* entry,
                       AdjustablePriorityQueue<NodeEntry>* priority_queue,
                       std::vector<NodeEntries>* node_entries);

  // Inserts neighbor_index in
  // node_index_to_[pickup|delivery|single]_neighbors_per_cost_class_
  // [node_index][cost_class] according to whether neighbor is a pickup,
  // a delivery, or neither.
  void AddNeighborForCostClass(int cost_class, int64 node_index,
                               int64 neighbor_index, bool neighbor_is_pickup,
                               bool neighbor_is_delivery);

  // Returns true iff neighbor_index is in node_index's neighbors list
  // corresponding to neighbor_is_pickup and neighbor_is_delivery.
  bool IsNeighborForCostClass(int cost_class, int64 node_index,
                              int64 neighbor_index) const;

  // Returns a reference to the set of pickup neighbors of node_index.
  const absl::flat_hash_set<int64>& GetPickupNeighborsOfNodeForCostClass(
      int cost_class, int64 node_index) {
    if (neighbors_ratio_ == 1) {
      return pickup_nodes_;
    }
    return node_index_to_pickup_neighbors_by_cost_class_[node_index]
                                                        [cost_class];
  }

  // Same as above for delivery neighbors.
  const absl::flat_hash_set<int64>& GetDeliveryNeighborsOfNodeForCostClass(
      int cost_class, int64 node_index) {
    if (neighbors_ratio_ == 1) {
      return delivery_nodes_;
    }
    return node_index_to_delivery_neighbors_by_cost_class_[node_index]
                                                          [cost_class];
  }

  const bool is_sequential_;
  const double farthest_seeds_ratio_;
  const double neighbors_ratio_;

  // clang-format off
  std::vector<std::vector<absl::flat_hash_set<int64> > >
      node_index_to_single_neighbors_by_cost_class_;
  std::vector<std::vector<absl::flat_hash_set<int64> > >
      node_index_to_pickup_neighbors_by_cost_class_;
  std::vector<std::vector<absl::flat_hash_set<int64> > >
      node_index_to_delivery_neighbors_by_cost_class_;
  // clang-format on

  // When neighbors_ratio is 1, we don't compute the neighborhood members above,
  // and use the following sets in the code to avoid unnecessary computations
  // and decrease the time and space complexities.
  absl::flat_hash_set<int64> pickup_nodes_;
  absl::flat_hash_set<int64> delivery_nodes_;
};

// Filter-base decision builder which builds a solution by inserting
// nodes at their cheapest position. The cost of a position is computed
// an arc-based cost callback. Node selected for insertion are considered in
// decreasing order of distance to the start/ends of the routes, i.e. farthest
// nodes are inserted first.
class LocalCheapestInsertionFilteredDecisionBuilder
    : public CheapestInsertionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  LocalCheapestInsertionFilteredDecisionBuilder(
      RoutingModel* model, std::function<int64(int64, int64, int64)> evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  ~LocalCheapestInsertionFilteredDecisionBuilder() override {}
  bool BuildSolution() override;

 private:
  // Computes the possible insertion positions of 'node' and sorts them
  // according to the current cost evaluator.
  // 'node' is a variable index corresponding to a node, 'sorted_positions' is a
  // vector of variable indices corresponding to nodes after which 'node' can be
  // inserted.
  void ComputeEvaluatorSortedPositions(int64 node,
                                       std::vector<int64>* sorted_positions);
  // Like ComputeEvaluatorSortedPositions, subject to the additional
  // restrictions that the node may only be inserted after node 'start' on the
  // route. For convenience, this method also needs the node that is right after
  // 'start' on the route.
  void ComputeEvaluatorSortedPositionsOnRouteAfter(
      int64 node, int64 start, int64 next_after_start,
      std::vector<int64>* sorted_positions);
};

// Filtered-base decision builder based on the addition heuristic, extending
// a path from its start node with the cheapest arc.
class CheapestAdditionFilteredDecisionBuilder
    : public RoutingFilteredDecisionBuilder {
 public:
  CheapestAdditionFilteredDecisionBuilder(
      RoutingModel* model, const std::vector<LocalSearchFilter*>& filters);
  ~CheapestAdditionFilteredDecisionBuilder() override {}
  bool BuildSolution() override;

 private:
  class PartialRoutesAndLargeVehicleIndicesFirst {
   public:
    explicit PartialRoutesAndLargeVehicleIndicesFirst(
        const CheapestAdditionFilteredDecisionBuilder& builder)
        : builder_(builder) {}
    bool operator()(int vehicle1, int vehicle2) const;

   private:
    const CheapestAdditionFilteredDecisionBuilder& builder_;
  };
  // Returns a vector of possible next indices of node from an iterator.
  template <typename Iterator>
  std::vector<int64> GetPossibleNextsFromIterator(int64 node, Iterator start,
                                                  Iterator end) const {
    const int size = model()->Size();
    std::vector<int64> nexts;
    for (Iterator it = start; it != end; ++it) {
      const int64 next = *it;
      if (next != node && (next >= size || !Contains(next))) {
        nexts.push_back(next);
      }
    }
    return nexts;
  }
  // Sorts a vector of successors of node.
  virtual void SortSuccessors(int64 node, std::vector<int64>* successors) = 0;
  virtual int64 FindTopSuccessor(int64 node,
                                 const std::vector<int64>& successors) = 0;
};

// A CheapestAdditionFilteredDecisionBuilder where the notion of 'cheapest arc'
// comes from an arc evaluator.
class EvaluatorCheapestAdditionFilteredDecisionBuilder
    : public CheapestAdditionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  EvaluatorCheapestAdditionFilteredDecisionBuilder(
      RoutingModel* model, std::function<int64(int64, int64)> evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  ~EvaluatorCheapestAdditionFilteredDecisionBuilder() override {}

 private:
  // Next nodes are sorted according to the current evaluator.
  void SortSuccessors(int64 node, std::vector<int64>* successors) override;
  int64 FindTopSuccessor(int64 node,
                         const std::vector<int64>& successors) override;

  std::function<int64(int64, int64)> evaluator_;
};

// A CheapestAdditionFilteredDecisionBuilder where the notion of 'cheapest arc'
// comes from an arc comparator.
class ComparatorCheapestAdditionFilteredDecisionBuilder
    : public CheapestAdditionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  ComparatorCheapestAdditionFilteredDecisionBuilder(
      RoutingModel* model, Solver::VariableValueComparator comparator,
      const std::vector<LocalSearchFilter*>& filters);
  ~ComparatorCheapestAdditionFilteredDecisionBuilder() override {}

 private:
  // Next nodes are sorted according to the current comparator.
  void SortSuccessors(int64 node, std::vector<int64>* successors) override;
  int64 FindTopSuccessor(int64 node,
                         const std::vector<int64>& successors) override;

  Solver::VariableValueComparator comparator_;
};

// Filter-based decision builder which builds a solution by using
// Clarke & Wright's Savings heuristic. For each pair of nodes, the savings
// value is the difference between the cost of two routes visiting one node each
// and one route visiting both nodes. Routes are built sequentially, each route
// being initialized from the pair with the best avalaible savings value then
// extended by selecting the nodes with best savings on both ends of the partial
// route.
// Cost is based on the arc cost function of the routing model and cost classes
// are taken into account.
class SavingsFilteredDecisionBuilder : public RoutingFilteredDecisionBuilder {
 public:
  // If savings_neighbors_ratio > 0 then for each node only this ratio of its
  // neighbors leading to the smallest arc costs are considered.
  // Furthermore, if add_reverse_arcs is true, the neighborhood relationships
  // are always considered symmetrically.
  // Finally, savings_arc_coefficient is a strictly positive parameter
  // indicating the coefficient of the arc being considered in the saving
  // formula.
  // TODO(user): Add all parameters as struct to the class.
  SavingsFilteredDecisionBuilder(
      RoutingModel* model, RoutingIndexManager* manager,
      double savings_neighbors_ratio, bool add_reverse_arcs,
      double savings_arc_coefficient,
      const std::vector<LocalSearchFilter*>& filters);
  ~SavingsFilteredDecisionBuilder() override;
  bool BuildSolution() override;

 protected:
  typedef std::pair</*saving*/ int64, /*saving index*/ int64> Saving;

  template <typename S>
  class SavingsContainer;

  struct VehicleClassEntry {
    int vehicle_class;
    int64 fixed_cost;

    bool operator<(const VehicleClassEntry& other) const {
      return std::tie(fixed_cost, vehicle_class) <
             std::tie(other.fixed_cost, other.vehicle_class);
    }
  };

  virtual void BuildRoutesFromSavings() = 0;

  // Returns the cost class from a saving.
  int64 GetVehicleTypeFromSaving(const Saving& saving) const {
    return saving.second / size_squared_;
  }
  // Returns the "before node" from a saving.
  int64 GetBeforeNodeFromSaving(const Saving& saving) const {
    return (saving.second % size_squared_) / Size();
  }
  // Returns the "after node" from a saving.
  int64 GetAfterNodeFromSaving(const Saving& saving) const {
    return (saving.second % size_squared_) % Size();
  }
  // Returns the saving value from a saving.
  int64 GetSavingValue(const Saving& saving) const { return saving.first; }

  // Finds the best available vehicle of type "type" to start a new route to
  // serve the arc before_node-->after_node.
  // Since there are different vehicle classes for each vehicle type, each
  // vehicle class having its own capacity constraints, we go through all
  // vehicle types (in each case only studying the first available vehicle) to
  // make sure this Saving is inserted if possible.
  // If possible, the arc is committed to the best vehicle, and the vehicle
  // index is returned. If this arc can't be served by any vehicle of this type,
  // the function returns -1.
  int StartNewRouteWithBestVehicleOfType(int type, int64 before_node,
                                         int64 after_node);

  std::vector<int> type_index_of_vehicle_;
  // clang-format off
  std::vector<std::set<VehicleClassEntry> > sorted_vehicle_classes_per_type_;
  std::vector<std::deque<int> > vehicles_per_vehicle_class_;
  std::unique_ptr<SavingsContainer<Saving> > savings_container_;
  // clang-format on

 private:
  // Used when add_reverse_arcs_ is true.
  // Given the vector of adjacency lists of a graph, adds symetric arcs not
  // already in the graph to the adjacencies (i.e. if n1-->n2 is present and not
  // n2-->n1, then n1 is added to adjacency_matrix[n2].
  // clang-format off
  void AddSymetricArcsToAdjacencyLists(
      std::vector<std::vector<int64> >* adjacency_lists);
  // clang-format on

  // Computes saving values for all node pairs and vehicle types (see
  // ComputeVehicleTypes()).
  // The saving index attached to each saving value is an index used to
  // store and recover the node pair to which the value is linked (cf. the
  // index conversion methods below).
  // The computed savings are stored and sorted using the savings_container_.
  void ComputeSavings();
  // Builds a saving from a saving value, a vehicle type and two nodes.
  Saving BuildSaving(int64 saving, int vehicle_type, int before_node,
                     int after_node) const {
    return std::make_pair(saving, vehicle_type * size_squared_ +
                                      before_node * Size() + after_node);
  }

  // Computes the vehicle type of every vehicle and stores it in
  // type_index_of_vehicle_. A "vehicle type" consists of the set of vehicles
  // having the same cost class and start/end nodes, therefore the same savings
  // value for each arc.
  // The vehicle classes corresponding to each vehicle type index are stored and
  // sorted by fixed cost in sorted_vehicle_classes_per_type_, and the vehicles
  // for each vehicle class are stored in vehicles_per_vehicle_class_.
  void ComputeVehicleTypes();

  RoutingIndexManager* const manager_;
  const double savings_neighbors_ratio_;
  const bool add_reverse_arcs_;
  const double savings_arc_coefficient_;
  int64 size_squared_;
};

class SequentialSavingsFilteredDecisionBuilder
    : public SavingsFilteredDecisionBuilder {
 public:
  SequentialSavingsFilteredDecisionBuilder(
      RoutingModel* model, RoutingIndexManager* manager,
      double savings_neighbors_ratio, bool add_reverse_arcs,
      double savings_arc_coefficient,
      const std::vector<LocalSearchFilter*>& filters)
      : SavingsFilteredDecisionBuilder(model, manager, savings_neighbors_ratio,
                                       add_reverse_arcs,
                                       savings_arc_coefficient, filters) {}
  ~SequentialSavingsFilteredDecisionBuilder() override{};

 private:
  // Builds routes sequentially.
  // Once a Saving is used to start a new route, we extend this route as much as
  // possible from both ends by gradually inserting the best Saving at either
  // end of the route.
  void BuildRoutesFromSavings() override;
};

class ParallelSavingsFilteredDecisionBuilder
    : public SavingsFilteredDecisionBuilder {
 public:
  ParallelSavingsFilteredDecisionBuilder(
      RoutingModel* model, RoutingIndexManager* manager,
      double savings_neighbors_ratio, bool add_reverse_arcs,
      double savings_arc_coefficient,
      const std::vector<LocalSearchFilter*>& filters)
      : SavingsFilteredDecisionBuilder(model, manager, savings_neighbors_ratio,
                                       add_reverse_arcs,
                                       savings_arc_coefficient, filters) {}
  ~ParallelSavingsFilteredDecisionBuilder() override{};

 private:
  // Goes through the ordered computed Savings to build routes in parallel.
  // Given a Saving for a before-->after arc :
  // -- If both before and after are uncontained, we start a new route.
  // -- If only before is served and is the last node on its route, we try
  //    adding after at the end of the route.
  // -- If only after is served and is first on its route, we try adding before
  //    as first node on this route.
  // -- If both nodes are contained and are respectively the last and first
  //    nodes on their (different) routes, we merge the routes of the two nodes
  //    into one if possible.
  void BuildRoutesFromSavings() override;

  // Merges the routes of first_vehicle and second_vehicle onto the vehicle with
  // lower fixed cost. The routes respectively end at before_node and start at
  // after_node, and are merged into one by adding the arc
  // before_node-->after_node.
  void MergeRoutes(int first_vehicle, int second_vehicle, int64 before_node,
                   int64 after_node);

  // First and last non start/end nodes served by each vehicle.
  std::vector<int64> first_node_on_route_;
  std::vector<int64> last_node_on_route_;
  // For each first/last node served by a vehicle (besides start/end nodes of
  // vehicle), this vector contains the index of the vehicle serving them.
  // For other (intermediary) nodes, contains -1.
  std::vector<int> vehicle_of_first_or_last_node_;
};

// Christofides addition heuristic. Initially created to solve TSPs, extended to
// support any model by extending routes as much as possible following the path
// found by the heuristic, before starting a new route.

class ChristofidesFilteredDecisionBuilder
    : public RoutingFilteredDecisionBuilder {
 public:
  ChristofidesFilteredDecisionBuilder(
      RoutingModel* model, const std::vector<LocalSearchFilter*>& filters);
  ~ChristofidesFilteredDecisionBuilder() override {}
  bool BuildSolution() override;
};

// Generic path-based filter class.

class BasePathFilter : public IntVarLocalSearchFilter {
 public:
  BasePathFilter(const std::vector<IntVar*>& nexts, int next_domain_size,
                 std::function<void(int64)> objective_callback);
  ~BasePathFilter() override {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta) override;
  void OnSynchronize(const Assignment* delta) override;

 protected:
  static const int64 kUnassigned;

  int64 GetNext(int64 node) const {
    return (new_nexts_[node] == kUnassigned)
               ? (IsVarSynced(node) ? Value(node) : kUnassigned)
               : new_nexts_[node];
  }
  int NumPaths() const { return starts_.size(); }
  int64 Start(int i) const { return starts_[i]; }
  int GetPath(int64 node) const { return paths_[node]; }
  int Rank(int64 node) const { return ranks_[node]; }
  bool IsDisabled() const { return status_ == DISABLED; }

 private:
  enum Status { UNKNOWN, ENABLED, DISABLED };

  virtual bool DisableFiltering() const { return false; }
  virtual void OnBeforeSynchronizePaths() {}
  virtual void OnAfterSynchronizePaths() {}
  virtual void OnSynchronizePathFromStart(int64 start) {}
  virtual void InitializeAcceptPath() {}
  virtual bool AcceptPath(int64 path_start, int64 chain_start,
                          int64 chain_end) = 0;
  virtual bool FinalizeAcceptPath(const Assignment* delta) { return true; }
  // Detects path starts, used to track which node belongs to which path.
  void ComputePathStarts(std::vector<int64>* path_starts,
                         std::vector<int>* index_to_path);
  bool HavePathsChanged();
  void SynchronizeFullAssignment();
  void UpdateAllRanks();
  void UpdatePathRanksFromStart(int start);

  std::vector<int64> node_path_starts_;
  std::vector<int64> starts_;
  std::vector<int> paths_;
  std::vector<int64> new_nexts_;
  std::vector<int> delta_touched_;
  SparseBitset<> touched_paths_;
  SparseBitset<> touched_path_nodes_;
  std::vector<int> ranks_;

  Status status_;
};

// This filter accepts deltas for which the assignment satisfies the constraints
// of the Solver. This is verified by keeping an internal copy of the assignment
// with all Next vars and their updated values, and calling RestoreAssignment()
// on the assignment+delta.
// TODO(user): Also call the solution finalizer on variables, with the
// exception of Next Vars (woud fail on large instances).
// WARNING: In the case of mandatory nodes, when all vehicles are currently
// being used in the solution but uninserted nodes still remain, this filter
// will reject the solution, even if the node could be inserted on one of these
// routes, because all Next vars of vehicle starts are already instantiated.
// TODO(user): Avoid such false negatives.
class CPFeasibilityFilter : public IntVarLocalSearchFilter {
 public:
  explicit CPFeasibilityFilter(const RoutingModel* routing_model);
  ~CPFeasibilityFilter() override {}
  std::string DebugString() const override { return "CPFeasibilityFilter"; }
  bool Accept(const Assignment* delta, const Assignment* deltadelta) override;
  void OnSynchronize(const Assignment* delta) override;

 private:
  void AddDeltaToAssignment(const Assignment* delta, Assignment* assignment);

  static const int64 kUnassigned;
  const RoutingModel* const model_;
  Solver* const solver_;
  Assignment* const assignment_;
  Assignment* const temp_assignment_;
  DecisionBuilder* const restore_;
};

#if !defined(SWIG)
IntVarLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model,
    std::function<void(int64)> objective_callback);
IntVarLocalSearchFilter* MakeVehicleAmortizedCostFilter(
    const RoutingModel& routing_model,
    Solver::ObjectiveWatcher objective_callback);
IntVarLocalSearchFilter* MakeTypeIncompatibilityFilter(
    const RoutingModel& routing_model);
IntVarLocalSearchFilter* MakePathCumulFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension,
    std::function<void(int64)> objective_callback);
IntVarLocalSearchFilter* MakePickupDeliveryFilter(
    const RoutingModel& routing_model, const RoutingModel::IndexPairs& pairs,
    const std::vector<RoutingModel::PickupAndDeliveryPolicy>& vehicle_policies);
IntVarLocalSearchFilter* MakeVehicleVarFilter(
    const RoutingModel& routing_model);
IntVarLocalSearchFilter* MakeVehicleBreaksFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension);
IntVarLocalSearchFilter* MakeCPFeasibilityFilter(
    const RoutingModel* routing_model);

// Utility class used in RouteDimensionCumulOptimizer to set the LP constraints
// and solve the problem.
class DimensionCumulOptimizerCore {
 public:
  explicit DimensionCumulOptimizerCore(const RoutingDimension* dimension)
      : dimension_(dimension) {}

  bool OptimizeSingleRoute(int vehicle,
                           const std::function<int64(int64)>& next_accessor,
                           glop::LinearProgram* linear_program,
                           glop::LPSolver* lp_solver,
                           std::vector<int64>* cumul_values, int64* cost,
                           int64* transit_cost);

  const RoutingDimension* dimension() const { return dimension_; }

 private:
  void SetRouteCumulConstraints(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      glop::LinearProgram* linear_program, int64* route_transit_cost);

  bool FinalizeAndSolve(glop::LinearProgram* linear_program,
                        glop::LPSolver* lp_solver,
                        std::vector<int64>* cumul_values, int64* cost);
  const RoutingDimension* const dimension_;
  std::vector<glop::ColIndex> current_route_cumul_variables_;
};

// Class used to compute optimal values for dimension cumuls of routes,
// minimizing cumul soft lower and upper bound costs, and vehicle span costs of
// a route.
// In its methods, next_accessor is a callback returning the next node of a
// given node on a route.
class RouteDimensionCumulOptimizer {
 public:
  explicit RouteDimensionCumulOptimizer(const RoutingDimension* dimension);
  // If feasible, computes the optimal cost of the route performed by a vehicle,
  // minimizing cumul soft lower and upper bound costs and vehicle span costs,
  // and stores it in "optimal_cost" (if not null).
  // Returns true iff the route respects all constraints.
  bool ComputeRouteCumulCost(int vehicle,
                             const std::function<int64(int64)>& next_accessor,
                             int64* optimal_cost);
  // Same as ComputeRouteCumulCost, but the cost computed does not contain
  // the part of the vehicle span cost due to fixed transits.
  bool ComputeRouteCumulCostWithoutFixedTransits(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      int64* optimal_cost_without_transits);
  // If feasible, computes the optimal cumul values of the route performed by a
  // vehicle, minimizing cumul soft lower and upper bound costs and vehicle span
  // costs, stores them in "optimal_cumuls" (if not null), and returns true.
  // Returns false if the route is not feasible.
  bool ComputeRouteCumuls(int vehicle,
                          const std::function<int64(int64)>& next_accessor,
                          std::vector<int64>* optimal_cumuls);

  const RoutingDimension* dimension() const {
    return optimizer_core_.dimension();
  }

 private:
  std::vector<std::unique_ptr<glop::LPSolver>> lp_solver_;
  std::vector<std::unique_ptr<glop::LinearProgram>> linear_program_;
  DimensionCumulOptimizerCore optimizer_core_;
};
#endif

}  // namespace operations_research
#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
