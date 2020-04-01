#include <chrono>
#include <ctime>
#include <iostream>
#include <set>
#include <vector>

#include "dg/analysis/BBlocksBuilder.h"
#include "dg/analysis/ReachingDefinitions/RDMap.h"
#include "dg/analysis/ReachingDefinitions/ReachingDefinitions.h"

#include "dg/util/debug.h"

namespace dg {
namespace analysis {
namespace rd {

RDNode UNKNOWN_MEMLOC;
RDNode *UNKNOWN_MEMORY = &UNKNOWN_MEMLOC;

void ReachingDefinitionsGraph::buildBBlocks(bool dce) {
  assert(getRoot() && "No root node");
  DBG(dda, "Building basic blocks");

  BBlocksBuilder<RDBBlock> builder;
  _bblocks = std::move(builder.buildAndGetBlocks(getRoot()));

  assert(getRoot()->getBBlock() && "Root node has no BBlock");

  // should we eliminate dead code?
  // The dead code are the nodes that have no basic block assigned
  // (follows from the DFS nature of the block builder algorithm)
  if (!dce) return;

  for (auto &nd : _nodes) {
    if (nd->getBBlock() == nullptr) {
      nd->isolate();
      nd.reset();
    }
  }
}

void ReachingDefinitionsGraph::removeUselessNodes() {}

bool ReachingDefinitionsAnalysis::processNode(RDNode *node) {
  bool changed = false;

  // merge maps from predecessors
  for (RDNode *n : node->getPredecessors()) {
    changed |= node->def_map.merge(
        &n->def_map, &node->overwrites /* strong update */,
        options.strongUpdateUnknown,
        *options.maxSetSize, /* max size of set of reaching definition
                               of one definition site */
        options.mergeUnknown /* merge unknown */);
  }

  return changed;
}

void ReachingDefinitionsAnalysis::run() {
  DBG_SECTION_BEGIN(dda, "Starting reaching definitions analysis");
  assert(getRoot() && "Do not have root");

  std::vector<RDNode *> to_process = getNodes(getRoot());
  std::vector<RDNode *> changed;

  uint64_t total_processed = 0;

  std::clock_t time_start;
  int64_t max_clock_ticks;
  if (options.timeout > 0) {
    time_start = std::clock();
    // calculate the max clock ticks, the timeout is specified in the unit
    // of milliseconds
    max_clock_ticks = options.timeout * CLOCKS_PER_SEC / 1000.0;
  }

#ifdef DEBUG_ENABLED
  int n = 0;
#endif

  // do fixpoint
  do {
#ifdef DEBUG_ENABLED
    if (n % 100 == 0) {
      DBG(dda, "Iteration " << n << ", queued " << to_process.size()
                            << " nodes");
    }
    ++n;
#endif
    unsigned last_processed_num = to_process.size();
    changed.clear();
    std::cerr << "[RD] Info: processing " << last_processed_num << " RDNodes\n";
    for (RDNode *cur : to_process) {
      if (processNode(cur)) changed.push_back(cur);
    }
    total_processed += last_processed_num;

    if (!changed.empty()) {
      to_process.clear();
      to_process = getNodes(changed /* starting set */,
                            last_processed_num /* expected num */);

      // since changed was not empty,
      // the to_process must not be empty too
      assert(!to_process.empty());
    }
    bool abort = false;
    if (options.timeout > 0) {
      // timeout limit is set, check the duration
      if (std::clock() - time_start > max_clock_ticks) {
        // timeout exceeded, if there is a max iteration limit,
        // we'll also check it. In other words, the condition is timeout
        // AND max iteration reached, instead of timeout OR max iteration.
        if (options.fixedPointThreshold > 0 &&
            total_processed > options.fixedPointThreshold) {
          abort = true;
        }
      }
    } else {
      // timeout limit is not set, check iteration
      if (options.fixedPointThreshold > 0 &&
          total_processed > options.fixedPointThreshold) {
        abort = true;
      }
    }
    if (abort) {
      std::cerr << "[RD] Warning: Processed " << total_processed
                << " RDNodes in total but has not reached fixed point, "
                   "abort remaining analysis.\n";
      break;
    }
  } while (!changed.empty());

  DBG_SECTION_END(dda, "Finished reaching definitions analysis");
}

// return the reaching definitions of ('mem', 'off', 'len')
// at the location 'where'
void ReachingDefinitionsAnalysis::getReachingDefinitions(
    RDNode *where, RDNode *mem, const Offset &off, const Offset &len,
    RDNodeSetVector &result) {
  if (mem->isUnknown()) {
    // gather all definitions of memory
    for (auto &it : where->def_map) {
      result.insert(it.second.begin(), it.second.end());
    }
  } else {
    // gather all possible definitions of the memory
    where->def_map.get(UNKNOWN_MEMORY, Offset::UNKNOWN, Offset::UNKNOWN,
                       result);
    where->def_map.get(mem, off, len, result);
  }
}

void ReachingDefinitionsAnalysis::getReachingDefinitions(
    RDNode *use, RDNodeSetVector &result) {
  // gather all possible definitions of the memory including the unknown mem
  for (auto &ds : use->uses) {
    if (ds.target->isUnknown()) {
      // gather all definitions of memory

      // Arthas CHANGES:
      //  iterate the def map based on insertion order
      auto kit = use->def_map.key_begin();
      auto kend = use->def_map.key_end();
      while (kit != kend) {
        auto &def = use->def_map[*kit];
        result.insert(def.begin(), def.end());
        ++kit;
      }
      // Arthas CHANGES:
      //  previously the def map is iterated based on sorted key order
      //
      /*
      for (auto &it : use->def_map) {
        result.insert(it.second.begin(), it.second.end());
      }
      */
      break;  // we may bail out as we added everything
    }
    // std::cerr << "use " << use << " known target " << ds.target << "\n";
    use->def_map.get(ds.target, ds.offset, ds.len, result);
  }
  use->def_map.get(UNKNOWN_MEMORY, Offset::UNKNOWN, Offset::UNKNOWN, result);
}

///
// Find the nodes that define the given def-site.
// Create PHI nodes if needed.
std::vector<RDNode *> SSAReachingDefinitionsAnalysis::findDefinitions(
    RDBBlock *block, const DefSite &ds) {
  // FIXME: the graph may contain dead code for which no blocks
  // are set (as the blocks are created only for the reachable code).
  // Removing the dead code is easy, but then we must somehow
  // adjust the mapping in the builder, which is not that straightforward.
  // Once we have that, uncomment this assertion.
  // assert(block && "Block is null");
  if (!block) return {};

  assert(ds.target && "Target is null");

  // Find known definitions.
  auto defSet = block->definitions.get(ds);
  std::vector<RDNode *> defs(defSet.begin(), defSet.end());

  // add definitions to unknown memory
  auto unknown = block->definitions.get({UNKNOWN_MEMORY, 0, Offset::UNKNOWN});
  defs.insert(defs.end(), unknown.begin(), unknown.end());

  // Find definitions that are not in this block (if any).
  auto uncovered = block->definitions.undefinedIntervals(ds);
  for (auto &interval : uncovered) {
    // if we have a unique predecessor, try finding definitions
    // and creating the new PHI nodes there.
    if (auto pred = block->getSinglePredecessor()) {
      auto pdefs = findDefinitions(pred, ds);
      defs.insert(defs.end(), pdefs.begin(), pdefs.end());
    } else {
      // Several predecessors -- we must create a PHI.
      // This phi is the definition that we are looking for.
      _phis.emplace_back(graph.create(RDNodeType::PHI));
      _phis.back()->addOverwrites(ds.target, interval.start, interval.length());
      // update definitions in the block -- this
      // phi node defines previously uncovered memory
      assert(
          block->definitions.get({ds.target, interval.start, interval.length()})
              .empty());
      block->definitions.update({ds.target, interval.start, interval.length()},
                                _phis.back());

      // Inserting at the beginning of the block should not
      // invalidate the iterator
      block->prependAndUpdateCFG(_phis.back());

      // this represents the sought definition
      defs.push_back(_phis.back());
    }
  }

  return defs;
}

///
// Find the nodes that define the given def-site.
// Create PHI nodes if needed. For LVN only.
std::vector<RDNode *> SSAReachingDefinitionsAnalysis::findDefinitionsInBlock(
    RDBBlock *block, const DefSite &ds) {
  // get defs of known definitions
  auto defSet = block->definitions.get(ds);
  std::vector<RDNode *> defs(defSet.begin(), defSet.end());

  // add definitions to unknown memory
  auto unknown = block->definitions.get({UNKNOWN_MEMORY, 0, Offset::UNKNOWN});
  defs.insert(defs.end(), unknown.begin(), unknown.end());

  // find out which bytes are not covered yet
  // and create phi nodes for these intervals
  auto uncovered = block->definitions.undefinedIntervals(ds);
  for (auto &interval : uncovered) {
    _phis.emplace_back(graph.create(RDNodeType::PHI));
    _phis.back()->addOverwrites(ds.target, interval.start, interval.length());
    // update definitions in the block -- this
    // phi node defines previously uncovered memory
    assert(
        block->definitions.get({ds.target, interval.start, interval.length()})
            .empty());
    block->definitions.update({ds.target, interval.start, interval.length()},
                              _phis.back());

    // Inserting at the beginning of the block should not
    // invalidate the iterator
    block->prependAndUpdateCFG(_phis.back());

    defs.push_back(_phis.back());
  }

  return defs;
}

void SSAReachingDefinitionsAnalysis::performLvn(RDBBlock *block) {
  // perform Lvn for one block
  for (RDNode *node : block->getNodes()) {
    // strong update
    for (auto &ds : node->overwrites) {
      assert(!ds.offset.isUnknown() && "Update on unknown offset");
      assert(!ds.target->isUnknown() && "Update on unknown memory");

      block->definitions.update(ds, node);
    }

    // weak update
    for (auto &ds : node->defs) {
      if (ds.target->isUnknown()) {
        // special handling for unknown memory
        // -- this node may define any memory that we know
        // about at this moment, so just add it to every
        // element of the definition map
        block->definitions.addAll(node);
        // also add the definition as a proper target for Gvn
        block->definitions.add({ds.target, 0, Offset::UNKNOWN}, node);
        continue;
      }

      // since this is just weak update,
      // look for the previous definitions of 'ds'
      // and if there are none, add a PHI node
      node->defuse.add(findDefinitionsInBlock(block, ds));

      // NOTE: this must be after findDefinitionsInBlock, otherwise
      // also this definition will be found
      block->definitions.add(ds, node);
    }

    // use
    for (auto &ds : node->uses) {
      node->defuse.add(findDefinitionsInBlock(block, ds));
    }
  }
}

void SSAReachingDefinitionsAnalysis::performLvn() {
  DBG_SECTION_BEGIN(dda, "Starting LVN");
  for (RDBBlock *block : graph.blocks()) {
    performLvn(block);
  }
  DBG_SECTION_END(dda, "LVN finished");
}

void SSAReachingDefinitionsAnalysis::performGvn() {
  DBG_SECTION_BEGIN(dda, "Starting GVN");
  std::set<RDNode *> phis(_phis.begin(), _phis.end());

  while (!phis.empty()) {
    RDNode *phi = *(phis.begin());
    phis.erase(phis.begin());

    // get the definition from the PHI node
    assert(phi->overwrites.size() == 1);
    const auto &ds = *(phi->overwrites.begin());

    auto block = phi->getBBlock();

    for (auto I = block->pred_begin(), E = block->pred_end(); I != E; ++I) {
      auto old_phis_size = _phis.size();

      // find definitions of this memory in the predecessor blocks
      phi->defuse.add(findDefinitions(*I, ds));

      // Queue the new phi (if any) for processing.
      if (_phis.size() != old_phis_size) {
        assert(_phis.size() > old_phis_size);
        for (auto i = old_phis_size; i < _phis.size(); ++i) {
          phis.insert(_phis[i]);
        }
      }
    }
  }
  DBG_SECTION_END(dda, "GVN finished");
}

static void recGatherNonPhisDefs(RDNode *phi, std::set<RDNode *> &phis,
                                 RDNodeSetVector &ret) {
  assert(phi->getType() == RDNodeType::PHI);
  if (!phis.insert(phi).second) return;  // we already visited this phi

  for (auto n : phi->defuse) {
    if (n->getType() != RDNodeType::PHI) {
      ret.insert(n);
    } else {
      recGatherNonPhisDefs(n, phis, ret);
    }
  }
}

// recursivelu replace all phi values with its non-phi definitions
template <typename ContT>
void gatherNonPhisDefs(const ContT &nodes, RDNodeSetVector &result) {
  std::set<RDNode *> phis;  // set of visited phi nodes - to check the fixpoint
  for (auto n : nodes) {
    if (n->getType() != RDNodeType::PHI) {
      result.insert(n);
    } else {
      recGatherNonPhisDefs(n, phis, result);
    }
  }
}

void SSAReachingDefinitionsAnalysis::getReachingDefinitions(
    RDNode *use, RDNodeSetVector &result) {
  if (use->usesUnknown()) {
    findAllReachingDefinitions(use, result);
    return;
  }
  gatherNonPhisDefs(use->defuse, result);
}

void SSAReachingDefinitionsAnalysis::findAllReachingDefinitions(
    RDNode *from, RDNodeSetVector &result) {
  DBG_SECTION_BEGIN(dda, "MemorySSA - finding all definitions");
  assert(from->getBBlock() && "The node has no BBlock");

  DefinitionsMap<RDNode> defs;   // auxiliary map for finding defintions
  RDNodeSetVector foundDefs;     // definitions that we found

  ///
  // get the definitions from this block
  // (this is basically the LVN)
  ///
  auto block = from->getBBlock();
  for (auto node : block->getNodes()) {
    // run only from the beginning of the block up to the node
    if (node == from) break;

    for (auto &ds : node->overwrites) {
      defs.update(ds, node);
    }

    // weak update
    for (auto &ds : node->defs) {
      if (ds.target->isUnknown()) {
        defs.addAll(node);
        defs.add({ds.target, 0, Offset::UNKNOWN}, node);
        continue;
      }

      defs.add(ds, node);
    }
  }

  for (auto &it : defs) {
    for (auto &nds : it.second) {
      foundDefs.insert(nds.second.begin(), nds.second.end());
    }
  }

  ///
  // get the definitions from predecessors
  ///
  std::set<RDBBlock *> visitedBlocks;  // for terminating the search
  // NOTE: do not add block to visitedBlocks, it may be its own predecessor,
  // in which case we want to process it
  if (auto singlePred = block->getSinglePredecessor()) {
    findAllReachingDefinitions(defs, singlePred, foundDefs, visitedBlocks);
  } else {
    // for multiple predecessors, we must create a copy of the
    // definitions that we have not found yet
    for (auto I = block->pred_begin(), E = block->pred_end(); I != E; ++I) {
      auto tmpDefs = defs;
      findAllReachingDefinitions(tmpDefs, *I, foundDefs, visitedBlocks);
    }
  }

  ///
  // Gather all the defintions
  ///
  DBG_SECTION_END(dda, "MemorySSA - finding all definitions done");
  gatherNonPhisDefs(foundDefs, result);
}

void SSAReachingDefinitionsAnalysis::findAllReachingDefinitions(
    DefinitionsMap<RDNode> &defs, RDBBlock *from, RDNodeSetVector &foundDefs,
    std::set<RDBBlock *> &visitedBlocks) {
  if (!from) return;

  if (!visitedBlocks.insert(from).second) return;

  // get the definitions from this block
  for (auto &it : from->definitions) {
    if (!defs.definesTarget(it.first)) {
      // just copy the definitions
      defs.add(it.first, it.second);
      for (auto &nds : it.second) {
        foundDefs.insert(nds.second.begin(), nds.second.end());
      }
      continue;
    }

    for (auto &it2 : it.second) {
      auto &interv = it2.first;
      auto uncovered =
          defs.undefinedIntervals({it.first, interv.start, interv.length()});
      for (auto &undefInterv : uncovered) {
        // we still do not have definitions for these bytes, add it
        defs.add({it.first, undefInterv.start, undefInterv.length()},
                 it2.second);
      }
    }
  }

  // recurs into predecessors
  if (auto singlePred = from->getSinglePredecessor()) {
    findAllReachingDefinitions(defs, singlePred, foundDefs, visitedBlocks);
  } else {
    for (auto I = from->pred_begin(), E = from->pred_end(); I != E; ++I) {
      auto tmpDefs = defs;
      findAllReachingDefinitions(tmpDefs, *I, foundDefs, visitedBlocks);
    }
  }
}

}  // namespace rd
}  // namespace analysis
}  // namespace dg
