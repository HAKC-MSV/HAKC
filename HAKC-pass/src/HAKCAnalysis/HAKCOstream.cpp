//
// Created by de29664 on 10/28/24.
//

#include "HAKCAnalysis/HAKCOstream.h"

namespace hakc {
    HAKCOstream::HAKCOstream() : os(errs()) {

    }

    raw_ostream &HAKCOstream::GetOS() {
        return os;
    }
} // hakc
