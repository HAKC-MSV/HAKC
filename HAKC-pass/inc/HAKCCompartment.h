//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCCOMPARTMENT_H
#define HAKC_HAKCCOMPARTMENT_H

#include <set>

// #include "HAKCYaml.h"
#include "HAKC-defs.h"

namespace hakc {
    class HAKCCompartment {
    protected:
        hakc_compartment_id_t id;
        sym_color_t color;
        hakc_access_token_t entry_token;
        hakc_access_token_t access_token;
        std::set<std::shared_ptr<HAKCCompartment>> targets;

    public:
        HAKCCompartment(hakc_compartment_id_t id, hakc_access_token_t entry_token, hakc_access_token_t access_token,
                        sym_color_t color);

        void addTarget(std::shared_ptr<HAKCCompartment> &target);

        hakc_compartment_id_t getID();

        hakc_access_token_t getEntryToken();

        hakc_access_token_t getAccessToken();

        sym_color_t getColor();

        std::set<std::shared_ptr<HAKCCompartment>> getTargets();
    };

}// namespace hakc

#endif//HAKC_HAKCCOMPARTMENT_H
