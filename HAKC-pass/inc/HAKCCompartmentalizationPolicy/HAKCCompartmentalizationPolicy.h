//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H

#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlCompartmentalizationPolicy.h"
#include "HAKCCompartment.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

#include "kuzu.hpp"

namespace hakc {

    class HAKCModuleAnalysis;

    class HAKCTypeIdentifier;

    typedef std::shared_ptr<HAKCCompartment> HAKCCompartmentP;
    typedef std::shared_ptr<HAKCCompartmentDivision> HAKCDivisionP;
    typedef std::unique_ptr<kuzu::main::PreparedStatement> HAKCPreparedStatementP;
    typedef kuzu::common::Value HAKCDBValue;
    typedef std::unique_ptr<HAKCDBValue> HAKCDBValueP;

    class HAKCCompartmentalizationPolicy {
    public:
        explicit HAKCCompartmentalizationPolicy(bool Debug);
        ~HAKCCompartmentalizationPolicy();

        void ConnectToDatabase(StringRef DatabasePath);

        HAKCCompartmentP GetCompartment(GlobalValue *GV);

        HAKCDivisionP GetDivision(GlobalValue *GV);

        HAKCDivisionP
        GetDivision(hakc_compartment_id_t CompartmentID, hakc_compartment_division_t DivisionID);

        HAKC_Division_ID GetDivisionID(GlobalValue *GV);

        HAKCCompartmentP GetCompartment(hakc_compartment_id_t ID);

    protected:
        std::shared_ptr<kuzu::main::Database> Database;
        std::unique_ptr<kuzu::main::Connection> Conn;
        bool Debug;

        void CheckConnection();
        void Reset();

        HAKCPreparedStatementP CreatePreparedStatement(StringRef query);
        std::unique_ptr<kuzu::main::QueryResult> Execute(HAKCPreparedStatementP&, std::unordered_map<std::string, HAKCDBValueP> &Arguments);

    };

} // hakc

#endif //HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
