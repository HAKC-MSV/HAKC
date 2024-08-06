//
// Created by de29664 on 8/6/24.
//

#ifndef HAKC_HAKCCOMPARTMENT_H
#define HAKC_HAKCCOMPARTMENT_H

#include "HAKC-defs.h"

namespace hakc {

    class HAKCCompartment {
    public:
        HAKCCompartment(hakc_compartment_id_t Compartment, hakc_access_token_t AccessToken, class LLVMContext &Context);

        HAKC_Compartment_ID GetCompartmentID();
        HAKC_Access_Token GetAccessToken();

        bool IsKernelCompartment();

        friend bool operator==(const HAKCCompartment &lhs, const HAKCCompartment &rhs) {
            return lhs.Compartment == rhs.Compartment;
        }

        friend bool operator!=(const HAKCCompartment &lhs, const HAKCCompartment &rhs) {
            return !(lhs == rhs);
        }
    protected:
        HAKC_Compartment_ID Compartment;
        HAKC_Access_Token AccessToken;
    };

} // hakc

#endif //HAKC_HAKCCOMPARTMENT_H
