//
// Created by de29664 on 3/22/23.
//

#include "llvm/IR/Verifier.h"

#include "HAKCTransformers/CheriBSD/HAKCTransformerCheriBSDCheri.h"
#include "HAKCAnalysis/CheriBSD/HAKCFunctionAnalysisCheriBSDCheri.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

namespace hakc {
    HAKCTransformerCheriBSDCheri::HAKCTransformerCheriBSDCheri(Module &Module,
                                                               HAKCModuleAnalysisCheriBSDCheri *Transformation) :
            HAKCTransformer(Module, Transformation), CapabilityAddressSpace(0) {
        auto *KernelCap = GetAccessCapability(0);
        if (KernelCap) {
            CapabilityAddressSpace = KernelCap->getAddressSpace();
        }
    }

    Value *HAKCTransformerCheriBSDCheri::CreateSafePointer_Arch(Value *HAKCPointer, Instruction *I) {
        auto AddrSpace = GetPointerAddrSpace(HAKCPointer);
        auto *AuthFunctionTy = GetHAKCDataAuthenticationFunctionType(AddrSpace);
        auto *BitCast = CreateBitCast(HAKCPointer, AuthFunctionTy->getParamType(0), I);
        StringRef AuthCallName;
        if (AddrSpace > 0) {
            AuthCallName = GetSafeCapabilityName();
        } else {
            AuthCallName = GetSafePointerName();
        }
        auto *AuthCall = CreateCall(AuthCallName, AuthFunctionTy->getReturnType(), {BitCast});
        auto *ReturnBitCast = CreateBitCast(AuthCall, HAKCPointer->getType(), AuthCall->getNextNonDebugInstruction());
        return ReturnBitCast;
    }

    Type *HAKCTransformerCheriBSDCheri::GetEntryTokenType(unsigned AddrSpace) {
        return HAKCIRBuilder.getInt32Ty();
    }

    Type *HAKCTransformerCheriBSDCheri::GetCapabilityType() {
        return HAKCIRBuilder.getInt8PtrTy(CapabilityAddressSpace);
    }

    Constant *HAKCTransformerCheriBSDCheri::GetEntryToken(hakc_compartment_id_t CompartmentID) {
        return ConstantInt::get(GetEntryTokenType(CapabilityAddressSpace), CompartmentID);
    }

