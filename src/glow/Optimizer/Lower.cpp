// Copyright 2017 Facebook Inc.  All Rights Reserved.

#include "glow/Graph/Graph.h"
#include "glow/Graph/Node.h"
#include "glow/Graph/Nodes.h"
#include "glow/Optimizer/Optimizer.h"

#include "llvm/Support/Casting.h"

using namespace glow;
using llvm::cast;
using llvm::dyn_cast;

void glow::lower(Graph &G, CompilationMode mode) {
  auto &nodes = G.getNodes();

  // For each node:
  for (auto const &node : nodes) {
    // Sink Transpose below batch normalization nodes:
    if (auto *EMG = dyn_cast<ArithmeticGradNode>(node)) {
      switch (EMG->getMode()) {
      case ArithmeticGradNode::Mode::Add: {
        /// The chain rule for Addition:
        /// LHS' = OUT'
        /// RHS' = OUT'
        auto outG = EMG->getGradOfOriginalOutputNamedResult();
        EMG->getGradOfInputNamedLHS()->replaceAllUsesOfWith(outG);
        EMG->getGradOfInputNamedRHS()->replaceAllUsesOfWith(outG);
        continue;
      }

      case ArithmeticGradNode::Mode::Mul: {
        auto outG = EMG->getGradOfOriginalOutputNamedResult();
        NodeValue LHS = EMG->getLHS();
        NodeValue RHS = EMG->getRHS();
        /// The chain rule for multiplication:
        /// LHS' = RHS' * OUT'
        /// RHS' = LHS' * OUT'
        auto LHS1 = G.createArithmetic("mul.grad.lhs", RHS, outG,
                                       ArithmeticNode::Mode::Mul);
        auto RHS1 = G.createArithmetic("mul.grad.rhs", LHS, outG,
                                       ArithmeticNode::Mode::Mul);
        EMG->getGradOfInputNamedLHS()->replaceAllUsesOfWith(LHS1);
        EMG->getGradOfInputNamedRHS()->replaceAllUsesOfWith(RHS1);
        continue;
      }

      case ArithmeticGradNode::Mode::Sub: {
        /// The chain rule for subtraction:
        /// LHS' =  OUT'
        /// RHS' =  -OUT'
        auto outG = EMG->getGradOfOriginalOutputNamedResult();
        EMG->getGradOfInputNamedLHS()->replaceAllUsesOfWith(outG);
        auto zero = G.createZero("zero", outG.getType());
        auto sub =
            G.createArithmetic("sub", zero, outG, ArithmeticNode::Mode::Sub);
        EMG->getGradOfInputNamedRHS()->replaceAllUsesOfWith(sub);
        continue;
      }
      }
    } // Arithmetic Grad.
  }   // For all nodes.
}