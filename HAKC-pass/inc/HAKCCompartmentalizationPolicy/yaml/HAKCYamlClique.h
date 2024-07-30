//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLCLIQUE_H
#define HAKC_HAKCYAMLCLIQUE_H

#include "HAKC-defs.h"

#include "llvm/Support/YAMLTraits.h"

namespace hakc {

    class HAKCYamlClique {
    public:
        HAKCYamlClique() = default;

        hakc_access_token_t AccessToken;
        sym_color_t Color;
    };

} // hakc

#endif //HAKC_HAKCYAMLCLIQUE_H
