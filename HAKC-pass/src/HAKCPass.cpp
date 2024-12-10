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

// critical reference guide for cl: https://llvm.org/docs/CommandLine.html#internal-vs-external-storage
std::string HAKC_CONFIG_PATH;
// std::string SOURCE_PATH;
// std::string BUILD_PATH;

static cl::opt<std::string, true> HAKC_CONFIG_CL("HAKC_CONFIG", cl::desc("Path to HAKC Configuration File"),
                                                      cl::location(HAKC_CONFIG_PATH), cl::Required);

namespace hakc {
    
    bool runCompartmentalization(CommonHAKCAnalysis &HAKCAnalysis) {
        bool PerformTransformations = true;
        Module &M = HAKCAnalysis.GetModule();
        // SOURCE_PATH = HAKCAnalysis.GetSystemInfo().GetSourcePath();
        // BUILD_PATH = HAKCAnalysis.GetSystemInfo().GetBuildPath();
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
    bool runDataAccessGraphAnalysis (CommonHAKCAnalysis &HAKCAnalysis) {
        Module &M = HAKCAnalysis.GetModule();
        // SOURCE_PATH = HAKCAnalysis.GetSystemInfo().GetSourcePath();
        // BUILD_PATH = HAKCAnalysis.GetSystemInfo().GetBuildPath();
        auto BasePath = CommonHAKCAnalysis::GetModuleFullPath(M);
        auto P = HAKCAnalysis.GetTransformedPath(BasePath);
        
        auto Prefix = HAKCAnalysis.GetSystemInfo().GetDagAnalysisRootPath().str(); 
        if (Prefix.back() != llvm::sys::path::get_separator().back()) {
            Prefix += llvm::sys::path::get_separator();
        }
        auto Path = Prefix;
        Path += P;
        Path += ".dag.yml";

        std::error_code err;
        err = sys::fs::create_directories(sys::path::parent_path(Path));
        if (err) {
            errs() << "Failed to create " << sys::path::parent_path(Path) << "\n";
            throw std::exception();
        }
        errs() << "about to raw_fd_ostream\n";
        raw_fd_ostream out(Path, err);
        if (!err) {
            errs() << "About to output YAML\n";
            HAKCAnalysis.GetSystemInfo().GetTypeIdentifier().OutputYAML(out);
            out.close();
        } else {
            errs() << "Failed to open " << Path << "\n";
            throw std::exception();
        }

        return false;
    }

    bool RunHAKCAnalysis(Module &M) {
        
        if(HAKC_CONFIG_PATH.empty()){
            errs() << "HAKC_CONFIG_PATH parameter '-mllvm -HAKC_CONFIG=somepath' not specifiecd\n";
            throw std::exception();
        }
        errs() << HAKC_CONFIG_PATH << "\n"; 
        // wrapper for getting analysis type 
        errs() << "here001\n";
        CommonHAKCAnalysis HAKCAnalysis(M, HAKC_CONFIG_PATH);
        errs() << "here002\n";
        
        if(HAKCAnalysis.GetSystemInfo().GetPassMode() == RunDataAccessGraphAnalysis){
            return runDataAccessGraphAnalysis(HAKCAnalysis);
        }
        else if(HAKCAnalysis.GetSystemInfo().GetPassMode() == RunCompartmentalization){
            return runCompartmentalization(HAKCAnalysis);
        }
        else if(HAKCAnalysis.GetSystemInfo().GetPassMode() == InvalidPassModeType){
            errs() << "Failed to get valid PassMode (this should never be called)\n";
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
