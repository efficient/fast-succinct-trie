#include "microbench.h"

#define INIT_LIMIT 25000000

typedef std::string keytype;
typedef std::less<uint64_t> keycomp;
//typedef GenericComparator<31> keycomp;

static const uint64_t key_type=0;
static const uint64_t value_type=1; // 0 = random pointers, 1 = pointers to keys

//==============================================================
// GET INSTANCE
//==============================================================
template<typename KeyType, class KeyComparator>
Index<KeyType, KeyComparator> *getInstance(const int type) {
    if (type == 0)
	return new ArtIndex_Email<KeyType, KeyComparator>();
    else if (type == 1)
	return new CArtIndex_Email<KeyType, KeyComparator>();
    else if (type == 2)
	return new FSTIndex_Email<KeyType, KeyComparator>();
    else
	return new ArtIndex_Email<KeyType, KeyComparator>();
}

//==============================================================
// LOAD
//==============================================================
inline void load(int wl, int index_type, std::vector<keytype> &init_keys, std::vector<keytype> &keys, std::vector<uint64_t> &values, std::vector<int> &ranges, std::vector<int> &ops) {
    std::string init_file;
    std::string txn_file;
    // 0 = c, 1 = e
    if (wl == 0) {
	init_file = "../../benchmark/workloads/load_email_workloadc";
	txn_file = "../../benchmark/workloads/txn_email_workloadc";
    }
    else if (wl == 1) {
	init_file = "../../benchmark/workloads/load_email_workloade";
	txn_file = "../../benchmark/workloads/txn_email_workloade";
    }
    else {
	init_file = "../../benchmark/workloads/load_email_workloadc";
	txn_file = "../../benchmark/workloads/txn_email_workloadc";
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

    if (value_type == 0) {
	while (count < INIT_LIMIT) {
	    value = base + rand();
	    values.push_back(value);
	    count++;
	}
    }
    else {
	while (count < INIT_LIMIT) {
	    values.push_back((uint64_t)(init_keys[count].data()));
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
inline void exec(int wl, int index_type, std::vector<keytype> &init_keys, std::vector<keytype> &keys, std::vector<uint64_t> &values, std::vector<int> &ranges, std::vector<int> &ops) {
    Index<keytype, keycomp> *idx = getInstance<keytype, keycomp>(index_type);

    //WRITE ONLY TEST-----------------
    double start_time = get_now();
    idx->load(init_keys, values);
    double end_time = get_now();
    double tput = init_keys.size() / (end_time - start_time) / 1000000; //Mops/sec

    std::cout << "insert " << tput << "\n";
    std::cout << "memory " << (idx->getMemory() / 1000000) << "\n";

    //READ/SCAN TEST----------------
    start_time = get_now();
    int txn_num = 0;

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

    end_time = get_now();
    tput = txn_num / (end_time - start_time) / 1000000; //Mops/sec

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
	std::cout << "2. index type: art, cart, hrt\n";
	return 1;
    }

    int wl = 0;
    if (strcmp(argv[1], "c") == 0)
	wl = 0;
    else if (strcmp(argv[1], "e") == 0)
	wl = 1;

    int index_type = 0;
    if (strcmp(argv[2], "art") == 0)
	index_type = 0;
    else if (strcmp(argv[2], "cart") == 0)
	index_type = 1;
    else if (strcmp(argv[2], "hrt") == 0)
	index_type = 2;

    std::vector<keytype> init_keys;
    std::vector<keytype> keys;
    std::vector<uint64_t> values;
    std::vector<int> ranges;
    std::vector<int> ops;

    load(wl, index_type, init_keys, keys, values, ranges, ops);
    exec(wl, index_type, init_keys, keys, values, ranges, ops);

    return 0;
}
