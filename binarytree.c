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
 * 'height' keeps the size maximum number of nodes between a node and a leaf.
 * It is initialized as 1 for leaves.
 */ 
typedef struct _Node {
	PyObject_HEAD
	
	PyObject * item;
	struct _Node * lchild, * rchild;
	int balance;
	int height;
} Node;

/* The main binary tree class, exposed to the interpreter as BinaryTree.
 * 'root' holds a reference to the root (a Node) of the tree.
 */
typedef struct {
	PyObject_HEAD

	Node * root;
} BinaryTree;

#define NODE_UPDATE_BALANCE(node) \
	(node)->balance = ((node)->rchild ? (node)->rchild->height : 0) \
			- ((node)->lchild ? (node)->lchild->height : 0)

#define NODE_IS_LEAF(node) ((node)->height == 1 ) && \
			!((node)->rchild || (node)->rchild) && \
			((node)->balance == 0)

/* Prototypes for NodeType methods */
static void Node_dealloc(Node * self);
static int Node_traverse(Node * self, visitproc visit, void * arg);
static void Node_clear(Node * self);

/* Prototypes for Node methods.
 * Generally, the corresponding BinaryTree methods are simply bindings
 * to these.
 */
static BinaryTree * Node_lchild(Node * self);
static BinaryTree * Node_rchild(Node * self);
static Node * Node_insert(Node * root, Node * new);
static int Node_inOrder(Node * root, PyObject * func);
static int Node_preOrder(Node * root, PyObject * func);
static int Node_postOrder(Node * root, PyObject * func);

/* Prototypes for BinaryTreeType methods */
static void BinaryTree_dealloc(BinaryTree * self);
static int BinaryTree_traverse(BinaryTree * self, visitproc visit, void * arg);
static void BinaryTree_clear(BinaryTree * self);
static int BinaryTree_contains(BinaryTree * self, PyObject * value);

/* Protoypes for BinaryTree methods */
static PyObject * BinaryTree_insert(BinaryTree * self, PyObject * new);
static PyObject * BinaryTree_locate(BinaryTree * self, PyObject * target);
static PyObject * BinaryTree_inOrder(BinaryTree * self, PyObject * func);
static PyObject * BinaryTree_preOrder(BinaryTree * self, PyObject * func);
static PyObject * BinaryTree_postOrder(BinaryTree * self, PyObject * func);

/* Left and right rotation */
static Node * rotateLeft(Node * root);
static Node * rotateRight(Node * root);

static void Node_updateHeight(Node * node);
static Node * Node_new(void);

static PyTypeObject NodeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyGetSetDef Node_getsetters[] = {
	{"left_child",
	(getter) Node_lchild,
	},
	{"right_child",
	(getter) Node_rchild,
	},
	{NULL}, /* Sentinel */
};

static PyMemberDef Node_members[] = {
	{"item", T_OBJECT_EX, offsetof(Node, item), READONLY,
	"The item kept by this node.",
	},
	{NULL}, /* Sentinel */
};

static PyTypeObject BinaryTreeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyMethodDef BinaryTree_methods[] = {
	{ "insert", (PyCFunction) BinaryTree_insert, METH_O,
	"Inserts the parameter into the tree, without creating duplicates."
	},
	{"locate", (PyCFunction) BinaryTree_locate, METH_O,
	"The Node that contains the parameter if it is in the tree, or None."
	},
	{"in_order", (PyCFunction) BinaryTree_inOrder, METH_O,
	"in_order(callable) -> apply 'callable' to each node, in-order."
	},
	{"pre_order", (PyCFunction) BinaryTree_preOrder, METH_O,
	"pre_order(callable) -> apply 'callable' to each node, pre-order."
	},
	{NULL}, /* Sentinel */
};

static PyMemberDef BinaryTree_members[] = {
	{"root", T_OBJECT, offsetof(BinaryTree, root), READONLY,
	"Root of the tree.",
	},
	{NULL}, /* Sentinel */
};

static PySequenceMethods BinaryTree_sequence;

/* Returns the left subtree of a given node, as a new reference */
static BinaryTree * Node_lchild(Node * self) {
	BinaryTree * subtree;

	subtree = PyObject_GC_New(BinaryTree, &BinaryTreeType);
	if ( subtree == NULL ) return NULL;

	Py_XINCREF(self->lchild);
	subtree->root = self->lchild;
	PyObject_GC_Track((PyObject *) subtree);

	return subtree;
}

/* Returns the right subtree of a given node, as a new reference */
static BinaryTree * Node_rchild(Node * self) {
	BinaryTree * subtree;

	subtree = PyObject_GC_New(BinaryTree, &BinaryTreeType);
	if ( subtree == NULL ) return NULL;

	Py_XINCREF(self->rchild);
	subtree->root = self->rchild;
	PyObject_GC_Track((PyObject *) subtree);

	return subtree;
}

