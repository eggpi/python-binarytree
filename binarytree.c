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
static Node * Node_insert(Node * root, Node * new, int * node_no);
static int Node_traverse(Node * self, visitproc visit, void * arg);
static void Node_clear(Node * self);

/* Prototypes for BinaryTreeType methods */
static void BinaryTree_dealloc(BinaryTree * self);
static int BinaryTree_traverse(BinaryTree * self, visitproc visit, void * arg);
static void BinaryTree_clear(BinaryTree * self);
static Py_ssize_t BinaryTree_length(BinaryTree * self);
static int BinaryTree_contains(BinaryTree * self, PyObject * value);

/* Protoypes for BinaryTree methods */
static PyObject * BinaryTree_insert(BinaryTree * self, PyObject * new);
static PyObject * BinaryTree_locate(BinaryTree * self, PyObject * target);

/* Left and right rotation */
static Node * rotateLeft(Node * root);
static Node * rotateRight(Node * root);

static void Node_updateHeight(Node * node);
static Node * Node_new(void);

static PyTypeObject NodeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyTypeObject BinaryTreeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyMethodDef BinaryTree_methods[] = {
	{ "insert", (PyCFunction) BinaryTree_insert, METH_O,
	"Inserts the parameter into the tree, without creating duplicates."
	},
	{"locate", (PyCFunction) BinaryTree_locate, METH_O,
	"True/False if the parameter exists in the tree."
	},
	{NULL}, /* Sentinel */
};

static PySequenceMethods BinaryTree_sequence;

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
	/* Since Node objects are not exposed, the only reference
	 * to a node should be that of his parent.
	 */
	if ( self->lchild )
		assert(self->lchild->ob_refcnt == 1);
	if ( self->rchild )
		assert(self->rchild->ob_refcnt == 1);

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
	if ( self->root )
		assert(self->root->ob_refcnt == 1);

	Py_CLEAR(self->root);

	return;
}

static Py_ssize_t BinaryTree_length(BinaryTree * self) {
	return Py_SIZE((PyObject *) self);
}

static int BinaryTree_contains(BinaryTree * self, PyObject * value) {
	PyObject * res;
	
	res = BinaryTree_locate(self, value);
	if ( res == NULL ) return -1;

	return ( res == Py_True ) ? 1 : 0;
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
 * Returns the new root of the tree, or NULL in case of error. */
static Node * Node_insert(Node * root, Node * new, int * node_no) {
	int child_height;

	switch ( PyObject_Compare(root->item, new->item) ) {
		case 0:
			return root; /* Node already in the tree */
		case 1:
			if ( root->lchild == NULL ) {
				/* Base case: simple insertion */
				root->lchild = new;
				root->balance--;

				/* Unbalancing increases height */
				if ( root->balance ) root->height++;

				*node_no += 1;				
				return root;
			}

			/* Descend left */
			child_height = root->lchild->height;

			root->lchild = Node_insert(root->lchild, new, node_no);
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
				root->rchild = new;
				root->balance++;

				/* Unbalancing increases height */
				if ( root->balance ) root->height++;
				
				*node_no += 1;
				return root;
			}

			/* Descend right.
			 * Symmetrical to the left case above.
			 */
			child_height = root->rchild->height;

			root->rchild = Node_insert(root->rchild, new, node_no);
			if ( root->rchild == NULL ) return NULL;

			if ( child_height == root->rchild->height )
				return root;

			assert(root->rchild->height == 1 + child_height);
			
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
	Node * newnode;

	/* Create a new container */
	newnode = Node_new();
	if ( newnode == NULL ) return NULL;

	Py_INCREF(new);
	newnode->item = new;

	if ( self->root != NULL ) {
		/* Insert into the existing tree */
		self->root = Node_insert(self->root, newnode, &self->ob_size);
		
		if ( self->root == NULL ) {
			Py_DECREF(newnode);
			return NULL;
		}
		
		Node_updateHeight(self->root);
	} else {
		/* First insertion */
		self->root = newnode;
		self->ob_size++;
	}

	Py_RETURN_NONE;
}

/* Finds 'target' in the binary tree.
 * Returns True/False if 'target' is or not in the tree, or NULL in
 * case of error.
 */
static PyObject * BinaryTree_locate(BinaryTree * self, PyObject * target) {
	Node * current = self->root;

	while ( current ) {
		switch ( PyObject_Compare(current->item, target) ) {
			case 0:
				Py_RETURN_TRUE;
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

	Py_RETURN_FALSE;
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
	NodeType.tp_alloc = PyType_GenericAlloc;
	NodeType.tp_dealloc = (destructor) Node_dealloc;
	NodeType.tp_traverse = (traverseproc) Node_traverse;
	NodeType.tp_clear = (inquiry) Node_clear;
	
	if ( PyType_Ready(&NodeType) < 0 ) return;

	/* BinaryTreeType setup */
	BinaryTree_sequence.sq_length = (lenfunc) BinaryTree_length;
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
	BinaryTreeType.tp_as_sequence = &BinaryTree_sequence;
	BinaryTreeType.tp_traverse = (traverseproc) BinaryTree_traverse;
	BinaryTreeType.tp_clear = (inquiry) BinaryTree_clear;

	if ( PyType_Ready(&BinaryTreeType) < 0 ) return;

	module = Py_InitModule3("binarytree", NULL,
				"A simple binary search tree.");
	
	Py_INCREF(&BinaryTreeType);
	PyModule_AddObject(module, "BinaryTree", (PyObject *) &BinaryTreeType);

	return;
}
