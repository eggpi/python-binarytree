/*  Copyright 2010, 2011 Guilherme Gonçalves (guilherme.p.gonc@gmail.com)
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

/* A container for Python objects.
 * 'item' holds the actual data, a reference to a PyObject.
 * 'lchild' and 'rchild' refer to the left and right child nodes of a given
 * node.
 * 'balance' is the height balance of the node. -1 for left-unbalanced, 0 for
 * balanced and +1 for right-unbalanced.
 * 'height' keeps the maximum number of nodes between a node and a leaf.
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

/* Subtrees safely implement the recursive notion of a binary tree, ie, that
 * a node's left and right children are themselves roots of smaller trees.
 * It might be tempting to expose a Node object's left and right subtrees as
 * BinaryTrees referencing these nodes as their roots.
 * Unfortunately then, any methods that modify the subtree would also modify
 * the original tree -- in particular, insertions and removals would mess up
 * the 'height' and 'balance' fields of all nodes above the root of the
 * subtree in the original tree.
 * So we define a new type, a Subtree, which is basically an immutable
 * BinaryTree that can be safely shallow-copied into a full-fledged
 * BinaryTree.
 */
typedef BinaryTree Subtree;

#define NODE_UPDATE_BALANCE(node) \
	(node)->balance = ((node)->rchild ? (node)->rchild->height : 0) \
			- ((node)->lchild ? (node)->lchild->height : 0)

#define NODE_SET_LEAF(node) do { \
				(node)->height = 1; \
				(node)->rchild = NULL; \
				(node)->lchild = NULL; \
				(node)->balance = 0; \
			 } while (0);

#define NODE_IS_LEAF(node) ((node)->height == 1 ) && \
			!((node)->lchild || (node)->rchild) && \
			((node)->balance == 0)

/* Prototypes for NodeType methods */
static Node * Node_new(void);
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
static Node * Node_copytree(Node * root);
static int Node_inOrder(Node * root, PyObject * func);
static int Node_preOrder(Node * root, PyObject * func);
static int Node_postOrder(Node * root, PyObject * func);

/* Prototypes for BinaryTreeType methods */
static int BinaryTree_init(BinaryTree * t, PyObject * args, PyObject * kwds);
static void BinaryTree_dealloc(BinaryTree * self);
static int BinaryTree_traverse(BinaryTree * self, visitproc visit, void * arg);
static void BinaryTree_clear(BinaryTree * self);
static int BinaryTree_contains(BinaryTree * self, PyObject * value);

/* Protoypes for BinaryTree methods */
static PyObject * BinaryTree_insert(BinaryTree * self, PyObject * new);
static PyObject * BinaryTree_remove(BinaryTree * self, PyObject * target);
static PyObject * BinaryTree_locate(BinaryTree * self, PyObject * target);
static PyObject * BinaryTree_inOrder(BinaryTree * self, PyObject * func);
static PyObject * BinaryTree_preOrder(BinaryTree * self, PyObject * func);
static PyObject * BinaryTree_postOrder(BinaryTree * self, PyObject * func);

/* Prototypes for Subtree methods */
static PyObject * Subtree_maketree(Subtree * self);

/* Left and right rotation */
static Node * rotateLeft(Node * root);
static Node * rotateRight(Node * root);

static void Node_updateHeight(Node * node);

