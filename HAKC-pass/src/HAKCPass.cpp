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

static cl::opt<std::string, true> HAKC_ANALYSIS_CL("HAKC_ANALYSIS", cl::desc("Specify HAKC Pass Mode"),
                                                   cl::location(HAKC_ANALYSIS), cl::Required);
static cl::opt<std::string, true> HAKC_DEBUG_NAME_CL("HAKC_DEBUG_NAME",
                                                     cl::desc("Enable debug output for a specific function"),
                                                     cl::location(HAKC_DEBUG_NAME));
static cl::opt<std::string, true> HAKC_DAG_ANALYSIS_ROOT_CL("HAKC_DAG_ANALYSIS_ROOT", cl::desc(""),
                                                            cl::location(HAKC_DAG_ANALYSIS_ROOT));
static cl::opt<std::string, true> HAKC_ARCH_CONFIG_CL("HAKC_ARCH_CONFIG", cl::desc("Path to HAKC arch yaml"),
                                                      cl::location(HAKC_ARCH_CONFIG), cl::Required);
static cl::opt<std::string, true> HAKC_COMPARTMENT_PATH_CL("HAKC_COMPARTMENT_PATH",
                                                           cl::desc("Path to HAKC compartment yaml"),
                                                           cl::location(HAKC_COMPARTMENT_PATH), cl::Required);
static cl::opt<std::string, true> HAKC_NO_KERNEL_TRANSFERS_CL("HAKC_NO_KERNEL_TRANSFERS", cl::desc(""),
                                                              cl::location(HAKC_NO_KERNEL_TRANSFERS));
static cl::opt<std::string, true> HAKC_MORELLO_HYBRID_CL("HAKC_MORELLO_HYBRID", cl::desc(""),
                                                         cl::location(HAKC_MORELLO_HYBRID));

namespace hakc {
    bool runDataAccessGraphAnalysis(Module &M) {
        auto BasePath = CommonHAKCAnalysis::GetModuleFullPath(M);
        auto P = HAKCTypeIdentifier::GetTransformedPath(BasePath);

        if (HAKC_DAG_ANALYSIS_ROOT.empty()) {
            CommonHAKCAnalysis::getWriter() << HAKC_DAG_ANALYSIS_ROOT << " is not set!\n";
            throw std::exception();
        }

        auto Prefix = HAKC_DAG_ANALYSIS_ROOT;
        if (Prefix.back() != llvm::sys::path::get_separator().back()) {
            Prefix += llvm::sys::path::get_separator();
        }
        auto Path = Prefix;
        Path += P;
        Path += ".dag.yml";

        std::error_code err;
        err = sys::fs::create_directories(sys::path::parent_path(Path));
        if (err) {
            CommonHAKCAnalysis::getWriter() << "Failed to create " << sys::path::parent_path(Path) << "\n";
            throw std::exception();
        }
        raw_fd_ostream out(Path, err);
        if (!err) {
            CommonHAKCAnalysis HAKCAnalysis(M);
            HAKCTypeIdentifier TypeIdentifier(HAKCAnalysis);
            TypeIdentifier.ProcessDebugInfo();
            TypeIdentifier.OutputYAML(out);
            out.close();
        } else {
            CommonHAKCAnalysis::getWriter() << "Failed to open " << Path << "\n";
            throw std::exception();
        }

        return false;
    }

    bool runCompartmentalization(Module &M) {
        bool PerformTransformations = true;
        CommonHAKCAnalysis HAKCAnalysis(M);
        HAKCAnalysis.InitConfig(HAKC_ARCH_CONFIG);
        HAKCTypeIdentifier TypeIdentifier(HAKCAnalysis);

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

    struct HAKCPass : public PassInfoMixin<HAKCPass> {
        const std::vector<std::pair<StringRef, std::function<bool(Module &)>>> available_options =
                {
                        {"dag",              runDataAccessGraphAnalysis},
                        {"compartmentalize", runCompartmentalization}
                };

        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
            for (const auto &opt: available_options) {
                if (opt.first == HAKC_ANALYSIS) {
                    return opt.second(M) ? PreservedAnalyses::none() : PreservedAnalyses::all();
                }
            }
            return PreservedAnalyses::all();
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
