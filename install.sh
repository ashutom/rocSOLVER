#!/usr/bin/env bash

# #################################################
# helper functions
# #################################################
function display_help()
{
cat <<EOF

rocSOLVER library build & installation helper script.

Usage:
  $0 (build rocsolver and put library files at ./build/rocsolver-install)
  $0 <options> (modify default behavior according to the following flags)

Options:
  --help                      Print this help message.

  --build_dir <builddir>      Specify path to the configure & build process output directory. 
                              Relative paths are relative to the current directory.
                              (Default is ./build)

  --lib_dir <libdir>          Specify path to the directory where the library generated files 
                              will be located. Relative paths are relative to builddir/release 
                              or builddir/debug, depending on the build type. 
                              (Default is builddir/release/rocsolver-install)

  --install_dir <installdir>  Specify path to the directory where the library package 
                              (when generated) will be installed. Use only absolute paths. 
                              (Default is /opt/rocm)

  --rocblas_dir <blasdir>     Specify path to the companion rocBLAS-library root directory. 
                              Use only absolute paths.
                              (Default is /opt/rocm/rocblas)
   
  -g | --debug                Pass this flag to build in Debug mode (equivalent to set CMAKE_BUILD_TYPE=Debug).
                              (Default build type is Release)

  -p | --package              Pass this flag to generate library package after build.

  -i | --install              Pass this flag to generate and install library package after build.

  -d | --dependencies         Pass this flag to also build and install external dependencies.
                              Dependecies are to be installed in /usr/local. This should be done only once.
                              (this does not install rocBLAS nor ROCm software stack)

  -c | --clients              Pass this flag to also build the library clients benchmark and gtest.
                              (Generated binaries will be located at builddir/clients/staging)

  -h | --hip-clang            Pass this flag to build library for amdgpu backend using hip-clang.

  -s | --static               Pass this flag to build rocsolver as a static library.

  -r | --relocatable          Pass this to add RUNPATH(based on ROCM_RPATH) and remove ldconf entry.

EOF
}

# This function is helpful for dockerfiles that do not have sudo installed, but the default user is root
# true is a system command that completes successfully, function returns success
# prereq: ${ID} must be defined before calling
supported_distro( )
{
  if [ -z ${ID+foo} ]; then
    printf "supported_distro(): \$ID must be set\n"
    exit 2
  fi

  case "${ID}" in
    ubuntu|centos|rhel|fedora|sles|opensuse-leap)
        true
        ;;
    *)  printf "This script is currently supported on Ubuntu, CentOS, RHEL, SLES, OpenSUSE-Leap, and Fedora\n"
        exit 2
        ;;
  esac
}

# This function is helpful for dockerfiles that do not have sudo installed, but the default user is root
check_exit_code( )
{
  if (( $1 != 0 )); then
    exit $1
  fi
}

# This function is helpful for dockerfiles that do not have sudo installed, but the default user is root
elevate_if_not_root( )
{
  local uid=$(id -u)

  if (( ${uid} )); then
    sudo $@
    check_exit_code "$?"
  else
    $@
    check_exit_code "$?"
  fi
}

# Take an array of packages as input, and install those packages with 'apt' if they are not already installed
install_apt_packages( )
{
  package_dependencies=("$@")
  for package in "${package_dependencies[@]}"; do
    if [[ $(dpkg-query --show --showformat='${db:Status-Abbrev}\n' ${package} 2> /dev/null | grep -q "ii"; echo $?) -ne 0 ]]; then
      printf "\033[32mInstalling \033[33m${package}\033[32m from distro package manager\033[0m\n"
      elevate_if_not_root apt install -y --no-install-recommends ${package}
    fi
  done
}

# Take an array of packages as input, and install those packages with 'yum' if they are not already installed
install_yum_packages( )
{
  package_dependencies=("$@")
  for package in "${package_dependencies[@]}"; do
    if [[ $package == *-PyYAML ]] || [[ $(yum list installed ${package} &> /dev/null; echo $? ) -ne 0 ]]; then
      printf "\033[32mInstalling \033[33m${package}\033[32m from distro package manager\033[0m\n"
      elevate_if_not_root yum -y --nogpgcheck install ${package}
    fi
  done
}

