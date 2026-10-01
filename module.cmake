# wxl-unit-outline: per-extension build glue, included by the core's extension loop after the target
# is created. The module is header-only besides the core SDK, so the only extra step is deploying the
# INI beside the DLL (the core copies the DLL itself).

if(CLIENT_PATH)
    add_custom_command(TARGET ${wxl_ext_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CLIENT_PATH}/Extensions/${wxl_ext_name}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${wxl_ext_dir}/wxl-unit-outline.ini"
                "${CLIENT_PATH}/Extensions/${wxl_ext_name}/wxl-unit-outline.ini"
        COMMENT "Deploy wxl-unit-outline config -> ${CLIENT_PATH}/Extensions/${wxl_ext_name}")
endif()
