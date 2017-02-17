#include "microbench.h"

#define INIT_LIMIT 50000000

typedef uint64_t keytype;
typedef std::less<uint64_t> keycomp;

static const uint64_t key_type=0;
static const uint64_t value_type=1; // 0 = random pointers, 1 = pointers to keys

static const char* GREEN="\033[0;32m";
static const char* RED="\033[0;31m";
static const char* NC="\033[0;0m";

static Index<keytype, keycomp> *idx = NULL;

//==============================================================
// GET INSTANCE
//==============================================================
template<typename KeyType, class KeyComparator>
Index<KeyType, KeyComparator> *getInstance(const int type) {
    if (type == 0)
	return new BtreeIndex<KeyType, KeyComparator>();
    else if (type == 1)
	return new ArtIndex<KeyType, KeyComparator>();
    else if (type == 2)
	return new CArtIndex<KeyType, KeyComparator>();
    else if (type == 3)
	return new FSTIndex<KeyType, KeyComparator>();
    else
	return new BtreeIndex<KeyType, KeyComparator>();
}

//==============================================================
// LOAD
//==============================================================
inline void load(int wl, std::vector<keytype> &init_keys, std::vector<keytype> &keys, std::vector<uint64_t> &values, std::vector<int> &ranges, std::vector<int> &ops) {
    std::string init_file;
    std::string txn_file;
    // 0 = c, 1 = e
    if (wl == 0) {
	init_file = "../../benchmark/workloads/load_randint_workloadc";
	txn_file = "../../benchmark/workloads/txn_randint_workloadc";
    }
    else if (wl == 1) {
	init_file = "../../benchmark/workloads/load_randint_workloade";
	txn_file = "../../benchmark/workloads/txn_randint_workloade";
    }
    else {
	init_file = "../../benchmark/workloads/load_randint_workloadc";
	txn_file = "../../benchmark/workloads/txn_randint_workloadc";
    }

    std::ifstream infile_load(init_file);
    std::ifstream infile_txn(txn_file);

    std::string op;
    keytype key;
    int range;

    std::string insert("INSERT");
    std::string read("READ");
    std::string scan("SCAN");

    int count = 0;
    while ((count < INIT_LIMIT) && infile_load.good()) {
	infile_load >> op >> key;
	if (op.compare(insert) != 0) {
	    std::cout << "READING LOAD FILE FAIL!\n";
	    return;
	}
	init_keys.push_back(key);
	count++;
    }

    count = 0;
    uint64_t value = 0;
    void *base_ptr = malloc(8);
    uint64_t base = (uint64_t)(base_ptr);
    free(base_ptr);

    keytype *init_keys_data = init_keys.data();

    if (value_type == 0) {
	while (count < INIT_LIMIT) {
	    value = base + rand();
	    values.push_back(value);
	    count++;
	}
    }
    else {
	while (count < INIT_LIMIT) {
	    values.push_back(init_keys_data[count]);
	    count++;
	}
    }

    count = 0;
    while ((count < LIMIT) && infile_txn.good()) {
	infile_txn >> op >> key;
	if (op.compare(read) == 0) {
	    ops.push_back(1);
	    keys.push_back(key);
	}
	else if (op.compare(scan) == 0) {
	    infile_txn >> range;
	    ops.push_back(2);
	    keys.push_back(key);
	    ranges.push_back(range);
	}
	else {
	    std::cout << "UNRECOGNIZED CMD!\n";
	    return;
	}
	count++;
    }

}

//==============================================================
// EXEC
//==============================================================
inline void exec_load(int index_type, std::vector<keytype> &init_keys, std::vector<uint64_t> &values) {
    idx = getInstance<keytype, keycomp>(index_type);

    //WRITE ONLY TEST-----------------
    int count = (int)init_keys.size();
    double start_time = get_now();
    if (!idx->load(init_keys, values))
	return;
    double end_time = get_now();
    double tput = count / (end_time - start_time) / 1000000; //Mops/sec

    std::cout << "insert " << tput << "\n";
    std::cout << "memory " << (idx->getMemory() / 1000000) << "\n";
}

inline void exec_txn(int wl, std::vector<keytype> &keys, std::vector<uint64_t> &values, std::vector<int> &ranges, std::vector<int> &ops) {
    //READ/SCAN TEST----------------
    double start_time = get_now();
    int txn_num = 0;
    uint64_t s = 0;

    while ((txn_num < LIMIT) && (txn_num < (int)ops.size())) {
	if (ops[txn_num] == 1) { //READ
	    idx->find(keys[txn_num]);
	}
	else if (ops[txn_num] == 2) { //SCAN
	    idx->scan(keys[txn_num], ranges[txn_num] + RANGE_PLUS);
	}
	else {
	    std::cout << "UNRECOGNIZED CMD!\n";
	    return;
	}
	txn_num++;
    }

    double end_time = get_now();
    double tput = txn_num / (end_time - start_time) / 1000000; //Mops/sec

    if (wl == 0)
	std::cout << "read " << tput << "\n";
    else if (wl == 1)
	std::cout << "scan " << tput << "\n";
    else
	std::cout << "read " << tput << "\n";
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
	std::cout << "Usage:\n";
	std::cout << "1. workload type: c, e\n";
	std::cout << "2. index type: btree, art, cart, hrt\n";
	return 1;
    }

    int wl = 0;
    if (strcmp(argv[1], "c") == 0)
	wl = 0;
    else if (strcmp(argv[1], "e") == 0)
	wl = 1;

    int index_type = 0;
    if (strcmp(argv[2], "btree") == 0)
	index_type = 0;
    else if (strcmp(argv[2], "art") == 0)
	index_type = 1;
    else if (strcmp(argv[2], "cart") == 0)
	index_type = 2;
    else if (strcmp(argv[2], "hrt") == 0)
	index_type = 3;

    std::vector<keytype> init_keys;
    std::vector<keytype> keys;
    std::vector<uint64_t> values;
    std::vector<int> ranges;
    std::vector<int> ops;

    load(wl, init_keys, keys, values, ranges, ops);

    exec_load(index_type, init_keys, values);
    exec_txn(wl, keys, values, ranges, ops);

    return 0;
}
