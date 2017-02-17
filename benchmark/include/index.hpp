#include <iostream>
#include <vector>
#include <algorithm>
//#include "indexkey.h"
#include "btree_map.h"
#include "btree.h"
#include "ART.hpp"
#include "FST.hpp"
#include "allocatortracker.h"

template<typename KeyType, class KeyComparator>
class Index
{
 public:
    virtual bool load(std::vector<KeyType> &keys, std::vector<uint64_t> &values) = 0;

    virtual bool insert(KeyType key, uint64_t value) = 0;

    virtual uint64_t find(KeyType key) = 0;

    virtual uint64_t scan(KeyType key, int range) = 0;

    virtual int64_t getMemory() const = 0;
};

//***********************************************************
// B+tree
//***********************************************************

template<typename KeyType, class KeyComparator>
class BtreeIndex : public Index<KeyType, KeyComparator>
{
 public:
    typedef AllocatorTracker<std::pair<const KeyType, uint64_t> > AllocatorType;
    typedef stx::btree_map<KeyType, uint64_t, KeyComparator, stx::btree_default_map_traits<KeyType, uint64_t>, AllocatorType> MapType;

    ~BtreeIndex() {
	delete idx;
	delete alloc;
    }

    bool load(std::vector<KeyType> &keys, std::vector<uint64_t> &values) {
	int count = 0;
	while (count < (int)keys.size()) {
	    insert(keys[count], values[count]);
	    count++;
	}
	return true;
    }

    bool insert(KeyType key, uint64_t value) {
	std::pair<typename MapType::iterator, bool> retval = idx->insert(key, value);
	return retval.second;
    }

    uint64_t find(KeyType key) {
	iter = idx->find(key);
	if (iter == idx->end()) {
	    std::cout << "READ FAIL\n";
	    return 0;
	}
	return iter->second;
    }

    uint64_t scan(KeyType key, int range) {
	iter = idx->lower_bound(key);
	if (iter == idx->end()) {
	    std::cout << "SCAN FIRST READ FAIL\n";
	    return 0;
	}
    
	uint64_t sum = 0;
	sum += iter->second;
	for (int i = 0; i < range; i++) {
	    ++iter;
	    if (iter == idx->end()) {
		break;
	    }
	    sum += iter->second;
	}
	return sum;
    }

    int64_t getMemory() const {
	return memory;
    }

    BtreeIndex() {
	memory = 0;
	alloc = new AllocatorType(&memory);
	idx = new MapType(KeyComparator(), (*alloc));
    }

    MapType *idx;
    int64_t memory;
    AllocatorType *alloc;
    typename MapType::const_iterator iter;
};

//***********************************************************
// ART
//***********************************************************

template<typename KeyType, class KeyComparator>
class ArtIndex : public Index<KeyType, KeyComparator>
{
 public:

    ~ArtIndex() {
	delete idx;
	delete key_bytes;
    }

    bool load(std::vector<KeyType> &keys, std::vector<uint64_t> &values) {
	idx->load(keys, values);
	iter = ARTIter(idx);
	return true;
    }

    bool insert(KeyType key, uint64_t value) {
	loadKey(key);
	idx->insert(key_bytes, value, 8);
	return true;
    }

    uint64_t find(KeyType key) {
	return idx->lookup(key);
    }

    uint64_t scan(KeyType key, int range) {
	uint64_t sum = 0;
	idx->lower_bound(key, &iter);
	sum += iter.value();
	for (int i = 0; i < range - 1; i++) {
	    if (!iter++) break;
	    sum += iter.value();
	}
	return sum;
    }

    int64_t getMemory() const {
	return idx->getMemory();
    }

    ArtIndex() {
	idx = new ART(8);
	key_bytes = new uint8_t [8];
    }

 private:
    inline void loadKey(KeyType key) {
	reinterpret_cast<uint64_t*>(key_bytes)[0]=__builtin_bswap64(key);
    }

    ART *idx;
    uint8_t* key_bytes;
    ARTIter iter;
};

//***********************************************************
// ART EMAIL
//***********************************************************