# Take an array of packages as input, and install those packages with 'dnf' if they are not already installed
install_dnf_packages( )
{
  package_dependencies=("$@")
  for package in "${package_dependencies[@]}"; do
    if [[ $package == *-PyYAML ]] || [[ $(dnf list installed ${package} &> /dev/null; echo $? ) -ne 0 ]]; then
      printf "\033[32mInstalling \033[33m${package}\033[32m from distro package manager\033[0m\n"
      elevate_if_not_root dnf install -y ${package}
    fi
  done
}

install_zypper_packages( )
{
    package_dependencies=("$@")
    for package in "${package_dependencies[@]}"; do
        if [[ $(rpm -q ${package} &> /dev/null; echo $? ) -ne 0 ]]; then
            printf "\033[32mInstalling \033[33m${package}\033[32m from distro package manager\033[0m\n"
            elevate_if_not_root zypper install -y ${package}
        fi
    done
}

# Take an array of packages as input, and delegate the work to the appropriate distro installer
# prereq: ${ID} must be defined before calling
# prereq: ${build_clients} must be defined before calling
install_packages( )
{
  if [ -z ${ID+foo} ]; then
    printf "install_packages(): \$ID must be set\n"
    exit 2
  fi

  if [ -z ${build_clients+foo} ]; then
    printf "install_packages(): \$build_clients must be set\n"
    exit 2
  fi

  # dependencies needed to build the rocblas library
  local library_dependencies_ubuntu=( "make" "cmake-curses-gui" "pkg-config"
                                      "python2.7" "python3" "python-yaml" "python3-yaml" "python3*-distutils"
                                      "llvm-6.0-dev" "zlib1g-dev" "wget")
  local library_dependencies_centos=( "epel-release"
                                      "make" "cmake3" "rpm-build"
                                      "python34" "PyYAML" "python3*-PyYAML" "python3*-distutils-extra"
                                      "gcc-c++" "llvm7.0-devel" "llvm7.0-static"
                                      "zlib-devel" "wget" )
  local library_dependencies_fedora=( "make" "cmake" "rpm-build"
                                      "python34" "PyYAML" "python3*-PyYAML" "python3*-distutils-extra"
                                      "gcc-c++" "libcxx-devel" "zlib-devel" "wget" )
  local library_dependencies_sles=(   "make" "cmake" "python3-PyYAM" "python3-distutils-extra"
                                      "gcc-c++" "libcxxtools9" "rpm-build" "wget" )

  if [[ "${build_hip_clang}" == false ]]; then
    # Installing rocm-dev installs hip-hcc, which overwrites the hip-vdi runtime
    library_dependencies_ubuntu+=( "rocm-dev" )
    library_dependencies_centos+=( "rocm-dev" )
    library_dependencies_fedora+=( "rocm-dev" )
    library_dependencies_sles+=( "rocm-dev" )
  fi

  # dependencies to build the client
  local client_dependencies_ubuntu=( "gfortran" "libomp-dev" "libboost-program-options-dev")
  local client_dependencies_centos=( "devtoolset-7-gcc-gfortran" "libgomp" "boost-devel" )
  local client_dependencies_fedora=( "gcc-gfortran" "libgomp" "boost-devel" )
  local client_dependencies_sles=( "gcc-fortran" "libgomp1" "libboost_program_options1_66_0-devel" )

  case "${ID}" in
    ubuntu)
      elevate_if_not_root apt update
      install_apt_packages "${library_dependencies_ubuntu[@]}"

      if [[ "${build_clients}" == true ]]; then
        install_apt_packages "${client_dependencies_ubuntu[@]}"
      fi
      ;;

    centos|rhel)
      install_yum_packages "${library_dependencies_centos[@]}"

      if [[ "${build_clients}" == true ]]; then
        install_yum_packages "${client_dependencies_centos[@]}"
      fi
      ;;

    fedora)
      install_dnf_packages "${library_dependencies_fedora[@]}"

      if [[ "${build_clients}" == true ]]; then
        install_dnf_packages "${client_dependencies_fedora[@]}"
      fi
      ;;

    sles|opensuse-leap)
       install_zypper_packages "${client_dependencies_sles[@]}"

        if [[ "${build_clients}" == true ]]; then
            install_zypper_packages "${client_dependencies_sles[@]}"
        fi
        ;;
    *)
      echo "This script is currently supported on Ubuntu, CentOS, RHEL, SLES, OpenSUSE-Leap, and Fedora"
      exit 2
      ;;
  esac
}

