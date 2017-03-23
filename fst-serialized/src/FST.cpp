#include <FST.hpp>

inline bool insertChar_cond(const uint8_t ch, vector<uint8_t> &c, vector<uint64_t> &t, vector<uint64_t> &s, int &pos, int &nc) {
    if (c.empty() || c.back() != ch) {
	c.push_back(ch);
	if (c.size() == 1) {
	    setBit(s.back(), pos % 64);
	    nc++;
	}
	pos++;
	if (pos % 64 == 0) {
	    t.push_back(0);
	    s.push_back(0);
	}
	return true;
    }
    else {
	if (pos % 64 == 0)
	    setBit(t.rbegin()[1], 63);
	else
	    setBit(t.back(), (pos - 1) % 64);
	return false;
    }
}

inline bool insertChar(const uint8_t ch, bool isTerm, vector<uint8_t> &c, vector<uint64_t> &t, vector<uint64_t> &s, int &pos, int &nc) {
    c.push_back(ch);
    if (!isTerm)
	setBit(t.back(), pos % 64);
    setBit(s.back(), pos % 64);
    nc++;
    pos++;
    if (pos % 64 == 0) {
	t.push_back(0);
	s.push_back(0);
    }
    return true;
}

//******************************************************
// LOAD
//******************************************************
FST* load(vector<string> &keys, vector<uint64_t> &values, int longestKeyLen) {
    uint16_t tree_height = longestKeyLen;
    vector<vector<uint8_t> > c;
    vector<vector<uint64_t> > t;
    vector<vector<uint64_t> > s;
    vector<vector<uint64_t> > val;

    vector<int> pos_list;
    vector<int> nc; //node count

    // init
    for (int i = 0; i < longestKeyLen; i++) {
	c.push_back(vector<uint8_t>());
	t.push_back(vector<uint64_t>());
	s.push_back(vector<uint64_t>());
	val.push_back(vector<uint64_t>());

	pos_list.push_back(0);
	nc.push_back(0);

	t[i].push_back(0);
	s[i].push_back(0);
    }

    int first_value_level = 0;
    int last_value_level = 0;
    for (int k = 0; k < (int)keys.size(); k++) {
	string key = keys[k];
	uint64_t value = values[k];

	// if same key
	if (k < (int)(keys.size()-1) && key.compare(keys[k+1]) == 0)
	    continue;

	int i = 0;
	while (i < key.length() && !insertChar_cond((uint8_t)key[i], c[i], t[i], s[i], pos_list[i], nc[i]))
	    i++;

	if (i < key.length()) {
	    if (k + 1 < (int)keys.size()) {
		int cpl = commonPrefixLen(key, keys[k+1]);
		if (i < cpl) {
		    if (pos_list[i] % 64 == 0)
			setBit(t[i].rbegin()[1], 63);
		    else
			setBit(t[i].back(), (pos_list[i] - 1) % 64);
		}

		while (i < cpl) {
		    i++;
		    if (i < cpl)
			insertChar((uint8_t)key[i], false, c[i], t[i], s[i], pos_list[i], nc[i]);
		    else {
			if (i < key.length())
			    insertChar((uint8_t)key[i], true, c[i], t[i], s[i], pos_list[i], nc[i]);
			else
			    insertChar(TERM, true, c[i], t[i], s[i], pos_list[i], nc[i]);
		    }
		}
	    }
	    val[i].push_back(value);
	}
	else
	    cout << "ERROR!\n";

	if (k == 0)
	    first_value_level = i;

	if (k >= keys.size() - 1)
	    last_value_level = i;
    }

    // put together
    int nc_total = 0;
    for (int i = 0; i < (int)nc.size(); i++)
	nc_total += nc[i];

    int16_t cutoff_level = 0;
    int8_t first_value_pos = 0;
    int32_t last_value_pos = 0;
    int nc_u = 0;
    while (nc_u * CUTOFF_RATIO < nc_total) {
	nc_u += nc[cutoff_level];
	cutoff_level++;
    }
    cutoff_level--;

    //cout << "cutoff_level = " << cutoff_level << "\n";

    // determine the position of the first value for range query boundary check
    if (first_value_level < cutoff_level)
	first_value_pos = -1;
    else
	first_value_pos = 1;

    // determine the position of the last value for range query boundary check
    if (last_value_level < cutoff_level) {
	for (int i = 0; i <= last_value_level; i++)
	    last_value_pos -= val[i].size();
	last_value_pos++;
    }
    else {
	for (int i = cutoff_level; i <= last_value_level; i++)
	    last_value_pos += val[i].size();
	last_value_pos--;
    }

    //-------------------------------------------------
    vector<vector<uint64_t> > cU;
    vector<vector<uint64_t> > tU;
    vector<vector<uint8_t> > oU;
    uint32_t nodeCountU = 0;
    uint32_t childCountU = 0;

    for (int i = 0; i < cutoff_level; i++) {
	cU.push_back(vector<uint64_t>());
	tU.push_back(vector<uint64_t>());
	oU.push_back(vector<uint8_t>());
    }

    for (int i = 0; i < cutoff_level; i++) {
	uint64_t sbitPos_i = 0;
	uint64_t smaxPos_i = pos_list[i];

	uint8_t ch = 0;
	int j = 0;
	int k = 0;
	for (j = 0; j < (int)s[i].size(); j++) {
	    if (sbitPos_i >= smaxPos_i) break;
	    for (k = 0; k < 64; k++) {
		if (sbitPos_i >= smaxPos_i) break;

		ch = c[i][sbitPos_i];
		if (readBit(s[i][j], k)) {
		    for (int l = 0; l < 4; l++) {
			cU[i].push_back(0);
			tU[i].push_back(0);
		    }
		    nodeCountU++;

		    if (ch == TERM)
			oU[i].push_back(1);
		    else {
			oU[i].push_back(0);
			setLabel((uint64_t*)cU[i].data() + cU[i].size() - 4, ch);
			if (readBit(t[i][j], k)) {
			    setLabel((uint64_t*)tU[i].data() + tU[i].size() - 4, ch);
			    childCountU++;
			}
		    }
		}
		else {
		    setLabel((uint64_t*)cU[i].data() + cU[i].size() - 4, ch);
		    if (readBit(t[i][j], k)) {
			setLabel((uint64_t*)tU[i].data() + tU[i].size() - 4, ch);
			childCountU++;
		    }
		}
		sbitPos_i++;
	    }
	}
    }

    //-------------------------------------------------    
    int cUlen = 0;
    int oUlen = 0;
    int vallenU = 0;
    for (int i = 0; i < (int)cU.size(); i++) {
	cUlen += cU[i].size();
	oUlen += oU[i].size();
	vallenU += val[i].size();
    }

    int c_sizeU = (cUlen / 32 + 1) * 32; // round-up to 2048-bit block size
    int t_sizeU = (cUlen / 32 + 1) * 32; // round-up to 2048-bit block size
    int o_sizeU = (oUlen / 64 / 32 + 1) * 32; // round-up to 2048-bit block size

    uint32_t cUmem = c_sizeU * 8;
    uint32_t tUmem = t_sizeU * 8;
    uint32_t oUmem = o_sizeU * 8;
    uint32_t valUmem = vallenU * 8;

    uint32_t cUnbits = cUmem * 8;
    uint32_t tUnbits = tUmem * 8;
    uint32_t oUnbits = oUmem * 8;

    uint32_t cmem = 0;    
    uint32_t tmem = 0;
    uint32_t smem = 0;
    uint32_t valmem = 0;

    for (int i = cutoff_level; i < (int)c.size(); i++)
	cmem += c[i].size();

    for (int i = cutoff_level; i < (int)c.size(); i++)
	valmem += val[i].size() * sizeof(uint64_t);

    if (cmem % 64 == 0) {
	tmem = cmem / 64;
	smem = cmem / 64;
    }
    else {
	tmem = cmem / 64 + 1;
	smem = cmem / 64 + 1;
    }
    tmem = (tmem / 32 + 1) * 32; // round-up to 2048-bit block size for Poppy
    smem = (smem / 32 + 1) * 32; // round-up to 2048-bit block size for Poppy

    tmem *= 8;
    smem *= 8;

    uint32_t tnbits = tmem * 8;
    uint32_t snbits = smem * 8;

    uint32_t wordCount = snbits / kWordSize;
    uint64_t* sbits_tmp = new uint64_t[smem/8];
    memset(sbits_tmp, 0, smem);
    uint64_t s_bitPos = 0;
    for (int i = cutoff_level; i < (int)s.size(); i++) {
	uint64_t bitPos_i = 0;
	uint64_t maxPos_i = pos_list[i];
	for (int j = 0; j < (int)s[i].size(); j++) {
	    if (bitPos_i >= maxPos_i) break;
	    for (int k = 0; k < 64; k++) {
		if (bitPos_i >= maxPos_i) break;
		if (readBit(s[i][j], k))
		    setBit(sbits_tmp[s_bitPos/64], s_bitPos % 64);
		s_bitPos++;
		bitPos_i++;
	    }
	}
    }
    uint32_t spCount = 0;
    for (uint32_t i = 0; i < wordCount; i++) {
	spCount += popcount(sbits_tmp[i]);
    }
    uint32_t sselectLUTCount = spCount / skip + 1;

    //-------------------------------------------------
    uint32_t totalMem = cUmem + (cUnbits / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem + (tUnbits / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem + (oUnbits / kBasicBlockSizeU) * sizeof(uint32_t) + cmem + tmem + (tnbits / kBasicBlockSize) * sizeof(uint32_t) + smem + (sselectLUTCount + 1) * sizeof(uint32_t) + valUmem + valmem;

    void* ptr = malloc(sizeof(FST) + totalMem);
    memset(ptr, 0, sizeof(FST) + totalMem);
    FST* fst = new(ptr) FST(cutoff_level, tree_height, first_value_pos, last_value_pos, 
			    nodeCountU, childCountU, 
			    cUnbits, cUmem, tUnbits, tUmem, oUnbits, oUmem,
			    valUmem, cmem, tnbits, tmem, 
			    smem, sselectLUTCount, valmem);

    //-------------------------------------------------
    uint64_t* cUbits = fst->cUbits_();
    uint64_t* tUbits = fst->tUbits_();
    uint64_t* oUbits = fst->oUbits_();
    uint64_t* valuesU = fst->valuesU_();

    uint8_t* cbytes = fst->cbytes_();
    uint64_t* tbits = fst->tbits_();
    uint64_t* sbits = fst->sbits_();
    uint64_t* valuesS = fst->values_();

    uint64_t c_bitPosU = 0;
    for (int i = 0; i < (int)cU.size(); i++) {
	for (int j = 0; j < (int)cU[i].size(); j++) {
	    for (int k = 0; k < 64; k++) {
		if (readBit(cU[i][j], k))
		    setBit(cUbits[c_bitPosU/64], c_bitPosU % 64);
		c_bitPosU++;
	    }
	}
    }

    fst->cUinit(cUbits, c_sizeU * 64);

    uint64_t t_bitPosU = 0;
    for (int i = 0; i < (int)tU.size(); i++) {
	for (int j = 0; j < (int)tU[i].size(); j++) {
	    for (int k = 0; k < 64; k++) {
		if (readBit(tU[i][j], k))
		    setBit(tUbits[t_bitPosU/64], t_bitPosU % 64);
		t_bitPosU++;
	    }
	}
    }

    fst->tUinit(tUbits, t_sizeU * 64);

    uint64_t o_bitPosU = 0;
    for (int i = 0; i < (int)oU.size(); i++) {
	for (int j = 0; j < (int)oU[i].size(); j++) {
	    if (oU[i][j] == 1)
		setBit(oUbits[o_bitPosU/64], o_bitPosU % 64);
	    o_bitPosU++;
	}
    }

    fst->oUinit(oUbits, o_sizeU * 64);

    uint64_t val_posU = 0;
    for (int i = 0; i < cutoff_level; i++) {
	for (int j = 0; j < (int)val[i].size(); j++) {
	    valuesU[val_posU] = val[i][j];
	    val_posU++;
	}
    }

    //-------------------------------------------------
    uint64_t c_pos = 0;
    for (int i = cutoff_level; i < (int)c.size(); i++) {
	for (int j = 0; j < (int)c[i].size(); j++) {
	    cbytes[c_pos] = c[i][j];
	    c_pos++;
	}
    }

    uint64_t t_bitPos = 0;
    for (int i = cutoff_level; i < (int)t.size(); i++) {
	uint64_t bitPos_i = 0;
	uint64_t maxPos_i = pos_list[i];
	for (int j = 0; j < (int)t[i].size(); j++) {
	    if (bitPos_i >= maxPos_i) break;
	    for (int k = 0; k < 64; k++) {
		if (bitPos_i >= maxPos_i) break;
		if (readBit(t[i][j], k))
		    setBit(tbits[t_bitPos/64], t_bitPos % 64);
		t_bitPos++;
		bitPos_i++;
	    }
	}
    }

    fst->tinit(tbits, tmem * 8);

    s_bitPos = 0;
    for (int i = cutoff_level; i < (int)s.size(); i++) {
	uint64_t bitPos_i = 0;
	uint64_t maxPos_i = pos_list[i];
	for (int j = 0; j < (int)s[i].size(); j++) {
	    if (bitPos_i >= maxPos_i) break;
	    for (int k = 0; k < 64; k++) {
		if (bitPos_i >= maxPos_i) break;
		if (readBit(s[i][j], k))
		    setBit(sbits[s_bitPos/64], s_bitPos % 64);
		s_bitPos++;
		bitPos_i++;
	    }
	}
    }

    fst->sinit(sbits, smem * 8);

    uint64_t val_pos = 0;
    for (int i = cutoff_level; i < (int)val.size(); i++) {
	for (int j = 0; j < (int)val[i].size(); j++) {
	    valuesS[val_pos] = val[i][j];
	    val_pos++;
	}
    }

    return fst;
}

FST* load(vector<uint64_t> &keys, vector<uint64_t> &values) {
    vector<string> keys_str;
    for (int i = 0; i < (int)keys.size(); i++) {
	char key[8];
	reinterpret_cast<uint64_t*>(key)[0]=__builtin_bswap64(keys[i]);
	keys_str.push_back(string(key, 8));
    }
    return load(keys_str, values, sizeof(uint64_t));
}


//*******************************************************************
FST::FST() 
    : cutoff_level_(0), tree_height_(0), first_value_pos_(0),
      last_value_pos_(0), nodeCountU_(0), childCountU_(0),
      cUnbits_(0), cUpCount_(0), cUmem_(0),
      tUnbits_(0), tUpCount_(0), tUmem_(0),
      oUnbits_(0), oUpCount_(0), oUmem_(0),
      valUmem_(0),
      cmem_(0),
      tnbits_(0), tpCount_(0), tmem_(0),
      snbits_(0), spCount_(0), smem_(0), sselectLUTCount_(0),
      valmem_(0) {}

FST::FST(int16_t cl, uint16_t th, int8_t fvp, int32_t lvp, uint32_t ncu, uint32_t ccu,
	 uint32_t cUnb, uint32_t cUmem, uint32_t tUnb, uint32_t tUmem,
	 uint32_t oUnb, uint32_t oUmem, uint32_t vUm, uint32_t cmem,
	 uint32_t tnb, uint32_t tmem, uint32_t smem, uint32_t sbbc, uint32_t vm) 
    : cutoff_level_(cl), tree_height_(th), first_value_pos_(fvp),
      last_value_pos_(lvp), nodeCountU_(ncu), childCountU_(ccu),
      cUnbits_(cUnb), cUpCount_(0), cUmem_(cUmem),
      tUnbits_(tUnb), tUpCount_(0), tUmem_(tUmem),
    oUnbits_(oUnb), oUpCount_(0), oUmem_(oUmem),
    valUmem_(vUm),
    cmem_(cmem),
    tnbits_(tnb), tpCount_(0), tmem_(tmem),
    snbits_(0), spCount_(0), smem_(smem), sselectLUTCount_(sbbc),
    valmem_(vm) {}

FST::~FST() {}

//stat
uint32_t FST::cMemU() { return cUmem_; }
uint32_t FST::cRankMemU() { return (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t); }
uint32_t FST::tMemU() { return tUmem_; }
uint32_t FST::tRankMemU() { return (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t); }
uint32_t FST::oMemU() { return oUmem_;}
uint32_t FST::oRankMemU() { return (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t); }
uint32_t FST::valueMemU() { return valUmem_; }

uint64_t FST::cMem() { return cmem_; }
uint32_t FST::tMem() { return tmem_; }
uint32_t FST::tRankMem() { return (tnbits_ / kBasicBlockSize) * sizeof(uint32_t); }
uint32_t FST::sMem() { return smem_;}
uint32_t FST::sSelectMem() { return (sselectLUTCount_ + 1) * sizeof(uint32_t); }
uint64_t FST::valueMem() { return valmem_; }

uint64_t FST::mem() {
    return sizeof(FST) + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_ + (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + cmem_ + tmem_ + (tnbits_ / kBasicBlockSize) * sizeof(uint32_t) + smem_ + (sselectLUTCount_ + 1) * sizeof(uint32_t) + valUmem_ + valmem_;
}

//******************************************************
// rank and select support
//******************************************************
//*******************************************************************
inline void FST::cUinit(uint64_t* bits, uint32_t nbits) {
    cUnbits_ = nbits;
    uint32_t* cUrankLUT = cUrankLUT_();

    uint32_t rankCum = 0;
    for (uint32_t i = 0; i < (cUnbits_ / kBasicBlockSizeU); i++) {
	cUrankLUT[i] = rankCum;
	rankCum += popcountLinear(bits, 
				  i * kWordCountPerBasicBlockU, 
				  kBasicBlockSizeU);
    }
    cUrankLUT[(cUnbits_ / kBasicBlockSizeU)-1] = rankCum;

    cUpCount_ = rankCum;
}

inline uint32_t FST::cUrank(uint32_t pos) {
    assert(pos <= cUnbits_);
    uint32_t blockId = pos >> kBasicBlockBitsU;
    uint32_t offset = pos & (uint32_t)63;

    uint32_t* cUrankLUT = cUrankLUT_();
    uint64_t* cUbits = cUbits_();
    if (offset)
	return cUrankLUT[blockId] + popcount(cUbits[blockId] >> (64 - offset));
    else
	return cUrankLUT[blockId];
}

//*******************************************************************
inline void FST::tUinit(uint64_t* bits, uint32_t nbits) {
    tUnbits_ = nbits;
    uint32_t* tUrankLUT = tUrankLUT_();

    uint32_t rankCum = 0;
    for (uint32_t i = 0; i < (tUnbits_ / kBasicBlockSizeU); i++) {
	tUrankLUT[i] = rankCum;
	rankCum += popcountLinear(bits, 
				  i * kWordCountPerBasicBlockU, 
				  kBasicBlockSizeU);
    }
    tUrankLUT[(tUnbits_ / kBasicBlockSizeU)-1] = rankCum;

    tUpCount_ = rankCum;
}

inline uint32_t FST::tUrank(uint32_t pos) {
    assert(pos <= tUnbits_);
    uint32_t blockId = pos >> kBasicBlockBitsU;
    uint32_t offset = pos & (uint32_t)63;

    uint32_t* tUrankLUT = tUrankLUT_();
    uint64_t* tUbits = tUbits_();
    if (offset)
	return tUrankLUT[blockId] + popcount(tUbits[blockId] >> (64 - offset));
    else
	return tUrankLUT[blockId];
}

//*******************************************************************
inline void FST::oUinit(uint64_t* bits, uint32_t nbits) {
    oUnbits_ = nbits;
    uint32_t* oUrankLUT = oUrankLUT_();

    uint32_t rankCum = 0;
    for (uint32_t i = 0; i < (oUnbits_ / kBasicBlockSizeU); i++) {
	oUrankLUT[i] = rankCum;
	rankCum += popcountLinear(bits, 
				  i * kWordCountPerBasicBlockU, 
				  kBasicBlockSizeU);
    }
    oUrankLUT[(tUnbits_ / kBasicBlockSizeU)-1] = rankCum;

    oUpCount_ = rankCum;
}

inline uint32_t FST::oUrank(uint32_t pos) {
    assert(pos <= oUnbits_);
    uint32_t blockId = pos >> kBasicBlockBitsU;
    uint32_t offset = pos & (uint32_t)63;

    uint32_t* oUrankLUT = oUrankLUT_();
    uint64_t* oUbits = oUbits_();
    if (offset)
	return oUrankLUT[blockId] + popcount(oUbits[blockId] >> (64 - offset));
    else
	return oUrankLUT[blockId];
}

//*******************************************************************
inline void FST::tinit(uint64_t* bits, uint32_t nbits) {
    tnbits_ = nbits;    
    uint32_t* trankLUT = trankLUT_();

    uint32_t rankCum = 0;
    for (uint32_t i = 0; i < (tnbits_ / kBasicBlockSize); i++) {
	trankLUT[i] = rankCum;
	rankCum += popcountLinear(bits, 
				  i * kWordCountPerBasicBlock, 
				  kBasicBlockSize);
    }
    trankLUT[(tnbits_ / kBasicBlockSize)-1] = rankCum;

    tpCount_ = rankCum;
}

inline uint32_t FST::trank(uint32_t pos) {
    assert(pos <= tnbits_);
    uint32_t blockId = pos >> kBasicBlockBits;
    uint32_t* trankLUT = trankLUT_();
    uint64_t* tbits = tbits_();
    return trankLUT[blockId] + popcountLinear(tbits, (blockId << 3), (pos & 511));
}

//*******************************************************************
inline void FST::sinit(uint64_t* bits, uint32_t nbits) {
    snbits_ = nbits;

    uint64_t* sbits = sbits_();

    uint32_t wordCount_ = snbits_ / kWordSize;

    uint32_t* rankLUT;
    assert(posix_memalign((void **) &rankLUT, kCacheLineSize, (wordCount_ + 1) * sizeof(uint32_t)) >= 0);

    uint32_t rankCum = 0;
    for (uint32_t i = 0; i < wordCount_; i++) {
	rankLUT[i] = rankCum;
	rankCum += popcount(sbits[i]);
    }
    rankLUT[wordCount_] = rankCum;
    spCount_ = rankCum;

    sselectLUTCount_ = spCount_ / skip + 1;

    uint32_t* sselectLUT = sselectLUT_();

    sselectLUT[0] = 0;
    uint32_t idx = 1;
    for (uint32_t i = 1; i <= wordCount_; i++) {
	while (idx * skip <= rankLUT[i]) {
	    int rankR = idx * skip - rankLUT[i-1];
	    sselectLUT[idx] = (i - 1) * kWordSize + select64_popcount_search(sbits[i-1], rankR) + 1;
	    idx++;
	}
    }

    free(rankLUT);
}
 
inline uint32_t FST::sselect(uint32_t rank) {
    assert(rank <= spCount_);

    uint32_t* sselectLUT = sselectLUT_();
    uint64_t* sbits = sbits_();

    uint32_t s = sselectLUT[rank >> kSkipBits];
    uint32_t rankR = rank & kSkipMask;

    if (rankR == 0)
	return s - 1;

    int idx = s >> kWordBits;
    int startWordBit = s & kWordMask;
    uint64_t word = sbits[idx] << startWordBit >> startWordBit;

    int pop = 0;
    while ((pop = popcount(word)) < rankR) {
	idx++;
	word = sbits[idx];
	rankR -= pop;
    }

    return (idx << kWordBits) + select64_popcount_search(word, rankR);
}

//******************************************************
// bit/byte vector accessors
//******************************************************
//*******************************************************************
inline uint64_t* FST::cUbits_() {return (uint64_t*)data_;}

inline uint32_t* FST::cUrankLUT_() {return (uint32_t*)(data_ + cUmem_);}

inline uint64_t* FST::tUbits_() {return (uint64_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t));}

inline uint32_t* FST::tUrankLUT_() {return (uint32_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_);}

inline uint64_t* FST::oUbits_() {return (uint64_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t));}

inline uint32_t* FST::oUrankLUT_() {return (uint32_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_);}

inline uint8_t* FST::cbytes_() {return (uint8_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_ + (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t));}

inline uint64_t* FST::tbits_() {return (uint64_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_ + (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + cmem_);}

inline uint32_t* FST::trankLUT_() {return (uint32_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_ + (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + cmem_ + tmem_);}

inline uint64_t* FST::sbits_() {return (uint64_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_ + (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + cmem_ + tmem_ + (tnbits_ / kBasicBlockSize) * sizeof(uint32_t));}

inline uint32_t* FST::sselectLUT_() {return (uint32_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_ + (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + cmem_ + tmem_ + (tnbits_ / kBasicBlockSize) * sizeof(uint32_t) + smem_);}

inline uint64_t* FST::valuesU_() {return (uint64_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_ + (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + cmem_ + tmem_ + (tnbits_ / kBasicBlockSize) * sizeof(uint32_t) + smem_ + (sselectLUTCount_ + 1) * sizeof(uint32_t));}

inline uint64_t* FST::values_() {return (uint64_t*)(data_ + cUmem_ + (cUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + tUmem_ + (tUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + oUmem_ + (oUnbits_ / kBasicBlockSizeU) * sizeof(uint32_t) + cmem_ + tmem_ + (tnbits_ / kBasicBlockSize) * sizeof(uint32_t) + smem_ + (sselectLUTCount_ + 1) * sizeof(uint32_t) + valUmem_);}

//******************************************************
// IS O BIT SET U?
//******************************************************
inline bool FST::isCbitSetU(uint64_t nodeNum, uint8_t kc) {
    return isLabelExist(cUbits_() + (nodeNum << 2), kc);
}
//******************************************************
// IS O BIT SET U?
//******************************************************
inline bool FST::isTbitSetU(uint64_t nodeNum, uint8_t kc) {
    return isLabelExist(tUbits_() + (nodeNum << 2), kc);
}
//******************************************************
// IS O BIT SET U?
//******************************************************
inline bool FST::isObitSetU(uint64_t nodeNum) {
    return readBit(oUbits_()[nodeNum >> 6], nodeNum & (uint64_t)63);
}
//******************************************************
// IS S BIT SET?
//******************************************************
inline bool FST::isSbitSet(uint64_t pos) {
    return readBit(sbits_()[pos >> 6], pos & (uint64_t)63);
}
//******************************************************
// IS T BIT SET?
//******************************************************
inline bool FST::isTbitSet(uint64_t pos) {
    return readBit(tbits_()[pos >> 6], pos & (uint64_t)63);
}
//******************************************************
// GET VALUE POS U
//******************************************************
inline uint64_t FST::valuePosU(uint64_t nodeNum, uint64_t pos) {
    return cUrank(pos + 1) - tUrank(pos + 1) + oUrank(nodeNum + 1) - 1;
}
//******************************************************
// GET VALUE POS
//******************************************************
inline uint64_t FST::valuePos(uint64_t pos) {
    return pos - trank(pos+1);
}

//******************************************************
// CHILD NODE NUM
//******************************************************
inline uint64_t FST::childNodeNumU(uint64_t pos) {
    return tUrank(pos + 1);
}
inline uint64_t FST::childNodeNum(uint64_t pos) {
    return trank(pos + 1);
}

//******************************************************
// CHILD POS
//******************************************************
inline uint64_t FST::childpos(uint64_t nodeNum) {
    return sselect(nodeNum - nodeCountU_ + 1);
}


//******************************************************
// NODE SIZE
//******************************************************
inline int FST::nodeSize(uint64_t pos) {
    pos++;
    uint64_t startIdx = pos >> 6;
    uint64_t shift = pos & (uint64_t)0x3F;

    uint64_t bits = sbits_()[startIdx] << shift;
    if (bits > 0)
	return __builtin_clzll(bits) + 1;

    for (int i = 1; i < 5; i++) {
	bits = sbits_()[startIdx + i];
	if (bits > 0)
	    return 64 * i - shift + __builtin_clzll(bits) + 1;
    }

    if (pos < cmem_)
	return (cmem_ - pos + 1);

    return -1;
}

//******************************************************
// SIMD SEARCH
//******************************************************
inline bool FST::simdSearch(uint64_t &pos, uint64_t size, uint8_t target) {
    uint64_t s = 0;
    while (size >> 4) {
	__m128i cmp= _mm_cmpeq_epi8(_mm_set1_epi8(target), _mm_loadu_si128(reinterpret_cast<__m128i*>(cbytes_() + pos + s)));
	unsigned bitfield= _mm_movemask_epi8(cmp);
	if (bitfield) {
	    pos += (s + __builtin_ctz(bitfield));
	    return true;
	}
	s += 16;
	size -= 16;
    }

    if (size > 0) {
	__m128i cmp= _mm_cmpeq_epi8(_mm_set1_epi8(target), _mm_loadu_si128(reinterpret_cast<__m128i*>(cbytes_() + pos + s)));
	unsigned bitfield= _mm_movemask_epi8(cmp) & ((1 << size) - 1);
	if (bitfield) {
	    pos += (s + __builtin_ctz(bitfield));
	    return true;
	}
    }
    return false;
}

//******************************************************
// BINARY SEARCH
//******************************************************
inline bool FST::binarySearch(uint64_t &pos, uint64_t size, uint8_t target) {
    uint64_t l = pos;
    uint64_t r = pos + size - 1;
    uint64_t m = (l + r) >> 1;

    while (l <= r) {
	if (cbytes_()[m] == target) {
	    pos = m;
	    return true;
	}
	else if (cbytes_()[m] < target)
	    l = m + 1;
	else
	    r = m - 1;
	m = (l + r) >> 1;
    }
    return false;
}

inline bool FST::binarySearch_lowerBound(uint64_t &pos, uint64_t size, uint8_t target) {
    uint64_t leftBound = pos;
    uint64_t rightBound = pos + size;
    uint64_t l = pos;
    uint64_t r = pos + size - 1;
    uint64_t m = (l + r) >> 1;

    while (l < r) {
	if (m == leftBound)
	    break;

	if (cbytes_()[m] == target) {
	    pos = m;
	    return true;
	}
	else if (cbytes_()[m] < target)
	    l = m + 1;
	else
	    r = m - 1;
	m = (l + r) >> 1;
    }

    if ((m + 1) < rightBound && cbytes_()[m + 1] < target)
	pos = m + 2;
    else if (cbytes_()[m] < target)
	pos = m + 1;
    else
	pos = m;

    return pos < rightBound;
}

inline bool FST::binarySearch_upperBound(uint64_t &pos, uint64_t size, uint8_t target) {
    uint64_t leftBound = pos;
    uint64_t rightBound = pos + size;
    uint64_t l = pos;
    uint64_t r = pos + size - 1;
    uint64_t m = (l + r) >> 1;

    while (l < r) {
	if (m == leftBound)
	    break;

	if (cbytes_()[m] == target) {
	    pos = m;
	    return true;
	}
	else if (cbytes_()[m] < target)
	    l = m + 1;
	else
	    r = m - 1;
	m = (l + r) >> 1;
    }

    if (cbytes_()[m] > target) {
	if (m == 0)
	    return false;
	pos = m - 1;
    }
    else if ((m + 1) < rightBound && cbytes_()[m + 1] <= target)
	pos = m + 1;
    else
	pos = m;

    return pos >= leftBound;
}

//******************************************************
// LINEAR SEARCH
//******************************************************
inline bool FST::linearSearch(uint64_t &pos, uint64_t size, uint8_t target) {
    for (int i = 0; i < size; i++) {
	if (cbytes_()[pos] == target)
	    return true;
	pos++;
    }
    return false;
}

inline bool FST::linearSearch_lowerBound(uint64_t &pos, uint64_t size, uint8_t target) {
    for (int i = 0; i < size; i++) {
	if (cbytes_()[pos] >= target)
	    return true;
	pos++;
    }
    return false;
}

inline bool FST::linearSearch_upperBound(uint64_t &pos, uint64_t size, uint8_t target) {
    pos += (size - 1);
    for (int i = 0; i < size; i++) {
	if (cbytes_()[pos] <= target)
	    return true;
	pos--;
    }
    return false;
}

//******************************************************
// NODE SEARCH
//******************************************************
inline bool FST::nodeSearch(uint64_t &pos, int size, uint8_t target) {
    if (size < 3)
	return linearSearch(pos, size, target);
    else if (size < 12)
	return binarySearch(pos, size, target);
    else
	return simdSearch(pos, size, target);
}

inline bool FST::nodeSearch_lowerBound(uint64_t &pos, int size, uint8_t target) {
    if (size < 3)
	return linearSearch_lowerBound(pos, size, target);
    else
	return binarySearch_lowerBound(pos, size, target);
}

inline bool FST::nodeSearch_upperBound(uint64_t &pos, int size, uint8_t target) {
    if (size < 3)
	return linearSearch_upperBound(pos, size, target);
    else
	return binarySearch_upperBound(pos, size, target);
}


//******************************************************
// LOOKUP
//******************************************************
bool FST::lookup(const uint8_t* key, const int keylen, uint64_t &value) {
    int keypos = 0;
    uint64_t nodeNum = 0;
    uint8_t kc = (uint8_t)key[keypos];
    uint64_t pos = kc;

    while (keypos < keylen && keypos < cutoff_level_) {
	kc = (uint8_t)key[keypos];
	pos = (nodeNum << 8) + kc;

	__builtin_prefetch(tUbits_() + (nodeNum << 2) + (kc >> 6), 0);
	__builtin_prefetch(tUrankLUT_() + ((pos + 1) >> 6), 0);

	if (!isCbitSetU(nodeNum, kc))
	    return false;

	if (!isTbitSetU(nodeNum, kc)) {
	    value = valuesU_()[valuePosU(nodeNum, pos)];
	    return true;
	}

	nodeNum = childNodeNumU(pos);
	keypos++;
    }

    if (keypos < cutoff_level_) {
	if (isObitSetU(nodeNum)) {
	    value = valuesU_()[valuePosU(nodeNum, (nodeNum << 8))];
	    return true;
	}
	return false;
    }

    //-----------------------------------------------------------------------
    pos = (cutoff_level_ == 0) ? 0 : childpos(nodeNum);

    while (keypos < keylen) {
	kc = (uint8_t)key[keypos];

	int nsize = nodeSize(pos);
	if (!nodeSearch(pos, nsize, kc))
	    return false;

	if (!isTbitSet(pos)) {
	    value = values_()[valuePos(pos)];
	    return true;
	}

	pos = childpos(childNodeNum(pos) + childCountU_);
	keypos++;

	__builtin_prefetch(cbytes_() + pos, 0, 1);
	__builtin_prefetch(tbits_() + (pos >> 6), 0, 1);
	__builtin_prefetch(trankLUT_() + ((pos + 1) >> 9), 0);
    }

    if (cbytes_()[pos] == TERM && !isTbitSet(pos)) {
	value = values_()[valuePos(pos)];
	return true;
    }
    return false;
}

bool FST::lookup(const uint64_t key, uint64_t &value) {
    uint8_t key_str[8];
    reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(key);

    return lookup(key_str, 8, value);
}


//******************************************************
// NEXT ITEM U
//******************************************************
// Find an existing label cc >= kc in node nodeNum
// Return false if there isn't one
inline bool FST::nextItemU(uint64_t nodeNum, uint8_t kc, uint8_t &cc) {
    return isLabelExist_lowerBound(cUbits_() + (nodeNum << 2), kc, cc);
}

//******************************************************
// PREV ITEM U
//******************************************************
// Find an existing label cc <= kc in node nodeNum
// Return false if there isn't one
inline bool FST::prevItemU(uint64_t nodeNum, uint8_t kc, uint8_t &cc) {
    return isLabelExist_upperBound(cUbits_() + (nodeNum << 2), kc, cc);
}

//******************************************************
// NEXT LEFT ITEM
//******************************************************
// Find the left most leaf node in a subtree
inline bool FST::nextLeftU(int level, uint64_t pos, FSTIter* iter) {
    uint64_t nodeNum = pos >> 8;
    uint8_t cc = pos & 255;

    if (!isTbitSetU(nodeNum, cc)) {
	iter->setVU(level, nodeNum, pos);
	return true;
    }

    level++;
    nodeNum = (iter->positions[level].keyPos < 0) ? childNodeNumU(pos) : (iter->positions[level].keyPos >> 8);

    while (level < cutoff_level_) {
	if (isObitSetU(nodeNum)) {
	    iter->setKVU(level, nodeNum, (nodeNum << 8), true);
	    return true;
	}
	nextItemU(nodeNum, 0, cc);
	pos = (nodeNum << 8) + cc;
	iter->positions[level].keyPos = pos;

	if (!isTbitSetU(nodeNum, cc)) {
	    iter->setVU(level, nodeNum, pos);
	    return true;
	}

	level++;
	nodeNum = (iter->positions[level].keyPos < 0) ? childNodeNumU(pos) : (iter->positions[level].keyPos >> 8);
    }

    pos = (iter->positions[level].keyPos < 0) ? childpos(nodeNum) : iter->positions[level].keyPos;
    return nextLeft(level, pos, iter);
}

inline bool FST::nextLeft(int level, uint64_t pos, FSTIter* iter) {
    while (isTbitSet(pos)) {
	iter->positions[level].keyPos = pos;
	level++;
	pos = (iter->positions[level].keyPos < 0) ? childpos(childNodeNum(pos) + childCountU_) : iter->positions[level].keyPos;
    }
    iter->setKV(level, pos);
    return true;
}

//******************************************************
// NEXT RIGHT ITEM
//******************************************************
// Find the right most leaf node in a subtree
inline bool FST::nextRightU(int level, uint64_t pos, FSTIter* iter) {
    uint64_t nodeNum = pos >> 8;
    uint8_t cc = pos & 255;

    if (!isTbitSetU(nodeNum, cc)) {
	iter->setVU_R(level, nodeNum, pos);
	return true;
    }

    level++;
    nodeNum = (iter->positions[level].keyPos < 0) ? childNodeNumU(pos) : (iter->positions[level].keyPos >> 8);

    while (level < cutoff_level_) {
	prevItemU(nodeNum, (uint8_t)255, cc);
	pos = (nodeNum << 8) + cc;
	iter->positions[level].keyPos = pos;

	if (!isTbitSetU(nodeNum, cc)) {
	    iter->setVU_R(level, nodeNum, pos);
	    return true;
	}

	level++;
	nodeNum = (iter->positions[level].keyPos < 0) ? childNodeNumU(pos) : (iter->positions[level].keyPos >> 8);
    }

    if (iter->positions[level].keyPos < 0) {
	pos = childpos(nodeNum);
	pos += (nodeSize(pos) - 1);
    }
    else
	pos = iter->positions[level].keyPos;

    return nextRight(level, pos, iter);
}

inline bool FST::nextRight(int level, uint64_t pos, FSTIter* iter) {
    while (isTbitSet(pos)) {
	iter->positions[level].keyPos = pos;
	level++;

	if (iter->positions[level].keyPos < 0) {
	    pos = childpos(childNodeNum(pos) + childCountU_);
	    pos += (nodeSize(pos) - 1);
	}
	else
	    pos = iter->positions[level].keyPos;
    }

    iter->setKV_R(level, pos);
    return true;
}


//******************************************************
// NEXT NODE
//******************************************************
inline bool FST::nextNodeU(int level, uint64_t nodeNum, FSTIter* iter) {
    int cur_level = (level < cutoff_level_) ? level : (cutoff_level_ - 1);
    uint8_t cc = 0;
    uint8_t kc = 0;
    bool inNode = false;

    while (!inNode) {
	nodeNum++;
	if (isObitSetU(nodeNum)) {
	    iter->positions[cur_level].keyPos = nodeNum << 8;
	    iter->positions[cur_level].isO = true;
	}
	else {
	    nextItemU(nodeNum, 0, cc);
	    iter->positions[cur_level].keyPos = (nodeNum << 8) + cc;
	}

	if (cur_level == 0)
	    break;

	cur_level--;
	nodeNum = iter->positions[cur_level].keyPos >> 8;
	kc = iter->positions[cur_level].keyPos & 255;

	uint8_t next_kc = (kc == 0 && iter->positions[cur_level].isO) ? kc : (kc + 1);
	iter->positions[cur_level].isO = false;
	inNode = (kc == 255) ? false : nextItemU(nodeNum, next_kc, cc);
    }
    iter->positions[cur_level].keyPos = (nodeNum << 8) + cc;

    while (cur_level < level && cur_level < cutoff_level_) {
	uint64_t pos = iter->positions[cur_level].keyPos;
	nodeNum = pos >> 8;
	cc = pos & 255;
	if (!isTbitSetU(nodeNum, cc)) {
	    iter->setVU(cur_level, nodeNum, pos);
	    return true;
	}
	cur_level++;
    }

    if (level < cutoff_level_)
	return nextLeftU(level, iter->positions[cur_level].keyPos, iter);
    else
	return false;
}

inline bool FST::nextNode(int level, uint64_t pos, FSTIter* iter) {
    bool inNode = false;
    int cur_level = level - 1;
    while (!inNode && cur_level >= cutoff_level_) {
	iter->positions[cur_level].keyPos++;
	pos = iter->positions[cur_level].keyPos;
	inNode = !isSbitSet(pos);
	cur_level--;
    }

    if (!inNode && cur_level < cutoff_level_) {
	uint64_t nodeNum = iter->positions[cur_level].keyPos >> 8;
	uint8_t kc = iter->positions[cur_level].keyPos & 255;
	uint8_t cc = 0;
	uint8_t next_kc = (kc == 0 && iter->positions[cur_level].isO) ? kc : (kc + 1);
	iter->positions[cur_level].isO = false;

	inNode = (kc == 255) ? false : nextItemU(nodeNum, next_kc, cc);

	if (!inNode) {
	    if (nextNodeU(level, (iter->positions[cur_level].keyPos >> 8), iter))
		return true;
	}
	else {
	    iter->positions[cur_level].keyPos = (nodeNum << 8) + cc;
	    return nextLeftU(cur_level, iter->positions[cur_level].keyPos, iter);
	}
    }

    cur_level++;
    while (cur_level < level) {
	uint64_t pos = iter->positions[cur_level].keyPos;
	if (!isTbitSet(pos)) {
	    iter->setV(cur_level, pos);
	    return true;
	}
	cur_level++;
    }

    return nextLeft(level, iter->positions[cur_level].keyPos, iter);
}

//******************************************************
// PREV NODE
//******************************************************
inline bool FST::prevNodeU(int level, uint64_t nodeNum, FSTIter* iter) {
    int cur_level = (level < cutoff_level_) ? level : (cutoff_level_ - 1);
    uint8_t cc = 0;
    uint8_t kc = 0;
    bool inNode = false;

    while (!inNode) {
	nodeNum--;
	prevItemU(nodeNum, (uint8_t)255, cc);
	iter->positions[cur_level].keyPos = (nodeNum << 8) + cc;

	if (cur_level == 0)
	    break;

	cur_level--;
	nodeNum = iter->positions[cur_level].keyPos >> 8;
	kc = iter->positions[cur_level].keyPos & 255;

	if (kc == 0) {
	    if (!iter->positions[cur_level].isO && isObitSetU(nodeNum)) {
		iter->positions[cur_level].isO = true;
		inNode = true;
	    }
	    else
		inNode = false;
	}
	else {
	    inNode = prevItemU(nodeNum, (kc-1), cc);
	    if (!inNode && isObitSetU(nodeNum)) {
		cc = 0;
		iter->positions[cur_level].isO = true;
		inNode = true;
	    }
	}
    }
    iter->positions[cur_level].keyPos = (nodeNum << 8) + cc;

    while (cur_level < level && cur_level < cutoff_level_) {
	uint64_t pos = iter->positions[cur_level].keyPos;
	nodeNum = pos >> 8;
	cc = pos & 255;

	if (!isTbitSetU(nodeNum, cc)) {
	    iter->setVU_R(cur_level, nodeNum, pos);
	    return true;
	}
	cur_level++;
    }

    if (level < cutoff_level_)
	return nextRightU(level, iter->positions[cur_level].keyPos, iter);
    else
	return false;
}

inline bool FST::prevNode(int level, uint64_t pos, FSTIter* iter) {
    bool inNode = false;
    int cur_level = level - 1;

    while (!inNode && cur_level >= cutoff_level_) {
	pos = iter->positions[cur_level].keyPos;
	inNode = !isSbitSet(pos);
	iter->positions[cur_level].keyPos--;
	pos = iter->positions[cur_level].keyPos;
	cur_level--;
    }

    if (!inNode && cur_level < cutoff_level_) {
	uint64_t nodeNum = iter->positions[cur_level].keyPos >> 8;
	uint8_t kc = iter->positions[cur_level].keyPos & 255;
	uint8_t cc = 0;

	if (kc == 0) {
	    if (!iter->positions[cur_level].isO && isObitSetU(nodeNum)) {
		iter->positions[cur_level].isO = true;
		inNode = true;
	    }
	    else
		inNode = false;
	}
	else {
	    inNode = prevItemU(nodeNum, (kc-1), cc);
	    if (!inNode && isObitSetU(nodeNum)) {
		cc = 0;
		iter->positions[cur_level].isO = true;
		inNode = true;
	    }
	}

	if (!inNode) {
	    if (prevNodeU(level, (iter->positions[cur_level].keyPos >> 8), iter))
		return true;
	}
	else {
	    iter->positions[cur_level].keyPos = (nodeNum << 8) + cc;
	    return nextRightU(cur_level, iter->positions[cur_level].keyPos, iter);
	}
    }

    cur_level++;
    while (cur_level < level) {
	uint64_t pos = iter->positions[cur_level].keyPos;
	if (!isTbitSet(pos)) {
	    iter->setV_R(cur_level, pos);
	    return true;
	}
	cur_level++;
    }

    return nextRight(level, iter->positions[cur_level].keyPos, iter);
}

//******************************************************
// LOWER BOUND
//******************************************************
bool FST::lowerBound(const uint8_t* key, const int keylen, FSTIter &iter) {
    iter.clear();
    int keypos = 0;
    uint64_t nodeNum = 0;
    uint8_t kc = (uint8_t)key[keypos];
    uint8_t cc = 0;
    uint64_t pos = kc;

    while (keypos < keylen && keypos < cutoff_level_) {
	kc = (uint8_t)key[keypos];
	pos = (nodeNum << 8) + kc;

	__builtin_prefetch(tUbits_() + (nodeNum << 2) + (kc >> 6), 0);
	__builtin_prefetch(tUrankLUT_() + ((pos + 1) >> 6), 0);

	if (!nextItemU(nodeNum, kc, cc)) { // next char is in next node
	    nextNodeU(keypos, nodeNum, &iter);
	    return nextLeftU(keypos, iter.positions[keypos].keyPos, &iter);
	}

	if (cc != kc) {
	    iter.positions[keypos].keyPos = (nodeNum << 8) + cc;
	    return nextLeftU(keypos, iter.positions[keypos].keyPos, &iter);
	}

	iter.positions[keypos].keyPos = pos;

	if (!isTbitSetU(nodeNum, kc)) { // found key terminiation (value)
	    iter.len = keypos + 1;
	    iter.positions[keypos].valPos = valuePosU(nodeNum, pos);
	    return true;
	}

	nodeNum = childNodeNumU(pos);
	keypos++;
    }

    if (keypos < cutoff_level_) {
	pos = nodeNum << 8;
	if (isObitSetU(nodeNum)) {
	    iter.setKVU(keypos, nodeNum, pos, true);
	    return true;
	}
	keypos--;
	return nextLeftU(keypos, iter.positions[keypos].keyPos, &iter);
    }

    //----------------------------------------------------------
    pos = (cutoff_level_ == 0) ? 0 : childpos(nodeNum);

    bool inNode = true;
    while (keypos < keylen) {
	kc = (uint8_t)key[keypos];

	int nsize = nodeSize(pos);

	inNode = nodeSearch_lowerBound(pos, nsize, kc);

	iter.positions[keypos].keyPos = pos;

	if (!inNode) {
	    return nextNode(keypos, pos, &iter);
	    //return nextLeft(keypos, pos, &iter);
	}

	cc = cbytes_()[pos];
	if (cc != kc)
	    return nextLeft(keypos, pos, &iter);

	if (!isTbitSet(pos)) {
	    iter.len = keypos + 1;
	    iter.positions[keypos].valPos = valuePos(pos);
	    return true;
	}

	pos = childpos(childNodeNum(pos) + childCountU_);
	keypos++;

	__builtin_prefetch(cbytes_() + pos, 0, 1);
	__builtin_prefetch(tbits_() + (pos >> 6), 0, 1);
	__builtin_prefetch(trankLUT_() + ((pos + 1) >> 9), 0);
    }

    if (cbytes_()[pos] == TERM && !isTbitSet(pos)) {
	iter.positions[keypos].keyPos = pos;
	iter.len = keypos + 1;
	iter.positions[keypos].valPos = valuePos(pos);
	return true;
    }
    keypos--;
    return nextLeft(keypos, iter.positions[keypos].keyPos, &iter);
}

bool FST::lowerBound(const uint64_t key, FSTIter &iter) {
    uint8_t key_str[8];
    reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(key);
    return lowerBound(key_str, 8, iter);
}

//******************************************************
// UPPER BOUND
//******************************************************
bool FST::upperBound(const uint8_t* key, const int keylen, FSTIter &iter) {
    iter.clear();
    int keypos = 0;
    uint64_t nodeNum = 0;
    uint8_t kc = (uint8_t)key[keypos];
    uint8_t cc = 0;
    uint64_t pos = kc;

    while (keypos < keylen && keypos < cutoff_level_) {
	kc = (uint8_t)key[keypos];
	pos = (nodeNum << 8) + kc;

	__builtin_prefetch(tUbits_() + (nodeNum << 2) + (kc >> 6), 0);
	__builtin_prefetch(tUrankLUT_() + ((pos + 1) >> 6), 0);

	if (!prevItemU(nodeNum, kc, cc)) { // previous char is in previous node
	    if (isObitSetU(nodeNum)) {
		iter.setKVU(keypos, nodeNum, pos, true);
		return true;
	    }
	    prevNodeU(keypos, nodeNum, &iter);
	    return nextRightU(keypos, iter.positions[keypos].keyPos, &iter);
	}

	if (cc != kc) {
	    iter.positions[keypos].keyPos = (nodeNum << 8) + cc;
	    return nextRightU(keypos, iter.positions[keypos].keyPos, &iter);
	}

	iter.positions[keypos].keyPos = pos;

	if (!isTbitSetU(nodeNum, kc)) { // found key terminiation (value)
	    iter.len = keypos + 1;
	    iter.positions[keypos].valPos = valuePosU(nodeNum, pos);
	    return true;
	}

	nodeNum = childNodeNumU(pos);
	keypos++;
    }

    if (keypos < cutoff_level_) {
	pos = nodeNum << 8;
	if (isObitSetU(nodeNum)) {
	    iter.setKVU(keypos, nodeNum, pos, true);
	    return true;
	}
	keypos--;
	return nextRightU(keypos, iter.positions[keypos].keyPos, &iter);
    }

    //----------------------------------------------------------
    pos = (cutoff_level_ == 0) ? 0 : childpos(nodeNum);

    bool inNode = true;
    while (keypos < keylen) {
	kc = (uint8_t)key[keypos];

	int nsize = nodeSize(pos);
	inNode = nodeSearch_upperBound(pos, nsize, kc);

	iter.positions[keypos].keyPos = pos;

	if (!inNode) {
	    return prevNode(keypos, pos, &iter);
	    //return nextRight(keypos, pos, &iter);
	}

	cc = cbytes_()[pos];
	if (cc != kc)
	    return nextRight(keypos, pos, &iter);

	if (!isTbitSet(pos)) {
	    iter.len = keypos + 1;
	    iter.positions[keypos].valPos = valuePos(pos);
	    return true;
	}

	pos = childpos(childNodeNum(pos) + childCountU_);
	keypos++;

	__builtin_prefetch(cbytes_() + pos, 0, 1);
	__builtin_prefetch(tbits_() + (pos >> 6), 0, 1);
	__builtin_prefetch(trankLUT_() + ((pos + 1) >> 9), 0);
    }

    if (cbytes_()[pos] == TERM && !isTbitSet(pos)) {
	iter.positions[keypos].keyPos = pos;
	iter.len = keypos + 1;
	iter.positions[keypos].valPos = valuePos(pos);
	return true;
    }
    keypos--;
    return nextRight(keypos, iter.positions[keypos].keyPos, &iter);
}

bool FST::upperBound(const uint64_t key, FSTIter &iter) {
    uint8_t key_str[8];
    reinterpret_cast<uint64_t*>(key_str)[0]=__builtin_bswap64(key);
    return upperBound(key_str, 8, iter);
}


//******************************************************
// PRINT
//******************************************************
void FST::printU() {
    cout << "\n======================================================\n\n";
    for (int i = 0; i < cUnbits_; i += 4) {
	for (int j = 0; j < 256; j++) {
	    if (isLabelExist(cUbits_() + i, (uint8_t)j))
		cout << (char)j;
	}
	cout << " || ";
    }

    cout << "\n======================================================\n\n";
    for (int i = 0; i < cUnbits_; i += 4) {
	for (int j = 0; j < 256; j++) {
	    if (isLabelExist(tUbits_() + i, (uint8_t)j))
		cout << (char)j;
	    //cout << j << " ";
	}
	cout << " || ";
    }

    cout << "\n======================================================\n\n";
    for (int i = 0; i < oUnbits_; i++) {
	if (readBit(oUbits_()[i/64], i % 64))
	    cout << "1";
	else
	    cout << "0";
    }

    cout << "\n======================================================\n\n";
    for (int i = 0; i < valUmem_/8; i++) {
	cout << valuesU_()[i] << " ";
    }
    cout << "\n";
}

void FST::print() {
    int c_pos = 0;
    int v_pos = 0;
    int s_pos = 0;
    int value_pos = 0;

    cout << "\n======================================================\n\n";

    for (int i = 0; i < cmem_; i++)
	cout << "(" << i << ")" << cbytes_()[i] << " ";

    cout << "\n======================================================\n\n";
    for (int i = 0; i < cmem_; i++) {
	if (readBit(tbits_()[i/64], i % 64))
	    cout << "(" << i << ")" << "1 ";
	else
	    cout << "(" << i << ")" << "0 ";
    }

    cout << "\n======================================================\n\n";
    for (int i = 0; i < cmem_; i++) {
	if (readBit(sbits_()[i/64], i % 64))
	    cout << "(" << i << ")" << "1 ";
	else
	    cout << "(" << i << ")" << "0 ";
    }

    cout << "\n======================================================\n\n";
    for (int i = 0; i < valmem_/8; i++) {
	cout << "(" << i << ")" << values_()[i] << " ";
    }
    cout << "\n";
}


//******************************************************
// ITERATOR
//******************************************************
FSTIter::FSTIter() : index(NULL), len(0), isBegin(false), isEnd(false), cBoundU(0), cBound(0), cutoff_level(0), tree_height(0), first_value_pos(0), last_value_pos(0) { }

FSTIter::FSTIter(FST* idx) {
    index = idx;
    tree_height = index->tree_height_;
    cutoff_level = index->cutoff_level_;
    cBoundU = (index->cUnbits_ << 6) - 1;
    cBound = index->cMem() - 1;
    first_value_pos = index->first_value_pos_;
    last_value_pos = index->last_value_pos_;

    len = 0;
    isBegin = false;
    isEnd = false;

    for (int i = 0; i < tree_height; i++) {
	Cursor c;
	c.keyPos = -1;
	c.valPos = -1;
	c.isO = false;
	positions.push_back(c);
    }
}

void FSTIter::clear() {
    for (int i = 0; i < tree_height; i++) {
	positions[i].keyPos = -1;
	positions[i].valPos = -1;
	positions[i].isO = false;
    }

    len = 0;
    isBegin = false;
    isEnd = false;
}

inline void FSTIter::setVU(int level, uint64_t nodeNum, uint64_t pos) {
    len = level + 1;
    if (positions[level].valPos < 0)
	positions[level].valPos = index->valuePosU(nodeNum, pos);
    else
	positions[level].valPos++;
}

inline void FSTIter::setVU_R(int level, uint64_t nodeNum, uint64_t pos) {
    len = level + 1;
    if (positions[level].valPos <= 0)
	positions[level].valPos = index->valuePosU(nodeNum, pos);
    else
	positions[level].valPos--;
}

inline void FSTIter::setKVU(int level, uint64_t nodeNum, uint64_t pos, bool o) {
    positions[level].keyPos = pos;
    positions[level].isO = o;
    len = level + 1;
    if (positions[level].valPos < 0)
	positions[level].valPos = index->valuePosU(nodeNum, pos);
    else
	positions[level].valPos++;
}

inline void FSTIter::setKVU_R(int level, uint64_t nodeNum, uint64_t pos, bool o) {
    positions[level].keyPos = pos;
    positions[level].isO = o;
    len = level + 1;
    if (positions[level].valPos <= 0)
	positions[level].valPos = index->valuePosU(nodeNum, pos);
    else
	positions[level].valPos--;
}

inline void FSTIter::setV(int level, uint64_t pos) {
    len = level + 1;
    if (positions[level].valPos < 0)
	positions[level].valPos = index->valuePos(pos);
    else
	positions[level].valPos++;
}

inline void FSTIter::setV_R(int level, uint64_t pos) {
    len = level + 1;
    if (positions[level].valPos <= 0)
	positions[level].valPos = index->valuePos(pos);
    else
	positions[level].valPos--;
}

inline void FSTIter::setKV(int level, uint64_t pos) {
    positions[level].keyPos = pos;
    len = level + 1;
    if (positions[level].valPos < 0)
	positions[level].valPos = index->valuePos(pos);
    else
	positions[level].valPos++;
}

inline void FSTIter::setKV_R(int level, uint64_t pos) {
    positions[level].keyPos = pos;
    len = level + 1;
    if (positions[level].valPos <= 0)
	positions[level].valPos = index->valuePos(pos);
    else
	positions[level].valPos--;
}

string FSTIter::key () {
    string retStr;
    for (int i = 0; i < len; i++) {
	if (i < cutoff_level)
	    retStr += (char)(positions[i].keyPos & 255);
	else {
	    int32_t keypos = positions[i].keyPos;
	    char c = index->cbytes_()[keypos];
	    if (c != TERM || (c == TERM && !readBit(index->sbits_()[keypos >> 6], keypos & (uint64_t)63)))
		retStr += c;
	}
    }
    return retStr;
}

uint64_t FSTIter::value () {
    if (len <= cutoff_level) {
	__builtin_prefetch(index->valuesU_() + positions[len-1].valPos + 1, 0, 1);
	return index->valuesU_()[positions[len-1].valPos];
    }
    else {
	__builtin_prefetch(index->values_() + positions[len-1].valPos + 1, 0, 1);
	return index->values_()[positions[len-1].valPos];
    }
}

bool FSTIter::operator ++ (int) {
    if (unlikely(isEnd))
	return false;

    if (unlikely(positions[len-1].valPos == (0 - last_value_pos) || positions[len-1].valPos == last_value_pos))
	if ((last_value_pos < 0 && len <= cutoff_level) || (last_value_pos > 0 && len > cutoff_level)) {
	    isEnd = true;
	    return false;
	}

    uint64_t nodeNum = 0;
    uint8_t kc = 0;
    uint8_t cc = 0;
    bool inNode = true;
    int level = len - 1;
    while (level >= 0) {
	if (level < cutoff_level) {
	    if (unlikely(positions[level].keyPos >= cBoundU)) {
		level--;
		continue;
	    }

	    nodeNum = positions[level].keyPos >> 8;
	    kc = positions[level].keyPos & 255;
	    uint8_t next_kc = (kc == 0 && positions[level].isO) ? kc : (kc + 1);
	    positions[level].isO = false;

	    inNode = (kc == 255) ? false : index->nextItemU(nodeNum, next_kc, cc);

	    if (!inNode)
		return index->nextNodeU(level, nodeNum, this);

	    positions[level].keyPos = (nodeNum << 8) + cc;
	    return index->nextLeftU(level, positions[level].keyPos, this);
	}
	else {
	    if (unlikely(positions[level].keyPos >= cBound)) {
		level--;
		continue;
	    }

	    positions[level].keyPos++;

	    if (index->isSbitSet(positions[level].keyPos))
		return index->nextNode(level, positions[level].keyPos, this);

	    return index->nextLeft(level, positions[level].keyPos, this);
	}
    }

    return false;
}

bool FSTIter::operator -- (int) {
    if (unlikely(isBegin))
	return false;

    if (unlikely(positions[len-1].valPos == 0))
	if ((first_value_pos < 0 && len <= cutoff_level) || (first_value_pos > 0 && len > cutoff_level)) {
	    isBegin = true;
	    return false;
	}

    uint64_t nodeNum = 0;
    uint8_t kc = 0;
    uint8_t cc = 0;
    bool inNode = true;
    int level = len - 1;
    while (level >= 0) {
	if (level < cutoff_level) {
	    if (unlikely(positions[level].keyPos < 0)) {
		level--;
		continue;
	    }

	    nodeNum = positions[level].keyPos >> 8;
	    kc = positions[level].keyPos & 255;

	    if (kc == 0) {
		if (!positions[level].isO && index->isObitSetU(nodeNum)) {
		    positions[level].isO = true;
		    inNode = true;
		}
		else
		    inNode = false;
	    }
	    else {
		inNode = index->prevItemU(nodeNum, (kc-1), cc);
		if (!inNode && index->isObitSetU(nodeNum)) {
		    cc = 0;
		    positions[level].isO = true;
		    inNode = true;
		}
	    }

	    if (!inNode)
		return index->prevNodeU(level, nodeNum, this);

	    positions[level].keyPos = (nodeNum << 8) + cc;
	    return index->nextRightU(level, positions[level].keyPos, this);
	}
	else {
	    if (unlikely(positions[level].keyPos <= 0)) {
		level--;
		continue;
	    }

	    if (index->isSbitSet(positions[level].keyPos)) {
		positions[level].keyPos--;
		return index->prevNode(level, positions[level].keyPos, this);
	    }

	    positions[level].keyPos--;

	    return index->nextRight(level, positions[level].keyPos, this);
	}
    }

    return false;
}

