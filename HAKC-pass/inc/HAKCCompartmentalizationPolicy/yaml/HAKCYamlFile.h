//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLFILE_H
#define HAKC_HAKCYAMLFILE_H

#include "HAKCYamlSymbol.h"

namespace hakc {

    class HAKCYamlFile {
    public:
        HAKCYamlFile() = default;

        std::string Filename;
        std::vector<HAKCYamlSymbol> Symbols;
    };

} // hakc

#endif //HAKC_HAKCYAMLFILE_H
