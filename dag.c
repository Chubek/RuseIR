#include <stdbool.h>
#include <stdlib.h>

typedef struct DAGNode {
  bool isConstant;
  IROpcode opcode;
  SSAOperand result;
  IROperand operand1;
  IROperand operand2;
  struct DAGNode *left;
  struct DAGNode *right;
} DAGNode;

typedef enum {
  INST_DAG_NODE,
  INST_DAG_PHI,
} DAGInstructionKind;

typedef struct {
  DAGInstructionKind kind;
  union {
    PhiInstruction phiDAGInstruction;
    DAGNode *dagNode;
  };
} DAGInstruction;

DAGNode *createDAGNode(bool isConstant, IROpcode opcode, SSAOperand result,
                       IROperand operand1, IROperand operand2, DAGNode *left,
                       DAGNode *right) {
  DAGNode *node = (DAGNode *)zAlloc(sizeof(DAGNode));
  node->isConstant = isConstant;
  node->opcode = opcode;
  node->result = result;
  node->operand1 = operand1;
  node->operand2 = operand2;
  node->left = left;
  node->right = right;
  return node;
}

DAGInstruction createDAGInstruction(DAGInstructionKind kind,
                                    PhiInstruction phiInstr, DAGNode *dagNode) {
  DAGInstruction instruction;
  instruction.kind = kind;

  if (kind == INST_DAG_PHI) {
    instruction.phiInstruction = phiInstr;
  } else {
    instruction.dagNode = dagNode;
  }

  return instruction;
}

IRFunction createDAGIrFunction(InstructionBlock *instructionBlocks,
                               CFGBlock *entryCFGBlock, int numBlocks) {
  IRFunction irFunc;
  irFunc.instructionBlocks = instructionBlocks;
  irFunc.entryCFGBlock = entryCFGBlock;
  irFunc.numBlocks = numBlocks;
  return irFunc;
}

void optimizeRemoveDeadCode(DAGNode *root) {
  if (root == NULL) {
    return;
  }

  if (root->opcode == IR_NULL) {
    // Null node, remove it
    root = NULL;
    return;
  }

  nullSequenceOptimization(root->left);
  nullSequenceOptimization(root->right);

  // Check if both children are null, and the node itself is not a PHI node
  if (root->opcode != IR_PHI && root->left == NULL && root->right == NULL) {
    // Null sequence, replace this node with a null node
    root->opcode = IR_NULL;
    root->result.irValue = NULL;
  }
}

void optimizeUnsignedDivision(DAGNode *node, int n, double e) {
  if (node == NULL) {
    return;
  }

  optimizeUnsignedDivision(node->left);
  optimizeUnsignedDivision(node->right);

  if (node->opcode == IR_UDIV && node->operand2.isConstant &&
      node->operand2.value.pointer != 0) {

    // Calculate k as recommended by Ertl and Wien
    unsigned long k = 2 * n;

    // Calculate C
    double twoPowK = pow(2, k);
    double C = (twoPowK + e) / node->operand2.value.doubleRational;

    // Replace IR_UDIV with the formula q = (n * C) / (2 ** k)
    node->opcode = IR_MUL;
    node->operand2.value = 1.0 / twoPowK; // Replace with reciprocal

    // Replace IR_UDIV with the formula q = (n * C) / (2 ** k)
    node->opcode = IR_UMUL;
    node->operand2.isConstant = true;
    node->operand2.value.doubleRational = C;
    node->right = createDAGNode(true, IR_POW, node->result, node->operand1,
                                createDAGNode(true, IR_CONSTANT, node->result,
                                              node->operand2, NULL, NULL),
                                NULL);
  }
}

void optimizeSignedDivision(DAGNode *node, int n, double e) {
  if (node == NULL) {
    return;
  }

  optimizeSignedDivision(node->left, n, e);
  optimizeSignedDivision(node->right, n, e);

  if (node->opcode == IR_DIV && node->operand2.isConstant &&
      node->operand2.value.pointer != 0) {

    // Calculate k as recommended by Ertl and Wien
    unsigned long k = 2 * n;

    // Calculate C
    double twoPowK = pow(2, k);
    double C = (twoPowK + e) / fabs(node->operand2.value.doubleRational);

    // Replace IR_DIV with the formula q = (n * C) / (2 ** k)
    node->opcode = IR_MUL;

    // Create new DAG nodes for the division and multiplication
    DAGNode *absOperand2 =
        createDAGNode(true, IR_ABS, node->result, node->operand2, NULL, NULL);
    node->operand2 =
        createDAGNode(true, IR_POW, node->result,
                      createDAGNode(true, IR_CONSTANT, node->result,
                                    node->operand2, NULL, NULL),
                      createDAGNode(true, IR_CONSTANT, node->result,
                                    1.0 / twoPowK, NULL, NULL),
                      NULL);
    node->right =
        createDAGNode(true, IR_DIV, node->result,
                      createDAGNode(true, IR_MUL, node->result, node->operand1,
                                    createDAGNode(true, IR_CONSTANT,
                                                  node->result, C, NULL, NULL),
                                    NULL, NULL),
                      absOperand2, NULL);
  }
}
