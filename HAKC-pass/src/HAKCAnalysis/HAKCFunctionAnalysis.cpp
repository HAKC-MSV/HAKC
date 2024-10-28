//
// Created by derrick on 8/20/21.
//
#include "llvm/IR/Verifier.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InlineAsm.h"

#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCAnalysis/ManagedHAKCPointer.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

namespace hakc {

    HAKCFunctionAnalysis::HAKCFunctionAnalysis(Function *F, HAKCCompartmentalizationPolicy &Policy, bool debug)
            : CommonHAKCAnalysis(debug),
              PointerManager(this, Policy, debug),
              DTree(*F),
              CurrentFunction(F),
              SetupHasRun(false),
              CompartmentTransferCount(0) {
    }

    void HAKCFunctionAnalysis::UpdateHAKCFunctionParameters(HAKCCompartmentalizationPolicy &Policy) {
        if (CommonHAKCAnalysis::IsKernelSymbol(CurrentFunction, Policy)) {
            return;
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Updating parameters for the following HAKC functions:\n";
            for (auto *CallI: HAKCFunctionCalls) {
                CommonHAKCAnalysis::getWriter() << CallI << "\n";
            }
        }

        auto *F = &getFunction();
        auto *TransferTarget = F;
        if (isOutsideTransferFunc(F)) {
            auto transferTargetName = F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
            TransferTarget = F->getParent()->getFunction(transferTargetName);
        }
        auto TargetCompartment = Policy.GetCompartment(TransferTarget);

        for (auto *CallI: HAKCFunctionCalls) {
            auto HAKCTransferFunction = GetHAKCTransferDef(CallI->getCalledFunction()->getName());
            if (HAKCTransferFunction) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Updating HAKC call parameters for " << CallI << "\n";
                }
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Updating index " << std::to_string
                            (HAKCTransferFunction->GetCompartmentIdIdx()) << " ("
                                                    << CallI->getArgOperand(
                                                            HAKCTransferFunction->GetCompartmentIdIdx()) << ") to "
                                                    << std::to_string(TargetCompartment.GetCompartmentIDValue())
                                                    << "\n";

                }

