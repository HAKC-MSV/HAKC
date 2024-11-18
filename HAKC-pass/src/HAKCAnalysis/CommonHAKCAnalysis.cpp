//
// Created by derrick on 8/20/21.
//
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"

namespace hakc {
    HAKCOstream hos;

    bool CommonHAKCAnalysis::IsNoTransferFunction(Function *F) {
        return IsFunctionInFunctionList(F, SystemInfo.GetNoTransferFunctions());
    }

    bool CommonHAKCAnalysis::IsFunctionInFunctionList(Function *F, iterator_range<FunctionList::iterator> Range) {
        if (!F) {
            return false;
        }

        auto Search = [F](Function *Func) {
            return F == Func;
        };
        return llvm::any_of(Range, Search);
    }

    bool
    CommonHAKCAnalysis::IsFunctionInHAKCFunctionList(Function *F, iterator_range<HAKCFunctionList::iterator> Range) {
        if (!F) {
            return false;
        }

        auto Search = [F](hakc_function_def_t &Func) {
            return F == Func->GetFunction();
        };
        return llvm::any_of(Range, Search);
    }

    bool CommonHAKCAnalysis::IsFunctionInHAKCTransferFunctionList(Function *F,
                                                                  iterator_range<HAKCTransferList::iterator> Range) {
        if (!F) {
            return false;
        }

        auto Search = [F](hakc_transfer_def_t &Func) {
            return F == Func->GetFunction();
        };
        return llvm::any_of(Range, Search);
    }

    HAKCSystemInformation &CommonHAKCAnalysis::GetSystemInfo() {
        return SystemInfo;
    }

    CommonHAKCAnalysis::CommonHAKCAnalysis(Module &M, StringRef ConfigPath) : SystemInfo(M) {
        if (!sys::fs::exists(ConfigPath)) {
            CommonHAKCAnalysis::getWriter() << "Could not find YAML file " << ConfigPath << "\n";
            throw std::exception();
        } else if (!sys::fs::is_regular_file(ConfigPath)) {
            CommonHAKCAnalysis::getWriter() << ConfigPath << " is not a regular file\n";
            throw std::exception();
        }

        HAKCYamlConfig SystemConfig;
        ErrorOr<std::unique_ptr<MemoryBuffer>> mb = MemoryBuffer::getFile(ConfigPath);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        // yaml is actually parsed here, for some reason
        yin >> SystemConfig;
        if (yin.error()) {
            CommonHAKCAnalysis::getWriter() << "Error parsing config file " << ConfigPath << "\n";
            throw std::exception();
        }

        SystemInfo << SystemConfig;
    }

    bool CommonHAKCAnalysis::IsHAKCTransferFunction(Function *F) {
        return IsFunctionInHAKCTransferFunctionList(F, SystemInfo.CompartmentTransferFunctions());
    }

    bool CommonHAKCAnalysis::IsHAKCValidationFunction(Function *F) {
        return IsFunctionInHAKCFunctionList(F, SystemInfo.CompartmentalizationValidationFunctions());
    }

    bool CommonHAKCAnalysis::IsHAKCCompartmentalizationSupportFunction(Function *F) {
        return IsFunctionInFunctionList(F, SystemInfo.CompartmentalizationSupportFunctions());
    }

