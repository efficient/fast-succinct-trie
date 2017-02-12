/**
Copyright 2013 Carnegie Mellon University

Authors: Dong Zhou, David G. Andersen and Michale Kaminsky

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Modified by Huanchen Zhang
*/

#include <assert.h>
#include <stdlib.h>
#include <algorithm>

#include "bitmap-select.h"
#include "popcount.h"
#include "shared.h"

#include <iostream>

BitmapSelectPoppy::BitmapSelectPoppy(uint64 *bits, uint32 nbits)
{
    bits_ = bits;
    nbits_ = nbits;

    uint32 wordCount_ = nbits_ / kWordSize;

    uint32* rankLUT;
    assert(posix_memalign((void **) &rankLUT, kCacheLineSize, (wordCount_ + 1) * sizeof(uint32)) >= 0);

    uint32 rankCum = 0;
    for (uint32 i = 0; i < wordCount_; i++) {
	rankLUT[i] = rankCum;
	rankCum += popcount(bits_[i]);
    }
    rankLUT[wordCount_] = rankCum;
    pCount_ = rankCum;

    selectLUTCount_ = pCount_ / skip + 1;
    assert(posix_memalign((void **) &selectLUT_, kCacheLineSize, (selectLUTCount_ + 1) * sizeof(uint32)) >= 0);

    selectLUT_[0] = 0;
    uint32 idx = 1;
    for (uint32 i = 1; i <= wordCount_; i++) {
	while (idx * skip <= rankLUT[i]) {
	    int rankR = idx * skip - rankLUT[i-1];
	    selectLUT_[idx] = (i - 1) * kWordSize + select64_popcount_search(bits_[i-1], rankR) + 1;
	    idx++;
	}
    }

    free(rankLUT);

    mem_ = nbits_ / 8 + (selectLUTCount_ + 1) * sizeof(uint32);
}

uint32 BitmapSelectPoppy::select(uint32 rank) {
    assert(rank <= pCount_);

    uint32 s = selectLUT_[rank >> kSkipBits];
    uint32 rankR = rank & kSkipMask;

    if (rankR == 0)
	return s - 1;

    int idx = s >> kWordBits;
    int startWordBit = s & kWordMask;
    uint64 word = bits_[idx] << startWordBit >> startWordBit;

    int pop = 0;
    while ((pop = popcount(word)) < rankR) {
	idx++;
	word = bits_[idx];
	rankR -= pop;
    }

    return (idx << kWordBits) + select64_popcount_search(word, rankR);
}

uint64* BitmapSelectPoppy::getBits() {
    return bits_;
}

uint32 BitmapSelectPoppy::getNbits() {
    return nbits_;
}

uint32 BitmapSelectPoppy::getMem() {
    return mem_;
}
