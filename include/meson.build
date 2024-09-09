headers = files(
    'sqsh.h',
    'sqsh_archive.h',
    'sqsh_common.h',
    'sqsh_content.h',
    'sqsh_data.h',
    'sqsh_directory.h',
    'sqsh_easy.h',
    'sqsh_error.h',
    'sqsh_file.h',
    'sqsh_mapper.h',
    'sqsh_posix.h',
    'sqsh_table.h',
    'sqsh_tree.h',
    'sqsh_utils.h',
    'sqsh_xattr.h',
)

libsqsh_include = include_directories('.')

if not meson.is_subproject()
    install_headers(headers, subdir: 'sqsh')
endif
