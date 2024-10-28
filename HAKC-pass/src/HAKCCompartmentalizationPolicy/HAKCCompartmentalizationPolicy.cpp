//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"

#include "llvm/Support/FileSystem.h"

#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

namespace hakc {
    HAKCCompartmentalizationPolicy::HAKCCompartmentalizationPolicy(bool Debug)
            : Database(nullptr), Conn(nullptr), Debug(Debug) {
    }

    HAKCCompartmentalizationPolicy::~HAKCCompartmentalizationPolicy() {
        Reset();
    }

    void HAKCCompartmentalizationPolicy::Reset() {
        if (Debug) {
            CommonHAKCAnalysis::getWriter() << "Resetting Connection\n";
        }
        if (Conn) {
            Conn.reset();
        }
        if (Database) {
            Database.reset();
        }
    }

    void HAKCCompartmentalizationPolicy::ConnectToDatabase(StringRef DatabasePath) {
        if (Database || Conn) {
            Reset();
        }
        if (Debug) {
            CommonHAKCAnalysis::getWriter() << "Connecting to " << DatabasePath << "\n";
        }

        kuzu::main::SystemConfig SysConfig;
        SysConfig.readOnly = true;
        Database = std::make_shared<kuzu::main::Database>(DatabasePath, SysConfig);
        Conn = std::make_unique<kuzu::main::Connection>(Database.get());
    }

    void HAKCCompartmentalizationPolicy::CheckConnection() {
        if (!Conn) {
            CommonHAKCAnalysis::getWriter() << "Connection is not valid\n";
            throw std::exception();
        }
    }

    HAKCCompartmentP HAKCCompartmentalizationPolicy::GetCompartment(GlobalValue *GV) {
        return nullptr;
    }

    HAKCDivisionP HAKCCompartmentalizationPolicy::GetDivision(GlobalValue *GV) {
        return nullptr;
    }

    HAKCDivisionP HAKCCompartmentalizationPolicy::GetDivision(hakc_compartment_id_t CompartmentID,
                                                              hakc_compartment_division_t DivisionID) {
        auto Statement = CreatePreparedStatement(
                "MATCH (d:HAKCDivision)-[:InCompartment]->(c:HAKCCompartment) WHERE d.DivisionID = $division_id AND c.CompartmentID = $compartment_id RETURN d.*, c.*");
        std::unordered_map<std::string, HAKCDBValueP> Arguments;
        Arguments["division_id"] = std::make_unique<HAKCDBValue>(DivisionID);
        Arguments["compartment_id"] = std::make_unique<HAKCDBValue>(CompartmentID);
        auto Result = Execute(Statement, Arguments);
        if (!Result->isSuccess()) {
            CommonHAKCAnalysis::getWriter() << "Failed to execute query: " << Result->getErrorMessage() << "\n";
            throw std::exception();
        }
        if (!Result->hasNext()) {
            return nullptr;
        }


    }

    HAKC_Division_ID HAKCCompartmentalizationPolicy::GetDivisionID(GlobalValue *GV) {
        return nullptr;
    }

    HAKCCompartmentP HAKCCompartmentalizationPolicy::GetCompartment(hakc_compartment_id_t ID) {
        return nullptr;
    }

    HAKCPreparedStatementP HAKCCompartmentalizationPolicy::CreatePreparedStatement(StringRef query) {
        CheckConnection();
        auto Statement = Conn->prepare(query);
        if (!Statement) {
            CommonHAKCAnalysis::getWriter() << "Could not create PreparedStatement\n";
            throw std::exception();
        }
        return Statement;
    }

    std::unique_ptr<kuzu::main::QueryResult>
    HAKCCompartmentalizationPolicy::Execute(HAKCPreparedStatementP &PreparedStmt,
                                            std::unordered_map<std::string, HAKCDBValueP> &Arguments) {
        CheckConnection();
        return Conn->executeWithParams(PreparedStmt.get(), Arguments);
    }

} // hakc
