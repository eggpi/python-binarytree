import unittest
import binarytree
from collections import deque

# Traverses a tree in transversal order. Returns a deque of the
# visited elements in order.
def transversal(tree):
	queue = deque()
	items = deque()

	queue.append(tree)
	while queue:
		tree = queue.popleft()

		if tree.root is not None:
			items.append(tree.root.item)
			
			sub = tree.root.left_child
			if sub.root is not None:
				queue.append(sub)
			sub = tree.root.right_child
			if sub.root is not None:
				queue.append(sub)
	return items

class BinaryTreeTest(unittest.TestCase):
	def setUp(self):
		''' Build the test tree. '''

		self.tree = binarytree.BinaryTree()

		# A collection of items to add, containing duplicates.
		self.items = (56, 56, 54, 78, 73, 70, 80, 74, 85, 85,
				83, 71, 62, 60, 12, 44, 57, 0, 1)
	
		
		for i in self.items:
			self.tree.insert(i)
	
	def testSetUp(self):
		''' Tests building the tree '''

		self.assertEquals(transversal(self.tree),
			deque([73, 56, 78, 44, 62, 74, 83, 1, 54, 60, 70,
				80, 85, 0, 12, 57, 71]))
	
	def testLocate(self):
		''' Tests if all inserted items are located, and if locating a
		nonexisting item fails. '''

		for i in self.items:
			self.assertTrue(i in self.tree)

		self.assertTrue(99 not in self.items)	
		self.assertFalse(99 in self.tree)
	
	def testSubtree(self):
		''' Tests the subtree implementation. '''

		# Test creating a new subtree
		right = self.tree.root.right_child
		self.assertEquals(transversal(right),
			deque([78, 74, 83, 80, 85]))

		# Test copying subtree into a new tree
		right_tree = right.make_tree()
		self.assertFalse(right is right_tree)

		# Test two simple insertions
		right_tree.insert(75)
		right_tree.insert(73)
		self.assertEquals(transversal(right_tree),
			deque([78, 74, 83, 73, 75, 80, 85]))

		# Nothing should have changed in the other trees
		self.assertTrue(73 not in right)
		self.assertTrue(75 not in right)
		self.assertTrue(75 not in self.tree)
	
	def testInOrder(self):
		''' Tests the inorder traversal of self.tree '''

		res = deque((0, 1, 12, 44, 54, 56, 57, 60, 62, 70, 71,
			73, 74, 78, 80, 83, 85))

		self.tree.in_order(lambda x:
					self.assertEquals(x, res.popleft()))
	
	def testPreOrder(self):
		''' Tests the pre-order traversal of self.tree '''
		
		res = deque((73, 56, 44, 1, 0, 12, 54, 62, 60, 57, 70, 71,
			78, 74, 83, 80, 85))

		self.tree.pre_order(lambda x:
					self.assertEquals(x, res.popleft()))
	
	def testPostOrder(self):
		''' Tests the pos-order traversal of self.tree '''

		res = deque((0, 12, 1, 54, 44, 57, 60, 71, 70, 62, 56, 74,
			80, 85, 83, 78, 73))

		self.tree.post_order(lambda x:
					self.assertEquals(x, res.popleft()))

if __name__ == "__main__":
	unittest.main()

