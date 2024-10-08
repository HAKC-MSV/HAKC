/**
 * @brief HAKC FunctionAnalysis and Transformation pass
 * @file HAKCPass.cpp
 */

#include "HAKCPass.h"
#include "HAKCSymbolGenerator.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"
#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCSystemInformation.h"

#if defined(HAKC_CHERIBSD_MORELLO)
#include "HAKCAnalysis/CheriBSD/HAKCModuleAnalysisCheriBSDCheri.h"
#elif defined(HAKC_LINUX_X86)
#include "HAKCAnalysis/Linux/X86/HAKCModuleAnalysisLinuxX86.h"
#elif defined(HAKC_LINUX_ARMV8)
#include "HAKCAnalysis/Linux/Arm/HAKCModuleAnalysisLinuxArmV8.h"
#elif defined(HAKC_LINUX_ARMV9)
#include "HAKCAnalysis/Linux/Arm/HAKCModuleAnalysisLinuxArmV9.h"
#else
#error "HAKC Architecture Unspecified"
#endif

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"

static cl::opt<std::string> HAKCPassMode("HAKCPassMode", cl::desc("Specify HAKC Pass Mode"), cl::value_desc("dag, compartmentalize, or custom"), cl::Required);
static cl::opt<std::string> CompartmentYamlPath("CompartmentYamlPath", cl::desc("CompartmentYamlPath"), cl::value_desc("HAKC Compartment Path"), cl::Required);
static cl::opt<std::string> ArchYamlPath("ArchYamlPath", cl::desc("ArchYamlPath"), cl::value_desc("Architecture specific yaml configuration file"), cl::Required);

namespace hakc {

    HAKCModuleAnalysis *GetModuleAnalysis(Module &M) {
        HAKCModuleAnalysis *ModuleAnalysis;
#if defined(HAKC_CHERIBSD_MORELLO)
        ModuleAnalysis = new HAKCModuleAnalysisCheriBSDCheri(M);
#elif defined(HAKC_LINUX_X86)
        ModuleAnalysis = new HAKCModuleAnalysisLinuxX86(M);
#elif defined(HAKC_LINUX_ARMV8)
        ModuleAnalysis = new HAKCModuleAnalysisLinuxArmV8(M);
#elif defined(HAKC_LINUX_ARMV9)
        ModuleAnalysis = new HAKCModuleAnalysisLinuxArmV9(M);
#else
#error "HAKC Architecture Unspecified"
#endif
        // TODO: add error checking 
        ModuleAnalysis->InitSystemInformation(ArchYamlPath, CompartmentYamlPath, HAKCPassMode);
        ModuleAnalysis->InitAnalysis();
        return ModuleAnalysis;
    }

    bool runDataAccessGraphAnalysis(Module &M) {
        StringRef P = M.getSourceFileName();
        bool end;
        do {
            end = P.consume_front("../");
        } while (end);

        SmallString<512> Path = P;
        auto *Transformation = GetModuleAnalysis(M);
        HAKCTypeIdentifier typeIdentifier(M, Transformation);
        const char *root = std::getenv(DAG_ANALYSIS_ROOT_ENV_VAR.str().c_str());
        if (!root || std::strlen(root) == 0) {
            CommonHAKCAnalysis::getWriter() << DAG_ANALYSIS_ROOT_ENV_VAR << " is not set!\n";
            throw std::exception();
        }

        std::string Prefix = root;
        if (Prefix.back() != llvm::sys::path::get_separator().back()) {
            Prefix += llvm::sys::path::get_separator();
        }
        llvm::sys::path::replace_path_prefix(Path, "", Prefix);
        llvm::sys::path::remove_dots(Path, true);
        llvm::sys::path::replace_extension(Path, ".dag.yml");

        if (!Path.endswith(".dag.yml")) {
            CommonHAKCAnalysis::getWriter() << "Invalid file name: " << Path << "\n";
            throw std::exception();
        }

        std::error_code err;
        err = sys::fs::create_directories(sys::path::parent_path(Path));
        if (err) {
            CommonHAKCAnalysis::getWriter() << "Failed to create " << sys::path::parent_path(Path) << "\n";
            throw std::exception();
        }
        raw_fd_ostream out(Path, err);
        if (!err) {
            typeIdentifier.outputTypes(out);
            out.close();
        } else {
            CommonHAKCAnalysis::getWriter() << "Failed to open " << Path << "\n";
            throw std::exception();
        }
        delete Transformation;
        return false;
    }

    bool runCustom(Module &M){
        CommonHAKCAnalysis::getWriter() << "running my custom code with HAKCPassMode of " << HAKCPassMode << "! \n";
        HAKCModuleAnalysis *Transformation = GetModuleAnalysis(M);
        return true; 
    }

    bool runCompartmentalization(Module &M) {
        bool PerformTransformations = true;
        HAKCModuleAnalysis *Transformation = GetModuleAnalysis(M);
        for (auto path: Transformation->GetHAKCSourcePaths()) {
            if (M.getSourceFileName().find(path.str()) != M.getSourceFileName().npos) {
                CommonHAKCAnalysis::getWriter() << "Skipping hakc source " << M.getSourceFileName() << "\n";
                PerformTransformations = false;
            }
        }

        for (auto path: Transformation->GetSeparateNamespacePaths()) {
            if (M.getSourceFileName().find(path.str()) != M.getSourceFileName().npos) {
                CommonHAKCAnalysis::getWriter() << "Skipping separate namespace source " << M.getSourceFileName()
                                                << "\n";
                PerformTransformations = false;
            }
        }

        if (PerformTransformations) {
            Transformation->performTransformations();
            /*if (transformation.isCompartmentalized()) {
                CommonHAKCAnalysis::getWriter() << "Total Data Checks: "
                       << transformation.totalDataChecks << "\n"
                       << "Total Code Checks: "
                       << transformation.totalCodeChecks << "\n"
                       << "Total Transfers:   "
                       << transformation.totalTransfers
                       << "\n";
            }*/
        }

        bool moduleTransformed = Transformation->isModuleTransformed();

        delete Transformation;
        return moduleTransformed;
    }

    struct HAKCPass : public PassInfoMixin<HAKCPass> {
        const std::vector<std::pair<std::string,
                std::function<bool(Module &)>>>
                available_options = {
                {"dag",              runDataAccessGraphAnalysis},
                {"compartmentalize", runCompartmentalization},
                {"custom", runCustom}};

        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
            const char *preload = std::getenv(HAKC_ENV_VAR.str().c_str());
            if (HAKCPassMode.getValue() != "") {
                for (const auto &opt: available_options) {
                    if (opt.first == HAKCPassMode.c_str()) {
                        return opt.second(M) ? PreservedAnalyses::none() : PreservedAnalyses::all();
                    }
                }
                CommonHAKCAnalysis::getWriter() << "WARNING: "
                                                << HAKC_ENV_VAR
                                                << " was set to " << preload
                                                << " which is invalid.  No HAKC analysis was performed\n";
                return PreservedAnalyses::all();
            } else {
                CommonHAKCAnalysis::getWriter() << "WARNING: "
                                                << HAKC_ENV_VAR
                                                << " is not set! No HAKC analysis was performed!\n";
                return PreservedAnalyses::all();
            }
        }

        static bool isRequired() { return true; }
    };
}// namespace hakc

// env HAKC_ANALYSIS=custom HAKC_ARCH_CONFIG=x86config.yaml HAKC_COMPARTMENT_PATH=tests/hakc-test0.yml $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -mypass_option=abc  -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c
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

