//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"

#include "llvm/Support/FileSystem.h"

#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

#include <string_view>

namespace hakc {
    HAKCCompartmentalizationPolicy::HAKCCompartmentalizationPolicy(bool Debug, LLVMContext &Ctx,
                                                                   hakc_compartment_id_t DefaultCompartmentID,
                                                                   hakc_compartment_division_t DefaultDivisionID,
                                                                   StringRef DatabasePath) : Database(nullptr),
                                                                                             Conn(nullptr),
                                                                                             Debug(Debug),
                                                                                             DefaultDivision(nullptr),
                                                                                             LLVMCtx(Ctx),
                                                                                             Compartments(),
                                                                                             Divisions() {
        ConnectToDatabase(DatabasePath);
        SetDefaultDivision(DefaultCompartmentID, DefaultDivisionID);
    }

    HAKCCompartmentalizationPolicy::~HAKCCompartmentalizationPolicy() {
        Reset();
    }

    void HAKCCompartmentalizationPolicy::SetDefaultDivision(hakc_compartment_id_t CompartmentID,
                                                            hakc_compartment_division_t DivisionID) {
        DefaultDivision = GetDivision(CompartmentID, DivisionID);
        if (!DefaultDivision) {
            CommonHAKCAnalysis::getWriter() << "Could not find Division " << DivisionID << " and Compartment "
                                            << CompartmentID << " in the database!\n";
            throw std::exception();
        }
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
        Database = std::make_shared<kuzu::main::Database>(DatabasePath.str(), SysConfig);
        Conn = std::make_unique<kuzu::main::Connection>(Database.get());
    }

    void HAKCCompartmentalizationPolicy::CheckConnection() {
        if (!Conn) {
            CommonHAKCAnalysis::getWriter() << "Connection is not valid\n";
            throw std::exception();
        }
    }

    hakc::HAKCCompartmentDivision &HAKCCompartmentalizationPolicy::GetDivision(GlobalValue *GV) {
        return *DefaultDivision;
    }

    HAKCDivisionP HAKCCompartmentalizationPolicy::GetDivision(hakc_compartment_id_t CompartmentID,
                                                              hakc_compartment_division_t DivisionID) {
        auto Division = FindCachedDivision(CompartmentID, DivisionID);
        if (Division) {
            return Division;
        }

        auto Compartment = GetCompartment(CompartmentID);

        auto Statement = CreatePreparedStatement(
                "MATCH (d:HAKCDivision)-[:InCompartment]->(c:HAKCCompartment) WHERE d.DivisionID = $division_id AND "
                "c.CompartmentID = $compartment_id RETURN d.AccessToken as AccessToken");
        std::unordered_map<std::string, HAKCDBValueP> Arguments;
        Arguments["division_id"] = std::make_unique<HAKCDBValue>(DivisionID);
        Arguments["compartment_id"] = std::make_unique<HAKCDBValue>(CompartmentID);
        auto Result = Execute(Statement, Arguments);
        if (!Result->hasNext()) {
            return nullptr;
        }

        auto Data = Result->getNext();
        auto AccessToken = Data->getValue(0)->getValue<hakc_access_token_t>();
        Division = std::make_shared<HAKCCompartmentDivision>(*Compartment, DivisionID, AccessToken, LLVMCtx);
        Divisions.push_back(Division);

        return Division;
    }

    HAKCCompartmentP HAKCCompartmentalizationPolicy::GetCompartment(hakc_compartment_id_t CompartmentID) {
        auto Compartment = FindCachedCompartment(CompartmentID);
        if (Compartment) {
            return Compartment;
        }

        auto Statement = CreatePreparedStatement(
                "MATCH (c:HAKCCompartment) WHERE c.CompartmentID = $compartment_id RETURN c.EntryToken");
        std::unordered_map<std::string, HAKCDBValueP> Arguments;
        Arguments["compartment_id"] = std::make_unique<HAKCDBValue>(CompartmentID);
        auto Result = Execute(Statement, Arguments);
        if (!Result->hasNext()) {
            CommonHAKCAnalysis::getWriter() << "Could not find Compartment " << CompartmentID << " in database\n";
            throw std::exception();
        }
        auto EntryToken = Result->getNext()->getValue(0)->getValue<hakc_access_token_t>();
        Compartment = std::make_shared<HAKCCompartment>(CompartmentID, EntryToken, LLVMCtx);
        Compartments.push_back(Compartment);

        return Compartment;
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
        auto Result = Conn->executeWithParams(PreparedStmt.get(), Arguments);
        if (!Result->isSuccess()) {
            CommonHAKCAnalysis::getWriter() << "Failed to execute query: " << Result->getErrorMessage() << "\n";
            throw std::exception();
        }
        return Result;
    }

    HAKCDivisionP HAKCCompartmentalizationPolicy::FindCachedDivision(hakc_compartment_id_t CompartmentID,
                                                                     hakc_compartment_division_t DivisionID) {
        for (auto &Division: Divisions) {
            if (Division->GetHAKCCompartment().GetCompartmentID()->equalsInt(CompartmentID) &&
                Division->GetDivisionID()->equalsInt(DivisionID)) {
                return Division;
            }
        }
        return nullptr;
    }

    HAKCCompartmentP HAKCCompartmentalizationPolicy::FindCachedCompartment(hakc_compartment_id_t CompartmentID) {
        for (auto &Compartment: Compartments) {
            if (Compartment->GetCompartmentID()->equalsInt(CompartmentID)) {
                return Compartment;
            }
        }
        return nullptr;
    }

} // hakc
