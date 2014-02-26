//
//  main.c
//  adv_data_structures_hw_1
//
//  Created by Samy Vilar.
//  Copyright (c) 2012 . All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 1024 /* size of block for node allocation set to desired value .... */

typedef int tree_key_type;
typedef char tree_object_type;

/*
    CONVENTION:
        - @@OBJECTS are stored within node->left, type-casted to what ever type required!!@@
        - a node is a leaf if node->right == NULL
        - an empty_tree is root->left == NULL
        - a tree has only a single node if root->left != NULL(OBJECT) AND root->right == NULL
        - this is a height-balance-tree where the height difference between each child must not differ by more than one
        - the weight node of n is n->weight = n->left->weight + n->right->weight
 */

typedef struct tree_node_type
{
    struct tree_node_type
            *left,     /* left child */
            *right;    /* right child */

    unsigned int
            height,    /* height of the tree */
            weight;    /* weight of the tree, the number of leaves below it */

} tree_node_type;


tree_node_type *recycled_nodes_ll    = NULL,  /* Link List of freed nodes, re-used, when allocating new nodes. */
               *allocated_node_block = NULL;  /* A block of allocated nodes. */

unsigned int number_of_allocated_nodes = 0;

void recycle_node(tree_node_type *node);
void recycle_node(tree_node_type *node)
{
    node->left = recycled_nodes_ll; /* Extend the LinkList of de-allocated nodes.... */
    recycled_nodes_ll = node;       /* Set the newly de-allocated node at the top.   */
}

void free_blocks()
{
    tree_node_type *tree = allocated_node_block;
    while (tree)
    {
        tree = *(tree_node_type **)(&allocated_node_block[BLOCK_SIZE]);
        free(allocated_node_block);
        allocated_node_block = tree;
    }
}


tree_node_type *allocate_node(void);
tree_node_type *allocate_node(void)
{
    /* we have exhausted both previously freed nodes as well as the allocated pool, so create new pool. */
    if (!number_of_allocated_nodes) /* This should be unlikely for large Block sizes ... please use a large block.*/
    {
        tree_node_type *previous_block = allocated_node_block; /* track previous block */
        allocated_node_block = malloc((sizeof(tree_node_type) * BLOCK_SIZE) + sizeof(void *)); /* allocate new block + space for addr of prev block */
        *(tree_node_type **)(&allocated_node_block[BLOCK_SIZE]) = previous_block; /* save previous block */

        number_of_allocated_nodes = BLOCK_SIZE;
    }

    tree_node_type *temp;

    /* Re-using previously freed nodes ... */
    if (recycled_nodes_ll)
    {
        temp = recycled_nodes_ll;    /* Get the latest de-allocated node */
        recycled_nodes_ll = recycled_nodes_ll->left; /* move to the next available node ...*/
    }
    else
        temp = &allocated_node_block[--number_of_allocated_nodes];

    temp->left   = NULL;
    temp->height = 0;
    temp->weight = 1;
    return temp;
}

char *last_line = "THIS IS THE LAST LINE ...";
typedef tree_node_type text_t;

static tree_node_type *stack[100] = {NULL}; /* common stack used to keep track of our path as we descend down ... */

#define is_index_not_ok(txt, index)  !txt                     || /* check if txt is NULL */              \
                                     (index < 1)              || /* check if the index is less than 1*/  \
                                     (index > length_text(txt))  /* check if the index does not exceed length of the text */


/*
    AS PRE-DEFINE BY THE ASSIGNMENT.
        returns the number of lines of the current text.

    The length of the text is simply the weight of the root - 1 to account
    for the last line ...
 */
int length_text(text_t *txt);
int length_text(text_t *txt)
{   return txt->weight - 1; }



/*
    AS PRE-DEFINE BY THE ASSIGNMENT.
        creates an empty text, whose length is 0.
        and has an empty line.
*/
text_t *create_text(void);
text_t *create_text(void)
{
    text_t *root = allocate_node();
    root->height = 0;
    root->weight = 1; /* single last line marker */

    root->right = NULL; /* mark as a leaf */
    root->left = (tree_node_type  *)last_line; /* insert last line marker. */

    return root;
}

