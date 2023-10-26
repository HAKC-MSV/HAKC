//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCHASH_H
#define HAKC_HAKCHASH_H

#include "llvm/Support/MD5.h"

using namespace llvm;

namespace hakc {

    class HAKCHash {
    public:
        HAKCHash();

        bool isFinalized();

        void update(StringRef Str);

        void final();

        std::string digest();


    protected:
        MD5::MD5Result result;
        MD5 hasher;
        bool updated, finalized;
    };

} // hakc

#endif //HAKC_HAKCHASH_H