# #################################################
# Pre-requisites check
# #################################################
# Exit code 0: all is well
# Exit code 1: problems with getopt
# Exit code 2: problems with supported platforms

# check if getopt command is installed
type getopt > /dev/null
if [[ $? -ne 0 ]]; then
  echo "This script uses getopt to parse arguments; try installing the util-linux package";
  exit 1
fi

# os-release file describes the system
if [[ -e "/etc/os-release" ]]; then
  source /etc/os-release
else
  echo "This script depends on the /etc/os-release file"
  exit 2
fi

# The following function exits script if an unsupported distro is detected
supported_distro

# #################################################
# global variables
# #################################################
install_package=false
build_package=false
install_dependencies=false
static_lib=false
build_clients=false
build_hip_clang=false
lib_dir=rocsolver-install
install_dir=/opt/rocm
rocblas_dir=/opt/rocm/rocblas
build_dir=./build
build_type=Release
build_relocatable=false

rocm_path=/opt/rocm
if ! [ -z ${ROCM_PATH+x} ]; then
    rocm_path=${ROCM_PATH}
fi

# #################################################
# Parameter parsing
# #################################################

# check if we have a modern version of getopt that can handle whitespace and long parameters
getopt -T
if [[ $? -eq 4 ]]; then
  GETOPT_PARSE=$(getopt --name "${0}" --longoptions help,install,package,clients,dependencies,debug,hip-clang,build_dir:,rocblas_dir:,lib_dir:,install_dir:,static:,relocatable --options hipcdgsr -- "$@")
else
  echo "Need a new version of getopt"
  exit 1
fi

if [[ $? -ne 0 ]]; then
  echo "getopt invocation failed; could not parse the command line";
  exit 1
fi

eval set -- "${GETOPT_PARSE}"

while true; do
  case "${1}" in
    --help)
        display_help
        exit 0
        ;;
    -i|--install)
        build_package=true
        install_package=true
        shift ;;
    -p|--package)
        build_package=true
        shift ;;
    -d|--dependencies)
        install_dependencies=true
        shift ;;
    -c|--clients)
        build_clients=true
        shift ;;
    -g|--debug)
        build_type=Debug
        shift ;;
    -s|--static)
        static_lib=true
        shift ;;
    -h|--hip-clang)
        build_hip_clang=true
        shift ;;
    --build_dir)
        build_dir=${2}
        shift 2;;
    --lib_dir)
        lib_dir=${2}
        shift 2 ;;
    --install_dir)
        install_dir=${2}
        shift 2 ;;
    --rocblas_dir)
        rocblas_dir=${2}
        shift 2 ;;
    -r|--relocatable)
        build_relocatable=true
        shift ;;
    --) shift ; break ;;
    *)  
        echo "Unexpected command line parameter received; aborting";
        exit 1
        ;;
  esac
done

set -x
printf "\033[32mCreating project build directory in: \033[33m${build_dir}\033[0m\n"

# #################################################
# prep
# #################################################
# ensure a clean build environment
rm -rf ${build_dir}

# Default cmake executable is called cmake
cmake_executable=cmake

case "${ID}" in
  centos|rhel)
  cmake_executable=cmake3
  ;;
esac

main=`pwd`