static void Node_dealloc(Node * self) {
	PyObject_GC_UnTrack(self);
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
	PyObject_GC_UnTrack(self);
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

static int BinaryTree_contains(BinaryTree * self, PyObject * value) {
	PyObject * res;
	
	res = BinaryTree_locate(self, value);
	if ( res == NULL ) return -1;

	return ( res == Py_None ) ? 0 : 1;
}

/* Rotates the subtree starting at 'root' to the left.
 * Returns the new root */
static Node * rotateLeft(Node * root) {
	Node * newroot;

	if ( root == NULL ) return NULL;
	newroot = root->rchild;
	
	if ( newroot ) {
		root->rchild = newroot->lchild;
		Node_updateHeight(root);
		
		newroot->lchild = root;
		Node_updateHeight(newroot);

		NODE_UPDATE_BALANCE(root);
		NODE_UPDATE_BALANCE(newroot);

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
		Node_updateHeight(root);
		
		newroot->rchild = root;
		Node_updateHeight(newroot);
		
		NODE_UPDATE_BALANCE(root);
		NODE_UPDATE_BALANCE(newroot);

		return newroot;
	}

	return root;
}

static void Node_updateHeight(Node * node) {
	int lheight, rheight;

	if ( node == NULL ) return;

	lheight = node->lchild ? node->lchild->height : 0;
	rheight = node->rchild ? node->rchild->height : 0;
	
	node->height = 1 + ((lheight > rheight) ? lheight : rheight);

	return;
}

/* Creates and returns an empty leaf node.
 * Returns a new reference.
 */
static Node * Node_new(void) {
	Node * newnode;
	
	newnode = PyObject_GC_New(Node, &NodeType);	
	if ( newnode == NULL ) return NULL;

	/* Initializing as a leaf */
	newnode->height = 1;
	newnode->balance = 0;
	
	newnode->lchild = NULL;
	newnode->rchild = NULL;
	newnode->item = NULL;

	PyObject_GC_Track(newnode);
	
	return newnode;
}

/* Inserts 'new' into the tree whose root is 'root'.
 * Returns the new root of the tree, or NULL on failure.
 * Note: Assumes 'new' has been initialized as a leaf.
 */
static Node * Node_insert(Node * root, Node * new) {
	int child_height = 0;

	if ( root == NULL ) {
		assert(NODE_IS_LEAF(new));
		return new;
	}

	switch ( PyObject_Compare(root->item, new->item) ) {
		case 0:
			return root; /* Node already in the tree */
		case 1:
			/* Descend left */
			if ( root->lchild )
				child_height = root->lchild->height;

			root->lchild = Node_insert(root->lchild, new);
			if ( root->lchild == NULL ) return NULL;

			/* No change in height.
			 * Either the insertion balanced the left subtree,
			 * or there has already been a rebalancing somewhere
			 * along the way up here.
			 */
			if ( child_height == root->lchild->height )
				return root;

			assert(root->lchild->height == 1 + child_height);
			
			/* Fixing balance and height of the root */
			Node_updateHeight(root);
			NODE_UPDATE_BALANCE(root);

			/* Do we need to rebalance? */
			if ( root->balance > -2 ) return root;

			if ( root->lchild->balance == 1 ) {
				/* Left-right case */
				root->lchild = rotateLeft(root->lchild);
			}

			/* Left-left case */
			return rotateRight(root);

		case -1:
			/* Descend right.
			 * Symmetrical to the left case above.
			 */
			if ( root->rchild )
				child_height = root->rchild->height;

			root->rchild = Node_insert(root->rchild, new);
			if ( root->rchild == NULL ) return NULL;

			if ( child_height == root->rchild->height )
				return root;

			assert(root->rchild->height == 1 + child_height);
			
			Node_updateHeight(root);
			NODE_UPDATE_BALANCE(root);

			if ( root->balance < 2 ) return root;

			if ( root->rchild->balance == -1 ) {
				/* Right-left case */
				root->rchild = rotateRight(root->rchild);
			}

			/* Right-Right case */
			return rotateLeft(root);
	}

	return NULL; /* Unexpected result in comparison */
}

/* Inserts 'new' into a binary tree.
 * Returns 1 on success, 0 on error. */
static PyObject * BinaryTree_insert(BinaryTree * self, PyObject * new) {
	Node * newnode;

	/* Create a new container */
	newnode = Node_new();
	if ( newnode == NULL ) return NULL;

	Py_INCREF(new);
	newnode->item = new;

	self->root = Node_insert(self->root, newnode);
	if ( self->root == NULL ) {
		Py_DECREF(newnode);
		return NULL;
	}
		
	Py_RETURN_NONE;
}

/* Finds 'target' in the binary tree.
 * Returns the node containing 'target' as a new reference if it is in
 * the tree, None if it is not, or NULL upon failure.
 */
static PyObject * BinaryTree_locate(BinaryTree * self, PyObject * target) {
	Node * current = self->root;

	while ( current ) {
		switch ( PyObject_Compare(current->item, target) ) {
			case 0:
				Py_INCREF((PyObject *) current);
				return (PyObject *) current;
			case 1:
				/* Descend left */
				current = current->lchild;
				break;
			case -1:
				/* Descend right */
				current = current->rchild;
				break;
			default:
				return NULL;
		}
	}

	Py_RETURN_NONE;
}

/* Traverses the subtree with root at 'root' in-order applying
 * 'func' to every item.
 * Returns 1 on success, -1 on error.
 */
static int Node_inOrder(Node * root, PyObject * func) {
	PyObject * res;

	if ( root == NULL ) return 1;

	if ( Node_inOrder(root->lchild, func) == -1 ) return -1;

	res = PyObject_CallFunctionObjArgs(func, root->item, NULL);
	if ( res == NULL ) return -1;
	
	/* The new reference returned by the call won't be used. */
	Py_DECREF(res);

	return Node_inOrder(root->rchild, func);
}

/* Traverses the subtree with root at 'root' in pre-order applying
 * 'func' to every item.
 * Returns 1 on success, -1 on error.
 */
static int Node_preOrder(Node * root, PyObject * func) {
	PyObject * res;

	if ( root == NULL ) return 1;

	res = PyObject_CallFunctionObjArgs(func, root->item, NULL);
	if ( res == NULL ) return -1;

	Py_DECREF(res);

	if ( Node_preOrder(root->lchild, func) == -1 ) return -1;

	return Node_preOrder(root->rchild, func);
}

/* Traverses the subtree with root at 'root' in post-order
 * applying 'func' to every item.
 * Returns 1 on success, -1 on error.
 */
static int Node_postOrder(Node * root, PyObject * func) {
	PyObject * res;

	if ( root == NULL ) return 1;
	
	if ( Node_preOrder(root->lchild, func) == -1 ) return -1;
	if ( Node_preOrder(root->rchild, func) == -1 ) return -1;
	
	res = PyObject_CallFunctionObjArgs(func, root->item, NULL);
	if ( res == NULL ) return -1;

	Py_DECREF(res);
	return 1;
}

/* Traverses the binary tree in-order, applying 'func' to every item.
 * Returns None on success, NULL on failure.
 */
static PyObject * BinaryTree_inOrder(BinaryTree * self, PyObject * func) {
	if ( Node_inOrder(self->root, func) == 1 ) {
		Py_RETURN_NONE;
	}
	
	return NULL;
}

/* Traverses the binary tree in pre-order, applying 'func' to every item.
 * Returns None on success, NULL on failure.
 */
static PyObject * BinaryTree_preOrder(BinaryTree * self, PyObject * func) {
	if ( Node_preOrder(self->root, func) == 1 ) {
		Py_RETURN_NONE;
	}
	
	return NULL;
}

/* Traverses the binary tree in post-order, applying 'func' to every item.
 * Returns None on success, NULL on failure.
 */
static PyObject * BinaryTree_postOrder(BinaryTree * self, PyObject * func) {
	if ( Node_postOrder(self->root, func) == 1) {
		Py_RETURN_NONE;
	}

	return NULL;
}

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initbinarytree(void) {
	PyObject * module;

	/* NodeType setup */
	NodeType.tp_basicsize = sizeof(Node);
	NodeType.tp_name = "binarytree.Node";
	NodeType.tp_doc = "An internal data container.";
	NodeType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC;
	NodeType.tp_dealloc = (destructor) Node_dealloc;
	NodeType.tp_traverse = (traverseproc) Node_traverse;
	NodeType.tp_clear = (inquiry) Node_clear;
	NodeType.tp_free = PyObject_GC_Del;
	NodeType.tp_getset = Node_getsetters;
	NodeType.tp_members = Node_members;
	
	if ( PyType_Ready(&NodeType) < 0 ) return;

	/* BinaryTreeType setup */
	BinaryTree_sequence.sq_contains = (objobjproc) BinaryTree_contains;

	BinaryTreeType.tp_new = (newfunc) PyType_GenericNew;
	BinaryTreeType.tp_basicsize = sizeof(BinaryTree);
	BinaryTreeType.tp_name = "binarytree.BinaryTree";
	BinaryTreeType.tp_doc = "The main binary tree class.";
	BinaryTreeType.tp_flags = Py_TPFLAGS_DEFAULT |
				Py_TPFLAGS_BASETYPE |
				Py_TPFLAGS_HAVE_GC;
	BinaryTreeType.tp_dealloc = (destructor) BinaryTree_dealloc;
	BinaryTreeType.tp_methods = BinaryTree_methods;
	BinaryTreeType.tp_members = BinaryTree_members;
	BinaryTreeType.tp_as_sequence = &BinaryTree_sequence;
	BinaryTreeType.tp_traverse = (traverseproc) BinaryTree_traverse;
	BinaryTreeType.tp_clear = (inquiry) BinaryTree_clear;
	BinaryTreeType.tp_free = PyObject_GC_Del;

	if ( PyType_Ready(&BinaryTreeType) < 0 ) return;

	module = Py_InitModule3("binarytree", NULL,
				"A simple binary search tree.");
	
	Py_INCREF(&BinaryTreeType);
	PyModule_AddObject(module, "BinaryTree", (PyObject *) &BinaryTreeType);

	return;
}
