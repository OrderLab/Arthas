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
  for (auto ei = node.edge_begin(); ei != node.edge_end(); ++ei) {
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
    edge_iterator eit = n->edge_begin();
    while (eit != n->edge_end()) {
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
  for (it = node1->edge_begin(); it != node1->edge_end(); ++it) {
    if ((*it)->getTargetNode() == node2) break;
  }
  if (it != node1->edge_end()) {
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
    for (auto ei = elem->edge_begin(); ei != elem->edge_end(); ++ei) {
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
            (*curr_slice->begin())->getFunction()) {
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

  if (separate_dependence) {
  }
  return true;
}

bool SliceGraph::computeDistances() {
  Instruction *root_inst = _root->getValue();
  Function *root_func = root_inst->getFunction();
  map<Instruction *, EdgeListTy> localInsts;
  for (auto edge : _edges) {
    Instruction *target = edge->getTargetNode()->getValue();
    Function *func = target->getFunction();
    if (root_func == func) {
      auto lit = localInsts.find(target);
      if (lit != localInsts.end()) {
        lit->second.push_back(edge);
      } else {
        localInsts.emplace(target, EdgeListTy{edge});
      }
    } else {
      // FIXME: dirty, assuming they are from callers
      edge->setDistance(-10000000);
    }
  }
  map<Instruction *, uint64_t> instPos;
  uint64_t root_position = 0;
  uint64_t position = 0;
  for (inst_iterator ii = inst_begin(root_func), ie = inst_end(root_func);
       ii != ie; ++ii) {
    ++position;
    Instruction *inst = &*ii;
    if (localInsts.find(inst) != localInsts.end()) {
      instPos.emplace(inst, position);
    }
    if (inst == root_inst) root_position = position;
  }
  if (root_position == 0) {
    errs() << "ERROR: cannot find position of the root instruction "
           << *_root->getValue() << "\n";
    return false;
  }
  int64_t distance;
  for (auto inst : localInsts) {
    auto inst_pit = instPos.find(inst.first);
    if (inst_pit == instPos.end()) {
      errs() << "Warning: cannot find position of instruction " << inst.first
             << "\n";
      continue;
    }
    distance = inst_pit->second - root_position;
    for (auto edge : inst.second) {
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
    std::sort(node->edge_begin(), node->edge_end(), SliceEdgeComparator());
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

