/*  Copyright 2010, Guilherme Gonçalves (guilherme.p.gonc@gmail.com)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Python.h>
#include <structmember.h>

/* Node objects are not exposed to the Python interpreter. Instead, they act as
 * internal data structures referenced by the main BinaryTree object.
 * 'item' holds the actual data, a reference to a PyObject.
 * 'lchild' and 'rchild' refer to the left and right child nodes of a given
 * node.
 * 'balance' is the height balance of the node. -1 for left-unbalanced, 0 for
 * balanced and +1 for right-unbalanced.
 */ 
typedef struct _Node {
	PyObject_HEAD
	
	PyObject * item;
	struct _Node * lchild, * rchild;
	int balance;
} Node;

/* The main binary tree class, exposed to the interpreter as BinaryTree.
 * 'root' holds a reference to the root (a Node) of the tree.
 * ob_size (defined in PyObject_VAR_HEAD) holds the number of nodes in the tree
 */
typedef struct {
	PyObject_VAR_HEAD

	Node * root;
} BinaryTree;

/* Prototypes for NodeType methods */
static void Node_dealloc(Node * self);
static int Node_traverse(Node * self, visitproc visit, void * arg);
static void Node_clear(Node * self);

/* Prototypes for BinaryTreeType methods */
static void BinaryTree_dealloc(BinaryTree * self);
static int BinaryTree_traverse(BinaryTree * self, visitproc visit, void * arg);
static void BinaryTree_clear(BinaryTree * self);

/* Left and right rotation */
static Node * rotateLeft(Node * root);
static Node * rotateRight(Node * root);

static PyTypeObject NodeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyTypeObject BinaryTreeType = {
	PyObject_HEAD_INIT(NULL)
};

static void Node_dealloc(Node * self) {
	Node_clear(self);
	Py_TYPE((PyObject *) self)->tp_free((PyObject *) self);

	return;
}

static int Node_traverse(Node * self, visitproc visit, void * arg) {
	Py_VISIT(self->item);
	Py_VISIT((PyObject *) self->lchild);
	Py_VISIT((PyObject *) self->rchild);

	return 0;
}

static void Node_clear(Node * self) {
	Py_CLEAR(self->item);
	Py_CLEAR(self->lchild);
	Py_CLEAR(self->rchild);

	return;
}

static void BinaryTree_dealloc(BinaryTree * self) {
	BinaryTree_clear(self);
	Py_TYPE((PyObject *) self)->tp_free((PyObject *) self);

	return;
}

static int BinaryTree_traverse(BinaryTree * self, visitproc visit, void * arg) {
	Py_VISIT(self->root);

	return 0;
}

static void BinaryTree_clear(BinaryTree * self) {
	Py_CLEAR(self->root);

	return;
}

/* Rotates the subtree starting at 'root' to the left.
 * Returns the new root */
static Node * rotateLeft(Node * root) {
	Node * newroot;

	if ( root == NULL ) return NULL;
	newroot = root->rchild;

	if ( newroot ) {
		root->rchild = newroot->lchild;
		newroot->lchild = root;

		return newroot;
	}

	return root;
}


/* Rotates the subtree starting at 'root' to the right.
 * Returns the new root. */
static Node * rotateRight(Node * root) {
	Node * newroot;

	if ( root == NULL ) return NULL;
	newroot = root->lchild;

	if ( newroot ) {
		root->lchild = newroot->rchild;
		newroot->rchild = root;

		return newroot;
	}

	return root;
}

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initbinarytree(void) {
	PyObject * module;

	/* NodeType setup */
	NodeType.tp_new = PyType_GenericNew;
	NodeType.tp_basicsize = sizeof(Node);
	NodeType.tp_name = "binarytree.Node";
	NodeType.tp_doc = "An internal data container.";
	NodeType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC;
	NodeType.tp_traverse = (traverseproc) Node_traverse;
	NodeType.tp_clear = (inquiry) Node_clear;
	NodeType.tp_dealloc = (destructor) Node_dealloc;
	
	if ( PyType_Ready(&NodeType) < 0 ) return;

	/* BinaryTreeType setup */
	BinaryTreeType.tp_new = PyType_GenericNew;
	BinaryTreeType.tp_basicsize = sizeof(BinaryTree);
	BinaryTreeType.tp_name = "binarytree.BinaryTree";
	BinaryTreeType.tp_doc = "The main binary tree class.";
	BinaryTreeType.tp_flags = Py_TPFLAGS_DEFAULT |
				Py_TPFLAGS_BASETYPE |
				Py_TPFLAGS_HAVE_GC;
	BinaryTreeType.tp_traverse = (traverseproc) BinaryTree_traverse;
	BinaryTreeType.tp_clear = (inquiry) BinaryTree_clear;
	BinaryTreeType.tp_dealloc = (destructor) BinaryTree_dealloc;

	if ( PyType_Ready(&BinaryTreeType) < 0 ) return;

	module = Py_InitModule3("binarytree", NULL,
				"A simple binary search tree.");
	
	Py_INCREF(&BinaryTreeType);
	PyModule_AddObject(module, "BinaryTree", (PyObject *) &BinaryTreeType);

	return;
}
