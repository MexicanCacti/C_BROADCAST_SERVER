#include <stdlib.h>

struct node
{
    int val;
    struct node *next;
} typedef node;

struct linkedNodes{
    int nodeCount;
    node* headNode;
} typedef linkedNodes;


extern void freeLinkedNodes(linkedNodes **list);
extern void freeNodes(node **head);

void removeNode(linkedNodes** l, int val)
{
    node *current, *prev = NULL;
    current = (*l)->headNode;

    if((*l)->nodeCount == 0 || current == NULL) return;

    while(current!= NULL && current->val != val)
    {
        prev = current;
        current = current->next;
    }

    if(current == NULL) return;

    if(prev == NULL) // headnode removal
    {
        (*l)->headNode = current->next;
    }
    else
    {
        prev->next = current->next;
    }
    
    free(current);
    (*l)->nodeCount--;
    
}

/*
    Inserts a new node into the linked list with a value of val
*/
void insertNode(linkedNodes **l, int val)
{

    node *newNode = malloc(sizeof(node));
    if(!newNode) return;

    newNode->val = val;
    newNode->next = (*l)->headNode;

    (*l)->headNode = newNode;

    (*l)->nodeCount++;
}

/*
    Will populate the linked list to be 0 -> val
*/
void insertNodesUpTo(linkedNodes **list, int val)
{
    for(int i = 0 ; i < val; ++i)
    {
        insertNode(list, i);
    }
}

int popNode(linkedNodes **list)
{
    
    linkedNodes * l = NULL;
    l = *list;

    node *head, *current, *prev = NULL;
    int retVal = -1;

    head = l->headNode;
    if(head == NULL) return retVal;

    current = head;
    while(current->next != NULL)
    {
        prev = current;
        current = current->next;
    }
    
    retVal = current->val;
    free(current);

    if(prev) prev->next = NULL;
    else l->headNode = NULL;

    l->nodeCount = l->nodeCount - 1;
    return retVal;
}

void freeLinkedNodes(linkedNodes **list)
{
    if(!*list) return;
    linkedNodes *l = *list;

    if(l->headNode != NULL) freeNodes(&l->headNode);

    free(*list);
    *list = NULL;
}

void freeNodes(node **head)
{

    node *current = *head;
    node *prev, *next = NULL;

    while(current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }

}