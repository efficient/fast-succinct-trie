/*
  Adaptive Radix Tree
  Viktor Leis, 2012
  leis@in.tum.de

  Modified by Huanchen Zhang, 2016
 */

#include <stdlib.h>    // malloc, free
#include <string.h>    // memset, memcpy
#include <stdint.h>    // integer types
#include <emmintrin.h> // x86 SSE intrinsics
#include <stdio.h>
#include <assert.h>
#include <vector>
#include <deque>
#include <string>
#include <iostream>

using namespace std;

// Constants for the node types
static const int8_t NodeType4=0;
static const int8_t NodeType16=1;
static const int8_t NodeType48=2;
static const int8_t NodeType256=3;

//static
static const int8_t NodeTypeD=0;
static const int8_t NodeTypeDP=1;
static const int8_t NodeTypeF=2;
static const int8_t NodeTypeFP=3;
static const int8_t NodeTypeU=4;

// The maximum prefix length for compressed paths stored in the
// header, if the path is longer it is loaded from the database on
// demand
static const unsigned maxPrefixLength=9;

static const unsigned NodeDItemTHold=227;

// Shared header of all inner nodes
struct Node {
    // length of the compressed path (prefix)
    uint32_t prefixLength;
    // number of non-null children
    uint16_t count;
    // node type
    int8_t type;
    // compressed path (prefix)
    uint8_t prefix[maxPrefixLength];

    Node(int8_t type) : prefixLength(0),count(0),type(type) {}
};

// Node with up to 4 children
struct Node4 : Node {
    uint8_t key[4];
    Node* child[4];

    Node4() : Node(NodeType4) {
	memset(key,0,sizeof(key));
	memset(child,0,sizeof(child));
    }
};

// Node with up to 16 children
struct Node16 : Node {
    uint8_t key[16];
    Node* child[16];

    Node16() : Node(NodeType16) {
	memset(key,0,sizeof(key));
	memset(child,0,sizeof(child));
    }
};

static const uint8_t emptyMarker=48;

// Node with up to 48 children
struct Node48 : Node {
    uint8_t childIndex[256];
    Node* child[48];

    Node48() : Node(NodeType48) {
	memset(childIndex,emptyMarker,sizeof(childIndex));
	memset(child,0,sizeof(child));
    }
};

// Node with up to 256 children
struct Node256 : Node {
    Node* child[256];

    Node256() : Node(NodeType256) {
	memset(child,0,sizeof(child));
    }
};

typedef struct {
    Node* node;
    uint16_t cursor;
} NodeCursor;

//static-----------------------------------------------------
struct NodeStatic {
    int8_t type;

    NodeStatic() : type(NodeTypeFP) {}
    NodeStatic(int8_t t) : type(t) {}
};

struct NodeD : NodeStatic {
    uint8_t count;
    uint8_t data[0];

    NodeD() : NodeStatic(NodeTypeD) {}
    NodeD(uint8_t size) : NodeStatic(NodeTypeD) { count = size; }

    uint8_t* key() { return data; }
    uint8_t* key(unsigned pos) { return &data[pos]; }
    NodeStatic** child() { return (NodeStatic**)(&data[count]); }
    NodeStatic** child(unsigned pos) { return (NodeStatic**)(&data[count + sizeof(NodeStatic*) * pos]); }
};

struct NodeDP : NodeStatic {
    uint8_t count;
    uint32_t prefixLength;
    uint8_t data[0];

    NodeDP() : NodeStatic(NodeTypeDP) {}
    NodeDP(uint8_t size) : NodeStatic(NodeTypeDP) { count = size; }
    NodeDP(uint8_t size, uint32_t pl) : NodeStatic(NodeTypeDP) {
	count = size;
	prefixLength = pl;
    }

