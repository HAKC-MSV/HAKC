//
// Created by de29664 on 8/6/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCCompartment.h"

namespace hakc {
    HAKCCompartment::HAKCCompartment(hakc_compartment_id_t Compartment, hakc_access_token_t AccessToken,
                                     class LLVMContext &Context) :
            Compartment(ConstantInt::get(IntegerType::get(Context, 64), Compartment)),
            AccessToken(ConstantInt::get(IntegerType::get(Context, 64), AccessToken)),
            Targets() {

    }

    HAKC_Compartment_ID HAKCCompartment::GetCompartmentID() {
        return Compartment;
    }

    HAKC_Access_Token HAKCCompartment::GetAccessToken() {
        return AccessToken;
    }

    bool HAKCCompartment::IsKernelCompartment() {
        return GetCompartmentIDValue() == KERNEL_COMPARTMENT;
    }

    std::set<HAKC_Compartment_ID> HAKCCompartment::GetValidTargets() {
        return Targets;
    }

    void HAKCCompartment::AddTarget(HAKC_Compartment_ID CompartmentID) {
        Targets.insert(CompartmentID);
    }

    hakc_compartment_id_t HAKCCompartment::GetCompartmentIDValue() {
        return Compartment->getSExtValue();
    }
} // hakc
