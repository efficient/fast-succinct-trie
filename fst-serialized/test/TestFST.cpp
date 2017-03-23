//************************************************
// FST unit tests
//************************************************
#include "gtest/gtest.h"
#include <stdlib.h>
#include <fstream>
#include <algorithm>

#include "FST.hpp"

#define TEST_SIZE 234369
#define TEST_SIZE_INT 1000000
#define RANGE_SIZE 10

using namespace std;

const string testFilePath = "../../test/testStringKeys.txt";

class UnitTest : public ::testing::Test {
public:
    virtual void SetUp () { }
    virtual void TearDown () { }
};

inline void printStatFST(FST* index) {
    cout << "mem = " << index->mem() << "\n";

    cout << "cMemU = " << index->cMemU() << "\n";
    cout << "tMemU = " << index->tMemU() << "\n";
    cout << "oMemU = " << index->oMemU() << "\n";
    cout << "valueMemU = " << index->valueMemU() << "\n";

    cout << "cMem = " << index->cMem() << "\n";
    cout << "tMem = " << index->tMem() << "\n";
    cout << "sMem = " << index->sMem() << "\n";
    cout << "valueMem = " << index->valueMem() << "\n";
}

inline int loadFile (string filePath, vector<string> &keys, vector<uint64_t> &values) {
    ifstream infile(filePath);
    string op;
    string key;
    uint64_t count = 0;
    int longestKeyLen = 0;
    while (count < TEST_SIZE && infile.good()) {
	infile >> key; //subject to change
	keys.push_back(key);
	values.push_back(count);
	if (key.length() > longestKeyLen)
	    longestKeyLen = key.length();
	count++;
    }

    return longestKeyLen;
}

inline int loadFile_ptrValue (string filePath, vector<string> &keys, vector<uint64_t> &values) {
    ifstream infile(filePath);
    string op;
    string key;
    uint64_t count = 0;
    int longestKeyLen = 0;
    while (count < TEST_SIZE && infile.good()) {
	infile >> key; //subject to change
	keys.push_back(key);
	//values.push_back(count);
	values.push_back((uint64_t)(const_cast<char*>(keys[count].c_str())));
	if (key.length() > longestKeyLen)
	    longestKeyLen = key.length();
	count++;
    }

    return longestKeyLen;
}

inline int loadMonoInt (vector<uint64_t> &keys) {
    for (uint64_t i = 0; i < TEST_SIZE_INT; i++)
	keys.push_back(i);
    return sizeof(uint64_t);
}

inline int loadMonoSkipInt (vector<uint64_t> &keys) {
    uint64_t skip = 10;
    for (uint64_t i = skip - 1; i < (TEST_SIZE_INT * skip); i += skip)
	keys.push_back(i);
    return sizeof(uint64_t);
}

inline int loadRandInt (vector<uint64_t> &keys) {
    srand(0);
    for (uint64_t i = 0; i < TEST_SIZE_INT; i++) {
	uint64_t r = rand();
	keys.push_back(r);
    }
    sort(keys.begin(), keys.end());
    return sizeof(uint64_t);
}


//*****************************************************************
// FST TESTS
//*****************************************************************

TEST_F(UnitTest, LookupTest) {
    vector<string> keys;
    vector<uint64_t> values;
    int longestKeyLen = loadFile(testFilePath, keys, values);

    FST *index = load(keys, values, longestKeyLen);
    printStatFST(index);

    //index->printU();
    //index->print();

    uint64_t fetchedValue;
    for (int i = 0; i < TEST_SIZE; i++) {
	if (i > 0 && keys[i].compare(keys[i-1]) == 0)
	    continue;
	ASSERT_TRUE(index->lookup((uint8_t*)keys[i].c_str(), keys[i].length(), fetchedValue));
	ASSERT_EQ(values[i], fetchedValue);
    }

    delete index;
}


TEST_F(UnitTest, LookupMonoIntTest) {
    vector<uint64_t> keys;
    int longestKeyLen = loadMonoInt(keys);

    FST *index = load(keys, keys);

    printStatFST(index);

    uint64_t fetchedValue;
    for (uint64_t i = 0; i < TEST_SIZE_INT; i++) {
	ASSERT_TRUE(index->lookup(keys[i], fetchedValue));
	ASSERT_EQ(keys[i], fetchedValue);
    }
}


