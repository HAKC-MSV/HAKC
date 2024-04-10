//
// Created by de29664 on 3/21/23.
//

#include "HAKCSystemInformation.h"
#include "HAKCCompartment.h"
#include "HAKCFile.h"
#include "HAKCSymbol.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

#include <memory>

#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlSymbol)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlFile)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlCompartment)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlClique)

template<>
struct yaml::ScalarEnumerationTraits<hakc::sym_color_t> {
    static void enumeration(yaml::IO &io, hakc::sym_color_t &value) {
        io.enumCase(value, "SILVER_CLIQUE", hakc::SILVER_CLIQUE);
        io.enumCase(value, "GREEN_CLIQUE", hakc::GREEN_CLIQUE);
        io.enumCase(value, "RED_CLIQUE", hakc::RED_CLIQUE);
        io.enumCase(value, "ORANGE_CLIQUE", hakc::ORANGE_CLIQUE);
        io.enumCase(value, "YELLOW_CLIQUE", hakc::YELLOW_CLIQUE);
        io.enumCase(value, "PURPLE_CLIQUE", hakc::PURPLE_CLIQUE);
        io.enumCase(value, "BLUE_CLIQUE", hakc::BLUE_CLIQUE);
        io.enumCase(value, "GREY_CLIQUE", hakc::GREY_CLIQUE);
        io.enumCase(value, "PINK_CLIQUE", hakc::PINK_CLIQUE);
        io.enumCase(value, "BROWN_CLIQUE", hakc::BROWN_CLIQUE);
        io.enumCase(value, "WHITE_CLIQUE", hakc::WHITE_CLIQUE);
        io.enumCase(value, "BLACK_CLIQUE", hakc::BLACK_CLIQUE);
        io.enumCase(value, "TEAL_CLIQUE", hakc::TEAL_CLIQUE);
        io.enumCase(value, "VIOLET_CLIQUE", hakc::VIOLET_CLIQUE);
        io.enumCase(value, "CRIMSON_CLIQUE", hakc::CRIMSON_CLIQUE);
        io.enumCase(value, "GOLD_CLIQUE", hakc::GOLD_CLIQUE);
        io.enumCase(value, "NO_CLIQUE", hakc::NO_CLIQUE);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlSymbol> {
    static void mapping(yaml::IO &io, hakc::YamlSymbol &info) {
        io.mapRequired("CLIQUE", info.color);
        io.mapRequired("NAME", info.name);
        io.mapRequired("COMPARTMENT", info.compartment);
        io.mapRequired("IS_GLOBAL", info.is_global);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlClique> {
    static void mapping(yaml::IO &io, hakc::YamlClique &info) {
        io.mapRequired("ACCESS_TOKEN", info.access_token);
        io.mapRequired("COLOR", info.color);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlFile> {
    static void mapping(yaml::IO &io, hakc::YamlFile &info) {
        io.mapOptional("GUID", info.guid);
        io.mapRequired("PATH", info.name);
        io.mapOptional("SYMBOLS", info.symbols);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlCompartment> {
    static void mapping(yaml::IO &io, hakc::YamlCompartment &info) {
        io.mapRequired("ID", info.id);
        io.mapOptional("TARGETS", info.targets);
        io.mapRequired("CLIQUES", info.cliques);
        io.mapOptional("ENTRY_TOKEN", info.entry_token);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlInformation> {
    static void mapping(yaml::IO &io, hakc::YamlInformation &info) {
        io.mapRequired("COMPARTMENTS", info.compartments);
        io.mapRequired("FILES", info.files);
    }
};

namespace hakc {

    std::string HAKCSystemInformation::getCompartmentYamlPath() {
        const char *path_env_var = std::getenv(COMPARTMENT_PATH_ENV_VAR.str().c_str());
        if (path_env_var == nullptr) {
            CommonHAKCAnalysis::getWriter() << COMPARTMENT_PATH_ENV_VAR << " is not set!\n";
            throw std::exception();
        }
        return path_env_var;
    }

    HAKCSystemInformation::HAKCSystemInformation(Module &M) : M(M),
                                                              compartments(),
                                                              symbols(),
                                                              ModuleContainsCompartmentalizedSymbols(false),
                                                              DebugInfo() {
        YamlInformation yi;

        auto yaml_file = getCompartmentYamlPath();
        if (!sys::fs::exists(yaml_file)) {
            CommonHAKCAnalysis::getWriter() << "Could not find YAML file " << yaml_file << "\n";
            throw std::exception();
        } else if (!sys::fs::is_regular_file(yaml_file)) {
            CommonHAKCAnalysis::getWriter() << yaml_file << " is not a regular file\n";
            throw std::exception();
        }
        ErrorOr<std::unique_ptr<MemoryBuffer>> mb = MemoryBuffer::getFile(yaml_file);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        if (yin.error()) {
            CommonHAKCAnalysis::getWriter() << "Error parsing YAML file " << yaml_file << "\n";
            throw std::exception();
        }
        yin >> yi;

        IntegerType *i32_type = IntegerType::getInt32Ty(M.getContext());

        for (YamlCompartment &comp: yi.compartments) {
            for (auto clique: comp.cliques) {
                auto Compartment = std::make_shared<HAKCCompartment>(comp.id, comp.entry_token, clique.access_token,
                                                                     clique.color);
                compartments.insert(Compartment);
            }
        }

        for (YamlCompartment &comp: yi.compartments) {
            for (auto &id: comp.targets) {
                auto TailCompartments = getCompartments(id);
                for (auto &HeadCompartment: getCompartments(comp.id)) {
                    for (auto TailCompartment: TailCompartments) {
                        HeadCompartment->addTarget(TailCompartment);
                    }
                }
            }
        }

        for (YamlFile &YamlFile: yi.files) {
            auto File = std::make_shared<HAKCFile>(YamlFile);
            for (YamlSymbol &sym: YamlFile.symbols) {
                auto Compartment = getCompartment(sym.compartment, sym.color);
                if (!Compartment) {
                    CommonHAKCAnalysis::getWriter() << "Could find find Compartment " << std::to_string(sym.compartment)
                                                    << " "
                                                    << getColorStringFromValue(ConstantInt::get(i32_type, sym.color))
                                                    << " for Symbol " << sym.name
                                                    << "\n";
                    throw std::exception();
                }
                std::shared_ptr<HAKCSymbol> symbol = std::make_shared<HAKCSymbol>(sym.name, Compartment, File,
                                                                                  sym.is_global);
                symbols.insert(symbol);
            }
        }

        DebugInfo.processModule(M);
        DetectCompartmentalization();
    }

    std::set<std::shared_ptr<HAKCCompartment>> HAKCSystemInformation::getCompartments(hakc_compartment_id_t ID) {
        std::set<std::shared_ptr<HAKCCompartment>> result;
        for (auto &Compartment: compartments) {
            if (Compartment->getID() == ID) {
                result.insert(Compartment);
            }
        }
        return result;
    }

    std::shared_ptr<HAKCCompartment>
    HAKCSystemInformation::getCompartment(hakc_compartment_id_t id, sym_color_t color) {
        for (auto Compartment: getCompartments(id)) {
            if (Compartment->getColor() == color) {
                return Compartment;
            }
        }
        return nullptr;
    }

    hakc_access_token_t HAKCSystemInformation::getEntryToken(hakc_compartment_id_t CompartmentID) {
        auto Compartments = getCompartments(CompartmentID);
        if (Compartments.empty()) {
            CommonHAKCAnalysis::getWriter() << "Could not find any Compartment with ID "
                                            << std::to_string(CompartmentID) << "\n";
            throw std::exception();
        }
        return (*Compartments.begin())->getEntryToken();
    }

    void HAKCSystemInformation::DetectCompartmentalization() {
        std::vector<GlobalValue *> ModuleSymbols;

#define COMPARTMENT_CHECK(ModuleSymbol)                                                                 \
        do {                                                                                            \
            auto Symbol = findSymbol(ModuleSymbol);                                                     \
            if(Symbol) {                                                                                \
                if (!CommonHAKCAnalysis::IsKernelCompartment(Symbol->getCompartmentID())) {             \
                    ModuleContainsCompartmentalizedSymbols = true;                                      \
                    return;                                                                             \
                }                                                                                       \
            }                                                                                           \
        } while(0)

        for (auto &GV: M.globals()) {
            if (GV.isDeclaration()) {
                continue;
            }
            COMPARTMENT_CHECK(&GV);
        }
        for (auto &F: M.functions()) {
            if (F.isIntrinsic() || F.isDeclaration()) {
                continue;
            }
            COMPARTMENT_CHECK(&F);
        }
    }

    bool HAKCSystemInformation::ContainsCompartmentalizedSymbols() const {
        return ModuleContainsCompartmentalizedSymbols;
    }

    bool HAKCSystemInformation::SymbolIsInScope(const std::shared_ptr<HAKCSymbol>& Symbol, const DIScope *Scope) {
        std::string ScopeFile;
        if (sys::path::is_relative(Scope->getFilename())) {
            ScopeFile = Scope->getDirectory().str();
            if (!Scope->getDirectory().endswith(llvm::sys::path::get_separator())) {
                ScopeFile += llvm::sys::path::get_separator();
            }
            ScopeFile += Scope->getFilename();
        } else {
            ScopeFile = Scope->getFilename().str();
        }

        std::error_code err;
        SmallString<256> ScopePath;
        err = sys::fs::real_path(ScopeFile, ScopePath, true);
        if (err) {
            CommonHAKCAnalysis::getWriter() << "Could not get real path to " << ScopeFile << "\n";
            throw std::exception();
        }

        auto TransformedScopePath = HAKCTypeIdentifier::GetTransformedPath(ScopePath);
        return TransformedScopePath == Symbol->getFile()->GetPath();
    }

    std::shared_ptr<HAKCSymbol> HAKCSystemInformation::findSymbol(GlobalValue *GV) {
        StringRef Name = GV->getName();
        if (auto *Func = dyn_cast<Function>(GV)) {
            Name = CommonHAKCAnalysis::GetFunctionName(Func);
        }
        auto Symbols = getSymbols(Name);
        if (Symbols.empty()) {
            return nullptr;
        } else if (Symbols.size() == 1) {
            return *Symbols.begin();
        }
        for (auto Symbol: Symbols) {
            if (isa<Function>(GV)) {
                for (auto *DIF: DebugInfo.subprograms()) {
                    if (SymbolIsInScope(Symbol, DIF->getScope())) {
                        return Symbol;
                    }
                }
            } else {
                for (auto *DIGV: DebugInfo.global_variables()) {
                    if (SymbolIsInScope(Symbol, DIGV->getVariable()->getScope())) {
                        return Symbol;
                    }
                }
            }
        }
        return *Symbols.begin();
    }

    std::set<std::shared_ptr<HAKCSymbol>> HAKCSystemInformation::getSymbols(StringRef name) {
        std::set<std::shared_ptr<HAKCSymbol>> result;
        for (auto &Symbol: symbols) {
            if (Symbol->getName() == name) {
                result.insert(Symbol);
            }
        }
        return result;
    }

    std::string HAKCSystemInformation::getColorStringFromValue(ConstantInt *color) {
        switch (color->getZExtValue()) {
            case SILVER_CLIQUE:
                return "SILVER_CLIQUE";
            case GREEN_CLIQUE:
                return "GREEN_CLIQUE";
            case RED_CLIQUE:
                return "RED_CLIQUE";
            case ORANGE_CLIQUE:
                return "ORANGE_CLIQUE";
            case YELLOW_CLIQUE:
                return "YELLOW_CLIQUE";
            case PURPLE_CLIQUE:
                return "PURPLE_CLIQUE";
            case BLUE_CLIQUE:
                return "BLUE_CLIQUE";
            case GREY_CLIQUE:
                return "GREY_CLIQUE";
            case PINK_CLIQUE:
                return "PINK_CLIQUE";
            case BROWN_CLIQUE:
                return "BROWN_CLIQUE";
            case WHITE_CLIQUE:
                return "WHITE_CLIQUE";
            case BLACK_CLIQUE:
                return "BLACK_CLIQUE";
            case TEAL_CLIQUE:
                return "TEAL_CLIQUE";
            case VIOLET_CLIQUE:
                return "VIOLET_CLIQUE";
            case CRIMSON_CLIQUE:
                return "CRIMSON_CLIQUE";
            case GOLD_CLIQUE:
                return "GOLD_CLIQUE";
            case NO_CLIQUE:
                return "NO_CLIQUE";
            default:
                CommonHAKCAnalysis::getWriter() << "number " << color->getZExtValue() << "isn't a valid color\n";
                return "INVALID_CLIQUE";
        }
    }
} // hakc
