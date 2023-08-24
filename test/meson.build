add_languages('cpp', native: false)

sqsh_test = [
    'archive/compression_options.c',
    'easy/directory.c',
    'easy/file.c',
    'cpp-test.cpp',
    'directory/directory_iterator.c',
    'extract/extract.c',
    'extract/extract_manager.c',
    'extract/lzma.c',
    'file/file_iterator.c',
    'file/file_reader.c',
    'fuzzer_repro.c',
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
    'file/file.c',
    'integration.c',
    'mapper/map_iterator.c',
    'mapper/map_reader.c',
    'metablock/metablock_iterator.c',
    'metablock/metablock_reader.c',
    'nasty.c',
    'reader/reader.c',
    'tree/walker.c',
    'xattr/xattr_iterator.c',
]
sqsh_extended_test = [
    'selftest.sh',
    'tools/cat/large-file-compressed.sh',
    'tools/cat/large-file-uncompressed.sh',
    'tools/read-chunk/tail.sh',
    'tools/unpack/repack.sh',
    'tools/unpack/pathtraversal/pathtraversal.sh',
]
sqsh_extended_fs_test = [
    'tools/fs/large-file.sh',
    'tools/fs/repack.sh',
]
sqsh_extra_source = {
    'fuzzer_repro.c': '../fuzzer/simple.c',
}
sqsh_failing_test = []
sqsh_test_util = [
    'util.c',
]
test_c_args = []

cc = meson.get_compiler('c')
if cc.get_id() == 'gcc'
    test_c_args += '-Wno-pedantic'
elif cc.get_id() == 'clang'
    test_c_args += '-Wno-gnu-designator'
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

squashfs_h = custom_target(
    'squashfs_image.c',
    input: squashfs,
    output: 'squashfs_image.c',
    command: ['../utils/f2h.sh', 'squashfs_image', '@INPUT@', '@OUTPUT@'],
)
sqsh_extra_source += {
    'integration.c': squashfs_h,
}

read_chunk = executable(
    'read-chunk',
    'read-chunk.c',
    install: false,
    c_args: test_c_args,
    dependencies: libsqsh_dep,
)

testlib_dep = subproject('cextras').get_variable('testlib_dep')
foreach p : sqsh_test
    sources = [p, sqsh_test_util]
    if sqsh_extra_source.has_key(p)
        sources += sqsh_extra_source[p]
    endif
    this_c_args = []
    this_c_args += test_c_args
    t = executable(
        p.underscorify(),
        sources,
        install: false,
        c_args: this_c_args,
        link_with: libsqsh.get_static_lib(),
        dependencies: [threads_dep, testlib_dep, cextras_dep],
    )
    test(p, t)
endforeach

if get_option('test') == 'extended'
    foreach p : sqsh_extended_test
        test(
            p,
            files(p),
            should_fail: sqsh_failing_test.contains(p),
            depends: [
                read_chunk,
                tools['sqsh-ls'],
                tools['sqsh-cat'],
                tools['sqsh-unpack'],
            ],
            env: {
                'BUILD_DIR': meson.current_build_dir(),
                'SOURCE_ROOT': meson.project_source_root(),
                'MKSQUASHFS': mksquashfs.full_path(),
                'READ_CHUNK': read_chunk.full_path(),
                'SQSH_CAT': tools['sqsh-cat'].full_path(),
                'SQSH_LS': tools['sqsh-ls'].full_path(),
                'SQSH_UNPACK': tools['sqsh-unpack'].full_path(),
            },
        )
    endforeach

    fs_impl = []
    if fuse3_dep.found()
        fs_impl += tools['sqshfs']
    endif
    if fuse2_dep.found()
        fs_impl += tools['sqshfs2']
    endif

    foreach impl : fs_impl
        foreach p : sqsh_extended_fs_test
            test(
                p + ' (' + impl.name() + ')',
                files(p),
                should_fail: sqsh_failing_test.contains(p),
                depends: [
                    read_chunk,
                    fs_impl,
                    tools['sqsh-ls'],
                    tools['sqsh-cat'],
                    tools['sqsh-unpack'],
                ],
                env: {
                    'SQSHFS': impl.full_path(),
                    'BUILD_DIR': meson.current_build_dir(),
                    'SOURCE_ROOT': meson.project_source_root(),
                    'MKSQUASHFS': mksquashfs.full_path(),
                    'READ_CHUNK': read_chunk.full_path(),
                    'SQSH_CAT': tools['sqsh-cat'].full_path(),
                    'SQSH_LS': tools['sqsh-ls'].full_path(),
                    'SQSH_UNPACK': tools['sqsh-unpack'].full_path(),
                },
            )
        endforeach
    endforeach
endif
