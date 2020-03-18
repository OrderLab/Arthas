// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Slicing/SliceGraph.h"
#include "Matcher/Matcher.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/Debug.h"

#include <algorithm>
#include <functional>

#define DEBUG_TYPE "slice-graph"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;

raw_ostream &operator<<(raw_ostream &os, const SliceEdge::EdgeKind &kind) {
  switch (kind) {
    case SliceEdge::EdgeKind::Unknown:
      os << "n/a";
      return os;
    case SliceEdge::EdgeKind::RegisterDefUse:
      os << "reg-def-use";
      return os;
    case SliceEdge::EdgeKind::MemoryDependence:
      os << "memory-dep";
      return os;
    case SliceEdge::EdgeKind::ControlDependence:
      os << "control-dep";
      return os;
    case SliceEdge::EdgeKind::InterfereDependence:
      os << "interfere-dep";
      return os;
    default:
      os << "unknown";
      return os;
  }
  return os;
}

raw_ostream &operator<<(raw_ostream &os, const SliceEdge &edge) {
  SliceNode::ValueTy value = edge.getTargetNode()->getValue();
  os << "----[" << edge.getKind() << "] to distance " << edge.getDistance();
  os << " " << value->getFunction()->getName() << "(): " << *value << "\n";
  return os;
}

raw_ostream &operator<<(raw_ostream &os, const SliceNode &node) {
  os << "* node: " << *node.getValue() << "\n";
  for (auto ei = node.begin(); ei != node.end(); ++ei) {
    SliceEdge *edge = *ei;
    os << *edge;
  }
  return os;
}

raw_ostream &operator<<(raw_ostream &os, const SliceGraph &graph) {
  for (auto ni = graph.node_begin(); ni != graph.node_end(); ++ni) {
    SliceNode *node = *ni;
    os << *node;
  }
  return os;
}

bool SliceEdgeComparator::operator()(SliceEdge *edge1, SliceEdge *edge2) const {
  auto dist1 = edge1->getDistance();
  auto dist2 = edge2->getDistance();
  // with a sequence of -4, -3, -2, -1, 0, 1, 2, 3, where 0 is the target
  // instruction, the backward sort order is:
  // 0, -1, -2, -3, -4, 1, 2, 3
  if (dist1 == 0 && dist2 == 0) return false;
  if (dist1 < 0) {
    if (dist2 > 0) return true;
    return -dist1 < -dist2;
  } else {
    if (dist2 < 0) return false;
    return dist1 < dist2;
  }
}

bool SliceNode::findEdgesTo(SliceNode *node, SmallVectorImpl<SliceEdge *> &el) {
  for (auto *edge : _edges) {
    if (edge->getTargetNode() == node) el.push_back(edge);
  }
  return !el.empty();
}

bool SliceNode::hasEdgeTo(SliceNode *node) {
  for (auto *edge : _edges) {
    if (edge->getTargetNode() == node) return true;
  }
  return false;
}

bool SliceNode::removeEdge(SliceEdge *edge) {
  iterator it;
  for (it = _edges.begin(); it != _edges.end(); ++it) {
    if (*it == edge) break;
  }
  if (it != _edges.end()) {
    _edges.erase(it);
    return true;
  }
  return false;
}

void SliceNode::destroyEdges() {
  for (SliceEdge *edge : _edges) {
    delete edge;
  }
  _edges.clear();
}

SliceNode::~SliceNode() { destroyEdges(); }

bool SliceGraph::addNode(SliceNode *node) {
  if (findNode(node) != _nodes.end()) return false;
  _nodes.push_back(node);
  _node_map.emplace(node->getValue(), node);
  return true;
}

SliceNode *SliceGraph::getOrCreateNode(SliceNode::ValueTy val) {
  auto it = _node_map.find(val);
  if (it == _node_map.end()) {
    SliceNode *node = new SliceNode(val);
    _node_map.emplace(val, node);
    // also should insert this node into the node list
    _nodes.push_back(node);
    return node;
  }
  return it->second;
}

