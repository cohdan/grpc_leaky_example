import argparse
import subprocess
import os
import sys
import shutil
import platform

googletest_dir_prefix = "googletest"
spdlog_dir_prefix = "spdlog"
grpc_dir_prefix = "grpc"
zlib_dir_prefix = "zlib"

def update_directory_paths(args):
    if args.clone_git:
        args.root_dir = args.clone_target

    root_abs_path = os.path.abspath(args.root_dir)
    args.download_dir = os.path.join(root_abs_path, "download")
    args.install_dir = os.path.join(root_abs_path, "ext_libs/install")
    return root_abs_path

def run_command(command):
    try:
        subprocess.run(command, check=True, shell=True)
    except subprocess.CalledProcessError as e:
        print(f"Error executing command: {e}", file=sys.stderr)
        sys.exit(1)

def get_directory_name_from_url(url):
    name = url.split('/')[-1].split('.tar.gz')[0]
    # Remove leading 'v' if it exists
    if name.startswith('v'):
        name = name[1:]
    return name

def main():
    parser = argparse.ArgumentParser(description="Setup a local agent-core development environment.\nNote: You should install git, gcc/clang (Linux/Max) and cmake before using this script.\n")
    parser.add_argument("--root_dir", default=".", help="Root directory for the operation.")
    parser.add_argument("--download_dir", default="download", help="Directory for downloads.")
    parser.add_argument("--install_dir", default="ext_libs/install", help="Installation directory.")
    parser.add_argument("--build_output_dir", default="build", help="Build output directory.")
    parser.add_argument("--protobuf_gen_dir", default="connectors/saas/Protos/gen", help="Protobuf generation directory.")
    parser.add_argument("--do_not_run_cmake", action="store_true", help="Whether to skip CMake configuration and build.", default=False)
    parser.add_argument("--create_dirs", default="yes", choices=["yes", "no"], help="Whether to create directories.")
    parser.add_argument("--abseil_url", default="https://github.com/abseil/abseil-cpp/releases/download/20240722.0/abseil-cpp-20240722.0.tar.gz", help="URL for Abseil.")
    parser.add_argument("--protobuf_url", default="https://github.com/protocolbuffers/protobuf/releases/download/v28.3/protobuf-28.3.tar.gz", help="URL for protobuf.")
    parser.add_argument("--zlib_url", default="https://github.com/madler/zlib/archive/refs/tags/v1.3.tar.gz", help="URL for zlib.")
    parser.add_argument("--grpc_url", default="https://github.com/grpc/grpc.git", help="URL for gRPC.")
    parser.add_argument("--grpc_version", default="v1.66.0", help="gRPC version.")
    parser.add_argument("--c_compiler", default="gcc", help="C compiler.")
    parser.add_argument("--cpp_compiler", default="g++", help="C++ compiler.")
    parser.add_argument("--clone_git", action="store_true", help="Whether to clone the Git repository.")
    parser.add_argument("--cores_to_use", type=int, default=8, help="Number of cores to use when building.")

    args = parser.parse_args()

    arch_map = {'x86_64': 'x86_64', 'AMD64': 'x86_64', 'aarch64': 'aarch64', 'arm64': 'aarch64'}
    plat_machine = platform.machine()
    arch = arch_map.get(plat_machine)
    if not arch:
        print(f"Unsupported cpu architecture {plat_machine}")
        exit(1)

    root_abs_path = update_directory_paths(args)

    # Delete python script default environment vars as they somehow messes up the protobuf compilation.
    if 'SDKROOT' in os.environ:
        del os.environ['SDKROOT']
    if 'CPATH' in os.environ:
        del os.environ['CPATH']
    if 'LIBRARY_PATH' in os.environ:
        del os.environ['LIBRARY_PATH']

    # Create necessary directories
    if args.create_dirs == "yes":
        os.makedirs(args.download_dir, exist_ok=True)
        os.makedirs(args.install_dir, exist_ok=True)


    # Download and build Abseil
    abseil_dir = f"{args.download_dir}/{get_directory_name_from_url(args.abseil_url)}"
    run_command(f"wget {args.abseil_url} -P {args.download_dir}")
    run_command(f"tar xfz {args.download_dir}/{args.abseil_url.split('/')[-1]} -C {args.download_dir}")
    abseil_build_dir = os.path.join(abseil_dir, "release")
    os.makedirs(abseil_build_dir, exist_ok=True)
    abseil_cmake_cmd = (
        f"cmake -B {abseil_build_dir} -DCMAKE_CXX_COMPILER={args.cpp_compiler} -DABSL_BUILD_TESTING=OFF -DBUILD_TESTING=OFF "
        f"-DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17 -DCMAKE_PREFIX_PATH={args.install_dir} "
        f"-DCMAKE_INSTALL_PREFIX={args.install_dir} -DABSL_ENABLE_INSTALL=ON -S {abseil_dir}/"
    )
    run_command(abseil_cmake_cmd)
    run_command(f"cmake --build {abseil_build_dir} --config Debug -j {args.cores_to_use}")
    run_command(f"cmake --build {abseil_build_dir} --target install")

    # Download and build protobuf
    protobuf_dir = f"{args.download_dir}/{get_directory_name_from_url(args.protobuf_url)}"
    run_command(f"wget {args.protobuf_url} -P {args.download_dir}")
    run_command(f"tar xfz {args.download_dir}/{args.protobuf_url.split('/')[-1]} -C {args.download_dir}")
    protobuf_build_dir = os.path.join(protobuf_dir, "release")
    os.makedirs(protobuf_build_dir, exist_ok=True)
    protobuf_cmake_cmd = (
        f"cmake -B {protobuf_build_dir} -DCMAKE_CXX_COMPILER={args.cpp_compiler} -DCMAKE_C_COMPILER={args.c_compiler} "
        f"-DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH={args.install_dir} -DCMAKE_INSTALL_PREFIX={args.install_dir} "
        f"-DCMAKE_CXX_STANDARD=17 -Dprotobuf_ABSL_PROVIDER=package -Dprotobuf_BUILD_TESTS=OFF -S {protobuf_dir}/"
    )
    run_command(protobuf_cmake_cmd)
    run_command(f"cmake --build {protobuf_build_dir} --config Debug -j {args.cores_to_use}")
    run_command(f"cmake --build {protobuf_build_dir} --target install")

    # Download and build zlib
    zlib_dir = f"{args.download_dir}/{zlib_dir_prefix}-{get_directory_name_from_url(args.zlib_url)}"
    run_command(f"wget {args.zlib_url} -P {args.download_dir}")
    run_command(f"tar xfz {args.download_dir}/{args.zlib_url.split('/')[-1]} -C {args.download_dir}")
    zlib_build_dir = os.path.join(zlib_dir, "build")
    os.makedirs(zlib_build_dir, exist_ok=True)
    zlib_cmake_cmd = (
        f"cmake -B {zlib_build_dir} -DCMAKE_C_COMPILER={args.c_compiler} "
        f"-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX={args.install_dir} -S {zlib_dir}/"
    )
    run_command(zlib_cmake_cmd)
    run_command(f"cmake --build {zlib_build_dir} --config Release -j {args.cores_to_use}")
    run_command(f"cmake --build {zlib_build_dir} --target install")

    # Download and build gRPC
    grpc_dir = f"{args.download_dir}/{grpc_dir_prefix}"
    run_command(f"git clone --recurse-submodules -b {args.grpc_version} --depth 1 --shallow-submodules {args.grpc_url} {grpc_dir}")
    grpc_build_dir = os.path.join(grpc_dir, "cmake/build")
    os.makedirs(grpc_build_dir, exist_ok=True)
    grpc_cmake_cmd = (
        f"cmake -B {grpc_build_dir} -DCMAKE_CXX_COMPILER={args.cpp_compiler} -DCMAKE_C_COMPILER={args.c_compiler} "
        f"-DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17 -DgRPC_INSTALL=ON -DgRPC_ABSL_PROVIDER=package -DgRPC_PROTOBUF_PROVIDER=package "
        f"-DgRPC_ZLIB_PROVIDER=package -DCMAKE_INSTALL_PREFIX={args.install_dir} -S {grpc_dir}"
    )
    run_command(grpc_cmake_cmd)
    run_command(f"cmake --build {grpc_build_dir} --config Debug -j {args.cores_to_use}")
    run_command(f"cmake --build {grpc_build_dir} --target install")

    if args.create_dirs == "yes":
        shutil.rmtree(args.download_dir)

if __name__ == "__main__":
    main()
