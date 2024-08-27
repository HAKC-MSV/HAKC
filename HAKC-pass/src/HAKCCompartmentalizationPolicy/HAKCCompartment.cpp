//
// Created by de29664 on 8/6/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCCompartment.h"

#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

namespace hakc {
    HAKCCompartment::HAKCCompartment(hakc_compartment_id_t Compartment, hakc_access_token_t EntryToken,
                                     class LLVMContext &Context) :
            Compartment(ConstantInt::get(IntegerType::get(Context, COMPARTMENT_ID_BIT_LENGTH), Compartment)),
            EntryToken(ConstantInt::get(IntegerType::get(Context, 64), EntryToken)),
            Targets(), Divisions() {

    }

    HAKCCompartment::HAKCCompartment() : Compartment(nullptr), EntryToken(nullptr), Targets(), Divisions() {

    }

    HAKC_Compartment_ID HAKCCompartment::GetCompartmentID() const {
        return Compartment;
    }

    bool HAKCCompartment::IsKernelCompartment() const {
        return GetCompartmentIDValue() == KERNEL_COMPARTMENT;
    }

    std::vector<HAKC_Compartment_ID> HAKCCompartment::GetValidTargets() const {
        return Targets;
    }

    std::vector<HAKCCompartmentDivision> HAKCCompartment::GetDivisions() const {
        return Divisions;
    }

    void HAKCCompartment::AddTarget(HAKC_Compartment_ID CompartmentID) {
        Targets.push_back(CompartmentID);
    }

    hakc_compartment_id_t HAKCCompartment::GetCompartmentIDValue() const {
        return Compartment->getSExtValue();
    }

    void HAKCCompartment::AddDivision(HAKCCompartmentDivision &HAKCDivision) {
        Divisions.push_back(HAKCDivision);
    }

    HAKC_Access_Token HAKCCompartment::GetEntryToken() const {
        return EntryToken;
    }

    HAKC_Compartment_ID HAKCCompartment::CreateID(hakc_compartment_id_t ID, Module &M) {
        return ConstantInt::get(IntegerType::get(M.getContext(), COMPARTMENT_ID_BIT_LENGTH), ID);
    }
} // hakc