/*
    Recycle complete tree,
    THIS DOES NOT DE-ALLOCATE NODE BLOCKS!
 */
void recycle_text(text_t *txt)
{
    unsigned int stack_index = 0;
    stack[stack_index++] = txt;
    while (stack_index)
    {
        txt = stack[--stack_index];
        if (txt->right) /* its not a leaf */
        {
            stack[stack_index++] = txt->right;
            stack[stack_index++] = txt->left;
        }

        recycle_node(txt);
    }
}


/*
     since things like insert, delete, and find all have to look for a node,
     I've decided to implement a sub-routing that does the search accordingly,
     it will return NULL if index or tree are invalid.

     @tree: is just the txt
     @index: the line number
     @a reference to an int used to store the current stack index,
        to be used by things like insert and delete to re-balance the tree.
     @@IT DOES NOT PUSH THE LEAF!@@
*/
tree_node_type *find_node(tree_node_type * tree, int index, unsigned int *stack_index);
tree_node_type *find_node(tree_node_type * tree, int index, unsigned int *stack_index)
{
    if (is_index_not_ok(tree, index))  /* check the index is ok as well as the tree ... */
        return NULL;

    /*
       The search would go down either left or right depending on the
       the weights of the left and right child, we update the index if we go right
       other wise leave it as is and go left.

       Fundamentally if we have a node *n with a weight n->weight,
       then left child would contain
           [1, n->right->weight]       where the 1 is relative, only applicable at the left most leafs.
       then right child would contain
           [n->right->weight, n->weight[

       if we go right we have to normalize our index, index -= n->left->weight
    */

    while (tree->right) /* while we are not in a leaf */
    {
        if (stack_index)                    /* if stack_index was supplied push in push all the       */
            stack[(*stack_index)++] = tree; /* nodes visited unto the stack, excluding the leaf. */

        if (index <= tree->left->weight) /* if the line we are looking for is less than or equal  */
            tree = tree->left;           /* to the last line on the left (its weight) then go to the left */
        else
        {   /* otherwise the line must be on the right */
            index -= tree->left->weight; /* normalize index */
            tree = tree->right;
        }
    }

    return tree;

}

/*
    AS PRE-DEFINE BY THE ASSIGNMENT ...

    gets the line of number index, if such
        a line exists, or returns NULL if it doesn't exist, or giving a NULL as a tree.
 */
char *get_line(text_t *txt, int index);
char *get_line(text_t *txt, int index)
{
    txt = find_node(txt, index, NULL);      /* we don't need to use the stack... */

    return txt ? (char *)txt->left : NULL; /* if we found a node return line if not return NULL */
}



/*
    Rotations are done by simply updating the pointers
    and weights accordingly.
 */
void left_rotation(tree_node_type *tree);
void left_rotation(tree_node_type *tree)
{
    tree_node_type *left_node = tree->left; /* back-up original left node */

    tree->left = tree->right;               /* move right node to the left */
    tree->right = tree->right->right;       /* move right-right node up ... we don't need to updates its weight */

    /* we need to updates its weight since we've moved the tree->right->right to tree->right */
    tree->left->weight = tree->weight - tree->right->weight;

    tree->left->right = tree->left->left;   /* move original right->left node to new left->right  */
    tree->left->left = left_node;           /* move original left down another level to the left  */
}

void right_rotation(tree_node_type *tree);
void right_rotation(tree_node_type *tree)
{
    tree_node_type *right_node = tree->right; /* remember the right node ...*/

    tree->right = tree->left;                 /* move left node to the right */
    tree->left = tree->left->left;            /* move left-left node up */

    /* update weight again since we've moved tree->left->left from tree->left to tree->right */
    tree->right->weight = tree->weight - tree->left->weight;

    tree->right->left = tree->right->right; /* move original left->right node unto right->left */
    tree->right->right = right_node;        /* move original right node unto new right->right node */
}


