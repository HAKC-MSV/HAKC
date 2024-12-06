/**
 * @brief HAKC FunctionAnalysis and Transformation pass
 * @file HAKCPass.cpp
 */

#include "HAKCPass.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"
#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCSystem/HAKCSystemInformation.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"


static cl::opt<std::string, true> HAKC_CONFIG_CL("HAKC_CONFIG", cl::desc("Path to HAKC Configuration File"),
                                                      cl::location(HAKC_CONFIG_PATH), cl::Required);

namespace hakc {
    bool RunHAKCAnalysis(Module &M) {
        // wrapper for getting analysis type 
        CommonHAKCAnalysis HAKCAnalysis(M, HAKC_CONFIG_PATH);
        
        if(HAKCAnalysis.GetSystemInfo().PassMode == RunDataAccessGraphAnalysis){
            runDataAccessGraphAnalysis(HAKCAnalysis);
        }
        else if(HAKCAnalysis.GetSystemInfo().PassMode == RunCompartmentalization){
            runCompartmentalization(HAKCAnalysis);
        }
        else{
            CommonHAKCAnalysis::getWriter() << "Failed to get valid PassMode (this should never be called)\n";
        }

    }
    bool runDataAccessGraphAnalysis(CommonHAKCAnalysis &HAKCAnalysis) {
        bool PerformTransformations = true;
        Module &M = HAKCAnalysis.GetModule();
        StringRef CurrentSourceName(M.getSourceFileName());
        for (auto &path: HAKCAnalysis.GetSystemInfo().HAKCSourcePaths()) {
            if (CurrentSourceName.contains(path)) {
                CommonHAKCAnalysis::getWriter() << "Skipping hakc source " << CurrentSourceName << "\n";
                PerformTransformations = false;
            }
        }

        for (auto &path: HAKCAnalysis.GetSystemInfo().SeparateNamespacePaths()) {
            if (CurrentSourceName.contains(path)) {
                CommonHAKCAnalysis::getWriter() << "Skipping separate namespace source " << CurrentSourceName << "\n";
                PerformTransformations = false;
            }
        }

        if (PerformTransformations) {
            HAKCCompartmentalizationPolicy Policy(HAKCAnalysis.GetSystemInfo().OutputDebugInfo(),
                                                  M.getContext(), KERNEL_COMPARTMENT,
                                                  KERNEL_DIVISION, HAKCAnalysis.GetSystemInfo().DatabasePath());
            HAKCModuleAnalysis ModuleTransformation(HAKCAnalysis, Policy);
            ModuleTransformation.performTransformations();
        }

        return true;
    }
    bool runCompartmentalization(CommonHAKCAnalysis &HAKCAnalysis) {
        Module &M = HAKCAnalysis.GetModule();

        auto Path = HAKCAnalysis.GetSystemInfo().DagAnalysisRootPath;

        std::error_code err;
        err = sys::fs::create_directories(sys::path::parent_path(Path));
        if (err) {
            CommonHAKCAnalysis::getWriter() << "Failed to create " << sys::path::parent_path(Path) << "\n";
            throw std::exception();
        }
        raw_fd_ostream out(Path, err);
        if (!err) {
            HAKCAnalysis.GetSystemInfo().GetTypeIdentifier().OutputYAML(out);
            out.close();
        } else {
            CommonHAKCAnalysis::getWriter() << "Failed to open " << Path << "\n";
            throw std::exception();
        }

        return false;
    }
    

    struct HAKCPass : public PassInfoMixin<HAKCPass> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
            return RunHAKCAnalysis(M) ? PreservedAnalyses::none() : PreservedAnalyses::all();
        }
        static bool isRequired() { return true; }
    };
}// namespace hakc

llvm::PassPluginLibraryInfo getHAKCPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "HAKCPass", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerOptimizerLastEPCallback([&](ModulePassManager &MPM, auto) {
                    MPM.addPass(hakc::HAKCPass());
                    return true;
                });
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getHAKCPluginInfo();
}
