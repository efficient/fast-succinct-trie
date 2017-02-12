#include <stdint.h>
#include <emmintrin.h>

#include <vector>
#include <algorithm>
#include <iostream>

#include <common.h>

#include <bitmap-rank.h>
#include <bitmap-rankF.h>
#include <bitmap-select.h>

using namespace std;

class FSTIter;
class FST;

class FST {
public:
    static const uint8_t TERM = 36; //$
    static const int CUTOFF_RATIO = 64;

    FST();
    virtual ~FST();

    void load(vector<string> &keys, vector<uint64_t> &values, int longestKeyLen);
    void load(vector<uint64_t> &keys, vector<uint64_t> &values);

    bool lookup(const uint8_t* key, const int keylen, uint64_t &value);
    bool lookup(const uint64_t key, uint64_t &value);

    bool lowerBound(const uint8_t* key, const int keylen, FSTIter &iter);
    bool lowerBound(const uint64_t key, FSTIter &iter);

    uint32_t cMemU();
    uint32_t tMemU();
    uint32_t oMemU();
    uint32_t keyMemU();
    uint32_t valueMemU();

    uint64_t cMem();
    uint32_t tMem();
    uint32_t sMem();
    uint64_t keyMem();
    uint64_t valueMem();

    uint64_t mem();

    uint32_t numT();

    void printU();
    void print();

private:
    inline bool insertChar_cond(const uint8_t ch, vector<uint8_t> &c, vector<uint64_t> &t, vector<uint64_t> &s, int &pos, int &nc);
    inline bool insertChar(const uint8_t ch, bool isTerm, vector<uint8_t> &c, vector<uint64_t> &t, vector<uint64_t> &s, int &pos, int &nc);

    inline bool isCbitSetU(uint64_t nodeNum, uint8_t kc);
    inline bool isTbitSetU(uint64_t nodeNum, uint8_t kc);
    inline bool isObitSetU(uint64_t nodeNum);
    inline bool isSbitSet(uint64_t pos);
    inline bool isTbitSet(uint64_t pos);
    inline uint64_t valuePosU(uint64_t nodeNum, uint64_t pos);
    inline uint64_t valuePos(uint64_t pos);

    inline uint64_t childNodeNumU(uint64_t pos);
    inline uint64_t childNodeNum(uint64_t pos);
    inline uint64_t childpos(uint64_t nodeNum);

    inline int nodeSize(uint64_t pos);
    inline bool simdSearch(uint64_t &pos, uint64_t size, uint8_t target);
    inline bool binarySearch(uint64_t &pos, uint64_t size, uint8_t target);
    inline bool linearSearch(uint64_t &pos, uint64_t size, uint8_t target);
    inline bool nodeSearch(uint64_t &pos, int size, uint8_t target);
    inline bool nodeSearch_lowerBound(uint64_t &pos, int size, uint8_t target);

    inline bool binarySearch_lowerBound(uint64_t &pos, uint64_t size, uint8_t target);
    inline bool linearSearch_lowerBound(uint64_t &pos, uint64_t size, uint8_t target);

    inline bool nextItemU(uint64_t nodeNum, uint8_t kc, uint8_t &cc);

    inline bool nextLeftU(int keypos, uint64_t pos, FSTIter* iter);
    inline bool nextLeft(int keypos, uint64_t pos, FSTIter* iter);

    inline bool nextNodeU(int keypos, uint64_t nodeNum, FSTIter* iter);
    inline bool nextNode(int keypos, uint64_t pos, FSTIter* iter);

    int cutoff_level_;
    uint64_t nodeCountU_;
    uint64_t childCountU_;

    BitmapRankFPoppy* cbitsU_;
    BitmapRankFPoppy* tbitsU_;
    BitmapRankFPoppy* obitsU_;
    uint64_t* valuesU_;

    uint8_t* cbytes_;
    BitmapRankPoppy* tbits_;
    BitmapSelectPoppy* sbits_;
    uint64_t* values_;

    //stats
    uint32_t tree_height_;
    int32_t last_value_pos_; // negative means in valuesU_

    uint32_t c_lenU_;
    uint32_t o_lenU_;

    uint32_t c_memU_;
    uint32_t t_memU_;
    uint32_t o_memU_;
    uint32_t val_memU_;

    uint64_t c_mem_;
    uint32_t t_mem_;
    uint32_t s_mem_;
    uint64_t val_mem_;

    uint32_t num_t_;

    friend class FSTIter;
};

typedef struct {
    int32_t keyPos;
    int32_t valPos;
    bool isO;
} Cursor;


class FSTIter {
public:
    FSTIter();
    FSTIter(FST* idx);

    void clear ();

    inline void setVU (int keypos, uint64_t nodeNum, uint64_t pos);
    inline void setKVU (int keypos, uint64_t nodeNum, uint64_t pos, bool o);
    inline void setV (int keypos, uint64_t pos);
    inline void setKV (int keypos, uint64_t pos);

    uint64_t value ();
    bool operator ++ (int);
    bool operator -- (int);

private:
    FST* index;
    vector<Cursor> positions;

    uint32_t len;
    bool isEnd;

    uint32_t cBoundU;
    uint64_t cBound;
    int cutoff_level;
    uint32_t tree_height;
    uint32_t last_value_pos;

    friend class FST;
};