/*
    This will re-balance anything on the stack shared among all the sub routines,
    since so many things require re-balancing, like: insert, append, delete.
    I decided to make it a sub-routing.

    Being we are using a height balance tree, and the height cannot differ by more than 1
    there are 2 scenarios, each containing another 2 scenarios for a total of 4.

    a) left sub-tree is longer
        1) left->left subtree longer then left-right subtree
            / \             / \
           /         =>    /   \
          /
        2) left->right subtree longer then left-left subtree
            / \             / \       / \
           / \       =>    /     =>  /   \
              \           /

   b) right sub-tree is longer
        1) right->right heavy tree
            / \            / \
               \     =>   /   \
                \
        2) right->left heavy tree

            / \            / \          / \
             / \    =>        \   =>   /   \
            /                  \


   note rotations will update the weights accordingly, but if no rations where made
   we still need to update them, all the way up to the root.

 */
void re_balance_tree(unsigned int stack_index);
void re_balance_tree(unsigned int stack_index)
{
    if (!stack_index) /* nothing on the stack, so do nothing ;) */
        return ;

    int tmp_height;
    tree_node_type *tree;

    while (stack_index)
    {
        tree = stack[--stack_index]; /* pop a node */
        tree->weight = tree->left->weight + tree->right->weight; /* update weights for rotations ... */

        if  ((tree->left->height - tree->right->height) == 2) /* the difference is greater then one left is longer (+)*/
        {
            if (tree->left->left->height - tree->right->height == 1) /* we have a left-left heavy tree */
            {
                right_rotation(tree); /* single right rotation to balance */
                tree->right->height = tree->right->left->height + 1;
                tree->height = tree->right->height + 1;
            }
            else /* we have left->right heavy tree convert to left->left then do right rotation */
            {
                left_rotation(tree->left);
                right_rotation(tree);
                tmp_height = tree->left->left->height;
                tree->left->height  = tmp_height + 1;
                tree->right->height = tmp_height + 1;
                tree->height = tmp_height + 2;
            }
        }
        else if (tree->left->height - tree->right->height == -2) /* the difference is greater then one, right is longer (-) */
        {
            if (tree->right->right->height - tree->left->height == 1) /* we have right-right heavy tree */
            {
                left_rotation(tree);
                tree->left->height = tree->left->right->height + 1;
                tree->height = tree->left->height + 1;
            }
            else /* we have right-left heavy tree, transform to right-right first then balance */
            {
                right_rotation(tree->right);
                left_rotation(tree);
                tmp_height = tree->right->right->height;
                tree->left->height  = tmp_height + 1;
                tree->right->height = tmp_height + 1;
                tree->height = tmp_height + 2;
            }
        }
        else /* heights don't differ by more than one still update heights. */
        {
            if (tree->left->height > tree->right->height)
                tree->height = tree->left->height + 1;
            else
                tree->height = tree->right->height + 1;
        }
    }
}



/*
    AS PRE-DEFINE BY THE ASSIGNMENT.
    appends new line as new
        last line.
 */
void append_line(text_t *txt, char *new_line);
void append_line(text_t *txt, char *new_line)
{
    if (!txt) /* check we have an actual tree to work with, either-wise exit. */
        exit(-1);

    /*
        To append a line we simply keep going right keeping track of all the nodes
        we have visited by placing them on a stack, the last node should be the empty
        line marker, so we simply insert the new line to its left and another node
        on the right moving the empty line forward,
        once done move upwards and re-balancing accordingly updating both weight and height
        properties of each tree.

     */

    unsigned int stack_index = 0;

    while (txt->right)
    {
        stack[stack_index++] = txt;
        txt = txt->right;
    }


    txt->left = allocate_node(); /* should be right most node containing last_line, so insert line at left */
    txt->left->right = NULL;     /* mark left node as a leaf */
    txt->left->left = (tree_node_type *)new_line; /* insert actual line */

    txt->right = allocate_node(); /* now we need to move the last line down one more node */
    txt->right->right = NULL;     /* mark it as a leaf */
    txt->right->left = (tree_node_type *)last_line; /* insert last line marker */

    txt->weight = 2; /* since we've added a new line increase the weight, it should be 2 */
    txt->height = 1; /* the height should be 1 */

    /* now we need to re-balance ... */
    re_balance_tree(stack_index);
}

