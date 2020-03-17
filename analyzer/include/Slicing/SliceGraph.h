// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _SLICING_SLICEGRAPH_H_
#define _SLICING_SLICEGRAPH_H_

#include <queue>
#include <map>
#include <stack>
#include <utility>
#include <vector>

#include "Slicing/Slice.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
namespace slicing {

class SliceNode;
class SliceEdge;

class SliceEdge {
 public:
  typedef SliceDependence EdgeKind;

  typedef int64_t DistanceTy;

 public:
  SliceEdge(SliceNode *n, EdgeKind k = EdgeKind::Unknown)
      : target_node(n), kind(k), distance(0) {}
  SliceEdge(const SliceEdge &e)
      : target_node(e.target_node), kind(e.kind), distance(0) {}

  SliceEdge &operator=(const SliceEdge &e) {
    target_node = e.target_node;
    return *this;
  }

  SliceNode *getTargetNode() const { return target_node; }

  DistanceTy getDistance() const { return distance; }
  void setDistance(DistanceTy dist) { distance = dist; }

  EdgeKind getKind() const { return kind; };
  bool isDefUse() const { return kind == EdgeKind::RegisterDefUse; }
  bool isMemoryDependence() const { return kind == EdgeKind::MemoryDependence; }
  bool isControlDependence() const {
    return kind == EdgeKind::ControlDependence;
  }

 protected:
  SliceNode *target_node;
  EdgeKind kind;
  // the physical "distance" with the target node: positive meaning after
  // the node, negative meaning before the node.
  DistanceTy distance;
};

class SliceNode {
 public:
  using ValueTy = llvm::Instruction *;
  using ValueListTy = SmallVectorImpl<ValueTy>;
  using EdgeListTy = SmallVector<SliceEdge *, 8>;
  using iterator = typename EdgeListTy::iterator;
  using const_iterator = typename EdgeListTy::const_iterator;

  enum class NodeKind {
    SingleInstruction,
    MultiInstruction,
  };

 public:
  SliceNode(ValueTy val, uint32_t dep = 0,
            NodeKind kind = NodeKind::SingleInstruction)
      : _value(val), _depth(dep), _kind(kind), _valueList{val} {}
  ~SliceNode();

  ValueTy getValue() const { return _value; }
  const ValueListTy &getValues() const { return _valueList; }
  ValueListTy &getValues() { return _valueList; }
  inline uint32_t getDepth() const { return _depth; }
  void setDepth(uint32_t dep) { _depth = dep; }

  inline const_iterator edge_begin() const { return _edges.begin(); }
  inline const_iterator edge_end() const { return _edges.end(); }
  inline iterator edge_begin() { return _edges.begin(); }
  inline iterator edge_end() { return _edges.end(); }
  const SliceEdge &edge_front() const { return *_edges[0]; }
  SliceEdge &edge_front() { return *_edges[0]; }
  const SliceEdge &edge_back() const { return *_edges.back(); }
  SliceEdge &edge_back() { return *_edges.back(); }
  EdgeListTy &edges() { return _edges; }
  size_t edge_size() const { return _edges.size(); }
  bool empty_edges() const { return _edges.empty(); }

  inline bool addEdge(SliceEdge *e) {
    _edges.push_back(e);
    return true;
  }
  bool removeEdge(SliceEdge *e);
  // allows two nodes to have multiple edges
  bool findEdgesTo(SliceNode *node, SmallVectorImpl<SliceEdge *> &el);
  bool hasEdgeTo(SliceNode *node);
  void destroyEdges();

  NodeKind getKind() const { return _kind; }

  void appendValue(ValueTy val) {
    // once we append new values, this node becomes a MultiInstruction node
    _valueList.push_back(val);
    _kind = NodeKind::MultiInstruction;
  }

  void appendValues(ValueListTy &valList) {
    _kind = NodeKind::MultiInstruction;
    _valueList.insert(_valueList.end(), valList.begin(), valList.end());
  }

 protected:
  EdgeListTy _edges;
  ValueTy _value;
  unsigned _depth;
  NodeKind _kind;
  SmallVector<ValueTy, 2> _valueList;
};

class SliceGraph {
 public:
  using NodeListTy = SmallVector<SliceNode *, 10>;
  using EdgeListTy = SmallVector<SliceEdge *, 10>;
  using NodeMapTy = std::map<SliceNode::ValueTy, SliceNode *>;

  using node_iterator = typename NodeListTy::iterator;
  using const_node_iterator = typename NodeListTy::const_iterator;
  using edge_iterator = typename EdgeListTy::iterator;
  using const_edge_iterator = typename EdgeListTy::const_iterator;

 public:
  SliceGraph(SliceNode *root_node, SliceDirection dir, uint32_t slice_id)
      : _root(root_node), _direction(dir), _slice_id(slice_id) {
    addNode(root_node);
  }

  ~SliceGraph();

  SliceDirection getDirection() const { return _direction; }

  node_iterator node_begin() { return _nodes.begin(); }
  node_iterator node_end() { return _nodes.end(); }
  const_node_iterator node_begin() const { return _nodes.begin(); }
  const_node_iterator node_end() const { return _nodes.end(); }

  edge_iterator edge_begin() { return _edges.begin(); }
  edge_iterator edge_end() { return _edges.end(); }
  const_edge_iterator edge_begin() const { return _edges.begin(); }
  const_edge_iterator edge_end() const { return _edges.end(); }

  SliceNode *getRoot() { return _root; }
  node_iterator findNode(SliceNode *node);
  const_node_iterator findNode(SliceNode *node) const;
  bool addNode(SliceNode *node);
  bool removeNode(SliceNode *node);
  SliceNode *getOrCreateNode(SliceNode::ValueTy val);
  bool removeEdge(SliceEdge *edge);

  bool connect(SliceNode *node1, SliceNode *node2, SliceEdge::EdgeKind kind);
  bool disconnect(SliceNode *node1, SliceNode *node2);

  void mergeNodes(SliceNode *A, SliceNode *B);

  size_t size() const { return _nodes.size(); }
  uint32_t slice_id() const { return _slice_id; }

  // convert the slice graph into a list of slices, each slice representing one
  // path in the slice graph.
  bool computeSlices(Slices &slices, bool inter_procedurual = true,
                     bool separate_dependence = false);

  bool computeDistances();
  bool sort();

  // Make the slice graph much more compact
  void compact();

 protected:
  NodeListTy _nodes;
  EdgeListTy _edges;
  SliceNode *_root;
  SliceDirection _direction;
  NodeMapTy _node_map;
  uint32_t _slice_id;
};

struct SliceEdgeComparator {
  bool operator()(SliceEdge *edge1, SliceEdge *edge2) const;
};

}  // namespace slicing
}  // namespace llvm

llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                              const llvm::slicing::SliceEdge::EdgeKind &kind);
llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                              const llvm::slicing::SliceEdge &edge);
llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                              const llvm::slicing::SliceNode &node);
llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                              const llvm::slicing::SliceGraph &graph);

#endif
