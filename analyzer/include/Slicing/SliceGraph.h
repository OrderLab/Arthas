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

class SliceNode {
 public:
  using EdgeListTy = SetVector<SliceEdge *>;
  using iterator = typename EdgeListTy::iterator;
  using const_iterator = typename EdgeListTy::const_iterator;

 public:
  llvm::Value *value;
  int depth;

  SliceNode(llvm::Value *val, int dep = 0) {
    value = val;
    depth = dep;
  }

  inline const_iterator edge_begin() const { return _edges.begin(); }
  inline const_iterator edge_end() const { return _edges.end(); }
  inline iterator edge_begin() { return _edges.begin(); }
  inline iterator edge_end() { return _edges.end(); }
  const SliceEdge &edge_front() const { return *_edges[0]; }
  SliceEdge &edge_front() { return *_edges[0]; }
  const SliceEdge &edge_back() const { return *_edges.back(); }
  SliceEdge &edge_back() { return *_edges.back(); }
  EdgeListTy &edges() { return _edges; }

  bool addEdge(SliceEdge *e) { return _edges.insert(e); }
  void removeEdge(SliceEdge *e) { _edges.remove(e); }
  // allows two nodes to have multiple edges
  bool findEdgesTo(SliceNode *node, SmallVectorImpl<SliceEdge *> &el); 
  bool hasEdgeTo(SliceNode *node);
  void clearEdges() { _edges.clear(); }

  void dump(raw_ostream &os);

 protected:
  EdgeListTy _edges;
};

class SliceEdge {
 public:
  SliceEdge(SliceNode *n) : target_node(n) {}
  SliceEdge(const SliceEdge &e) : target_node(e.target_node) {}

  SliceEdge &operator=(const SliceEdge &e) {
    target_node = e.target_node;
    return *this;
  }

  SliceNode* getTargetNode() { return target_node; }

 protected:
  SliceNode *target_node;
};

class SliceGraph {
 public:
  using NodeListTy = SmallVector<SliceNode *, 10>;
  using EdgeListTy = SmallVector<SliceEdge *, 10>;

  using node_iterator = typename NodeListTy::iterator;
  using const_node_iterator = typename NodeListTy::const_iterator;
  using edge_iterator = typename EdgeListTy::iterator;
  using const_edge_iterator = typename EdgeListTy::const_iterator;

 public:
  SliceGraph(SliceNode *root_node) : _root(root_node) { addNode(root_node); }
  ~SliceGraph();

  inline node_iterator node_begin() { return _nodes.begin(); }
  inline node_iterator node_end() { return _nodes.end(); }
  inline const_node_iterator node_begin() const { return _nodes.begin(); }
  inline const_node_iterator node_end() const { return _nodes.end(); }
  inline SliceNode * getRoot() { return _root; }

  node_iterator findNode(SliceNode *node);
  const_node_iterator findNode(SliceNode *node) const;
  bool addNode(SliceNode *node);
  bool removeNode(SliceNode *node);

 protected:
  NodeListTy _nodes;
  SliceNode *_root;
};

} // namespace slicing
} // namespace llvm

#endif
