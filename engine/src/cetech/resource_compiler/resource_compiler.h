#pragma once

/*******************************************************************************
**** Includes
*******************************************************************************/

#include <cinttypes>

#include "rapidjson/document.h"

#include "celib/stringid_types.h"

#include "cetech/resource_compiler/compilatorapi.h"
#include "cetech/filesystem/filesystem.h"


/*******************************************************************************
**** Interface
*******************************************************************************/
namespace cetech {

    /***************************************************************************
    **** Resouce compiler.
    ***************************************************************************/
    namespace resource_compiler {

        /***********************************************************************
        **** Compile callback.
        ***********************************************************************/
        typedef void (* resource_compiler_clb_t)(const char*,
                                                 CompilatorAPI&);
        /***********************************************************************
        **** Register type compiler.
        ***********************************************************************/
        void register_compiler(StringId64_t type,
                               resource_compiler_clb_t clb);

        /***********************************************************************
        **** Compiler all resource in source and core dir
        ***********************************************************************/
        void compile_all();
    }

    /***************************************************************************
    **** Resouce compiler globals function.
    ***************************************************************************/
    namespace resource_compiler_globals {

        /***********************************************************************
        **** Init system.
        ***********************************************************************/
        void init();

        /***********************************************************************
        **** Shutdown system.
        ***********************************************************************/
        void shutdown();
    }
}