    uint8_t* prefix() { return data; }
    uint8_t* key() { return &data[prefixLength]; }
    uint8_t* key(unsigned pos) { return &data[prefixLength + pos]; }
    NodeStatic** child() {
	return (NodeStatic**)(&data[prefixLength + count]);
    }
    NodeStatic** child(unsigned pos) {
	return (NodeStatic**)(&data[prefixLength + count + sizeof(NodeStatic*) * pos]);
    }
};

struct NodeF : NodeStatic {
    uint16_t count;
    NodeStatic* child[256];

    NodeF() : NodeStatic(NodeTypeF) {}
    NodeF(uint16_t size) : NodeStatic(NodeTypeF) {
	count = size;
	memset(child, 0, sizeof(child));
    }
};

struct NodeFP : NodeStatic {
    uint16_t count;
    uint32_t prefixLength;
    uint8_t data[0];

    NodeFP() : NodeStatic(NodeTypeFP) {}
    NodeFP(uint16_t size) : NodeStatic(NodeTypeFP) { count = size; }
    NodeFP(uint16_t size, uint32_t pl) : NodeStatic(NodeTypeFP) {
	count = size;
	prefixLength = pl;
	memset(data, 0, prefixLength * sizeof(uint8_t) + 256 * sizeof(NodeStatic*));
    }

    uint8_t* prefix() { return data; }
    NodeStatic** child() { return (NodeStatic**)&data[prefixLength]; }
};

struct NodeU : NodeStatic {
    uint16_t count;
    uint32_t prefixLength;
    uint8_t prefix[maxPrefixLength];
    uint8_t data[0];

    NodeU() : NodeStatic(NodeTypeU) {}
    NodeU(uint16_t size) : NodeStatic(NodeTypeU) { count = size; }

    uint8_t* key() { return data; }
    NodeStatic** child() { return (NodeStatic**)(&data[count]); }
};

//static
typedef struct {
    NodeStatic* node;
    uint16_t cursor;
} NodeStaticCursor;

//-----------------------------------------------------------


class ART;
class CART;
class ARTIter;
class CARTIter;


class ART {

public:
    inline Node* makeLeaf(uintptr_t tid);
    inline uintptr_t getLeafValue(Node* node);
    inline bool isLeaf(Node* node);
    inline uint8_t flipSign(uint8_t keyByte);
    inline void loadKey(uintptr_t tid,uint8_t key[]);
    inline unsigned ctz(uint16_t x);

    //****************************************************************

    inline Node** findChild(Node* n,uint8_t keyByte);
    inline Node* minimum(Node* node);
    inline bool leafMatches(Node* leaf,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength);
    inline unsigned prefixMismatch(Node* node,uint8_t key[],unsigned depth,unsigned maxKeyLength);

    inline Node* lookup(Node* node,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength);
    inline Node* lookupPessimistic(Node* node,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength);

    //****************************************************************

    inline int CompareToPrefix(Node* node,uint8_t key[],unsigned depth,unsigned maxKeyLength);
    inline Node* findChild_recordPath(Node* n, uint8_t keyByte, ARTIter* iter);
    inline Node* lower_bound(Node* node,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength, ARTIter *iter);

    //****************************************************************

    inline unsigned min(unsigned a,unsigned b);
    inline void copyPrefix(Node* src,Node* dst);

    inline void insert(Node* node,Node** nodeRef,uint8_t key[],unsigned depth,uintptr_t value,unsigned maxKeyLength);

    inline void insertNode4(Node4* node,Node** nodeRef,uint8_t keyByte,Node* child);
    inline void insertNode16(Node16* node,Node** nodeRef,uint8_t keyByte,Node* child);
    inline void insertNode48(Node48* node,Node** nodeRef,uint8_t keyByte,Node* child);
    inline void insertNode256(Node256* node,Node** nodeRef,uint8_t keyByte,Node* child);

    //****************************************************************

    inline void erase(Node* node,Node** nodeRef,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength);

    inline void eraseNode4(Node4* node,Node** nodeRef,Node** leafPlace);
    inline void eraseNode16(Node16* node,Node** nodeRef,Node** leafPlace);
    inline void eraseNode48(Node48* node,Node** nodeRef,uint8_t keyByte);
    inline void eraseNode256(Node256* node,Node** nodeRef,uint8_t keyByte);