    hakc::HAKCOstream &CommonHAKCAnalysis::getWriter() {
        return hos;
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
            if (SystemInfo.OutputDebugInfo(Call->getFunction())) {
                CommonHAKCAnalysis::getWriter() << "Intrinsic (" << intrinsic->getIntrinsicID() << ") from " <<
                                                Call->getFunction()->getName() << " " << intrinsic;
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
            CommonHAKCAnalysis::getWriter() << "Getting Def Chain for " << v << "\n";
        }

        std::set<Value *> working_list = {v};
        std::vector<Value *> def_chain;
        while (!working_list.empty()) {
            Value *curr = *working_list.begin();
            working_list.erase(curr);

            if (DefchainCache.find(curr) != DefchainCache.end()) {
                auto CachedChain = DefchainCache[curr];
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "Adding cached chain for " << curr << " containing "
                                                    << std::to_string(CachedChain.size())
                                                    << " links\n";
                }
                for (auto *Link: CachedChain) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "\t" << Link << "\n";
                    }
                    def_chain.push_back(Link);
                }
                continue;
            }

            if (auto *gep = dyn_cast<GetElementPtrInst>(curr)) {
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "Adding GEP Operator pointer " << gep->getPointerOperand()
                                                    << "\n";
                }
                working_list.insert(gep->getPointerOperand());
            } else if (auto *BitCastI = dyn_cast<BitCastInst>(curr)) {
                working_list.insert(BitCastI->getOperand(0));
            } else if (auto *call = dyn_cast<CallInst>(curr)) {
                if (call->getCalledFunction() &&
                    IsHAKCTransferFunction(call->getCalledFunction())) {
                    auto TransferDef = SystemInfo.GetCustomAllocation(call->getCalledFunction());
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Adding Arg "
                                                        << std::to_string(TransferDef->GetSignedPtrIdx())
                                                        << " of HAKC Transfer " << call << "\n";
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
                            CommonHAKCAnalysis::getWriter() << "Adding argument 0 of " << call << "\n";
                        }
                        working_list.insert(call->getArgOperand(0));
                    }
                }
            } else if (auto *GEPOp = dyn_cast<GEPOperator>(curr)) {
                working_list.insert(GEPOp->getPointerOperand());
            } else if (auto *BitcastOp = dyn_cast<BitCastOperator>(curr)) {
                working_list.insert(BitcastOp->getOperand(0));
            } else if (auto *PtrToIntI = dyn_cast<PtrToIntInst>(curr)) {
                working_list.insert(PtrToIntI->getPointerOperand());
            } else if (auto *PtrToIntOp = dyn_cast<PtrToIntOperator>(curr)) {
                working_list.insert(PtrToIntOp->getPointerOperand());
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
                        CommonHAKCAnalysis::getWriter() << "BinaryOperator " << binOp
                                                        << " is not a pointer manipulating binary operation\n";
                    }
                    goto add_to_chain;
                }

                auto *LHSDef = getDef(binOp->getOperand(0), false, debug);
                auto *RHSDef = getDef(binOp->getOperand(1), false, debug);
                if (!isa<Constant>(LHSDef) && ValueIsUsedAsPointer(LHSDef, debug)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Adding LHS Binary Operand " << binOp->getOperand(0) << "\n";
                    }
                    working_list.insert(binOp->getOperand(0));
                } else if (!isa<Constant>(RHSDef) && ValueIsUsedAsPointer(RHSDef, debug)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Adding RHS Binary Operand " << binOp->getOperand(1) << "\n";
                    }
                    working_list.insert(binOp->getOperand(1));
                } else if (!isa<Constant>(LHSDef) && !isa<Constant>(RHSDef)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Neither LHS nor RHS of " << binOp << " are constants\n";
                    }
                    /* We stop here */
                    goto add_to_chain;
                } else if (isa<Constant>(LHSDef) && isa<Constant>(RHSDef)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Both LHS and RHS of " << binOp << " are constants\n";
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
                                            << " for " << v << "\n";
        }
        DefchainCache[v] = def_chain;
        return def_chain;
    }

    /**
     * @brief Returns the source definition of a Value
     * @param V
     * @return
     */
    Value *CommonHAKCAnalysis::getDef(Value *V, bool followLoad, bool debug) {
        std::vector<Value *> def_chain = findDefChain(V, followLoad, debug);
        if (def_chain.empty()) {
            CommonHAKCAnalysis::getWriter() << "Def Chain for " << V << " is empty!\n";
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
            return IsSafeTransitionFunction(call->getCalledFunction());
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
        return IsHAKCTransferFunction(F) || IsHAKCCompartmentalizationSupportFunction(F) || IsHAKCValidationFunction(F);
    }

    /**
 * @brief
 * @param F
 * @return true if F->getName() is in #safe_transition_functions, false
 * otherwise
 */
    bool CommonHAKCAnalysis::IsSafeTransitionFunction(Function *F) {
        return IsFunctionInFunctionList(F, SystemInfo.SafeTransitionFunctions());
/*        auto SafeTransitionFunctions = GetSafeTransitionFunctions();
        auto AllocationFunctions = GetKernelAllocationSizeMap();
        return (SafeTransitionFunctions.find(F->getName()) !=
                SafeTransitionFunctions.end() ||
                AllocationFunctions.find(F->getName()) !=
                AllocationFunctions.end());*/
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
            result = arg->hasAttribute(Kind) /*&&
                     !arg->getType()->getPointerElementType()->isPointerTy()*/;
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
                    result = func->hasFnAttribute(Kind);
                }
            }
        }

        return result;
    }

    bool CommonHAKCAnalysis::useHasAttribute(Use &U, Attribute::AttrKind Kind) {
        bool result = valueHasAttribute(U.get(), Kind);
        return result;
    }

    bool CommonHAKCAnalysis::IsIgnoredType(Type *Ty) {
        if (!Ty) {
            return false;
        }

        auto Search = [Ty](Type *T) {
            return Ty == T;
        };
        return llvm::any_of(SystemInfo.IgnoredTypes(), Search);
    }

    bool CommonHAKCAnalysis::IsIgnoredGlobal(Value *V) {
        bool Result = false;
        if (auto *GV = dyn_cast<GlobalVariable>(V)) {
            auto Search = [GV](GlobalVariable *G) {
                return GV == G;
            };
            return llvm::any_of(SystemInfo.IgnoredGlobals(), Search);
        }

        return Result;
    }

    bool CommonHAKCAnalysis::isPerCPUPointer(Value *V) {
//        return valueHasAttribute(V, Attribute::PerCPUPtr);
        // TODO: Fix this when attributes are added in again
        return false;
    }

    bool CommonHAKCAnalysis::isKernelUserPointer(Value *V) {
//        return valueHasAttribute(V, Attribute::KernelUserPtr);
        // TODO: Fix this when attributes are added in again
        return false;
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

    bool CommonHAKCAnalysis::valueShouldBeReplacedWithTransfer(Value *V, HAKCCompartmentalizationPolicy &Policy) {
        if (auto *F = dyn_cast<Function>(V)) {
            return functionIsTransferCandidate(F, Policy);
        } else if (auto *BCO = dyn_cast<BitCastOperator>(V)) {
            return valueShouldBeReplacedWithTransfer(BCO->getOperand(0), Policy);
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
        return (F->getName().starts_with(OUTSIDE_TRANSFER_PREFIX));
    }

    bool CommonHAKCAnalysis::isCapabilityReassignmentFunc(Function *F) {
        return F->getName().starts_with(CAPABILITY_REASSIGNMENT_PREFIX);
    }

    void CommonHAKCAnalysis::VerifyFunction(Function *F) {
        if (llvm::verifyFunction(*F, &CommonHAKCAnalysis::getWriter().GetOS())) {
            CommonHAKCAnalysis::getWriter() << "Verification failed for function\n" << F << "\n";
            throw std::exception();
        }
    }

    FunctionType *CommonHAKCAnalysis::GetDataAuthenticationFunctionType(Module &M, unsigned AddrSpace) {
        auto *RetTy = PointerType::get(M.getContext(), AddrSpace);
        Type *ArgTy[] = {
                PointerType::get(M.getContext(), AddrSpace),
                IntegerType::get(M.getContext(), 64),
                IntegerType::get(M.getContext(), 64)
        };

        return FunctionType::get(RetTy, ArgTy, false);
    }

    FunctionType *CommonHAKCAnalysis::GetTransferFunctionType(Module &M, unsigned int AddrSpace) {
        auto *RetTy = PointerType::get(M.getContext(), AddrSpace);
        Type *ArgTy[] = {
                PointerType::get(M.getContext(), AddrSpace),
                IntegerType::get(M.getContext(), 64),
                IntegerType::get(M.getContext(), 64)
        };

        return FunctionType::get(RetTy, ArgTy, false);
    }

    FunctionType *CommonHAKCAnalysis::GetCodeAuthenticationFunctionType(Module &M, unsigned AddrSpace) {
        auto *RetTy = PointerType::get(M.getContext(), AddrSpace);
        Type *ArgTy[] = {
                PointerType::get(M.getContext(), AddrSpace),
                IntegerType::get(M.getContext(), 64),
                IntegerType::get(M.getContext(), 64)
        };

        return FunctionType::get(RetTy, ArgTy, false);
    }

    bool CommonHAKCAnalysis::IsCompartmentalizedFunction(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        return !IsKernelSymbol(F, Policy) && !isOutsideTransferFunc(F);
    }

    std::string CommonHAKCAnalysis::GetOutsideTransferName(Function *F) {
        if (F->getName().starts_with(OUTSIDE_TRANSFER_PREFIX) || IsNoTransferFunction(F)) {
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
        return F->getName().starts_with(MODPARAM_GETCTX_PREFIX);
    }

    bool CommonHAKCAnalysis::functionIsTransferCandidate(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        auto Division = Policy.GetDivision(F);
        return !IsNoTransferFunction(F) &&
               !Division.GetHAKCCompartment().IsKernelCompartment() &&
               !F->isDeclaration() &&
               !isCapabilityReassignmentFunc(F) &&
               !FunctionIsComplexVariadic(F) &&
               !functionIsModParamGetCtx(F) &&
               FunctionHasPointerArg(F) &&
               (!isOutsideTransferFunc(F) ||
                !F->hasFnAttribute(Attribute::InlineHint));
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

    bool CommonHAKCAnalysis::FunctionIsAnalysisCandidate(Function *F) {
        if (IsSafeTransitionFunction(F)) {
            return false;
        }
        if (IsHAKCFunction(F)) {
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

        return V->getType()->isPointerTy() && !isa<FunctionType>(V->getType()) &&
               !isa<ConstantPointerNull>(V) && !isKernelUserPointer(V);
    }

    bool CommonHAKCAnalysis::valueIsReadonlyPtr(Value *value) {
        Type *Ty = value->getType();
        if (auto *Call = dyn_cast<CallInst>(value)) {
            Ty = Call->getFunctionType()->getReturnType();
        }
        bool result = isa<FunctionType>(Ty);
        return result;
    }

    bool CommonHAKCAnalysis::NoKernelTransferFunctionsSet() {
        return !HAKC_NO_KERNEL_TRANSFERS.empty();
    }

    void CommonHAKCAnalysis::SortGlobalList(std::vector<GlobalVariable *> &GlobalList) {
        llvm::sort(GlobalList.begin(), GlobalList.end(),
                   [](GlobalVariable *LHS, GlobalVariable *RHS) {
                       return LHS->getName().str() < RHS->getName().str();
                   });
    }

    void CommonHAKCAnalysis::SortFunctionList(std::vector<Function *> &FuncList) {
        llvm::sort(FuncList.begin(), FuncList.end(),
                   [](Function *LHS, Function *RHS) { return LHS->getName().str() < RHS->getName().str(); });
    }

    bool CommonHAKCAnalysis::IsKernelSymbol(GlobalValue *GV, HAKCCompartmentalizationPolicy &Policy) {
        auto Division = Policy.GetDivision(GV);
        return Division.GetHAKCCompartment().IsKernelCompartment();
    }

    std::string CommonHAKCAnalysis::getHAKCDebugName() {
        const char *name = HAKC_DEBUG_NAME.c_str();
        if (strlen(name) == 0) {
            name = "****UNUSED****";
        }
        return name;
    }

    bool CommonHAKCAnalysis::FunctionsAreInSameCompartment(Function *F, Function *G,
                                                           HAKCCompartmentalizationPolicy &Policy) {
        auto FCompartment = Policy.GetDivision(F).GetHAKCCompartment();
        auto GCompartment = Policy.GetDivision(G).GetHAKCCompartment();
        return FCompartment == GCompartment;
    }

    bool CommonHAKCAnalysis::IsKernelAllocation(Value *V) {
        V = getDef(V, false, false);
        auto AllocationDefinitions = GetKernelAllocationSizeMap();
        if (auto *call = dyn_cast<CallInst>(V)) {
            if (call->getCalledFunction() &&
                // AllocationDefinitions.find(call->getCalledFunction()->getName()) != AllocationDefinitions.end()) {
                AllocationDefinitions.find(call->getCalledFunction()->getName().str()) != AllocationDefinitions.end()) {
                return true;
            }
        }
        return false;
    }

    unsigned CommonHAKCAnalysis::getCompartmentStorageSizeInBits() {
        // TODO: Add this to SystemInformation
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