SliceGraph::node_iterator SliceGraph::findNode(SliceNode *node) {
  for (node_iterator ni = _nodes.begin(); ni != _nodes.end(); ni++)
    if (*ni == node) return ni;
  return _nodes.end();
}

SliceGraph::const_node_iterator SliceGraph::findNode(SliceNode *node) const {
  for (const_node_iterator ni = _nodes.begin(); ni != _nodes.end(); ni++)
    if (*ni == node) return ni;
  return _nodes.end();
}

bool SliceGraph::removeNode(SliceNode *node) {
  node_iterator ni = findNode(node);
  if (ni == _nodes.end()) return false;
  for (auto *n : _nodes) {
    if (n == node) continue;
    // remove all edges to the target node
    edge_iterator eit = n->begin();
    while (eit != n->end()) {
      SliceEdge *edge = *eit;
      if (edge->getTargetNode() == node) {
        eit = n->edges().erase(eit);
        delete edge;
      } else {
        ++eit;
      }
    }
  }
  node->destroyEdges();
  _nodes.erase(ni);
  return true;
}

bool SliceGraph::removeEdge(SliceEdge *edge) {
  edge_iterator it;
  for (it = _edges.begin(); it != _edges.end(); ++it) {
    if (*it == edge) break;
  }
  if (it != _edges.end()) {
    _edges.erase(it);
    return true;
  }
  return false;
}

bool SliceGraph::connect(SliceNode *node1, SliceNode *node2,
                         SliceEdge::EdgeKind kind) {
  if (node1 == node2 || node1->getValue() == node2->getValue()) {
    return false;
  }
  if (!node1->hasEdgeTo(node2)) {
    SliceEdge *edge = new SliceEdge(node2, kind);
    node1->addEdge(edge);
    // add the new edge to the global edge list
    _edges.push_back(edge);
    return true;
  }
  return false;
}

bool SliceGraph::disconnect(SliceNode *node1, SliceNode *node2) {
  SliceNode::iterator it;
  for (it = node1->begin(); it != node1->end(); ++it) {
    if ((*it)->getTargetNode() == node2) break;
  }
  if (it != node1->end()) {
    node1->edges().erase(it);
    edge_iterator eit;
    for (eit = _edges.begin(); eit != _edges.end(); ++eit) {
      if (*eit == *it) break;
    }
    if (eit != _edges.end()) {
      // remove the edge from the global edge list as well
      _edges.erase(eit);
    }
    // now it's safe to delete this edge
    delete *it;
    return true;
  }
  return false;
}

SliceGraph::~SliceGraph() {
  errs() << "Destructing slice graph " << _slice_id << "\n";
  for (auto *node : _nodes) {
    delete node;
  }
}