/*
    AS PRE-DEFINE BY THE ASSIGNMENT.
    sets the line of
        number index, if such a line exists, to new line,
        and returns a pointer to the previous line of that number.
        If no line of that number exists,
        it does not change the structure and returns NULL.
 */
char *set_line(text_t *txt, int index, char *new_line);
char *set_line(text_t *txt, int index, char *new_line)
{
    if (is_index_not_ok(txt, index))
        return NULL;

    txt = find_node(txt, index, NULL); /* we don't need to use the stack */

    char *temp = (char *)txt->left;
    txt->left = (tree_node_type *)new_line; /* update the line */
    return temp;
}

/*
    AS PRE-DEFINE BY THE ASSIGNMENT:
     inserts the line before the line of number index,
     if such a line exists, to new line,
     renumbering all lines after that line.
     If no such line exists, it appends new line as new last line.
*/
void insert_line(text_t *txt, int index, char *new_line);
void insert_line(text_t *txt, int index, char *new_line)
{
    if (is_index_not_ok(txt, index)) /* check if index is ok */
    {
        append_line(txt, new_line); /* if not append, the sub_routine (append_line) will check for txt NULL*/
        return ;
    }

    unsigned int stack_index = 0;
    txt = find_node(txt, index, &stack_index); /* we need to stored the path on the stack to re-balance after the insert. */

    char *temp = (char *)txt->left; /* keep back up of previous line */

    txt->left = allocate_node(); /* new node for new line, inserted on the left since its replacing the previous */
    txt->left->right = NULL;     /* its a leaf */
    txt->left->left = (tree_node_type *)new_line; /* insert actual new line */

    txt->right = allocate_node(); /* new node for previous line */
    txt->right->right = NULL;     /* its a leaf*/
    txt->right->left = (tree_node_type *)temp; /* insert previous line */

    stack[stack_index++] = txt; /* push this node unto the stack to update height and weight */
    re_balance_tree(stack_index);
}

/*
    AS PRE-DEFINE BY THE ASSIGNMENT:
        deletes the line of number index,
        renumbering all lines after that line,
        and returns a pointer to the deleted line.

    The assignment does not state what occurs when the index is invalid, so Im just exiting...
 */
char * delete_line(text_t *txt, int index);
char * delete_line(text_t *txt, int index)
{
    if (is_index_not_ok(txt, index))
        exit(-1);

    unsigned int stack_index = 0;
    txt = find_node(txt, index, &stack_index); /* we need to keep track of our path to re-balance after deletion */

    char *line = (char *)txt->left;


    tree_node_type *parent = stack[--stack_index]; /* get parent node  */

    /*
        check which node we are deleting.
        if left the, move up the right up to the parent,
        either-wise it must be the right so move up the left up to the parent.
     */


    if (parent->left == txt)          /* we must be deleting the left child.*/
    {
        parent->left = parent->right->left;     /* move right-left child to the parents left */
        recycle_node(txt);                      /* recycle left child */
        txt = parent->right;                    /* backup/save right child for recycling. */
        parent->right = parent->right->right;   /* move right-right child up */
        recycle_node(txt);                      /* recycle previous right child */
    }
    else                              /* we must be deleting the right child */
    {
        parent->right = parent->left->right;    /* move left-right child up */
        recycle_node(txt);                      /* recycle old right child  */
        txt = parent->left;                     /* backup/save old left child for recycling */
        parent->left = parent->left->left;      /* move left-left child up */
        recycle_node(txt);

    }

    if (parent->right) /* its not a leaf */
        stack[stack_index++] = parent; /* add parent node for balancing & updates ... */
    else
    {
        parent->weight = 1;
        parent->height = 0;
    }

    re_balance_tree(stack_index); /* re-balance tree updating heights and weights. */

    return line;
}

