problem:

Design an iterator over a binary search tree with the following rules:

Elements are visited in ascending order (i.e. an in-order traversal)
next() and hasNext() queries run in O(1) time in average.
Have you met this question in a real interview? Yes
Example
For the following binary search tree, in-order traversal by using iterator is [1, 6, 10, 11, 12]

   10
 /    \
1      11
 \       \
  6       12
  
key:

mine: 

public class BSTIterator {
    private Stack<TreeNode> stack = new Stack<>();
    TreeNode next = null;
    void AddNodeToStack(TreeNode root) {
        while (root != null) {
            stack.push(root);
            root = root.left;
        }
    } 
    
    // @param root: The root of binary tree.
    public BSTIterator(TreeNode root) {
        next = root;
    }

    //@return: True if there has next node, or false
    public boolean hasNext() {
        if (next != null) {
            AddNodeToStack(next);
            next = null;
        }
        return !stack.isEmpty();
    }
    
    //@return: return next node
    public TreeNode next() {
        if (!hasNext()) {
            return null;
        }
        TreeNode cur = stack.pop();
        next = cur.right;
        return cur;
    }
}
9 chapter: 


public class BSTIterator {
    private Stack<TreeNode> stack = new Stack<>();
    TreeNode next = null;
    void AddNodeToStack(TreeNode root) {
        while (root != null) {
            stack.push(root);
            root = root.left;
        }
    } 
    
    // @param root: The root of binary tree.
    public BSTIterator(TreeNode root) {
        next = root;
    }

    //@return: True if there has next node, or false
    public boolean hasNext() {
        if (next != null) {
            AddNodeToStack(next);
            next = null;
        }
        return !stack.isEmpty();
    }
    
    //@return: return next node
    public TreeNode next() {
        if (!hasNext()) {
            return null;
        }
        TreeNode cur = stack.pop();
        next = cur.right;
        return cur;
    }
}