bool SliceGraph::computeSlices(Slices &slices, bool inter_procedurual,
                               bool separate_dependence) {
  SmallPtrSet<SliceNode *, 8> visited;
  stack<SliceNode *> vstack;
  SliceNode *elem, *next;
  Slice *curr_slice, *next_slice, *forked_slice;
  uint64_t curr_slice_id = 1;
  size_t unexplored_edges;
  SliceEdge *edge;

  curr_slice = new Slice(curr_slice_id, _root->getValue(), _direction);
  vstack.push(_root);
  slices.add(curr_slice);
  while (!vstack.empty()) {
    elem = vstack.top();
    vstack.pop();
    visited.insert(elem);
    // root has been added in the initial slice constructor, skip adding it
    if (elem != _root) curr_slice->add(elem->getValue());

    // if this node has only one unexplored edge, proceed with the current
    // slice; if this node has more than one unexplored edges, we allow the
    // current slice to proceed on one edge, and fork new slices
    // for exploring the remaining edges.
    unexplored_edges = 0;
    bool current_dependence_unknown =
        (curr_slice->dependence == SliceDependence::Unknown);
    // because we are doing DFS, when we push the edges into the stack
    // if it's backward slice, we need to push the edges in reverse distance
    // order (rbegin instead of begin), so that the closest element can be
    // explored first.
    for (auto ei = elem->rbegin(); ei != elem->rend(); ++ei) {
      edge = *ei;
      if (separate_dependence) {
        // if separate_dependence flag is on, it means we only compute
        // slice that has the same dependency kind, e.g., a slice
        // that only has def-use relationship or a slice where each
        // element is connected through memory dependency only
        if (!current_dependence_unknown &&
            edge->getKind() == curr_slice->dependence) {
          // if we are on one dependency kind of a slice and the outgoing
          // edge is of a different kind, we should skip this edge
          continue;
        }
      }
      if (!inter_procedurual) {
        if (edge->getTargetNode()->getValue()->getFunction() !=
            curr_slice->begin()->first->getFunction()) {
          continue;
        }
      }
      next = edge->getTargetNode();
      if (visited.insert(next).second) {
        if (separate_dependence &&
            curr_slice->dependence == SliceDependence::Unknown) {
          // here we must use curr_slice->dependence instead of
          // current_dependence_unknown to test in the if so that
          // the current slice's dependence is only set once
          curr_slice->dependence = edge->getKind();
        }
        vstack.push(next);
        unexplored_edges++;
        if (unexplored_edges > 1) {
          forked_slice = curr_slice->fork();
          if (separate_dependence && current_dependence_unknown) {
            // normally the forked slice will inherit the current
            // slice's dependence, so we don't need to set it.
            // but if the current slice's dependence was *previously*
            // unknown, we must reset the forked slice's dependence
            forked_slice->dependence = edge->getKind();
          }
          ++curr_slice_id;
          forked_slice->id = curr_slice_id;
          DEBUG(dbgs() << "Forked slice " << curr_slice->id << " to slice "
                       << forked_slice->id << "\n");
          slices.add(forked_slice);
        }
      }
    }
    // if this node does not have any unexplored edges, or it is a leaf node
    if (unexplored_edges == 0) {
      // we are done with the current slice, add it to the slices
      // if it has not been added before, move on to the next slice, which
      // we assume is the current slice's id + 1
      //
      // FIXME: we may need to distinguish between empty edges and zero
      // unexplored edges because an already explored edge still represents
      // a dependency that we may need to add to the slice anyway.
      if (!slices.has(curr_slice->id)) {
        slices.add(curr_slice);
      }
      next_slice = slices.get(curr_slice->id + 1);
      if (next_slice == nullptr) {
        DEBUG(dbgs() << "Cannot find slice " << curr_slice->id + 1 << "\n");
        break;
      }
      curr_slice = next_slice;
    }
  }
  // the current slice may be the last slice that has not been added to the
  // slices, add it in this case.
  if (!slices.has(curr_slice->id)) {
    slices.add(curr_slice);
  }

  return true;
}

