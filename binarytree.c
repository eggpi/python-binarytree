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

typedef struct _Node {
	PyObject_HEAD
	
	PyObject * item;
	struct _Node * lchild, * rchild;
} Node;

static void Node_dealloc(Node * self);
static int Node_traverse(Node * self, visitproc visit, void * arg);
static void Node_clear(Node * self);

static PyTypeObject NodeType = {
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

	if ( PyType_Ready(&NodeType) < 0 ) return;

	module = Py_InitModule3("binarytree", NULL,
				"A simple binary search tree.");
	
/*
	Py_INCREF(&NodeType);
	PyModule_AddObject(module, "Node", (PyObject *) &NodeType);
*/

	return;
}
