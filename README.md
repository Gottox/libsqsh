# sqsh-tools [![CI](https://github.com/Gottox/libsqsh/actions/workflows/ci.yaml/badge.svg)](https://github.com/Gottox/libsqsh/actions/workflows/ci.yaml) [![codecov](https://codecov.io/github/Gottox/sqsh-tools/graph/badge.svg?token=AM5COPDMH0)](https://codecov.io/github/Gottox/sqsh-tools) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=Gottox_libsqsh&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=Gottox_libsqsh) [![License](https://img.shields.io/badge/License-BSD_2--Clause-orange.svg)](https://opensource.org/licenses/BSD-2-Clause)

squashfs is an open and free compressed read-only filesystem. It is used in
embedded devices, live-CDs, or in packaging. The original implementation
resides in the linux kernel, but there are also userspace implementations.

This project provides a userspace implementation of the squashfs filesystem
containing a set of command line tools and a C library.

## users of sqsh-tools

* [radare2](https://www.radare.org/) - A reverse engineering framework.
  Integrates [libsqsh](./libsqsh/README.md) into their virtual filesystem
  framework to explore inline squashfs archives.
* [Filer](https://github.com/probonopd/Filer) - A file manager for Unix systems.
  Uses [libsqsh](./libsqsh/README.md) to show previews of AppImages without
  mounting them first.
* [sqsh-rs](https://github.com/Dr-Emann/sqsh-rs) - Rust bindings for [libsqsh](./libsqsh/README.md).

## Building

### Dependencies

* **libc**: Any POSIX compliant libc should work. Open a bug if it doesn't.
* [**zlib**](https://zlib.net/) *optional*: For gzip compression support.
* [**liblz4**](https://lz4.org/) *optional*: For lz4 compression support.
* [**liblzma**](https://tukaani.org/xz) *optional*: For lzma compression support.
* [**libzstd**](https://facebook.github.io/zstd/) *optional*: For zstd compression
  support.
* [**fuse3**](https://libfuse.github.io/) *optional*: For mounting squashfs
  archives.
* [**fuse2**](https://libfuse.github.io/) *optional*: For mounting squashfs 
  archives on systems that don't support fuse3. e.g. OpenBSD.
* [**libcurl**](https://curl.se/) *optional*: For transparently reading squashfs
  archives from an URL.

Note that to do anything useful with *libsqsh*, you need to have at least one of the
compression libraries enabled.

### Compile & Install

```bash
meson setup build
cd build
meson compile
meson install
```

## tools

This project provides a set of command line tools to interact with squashfs
archives.

* [`sqsh-cat`](./tools/man/sqsh-cat.1.in): Prints the content of one or 
  multiple files to stdout.
* [`sqsh-ls`](./tools/man/sqsh-ls.1.in): Lists the content of a directory.
* [`sqsh-stat`](./tools/man/sqsh-stat.1.in): Prints the metadata of a file,
  directory, or the whole archive.
* [`sqsh-unpack`](./tools/man/sqsh-unpack.1.in): Extracts a squashfs archive to
  a directory.
* [`sqsh-xattr`](./tools/man/sqsh-xattr.1.in): Prints the extended attributes of
  a file or directory.
* [`sqshfs`](./tools/man/sqshfs.1.in): Mounts a squashfs archive to a directory.
  There are two versions of this tool, one for fuse3 and one for fuse2.

## libsqsh

sqsh-tools also provides a C library to interact with squashfs archives. There are two APIs
available:

* **Easy API**: This API is designed to be easy to use and provides a
  simple interface to read the content of a squashfs archive. It is the
  recommended API to use.

* **Low-level API**: This API provides a more fine-grained control over the
  squashfs archive. It allows zero copy reading of the archive.

For more information, see the [libsqsh README](./libsqsh/README.md).

## LZO2

LZO2 is a fast compression algorithm. Unfortunately the current implementation
is GPL licensed and therefore not included in this library. If you want to use 
LZO2 there's and independent glue library called [libsqsh-lzo](https://github.com/Gottox/libsqsh-lzo).
