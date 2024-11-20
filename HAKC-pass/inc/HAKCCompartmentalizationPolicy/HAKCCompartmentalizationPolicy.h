//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H

#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlCompartmentalizationPolicy.h"
#include "HAKCCompartment.h"
#include "HAKCCompartmentDivision.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

#include "kuzu.hpp"

namespace hakc {

    class HAKCModuleAnalysis;

    typedef std::shared_ptr<HAKCCompartment> HAKCCompartmentP;
    typedef std::shared_ptr<HAKCCompartmentDivision> HAKCDivisionP;
    typedef std::unique_ptr<kuzu::main::PreparedStatement> HAKCPreparedStatementP;
    typedef kuzu::common::Value HAKCDBValue;
    typedef std::unique_ptr<HAKCDBValue> HAKCDBValueP;

    class HAKCCompartmentalizationPolicy {
    public:
        explicit HAKCCompartmentalizationPolicy(bool Debug, LLVMContext &Ctx,
                                                hakc_compartment_id_t DefaultCompartmentID,
                                                hakc_compartment_division_t DefaultDivisionID, StringRef DatabasePath);

        ~HAKCCompartmentalizationPolicy();

        hakc::HAKCCompartmentDivision &GetDivision(GlobalValue *GV);

        HAKCCompartmentP GetCompartment(hakc_compartment_id_t CompartmentID);

    protected:
        std::shared_ptr<kuzu::main::Database> Database;
        std::unique_ptr<kuzu::main::Connection> Conn;
        bool Debug;
        HAKCDivisionP DefaultDivision;
        LLVMContext &LLVMCtx;
        std::vector<HAKCCompartmentP> Compartments;
        std::vector<HAKCDivisionP> Divisions;

        void CheckConnection();

        void Reset();

        HAKCPreparedStatementP CreatePreparedStatement(StringRef query);

        std::unique_ptr<kuzu::main::QueryResult>
        Execute(HAKCPreparedStatementP &, std::unordered_map<std::string, HAKCDBValueP> &Arguments);

        HAKCDivisionP GetDivision(hakc_compartment_id_t CompartmentID, hakc_compartment_division_t DivisionID);

        void ConnectToDatabase(StringRef DatabasePath);

        void
        SetDefaultDivision(hakc_compartment_id_t DefaultCompartmentID, hakc_compartment_division_t DefaultDivisionID);

        HAKCDivisionP FindCachedDivision(hakc_compartment_id_t CompartmentID, hakc_compartment_division_t DivisionID);

        HAKCCompartmentP FindCachedCompartment(hakc_compartment_id_t CompartmentID);

    };

} // hakc

#endif //HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
