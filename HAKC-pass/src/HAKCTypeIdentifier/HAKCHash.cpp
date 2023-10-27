//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCHash.h"

#include "HAKCAnalysis/CommonHAKCAnalysis.h"

#include "llvm/ADT/SmallString.h"

namespace hakc {
    hakc::HAKCHash::HAKCHash()
            : result(), hasher(), updated(false), finalized(false) {}

    bool hakc::HAKCHash::isFinalized() { return finalized; }

    void hakc::HAKCHash::update(StringRef Str) {
        updated = true;
        hasher.update(Str);
    }

    void hakc::HAKCHash::final() {
        if (!updated) {
            CommonHAKCAnalysis::getWriter() << "Hash has not been given data!\n";
            throw std::exception();
        }
        finalized = true;
        hasher.final(result);
    }

    std::string hakc::HAKCHash::digest() {
        if (!finalized) {
            CommonHAKCAnalysis::getWriter() << "Hash has not been finalized!\n";
            throw std::exception();
        }
        return result.digest().str().str();
    }

} // hakc