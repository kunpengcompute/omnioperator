/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Expression code generation utilities
 */

#include "llvm_engine.h"

#include <llvm/Transforms/Utils/Cloning.h>
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar/SimpleLoopUnswitch.h"

#include "expr_info_extractor.h"
#include "func_registry.h"
#include "util/config_util.h"

namespace omniruntime {
namespace codegen {
namespace {
std::once_flag g_codegenTargetInitFlag;
constexpr unsigned SMALL_VECTOR_DEFAULT_INLINED_ELEMENTS_COUNT = 20;
static llvm::StringRef CPU_NAME;
static llvm::SmallVector<std::string, SMALL_VECTOR_DEFAULT_INLINED_ELEMENTS_COUNT> CPU_ATTRS;
}

LLVMEngine::LLVMEngine()
{
    std::call_once(g_codegenTargetInitFlag, InitializeCodegenTargets);
    llvm::ExitOnError eoe;
    context = std::make_unique<LLVMContext>();
    jit = eoe(LLJITBuilder().create());
    builder = std::make_unique<IRBuilder<>>(*context);
    auto module = std::make_unique<Module>("the_module", *context);
    module->setDataLayout(jit->getDataLayout());
    modulePtr = module.get();
    llvmTypes = std::make_unique<LLVMTypes>(*context);
    fpm = std::make_unique<legacy::FunctionPassManager>(module.get());

    auto optLevel = llvm::CodeGenOpt::Aggressive;
    std::string builderError;
    llvm::EngineBuilder engine_builder(std::move(module));
    engine_builder.setEngineKind(llvm::EngineKind::JIT).setOptLevel(optLevel).setErrorStr(&builderError);
    engine_builder.setMCPU(CPU_NAME);
    engine_builder.setMAttrs(CPU_ATTRS);
    std::unique_ptr<llvm::ExecutionEngine> exec_engine { engine_builder.create() };
    execution_engine = std::move(exec_engine);
    // Although ConfigUtil::IsEnableBatchExprEvaluate() = true, RowProjection also need row functions.
#ifndef EXCLUDE_BATCH_FUNCTIONS
    RegisterFunctions(FunctionRegistry::GetBatchFunctions());
#endif
    RegisterFunctions(FunctionRegistry::GetRowFunctions());
}

llvm::IRBuilder<> *LLVMEngine::GetIRBuilder()
{
    return builder.get();
}

Module *LLVMEngine::GetModule()
{
    return modulePtr;
}

LLVMContext *LLVMEngine::GetContext()
{
    return context.get();
}

LLVMTypes *LLVMEngine::GetTypes()
{
    return llvmTypes.get();
}

int64_t LLVMEngine::Compile()
{
    jit->getMainJITDylib().addGenerator(
        eoe(DynamicLibrarySearchGenerator::GetForCurrentProcess(jit->getDataLayout().getGlobalPrefix())));
    auto resTracker = jit->getMainJITDylib().createResourceTracker();
    MakeThreadSafe(&resTracker);
    rt = resTracker;
    auto sym = eoe(jit->lookup("WRAPPER_FUNC"));
    return sym.getValue();
}

void LLVMEngine::MakeThreadSafe(ResourceTrackerSP *resTracker)
{
    execution_engine->removeModule(modulePtr);
    std::unique_ptr<Module> module(modulePtr);
    auto threadSafeModule = llvm::orc::ThreadSafeModule(move(module), move(context));
    eoe(jit->addIRModule(*resTracker, std::move(threadSafeModule)));
}

void LLVMEngine::InitializeCodegenTargets()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetDisassembler();
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    CPU_NAME = llvm::sys::getHostCPUName();
    llvm::StringMap<bool> host_features;
    if (llvm::sys::getHostCPUFeatures(host_features)) {
        for (auto &f : host_features) {
            std::string attr = f.second ? std::string("+") + f.first().str() : std::string("-") + f.first().str();
            CPU_ATTRS.push_back(attr);
        }
    }
}

void LLVMEngine::RegisterFunctions(const std::vector<Function> &functions)
{
    for (auto &func : functions) {
        auto &jd = jit->getMainJITDylib();
        auto &dl = jit->getDataLayout();
        llvm::orc::MangleAndInterner mangle(jit->getExecutionSession(), dl);
        std::vector<DataTypeId> params = func.GetParamTypes();
        DataTypeId retType = func.GetReturnType();
        std::vector<Type *> args = this->GetFunctionArgTypeVector(params, retType, func.IsExecutionContextSet());
        auto s = llvm::orc::absoluteSymbols({ { mangle(func.GetId()),
            JITEvaluatedSymbol(pointerToJITTargetAddress(func.GetAddress()), JITSymbolFlags::Exported) } });
        auto ign = jd.define(s);
        if (ign) {
            LogError("Error while defining absolute symbol in jd");
        }
        llvm::Type *ret = (retType == OMNI_DECIMAL128) ? llvmTypes->VoidType() : llvmTypes->ToLLVMType(retType);
        llvm::FunctionType *ft = llvm::FunctionType::get(ret, args, false);
        auto linkage = llvm::Function::ExternalLinkage;
        llvm::Function::Create(ft, linkage, func.GetId(), *modulePtr);
        modulePtr->getOrInsertFunction(func.GetId(), ft);
    }
}

std::vector<Type *> LLVMEngine::GetFunctionArgTypeVector(std::vector<DataTypeId> &params, DataTypeId &retTypeId,
    bool needsContext)
{
    std::vector<Type *> args;
    if (needsContext) {
        args.push_back(llvmTypes->I64Type());
    }
    for (auto type : params) {
        if (type == OMNI_DECIMAL128) {
            args.push_back(llvmTypes->I64Type());
            args.push_back(llvmTypes->I64Type());
        } else {
            args.push_back(llvmTypes->ToLLVMType(type));
            if (TypeUtil::IsStringType(type)) {
                if (type == OMNI_CHAR) {
                    args.push_back(llvmTypes->I32Type());
                }
                args.push_back(llvmTypes->I32Type());
            }
        }
    }
    // return arguments
    if (TypeUtil::IsStringType(retTypeId)) {
        args.push_back(llvmTypes->I32PtrType());
    } else if (retTypeId == OMNI_DECIMAL128) {
        // Add high and low output pointers
        args.push_back(llvmTypes->I64PtrType());
        args.push_back(llvmTypes->I64PtrType());
    }
    return args;
}

void LLVMEngine::OptimizeFunctionsAndModule()
{
    auto machine = execution_engine->getTargetMachine();
    llvm::TargetIRAnalysis target_analysis = machine->getTargetIRAnalysis();

    mpm.add(llvm::createTargetTransformInfoWrapperPass(target_analysis));
    mpm.add(llvm::createFunctionInliningPass());
    mpm.add(llvm::createInstructionCombiningPass());
    mpm.add(llvm::createPromoteMemoryToRegisterPass());
    mpm.add(llvm::createGVNPass());
    mpm.add(llvm::createNewGVNPass());
    mpm.add(llvm::createCFGSimplificationPass());
    mpm.add(llvm::createLoopVectorizePass());
    mpm.add(llvm::createSLPVectorizerPass());
    mpm.add(llvm::createGlobalOptimizerPass());
    mpm.add(llvm::createStripDeadPrototypesPass());

    fpm->add(llvm::createTargetTransformInfoWrapperPass(target_analysis));

    // run the optimiser
    llvm::PassManagerBuilder pass_builder;
    pass_builder.OptLevel = llvm::CodeGenOpt::Aggressive;

    pass_builder.populateFunctionPassManager(*fpm);

    pass_builder.populateModulePassManager(mpm);

    fpm->doInitialization();
    for (auto &f : *modulePtr)
        fpm->run(f);
    fpm->doFinalization();

    mpm.run(*modulePtr);
}

void LLVMEngine::OptimizeModule()
{
    mpm.add(createFunctionInliningPass());
    mpm.add(createPruneEHPass());

    mpm.run(*modulePtr);
}

CallInst *LLVMEngine::CreateCall(llvm::Function *func, const std::vector<llvm::Value *> &argsVals,
    const std::string &name)
{
    return builder->CreateCall(func, argsVals, name);
}

llvm::Value *LLVMEngine::CallExternFunction(const std::string &fn_name, const std::vector<DataTypeId> &params,
    const DataTypeId returnType, const std::vector<Value *> &args, llvm::Value *executionContextPtr,
    const std::string &msg, omniruntime::op::OverflowConfig *overflowConfig, llvm::Value *overflowNull)
{
    std::vector<Value *> funcArgs;
    funcArgs.insert(funcArgs.begin(), args.begin(), args.end());
    if (executionContextPtr != nullptr) {
        if (overflowConfig != nullptr &&
            overflowConfig->GetOverflowConfigId() == omniruntime::op::OVERFLOW_CONFIG_NULL) {
            funcArgs.insert(funcArgs.begin(), overflowNull);
        } else {
            funcArgs.insert(funcArgs.begin(), executionContextPtr);
        }
    }

    std::string funcId = FunctionSignature(fn_name, params, returnType).ToString(overflowConfig);
    auto f = modulePtr->getFunction(funcId);
    auto ret = CreateCall(f, funcArgs, msg);
    return ret;
}

void LLVMEngine::RecordMainFunction(llvm::Function *func)
{
    this->function = func;
}

void LLVMEngine::RemoveUnusedFunctions()
{
    llvm::Function *preserved = function;
    mpm.add(llvm::createInternalizePass(
        [preserved](const llvm::GlobalValue &func) { return (func.getName().str() == preserved->getName().str()); }));
    mpm.add(llvm::createGlobalDCEPass());
}

DecimalSplitValue LLVMEngine::Split(llvm::Value *fullValue)
{
    LLVMTypes types(*context);
    const int32_t intValue = 64;
    auto high = builder->CreateLShr(fullValue, types.CreateConstant128(intValue), "split_high");
    high = builder->CreateTrunc(high, types.I64Type(), "split_high");
    auto low = builder->CreateTrunc(fullValue, types.I64Type(), "split_low");
    return DecimalSplitValue(high, low);
}

llvm::Value *LLVMEngine::ToInt128(llvm::Value *high, llvm::Value *low) const
{
    LLVMTypes types(*context);
    auto value = builder->CreateSExt(high, types.I128Type());
    const int32_t intValue = 64;
    value = builder->CreateShl(value, types.CreateConstant128(intValue));
    value = builder->CreateAdd(value, builder->CreateZExt(low, types.I128Type()));
    return value;
}

std::shared_ptr<DecimalValue> LLVMEngine::BuildDecimalValue(llvm::Value *data, omniruntime::type::DataType &retType,
    llvm::Value *isNull)
{
    LLVMTypes llvmTypes(*context);
    llvm::Value *precision;
    llvm::Value *scale;
    if (TypeUtil::IsDecimalType(retType.GetId())) {
        precision = llvmTypes.CreateConstantInt(static_cast<DecimalDataType &>(retType).GetPrecision());
        scale = llvmTypes.CreateConstantInt(static_cast<DecimalDataType &>(retType).GetScale());
    } else {
        precision = llvmTypes.CreateConstantInt(0);
        scale = llvmTypes.CreateConstantInt(0);
    }
    return std::make_shared<DecimalValue>(data, isNull, precision, scale);
}

llvm::Value *LLVMEngine::CallDecimalFunction(const std::string &fnName, llvm::Type *retType,
    const std::vector<llvm::Value *> &args, llvm::Value *executionContextPtr,
    omniruntime::op::OverflowConfig *overflowConfig, llvm::Value *overflowNull)
{
    LLVMTypes llvmTypes(*context);
    std::vector<llvm::Value *> disassembledArgs;

    if (executionContextPtr != nullptr) {
        if (overflowConfig != nullptr &&
            overflowConfig->GetOverflowConfigId() == omniruntime::op::OVERFLOW_CONFIG_NULL) {
            disassembledArgs.push_back(overflowNull);
        } else {
            disassembledArgs.push_back(executionContextPtr);
        }
    }
    for (auto &arg : args) {
        if (arg->getType() == llvmTypes.I128Type()) {
            auto split = Split(arg);
            disassembledArgs.push_back(const_cast<llvm::Value *>(split.GetHigh()));
            disassembledArgs.push_back(const_cast<llvm::Value *>(split.GetLow()));
        } else {
            disassembledArgs.push_back(arg);
        }
    }
    auto f = modulePtr->getFunction(fnName);
    llvm::Value *result = nullptr;
    if (f) {
        if (retType == llvmTypes.I128Type()) {
            auto outHighPtr = builder->CreateAlloca(llvmTypes.I64Type(), nullptr, "out_high");
            auto outLowPtr = builder->CreateAlloca(llvmTypes.I64Type(), nullptr, "out_low");
            disassembledArgs.push_back(outHighPtr);
            disassembledArgs.push_back(outLowPtr);

            CreateCall(f, disassembledArgs, const_cast<std::string &>(fnName));

            auto outHigh = builder->CreateLoad(llvmTypes.I64Type(), outHighPtr);
            auto outLow = builder->CreateLoad(llvmTypes.I64Type(), outLowPtr);
            result = ToInt128(outHigh, outLow);
        } else {
            result = CreateCall(f, disassembledArgs, const_cast<std::string &>(fnName));
        }
        llvm::InlineFunctionInfo inlineFunctionInfo;
        llvm::InlineFunction(*((llvm::CallInst *)result), inlineFunctionInfo);
    } else {
        LogWarn("Unable to generate function : %s", fnName.c_str());
    }
    return result;
}
}
}