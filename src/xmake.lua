target("libco")
    set_kind("$(kind)")
    set_basename("co")
    add_files("**.cc")

    add_options("with_openssl")
    add_options("with_libcurl")
    add_options("fpic")

    if has_config("with_libcurl") then
        add_packages("libcurl")
        add_packages("openssl")
    elseif has_config("with_openssl") then
        add_packages("openssl")
    end 

    if is_plat("windows", "mingw") then
        add_defines("WIN32_LEAN_AND_MEAN")
        add_defines("_WINSOCK_DEPRECATED_NO_WARNINGS")
        if is_kind("shared") then
            add_defines("CO_DLL")
        end
        add_files("log/StackWalker.cpp")
        add_files("co/detours/creatwth.cpp")
        add_files("co/detours/detours.cpp")
        add_files("co/detours/image.cpp")
        add_files("co/detours/modules.cpp")
        add_files("co/detours/disasm.cpp")
        if is_plat("windows") then
            if is_arch("x64") then
                add_files("co/context/context_x64.asm")
            else
                add_files("co/context/context_x86.asm")
            end
        else
            add_defines("_FILE_OFFSET_BITS=64")
            add_defines("__MINGW_USE_VC2005_COMPAT=1") -- use 64bit time_t
            add_files("co/context/context.S")
        end
    else
        includes("check_cincludes.lua")
        includes("check_cxxincludes.lua")
        includes("check_links.lua")
        check_cincludes("HAS_BACKTRACE_H", "backtrace.h")
        check_cxxincludes("HAS_CXXABI_H", "cxxabi.h")
        check_links("HAS_LIBBACKTRACE", "backtrace")
        if not is_plat("android") then
            add_syslinks("pthread", "dl")
        end
        add_defines("_FILE_OFFSET_BITS=64")
        add_files("co/context/context.S")
    end