TEST_F(UnitTest, LookupRandIntTest) {
    vector<uint64_t> keys;
    int longestKeyLen = loadRandInt(keys);

    FST *index = load(keys, keys);

    printStatFST(index);

    random_shuffle(keys.begin(), keys.end());

    uint64_t fetchedValue;

    for (uint64_t i = 0; i < TEST_SIZE_INT; i++) {
	ASSERT_TRUE(index->lookup(keys[i], fetchedValue));
	ASSERT_EQ(keys[i], fetchedValue);
    }
}

TEST_F(UnitTest, LowerBoundTest) {
    vector<uint64_t> keys;
    int longestKeyLen = loadMonoSkipInt(keys);

    FST *index = load(keys, keys);

    printStatFST(index);

    string curkey;
    char* key_str = new char[8];
    string keyString;
    FSTIter iter(index);
    for (int i = 0; i < TEST_SIZE_INT - 1; i++) {
	ASSERT_TRUE(index->lowerBound(keys[i] - 1, iter));
	curkey = iter.key();
	reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(keys[i]);
	keyString = string(key_str, 8);
	auto res = mismatch(curkey.begin(), curkey.end(), keyString.begin());
	ASSERT_TRUE(res.first == curkey.end());
	ASSERT_EQ(keys[i], iter.value());

	for (int j = 0; j < RANGE_SIZE; j++) {
	    if (i+j+1 < TEST_SIZE_INT) {
		ASSERT_TRUE(iter++);
		curkey = iter.key();
		reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(keys[i+j+1]);
		keyString = string(key_str, 8);
		res = mismatch(curkey.begin(), curkey.end(), keyString.begin());
		ASSERT_TRUE(res.first == curkey.end());
		ASSERT_EQ(keys[i+j+1], iter.value());
	    }
	    else {
		ASSERT_FALSE(iter++);
		curkey = iter.key();
		reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(keys[TEST_SIZE_INT-1]);
		keyString = string(key_str, 8);
		res = mismatch(curkey.begin(), curkey.end(), keyString.begin());
		ASSERT_TRUE(res.first == curkey.end());
		ASSERT_EQ(keys[TEST_SIZE_INT-1], iter.value());
	    }
	}
    }
}

TEST_F(UnitTest, UpperBoundTest) {
    vector<uint64_t> keys;
    int longestKeyLen = loadMonoSkipInt(keys);

    FST *index = load(keys, keys);

    printStatFST(index);

    string curkey;
    char* key_str = new char[8];
    string keyString;
    FSTIter iter(index);
    for (int i = 0; i < TEST_SIZE_INT - 1; i++) {
 	ASSERT_TRUE(index->upperBound(keys[i] + 1, iter));
	curkey = iter.key();
	reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(keys[i]);
	keyString = string(key_str, 8);
	auto res = mismatch(curkey.begin(), curkey.end(), keyString.begin());
	ASSERT_TRUE(res.first == curkey.end());
	ASSERT_EQ(keys[i], iter.value());

	for (int j = 0; j < RANGE_SIZE; j++) {
	    if (i+j+1 < TEST_SIZE_INT) {
		ASSERT_TRUE(iter++);
		curkey = iter.key();
		reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(keys[i+j+1]);
		keyString = string(key_str, 8);
		res = mismatch(curkey.begin(), curkey.end(), keyString.begin());
		ASSERT_TRUE(res.first == curkey.end());
		ASSERT_EQ(keys[i+j+1], iter.value());
	    }
	    else {
		ASSERT_FALSE(iter++);
		curkey = iter.key();
		reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(keys[TEST_SIZE_INT-1]);
		keyString = string(key_str, 8);
		res = mismatch(curkey.begin(), curkey.end(), keyString.begin());
		ASSERT_TRUE(res.first == curkey.end());
		ASSERT_EQ(keys[TEST_SIZE_INT-1], iter.value());
	    }
	}

    }
}

