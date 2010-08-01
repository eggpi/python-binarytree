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
 * 'height' keeps the size of the longest path from this node to a leaf.
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
 * ob_size (defined in PyObject_VAR_HEAD) holds the number of nodes in the tree
 */
typedef struct {
	PyObject_VAR_HEAD

	Node * root;
} BinaryTree;

#define NODE_UPDATE_BALANCE(node) \
	(node)->balance = ((node)->rchild ? (node)->rchild->height : 0) \
			- ((node)->lchild ? (node)->lchild->height : 0)

#define NODE_IS_LEAF(node) !((node)->rchild || (node)->rchild)

/* Prototypes for NodeType methods */
static void Node_dealloc(Node * self);
static Node * Node_insert(Node * root, Node * new);

/* XXX - Needed for garbage collection, which I haven't managed to get to work
static int Node_traverse(Node * self, visitproc visit, void * arg);
*/
static void Node_clear(Node * self);

/* Prototypes for BinaryTreeType methods */
static void BinaryTree_dealloc(BinaryTree * self);
static PyObject * BinaryTree_insert(BinaryTree * self, PyObject * new);

/* XXX - Needed for garbage collection, which I haven't managed to get working
static int BinaryTree_traverse(BinaryTree * self, visitproc visit, void * arg);
*/
static void BinaryTree_clear(BinaryTree * self);

/* Left and right rotation */
static Node * rotateLeft(Node * root);
static Node * rotateRight(Node * root);

static void Node_updateHeight(Node * node);

static PyTypeObject NodeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyTypeObject BinaryTreeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyMethodDef BinaryTree_methods[] = {
	{ "insert", (PyCFunction) BinaryTree_insert, METH_O,
	"Inserts the parameter into the tree, without creating duplicates"
	},
	{NULL}, /* Sentinel */
};

static void Node_dealloc(Node * self) {
	Node_clear(self);
	Py_TYPE((PyObject *) self)->tp_free((PyObject *) self);

	return;
}

/*
static int Node_traverse(Node * self, visitproc visit, void * arg) {
	Py_VISIT(self->item);
	Py_VISIT((PyObject *) self->lchild);
	Py_VISIT((PyObject *) self->rchild);

	return 0;
}
*/

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

/*
static int BinaryTree_traverse(BinaryTree * self, visitproc visit, void * arg) {
	Py_VISIT(self->root);

	return 0;
}*/

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

		Node_updateHeight(root);
		NODE_UPDATE_BALANCE(root);
		
		Node_updateHeight(newroot);
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
		newroot->rchild = root;

		Node_updateHeight(root);
		NODE_UPDATE_BALANCE(root);
		
		Node_updateHeight(newroot);
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

static Node * Node_insert(Node * root, Node * new) {
	int child_height;
	Node * newchild;

	switch ( PyObject_Compare(root->item, new->item) ) {
		case 0:
			return root; /* Node already in the tree */
		case 1:
			if ( root->lchild == NULL ) {
				/* Base case: simple insertion */
				Py_INCREF(new);
				root->lchild = new;
				root->balance--;

				/* Unbalancing increases height */
				if ( root->balance ) root->height++;
				
				return root;
			}

			/* Descend left */
			child_height = root->lchild->height;

			newchild = Node_insert(root->lchild, new);
			if ( newchild == NULL ) return NULL;

			/* Subtree has already been rebalanced. */
			if ( root->lchild->balance == 0 ) {
				root->lchild = newchild;
				return root;
			}

			/* No change in height.
			 * Either the insertion balanced the left subtree,
			 * or there has already been a rebalancing somewhere
			 * along the way up here.
			 * Nothing left to do but except updating lchild.
			 */
			if ( child_height == root->lchild->height ) {
				root->lchild = newchild;
				return root;
			}

			assert(root->lchild->height == 1 + child_height);
			assert(newchild == root->lchild);
			
			/* Fixing balance and height of the root */
			root->balance--;
			root->height++;

			/* Do we need to rebalance? */
			if ( root->balance > -2 ) return root;

			if ( root->lchild->balance == 1 ) {
				/* Left-right case */
				root->lchild = rotateLeft(root->lchild);
			}

			/* Left-left case */
			return rotateRight(root);

		case -1:
			if ( root->rchild == NULL ) {
				/* Base case: simple insertion */
				Py_INCREF(new);
				root->rchild = new;
				root->balance++;

				/* Unbalancing increases height */
				if ( root->balance ) root->height++;
				
				return root;
			}

			/* Descend right.
			 * Symmetrical to the left case above.
			 */
			child_height = root->rchild->height;

			newchild = Node_insert(root->rchild, new);
			if ( newchild == NULL ) return NULL;

			if ( child_height == root->rchild->height ) {
				root->rchild = newchild;
				return root;
			}

			if ( root->rchild->balance == 0 ) {
				root->rchild = newchild;
				return root;
			}

			assert(root->rchild->height == 1 + child_height);
			assert(newchild == root->rchild);
			
			root->balance++;
			root->height++;

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
	Node * newnode, * newroot = NULL;

	/* Create a new container */
	newnode = PyObject_New(Node, &NodeType);	
	if ( newnode == NULL ) return NULL;

	/* Initializing as a leaf */
	newnode->lchild = NULL;
	newnode->rchild = NULL;
	newnode->balance = 0;
	newnode->height = 1;

	Py_INCREF(new);
	newnode->item = new;

	if ( self->root != NULL ) {
		/* Insert into the existing tree */
		newroot = Node_insert(self->root, newnode);
		
		if ( newroot == NULL ) {
			Py_DECREF(newnode);
			return NULL;
		}
		
		self->root = newroot;
		Node_updateHeight(self->root);
	} else {
		/* First insertion */
		self->root = newnode;
	}

	self->ob_size++;
	Py_RETURN_NONE;
}

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initbinarytree(void) {
	PyObject * module;

	/* NodeType setup */
	NodeType.tp_new = (newfunc) PyType_GenericNew;
	NodeType.tp_basicsize = sizeof(Node);
	NodeType.tp_name = "binarytree.Node";
	NodeType.tp_doc = "An internal data container.";
	NodeType.tp_flags = Py_TPFLAGS_DEFAULT;
	NodeType.tp_alloc = PyType_GenericAlloc;
	NodeType.tp_dealloc = (destructor) Node_dealloc;
	
	if ( PyType_Ready(&NodeType) < 0 ) return;

	/* BinaryTreeType setup */
	BinaryTreeType.tp_new = PyType_GenericNew;
	BinaryTreeType.tp_basicsize = sizeof(BinaryTree);
	BinaryTreeType.tp_name = "binarytree.BinaryTree";
	BinaryTreeType.tp_doc = "The main binary tree class.";
	BinaryTreeType.tp_flags = Py_TPFLAGS_DEFAULT |
				Py_TPFLAGS_BASETYPE;
	BinaryTreeType.tp_dealloc = (destructor) BinaryTree_dealloc;
	BinaryTreeType.tp_methods = BinaryTree_methods;

	if ( PyType_Ready(&BinaryTreeType) < 0 ) return;

	module = Py_InitModule3("binarytree", NULL,
				"A simple binary search tree.");
	
	Py_INCREF(&BinaryTreeType);
	PyModule_AddObject(module, "BinaryTree", (PyObject *) &BinaryTreeType);

	return;
}