static PyTypeObject NodeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyGetSetDef Node_getsetters[] = {
	{"left_child",
	(getter) Node_lchild,
	NULL,
	"The left subtree of this node."
	},
	{"right_child",
	(getter) Node_rchild,
	NULL,
	"The right subtree of this node."
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
	{ "remove", (PyCFunction) BinaryTree_remove, METH_O,
	"Removes the parameter from the tree."
	},
	{"locate", (PyCFunction) BinaryTree_locate, METH_O,
	"The Node that contains the parameter if it is in the tree, or None."
	},
	{"in_order", (PyCFunction) BinaryTree_inOrder, METH_O,
	"in_order(callable) -> apply 'callable' to each item, in-order."
	},
	{"pre_order", (PyCFunction) BinaryTree_preOrder, METH_O,
	"pre_order(callable) -> apply 'callable' to each item, pre-order."
	},
	{"post_order", (PyCFunction) BinaryTree_postOrder, METH_O,
	"post_order(callable) -> apply 'callable' to each item, post-order."
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

static PyTypeObject SubtreeType = {
	PyObject_HEAD_INIT(NULL)
};

static PyMethodDef Subtree_methods[] = {
	{"locate", (PyCFunction) BinaryTree_locate, METH_O,
	"The Node that contains the parameter if it is in the tree, or None."
	},
	{"in_order", (PyCFunction) BinaryTree_inOrder, METH_O,
	"in_order(callable) -> apply 'callable' to each node, in-order."
	},
	{"pre_order", (PyCFunction) BinaryTree_preOrder, METH_O,
	"pre_order(callable) -> apply 'callable' to each node, pre-order."
	},
	{"post_order", (PyCFunction) BinaryTree_postOrder, METH_O,
	"post_order(callable) -> apply 'callable' to each item, post-order."
	},
	{"make_tree", (PyCFunction) Subtree_maketree, METH_NOARGS,
	"Returns a shallow copy of this subtree as a new BinaryTree."
	},
	{NULL}, /* Sentinel */
};

/* Returns the left subtree of a given node, as a new reference */
static BinaryTree * Node_lchild(Node * self) {
	BinaryTree * subtree;

	subtree = PyObject_GC_New(Subtree, &SubtreeType);
	if ( subtree == NULL ) return NULL;

	Py_XINCREF(self->lchild);
	subtree->root = self->lchild;
	PyObject_GC_Track((PyObject *) subtree);

	return subtree;
}

/* Returns the right subtree of a given node, as a new reference */
static BinaryTree * Node_rchild(Node * self) {
	BinaryTree * subtree;

	subtree = PyObject_GC_New(Subtree, &SubtreeType);
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

static int BinaryTree_init(BinaryTree * t, PyObject * args, PyObject * kwds) {
	PyObject * elements = NULL, * iter, * item;

	if ( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError,
		"BinaryTree initializer does not accept keyword arguments");
		return -1;
	}

	if (! PyArg_ParseTuple(args, "|O", &elements) ) {
		return -1;
	}

	if ( elements ) {
		iter = PyObject_GetIter(elements);
		if (! iter ) return -1;

		while ( (item = PyIter_Next(iter)) != NULL ) {
			if (! BinaryTree_insert(t, item) ) {
				Py_DECREF(item);
				Py_DECREF(iter);
				return -1;
			}

			Py_DECREF(item);
		}
	}

	return 1;
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
	NODE_SET_LEAF(newnode);

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

			if ( Py_EnterRecursiveCall(" in insertion") != 0 )
				return NULL;

			root->lchild = Node_insert(root->lchild, new);
			if ( root->lchild == NULL ) return NULL;
			Py_LeaveRecursiveCall();

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

			if ( Py_EnterRecursiveCall(" in insertion") != 0 )
				return NULL;

			root->rchild = Node_insert(root->rchild, new);
			if ( root->rchild == NULL ) return NULL;
			Py_LeaveRecursiveCall();

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

/* Removes the node that contains 'target' from tree whose
 * root is 'root'.
 * Returns the new root of the tree (which may be NULL).
 */
static Node * Node_remove(Node * root, PyObject * target) {
	int child_height = 0, cmp;
	Node * rm = NULL;
	PyObject * tmp;

	if ( root == NULL ) return NULL;

	cmp = PyObject_Compare(root->item, target);
	if ( cmp == 0 ) {
		if ( NODE_IS_LEAF(root) ) {
			/* Simple removal of a leaf */
			Py_DECREF(root);
			return NULL;
		}

		/* Is root a Node with only one child? */
		if ( root->lchild != NULL ) {
			if ( root->rchild == NULL )
				rm = root->lchild;
		} else {
			if ( root->rchild != NULL )
				rm = root->rchild;
		}

		if ( rm != NULL ) {
			assert(NODE_IS_LEAF(rm));

			/* Yes, copy the item from the child to
			 * the root. */
			Py_INCREF(rm->item);
			root->item = rm->item;

			/* Deleting the leaf. */
			if ( rm == root->lchild )
				Py_CLEAR(root->lchild);
			else
				Py_CLEAR(root->rchild);

			/* root becomes a leaf */
			NODE_SET_LEAF(root);

			return root;
		}

		/* Root has both lchild and rchild */
		rm = root->lchild;
		while ( rm->rchild != NULL ) {
			rm = rm->rchild;
		}

		/* rm and root swap references */
		tmp = rm->item;
		rm->item = root->item;
		root->item = tmp;

		/* Fall through to left-descend */
		cmp = 1;

	} else { /* if ( cmp == 0 ) */

		if ( cmp == -1 ) {
			/* Descend right */
			if ( root->rchild )
				child_height = root->rchild->height;

			if ( Py_EnterRecursiveCall(" in removal") != 0 )
				return NULL;

			root->rchild = Node_remove(root->rchild, target);
			if ( root->rchild == NULL && PyErr_Occurred() != NULL )
				return NULL;
			Py_LeaveRecursiveCall();

			/* Fixing balance and height of the root */
			Node_updateHeight(root);
			NODE_UPDATE_BALANCE(root);

			if ( root->rchild != NULL ) {
				if ( child_height == root->rchild->height )
					return root;

				assert(root->rchild->height == child_height -1);
			}

			/* Do we need to rebalance? */
			if ( root->balance > -2 ) return root;

			if ( root->lchild->balance != 1 ) {
				return rotateRight(root);
			}

			root->lchild = rotateLeft(root->lchild);
			return rotateRight(root);
		}
	}

	if ( cmp != 1 ) {
		assert(PyErr_Occurred() != NULL);
		return NULL; /* Unexpected result in comparison */
	}

	/* Descend left */
	if ( root->lchild )
		child_height = root->lchild->height;

	if ( Py_EnterRecursiveCall(" in removal") != 0 )
		return NULL;

	root->lchild = Node_remove(root->lchild, target);
	if ( root->lchild == NULL && PyErr_Occurred() != NULL )
		return NULL;
	Py_LeaveRecursiveCall();

	/* Fixing balance and height of the root */
	Node_updateHeight(root);
	NODE_UPDATE_BALANCE(root);

	if ( root->lchild != NULL ) {
		if ( child_height == root->lchild->height )
			return root;

		assert(root->lchild->height == child_height - 1);
	}

	/* Do we need to rebalance? */
	if ( root->balance < 2 ) return root;

	if ( root->rchild->balance != -1 ) {
		return rotateLeft(root);
	}

	root->rchild = rotateRight(root->rchild);
	return rotateLeft(root);
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

static PyObject * BinaryTree_remove(BinaryTree * self, PyObject * target) {
	self->root = Node_remove(self->root, target);
	if ( self->root == NULL && PyErr_Occurred() != NULL )
		return NULL;

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

	/* Traverse left subtree */
	if ( Py_EnterRecursiveCall(" in in-order traversal") != 0 )
		return -1;

	if ( Node_inOrder(root->lchild, func) == -1 ) return -1;
	Py_LeaveRecursiveCall();

	/* Process this node */
	res = PyObject_CallFunctionObjArgs(func, root->item, NULL);
	if ( res == NULL ) return -1;

	/* The new reference returned by the call won't be used. */
	Py_DECREF(res);

	/* Traverse right subtree */
	if ( Py_EnterRecursiveCall(" in in-order traversal") != 0 )
		return -1;

	if ( Node_inOrder(root->rchild, func) == -1 ) return -1;
	Py_LeaveRecursiveCall();

	return 1;
}

/* Traverses the subtree with root at 'root' in pre-order applying
 * 'func' to every item.
 * Returns 1 on success, -1 on error.
 */
static int Node_preOrder(Node * root, PyObject * func) {
	PyObject * res;

	if ( root == NULL ) return 1;

	/* Process this node */
	res = PyObject_CallFunctionObjArgs(func, root->item, NULL);
	if ( res == NULL ) return -1;

	Py_DECREF(res);

	/* Traverse left subtree */
	if ( Py_EnterRecursiveCall(" in pre-order traversal") != 0 )
		return -1;

	if ( Node_preOrder(root->lchild, func) == -1 ) return -1;
	Py_LeaveRecursiveCall();

	/* Traverse right subtree */
	if ( Py_EnterRecursiveCall(" in pre-order traversal") != 0 )
		return -1;

	if ( Node_preOrder(root->rchild, func) == -1 ) return -1;
	Py_LeaveRecursiveCall();

	return 1;
}

/* Traverses the subtree with root at 'root' in post-order
 * applying 'func' to every item.
 * Returns 1 on success, -1 on error.
 */
static int Node_postOrder(Node * root, PyObject * func) {
	PyObject * res;

	if ( root == NULL ) return 1;

	/* Traverse left subtree */
	if ( Py_EnterRecursiveCall(" in post-order traversal") != 0 )
		return -1;

	if ( Node_postOrder(root->lchild, func) == -1 ) return -1;
	Py_LeaveRecursiveCall();

	/* Traverse right subtree */
	if ( Py_EnterRecursiveCall(" in post-order traversal") != 0 )
		return -1;

	if ( Node_postOrder(root->rchild, func) == -1 ) return -1;
	Py_LeaveRecursiveCall();

	/* Process this node */
	res = PyObject_CallFunctionObjArgs(func, root->item, NULL);
	if ( res == NULL ) return -1;

	Py_DECREF(res);
	return 1;
}

/* Creates a shallow copy of the tree starting at 'root'. Returns the
 * new (copied) root as a new reference or NULL on failure.
 */
static Node * Node_copytree(Node * root) {
	Node * newroot;

	newroot = Node_new();
	if ( newroot == NULL ) return NULL;

	memcpy((void *) newroot, (void *) root, sizeof(Node));
	Py_INCREF(newroot->item);

	if ( newroot->lchild != NULL ) {
		if ( Py_EnterRecursiveCall(" in copytree") != 0 )
			return NULL;

		newroot->lchild = Node_copytree(newroot->lchild);
		if ( newroot->lchild == NULL ) {
			Node_dealloc(newroot);
			return NULL;
		}
		Py_LeaveRecursiveCall();
	}

	if ( newroot->rchild != NULL ) {
		if ( Py_EnterRecursiveCall(" in copytree") != 0 )
			return NULL;

		newroot->rchild = Node_copytree(newroot->rchild);
		if ( newroot->rchild == NULL ) {
			Node_dealloc(newroot);
			return NULL;
		}
		Py_LeaveRecursiveCall();
	}

	return newroot;
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

/* Copies the contents of a Subtree into a BinaryTree.
 * Returns a reference to the new BinaryTree or NULL upon
 * failure.
 */
static PyObject * Subtree_maketree(Subtree * self) {
	BinaryTree * new;

	new = PyObject_GC_New(BinaryTree, &BinaryTreeType);
	if ( new == NULL ) return NULL;

	if ( self->root )
		new->root = Node_copytree(self->root);

	return (PyObject *) new;
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
	NodeType.tp_doc = "A data container.";
	NodeType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC;
	NodeType.tp_dealloc = (destructor) Node_dealloc;
	NodeType.tp_traverse = (traverseproc) Node_traverse;
	NodeType.tp_clear = (inquiry) Node_clear;
	NodeType.tp_free = PyObject_GC_Del;
	NodeType.tp_getset = Node_getsetters;
	NodeType.tp_members = Node_members;

	if ( PyType_Ready(&NodeType) < 0 ) return;

	/* BinaryTreeType setup */
	PyDoc_STRVAR(binary_tree_doc,
	"The main binary tree class.\n\
	BinaryTree() -> empty binary tree.\n\
	BinaryTree(iterable) -> Tree containing iterable's items.");

	BinaryTree_sequence.sq_contains = (objobjproc) BinaryTree_contains;

	BinaryTreeType.tp_init = (initproc) BinaryTree_init;
	BinaryTreeType.tp_new = (newfunc) PyType_GenericNew;
	BinaryTreeType.tp_basicsize = sizeof(BinaryTree);
	BinaryTreeType.tp_name = "binarytree.BinaryTree";
	BinaryTreeType.tp_doc = binary_tree_doc;
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

	/* Subtree setup. We don't inherit from BinaryTree because
	 * Subtrees have different methods.
	 */
	SubtreeType.tp_basicsize = sizeof(Subtree);
	SubtreeType.tp_name = "binarytree.Subtree";
	SubtreeType.tp_doc = "A read-only subtree of a BinaryTree.";
	SubtreeType.tp_methods = Subtree_methods;
	SubtreeType.tp_flags = Py_TPFLAGS_DEFAULT |
				Py_TPFLAGS_BASETYPE |
				Py_TPFLAGS_HAVE_GC;
	SubtreeType.tp_dealloc = (destructor) BinaryTree_dealloc;
	SubtreeType.tp_members = BinaryTree_members;
	SubtreeType.tp_as_sequence = &BinaryTree_sequence;
	SubtreeType.tp_traverse = (traverseproc) BinaryTree_traverse;
	SubtreeType.tp_clear = (inquiry) BinaryTree_clear;
	SubtreeType.tp_free = PyObject_GC_Del;

	if ( PyType_Ready(&SubtreeType) < 0 ) return;

	module = Py_InitModule3("binarytree", NULL,
				"A self-balancing binary search tree.");

	Py_INCREF(&BinaryTreeType);
	PyModule_AddObject(module, "BinaryTree", (PyObject *) &BinaryTreeType);

	Py_INCREF(&NodeType);
	PyModule_AddObject(module, "Node", (PyObject *) &NodeType);

	Py_INCREF(&SubtreeType);
	PyModule_AddObject(module, "Subtree", (PyObject *) &SubtreeType);

	return;
}