template<typename KeyType, class KeyComparator>
class ArtIndex_Email : public Index<KeyType, KeyComparator>
{
 public:

    ~ArtIndex_Email() {
	delete idx;
    }

    bool load(std::vector<KeyType> &keys, std::vector<uint64_t> &values) {
	idx->load(keys, values, maxKeyLength);
	iter = ARTIter(idx);
	return true;
    }

    bool insert(KeyType key, uint64_t value) {
	idx->insert((uint8_t*)(const_cast<char*>(key.c_str())), value, maxKeyLength);
	return true;
    }

    uint64_t find(KeyType key) {
	return idx->lookup((uint8_t*)(const_cast<char*>(key.c_str())), key.length(), maxKeyLength);
    }

    uint64_t scan(KeyType key, int range) {
	uint64_t sum = 0;
	idx->lower_bound((uint8_t*)(const_cast<char*>(key.c_str())), key.length(), maxKeyLength, &iter);
	sum += iter.value();
	for (int i = 0; i < range - 1; i++) {
	    if (!iter++) break;
	    sum += iter.value();
	}
	return sum;
    }

    int64_t getMemory() const {
	return idx->getMemory();
    }

    ArtIndex_Email() {
	maxKeyLength = 80;
	idx = new ART(maxKeyLength);
    }

 private:
    ART *idx;
    ARTIter iter;
    unsigned maxKeyLength;
};


//***********************************************************
// CART
//***********************************************************

template<typename KeyType, class KeyComparator>
class CArtIndex : public Index<KeyType, KeyComparator>
{
 public:

    ~CArtIndex() {
	delete idx;
	delete key_bytes;
    }

    bool load(std::vector<KeyType> &keys, std::vector<uint64_t> &values) {
	idx->load(keys, values);
	idx->convert();
	iter = CARTIter(idx);
	return true;
    }

    bool insert(KeyType key, uint64_t value) {
	loadKey(key);
	idx->insert(key_bytes, value, 8);
	return true;
    }

    uint64_t find(KeyType key) {
	return idx->lookup(key);
    }

    uint64_t scan(KeyType key, int range) {
	uint64_t sum = 0;
	idx->lower_bound(key, &iter);
	sum += iter.value();
	for (int i = 0; i < range - 1; i++) {
	    if (!iter++) break;
	    sum += iter.value();
	}
	return sum;
    }

    int64_t getMemory() const {
	return idx->getMemory();
    }

    CArtIndex() {
	idx = new CART(8);
	key_bytes = new uint8_t [8];
    }

 private:
    inline void loadKey(KeyType key) {
	reinterpret_cast<uint64_t*>(key_bytes)[0]=__builtin_bswap64(key);
    }

    CART *idx;
    uint8_t* key_bytes;
    CARTIter iter;
};

//***********************************************************
// CART EMAIL
//***********************************************************

template<typename KeyType, class KeyComparator>
class CArtIndex_Email : public Index<KeyType, KeyComparator>
{
 public:

    ~CArtIndex_Email() {
	delete idx;
    }

    bool load(std::vector<KeyType> &keys, std::vector<uint64_t> &values) {
	idx->load(keys, values, maxKeyLength);
	idx->convert();
	iter = CARTIter(idx);
	return true;
    }

    bool insert(KeyType key, uint64_t value) {
	idx->insert((uint8_t*)(const_cast<char*>(key.c_str())), value, maxKeyLength);
	return true;
    }

    uint64_t find(KeyType key) {
	return idx->lookup((uint8_t*)(const_cast<char*>(key.c_str())), key.length(), maxKeyLength);
    }

    uint64_t scan(KeyType key, int range) {
	uint64_t sum = 0;
	idx->lower_bound((uint8_t*)(const_cast<char*>(key.c_str())), key.length(), maxKeyLength, &iter);
	sum += iter.value();
	for (int i = 0; i < range - 1; i++) {
	    if (!iter++) break;
	    sum += iter.value();
	}
	return sum;
    }

    int64_t getMemory() const {
	return idx->getMemory();
    }

    CArtIndex_Email() {
	maxKeyLength = 80;
	idx = new CART(maxKeyLength);
    }