    GlobalValue *HAKCTransformerCheriBSDCheri::GetAccessCapability(hakc_compartment_id_t CompartmentID) {
        auto *AccessCap = getModule().getNamedGlobal(GetSealingCapabilityName(CompartmentID));
        if (!AccessCap) {
            AccessCap = dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(GetSealingCapabilityName(CompartmentID),
                                                                               GetCapabilityType()));
            if (CompartmentID > 0) {
                AccessCap->setLinkage(GlobalValue::InternalLinkage);
                AccessCap->setInitializer(ConstantPointerNull::get(dyn_cast<PointerType>(GetCapabilityType())));
            }
        }
        return AccessCap;
    }

    std::string HAKCTransformerCheriBSDCheri::GetSealingCapabilityName(hakc_compartment_id_t CompartmentID) {
        std::string name = "_hakc_compartment_cap_";
        name += std::to_string(CompartmentID);
        return name;
    }

    const StringRef HAKCTransformerCheriBSDCheri::GetSafeCapabilityName() {
        return HAKCFunctionAnalysisCheriBSDCheri::GetSafeCapName;
    }

    const StringRef HAKCTransformerCheriBSDCheri::GetSafePointerName() {
        return HAKCFunctionAnalysisCheriBSDCheri::GetSafePtrName;
    }

    const StringRef HAKCTransformerCheriBSDCheri::GetCompartmentInitName() {
        return "reassign_compartment_cap";
    }

    FunctionType *HAKCTransformerCheriBSDCheri::GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) {
        Type *CapTy = GetCapabilityType();
        Type *RetTy = HAKCIRBuilder.getInt8PtrTy(AddrSpace);
        Type *ArgTys[] = {
                HAKCIRBuilder.getInt8PtrTy(AddrSpace),
                CapTy,
        };
        return FunctionType::get(RetTy, ArgTys, false);
    }

    bool HAKCTransformerCheriBSDCheri::ValidateHAKCIntegerPointerSize(Value *HAKCPointer) {
        return /*HAKCTransformer::ValidateHAKCIntegerPointerSize(HAKCPointer) ||*/
               HAKCPointer->getType() == GetCapabilityType();
    }

    GlobalVariable *HAKCTransformerCheriBSDCheri::AddCompartmentMetadataEntry(hakc_compartment_id_t CompartmentId) {
        Module &M = getModule();
        std::string EntryName = "_hakc_compartment_";
        EntryName += std::to_string(CompartmentId);
        GlobalVariable *CompartmentEntry = M.getGlobalVariable(EntryName);
        if (!CompartmentEntry) {
            Align CapabilityAlign(16);
            /* This recreates the functionality of the MODULE_VERSION macro in CheriBSD */
            auto *CompartmentIdValue = GetHAKCCompartmentValue(CompartmentId);
            StructType *CompartmentEntryTy = StructType::get(M.getContext(), {CompartmentIdValue->getType()},
                                                             false);
            CompartmentEntry = dyn_cast<GlobalVariable>(M.getOrInsertGlobal(EntryName, CompartmentEntryTy));
            CompartmentEntry->setInitializer(ConstantStruct::get(CompartmentEntryTy,
                                                                 {CompartmentIdValue}));
            CompartmentEntry->setSection(".data");
            CompartmentEntry->setLinkage(llvm::GlobalValue::ExternalLinkage);


            auto CompartmentEntryLabelName = EntryName;
            CompartmentEntryLabelName += "_label";
            auto *CompartmentEntryLabelValue = ConstantDataArray::getString(M.getContext(), EntryName);
            GlobalVariable *CompartmentEntryLabel = dyn_cast<GlobalVariable>(M.getOrInsertGlobal
                    (CompartmentEntryLabelName, CompartmentEntryLabelValue->getType()));
            CompartmentEntryLabel->setConstant(true);
            CompartmentEntryLabel->setInitializer(CompartmentEntryLabelValue);
            CompartmentEntryLabel->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
            CompartmentEntryLabel->setLinkage(llvm::GlobalValue::PrivateLinkage);

            StructType *ModMetadataTy = StructType::getTypeByName(M.getContext(), "struct.mod_metadata");
            if (!ModMetadataTy) {
                /* This follows the CheriBSD struct mod_metadata defined in sys/module.h
                 * struct mod_metadata {
                    int		md_version;	    // structure version MDTV_*
                    int		md_type;	    // type of entry MDT_*
                    const void	*md_data;	// specific data
                    const char	*md_cval;	// common string label
                    };
                */
                ModMetadataTy = StructType::get(M.getContext(), {IntegerType::get(M.getContext(), 32),
                                                                 IntegerType::get(M.getContext(), 32),
                                                                 PointerType::getInt8PtrTy(M.getContext(),
                                                                                           GetPointerAddrSpace(
                                                                                                   CompartmentEntry)),
                                                                 PointerType::getInt8PtrTy(M.getContext(),
                                                                                           GetPointerAddrSpace
                                                                                                   (CompartmentEntryLabel))
                });
            }
            std::string ModMetadataName = "_mod_metadata_hakc_compartment_";
            ModMetadataName += std::to_string(CompartmentId);
            auto *GlobalModMetadata = dyn_cast<GlobalVariable>(M.getOrInsertGlobal(ModMetadataName,
                                                                                   ModMetadataTy));

            GlobalModMetadata->setInitializer(ConstantStruct::get(ModMetadataTy, {
                    ConstantInt::get(ModMetadataTy->getTypeAtIndex((unsigned) 0), HACK_CHERIBSD_DEFAULT_VERSION),
                    ConstantInt::get(ModMetadataTy->getTypeAtIndex(1), HAKC_CHERIBSD_COMPARTMENT_METADATA_TYPE),
                    ConstantExpr::getBitCast(CompartmentEntry, ModMetadataTy->getTypeAtIndex(2)),
                    ConstantExpr::getBitCast(CompartmentEntryLabel, ModMetadataTy->getTypeAtIndex(3))
            }));
            GlobalModMetadata->setLinkage(llvm::GlobalValue::ExternalLinkage);
            GlobalModMetadata->setAlignment(CapabilityAlign);

            std::string ModMetadataEntryName = "__set_modmetadata_set_sym_";
            ModMetadataEntryName += ModMetadataName;
            auto *GlobalModMetadataEntryValue = ConstantExpr::getBitCast(GlobalModMetadata,
                                                                         PointerType::getInt8PtrTy(M.getContext(),
                                                                                                   GetPointerAddrSpace
                                                                                                           (GlobalModMetadata)));
            auto *GlobalModMetadataEntry = dyn_cast<GlobalVariable>(M.getOrInsertGlobal
                    (ModMetadataEntryName,
                     GlobalModMetadataEntryValue->getType()));
            GlobalModMetadataEntry->setSection("set_modmetadata_set");
            GlobalModMetadataEntry->setInitializer(GlobalModMetadataEntryValue);
            GlobalModMetadataEntry->setConstant(true);
            GlobalModMetadataEntry->setLinkage(llvm::GlobalValue::ExternalLinkage);
            GlobalModMetadataEntry->setAlignment(CapabilityAlign);

            CreateCapabilityReassignment(CompartmentId);
            ModifyModuleDriverCap(CompartmentId);
        }
        return CompartmentEntry;
    }

    void HAKCTransformerCheriBSDCheri::ModifyModuleDriverCap(hakc_compartment_id_t CompartmentID) {
        if (CompartmentID == KERNEL_COMPARTMENT) {
            return;
        }

        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "Modifying Module Driver Cap for Compartment "
                                            << std::to_string(CompartmentID) << "\n";
        }
        StringRef TargetStructTyName = "struct.driver_module_data";
        StringRef DriverObjectTyName = "struct.kobj_class";
        auto *TargetStructTy = StructType::getTypeByName(getModule().getContext(), TargetStructTyName);
        if (!TargetStructTy) {
            if (DebugIsActive()) {
                CommonHAKCAnalysis::getWriter() << "Could not find " << TargetStructTyName << "\n";
            }
            return;
        }
        auto *DriverObjectTy = StructType::getTypeByName(getModule().getContext(), DriverObjectTyName);
        if (!DriverObjectTy) {
            if (DebugIsActive()) {
                CommonHAKCAnalysis::getWriter() << "Could not find " << DriverObjectTyName << "\n";
            }
            return;
        }

        auto *KernelSealingCap = GetAccessCapability(0);
        auto *CompartmentSealingCap = GetAccessCapability(CompartmentID);

        std::set<std::pair<User *, unsigned>> UsesToReplace;
        for (auto &KernelSealingCapUse: KernelSealingCap->uses()) {
            if (KernelSealingCapUse.getUser()->getType() == TargetStructTy) {
                if (DebugIsActive()) {
                    CommonHAKCAnalysis::getWriter() << "Found TargetStruct ";
                    KernelSealingCapUse.getUser()->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
                if (auto *DriverModuleInitializer = dyn_cast<ConstantStruct>(KernelSealingCapUse.getUser())) {
                    if (DebugIsActive()) {
                        CommonHAKCAnalysis::getWriter() << "Searching operands...\n";
                    }
                    for (auto &Member: DriverModuleInitializer->operands()) {
                        if (isa<PointerType>(Member->getType()) &&
                            Member->getType()->getPointerElementType() == DriverObjectTy) {
                            if (DebugIsActive()) {
                                CommonHAKCAnalysis::getWriter() << "Found driver object at argument "
                                                                << std::to_string(Member.getOperandNo()) << "\n";
                            }
                            auto MemberCompartmentID = getGlobalCompartmentID(dyn_cast<GlobalVariable>(Member.get()));
                            if (MemberCompartmentID == CompartmentID) {
                                UsesToReplace.insert(std::make_pair(KernelSealingCapUse.getUser(),
                                                                    KernelSealingCapUse.getOperandNo()));
                            } else if (DebugIsActive()) {
                                CommonHAKCAnalysis::getWriter() << "Member Compartment ID " << std::to_string
                                        (MemberCompartmentID) << " does not match " << std::to_string(CompartmentID)
                                                                << "\n";
                            }
                        }
                    }
                }
            }
        }
        for (auto it: UsesToReplace) {
            if (DebugIsActive()) {
                CommonHAKCAnalysis::getWriter() << "Setting Argument " << std::to_string(it.second) << " of ";
                it.first->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " to be ";
                CompartmentSealingCap->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            it.first->setOperand(it.second, CompartmentSealingCap);
        }
    }

    void HAKCTransformerCheriBSDCheri::CreateCapabilityReassignment(hakc_compartment_id_t CompartmentID) {
        std::string CapabilityInitName = CAPABILITY_REASSIGNMENT_PREFIX.str();
        CapabilityInitName += std::to_string(CompartmentID);
        auto *SealingCap = GetAccessCapability(CompartmentID);

        auto *F = HAKCAnalysis->GetFunctionByName(CapabilityInitName,
                                                  FunctionType::get(
                                                          HAKCIRBuilder.getVoidTy(),
                                                          {HAKCIRBuilder.getInt8PtrTy(
                                                                  GetPointerAddrSpace(SealingCap))},
                                                          false));
        F->setLinkage(llvm::GlobalValue::PrivateLinkage);
        BasicBlock *EntryBB = BasicBlock::Create(getModule().getContext(), "", F);
        if (EntryBB != &F->getEntryBlock()) {
            CommonHAKCAnalysis::getWriter() << "Invalid Entry BasicBlock created\n";
            throw std::exception();
        }

        HAKCIRBuilder.SetInsertPoint(EntryBB);
        auto *Ret = HAKCIRBuilder.CreateRetVoid();
        HAKCIRBuilder.SetInsertPoint(Ret);

        auto *CompartmentIDValue = GetHAKCCompartmentValue(CompartmentID);

        Value *CapabilityPointer;
        if (CompilingPureCapKernel()) {
            auto *CapCast = HAKCIRBuilder.CreateBitCast(SealingCap, HAKCIRBuilder.getInt8PtrTy(CapabilityAddressSpace));
            Value *BoundsSetArgs[] = {CapCast,
                                      HAKCIRBuilder.getInt64(CommonHAKCAnalysis::getCompartmentStorageSizeInBits() /
                                                             8)};
            auto *BoundsSet = HAKCIRBuilder.CreateIntrinsic(Intrinsic::cheri_cap_bounds_set,
                                                            {HAKCIRBuilder.getInt64Ty()},
                                                            BoundsSetArgs);
            CapabilityPointer = HAKCIRBuilder.CreateBitCast(BoundsSet, SealingCap->getType());
        } else {
            CapabilityPointer = SealingCap;
        }
        CreateCall(GetCompartmentInitName(), HAKCIRBuilder.getVoidTy(), {
                CompartmentIDValue, CapabilityPointer
        });

        if (llvm::verifyFunction(*F, &CommonHAKCAnalysis::getWriter())) {
            CommonHAKCAnalysis::getWriter() << "Invalid SysInit function\n";
            F->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }

        /**
         * Create the sysinit structure.
         * struct sysinit {
         *    enum sysinit_sub_id	subsystem;	    // subsystem identifier
         *    enum sysinit_elem_order	order;		// init order within subsystem
         *    sysinit_cfunc_t func;			    // function
         *    const void	*udata;			        // multiplexer/argument
         * };
         */
        std::string SysInitEntryName = "_hakc_sysinit_";
        SysInitEntryName += std::to_string(CompartmentID);
        StructType *SysInitTy = StructType::getTypeByName(getModule().getContext(), "struct.sysinit");
        if (!SysInitTy) {
            SysInitTy = StructType::get(getModule().getContext(),
                                        {
                                                HAKCIRBuilder.getInt32Ty(),
                                                HAKCIRBuilder.getInt32Ty(),
                                                F->getType(),
                                                HAKCIRBuilder.getInt8PtrTy(CapabilityAddressSpace)
                                        });
        }
        auto *SysInitEntry = dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(SysInitEntryName,
                                                                                    SysInitTy));
        auto *SysInitEntryInitializer = ConstantStruct::get(SysInitTy, {
                getInt32(HAKC_INIT_ORDER),
                getInt32(HAKC_ELEM_ORDER),
                F,
                ConstantPointerNull::get(dyn_cast<PointerType>(SysInitTy->getTypeAtIndex(3)))
        });
        SysInitEntry->setInitializer(SysInitEntryInitializer);
        SysInitEntry->setLinkage(llvm::GlobalValue::ExternalLinkage);
        SysInitEntry->setConstant(false);

        std::string SysInitRecordName = "_sym_";
        SysInitRecordName += SysInitEntryName;
        auto *SysInitRecord = dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(SysInitRecordName,
                                                                                     HAKCIRBuilder.getInt8PtrTy(
                                                                                             CapabilityAddressSpace)));
        SysInitRecord->setSection("set_sysinit_set");
        SysInitRecord->setInitializer(ConstantExpr::getPointerCast(SysInitEntry,
                                                                   HAKCIRBuilder.getInt8PtrTy(CapabilityAddressSpace)));
        SysInitRecord->setConstant(false);
        SysInitRecord->setLinkage(llvm::GlobalValue::ExternalLinkage);
        MaybeAlign align(16);
        SysInitRecord->setAlignment(align);
    }

    std::vector<Value *> HAKCTransformerCheriBSDCheri::CreateArgumentsWithCompartment(Value *HAKCPointer,
                                                                                      GlobalValue *Target) {
        Value *HAKCPointerBitCast;

        unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
        auto *DataAuthFuncTy = GetHAKCDataAuthenticationFunctionType(AddrSpace);
        if (HAKCPointer->getType()->isIntegerTy()) {
            HAKCPointerBitCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer, DataAuthFuncTy->getParamType(0));
        } else {
            HAKCPointerBitCast = HAKCIRBuilder.CreateBitCast(HAKCPointer, DataAuthFuncTy->getParamType(0));
        }
        auto *CurrentFunction = HAKCIRBuilder.GetInsertBlock()->getParent();
        LoadInst *CapabilityLoad = GetFunctionCapabilityLoad(CurrentFunction);

        return {
                HAKCPointerBitCast,
                CapabilityLoad
        };
    }

    std::vector<Value *> HAKCTransformerCheriBSDCheri::CreateDataAuthArguments(Value *HAKCPointer, Instruction *I) {
        return CreateArgumentsWithCompartment(HAKCPointer, I->getFunction());
    }

    std::vector<Value *> HAKCTransformerCheriBSDCheri::CreateCodeAuthArguments(Value *HAKCPointer, Instruction *I) {
        Function *F = I->getFunction();
        auto *ExitTokens = GetValidTargetCompartments(F);
        auto Symbol = SystemInformation.findSymbol(F);
        if (!Symbol) {
            CommonHAKCAnalysis::getWriter() << "Could not find symbol for function " << F->getName() << "\n";
            throw std::exception();
        }

        if (!ExitTokens->getValueType()->isArrayTy()) {
            CommonHAKCAnalysis::getWriter() << "Invalid ExitToken Type (";
            ExitTokens->getValueType()->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << ") for ";
            ExitTokens->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
        Value *FirstExitToken = HAKCIRBuilder.CreateGEP(ExitTokens->getValueType(),
                                                        ExitTokens, {
                                                                HAKCIRBuilder.getInt64(0), HAKCIRBuilder.getInt64(0)
                                                        });
        unsigned AddrSpace = GetPointerAddrSpace(FirstExitToken);
        Value *IndirectCallTarget = HAKCIRBuilder.CreateBitCast(HAKCPointer, HAKCIRBuilder.getInt8PtrTy(AddrSpace));
        return {
                IndirectCallTarget,
                FirstExitToken,
                HAKCIRBuilder.getInt64(ExitTokens->getType()->getPointerElementType()->getArrayNumElements())
        };
    }

    std::vector<Value *>
    HAKCTransformerCheriBSDCheri::CreateTransferArguments(Value *HAKCPointer, GlobalValue *Target,
                                                          bool IsData, ConstantInt *Size) {
        return CreateArgumentsWithCompartment(HAKCPointer, Target);
    }

    Instruction *
    HAKCTransformerCheriBSDCheri::CreateCompartmentTransfer(Value *HAKCPointer, Instruction *I, GlobalValue *Target,
                                                            bool IsData) {
        ValidateHAKCPointerAndLocation(HAKCPointer, I);
        return CreateSizedCompartmentTransfer(HAKCPointer, I, Target, IsData, nullptr);
    }

    bool HAKCTransformerCheriBSDCheri::CompilingPureCapKernel() const {
        return CapabilityAddressSpace > 0;
    }

    LoadInst *HAKCTransformerCheriBSDCheri::GetFunctionCapabilityLoad(Function *F) {
        if (CapabilityLoads.find(F) != CapabilityLoads.end()) {
            return CapabilityLoads[F];
        }

        if (F->empty()) {
            CommonHAKCAnalysis::getWriter() << "Function " << F->getName()
                                            << " needs a basic block to add a Capability Load";
            throw std::exception();
        }

        auto *TransferTarget = F;

        if (CommonHAKCAnalysis::isOutsideTransferFunc(F)) {
            auto transferTargetName = F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
            TransferTarget = getModule().getFunction(transferTargetName);
            if (!TransferTarget) {
                CommonHAKCAnalysis::getWriter() << "Could not find Function " << transferTargetName << "\n";
                throw std::exception();
            }
        }

        auto Symbol = SystemInformation.findSymbol(TransferTarget);
        hakc_compartment_id_t CompartmentID = 0;
        if (Symbol) {
            CompartmentID = Symbol->getCompartmentID();
        }

        auto *AccessCapacity = GetAccessCapability(CompartmentID);
        auto &EntryBlock = F->getEntryBlock();
        auto *FirstInstruction = EntryBlock.getFirstNonPHIOrDbgOrLifetime();
        IRBuilder<> TempBuilder(FirstInstruction);

        auto *CapabilityLoad = TempBuilder.CreateLoad(AccessCapacity->getType()->getPointerElementType(),
                                                      AccessCapacity);
        CapabilityLoads[F] = CapabilityLoad;
        return CapabilityLoad;
    }
} // hakc
