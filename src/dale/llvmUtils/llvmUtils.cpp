#include "llvmUtils.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <sys/stat.h>

#if D_LLVM_VERSION_ORD >= 36
#include "llvm/Transforms/Utils/Cloning.h"
#endif

#if D_LLVM_VERSION_ORD <= 32
#include "llvm/Support/Path.h"
#endif

#include "llvm/ADT/Triple.h"
#include "llvm/Support/Host.h"

namespace dale
{
std::string
getTriple()
{
#if D_LLVM_VERSION_ORD >= 32
    return llvm::sys::getDefaultTargetTriple();
#else
    return llvm::sys::getHostTriple();
#endif
}

void
linkModule(llvm::Linker *linker, llvm::Module *mod)
{
    std::string error;
    bool result;
#if D_LLVM_VERSION_ORD <= 32
    result = linker->LinkInModule(mod, &error);
#elif D_LLVM_VERSION_ORD <= 35
    result = linker->linkInModule(mod, &error);
#elif D_LLVM_VERSION_ORD <= 37
    result = linker->linkInModule(mod);
#else
    std::unique_ptr<llvm::Module> module_ptr(llvm::CloneModule(mod));
    result = linker->linkInModule(move(module_ptr));
#endif
    assert(!result && "unable to link module");
    _unused(result);
}

void
addDataLayout(PassManager *pass_manager, llvm::Module *mod)
{
#if D_LLVM_VERSION_ORD >= 37
#elif D_LLVM_VERSION_ORD >= 36
    pass_manager->add(new llvm::DataLayoutPass());
#elif D_LLVM_VERSION_ORD >= 35
    pass_manager->add(new llvm::DataLayoutPass(mod));
#elif D_LLVM_VERSION_ORD >= 32
    pass_manager->add(new llvm::DataLayout(mod));
#else
    pass_manager->add(new llvm::TargetData(mod));
#endif
}

#if D_LLVM_VERSION_ORD <= 34
std::auto_ptr<llvm::TargetMachine> target_sp;
#else
std::shared_ptr<llvm::TargetMachine> target_sp;
#endif
llvm::TargetMachine *
getTargetMachine(llvm::Module *last_module)
{
    llvm::Triple triple(last_module->getTargetTriple());
    if (triple.getTriple().empty()) {
        triple.setTriple(getTriple());
    }

    std::string Err;
    const llvm::Target *target =
        llvm::TargetRegistry::lookupTarget(triple.getTriple(), Err);
    assert(target && "cannot auto-select target for module");

#if D_LLVM_VERSION_ORD >= 32
    llvm::TargetOptions target_options;
#endif

    std::string Features;
    target_sp =
#if D_LLVM_VERSION_ORD <= 34
        std::auto_ptr<llvm::TargetMachine>
#else
        std::shared_ptr<llvm::TargetMachine>
#endif
        (target->createTargetMachine(
            triple.getTriple(), llvm::sys::getHostCPUName(),
            Features
#if D_LLVM_VERSION_ORD >= 32
            , target_options
#endif
#if D_LLVM_VERSION_ORD >= 39
            , llvm::Optional<llvm::Reloc::Model>()
#endif
        ));

    llvm::TargetMachine *tm = target_sp.get();
#if D_LLVM_VERSION_ORD <= 35
    tm->setAsmVerbosityDefault(true);
#endif
    return tm;
}

void
setDataLayout(llvm::Module *module, bool is_x86_64)
{
#if D_LLVM_VERSION_ORD <= 36
    static const char *x86_64_layout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128";
    static const char *x86_32_layout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:32:32";
    module->setDataLayout((is_x86_64) ? x86_64_layout : x86_32_layout);
#else
    llvm::TargetMachine *target_machine = getTargetMachine(module);
    module->setDataLayout(target_machine->createDataLayout());
#endif
}

void
populateLTOPassManager(llvm::PassManagerBuilder *pass_manager_builder,
                       PassManager *pass_manager)
{
#if D_LLVM_VERSION_ORD <= 35
    pass_manager_builder->populateLTOPassManager(*pass_manager, true, true);
#else
    pass_manager_builder->populateLTOPassManager(*pass_manager);
#endif
}

llvm_formatted_ostream*
getFormattedOstream(llvm::raw_fd_ostream *ostream)
{
#if D_LLVM_VERSION_ORD <= 36
    llvm_formatted_ostream *ostream_formatted =
        new llvm::formatted_raw_ostream(
            *ostream,
            llvm::formatted_raw_ostream::DELETE_STREAM
        );
    return ostream_formatted;
#else
    return ostream;
#endif
}

void
addPrintModulePass(PassManager *pass_manager,
                   llvm::raw_fd_ostream *ostream)
{
#if D_LLVM_VERSION_ORD <= 34
    pass_manager->add(llvm::createPrintModulePass(ostream));
#else
    pass_manager->add(llvm::createPrintModulePass(*ostream));
#endif
}

void
moduleDebugPass(llvm::Module *mod)
{
#if D_LLVM_VERSION_ORD <= 40
    mod->dump();
#else
    mod->print(llvm::outs(), nullptr);
#endif
#if D_LLVM_VERSION_ORD >= 35
    if (llvm::verifyModule(*mod, &(llvm::errs()))) {
	abort();
    }
#endif
}

void
addInlineAttribute(llvm::Function *fn)
{
#if D_LLVM_VERSION_ORD == 32
        fn->addFnAttr(llvm::Attributes::AlwaysInline);
#else
        fn->addFnAttr(llvm::Attribute::AlwaysInline);
#endif
}

llvm::FunctionType *
getFunctionType(llvm::Type *t, std::vector<llvm::Type*> &v, bool b) {
    llvm::ArrayRef<llvm::Type*> array_ref(v);
    return llvm::FunctionType::get(t, array_ref, b);
}

llvm::Constant *
getStringConstantArray(const char *data)
{
    return
        llvm::cast<llvm::Constant>(
#if D_LLVM_VERSION_ORD < 32
            llvm::ConstantArray::get(
#else
            llvm::ConstantDataArray::getString(
#endif
                *getContext(),
                data,
                true
            )
        );
}

llvm::ConstantPointerNull *
getNullPointer(llvm::Type *type)
{
    return
        llvm::ConstantPointerNull::get(
            llvm::cast<llvm::PointerType>(type)
        );
}

void
linkFile(llvm::Linker *linker, const char *path)
{
#if D_LLVM_VERSION_ORD <= 32
    const llvm::sys::Path bb(path);
    bool is_native = false;
    bool res = linker->LinkInFile(bb, is_native);
    assert(!res && "unable to link bitcode file");
#elif D_LLVM_VERSION_ORD <= 35
    llvm::SMDiagnostic sm_error;
    llvm::Module *path_mod = llvm::ParseIRFile(path, sm_error,
                                               *getContext());
    std::string error;
    bool res = linker->linkInModule(path_mod, &error);
    assert(!res && "unable to link bitcode file module");
#elif D_LLVM_VERSION_ORD <= 37
    llvm::SMDiagnostic sm_error;
    std::unique_ptr<llvm::Module> module_ptr(llvm::parseIRFile(path, sm_error,
                                                               *getContext()));
    bool res = linker->linkInModule(module_ptr.get());
    assert(!res && "unable to link bitcode file module");
#else
    llvm::SMDiagnostic sm_error;
    std::unique_ptr<llvm::Module> module_ptr(llvm::parseIRFile(path, sm_error,
                                                               *getContext()));
    bool res = linker->linkInModule(move(module_ptr));
    assert(!res && "unable to link bitcode file module");
#endif
    _unused(res);
}

llvm::LLVMContext*
getContext()
{
#if D_LLVM_VERSION_ORD <= 38
    return &llvm::getGlobalContext();
#else
    static llvm::LLVMContext* context = NULL;
    if (context) {
        return context;
    } else {
        context = new llvm::LLVMContext();
        return context;
    }
#endif
}
}
