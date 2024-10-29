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
#include "HAKCYAMLParser.h"
#include <memory>

#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

namespace hakc {

    HAKCSystemInformation::HAKCSystemInformation(Module &M) : M(M) {
        // todo: maybe fix weird memory issue here 
        parser = std::make_shared<HAKCYAMLParser>(M);
        ARCH = parser->ARCH;
        PLATFORM = parser->PLATFORM;
        ProcessYAML();
        ProcessCompartmentYaml();
    }

    std::map<std::string, std::set<std::string>> *HAKCSystemInformation::GetMethods(){
        return parser->GetMethods(); 
    }

    Module &HAKCSystemInformation::getModule() {
        return M;
    }

    void HAKCSystemInformation::ProcessYAML() {
        std::set<std::string> KernelAllocationSizeMapStrings = (*GetMethods())["GetKernelAllocationSizeMap"];
        for (std::set<std::string>::iterator ptr = KernelAllocationSizeMapStrings.begin(); ptr != KernelAllocationSizeMapStrings.end(); ptr++) {
            CommonHAKCAnalysis::getWriter() << "\t in kernel alloc " << *ptr << "\n";
            HAKCAllocationSize Size(*ptr);
            KernelAllocationSizeMap.insert({*ptr, Size});
        }
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

    bool HAKCSystemInformation::ContainsCompartmentalizedSymbols(Module &M) {
        for (auto Symbol : symbols) {
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

        for (auto &symbol : AllSymbols) {
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
            for (auto &symbol : FoundSymbols) {
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
                for (auto &symbol : FoundSymbols) {
                    CommonHAKCAnalysis::getWriter() << symbol->getFile()->GetPath() << ": "
                                                    << std::to_string(symbol->getCompartmentID())
                                                    << " (" << std::to_string(symbol->getFile()->GetPath().edit_distance(GV->getParent()->getName())) << ")"
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

    void HAKCSystemInformation::ProcessCompartmentYaml() {
        YamlInformation yi;
        std::string yaml_file = HAKC_COMPARTMENT_PATH;
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
        CommonHAKCAnalysis::getWriter() << "found compartments: \n";
        for (YamlCompartment &comp : yi.compartments) {
            CommonHAKCAnalysis::getWriter() << "found cliques: \n";
            for (auto clique : comp.cliques) {
                auto Compartment = std::make_shared<HAKCCompartment>(comp.id, comp.entry_token, clique.access_token, clique.color);
                CommonHAKCAnalysis::getWriter() << "found clique: \t" <<  comp.id << "\n";
                compartments.insert(Compartment);
            }
        }

        for (YamlCompartment &comp : yi.compartments) {
            for (auto &id : comp.targets) {
                auto TailCompartments = getCompartments(id);
                for (auto &HeadCompartment : getCompartments(comp.id)) {
                    for (auto TailCompartment : TailCompartments) {
                        HeadCompartment->addTarget(TailCompartment);
                    }
                }
            }
        }

        for (YamlFile &YamlFile : yi.files) {
            auto File = std::make_shared<HAKCFile>(YamlFile);
            for (YamlSymbol &sym : YamlFile.symbols) {
                auto Compartment = getCompartment(sym.compartment, sym.color);
                if (!Compartment) {
                    CommonHAKCAnalysis::getWriter() << "Could find find Compartment " << std::to_string(sym.compartment)
                                                    << " "
                                                    << getColorStringFromValue(ConstantInt::get(i32_type, sym.color))
                                                    << " for Symbol " << sym.name
                                                    << "\n";
                    throw std::exception();
                }
                std::shared_ptr<HAKCSymbol> symbol = std::make_shared<HAKCSymbol>(sym.name, Compartment, File, sym.is_global);
                symbols.insert(symbol);
            }
        }
    }

    std::set<std::shared_ptr<HAKCCompartment>> HAKCSystemInformation::getCompartments(hakc_compartment_id_t ID) {
        std::set<std::shared_ptr<HAKCCompartment>> result;
        for (auto &Compartment : compartments) {
            if (Compartment->getID() == ID) {
                result.insert(Compartment);
            }
        }
        return result;
    }

    std::shared_ptr<HAKCCompartment> HAKCSystemInformation::getCompartment(hakc_compartment_id_t id, sym_color_t color) {
        for (auto Compartment : getCompartments(id)) {
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

    std::set<std::shared_ptr<HAKCSymbol>> HAKCSystemInformation::getSymbols(StringRef name) {
        std::set<std::shared_ptr<HAKCSymbol>> result;
        for (auto &Symbol : symbols) {
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
}// namespace hakc