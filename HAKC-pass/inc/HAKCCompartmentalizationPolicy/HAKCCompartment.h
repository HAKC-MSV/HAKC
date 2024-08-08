//
// Created by de29664 on 8/6/24.
//

#ifndef HAKC_HAKCCOMPARTMENT_H
#define HAKC_HAKCCOMPARTMENT_H

#include "HAKC-defs.h"

namespace hakc {

    class HAKCCompartmentDivision;

    class HAKCCompartment {
    public:
        HAKCCompartment(hakc_compartment_id_t Compartment, hakc_access_token_t EntryToken, class LLVMContext &Context);

        HAKCCompartment();

        HAKCCompartment(const HAKCCompartment &C) = default;

        HAKC_Compartment_ID GetCompartmentID() const;

        HAKC_Access_Token GetEntryToken() const;

        std::vector<HAKC_Compartment_ID> GetValidTargets() const;

        std::vector<HAKCCompartmentDivision> GetDivisions() const;

        void AddTarget(HAKC_Compartment_ID CompartmentID);

        void AddDivision(HAKCCompartmentDivision &HAKCDivision);

        hakc_compartment_id_t GetCompartmentIDValue() const;

        bool IsKernelCompartment() const;

        friend bool operator==(const HAKCCompartment &lhs, const HAKCCompartment &rhs) {
            return lhs.Compartment == rhs.Compartment;
        }

        friend bool operator!=(const HAKCCompartment &lhs, const HAKCCompartment &rhs) {
            return !(lhs == rhs);
        }

    protected:
        HAKC_Compartment_ID Compartment;
        HAKC_Access_Token EntryToken;
        std::vector<HAKC_Compartment_ID> Targets;
        std::vector<HAKCCompartmentDivision> Divisions;
    };

} // hakc

#endif //HAKC_HAKCCOMPARTMENT_H
