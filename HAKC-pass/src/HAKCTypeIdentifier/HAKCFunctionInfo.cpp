//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include "HAKCTypeIdentifier/HAKCHash.h"
#include "llvm/IR/InstIterator.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

#include <sstream>

namespace hakc {
    HAKCFunctionInfo::HAKCFunctionInfo(Function *F, HAKCTypeIdentifier &identifier) :
            HAKCGlobalInfo(F, identifier, F->getSubprogram()->getDirectory(), F->getSubprogram()->getFilename(),
                           F->getSubprogram()->getLine()),
            indirectCalls(), directCalls() {
        for (auto it = inst_begin(F); it != inst_end(F); ++it) {
            Instruction *inst = &*it;
            if (auto *call = dyn_cast<CallInst>(inst)) {
                addCall(call);
            }
        }
    }

    void HAKCFunctionInfo::addEscapingMemberOffset(CallInst *call) {
        if (!call->getCalledFunction()) {
            return;
        }

        for (auto &arg: call->args()) {
            if (isa<PointerType>(arg->getType())) {
                auto defChain = identifier.GetDefChain(arg.get());
                for (auto *V: defChain) {
                    if (auto *gep = dyn_cast<GetElementPtrInst>(V)) {
                        APInt offset(64, 0, true);
                        if (gep->accumulateConstantOffset(call->getModule()->getDataLayout(),
                                                          offset)) {
                            auto hakcType = identifier.getHAKCType(gep->getSourceElementType());
                            if (!hakcType) {
                                if (identifier.debugActive()) {
                                    CommonHAKCAnalysis::getWriter() << "Could not find HAKCTypeInfo for escaping "
                                                                    << "symbol: ";
                                    arg->print(CommonHAKCAnalysis::getWriter());
                                    CommonHAKCAnalysis::getWriter() << " in function " << call->getFunction()->getName()
                                                                    << "\n";
                                }
                                break;
                            }
                            hakcType->addSubmemberEscape(
                                    call->getCalledFunction()->getName().str(),
                                    offset.getSExtValue());
                        }
                    }
                }
            }
        }
    }

    void HAKCFunctionInfo::addCall(CallInst *call) {
        if (call->isInlineAsm()) {
            return;
        }

        if (call->getCalledFunction()) {
            if (call->getCalledFunction()->isIntrinsic()) {
                return;
            }
            directCalls.insert(call->getCalledFunction()->getName().str());
            addEscapingMemberOffset(call);
        } else {
            if (identifier.debugActive()) {
                CommonHAKCAnalysis::getWriter() << "Call ";
                call->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " is an indirect call\n";
                CommonHAKCAnalysis::getWriter() << "CalledOperand Type: ";
                call->getCalledOperand()->getType()->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }

            auto TypeHash = HAKCTypeInfo::hashType(call->getCalledOperand()->getType());
            auto origin = identifier.getCallOperandOrigin(call);
            indirectCalls.insert(
                    std::make_pair(TypeHash, origin));
        }
    }

    std::string HAKCFunctionInfo::getYaml() {
        std::stringstream out;
        out << HAKCGlobalInfo::getYaml();
        out << "  direct-calls:\n";
        for (auto &call: directCalls) {
            out << "    - " << call << "\n";
        }
        out << "  indirect-calls:\n";
        for (auto &call: indirectCalls) {
            out << "    -\n";
            out << "      type: " << call.first << "\n";
            out << "      origin: " << call.second << "\n";
        }
        return out.str();
    }

} // hakc