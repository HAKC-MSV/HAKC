//
// Created by derrick on 8/20/21.
//
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCTransformers/HAKCTransformer.h"
#include "llvm/Support/FileSystem.h"

namespace hakc {

    /**
 * @brief Collective analysis functionality
 * @param debug
 */
    CommonHAKCAnalysis::CommonHAKCAnalysis(bool debug) :
            debug_output(debug) {}

    bool CommonHAKCAnalysis::IsNoTransferFunction(Function *F) {
        auto NoTransferFunctions = GetNoTransferFunctions();
        return NoTransferFunctions.find(F->getName()) != NoTransferFunctions.end();
    }

    bool CommonHAKCAnalysis::IsHAKCTransferFunction(Function *F) {
        if (!F) {
            return false;
        }
        auto TransferDef = GetHAKCTransferDef(F->getName());
        return TransferDef != nullptr;
    }

    hakc_transfer_def_t CommonHAKCAnalysis::GetHAKCTransferDef(StringRef name) {
        for (const auto &it: GetHAKCTransferFunctions()) {
            if (it->GetName() == name) {
                return it;
            }
        }
        return nullptr;
    }

    raw_ostream &CommonHAKCAnalysis::getWriter() {
        return errs();
    }

    bool CommonHAKCAnalysis::IsPointerLikeType(Type *Ty) {
        return Ty->isPointerTy() || Ty->isIntegerTy(64);
    }

    std::set<Intrinsic::ID> CommonHAKCAnalysis::GetBitshiftIntrinsics() {
        return {
                Intrinsic::fshl,
                Intrinsic::fshr,
        };
    }

    std::set<Instruction::BinaryOps> CommonHAKCAnalysis::GetPointerManipulatingBinaryOps() {
        return {
                Instruction::BinaryOps::Add,
                Instruction::BinaryOps::Xor,
                Instruction::BinaryOps::Sub,
                Instruction::BinaryOps::And,
                Instruction::BinaryOps::Or,
        };
    }

