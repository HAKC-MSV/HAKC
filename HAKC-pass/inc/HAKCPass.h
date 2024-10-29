//
// Created by derrick on 3/16/21.
//
/**
 * @brief HAKC FunctionAnalysis and Transformation pass
 * @file HAKCPass.h
 */

#ifndef PMC_HAKCPASS_H
#define PMC_HAKCPASS_H

#define MODULES_LIMIT 255
#define MASK_COLOR_LIMIT 65535

#include "llvm/IR/Constants.h"
#include "HAKC-defs.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"

// #include "HAKCAnalysis/CommonHAKCAnalysis.h"
// #include "HAKCAnalysis/HAKCModuleAnalysis.h"
// #include "HAKCSymbolGenerator.h"
// #include "HAKCSystemInformation.h"
// #include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"


using namespace llvm;
#endif//PMC_HAKCPASS_H