TEST_F(UnitTest, ScanTest) {
    vector<string> keys;
    vector<uint64_t> values;
    int longestKeyLen = loadFile(testFilePath, keys, values);

    FST *index = load(keys, values, longestKeyLen);

    printStatFST(index);

    string curkey;
    FSTIter iter(index);
    for (int i = 0; i < TEST_SIZE - 1; i++) {
	if (i > 0 && keys[i].compare(keys[i-1]) == 0)
	    continue;
	ASSERT_TRUE(index->lowerBound((uint8_t*)keys[i].c_str(), keys[i].length(), iter));

	curkey = iter.key();

	auto res = mismatch(curkey.begin(), curkey.end(), keys[i].begin());
	ASSERT_TRUE(res.first == curkey.end());
	ASSERT_EQ(values[i], iter.value());

	for (int j = 0; j < RANGE_SIZE; j++) {
	    if (i+j+1 < TEST_SIZE) {
		ASSERT_TRUE(iter++);
		curkey = iter.key();
		res = mismatch(curkey.begin(), curkey.end(), keys[i+j+1].begin());
		ASSERT_TRUE(res.first == curkey.end());
		ASSERT_EQ(values[i+j+1], iter.value());
	    }
	    else {
		ASSERT_FALSE(iter++);
		curkey = iter.key();
		res = mismatch(curkey.begin(), curkey.end(), keys[TEST_SIZE-1].begin());
		ASSERT_TRUE(res.first == curkey.end());
		ASSERT_EQ(values[TEST_SIZE-1], iter.value());
	    }
	}
    }
}


TEST_F(UnitTest, ScanMonoIntTest) {
    vector<uint64_t> keys;
    int longestKeyLen = loadMonoInt(keys);

    FST *index = load(keys, keys);

    printStatFST(index);

    FSTIter iter(index);
    for (int i = 0; i < TEST_SIZE_INT - 1; i++) {
	ASSERT_TRUE(index->lowerBound(keys[i], iter));
	ASSERT_EQ(keys[i], iter.value());

	for (int j = 0; j < RANGE_SIZE; j++) {
	    if (i+j+1 < TEST_SIZE_INT) {
		ASSERT_TRUE(iter++);
		ASSERT_EQ(keys[i+j+1], iter.value());
	    }
	    else {
		ASSERT_FALSE(iter++);
		ASSERT_EQ(keys[TEST_SIZE_INT-1], iter.value());
	    }
	}
    }
}

TEST_F(UnitTest, ScanReverseTest) {
    vector<string> keys;
    vector<uint64_t> values;
    int longestKeyLen = loadFile(testFilePath, keys, values);

    FST *index = load(keys, values, longestKeyLen);

    printStatFST(index);

    string curkey;
    FSTIter iter(index);
    for (int i = 0; i < TEST_SIZE - 1; i++) {
	if (i > 0 && keys[i].compare(keys[i-1]) == 0)
	    continue;

	ASSERT_TRUE(index->upperBound((uint8_t*)keys[i].c_str(), keys[i].length(), iter));
	curkey = iter.key();
	auto res = mismatch(curkey.begin(), curkey.end(), keys[i].begin());
	ASSERT_TRUE(res.first == curkey.end());
	ASSERT_EQ(values[i], iter.value());

	for (int j = 0; j < RANGE_SIZE; j++) {
	    if (i-j-1 >= 0) {
		ASSERT_TRUE(iter--);
		curkey = iter.key();
		res = mismatch(curkey.begin(), curkey.end(), keys[i-j-1].begin());
		ASSERT_TRUE(res.first == curkey.end());
		ASSERT_EQ(values[i-j-1], iter.value());
	    }
	    else {
		ASSERT_FALSE(iter--);
		curkey = iter.key();
		res = mismatch(curkey.begin(), curkey.end(), keys[0].begin());
		ASSERT_TRUE(res.first == curkey.end());
		ASSERT_EQ(values[0], iter.value());
	    }
	}
    }
}

TEST_F(UnitTest, ScanMonoIntReverseTest) {
    vector<uint64_t> keys;
    int longestKeyLen = loadMonoInt(keys);

    FST *index = load(keys, keys);

    printStatFST(index);

    FSTIter iter(index);
    for (int i = 0; i < TEST_SIZE_INT - 1; i++) {
	ASSERT_TRUE(index->upperBound(keys[i], iter));
	ASSERT_EQ(keys[i], iter.value());

	for (int j = 0; j < RANGE_SIZE; j++) {
	    if (i-j-1 >= 0) {
		ASSERT_TRUE(iter--);
		ASSERT_EQ(keys[i-j-1], iter.value());
	    }
	    else {
		ASSERT_FALSE(iter--);
		ASSERT_EQ(keys[0], iter.value());
	    }
	}
    }
}


int main (int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
