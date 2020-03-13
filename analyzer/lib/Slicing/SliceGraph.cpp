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
#include "llvm/Support/Debug.h"

#include <algorithm>
#include <functional>

#define DEBUG_TYPE "slice-graph"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;

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

bool SliceNode::findEdgesTo(SliceNode *node, SmallVectorImpl<SliceEdge *> &el)
{
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

bool SliceNode::connect(SliceNode *node, SliceEdge::EdgeKind kind) {
  if (node == this || _value == node->getValue()) {
    // errs() << "Trying to create a self-pointing edge\n";
    return false;
  }
  if (!hasEdgeTo(node)) {
    _edges.push_back(new SliceEdge(node, kind));
    return true;
  }
  return false;
}

bool SliceNode::disconnect(SliceNode *node) {
  iterator it;
  for (it = _edges.begin(); it != _edges.end(); ++it) {
    if ((*it)->getTargetNode() == node) break;
  }
  if (it != _edges.end()) {
    _edges.erase(it);
    return true;
  }
  return false;
}

void SliceNode::sort() {
  // TODO: set the distance of slice edges properly before sorting
  std::sort(_edges.begin(), _edges.end(), SliceEdgeComparator());
}

void SliceGraph::sort() {
  // sort each slice node's edges, but it's not necessary to sort
  // the nodes list as we want the root node to be in the first.
  for (auto node : _nodes) {
    node->sort();
  }
}

raw_ostream &operator<<(raw_ostream &os, const SliceEdge::EdgeKind &kind)
{
  switch (kind) {
    case SliceEdge::EdgeKind::Unknown: os << "n/a"; return os;
    case SliceEdge::EdgeKind::RegisterDefUse: os << "reg-def-use"; return os;
    case SliceEdge::EdgeKind::MemoryDependence: os << "memory-dep"; return os;
    case SliceEdge::EdgeKind::ControlDependence: os << "control-dep"; return os;
    case SliceEdge::EdgeKind::InterfereDependence: os << "interfere-dep"; return os;
    default: os << "unknown"; return os;
  }
  return os;
}

raw_ostream &operator<<(raw_ostream &os, const SliceEdge &edge)
{
  Value *value = edge.getTargetNode()->getValue();
  if (auto inst = dyn_cast<Instruction>(value)) {
    os << "----[" << edge.getKind() << "] to " << inst->getFunction()->getName()
       << "(): " << *inst << "\n";
  } else {
    os << "----[" << edge.getKind() << "] to " << *value << "\n";
  }
  return os;
}

raw_ostream &operator<<(raw_ostream &os, const SliceNode &node)
{
  os << "* node: " << *node.getValue() << "\n";
  for (auto ei = node.edge_begin(); ei != node.edge_end(); ++ei) {
    SliceEdge *edge = *ei;
    os << *edge;
  }
  return os;
}

raw_ostream &operator<<(raw_ostream &os, const SliceGraph &graph)
{
  for (auto ni = graph.node_begin(); ni != graph.node_end(); ++ni) {
    SliceNode *node = *ni;
    os << *node;
  }
  return os;
}

bool SliceGraph::addNode(SliceNode *node) 
{
  if (findNode(node) != _nodes.end()) return false;
  _nodes.push_back(node);
  _node_map.emplace(node->getValue(), node);
  return true;
}

SliceNode *SliceGraph::getOrCreateNode(SliceNode::ValueTy val)
{
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

SliceGraph::node_iterator SliceGraph::findNode(SliceNode *node)
{
  for (node_iterator ni = _nodes.begin(); ni != _nodes.end(); ni++)
    if (*ni == node)
      return ni;
  return _nodes.end();
}

SliceGraph::const_node_iterator SliceGraph::findNode(SliceNode *node) const 
{
  for (const_node_iterator ni = _nodes.begin(); ni != _nodes.end(); ni++)
    if (*ni == node)
      return ni;
  return _nodes.end();
}

bool SliceGraph::removeNode(SliceNode *node)
{
  node_iterator ni = findNode(node);
  if (ni == _nodes.end())
    return false;
  EdgeListTy el;
  for (auto *n : _nodes) {
    if (n == node) continue;
    // remove all edges to the target node
    n->findEdgesTo(node, el);
    for (auto *e : el)
      n->removeEdge(e);
    el.clear();
  }
  node->clearEdges();
  _nodes.erase(ni);
  return true;
}

SliceGraph::~SliceGraph()
{
  errs() << "Destructing slice graph " << _slice_id << "\n";
  for (auto *node : _nodes) {
    for (auto ei = node->edge_begin(); ei != node->edge_end(); ++ei) {
      SliceEdge *edge = *ei;
      delete edge;
    }
    delete node;
  }
}

bool SliceGraph::computeSlices(Slices &slices) {
  SmallPtrSet<SliceNode *, 8> visited;
  stack<SliceNode *> vstack;
  SliceNode *elem, *next;
  Slice *curr_slice, *next_slice, *forked_slice;
  uint64_t curr_slice_id = 1;
  size_t unexplored_edges;

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
    for (auto ei = elem->edge_begin(); ei != elem->edge_end(); ++ei) {
      next = (*ei)->getTargetNode();
      if (visited.insert(next).second) {
        vstack.push(next);
        unexplored_edges++;
        if (unexplored_edges > 1) {
          forked_slice = curr_slice->fork();
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
