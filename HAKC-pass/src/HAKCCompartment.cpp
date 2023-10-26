//
// Created by de29664 on 3/21/23.
//
#include "HAKCCompartment.h"

namespace hakc {
    HAKCCompartment::HAKCCompartment(hakc_compartment_id_t id, hakc_access_token_t entry_token,
                                     hakc_access_token_t access_token,
                                     sym_color_t color) :
            id(id),
            color(color),
            entry_token(entry_token),
            access_token(access_token),
            targets() {}


    void HAKCCompartment::addTarget(std::shared_ptr<HAKCCompartment> &target) {
        targets.insert(target);
    }

    hakc_compartment_id_t HAKCCompartment::getID() {
        return id;
    }

    hakc_access_token_t HAKCCompartment::getEntryToken() {
        return entry_token;
    }

    std::set<std::shared_ptr<HAKCCompartment>> HAKCCompartment::getTargets() {
        return targets;
    }

    sym_color_t HAKCCompartment::getColor() {
        return color;
    }

    hakc_access_token_t HAKCCompartment::getAccessToken() {
        return access_token;
    }
}