# #################################################
# dependencies
# #################################################
if [[ "${install_dependencies}" == true ]]; then
  install_packages

  if [[ "${build_clients}" == true ]]; then
    # The following builds googletest & lapack from source, installs into cmake default /usr/local
    pushd .
    printf "\033[32mBuilding \033[33mgoogletest & lapack\033[32m from source; installing into \033[33m/usr/local\033[0m\n"
    mkdir -p ${build_dir}/deps && cd ${build_dir}/deps
    ${cmake_executable} -lpthread -DBUILD_BOOST=OFF ${main}/rocblascommon/deps
    make -j$(nproc)
    elevate_if_not_root make install
    popd
  fi
fi

# We append customary rocm path; if user provides custom rocm path in ${path}, our
# hard-coded path has lesser priority
if [[ "${build_hip_clang}" == true ]]; then
  export PATH=${PATH}:${rocm_path}/bin:${rocm_path}/hip/bin:${rocm_path}/llvm/bin
else
  export PATH=${PATH}:${rocm_path}/bin:${rocm_path}/hip/bin:${rocm_path}/hcc/bin
fi

# #################################################
# configure & build
# #################################################
pushd .
cmake_common_options=""
cmake_client_options=""

mkdir -p ${build_dir} && cd ${build_dir}

if [[ "${build_type}" == Debug ]]; then
  mkdir -p debug && cd debug
else
  mkdir -p release && cd release
fi

cmake_common_options="${cmake_common_options} -DROCM_PATH=${rocm_path} -Drocblas_DIR=${rocblas_dir}/lib/cmake/rocblas -DCPACK_SET_DESTDIR=OFF -DCMAKE_INSTALL_PREFIX=${lib_dir} -DCPACK_PACKAGING_INSTALL_PREFIX=${install_dir} -DCMAKE_BUILD_TYPE=${build_type}" 

if [[ "${static_lib}" == true ]]; then
  cmake_common_options="${cmake_common_options} -DBUILD_SHARED_LIBS=OFF"
fi

if [[ "${build_clients}" == true ]]; then
  cmake_client_options="${cmake_client_options} -DBUILD_CLIENTS_TESTS=ON -DBUILD_CLIENTS_BENCHMARKS=ON"
fi

rocm_rpath=""
if [[ "${build_relocatable}" == true ]]; then
    rocm_rpath=" -Wl,--enable-new-dtags -Wl,--rpath,/opt/rocm/lib:/opt/rocm/lib64"
    if ! [ -z ${ROCM_RPATH+x} ]; then
        rocm_rpath=" -Wl,--enable-new-dtags -Wl,--rpath,${ROCM_RPATH}"
    fi
    cmake_common_options="${cmake_common_options} -DROCM_DISABLE_LDCONFIG=ON"
fi

compiler="hcc"
if [[ "${build_hip_clang}" == true ]]; then
  compiler="hipcc"
fi

case "${ID}" in
  centos|rhel)
    cmake_common_options="${cmake_common_options} -DCMAKE_FIND_ROOT_PATH=/usr/lib64/llvm7.0/lib/cmake/"
    ;;
esac

CXX=${compiler} ${cmake_executable} ${cmake_common_options} ${cmake_client_options} -DCMAKE_SHARED_LINKER_FLAGS="${rocm_rpath}" ${main}

make -j$(nproc) install
check_exit_code "$?"

# #################################################
# package build & install
# #################################################
# installing through package manager, which makes uninstalling easy
if [[ "${build_package}" == true ]]; then
  make package
  check_exit_code "$?"

  if [[ "${install_package}" == true ]]; then
    case "${ID}" in
      ubuntu)
        elevate_if_not_root dpkg -i rocsolver-*.deb
        ;;
      centos|rhel)
        elevate_if_not_root yum -y localinstall rocsolver-*.rpm
        ;;
      fedora)
        elevate_if_not_root dnf install rocsolver-*.rpm
        ;;
      sles|opensuse-leap)
	elevate_if_not_root zypper -n --no-gpg-checks install rocsolver-*.rpm
        ;;
    esac
  fi
fi
popd
