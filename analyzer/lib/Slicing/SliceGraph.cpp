// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Slicing/SliceGraph.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;

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

bool SliceNode::connect(SliceNode *node, SliceEdge::EdgeKind kind) {
  if (!hasEdgeTo(node)) {
    _edges.insert(new SliceEdge(node, kind));
    return true;
  }
  return false;
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
  os << "----[" << edge.getKind() << "] to " << *edge.getTargetNode()->getValue() << "\n";
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
  for (auto *node : _nodes) {
    for (auto ei = node->edge_begin(); ei != node->edge_end(); ++ei) {
      SliceEdge *edge = *ei;
      delete edge;
    }
    delete node;
  }
}

bool SliceGraph::computeSlices(Slices &slices)
{
  // TODO: iterate over all paths
  return true;
}