    bool CommonHAKCAnalysis::IsCallInIntrinsicSet(CallBase *Call, std::set<Intrinsic::ID> &IntrinsicsSet) const {
        bool result = false;
        if (auto *intrinsic = dyn_cast<IntrinsicInst>(Call)) {
            result = (IntrinsicsSet.find(intrinsic->getIntrinsicID()) != IntrinsicsSet.end());
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Intrinsic (" << intrinsic->getIntrinsicID() << ") from " <<
                                                Call->getFunction()->getName() << " ";
                intrinsic->print(CommonHAKCAnalysis::getWriter());
                if (result) {
                    CommonHAKCAnalysis::getWriter() << " is in { ";
                } else {
                    CommonHAKCAnalysis::getWriter() << " is not in { ";
                }
                for (auto id: IntrinsicsSet) {
                    CommonHAKCAnalysis::getWriter() << id << " ";
                }
                CommonHAKCAnalysis::getWriter() << "}\n";
            }
        }
        return result;
    }

    bool CommonHAKCAnalysis::IsConstantUsedInGlobal(Value *V) {
        bool Result = false;
        if (auto *Const = dyn_cast<Constant>(V)) {
            auto Search = [](User *U) {
                return isa<GlobalVariable>(U) || CommonHAKCAnalysis::IsConstantUsedInGlobal(U);
            };

            Result = llvm::any_of(Const->users(), Search);
        }
        return Result;
    }

    void CommonHAKCAnalysis::PrettyPrintValue(Value *V, raw_ostream &os) {
        if (V == nullptr) {
            os << "!!nullptr!!";
        } else if (auto *F = dyn_cast<Function>(V)) {
            os << "Function " << F->getName();
        } else if (auto *GV = dyn_cast<GlobalVariable>(V)) {
            os << "Global " << GV->getName();
        } else if (auto *Arg = dyn_cast<Argument>(V)) {
            os << "Argument " << Arg->getArgNo() << " of " << Arg->getParent()->getName();
        } else {
            os << *V;
        }
    }

    /**
     * @brief Computes the definition chain from an arbitrary value to its source definition
     * @param v
     * @return The chain of definitions starting from v to the source definition
     */
    std::vector<Value *>
    CommonHAKCAnalysis::findDefChain(Value *v, bool followLoad, bool debug) {
        if (v == nullptr) {
            CommonHAKCAnalysis::getWriter() << "v is null\n";
            throw std::exception();
        }
        if (DefchainCache.find(v) != DefchainCache.end()) {
            auto CachedChain = DefchainCache[v];
            return CachedChain;
        }


        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Getting Def Chain for ";
            CommonHAKCAnalysis::PrettyPrintValue(v, CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        std::set<Value *> working_list = {v};
        std::vector<Value *> def_chain;
        while (!working_list.empty()) {
            Value *curr = *working_list.begin();
            working_list.erase(curr);

            if (DefchainCache.find(curr) != DefchainCache.end()) {
                auto CachedChain = DefchainCache[curr];
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "Adding cached chain for ";
                    CommonHAKCAnalysis::PrettyPrintValue(curr, CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " containing " << std::to_string(CachedChain.size())
                                                    << " links\n";
                }
                for (auto *Link: CachedChain) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "\t" << *Link << "\n";
                    }
                    def_chain.push_back(Link);
                }
                continue;
            }

            if (auto *gep = dyn_cast<GetElementPtrInst>(curr)) {
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "Adding GEP Operator pointer " << *gep->getPointerOperand()
                                                    << "\n";
                }
                working_list.insert(gep->getPointerOperand());
            } else if (auto *BitCastI = dyn_cast<BitCastInst>(curr)) {
                working_list.insert(BitCastI->getOperand(0));
            } else if (auto *call = dyn_cast<CallInst>(curr)) {
                if (call->getCalledFunction() &&
                    IsHAKCTransferFunction(call->getCalledFunction())) {
                    auto TransferDef = GetHAKCTransferDef(call->getCalledFunction()->getName());
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Adding Arg "
                                                        << std::to_string(TransferDef->GetSignedPtrIdx())
                                                        << " of HAKC Transfer " << *call << "\n";
                    }
                    working_list.insert(call->getArgOperand(TransferDef->GetSignedPtrIdx()));
                } else if (call->getCalledFunction() && call->getCalledFunction()->isIntrinsic()) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Call is intrinsic: "
                                                        << call->getCalledFunction()->getName() << "\n";
                    }

                    auto BitshiftIntrinsics = GetBitshiftIntrinsics();
                    if (IsCallInIntrinsicSet(call, BitshiftIntrinsics)) {
                        if (debug) {
                            CommonHAKCAnalysis::getWriter() << "Adding argument 0 of " << *call << "\n";
                        }
                        working_list.insert(call->getArgOperand(0));
                    }
                }
            } else if (auto *gep = dyn_cast<GEPOperator>(curr)) {
                working_list.insert(gep->getPointerOperand());
            } else if (auto *bitcast = dyn_cast<BitCastOperator>(curr)) {
                working_list.insert(bitcast->getOperand(0));
            } else if (auto *cast = dyn_cast<PtrToIntInst>(curr)) {
                working_list.insert(cast->getPointerOperand());
            } else if (auto *cast = dyn_cast<PtrToIntOperator>(curr)) {
                working_list.insert(cast->getPointerOperand());
            } else if (followLoad && isa<LoadInst>(curr)) {
                auto *load = dyn_cast<LoadInst>(curr);
                working_list.insert(load->getPointerOperand());
            } else if (auto *bitcast = dyn_cast<IntToPtrInst>(curr)) {
                working_list.insert(bitcast->getOperand(0));
            } else if (auto *sext = dyn_cast<SExtInst>(curr)) {
                working_list.insert(sext->getOperand(0));
            } else if (auto *binOp = dyn_cast<BinaryOperator>(curr)) {
                auto PointerBinOps = GetPointerManipulatingBinaryOps();
                if (PointerBinOps.find(binOp->getOpcode()) == PointerBinOps.end()) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "BinaryOperator " << *binOp
                                                        << " is not a pointer manipulating binary operation\n";
                    }
                    goto add_to_chain;
                }

                auto *LHSDef = getDef(binOp->getOperand(0), false, debug);
                auto *RHSDef = getDef(binOp->getOperand(1), false, debug);
                if (!isa<Constant>(LHSDef) && ValueIsUsedAsPointer(LHSDef, debug)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Adding LHS Binary Operand ";
                        CommonHAKCAnalysis::PrettyPrintValue(binOp->getOperand(0), CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    working_list.insert(binOp->getOperand(0));
                } else if (!isa<Constant>(RHSDef) && ValueIsUsedAsPointer(RHSDef, debug)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Adding RHS Binary Operand ";
                        CommonHAKCAnalysis::PrettyPrintValue(binOp->getOperand(1), CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    working_list.insert(binOp->getOperand(1));
                } else if (!isa<Constant>(LHSDef) && !isa<Constant>(RHSDef)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Neither LHS nor RHS of " << *binOp << " are constants\n";
                    }
                    /* We stop here */
                    goto add_to_chain;
                } else if (isa<Constant>(LHSDef) && isa<Constant>(RHSDef)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Both LHS and RHS of " << *binOp << " are constants\n";
                    }
                    /* We stop here */
                    goto add_to_chain;
                }
            }
            add_to_chain:
            def_chain.push_back(curr);
        }

        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Returning Def Chain of length " << std::to_string(def_chain.size())
                                            << " for ";
            CommonHAKCAnalysis::PrettyPrintValue(v, CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        DefchainCache[v] = def_chain;
        return def_chain;
    }

    bool CommonHAKCAnalysis::ValueIsUsedAsPointer(Value *V, bool debug) {
        if (!IsPointerLikeType(V->getType())) {
            return false;
        }

        bool CallIsUsedAsPointer = V->getType()->isPointerTy();
        if (V->getType()->isIntegerTy()) {
            CallIsUsedAsPointer = false;
            /* Search for uses that determine if the call is considered a pointer or integer */
            for (auto &U: V->uses()) {
                if (isa<IntToPtrInst>(U.getUser())) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "User of " << *V << " is an inttoptr: " << *U.getUser()
                                                        << "\n";
                    }
                    CallIsUsedAsPointer = true;
                } else if (auto *BinOp = dyn_cast<BinaryOperator>(U.getUser())) {
                    if (BinOp->getOpcode() == BinaryOperator::Add) {
                        unsigned OpNum = (U.getOperandNo() + 1) % 2;
                        auto *OtherOp = U.getUser()->getOperand(OpNum);
                        if (debug) {
                            CommonHAKCAnalysis::getWriter() << "Checking operator " << std::to_string(OpNum)
                                                            << " of " << *BinOp << ": " << *OtherOp << "\n";
                        }
                        if (OtherOp->getType()->isPointerTy()) {
                            /* V is an integer (which could still be used as a pointer), but is used in an add operation
                             * that involves another pointer.  Adding two pointers together does not make sense, so V
                             * is a true integer and not a pointer.
                             */
                            break;
                        }
                    }
                }

                if (CallIsUsedAsPointer) {
                    break;
                }
            }
        }

        return CallIsUsedAsPointer;
    }

    /**
     * @brief Returns the source definition of a Value
     * @param V
     * @return
     */
    Value *CommonHAKCAnalysis::getDef(Value *V, bool followLoad, bool debug) {
        std::vector<Value *> def_chain = findDefChain(V, followLoad, debug);
        if (def_chain.empty()) {
            CommonHAKCAnalysis::getWriter() << "Def Chain for ";
            V->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " is empty!\n";
            throw std::exception();
        }
        return def_chain.back();
    }

    /**
     * @brief Returns true if the called function is in the list of safe transition calls defined above
     * @param call
     * @return
     */
    bool CommonHAKCAnalysis::callIsSafeTransition(CallBase *call) {
        if (call->getCalledFunction()) {
            return isSafeTransitionFunction(call->getCalledFunction());
        }

        return false;
    }

    /**
 * @brief
 * @param F
 * @return true if #F name is in #hakc_functions or
 * #hakc_transfer_funcs, false otherwise
 * */
    bool CommonHAKCAnalysis::isHAKCFunction(Function *F) {
        auto Search = [F](const hakc_function_def_t &Func) {
            return Func->GetName() == F->getName();
        };

        auto HAKCFunctions = GetHAKCFunctions();
        return std::any_of(HAKCFunctions.begin(), HAKCFunctions.end(), Search);
    }

    /**
 * @brief
 * @param F
 * @return true if F->getName() is in #safe_transition_functions, false
 * otherwise
 */
    bool CommonHAKCAnalysis::isSafeTransitionFunction(Function *F) {
        auto SafeTransitionFunctions = GetSafeTransitionFunctions();
        auto AllocationFunctions = GetKernelAllocationSizeMap();
        return (SafeTransitionFunctions.find(F->getName()) !=
                SafeTransitionFunctions.end() ||
                AllocationFunctions.find(F->getName()) !=
                AllocationFunctions.end());
    }

    bool CommonHAKCAnalysis::IsMultiSSAUser(Value *V) {
        return isa<PHINode>(V) || isa<SelectInst>(V) || isa<BinaryOperator>(V);
    }

    bool CommonHAKCAnalysis::valueHasAttribute(Value *V, Attribute::AttrKind Kind) {
        bool result = false;
        if (auto *gv = dyn_cast<GlobalVariable>(V)) {
            result = gv->hasAttribute(Kind);
        } else if (auto *arg = dyn_cast<Argument>(V)) {
            /* A pointer to a pointer is used by the kernel to allow setting
             * the value of a user pointer. See ___sys_recvmsg in net/socket.c
             */
            result = arg->hasAttribute(Kind) &&
                     !arg->getType()->getPointerElementType()->isPointerTy();
        } else if (auto *I = dyn_cast<Instruction>(V)) {
            auto *metadata = I->getMetadata(LLVMContext::MD_annotation);
            if (metadata) {
                auto attrName = Attribute::getNameFromAttrKind(Kind);
                if (attrName.empty()) {
                    CommonHAKCAnalysis::getWriter() << "Invalid AttrKind name for value " << std::to_string(Kind)
                                                    << "\n";
                    throw std::exception();
                }
                for (auto &operand: metadata->operands()) {
                    if (auto *mdstring = dyn_cast<MDString>(operand.get())) {
                        if (mdstring->getString() == attrName) {
                            result = true;
                            break;
                        }
                    }
                }
            }
            if (!result && isa<CallInst>(V)) {
                auto *call = cast<CallInst>(V);
                if (call->getCalledFunction()) {
                    Function *func = call->getCalledFunction();
                    result = func->hasFnAttribute(Attribute::PerCPUPtr);
                }
            }
        }

        return result;
    }

    bool CommonHAKCAnalysis::useHasAttribute(Use &U, Attribute::AttrKind Kind) {
        bool result = valueHasAttribute(U.get(), Kind);
        return result;
    }

    bool CommonHAKCAnalysis::isIgnoredType(Type *Ty) {
        if (Ty->isStructTy()) {
            auto *StructTy = dyn_cast<StructType>(Ty);
            if (!StructTy->isLiteral()) {
                auto IgnoredTypes = GetIgnoredTypes();
                return IgnoredTypes.find(Ty->getStructName()) != IgnoredTypes.end();
            }
        } else if (Ty->isPointerTy()) {
            return isIgnoredType(Ty->getPointerElementType());
        }

        return false;
    }

    bool CommonHAKCAnalysis::IsIgnoredGlobal(Value *V) {
        bool Result = false;
        if (auto *GV = dyn_cast<GlobalValue>(V)) {
            auto IgnoredGlobals = GetIgnoredGlobals();
            Result = IgnoredGlobals.find(GV->getName()) != IgnoredGlobals.end();
        }

        return Result;
    }

    bool CommonHAKCAnalysis::isKernelUserPointer(Use &U) {
        return useHasAttribute(U, Attribute::KernelUserPtr);
    }

    bool CommonHAKCAnalysis::isPerCPUPointer(Use &U) {
        return useHasAttribute(U, Attribute::PerCPUPtr);
    }

    bool CommonHAKCAnalysis::isPerCPUPointer(Value *V) {
        return valueHasAttribute(V, Attribute::PerCPUPtr);
    }

    bool CommonHAKCAnalysis::isKernelUserPointer(Value *V) {
        return valueHasAttribute(V, Attribute::KernelUserPtr);
    }

    bool CommonHAKCAnalysis::isFunctionStatic(Function *F) {
        return Function::isLocalLinkage(F->getLinkage()) || F->isDeclaration();
    }

    bool CommonHAKCAnalysis::FunctionHasPointerArg(Function *F) {
        bool ArgumentsContainPointer = false;
        for (auto &Arg: F->args()) {
            if (Arg.getType()->isPointerTy()) {
                ArgumentsContainPointer = true;
                break;
            }
        }

        return ArgumentsContainPointer;
    }

    bool CommonHAKCAnalysis::valueShouldBeReplacedWithTransfer(Value *V) {
        if (auto *F = dyn_cast<Function>(V)) {
            return functionIsTransferCandidate(F);
        } else if (auto *BCO = dyn_cast<BitCastOperator>(V)) {
            return valueShouldBeReplacedWithTransfer(BCO->getOperand(0));
        }
        return false;
    }

    bool CommonHAKCAnalysis::IsHAKCFunction(Function *F) {
        if (F == nullptr) {
            return false;
        }
        auto HAKCFuncDef = getHAKCFunction(F->getName());
        return HAKCFuncDef != nullptr;
    }

    hakc_function_def_t CommonHAKCAnalysis::getHAKCFunction(StringRef name) {
        for (auto HAKCFuncDef: GetHAKCFunctions()) {
            if (HAKCFuncDef->GetName() == name) {
                return HAKCFuncDef;
            }
        }
        return nullptr;
    }

    bool CommonHAKCAnalysis::isOutsideTransferFunc(Function *F) {
        return (F->getName().startswith(OUTSIDE_TRANSFER_PREFIX));
    }

    bool CommonHAKCAnalysis::isCapabilityReassignmentFunc(Function *F) {
        return F->getName().startswith(CAPABILITY_REASSIGNMENT_PREFIX);
    }

    bool CommonHAKCAnalysis::IsCompartmentalizedFunction(Function *F) {
        return !IsKernelFunction(F) && !isOutsideTransferFunc(F);
    }

    std::string CommonHAKCAnalysis::getOutsideTransferName(Function *F) {
        auto NoTransferFunctions = GetNoTransferFunctions();
        if (F->getName().startswith(OUTSIDE_TRANSFER_PREFIX) ||
            NoTransferFunctions.find(F->getName()) != NoTransferFunctions.end()) {
            return F->getName().str();
        }
        std::string name = OUTSIDE_TRANSFER_PREFIX.str();
        name += F->getName().str();
        return name;
    }

    std::string CommonHAKCAnalysis::getVariadicTransferName(Function *F) {
        std::string VariadicTransferName = VARIADIC_TRANSFER_PREFIX.str();
        VariadicTransferName += F->getName();
        return VariadicTransferName;
    }

    std::string CommonHAKCAnalysis::getOriginalTransformedName(Function *F) {
        std::string TransformedName = ORIGINAL_FUNCTION_PREFIX.str();
        TransformedName += F->getName();
        return TransformedName;
    }

    bool CommonHAKCAnalysis::functionIsModParamGetCtx(Function *F) {
        return F->getName().startswith(MODPARAM_GETCTX_PREFIX);
    }

    bool CommonHAKCAnalysis::functionIsTransferCandidate(Function *f) {
        auto NoTransferFuncs = GetNoTransferFunctions();
        return NoTransferFuncs.find(f->getName()) == NoTransferFuncs.end() &&
               !f->isDeclaration() &&
               !isCapabilityReassignmentFunc(f) &&
               !FunctionIsComplexVariadic(f) &&
               !functionIsModParamGetCtx(f) &&
               FunctionHasPointerArg(f) &&
               (!isOutsideTransferFunc(f) ||
                !f->hasFnAttribute(Attribute::InlineHint));
    }

    bool CommonHAKCAnalysis::FunctionIsComplexVariadic(Function *F) {
        return F->isVarArg();
    }

    bool CommonHAKCAnalysis::isRegisterRead(Value *v) {
        if (auto *call = dyn_cast<CallInst>(v)) {
            return call->isInlineAsm() || (call->getCalledFunction() &&
                                           call->getCalledFunction()->isIntrinsic() &&
                                           call->getCalledFunction()->getIntrinsicID() ==
                                           Intrinsic::IndependentIntrinsics::read_register);
        }
        return false;
    }

    Module &CommonHAKCAnalysis::getModule() {
        return getTransformer().getModule();
    }

    std::set<StringRef> CommonHAKCAnalysis::GetIgnoredGlobals() {
        return {};
    }

    bool CommonHAKCAnalysis::functionIsAnalysisCandidate(Function *F) {
        if (!F) {
            return true;
        }
        if (isSafeTransitionFunction(F)) {
            return false;
        }
        if (isHAKCFunction(F)) {
            return false;
        }
        if (F->getName() == "printk") {
            return false;
        }
        if (isOutsideTransferFunc(F)) {
            return false;
        }
        if (F->isIntrinsic()) {
            return false;
        }
        return true;
    }

    bool CommonHAKCAnalysis::argShouldTransfer(Value *V) {
        if (auto *Arg = dyn_cast<Argument>(V)) {
            if (Arg->hasAttribute(llvm::Attribute::ReadNone)) {
                return false;
            }
        }

        return V->getType()->isPointerTy() && !isa<FunctionType>(V->getType()->getPointerElementType()) &&
               !isa<ConstantPointerNull>(V) && !isKernelUserPointer(V);
    }

    bool CommonHAKCAnalysis::valueIsReadonlyPtr(Value *value) {
        Type *Ty = value->getType();
        if (auto *Call = dyn_cast<CallInst>(value)) {
            if (Call->getCalledFunction()) {
                Ty = Call->getCalledFunction()->getFunctionType()->getReturnType();
            }
        }
        bool result = isa<PointerType>(Ty) &&
                      isa<FunctionType>(Ty->getPointerElementType());
        return result;
    }

    bool CommonHAKCAnalysis::NoKernelTransferFunctionsSet() {
        const char *env = std::getenv(HAKC_NO_KERNEL_TRANSFERS.str().c_str());
        return env != nullptr;
    }

    bool CommonHAKCAnalysis::IsKernelCompartment(hakc_compartment_id_t ID) {
        return ID == KERNEL_COMPARTMENT;
    }

    bool CommonHAKCAnalysis::IsKernelFunction(Function *F) {
        return IsKernelSymbol(F);
    }

    bool CommonHAKCAnalysis::IsKernelSymbol(GlobalValue *GV) {
        if(!GV) {
            return false;
        }

        hakc_compartment_id_t CompartmentID;
        if(auto *F = dyn_cast<Function>(GV)) {
            CompartmentID = getTransformer().getFunctionCompartmentID(F);
        } else if(auto *GlobVar = dyn_cast<GlobalVariable>(GV)) {
            CompartmentID = getTransformer().getGlobalCompartmentID(GlobVar);
        } else {
            return false;
        }

        return CommonHAKCAnalysis::IsKernelCompartment(CompartmentID);
    }

    std::string CommonHAKCAnalysis::getHAKCDebugName() {
        const char *name = std::getenv(HAKC_DEBUG_ENV_VAR.str().c_str());
        if (name == nullptr) {
            name = "****UNUSED****";
        }
        return name;
    }

    std::set<StringRef> CommonHAKCAnalysis::AddToSet(std::set<StringRef> Existing, ArrayRef<StringRef> NewAdditions) {
        for (auto NewAddition: NewAdditions) {
            Existing.insert(NewAddition);
        }

        return Existing;
    }

    std::set<StringRef>
    CommonHAKCAnalysis::AddToSet(std::set<StringRef> Existing, const std::set<StringRef> &NewAdditions) {
        Existing.insert(NewAdditions.begin(), NewAdditions.end());
        return Existing;
    }

    bool CommonHAKCAnalysis::FunctionsAreInSameCompartment(Function *F, Function *G) {
        return getTransformer().getFunctionCompartmentID(F) == getTransformer().getFunctionCompartmentID(G);
    }

    bool CommonHAKCAnalysis::IsKernelAllocation(Value *V) {
        V = getDef(V, false, false);
        auto AllocationDefinitions = GetKernelAllocationSizeMap();
        if (auto *call = dyn_cast<CallInst>(V)) {
            if (call->getCalledFunction() &&
                AllocationDefinitions.find(call->getCalledFunction()->getName()) != AllocationDefinitions.end()) {
                return true;
            }
        }
        return false;
    }

    unsigned CommonHAKCAnalysis::getCompartmentStorageSizeInBits() {
#if defined(HAKC_CHERIBSD_MORELLO)
        return 128;
#else
        return 64;
#endif
    }

    StringRef CommonHAKCAnalysis::GetFunctionName(Function *F) {
        /* The compiler will sometimes rename functions when directed to, especially for
         * kernel modules, in order to facilitate general functionality.  However,
         * the debug symbols maintain the original name, so use that name if it is available */
        if (F->getSubprogram() && !F->getSubprogram()->getName().empty()) {
            return F->getSubprogram()->getName();
        }

        return F->getName();
    }

    bool CommonHAKCAnalysis::IsStringType(Type *Ty) {
        return Ty->isArrayTy() && Ty->getArrayElementType()->isIntegerTy(8);
    }

    Instruction *CommonHAKCAnalysis::GetTargetTypeCast(Instruction *I, Type *TargetType) {
        if (I->getType() == TargetType) {
            return I;
        }

        for (auto *U: I->users()) {
            if (auto *BitCastI = dyn_cast<BitCastInst>(U)) {
                if (BitCastI->getDestTy() == TargetType) {
                    return BitCastI;
                }
            }
        }

        return nullptr;
    }

    std::string CommonHAKCAnalysis::GetModuleFullPath(Module &M) {
        const auto &SourceFileName = M.getSourceFileName();
        SmallString<256> FilenameVec = StringRef(SourceFileName);
        SmallString<256> RealPath;

        auto err = sys::fs::real_path(FilenameVec, RealPath, true);
        if (err) {
            CommonHAKCAnalysis::getWriter() << "Could not get real path to " << M.getSourceFileName() << "\n";
            throw std::exception();
        }
        return RealPath.str().str();
    }
}// namespace hakc
