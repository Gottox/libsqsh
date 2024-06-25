add_languages('cpp', native: false)

sqsh_test = [
    'check_version.c',
    'cpp-test.cpp',
    'archive/compression_options.c',
    'archive/inode_map.c',
    'directory/directory_iterator.c',
    'easy/directory.c',
    'easy/file.c',
    'easy/xattr.c',
    'extract/extract_manager.c',
    'file/file.c',
    'file/file_iterator.c',
    'file/file_reader.c',
    'integration.c',
    'metablock/metablock_iterator.c',
    'metablock/metablock_reader.c',
    'mapper/map_iterator.c',
    'mapper/map_reader.c',
    'nasty.c',
    'reader/reader.c',
    'tree/path_resolver.c',
    'tree/traversal.c',
    'tree/walker.c',
    'xattr/xattr_iterator.c',
    'include_tests/sqsh_archive.c',
    'include_tests/sqsh_easy.c',
    'include_tests/sqsh_common.c',
    'include_tests/sqsh_directory.c',
    'include_tests/sqsh_error.c',
    'include_tests/sqsh_file.c',
    'include_tests/sqsh_mapper.c',
    'include_tests/sqsh_table.c',
    'include_tests/sqsh_tree.c',
    'include_tests/sqsh_xattr.c',
    'extract/extract.c',
]
sqsh_extra_source = {}
sqsh_failing_test = []
sqsh_test_util = [
    'util.c',
]
test_c_args = []
test_cpp_args = []

cc = meson.get_compiler('c')
if cc.get_id() == 'gcc'
    test_c_args += '-Wno-pedantic'
elif cc.get_id() == 'clang'
    test_c_args += '-Wno-gnu-designator'
    test_cpp_args += '-Wno-variadic-macros'
    test_cpp_args += '-Wno-c++11-extensions'
endif

mksquashfs = find_program('mksquashfs')
integration_create = files('integration_create.sh')
squashfs = custom_target(
    'squashfs.image',
    output: 'squashfs.image',
    env: {
        'MKSQUASHFS': mksquashfs.full_path(),
    },
    command: [integration_create, '', '@OUTPUT@', '@PRIVATE_DIR@'],
)

f2h = files('f2h.sh')
squashfs_h = custom_target(
    'squashfs_image.c',
    input: squashfs,
    output: 'squashfs_image.c',
    command: [f2h, 'squashfs_image', '@INPUT@', '@OUTPUT@'],
)
sqsh_extra_source += {
    'integration.c': squashfs_h,
}

foreach p : sqsh_test
    sources = [p, sqsh_test_util]
    if sqsh_extra_source.has_key(p)
        sources += sqsh_extra_source[p]
    endif
    this_c_args = [
        '-Wno-deprecated-declarations',
        '-DINTEGRATION_PATH="' + squashfs.full_path() + '"',
    ]
    this_c_args += test_c_args
    this_cpp_args = []
    this_cpp_args += test_cpp_args
    t = executable(
        p.underscorify(),
        sources,
        install: false,
        c_args: this_c_args,
        cpp_args: this_cpp_args,
        include_directories: [
            libsqsh_include,
            libsqsh_common_include,
            libsqsh_private_include,
            libmksqsh_include,
        ],
        link_with: [libsqsh.get_static_lib(), libmksqsh.get_static_lib()],
        dependencies: [threads_dep, utest_dep, cextras_dep, testlib_dep],
    )
    test(p, t, env: {'VERSION': meson.project_version()})
endforeach
