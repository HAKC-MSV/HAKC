//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "HAKCAnalysis/HAKCModuleAnalysis.h"

#include "llvm/Support/FileSystem.h"

#include "HAKCCompartmentalizationPolicy/yaml/HAKCMappings.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

namespace hakc {
    HAKCCompartmentalizationPolicy::HAKCCompartmentalizationPolicy(Module &M, HAKCModuleAnalysis *HAKCAnalysis)
            : YamlPolicy(), LLVMModule(M),
              KernelCompartment(KERNEL_COMPARTMENT, KERNEL_ACCESS_TOKEN, M.getContext()),
              TypeIdentifier(M, HAKCAnalysis), Compartments(), GlobalValueDivisionMapping() {
        HAKCCompartmentDivision KernelDivision(KernelCompartment, KERNEL_DIVISION, KERNEL_ACCESS_TOKEN, M.getContext());
        KernelCompartment.AddDivision(KernelDivision);
        Compartments[KernelCompartment.GetCompartmentIDValue()] = KernelCompartment;
    }

    void HAKCCompartmentalizationPolicy::ReadCompartmentalizationPolicy(const std::string &YamlPath) {
        if (!sys::fs::exists(YamlPath)) {
            CommonHAKCAnalysis::getWriter() << "Could not find YAML file " << YamlPath << "\n";
            throw std::exception();
        } else if (!sys::fs::is_regular_file(YamlPath)) {
            CommonHAKCAnalysis::getWriter() << YamlPath << " is not a regular file\n";
            throw std::exception();
        }

        ErrorOr<std::unique_ptr<MemoryBuffer>> mb = MemoryBuffer::getFile(YamlPath);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        yin >> YamlPolicy;
        if (yin.error()) {
            CommonHAKCAnalysis::getWriter() << "Error parsing " << YamlPath << "\n";
            throw std::exception();
        }

        for (auto &YamlCompartment: YamlPolicy.Compartments) {
            HAKCCompartment CurrentCompartment(YamlCompartment.CompartmentID, YamlCompartment.EntryToken,
                                               LLVMModule.getContext());
            for (auto &Target: YamlCompartment.Targets) {
                CurrentCompartment.AddTarget(ConstantInt::get(IntegerType::get(LLVMModule.getContext(), 64), Target));
            }
            for (auto &YamlDivision: YamlCompartment.Cliques) {
                HAKCCompartmentDivision Div(CurrentCompartment, YamlDivision.DivisionID, YamlDivision.AccessToken,
                                            LLVMModule.getContext());
                CurrentCompartment.AddDivision(Div);
            }

            Compartments[YamlCompartment.CompartmentID] = CurrentCompartment;
        }
        for (auto &File: YamlPolicy.Files) {
            for (auto &YamlSymbol: File.Symbols) {
                auto SymbolInfo = TypeIdentifier.FindYamlSymbol(YamlSymbol);
                if (SymbolInfo) {
                    auto Div = GetDivision(YamlSymbol.CompartmentID, YamlSymbol.DivisionID);
                    GlobalValueDivisionMapping[SymbolInfo->GetGlobalObj()] = Div;
                }
            }
        }
    }

    HAKCTypeIdentifier &HAKCCompartmentalizationPolicy::GetTypeIdentifier() {
        return TypeIdentifier;
    }

    HAKCCompartment HAKCCompartmentalizationPolicy::GetCompartment(GlobalValue *GV) {
        auto Division = GetDivision(GV);
        return Division.GetHAKCCompartment();
    }

    HAKCCompartmentDivision HAKCCompartmentalizationPolicy::GetDivision(GlobalValue *GV) {
        if(!GV) {
            CommonHAKCAnalysis::getWriter() << "Trying to find Division for null GlobalValue!\n";
            throw std::exception();
        }

        auto it = GlobalValueDivisionMapping.find(GV);
        if (it == GlobalValueDivisionMapping.end()) {
            return KernelCompartment.GetDivisions()[0];
        } else {
            return it->second;
        }
    }

    HAKCCompartmentDivision HAKCCompartmentalizationPolicy::GetDivision(hakc_compartment_id_t CompartmentID,
                                                                        hakc_compartment_division_t DivisionID) {
        auto Result = KernelCompartment.GetDivisions()[0];

        auto Compartment = GetCompartment(CompartmentID);
        for (auto &Div: Compartment.GetDivisions()) {
            if (Div.GetDivisionID()->getSExtValue() == DivisionID) {
                Result = Div;
            }
        }
        return Result;
    }

    HAKC_Division_ID HAKCCompartmentalizationPolicy::GetDivisionID(GlobalValue *GV) {
        auto Division = GetDivision(GV);
        return Division.GetDivisionID();
    }

    HAKCCompartment HAKCCompartmentalizationPolicy::GetCompartment(hakc_compartment_id_t ID) {
        auto it = Compartments.find(ID);
        if (it == Compartments.end()) {
            return KernelCompartment;
        } else {
            return it->second;
        }
    }

} // hakc
