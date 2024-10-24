//
// Created by de29664 on 3/21/23.
//

#include "HAKCSystemInformation.h"
#include "HAKCCompartment.h"
#include "HAKCFile.h"
#include "HAKCSymbol.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCAllocationSize.h"
#include <memory>

#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlSymbol)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlFile)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlCompartment)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlClique)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlMethodsInformation)

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

template<>
struct yaml::MappingTraits<hakc::YamlMethodsInformation> {
    static void mapping(yaml::IO &io, hakc::YamlMethodsInformation &info) {
        io.mapRequired("NAME", info.NAME);
        io.mapRequired("FUNCTIONS", info.FUNCTIONS);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlArchInformation> {
    static void mapping(yaml::IO &io, hakc::YamlArchInformation &info) {
        io.mapRequired("ARCH", info.ARCH);
        io.mapRequired("PLATFORM", info.PLATFORM);
        io.mapRequired("METHODS", info.METHODS);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlHAKCInformation> {
    static void mapping(yaml::IO &io, hakc::YamlHAKCInformation &info) {
        io.mapRequired("SYSTEMINFO", info.SYSTEMINFO);
    }
};

namespace hakc {
    Module &HAKCSystemInformation::getModule() {
        return M;
    }

    std::string HAKCSystemInformation::getCompartmentYamlPath() {
        const char *path_env_var = HAKC_COMPARTMENT_PATH.c_str();
        if (path_env_var == nullptr) {
            CommonHAKCAnalysis::getWriter() << HAKC_COMPARTMENT_PATH << " is not set!\n";
            throw std::exception();
        }
        return path_env_var;
    }

    void HAKCSystemInformation::ProcessArchYaml() {
        const char *yaml_file = HAKC_ARCH_CONFIG.c_str();
        if (!sys::fs::exists(yaml_file)) {
            CommonHAKCAnalysis::getWriter() << "Could not find YAML file " << yaml_file << "\n";
            throw std::exception();
        } else if (!sys::fs::is_regular_file(yaml_file)) {
            CommonHAKCAnalysis::getWriter() << yaml_file << " is not a regular file\n";
            throw std::exception();
        }

        YamlHAKCInformation yi;
        ErrorOr<std::unique_ptr<MemoryBuffer>> mb = MemoryBuffer::getFile(yaml_file);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        assert(!yin.error() && "Error parsing yaml file");
        // yaml is actually parsed here, for some reason 
        yin >> yi;

        CommonHAKCAnalysis::getWriter() << yi.SYSTEMINFO.ARCH << " found\n";
        ARCH = yi.SYSTEMINFO.ARCH;
        CommonHAKCAnalysis::getWriter() << yi.SYSTEMINFO.PLATFORM << " found\n";
        PLATFORM = yi.SYSTEMINFO.PLATFORM; 
        for (YamlMethodsInformation &method: yi.SYSTEMINFO.METHODS) {
            // if method name is not in map
            if(METHODS.find(method.NAME) == METHODS.end()){
                METHODS[method.NAME] = std::set<StringRef>();
            }
            CommonHAKCAnalysis::getWriter() << method.NAME << ":\n";
            for (StringRef function: method.FUNCTIONS) {
                METHODS[method.NAME].insert(function); 
                CommonHAKCAnalysis::getWriter() << "\t" << function << "\n";
            }

        }

        std::set<StringRef> KernelAllocationSizeMapStrings = METHODS["GetKernelAllocationSizeMap"];
        
        for(std::set<StringRef>::iterator ptr = KernelAllocationSizeMapStrings.begin(); ptr != KernelAllocationSizeMapStrings.end(); ptr++){
            CommonHAKCAnalysis::getWriter() << "\t in kernel alloc " << *ptr << "\n";
            HAKCAllocationSize Size(*ptr);
            // TODO: ensure no duplicate key names here 
            KernelAllocationSizeMap.insert({*ptr,Size});
        }
    }

    void HAKCSystemInformation::ProcessCompartmentYaml() {
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

        assert(!yin.error() && "Error parsing yaml file");
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
    }

    void HAKCSystemInformation::Init() {
        // add error checking 
        // process Arch Yaml
        ProcessArchYaml();

        // process Compartment Yaml
        ProcessCompartmentYaml();

        // process HAKC Pass 

    }
    
    HAKCSystemInformation::HAKCSystemInformation(Module &M) : M(M) {
        Init(); 
    }

    StringRef GetModifiedName(StringRef Path) {
        StringRef ModifiedFileName = Path.drop_while([](char c) {
            return c == '.' || llvm::sys::path::is_separator(c);
        });
        return ModifiedFileName;
    }

    bool HAKCSystemInformation::PathContainsPath(StringRef Path1, StringRef Path2) {
        auto ModifiedFileName = GetModifiedName(Path2);

        return Path1.contains(ModifiedFileName);
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

    bool HAKCSystemInformation::ContainsCompartmentalizedSymbols(Module &M) {
        for (auto Symbol: symbols) {
            if (CommonHAKCAnalysis::IsKernelCompartment(Symbol->getCompartmentID())) {
                continue;
            }
            if (PathContainsPath(Symbol->getFile()->GetPath(), M.getName())) {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<HAKCSymbol> HAKCSystemInformation::findSymbol(GlobalValue *GV) {
        bool IsGlobal = GV->hasExternalLinkage();
        StringRef Name = GV->getName();
        if (auto *Func = dyn_cast<Function>(GV)) {
            Name = CommonHAKCAnalysis::GetFunctionName(Func);
        }

        auto AllSymbols = getSymbols(Name);
        std::vector<std::shared_ptr<HAKCSymbol>> FoundSymbols;

        for (auto &symbol: AllSymbols) {
            if (IsGlobal && symbol->isGlobal()) {
                FoundSymbols.push_back(symbol);
            } else if (!IsGlobal && !symbol->isGlobal()) {
                if (PathContainsPath(symbol->getFile()->GetPath(), GV->getParent()->getName())) {
                    FoundSymbols.push_back(symbol);
                }
            }
        }

        if (FoundSymbols.empty()) {
            return nullptr;
        } else if (FoundSymbols.size() > 1) {
            /* FreeBSD allows for a symbol to be defined multiple places (see fse_compress.c in contrib/{zstd,
             * subrepo-openzfs}), so find the symbol with the smallest edit distance file location
             */
            std::vector<std::shared_ptr<HAKCSymbol>> ClosestMatches;
            unsigned ClosestEditDistance = UINT32_MAX;
            for (auto &symbol: FoundSymbols) {
                auto EditDistance = symbol->getFile()->GetPath().edit_distance(GV->getParent()->getName());
                if (EditDistance < ClosestEditDistance) {
                    ClosestMatches.clear();
                    ClosestMatches.push_back(symbol);
                    ClosestEditDistance = EditDistance;
                } else if (EditDistance == ClosestEditDistance) {
                    ClosestMatches.push_back(symbol);
                }
            }

            if (ClosestMatches.size() > 1) {
                CommonHAKCAnalysis::getWriter() << "Found multiple global definitions for " << Name
                                                << " in " << GV->getParent()->getName()
                                                << " are in Compartments:\n";
                unsigned ShortestPath = UINT32_MAX;
                std::shared_ptr<HAKCSymbol> ShortestPathSymbol;
                for (auto &symbol: FoundSymbols) {
                    CommonHAKCAnalysis::getWriter() << symbol->getFile()->GetPath() << ": "
                                                    << std::to_string(symbol->getCompartmentID())
                                                    << " (" << std::to_string(
                            symbol->getFile()->GetPath().edit_distance(GV->getParent()->getName())) << ")"
                                                    << "\n";
                    auto Path = symbol->getFile()->GetPath();
                    if (Path.size() < ShortestPath) {
                        ShortestPath = Path.size();
                        ShortestPathSymbol = symbol;
                    }
                }
                CommonHAKCAnalysis::getWriter() << "Choosing " << ShortestPathSymbol->getName() << "\n";
                return ShortestPathSymbol;
            } else {
                return ClosestMatches[0];
            }
        } else {
            return FoundSymbols[0];
        }
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