 private:
    CART *idx;
    CARTIter iter;
    unsigned maxKeyLength;
};


//***********************************************************
// FST
//***********************************************************

template<typename KeyType, class KeyComparator>
class FSTIndex : public Index<KeyType, KeyComparator>
{
 public:
    ~FSTIndex() {
	delete idx;
    }

    bool load(std::vector<KeyType> &keys, std::vector<uint64_t> &values) {
	std::sort(keys.begin(), keys.end());
	std::sort(values.begin(), values.end());
	idx->load(keys, values);
	iter = FSTIter(idx);
	return true;
    }

    bool insert(KeyType key, uint64_t value) {
	return true;
    }

    uint64_t find(KeyType key) {
	uint64_t value;
	idx->lookup(key, value);
	return value;
    }

    uint64_t scan(KeyType key, int range) {
	uint64_t sum = 0;
	idx->lowerBound(key, iter);
	sum += iter.value();
	for (int i = 0; i < range - 1; i++) {
	    if (!iter++) break;
	    sum += iter.value();
	}
	return sum;
    }

    int64_t getMemory() const {
	/*
	std::cout << "cMemU = " << idx->cMemU() << "\n";
	std::cout << "tMemU = " << idx->tMemU() << "\n";
	std::cout << "oMemU = " << idx->oMemU() << "\n";
	std::cout << "keyMemU = " << idx->keyMemU() << "\n";
	std::cout << "valueMemU = " << idx->valueMemU() << "\n\n";

	std::cout << "cMem = " << idx->cMem() << "\n";
	std::cout << "tMem = " << idx->tMem() << "\n";
	std::cout << "sMem = " << idx->sMem() << "\n";
	std::cout << "keyMem = " << idx->keyMem() << "\n";
	std::cout << "valueMem = " << idx->valueMem() << "\n\n";

	std::cout << "mem = " << idx->mem() << "\n\n";
	*/
	return idx->mem();
    }

    FSTIndex() {
	idx = new FST();
    }

 private:
    FST *idx;
    FSTIter iter;
};


//***********************************************************
// FST Email
//***********************************************************

template<typename KeyType, class KeyComparator>
class FSTIndex_Email : public Index<KeyType, KeyComparator>
{
 public:
    ~FSTIndex_Email() {
	delete idx;
    }

    bool load(std::vector<KeyType> &keys, std::vector<uint64_t> &values) {
	std::sort(keys.begin(), keys.end());
	std::sort(values.begin(), values.end());
	idx->load(keys, values, 80);
	iter = FSTIter(idx);
	return true;
    }

    bool insert(KeyType key, uint64_t value) {
	return true;
    }

    uint64_t find(KeyType key) {
	uint64_t value;
	idx->lookup((const uint8_t*)key.c_str(), key.length(), value);
	return value;
    }

    uint64_t scan(KeyType key, int range) {
	uint64_t sum = 0;
	idx->lowerBound((const uint8_t*)key.c_str(), key.length(), iter);
	for (int i = 0; i < range - 1; i++) {
	    if (!iter++) break;
	    sum += iter.value();
	}
	return sum;
    }

    int64_t getMemory() const {
	/*
	std::cout << "cMemU = " << idx->cMemU() << "\n";
	std::cout << "tMemU = " << idx->tMemU() << "\n";
	std::cout << "oMemU = " << idx->oMemU() << "\n";
	std::cout << "keyMemU = " << idx->keyMemU() << "\n";
	std::cout << "valueMemU = " << idx->valueMemU() << "\n\n";

	std::cout << "cMem = " << idx->cMem() << "\n";
	std::cout << "tMem = " << idx->tMem() << "\n";
	std::cout << "sMem = " << idx->sMem() << "\n";
	std::cout << "keyMem = " << idx->keyMem() << "\n";
	std::cout << "valueMem = " << idx->valueMem() << "\n\n";

	std::cout << "mem = " << idx->mem() << "\n\n";
	*/
	return idx->mem();
    }

    FSTIndex_Email() {
	idx = new FST();
    }

 private:
    FST *idx;
    FSTIter iter;
};