    ART();
    ART(unsigned kl);
    ART(Node* r);
    ART(Node* r, unsigned kl);

    void load(vector<string> &keys, vector<uint64_t> &values, unsigned maxKeyLength);
    void load(vector<uint64_t> &keys, vector<uint64_t> &values);
    void insert(uint8_t key[], uintptr_t value, unsigned maxKeyLength);
    uint64_t lookup(uint8_t key[], unsigned keyLength, unsigned maxKeyLength);
    uint64_t lookup(uint64_t key64);
    bool lower_bound(uint8_t key[], unsigned keyLength, unsigned maxKeyLength, ARTIter* iter);
    bool lower_bound(uint64_t key64, ARTIter* iter);
    void erase(uint8_t key[], unsigned keyLength, unsigned maxKeyLength);
    uint64_t getMemory();

    friend class ARTIter;

private:
    Node* root;
    unsigned key_length;

    //stats
    uint64_t memory;
    uint64_t num_items;
    uint64_t node4_count;
    uint64_t node16_count;
    uint64_t node48_count;
    uint64_t node256_count;

    // This address is used to communicate that search failed
    Node* nullNode;
};

class CART {

public:
    inline Node* makeLeaf(uintptr_t tid);
    inline uintptr_t getLeafValue(Node* node);
    inline uintptr_t getLeafValue(NodeStatic* node);
    inline bool isLeaf(Node* node);
    inline bool isLeaf(NodeStatic* node);
    inline uint8_t flipSign(uint8_t keyByte);
    inline void loadKey(uintptr_t tid,uint8_t key[]);
    inline unsigned ctz(uint16_t x);

    //****************************************************************

    inline Node** findChild(Node* n,uint8_t keyByte);
    inline NodeStatic** findChild(NodeStatic* n,uint8_t keyByte);
    inline Node* minimum(Node* node);
    inline NodeStatic* minimum(NodeStatic* node);
    inline bool leafMatches(Node* leaf,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength);
    inline bool leafMatches(NodeStatic* leaf,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength);
    inline unsigned prefixMismatch(Node* node,uint8_t key[],unsigned depth,unsigned maxKeyLength);
    inline unsigned prefixMismatch(NodeStatic* node,uint8_t key[],unsigned depth,unsigned maxKeyLength);

    inline NodeStatic* lookup(NodeStatic* node,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength);

    //****************************************************************

    inline int CompareToPrefix(Node* node,uint8_t key[],unsigned depth,unsigned maxKeyLength);
    inline int CompareToPrefix(NodeStatic* n,uint8_t key[],unsigned depth,unsigned maxKeyLength);
    inline NodeStatic* findChild_recordPath(NodeStatic* n, uint8_t keyByte, CARTIter* iter);
    //inline Node* findChild_recordPath(Node* n, uint8_t keyByte, ARTIter* iter);
    inline NodeStatic* lower_bound(NodeStatic* node, uint8_t key[], unsigned keyLength, unsigned depth, unsigned maxKeyLength, CARTIter* iter);
    //inline Node* lower_bound(Node* node,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength, ARTIter *iter);

    //****************************************************************

    inline unsigned min(unsigned a,unsigned b);
    inline void copyPrefix(Node* src,Node* dst);

    inline void insert(Node* node,Node** nodeRef,uint8_t key[],unsigned depth,uintptr_t value,unsigned maxKeyLength);

    inline void insertNode4(Node4* node,Node** nodeRef,uint8_t keyByte,Node* child);
    inline void insertNode16(Node16* node,Node** nodeRef,uint8_t keyByte,Node* child);
    inline void insertNode48(Node48* node,Node** nodeRef,uint8_t keyByte,Node* child);
    inline void insertNode256(Node256* node,Node** nodeRef,uint8_t keyByte,Node* child);

    //****************************************************************