/*
    Auxilary function to verify order of lines,
    simply prints the nodes in ascending order.
*/
void print_nodes_in_order(tree_node_type *tree)
{
    unsigned int stack_index = 0, line_index = 1;
    stack[stack_index++] = tree;

    while (stack_index)
    {
        tree = stack[--stack_index];
        if (tree->right) /* its not a leaf ... */
        {
            stack[stack_index++] = tree->right;
            stack[stack_index++] = tree->left;
        }
        else
            printf("line %u: %s\n", line_index++, (char *)tree->left);
    }
}


int main()
{  int i, tmp; text_t *txt1, *txt2, *txt3; char *c;
    printf("starting \n");
    txt1 = create_text();
    txt2 = create_text();
    txt3 = create_text();
    append_line(txt1, "line one" );
    if( (tmp = length_text(txt1)) != 1)
    {  printf("Test 1: length should be 1, is %d\n", tmp); exit(-1);
    }
    append_line(txt1, "line hundred" );
    insert_line(txt1, 2, "line ninetynine" );
    insert_line(txt1, 2, "line ninetyeight" );
    insert_line(txt1, 2, "line ninetyseven" );
    insert_line(txt1, 2, "line ninetysix" );
    insert_line(txt1, 2, "line ninetyfive" );
    for( i = 2; i <95; i++ )
        insert_line(txt1, 2, "some filler line between 1 and 95" );
    if( (tmp = length_text(txt1)) != 100)
    {  printf("Test 2: length should be 100, is %d\n", tmp); exit(-1);
    }
    printf("found at line 1:   %s\n",get_line(txt1,  1));
    printf("found at line 2:   %s\n",get_line(txt1,  2));
    printf("found at line 99:  %s\n",get_line(txt1, 99));
    printf("found at line 100: %s\n",get_line(txt1,100));
    for(i=1; i<=10000; i++)
    {  if( i%2==1 )
        append_line(txt2, "A");
    else
        append_line(txt2, "B");
    }
    if( (tmp = length_text(txt2)) != 10000)
    {  printf("Test 3: length should be 10000, is %d\n", tmp); exit(-1);
    }
    c = get_line(txt2, 9876 );
    if( *c != 'B')
    {  printf("Test 4: line 9876 of txt2 should be B, found %s\n", c); exit(-1);
    }
    for( i= 10000; i > 1; i-=2 )
    {  c = delete_line(txt2, i);
        if( *c != 'B')
        {  printf("Test 5: line %d of txt2 should be B, found %s\n", i, c); exit(-1);
        }
        append_line( txt2, c );
    }
    for( i=1; i<= 5000; i++ )
    {  c = get_line(txt2, i);
        if( *c != 'A')
        {  printf("Test 6: line %d of txt2 should be A, found %s\n", i, c); exit(-1);
        }
    }
    for( i=1; i<= 5000; i++ )
        delete_line(txt2, 1 );
    for( i=1; i<= 5000; i++ )
    {  c = get_line(txt2, i);
        if( *c != 'B')
        {  printf("Test 7: line %d of txt2 should be B, found %s\n", i, c); exit(-1);
        }
    }
    set_line(txt1, 100, "the last line");
    for( i=99; i>=1; i-- )
        delete_line(txt1, i );
    printf("found at the last line:   %s\n",get_line(txt1,  1));
    for(i=0; i<1000000; i++)
        append_line(txt3, "line" );
    if( (tmp = length_text(txt3)) != 1000000)
    {  printf("Test 8: length should be 1000000, is %d\n", tmp); exit(-1);
    }
    for(i=0; i<500000; i++)
        delete_line(txt3, 400000 );
    if( (tmp = length_text(txt3)) != 500000)
    {  printf("Test 9: length should be 500000, is %d\n", tmp); exit(-1);
    }
    printf("End of tests\n");

    /* de-allocating resources ...*/
    /*
    recycle_text(txt1);
    recycle_text(txt2);
    recycle_text(txt3);

    free_blocks();
    */
    return 0;
}