bool SliceGraph::computeDistances() {
  // NOTE: this is a different algorithm to compute distances compared to
  // the initial version.
  //
  // In our initial version, we compute the distances by counting the
  // position of the root node in the root node's function and subtracting
  // from that position with the positions of all other nodes. If some
  // slice nodes reside in a function that is different from the root
  // node's function, we set its position to be a larger number indicating
  // instructions that are far away.
  //
  // In this new version, we compute the edge distances relative to the
  // slice node, which is not necessarily a root node. If some edge
  // belongs to a function that is different from the slice node's function,
  // we'll compute the relative distance w.r.t. that function. For example,
  //
  // assuming a slice node---foo() inst no.7---has edges e1 := foo() inst no. 3,
  // e2 := foo() inst no. 5, e3 := bar() inst no. 10, e4 := bar() inst no.8,
  // e5 := bar() inst no. 13.
  //
  // The distances are: -4, -2, 10-N, 8-N, 13-N, where N is a large number
  //
  // With backward slicing, the sorted edges are: e2, e1, e5, e4, e3
  //
  map<Instruction *, uint64_t> instPosMap;
  map<Function *, set<Instruction *>> funcInsts;
  for (auto node : _nodes) {
    Instruction *root_inst = node->getValue();
    Function *root_func = root_inst->getFunction();
    auto fit = funcInsts.find(root_func);
    if (fit == funcInsts.end()) {
      funcInsts.emplace(root_func, set<Instruction *>{root_inst});
    } else {
      fit->second.insert(root_inst);
    }
    for (auto edge : *node) {
      Instruction *target = edge->getTargetNode()->getValue();
      Function *func = target->getFunction();
      auto fit = funcInsts.find(func);
      if (fit == funcInsts.end()) {
        funcInsts.emplace(func, set<Instruction *>{target});
      } else {
        fit->second.insert(target);
      }
    }
  }
  for (auto &entry : funcInsts) {
    Function *func = entry.first;
    auto inst_set = entry.second;
    uint64_t position = 0;
    for (inst_iterator ii = inst_begin(func), ie = inst_end(func); ii != ie;
         ++ii) {
      ++position;
      Instruction *inst = &*ii;
      if (inst_set.find(inst) != inst_set.end()) {
        instPosMap.emplace(inst, position);
      }
    }
  }

  uint64_t ref_position;
  int64_t distance;
  for (auto node : _nodes) {
    Instruction *root_inst = node->getValue();
    Function *root_func = root_inst->getFunction();
    auto pit = instPosMap.find(root_inst);
    if (pit == instPosMap.end()) {
      errs() << "Warning: cannot find position of " << *root_inst << "\n";
      return false;
    }
    map<Function *, uint64_t> refPosMap;
    uint64_t external_funcs = 1;
    // remember root function's reference position
    refPosMap.emplace(root_func, pit->second);
    for (auto edge : *node) {
      Instruction *target_inst = edge->getTargetNode()->getValue();
      Function *target_func = target_inst->getFunction();
      pit = instPosMap.find(target_inst);
      if (pit == instPosMap.end()) {
        errs() << "Warning: cannot find position of " << *target_inst << "\n";
        return false;
      }
      auto rit = refPosMap.find(target_func);
      if (rit == refPosMap.end()) {
        // assuming a function has at most 100,000 instructions
        ref_position = (_direction == SliceDirection::Backward)
                           ? (100000 * external_funcs++)
                           : 0;
        refPosMap.emplace(target_func, ref_position);
      } else {
        ref_position = rit->second;
      }
      distance = pit->second - ref_position;
      edge->setDistance(distance);
    }
  }
  return true;
}

bool SliceGraph::sort() {
  if (!computeDistances()) return false;
  // sort each slice node's edges, but it's not necessary to sort
  // the nodes list as we want the root node to be in the first.
  for (auto node : _nodes) {
    std::sort(node->begin(), node->end(), SliceEdgeComparator());
  }
  return true;
}

void SliceGraph::mergeNodes(SliceNode *A, SliceNode *B) {
  SliceEdge *&edgeToFold = A->edges().back();
  if (edgeToFold->getTargetNode() != B) {
    errs() << "To merge node " << B << " into node " << A << ", ";
    errs() << A << " must have a single edge to " << B << "\n";
    return;
  }

  // Copy instructions from node B to end of node A
  A->appendValues(B->getValues());

  // Move the outgoing edges from node B to node A
  for (auto edge : B->edges()) {
    connect(A, edge->getTargetNode(), edge->getKind());
  }
  // Remove the folded edge in A and from the graph
  A->removeEdge(edgeToFold);
  removeEdge(edgeToFold);
  // Destroy the folded edge
  delete edgeToFold;
  // Destroy the node
  removeNode(B);
  delete B;
}

void SliceGraph::compact() {
  // Make the slice graph more compact by coalescing nodes that have only
  // outgoing edge
  // TODO: implement compaction algorithm
  //
  // For now, the compaction does not really seem necessary
}