    inline void Node4_to_NodeD(Node4* n, NodeD* n_static);
    inline void Node16_to_NodeD(Node16* n, NodeD* n_static);
    inline void Node48_to_NodeD(Node48* n, NodeD* n_static);
    inline void Node256_to_NodeD(Node256* n, NodeD* n_static);
    inline void Node_to_NodeD(Node* n, NodeD* n_static);

    inline void Node4_to_NodeDP(Node4* n, NodeDP* n_static);
    inline void Node16_to_NodeDP(Node16* n, NodeDP* n_static);
    inline void Node48_to_NodeDP(Node48* n, NodeDP* n_static);
    inline void Node256_to_NodeDP(Node256* n, NodeDP* n_static);
    inline void Node_to_NodeDP(Node* n, NodeDP* n_static);

    inline void Node4_to_NodeF(Node4* n, NodeF* n_static);
    inline void Node16_to_NodeF(Node16* n, NodeF* n_static);
    inline void Node48_to_NodeF(Node48* n, NodeF* n_static);
    inline void Node256_to_NodeF(Node256* n, NodeF* n_static);
    inline void Node_to_NodeF(Node* n, NodeF* n_static);

    inline void Node4_to_NodeFP(Node4* n, NodeFP* n_static);
    inline void Node16_to_NodeFP(Node16* n, NodeFP* n_static);
    inline void Node48_to_NodeFP(Node48* n, NodeFP* n_static);
    inline void Node256_to_NodeFP(Node256* n, NodeFP* n_static);
    inline void Node_to_NodeFP(Node* n, NodeFP* n_static);

    inline size_t node_size(NodeStatic* n);
    inline NodeStatic* convert_to_static();
    inline NodeStatic* convert_to_static(Node* n);


    CART();
    CART(unsigned kl);
    CART(Node* r);
    CART(Node* r, unsigned kl);

    void load(vector<string> &keys, vector<uint64_t> &values, unsigned maxKeyLength);
    void load(vector<uint64_t> &keys, vector<uint64_t> &values);
    void insert(uint8_t key[], uintptr_t value, unsigned maxKeyLength);
    void convert();
    uint64_t lookup(uint8_t key[], unsigned keyLength, unsigned maxKeyLength);
    uint64_t lookup(uint64_t key64);
    bool lower_bound(uint8_t key[], unsigned keyLength, unsigned maxKeyLength, CARTIter* iter);
    bool lower_bound(uint64_t key64, CARTIter* iter);
    uint64_t getMemory();

    friend class CARTIter;

private:
    Node* root;
    NodeStatic* static_root;
    unsigned key_length;

    //stats
    uint64_t memory;
    uint64_t static_memory;
    uint64_t num_items;
    uint64_t node4_count;
    uint64_t node16_count;
    uint64_t node48_count;
    uint64_t node256_count;
    uint64_t nodeD_count;
    uint64_t nodeDP_count;
    uint64_t nodeF_count;
    uint64_t nodeFP_count;

    // This address is used to communicate that search failed
    Node* nullNode;
    NodeStatic* nullNode_static;
};

class ARTIter {
public:
    inline Node* minimum_recordPath(Node* node);
    inline Node* nextSlot();
    inline Node* currentLeaf();
    inline Node* nextLeaf();

    ARTIter();
    ARTIter(ART* idx);

    uint64_t value();
    bool operator ++ (int);

    friend class ART;

private:
    ART* index;
    std::vector<NodeCursor> node_stack;
    uint64_t val;
};

class CARTIter {
public:
    inline NodeStatic* minimum_recordPath(NodeStatic* node);
    inline NodeStatic* nextSlot();
    inline NodeStatic* currentLeaf();
    inline NodeStatic* nextLeaf();

    CARTIter();
    CARTIter(CART* idx);

    uint64_t value();
    bool operator ++ (int);

    friend class CART;

private:
    CART* index;
    std::vector<NodeStaticCursor> node_stack;
    uint64_t val;
};

