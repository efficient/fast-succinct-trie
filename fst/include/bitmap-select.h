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

#ifndef _BITMAPSELECT_H_
#define _BITMAPSELECT_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "shared.h"

class BitmapSelect {
public:
    const int kWordSize = 64;
    const int kWordBits = 6;
    const uint32 kWordMask = ((uint32)1 << kWordBits) - 1;

    const int skip = 64;
    const int kSkipBits = 6;
    const uint32 kSkipMask = ((uint32)1 << kSkipBits) - 1;

    BitmapSelect() { }
    virtual uint32 select(uint32 rank) = 0;
};

class BitmapSelectPoppy: public BitmapSelect {
public:
    BitmapSelectPoppy(uint64* bits, uint32 nbits);
    ~BitmapSelectPoppy() {}
    
    uint32 select(uint32 rank);

    uint64* getBits();
    uint32 getNbits();
    uint32 getMem();

    friend class FST;
    friend class FSTIter;

private:
    uint64* bits_;
    uint32  nbits_;
    uint32  mem_;

    uint32  wordCount_;
    uint32  pCount_;
    uint32* selectLUT_;
    uint32  selectLUTCount_;
};

#endif /* _BITMAPSELECT_H_ */