                UpdateHAKCFunctionParameters_Arch(CallI, TargetCompartment, HAKCTransferFunction, Policy);
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "After update call is " << CallI << "\n";
                }
            } else if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "No HAKC Transfer function found for "
                                                << CallI->getCalledFunction()->getName() << "\n";
            }
        }
    }

    /**
         * @brief Transfers a pointer argument back to its original color after an indirect call returns
         * @param operand Indirect call argument
         * @return The call to the kernel resigning operation
         */
    Instruction *HAKCFunctionAnalysis::addCompartmentTransferCall(Value *operand,
                                                                  const DebugLoc &debugLoc,
                                                                  Instruction *I,
                                                                  ConstantInt *Size,
                                                                  HAKCCompartmentalizationPolicy &Policy) {
        if (!operand->getType()->isPointerTy()
            && !isa<PtrToIntInst>(operand)
            && !operand->getType()->isIntegerTy(getCompartmentStorageSizeInBits())) {
            CommonHAKCAnalysis::getWriter() << "Compartment transfer target " << *operand
                                            << " is not a pointer but of type " << *operand->getType()
                                            << " in function\n" << getFunction() << "\n";
            throw std::exception();
        }

        bool isData = !valueIsReadonlyPtr(getDef(operand, false, debug_output));
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "isData: " << std::to_string(isData) << " for " << operand << "\n";
        }

        Instruction *TransferCall;
        if (Size == nullptr) {
            TransferCall = getTransformer(Policy).CreateCompartmentTransfer(operand, I, &getFunction(), isData);
        } else {
            TransferCall = getTransformer(Policy).CreateSizedCompartmentTransfer(operand, I, &getFunction(), isData,
                                                                                 Size);
        }
        TransferCall->setDebugLoc(debugLoc);
        CompartmentTransferCount++;
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Created transfer for ";
            if (!isData) {
                CommonHAKCAnalysis::getWriter() << operand->getName();
            } else {
                CommonHAKCAnalysis::getWriter() << operand;
            }
            CommonHAKCAnalysis::getWriter() << ": " << TransferCall << "\n";
        }
        return TransferCall;
    }

    /**
         * @brief Checks if a user is in the current function
         * @param user
         * @return True if the user is in the current function
         */
    bool HAKCFunctionAnalysis::userInFunction(Value *user) {
        Function &F = getFunction();
        if (auto I = dyn_cast<Instruction>(user)) {
            return &F == I->getFunction();
        } else {
            CommonHAKCAnalysis::getWriter() << "Unexpected user: " << user << "\n";
            throw std::exception();
        }
    }

    /**
         * @brief Finds the dominating BasicBlock among users and ptr
         * @param ptr
         * @param users
         * @return
         */
    BasicBlock *
    HAKCFunctionAnalysis::findDominatorUseBlock(Value *ptr,
                                                std::set<Instruction *> &users) {
        Function &F = getFunction();
        BasicBlock *dominator = nullptr;
        if (auto I = dyn_cast<Instruction>(ptr)) {
            if (!isa<AllocaInst>(ptr)) {
                dominator = I->getParent();
            }
        }

        std::set<BasicBlock *> BasicBlocks;

        for (auto *user: users) {
            if (!userInFunction(user)) {
                continue;
            }
            if (auto *PHI = dyn_cast<PHINode>(user)) {
                for (unsigned i = 0; i < PHI->getNumIncomingValues(); i++) {
                    auto *IncomingValue = PHI->getIncomingValue(i);
                    if (IncomingValue == ptr) {
                        BasicBlocks.insert(PHI->getIncomingBlock(i));
                    }
                }
            } else {
                BasicBlocks.insert(user->getParent());
            }
        }

        for (auto *BB: BasicBlocks) {
            if (!dominator) {
                dominator = BB;
            } else {
                dominator = DTree.findNearestCommonDominator(dominator, BB);
            }
        }

        if (!dominator) {
            dominator = &F.getEntryBlock();
        }

        return dominator;
    }

    /**
         * @brief Finds an insertion point for new instructions.
         * @param v The Value for which we want to insert a new Instruction
         * @param users The users of v
         * @return The location at which to insert a new Instruction
         */
    Instruction *
    HAKCFunctionAnalysis::FindUseInsertionPoint(Value *v,
                                                std::set<Instruction *> &users) {
        if (auto phi = dyn_cast<PHINode>(v)) {
            return phi->getParent()->getFirstNonPHIOrDbgOrLifetime();
        }
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Finding insertion point for ";
            if (v->getName().empty()) {
                CommonHAKCAnalysis::getWriter() << v;
            } else {
                CommonHAKCAnalysis::getWriter() << v->getName();
            }
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        BasicBlock *DominatorBlock = findDominatorUseBlock(v, users);
        if (!DominatorBlock) {
            CommonHAKCAnalysis::getWriter() << "Could not find block for " << v << "\n";
            getFunction().print(CommonHAKCAnalysis::getWriter().GetOS());
            throw std::exception();
        }

        for (Instruction &I: *DominatorBlock) {
            if (&I == v) {
                return I.getNextNonDebugInstruction();
            } else if (!isa<PHINode>(&I) && users.find(&I) != users.end()) {
                return &I;
            }
        }

        return DominatorBlock->getTerminator();
    }

    /**
         * @brief Returns the current Function
         * @return
         */
    Function &HAKCFunctionAnalysis::getFunction() {
        return *CurrentFunction;
    }

    /**
         * @brief Adds a check of a signed pointer which checks for valid data access
         * @param signed_ptr The pointer to check
         * @param location The location at which to place the check
         * @return The result of the transfer
         */
    Value *
    HAKCFunctionAnalysis::AddDataAuthCheckAtLocation(Value *signed_ptr, Instruction *location,
                                                     HAKCCompartmentalizationPolicy &Policy) {
        auto *bitcast = getTransformer(Policy).CreateDataAuthentication(signed_ptr, location);
        return bitcast;
    }

    Value *HAKCFunctionAnalysis::AddCodeAuthCheckAtLocation(Value *SignedPtr, Instruction *Location,
                                                            HAKCCompartmentalizationPolicy &Policy) {
        auto *SafePointer = getTransformer(Policy).CreateCodeAuthentication(SignedPtr, Location);
        return SafePointer;
    }

    void HAKCFunctionAnalysis::AddManagedPointer(Value *HAKCPointer) {
        if (!CommonHAKCAnalysis::IsPointerLikeType(HAKCPointer->getType())) {
            CommonHAKCAnalysis::getWriter() << "Trying to add an invalid ManagedHAKCPointer: " << HAKCPointer << "\n"
                                            << getFunction() << "\n";
            throw std::exception();
        }
        if (PointerManager.ManagePointer(HAKCPointer)) {
            auto ManagedPointer = PointerManager.GetManagedPointer(HAKCPointer);
            if (auto *PHII = dyn_cast<PHINode>(ManagedPointer->GetBaseDefinition())) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Definition is a PHI Node. Adding all non-null incoming "
                                                       "members\n";
                }
                for (auto &Incoming: PHII->incoming_values()) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Adding Incoming member " << Incoming << "\n";
                    }
                    RegisterPointerDereference(Incoming);
                }
            }
        }
    }

    /**
         * @brief Creates all authenticated pointers, and clones any intermediate pointer arithmetic
         * between authentication and dereference
         */
    void HAKCFunctionAnalysis::createAllAuthenticatedPointers() {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Function prior to making authenticated copies:\n" << getFunction() << "\n";
        }
        PointerManager.CreateAuthenticatedPointersAndAllClones();
    }

    /**
         * @brief Replace signed pointer dereferences with authenticated dereferences
         */
    void HAKCFunctionAnalysis::transformPointerDereferences() {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Function prior to transforming pointer dereferences\n" << getFunction()
                                            << "\n";
        }
        PointerManager.TransformPointers();
    }

    Value *HAKCFunctionAnalysis::AddSafePointerCreationAtLocation(Value *SignedPtr, Instruction *Location,
                                                                  HAKCCompartmentalizationPolicy &Policy) {
        auto *SafePtr = getTransformer(Policy).CreateSafePointer(SignedPtr, Location);
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Created Safe Pointer\n\t" << *SafePtr << "\nFor Signed Pointer\n\t"
                                            << *SignedPtr << "\nat\n" << *Location << "\n";
        }
        return SafePtr;
    }

    /**
         * @brief Returns true if an argument should be authenticated
         * @param arg The function argument to check
         * @return
         */
    bool HAKCFunctionAnalysis::argNeedsAuthentication(Use &arg) {
        if (auto *call = dyn_cast<CallInst>(arg.getUser())) {
            if (auto *inlineAsm = dyn_cast<InlineAsm>(
                    call->getCalledOperand())) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Arg " << *arg << " of " << *call << " is argument "
                                                    << arg.getOperandNo() << "\n";
                }
                /* The RCU protected double-link list generates this assembly, and we want
                     * to store authenticated pointers. So ensure that authenticated pointers
                     * are the values getting stored.  See __list_add_rcu for an example.
                     * Perhaps a better way to handle this is to use Capstone to analyze the
                     * inline assembly string, and figure out the stored value in an
                     * architectural independent way. But that's way down the road. */
                if (inlineAsm->getAsmString() == "stlr $1, $0") {
                    if (arg.getOperandNo() == 1) {
                        return false;
                    } /*else if (arg.getOperandNo() == 0) {
                        return true;
                    }*/
                }
            } else if (call->getCalledFunction()) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "arg.getOperandNo() = " << arg.getOperandNo()
                                                    << "\n";
                }
                return ((arg->getType()->isPointerTy() ||
                         isa<PtrToIntInst>(arg.get()))) &&
                       (isSafeTransitionFunction(call->getCalledFunction()) ||
                        IsIntrinsicNeedingAuthentication(call));
            }
        }
        return (!isa<Function>(arg) && PointerShouldBeManaged(arg));
    }

    bool HAKCFunctionAnalysis::IsIntrinsicNeedingAuthentication(CallBase *Call) {
        auto IntrinsicsNeedingAuth = GetIntrinsicsNeedingAuthenticatedArgs();
        return IsCallInIntrinsicSet(Call, IntrinsicsNeedingAuth);
    }

    bool HAKCFunctionAnalysis::IsIntrinsicsNeedingCloning(CallBase *Call) {
        auto IntrinsicsNeedingCloning = GetIntrinsicsToClone();
        return IsCallInIntrinsicSet(Call, IntrinsicsNeedingCloning);
    }

    std::set<StringRef> HAKCFunctionAnalysis::GetNoTransferFunctions() {
        return getModuleAnalysis().GetNoTransferFunctions();
    }

    std::set<StringRef> HAKCFunctionAnalysis::GetSafeTransitionFunctions() {
        return getModuleAnalysis().GetSafeTransitionFunctions();
    }

    std::set<hakc_transfer_def_t> HAKCFunctionAnalysis::GetHAKCTransferFunctions() {
        return getModuleAnalysis().GetHAKCTransferFunctions();
    }

    std::map<StringRef, hakc_allocation_size_map_t> HAKCFunctionAnalysis::GetKernelAllocationSizeMap() {
        return getModuleAnalysis().GetKernelAllocationSizeMap();
    }

    std::set<hakc_function_def_t> HAKCFunctionAnalysis::GetHAKCFunctions() {
        return getModuleAnalysis().GetHAKCFunctions();
    }

    std::set<StringRef> HAKCFunctionAnalysis::GetIgnoredTypes() {
        return getModuleAnalysis().GetIgnoredTypes();
    }

    std::set<StringRef> HAKCFunctionAnalysis::GetIgnoredGlobals() {
        return getModuleAnalysis().GetIgnoredGlobals();
    }

    std::set<Intrinsic::ID> HAKCFunctionAnalysis::GetIntrinsicsNeedingAuthenticatedArgs() {
        return {
                Intrinsic::IndependentIntrinsics::memcpy,
                Intrinsic::IndependentIntrinsics::memmove,
                Intrinsic::IndependentIntrinsics::memset
        };
    }

    std::set<Intrinsic::ID> HAKCFunctionAnalysis::GetIntrinsicsToClone() {
        return {
                Intrinsic::IndependentIntrinsics::lifetime_start,
                Intrinsic::IndependentIntrinsics::lifetime_end,
        };
    }

    std::set<Intrinsic::ID> HAKCFunctionAnalysis::GetInstrinsicsToSkip() {
        return {
                Intrinsic::IndependentIntrinsics::dbg_declare,
                /*Intrinsic::IndependentIntrinsics::dbg_addr,*/
                Intrinsic::IndependentIntrinsics::dbg_label,
                Intrinsic::IndependentIntrinsics::dbg_value,
                Intrinsic::IndependentIntrinsics::read_register,
        };
    }

    /**
         * @brief Returns true if the PHINode uses the specified target
         * @param phiNode
         * @param target
         * @return
         */
    bool HAKCFunctionAnalysis::phiNodeUsesValue(PHINode *phiNode, Value *target,
                                                std::set<PHINode *> &visited) {
        visited.insert(phiNode);
        for (auto &val: phiNode->incoming_values()) {
            Value *def = getDef(val.get(), true, debug_output);
            if (val.get() == target || def == target) {
                return true;
            } else if (auto *phi = dyn_cast<PHINode>(def)) {
                if (visited.find(phi) != visited.end()) {
                    continue;
                }
                if (phiNodeUsesValue(phi, target, visited)) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
         * @brief Perform analysis of an Instruction
         * @param I
         */
    void HAKCFunctionAnalysis::HandleInstruction(Instruction *I, HAKCCompartmentalizationPolicy &Policy) {
        if (auto *call = dyn_cast<CallInst>(I)) {
            handleCall(call, Policy);
        } else if (auto *load = dyn_cast<LoadInst>(I)) {
            handleLoad(load);
        } else if (auto *store = dyn_cast<StoreInst>(I)) {
            handleStore(store);
        } else if (auto *compare = dyn_cast<CmpInst>(I)) {
            handleComparison(compare, Policy);
        } else if (auto *binOp = dyn_cast<BinaryOperator>(I)) {
            handleBinaryOperator(binOp);
        }
    }

    /**
         * @brief Retrieves the Instruction of a User
         * @param user
         * @return
         */
    Instruction *HAKCFunctionAnalysis::getUserInst(User *user) {
        if (auto *inst = dyn_cast<Instruction>(user)) {
            return inst;
        } else if (isa<BitCastOperator>(user) || isa<GEPOperator>(user)) {
            return getUserInst(*user->user_begin());
        } else {
            CommonHAKCAnalysis::getWriter() << "Unexpected user: " << user << "\n";
            assert(false && "getUserInst");
            return nullptr; /* Shut up the compiler */
        }
    }

    bool HAKCFunctionAnalysis::IsPHIOfGlobalsOnly(Value *V) {
        std::set<PHINode *> nodes;
        return isPHIofGlobalsOnly(V, nodes);
    }

    /**
         * @brief Returns true of ptr is a PHINode consisting only of global variables
         * @param ptr
         * @param nodes
         * @return
         */
    bool HAKCFunctionAnalysis::isPHIofGlobalsOnly(Value *ptr,
                                                  std::set<PHINode *> &nodes) {
        if (auto *phiNode = dyn_cast<PHINode>(ptr)) {
            if (nodes.find(phiNode) != nodes.end()) {
                return true;
            }
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Examining PHI Node " << phiNode << " for Globals (" << nodes.size() << ")\n";
            }
            nodes.insert(phiNode);
            for (auto &val: phiNode->incoming_values()) {
                Value *def = getDef(val.get(), false, debug_output);
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "\tPHI Node value: " << val << "\n\t\tDef: " << def << "\n";
                }
                if (!isa<GlobalValue>(def)) {
                    if (isa<PHINode>(def)) {
                        if (isPHIofGlobalsOnly(def, nodes)) {
                            continue;
                        }
                    }
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    bool HAKCFunctionAnalysis::IsManualSafePointer(CallInst *Call) {
        if (Call->getCalledFunction()) {
            auto SafePointerNames = GetSafePointerFunctionNames();
            return SafePointerNames.find(Call->getCalledFunction()->getName()) != SafePointerNames.end();
        }

        return false;
    }

    /**
         * @brief Returns true if a pointer should be authenticated
         * @param ptr
         * @return
         */
    bool HAKCFunctionAnalysis::PointerShouldBeManaged(Use &U) {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Starting Pointer Management checks for " << U.get() << " from " << U.getUser() << "\n";
        }

        auto *ptr = getDef(U.get(), false, debug_output);
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " found def " << ptr << " for " << U.get() << "\n";
        }

        if (auto *call = dyn_cast<CallInst>(ptr)) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Value " << *ptr << " is a CallInst\n";
            }

            bool IsInline = call->isInlineAsm();
            bool IsManualSafe = IsManualSafePointer(call);
            if (IsInline || IsManualSafe) {
                if (debug_output) {
                    if (IsInline) {
                        CommonHAKCAnalysis::getWriter() << "Call is Inline Assembly\n";
                    }

                    if (IsManualSafe) {
                        CommonHAKCAnalysis::getWriter() << "Value " << *ptr << " is a manual safe pointer\n";
                    }
                }
                /* These are usually the result of reading a register value */
                return ValueIsUsedAsPointer(call, debug_output);
            } else if (call->getCalledFunction() &&
                       call->getCalledFunction()->isIntrinsic() &&
                       call->getCalledFunction()->getIntrinsicID() ==
                       Intrinsic::IndependentIntrinsics::read_register) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Call is a read register intrinsic\n";
                }
                return false;
            } else if (call->getType()->isIntegerTy(32)) {
                /* Sometimes functions that return i32 are cast to a pointer for a check
                 * against IS_ERR(). No need to check this.
                 * See find_mm_struct in mm/migrate.c.
                 */
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Call returns 32-bit integer\n";
                }
                return false;
            }
        } else if (auto *ConstExpr = dyn_cast<ConstantExpr>(ptr)) {
            if (ConstExpr->isCast()) {
                auto *Operand = getDef(ConstExpr->getOperand(0), false, debug_output);
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << *ConstExpr << " operand def is " << *Operand << "\n";
                }
                if (isa<ConstantInt>(Operand)) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "ConstExpr is from ConstantInt\n";
                    }
                    return false;
                }
            }
        } else if (isa<Constant>(ptr) && ptr->getType()->isIntegerTy()) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " is a constant int\n";
            }
            return false;
        } else if (!CommonHAKCAnalysis::IsPointerLikeType(ptr->getType()) &&
                   !ptr->getType()->isArrayTy() && !isa<PtrToIntInst>(ptr)) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " is not a pointer, array, or pointer to int cast\n";
            }
            return false;
        } else if (isa<ConstantPointerNull>(ptr)) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " is a constant null pointer\n";
            }
            return false;
        } /*else if (isa<GlobalValue>(ptr)) {
            if (debug_output) {
                ptr->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " is a GlobalValue\n";
            }
            return false;
        }*/ else if (IsPHIOfGlobalsOnly(ptr)) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " is a PHINode of Globals\n";
            }
            return false;
        } else if (isKernelUserPointer(ptr)) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " is a Kernel pointer from user space\n";
            }
            return false;
        } else if (isa<LoadInst>(U.getUser())) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " is used in a LoadInst\n";
            }
            return true;
        } else if (isa<StoreInst>(U.getUser())) {
            if (U.getOperandNo() == StoreInst::getPointerOperandIndex()) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << *ptr << " is used in a StoreInst\n";
                }
                return true;
            }
        } else if (isa<UndefValue>(ptr)) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " is an undef value\n";
            }
            return false;
        } else if (auto *CallI = dyn_cast<CallInst>(U.getUser())) {
            if (CallI->isInlineAsm()) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << *ptr << " is used in inline assembly\n";
                }
                return ValueIsUsedAsPointer(U.get(), debug_output);
            }
        } else if (!ptr->getType()->isPointerTy()) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " Type is not a pointer: " << *ptr->getType() << "\n";
            }
            return false;
        } else if (ptr->getType()->isPointerTy()) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *ptr << " Type is a pointer: " << *ptr->getType() << "\n";
            }
            return true;
        }
        return ptr->getType()->isPointerTy();
    }

    /**
         * @brief Adds a signed pointer dereference to the list if the source pointer should be authenticated
         * @param use
         */
    void HAKCFunctionAnalysis::RegisterPointerDereference(Use &use) {
        Value *definition = getDef(use.get(), false, debug_output);
        if ((isa<StoreInst>(use.getUser()) && isa<IntToPtrInst>(use.get())) ||
            (definition->getType()->isIntegerTy(64))) {
            bool registerUse = false;
            if (auto *call = dyn_cast<CallInst>(definition)) {
                registerUse = CommonHAKCAnalysis::isRegisterRead(call);
            }
            if (!registerUse) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Using " << *use << " instead of " << *definition << "\n";
                }
                definition = use.get();
            }
        }
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Checking if " << *use << " should be registered\n";
        }

        if (PointerShouldBeManaged(use)) {
            if (isa<IntToPtrInst>(use.get())) {
                bool is_percpu_ptr = isPerCPUPointer(use);

                if (is_percpu_ptr) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Detected per-cpu pointer: " << *use << "\nDef chain:\n";
                        for (auto *v: findDefChain(use.get(), false, debug_output)) {
                            CommonHAKCAnalysis::getWriter() << "\t" << *v << "\n";
                        }
                    }
                    definition = use.get();
                }
            }
            AddManagedPointer(definition);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Definition " << definition << " from " << *use.getUser() << " is registered\n";
            }
        } else {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Definition " << definition << " from " << *use.getUser() << " should not be checked\n";
            }
        }
    }

    Instruction *HAKCFunctionAnalysis::GetFinalAllocaDef(AllocaInst *Alloca) {
        return Alloca;
    }

    Value *HAKCFunctionAnalysis::getDef(Value *V, bool followLoad, bool debug) {
        auto *def = CommonHAKCAnalysis::getDef(V, followLoad, debug);
        if (!def) {
            CommonHAKCAnalysis::getWriter() << "Could not find definition for " << V << "\n";
            throw std::exception();
        }
        return def;
    }

    /**
         * @brief Process a LoadInst for analysis
         * @param load
         */
    void HAKCFunctionAnalysis::handleLoad(LoadInst *load) {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Handling " << *load->getOperandUse(LoadInst::getPointerOperandIndex())
                                            << " from Load " << *load << "\n";
        }
        RegisterPointerDereference(
                load->getOperandUse(LoadInst::getPointerOperandIndex()));
    }

    /**
         * @brief Process a StoreInst for analysis
         * @param store
         */
    void HAKCFunctionAnalysis::handleStore(StoreInst *store) {
        RegisterPointerDereference(
                store->getOperandUse(StoreInst::getPointerOperandIndex()));

        if (auto *globalValue = dyn_cast<GlobalValue>(
                store->getValueOperand())) {
            if (globalShouldBeTransferred(store->getOperandUse(0))) {
                GlobalArgumentUses[globalValue].insert(store);
            }
        }
    }

    void HAKCFunctionAnalysis::MaybeAddCompareToDirectUsers(CmpInst *CmpI, HAKCCompartmentalizationPolicy &Policy) {
        CheckCompareOperandForDirectFunctionUse(CmpI, Policy, 0);
        CheckCompareOperandForDirectFunctionUse(CmpI, Policy, 1);
    }

    void
    HAKCFunctionAnalysis::CheckCompareOperandForDirectFunctionUse(CmpInst *CmpI, HAKCCompartmentalizationPolicy &Policy,
                                                                  unsigned int OpNo) {
        auto *Op = getDef(CmpI->getOperand(OpNo), false, debug_output);
        if (auto *func = dyn_cast<Function>(Op)) {
            if (valueShouldBeReplacedWithTransfer(func, Policy)) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Adding comparison to directFunctionUsers for argument " <<
                                                    std::to_string(OpNo) << "\n";
                }
                directFunctionUsers.insert(CmpI);
            }
        }
    }

    /**
         * @brief Ensures that authenticated pointers are used in comparisons for correctness
         * @param compare
         */
    void HAKCFunctionAnalysis::handleComparison(CmpInst *compare, HAKCCompartmentalizationPolicy &Policy) {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Checking comparison " << *compare << "\n";
        }

        MaybeAddCompareToDirectUsers(compare, Policy);

        if (isa<ConstantPointerNull>(compare->getOperand(0)) ||
            isa<ConstantPointerNull>(compare->getOperand(1))) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "\tComparisons with null do not need authentication\n";
            }
            return;
        } else if (isa<Operator>(compare->getOperand(0)) ||
                   isa<Operator>(compare->getOperand(1))) {
            bool comparisonIsWithConstant = false;
            auto *bitCastOperator0 = dyn_cast<Operator>(compare->getOperand(0));
            if (bitCastOperator0) {
                if (auto *ci = dyn_cast<ConstantInt>(bitCastOperator0->getOperand(0))) {
                    comparisonIsWithConstant = (ci->getZExtValue() < user_space_end);
                }
            }
            if (!comparisonIsWithConstant) {
                auto *bitCastOperator1 = dyn_cast<Operator>(compare->getOperand(1));
                if (bitCastOperator1) {
                    if (auto *ci = dyn_cast<ConstantInt>(bitCastOperator1->getOperand(0))) {
                        comparisonIsWithConstant = (ci->getZExtValue() < user_space_end);
                    }
                }
            }

            if (comparisonIsWithConstant) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter()
                            << "\tComparisons with constant integers do not need authentications\n";
                }
                return;
            }
        }

        if (isCompartmentalizedFunction(Policy)) {
            bool arg0NeedsAuth =
                    argNeedsAuthentication(compare->getOperandUse(0)) &&
                    !isa<GlobalValue>(getDef(compare->getOperand(0), false, debug_output));
            bool arg1NeedsAuth =
                    argNeedsAuthentication(compare->getOperandUse(1)) &&
                    !isa<GlobalValue>(getDef(compare->getOperand(1), false, debug_output));
            if (debug_output) {
                if (arg0NeedsAuth) {
                    CommonHAKCAnalysis::getWriter() << "Argument 0 needs auth\n";
                } else {
                    CommonHAKCAnalysis::getWriter() << "Argument 0 does not need auth\n";
                }
                if (arg1NeedsAuth) {
                    CommonHAKCAnalysis::getWriter() << "Argument 1 needs auth\n";
                } else {
                    CommonHAKCAnalysis::getWriter() << "Argument 1 does not need auth\n";
                }
            }
            if (arg0NeedsAuth && arg1NeedsAuth) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Both operands should be checked\n";
                }
                RegisterPointerDereference(compare->getOperandUse(0));
                RegisterPointerDereference(compare->getOperandUse(1));
            } else {
                if (arg0NeedsAuth) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Registering argument 0\n";
                    }
                    RegisterPointerDereference(compare->getOperandUse(0));
                } else {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Argument 1 (" << compare->getOperand(1) << " ) already authenticated\n";
                    }
                }
                if (arg1NeedsAuth) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Registering argument 1\n";
                    }
                    RegisterPointerDereference(compare->getOperandUse(1));
                }
            }
        } else {
            if (argNeedsAuthentication(compare->getOperandUse(0))) {
                RegisterPointerDereference(compare->getOperandUse(0));
            }
            if (argNeedsAuthentication(compare->getOperandUse(1))) {
                RegisterPointerDereference(compare->getOperandUse(1));
            }
        }
    }

    /**
     * @brief BinaryOperators (like bitwise OR) should use authenticated values
     * @param binOp
     */
    void HAKCFunctionAnalysis::handleBinaryOperator(BinaryOperator *binOp) {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Checking binary op " << binOp << "\n";
        }
        /* Both operators need to be pointers to skip operations like
         * ptr | 0xFFFF
         */
        if (argNeedsAuthentication(binOp->getOperandUse(0)) &&
            argNeedsAuthentication(binOp->getOperandUse(1))) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Registering both operands\n";
            }
            RegisterPointerDereference(binOp->getOperandUse(0));
            RegisterPointerDereference(binOp->getOperandUse(1));
        }
    }


    /**
         * @brief Returns true if a GlobalValue should be transferred
         * @param globalValue
         * @return
         */
    bool HAKCFunctionAnalysis::globalShouldBeTransferred(Use &globalValueArg) {
        /* Don't transfer to printk */
        if (auto *globalValue = dyn_cast<GlobalValue>(
                getDef(globalValueArg.get(), false, debug_output))) {
            /* Don't transfer THIS_MODULE */
            if (globalValue->getName() == "__this_module") {
                return false;
            }

            /* Ignore constant string arrays */
            if (globalValue->getValueType()->isArrayTy() &&
                globalValue->getValueType()->getArrayElementType()->isIntegerTy(
                        8)) {
                return false;
            }

            if (auto *call = dyn_cast<CallInst>(
                    globalValueArg.getUser())) {
                if (!functionIsAnalysisCandidate(call->getCalledFunction())) {
                    return false;
                }
                return true;
            }

            return globalValue->getValueType()->isPointerTy();
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Arg " << globalValueArg.getOperandNo() << " (" << globalValueArg << " ) is not a GlobalValue\n";
        }
        return false;
    }

    bool HAKCFunctionAnalysis::isCompartmentalizedFunction(HAKCCompartmentalizationPolicy &Policy) {
        return CommonHAKCAnalysis::IsCompartmentalizedFunction(CurrentFunction, Policy);
    }

    /**
         * @brief Processes a function call for analysis
         * @param call
         */
    void HAKCFunctionAnalysis::handleCall(CallInst *call, HAKCCompartmentalizationPolicy &Policy) {
        auto IntrinsicsToSkip = GetInstrinsicsToSkip();
        if (call->getCalledFunction() &&
            (/*call->getCalledFunction()->isDebugInfoForProfiling() ||*/
             IntrinsicsToSkip.find(call->getIntrinsicID()) !=
             IntrinsicsToSkip.end())) {
            return;
        }

        if (IsHAKCFunction(call->getCalledFunction())) {
            HAKCFunctionCalls.insert(call);
        }

        auto CurrentCompartment = Policy.GetCompartment(CurrentFunction);

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Handling call " << *call << "\n";
        }

        if (ValueIsUsedAsPointer(call, debug_output)) {
            AddManagedPointer(call);
        } else if (debug_output) {
            CommonHAKCAnalysis::getWriter() << *call << " should not be managed\n";
        }

        bool needsAuthenticatedArgs = (call->isInlineAsm() ||
                                       (getModuleAnalysis().functionInAnalysisSet(
                                               call->getCalledFunction()) &&
                                        !isOutsideTransferFunc(
                                                call->getCalledFunction())) ||
                                       callIsSafeTransition(call));

        if (isa<IntrinsicInst>(call)) {
            needsAuthenticatedArgs = IsIntrinsicNeedingAuthentication(call);
        }

        if (debug_output) {
            if (needsAuthenticatedArgs) {
                CommonHAKCAnalysis::getWriter() << *call << " needs authenticated args\n";
            } else {
                CommonHAKCAnalysis::getWriter() << *call << " does not need authenticated args\n";
            }
        }

        if (call->isIndirectCall()) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Indirect call: " << *call << "\n";
            }
            RegisterPointerDereference(call->getCalledOperandUse());
            /* Using checked pointers for indirect calls because the indirect call
             * can be an assembly function, which currently requires valid pointers.
             * This is safe for other functions, since the target will be a transfer
             * function, and will perform the protecting before entering compartmentalized
             * code, or again create a valid pointer for uncompartmentalized code */
            for (auto &arg: call->args()) {
                if (argNeedsAuthentication(arg)) {
                    RegisterPointerDereference(arg);
                } else if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Argument " << *arg << " for " << *call
                                                    << " does not need authentication\n";
                }
            }
        } else if (needsAuthenticatedArgs) {
            for (auto &arg: call->args()) {
                if (argNeedsAuthentication(arg)) {
                    RegisterPointerDereference(arg);
                } else if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Argument " << *arg << " for " << *call
                                                    << " does not need authentication\n";
                }
            }
        } else if (!callIsSafeTransition(call)) {
            if (call->getCalledFunction() && call->getCalledFunction()->isIntrinsic()) {
                /* Intrinsics that don't need authenticated args are basically bitwise shifts
                 * and other minor things, so ignore them
                 */
                return;
            }
            for (auto &arg: call->args()) {
                Value *def = getDef(arg.get(), false, debug_output);
                if (auto *glob = dyn_cast<GlobalValue>(def)) {
                    if (globalShouldBeTransferred(arg)) {
                        if (debug_output) {
                            CommonHAKCAnalysis::getWriter() << "Global " << glob->getName()
                                                            << " used by " << *call << "\n";
                        }
                        GlobalArgumentUses[glob].insert(call);
                        RegisterPointerDereference(arg);
                    } else if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Global " << glob->getName()
                                                        << " should not be transferred to " << *call << "\n";
                    }
                } else if (auto *phiNode = dyn_cast<PHINode>(def)) {
                    for (auto &val: phiNode->incoming_values()) {
                        Value *valDef = getDef(val.get(), false, debug_output);
                        if (auto *globVal = dyn_cast<GlobalValue>(valDef)) {
                            if (globalShouldBeTransferred(val)) {
                                if (debug_output) {
                                    CommonHAKCAnalysis::getWriter() << "Global " << globVal->getName() << " used by "
                                                                    << *call << "\n";
                                }
                                GlobalArgumentUses[globVal].insert(call);
                            } else if (debug_output) {
                                CommonHAKCAnalysis::getWriter() << "Global " << globVal->getName()
                                                                << " should not be transferred to " << *call << "\n";
                            }
                        }
                    }
                } else if (isa<AllocaInst>(def)) {
                    if (!functionIsAnalysisCandidate(
                            call->getCalledFunction())) {
                        if (debug_output) {
                            CommonHAKCAnalysis::getWriter() << "Function called by " << *call
                                                            << " is not an analysis candidate\n";
                        }
                        continue;
                    }
                }
            }
            if (call->getCalledFunction()) {
                auto TargetCompartment = Policy.GetCompartment(call->getCalledFunction());
                if (!TargetCompartment.IsKernelCompartment()) {
                    NonKernelDirectFunctionCallSet.insert(call);
                }
            }
        }
    }

    /**
         * @brief Sets the function section to the correct PMC ELF section
         */
    void HAKCFunctionAnalysis::relocateFunctionSection(HAKCCompartmentalizationPolicy &Policy) {
        if (isCompartmentalizedFunction(Policy)) {
            getFunction().setSection(getHAKCFunctionSectionName(Policy));
        }
    }

    std::string HAKCFunctionAnalysis::getHAKCFunctionSectionName(HAKCCompartmentalizationPolicy &Policy) {
        std::string sectionName = HAKC_SECTION_PREFIX.str();
        auto Compartment = Policy.GetCompartment(&getFunction());
        sectionName += std::to_string(Compartment.GetCompartmentIDValue());
        if (getFunction().getSection().empty()) {
            sectionName += ".text";
        } else {
            sectionName += getFunction().getSection().str();
        }
        return sectionName;
    }

    void HAKCFunctionAnalysis::setup(HAKCCompartmentalizationPolicy &Policy) {
        if (!SetupHasRun) {
            auto Compartment = Policy.GetCompartment(CurrentFunction);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Running setup for " << getFunction().getName() << "\n"
                                                << getFunction() << "\nCompartmentID = "
                                                << std::to_string(Compartment.GetCompartmentIDValue()) << "\n";
            }
            PointerManager.SetFunctionIsCompartmentalized(!Compartment.IsKernelCompartment());
            for (auto it = inst_begin(CurrentFunction); it != inst_end(CurrentFunction); ++it) {
                Instruction *inst = &*it;
                HandleInstruction(inst, Policy);
            }
            SetupHasRun = true;
        } else if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "setup has run for " << getFunction().getName() << "\n";
        }
    }

    bool HAKCFunctionAnalysis::modifiedFunction() {
        return !(PointerManager.empty() &&
                 GlobalArgumentUses.empty() &&
                 NonKernelDirectFunctionCallSet.empty() &&
                 PointerManager.GetTotalAdditions() == 0 &&
                 CompartmentTransferCount == 0);
    }


    void HAKCFunctionAnalysis::CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls(
            HAKCCompartmentalizationPolicy &Policy) {
        auto CurrentDivision = Policy.GetDivision(&getFunction());
        for (auto *call: NonKernelDirectFunctionCallSet) {
            auto TargetCompartment = Policy.GetCompartment(call->getCalledFunction());
            if (CurrentDivision.GetHAKCCompartment().GetCompartmentID() == TargetCompartment.GetCompartmentID()) {
                /* Aliases are being used for transfer functions, so if the
                 * called function is in the same compartment use the transformed function
                 * name. Otherwise do not change the function name, because the
                 * transfer function will be used through the alias.
                 */
                auto TransformedName = CommonHAKCAnalysis::getOriginalTransformedName(call->getCalledFunction());
                auto TransformedFunction = getModuleAnalysis().GetFunctionByName(TransformedName,
                                                                                 call->getCalledFunction()->getFunctionType());
                call->setCalledFunction(TransformedFunction);
            } else {
                // Fixing https://github.mit.edu/inherently-secure/ARM-MTE/issues/40
                bool ValidTransition = false;

                for (auto *Target: CurrentDivision.GetHAKCCompartment().GetValidTargets()) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Testing Target Compartment " << *Target << " against "
                                                        << *TargetCompartment.GetCompartmentID() << "\n";
                    }
                    if (Target == TargetCompartment.GetCompartmentID()) {
                        ValidTransition = true;
                        break;
                    }
                }

                if (!ValidTransition) {
                    CommonHAKCAnalysis::getWriter() << "A direct Compartment transition from "
                                                    << std::to_string(
                                                            CurrentDivision.GetHAKCCompartment().GetCompartmentIDValue())
                                                    << " to "
                                                    << std::to_string(TargetCompartment.GetCompartmentIDValue())
                                                    << " is statically possible but not allowed in the"
                                                    << " Compartmentalization Policy\n"
                                                    << "A call from " << call->getFunction()->getName() << " to "
                                                    << call->getCalledFunction()->getName() << " is not allowed\n";
                    throw std::exception();
                }

                if (call->getCalledFunction()->isVarArg()) {
                    auto *VariadicTransfer = getTransformer(Policy).CreateTransferToVariadic(call);
                    call->setCalledFunction(VariadicTransfer);
                }
            }
        }
    }

    HAKCTransformer &HAKCFunctionAnalysis::getTransformer(HAKCCompartmentalizationPolicy &Policy) {
        return getModuleAnalysis().getTransformer(Policy);
    }

    void HAKCFunctionAnalysis::AddInstrumentation(bool RelocateSection, HAKCCompartmentalizationPolicy &Policy) {
        if (isOutsideTransferFunc(&getFunction())) {
            throw std::exception();
        }

        debug_output = getFunction().getName() == getHAKCDebugName();
        if (!SetupHasRun) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " calling setup for " << getFunction().getName()
                                                << "\n";
            }
            setup(Policy);
        } else if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "setup() has run for " << getFunction().getName() << "\n";
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Managed Pointers:\n";
        }
        SmallVector<ManagedHAKCPointerP> SortedPointers;
        PointerManager.GetSortedPointers(SortedPointers);

        for (auto &HAKCPointer: SortedPointers) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << *HAKCPointer << "\n+++\n";
            }
            HAKCPointer->DetermineIfBasePointerIsAuthenticated(Policy);
        }

        if (modifiedFunction()) {
            if (RelocateSection) {
                relocateFunctionSection(Policy);
            }
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "---- createMissingTransfers ----\n";
            }
            createMissingTransfers(Policy);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
                CommonHAKCAnalysis::getWriter() << "----- UpdateHAKCFunctionParameters ------\n";
            }
            UpdateHAKCFunctionParameters(Policy);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
                CommonHAKCAnalysis::getWriter() << "---- createAllAuthenticatedPointers ----\n";
            }
            createAllAuthenticatedPointers();
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
                CommonHAKCAnalysis::getWriter() << "----- transformPointerDereferences ------\n";
            }
            transformPointerDereferences();
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
                CommonHAKCAnalysis::getWriter() << "- ReplaceDirectFunctionUsesWithTransfers -\n";
            }
            ReplaceDirectFunctionUsesWithTransfers(Policy);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
            }
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
                CommonHAKCAnalysis::getWriter()
                        << "------ CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls -----\n";
            }
            CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls(Policy);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
            }

            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << getFunction() << "\n";
            }

            CommonHAKCAnalysis::VerifyFunction(&getFunction());
        } else if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Function " << getFunction().getName() << " unmodified\n";
        }
    }

    Instruction *HAKCFunctionAnalysis::CreateMissingTransfer(Instruction *PointerNeedingTransfer,
                                                             HAKCCompartmentalizationPolicy &Policy) {
        auto Allocations = GetKernelAllocationSizeMap();
        std::set<Instruction *> UserInstructions;
        for (auto *U: PointerNeedingTransfer->users()) {
            if (auto *I = dyn_cast<Instruction>(U)) {
                UserInstructions.insert(I);
            }
        }
        auto *InsertionPoint = FindUseInsertionPoint(PointerNeedingTransfer, UserInstructions);

        ConstantInt *Size = nullptr;
        if (auto *Call = dyn_cast<CallInst>(PointerNeedingTransfer)) {
            if (Call->getCalledFunction()) {
                const auto &SizeFunction = Allocations.find(Call->getCalledFunction()->getName());
                if (SizeFunction != Allocations.end()) {
                    Size = dyn_cast<ConstantInt>((*SizeFunction).second(Call));
                }
            }
        }
        return addCompartmentTransferCall(PointerNeedingTransfer, PointerNeedingTransfer->getDebugLoc(),
                                          InsertionPoint, Size, Policy);
    }

    Instruction *
    HAKCFunctionAnalysis::SignGlobalPointerWithColor(GlobalValue *GlobalVar, HAKCCompartmentalizationPolicy &Policy) {
        std::set<Instruction *> UserInstructions;
        for (auto *U: GlobalVar->users()) {
            if (auto *I = dyn_cast<Instruction>(U)) {
                if (I->getFunction() == &getFunction()) {
                    UserInstructions.insert(I);
                }
            }
        }

        auto *InsertionPoint = FindUseInsertionPoint(GlobalVar, UserInstructions);
        return getTransformer(Policy).CreateSignWithColor(GlobalVar, InsertionPoint, &getFunction(),
                                                          !isa<Function>(GlobalVar));
    }

    void HAKCFunctionAnalysis::createMissingTransfers(HAKCCompartmentalizationPolicy &Policy) {
        if (CommonHAKCAnalysis::IsKernelSymbol(CurrentFunction, Policy)) {
            return;
        }
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Function prior to making transfers:\n" << getFunction() << "\n";
        }
        PointerManager.CreateAllTransfers();
    }

    void HAKCFunctionAnalysis::ReplaceInstructionOperand(Instruction *I, unsigned ArgNo, Value *OldValue,
                                                         Value *NewValue, HAKCCompartmentalizationPolicy &Policy) {
        auto *V = I->getOperand(ArgNo);
        Value *Replacement;
        if (auto *Oper = dyn_cast<BitCastOperator>(V)) {
            Replacement = getTransformer(Policy).CreateBitCast(NewValue, Oper->getDestTy(), I);
        } else if (V == OldValue) {
            Replacement = NewValue;
        } else {
            CommonHAKCAnalysis::getWriter() << "Could not find ";
            if (auto *F = dyn_cast<Function>(OldValue)) {
                CommonHAKCAnalysis::getWriter() << F->getName();
            } else {
                CommonHAKCAnalysis::getWriter() << OldValue << "\n";
            }
            CommonHAKCAnalysis::getWriter() << " in " << *I << "\n";
            throw std::exception();
        }
        I->setOperand(ArgNo, Replacement);
    }

    void HAKCFunctionAnalysis::CheckAndReplaceArgument(Value *V, Instruction *I, unsigned int ArgNo,
                                                       HAKCCompartmentalizationPolicy &Policy) {
        if (auto *Func = dyn_cast<Function>(V)) {
            auto name = getOutsideTransferName(Func);
            auto transfer = getModuleAnalysis().GetFunctionByName(name, Func->getFunctionType());
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Changing operand " << std::to_string(ArgNo) << " to "
                                                << name << " for\n\t" << *I << "\n";
            }
            transfer->setLinkage(Func->getLinkage());
            transfer->copyAttributesFrom(Func);
            ReplaceInstructionOperand(I, ArgNo, V, transfer, Policy);
        }
    }

    void HAKCFunctionAnalysis::ReplaceDirectFunctionUsesWithTransfers(HAKCCompartmentalizationPolicy &Policy) {
        for (auto *I: directFunctionUsers) {
            for (unsigned i = 0; i < I->getNumOperands(); i++) {
                if (isa<CallInst>(I)) {
                    auto *call = dyn_cast<CallInst>(I);
                    if (call->getCalledOperandUse().getOperandNo() == i)
                        /* Don't change actual function call, only the arguments */
                        continue;
                }
                auto *Op = getDef(I->getOperand(i), false, debug_output);
                if (isa<Function>(Op)) {
                    CheckAndReplaceArgument(Op, I, i, Policy);
                } else if (auto *selectInst = dyn_cast<SelectInst>(Op)) {
                    auto *TrueValue = getDef(selectInst->getTrueValue(), false, debug_output);
                    CheckAndReplaceArgument(TrueValue, I, i, Policy);
                    auto *FalseValue = getDef(selectInst->getFalseValue(), false, debug_output);
                    CheckAndReplaceArgument(FalseValue, I, i, Policy);
                }
            }
        }
    }

    void HAKCFunctionAnalysis::InstrumentCode(HAKCCompartmentalizationPolicy &Policy) {
        auto Compartment = Policy.GetCompartment(&getFunction());

        AddInstrumentation(!Compartment.IsKernelCompartment(), Policy);
    }

    bool HAKCFunctionAnalysis::PointerIsAuthenticated_Arch(Value *Pointer) {
        return false;
    }

    bool HAKCFunctionAnalysis::PointerShouldBeConsideredCode(Value *Pointer) {
        if (Pointer->getType()->isPointerTy()) {
            /*return Pointer->getType()->getPointerElementType()->isFunctionTy();*/
            return Pointer->getType()->isFunctionTy();
        }
        return false;
    }

}// namespace hakc
