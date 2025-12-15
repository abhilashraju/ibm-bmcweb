project(
    'ibm-bmcweb',
    'cpp',
    version: '1.0',
    meson_version: '>=1.3.0',
    default_options: [
        'cpp_std=c++23',
        'warning_level=2',
        'werror=false',
        'buildtype=debugoptimized',
        'bmcweb:warning_level=1',
    ],
)

# Boost dependency configuration - must match bmcweb subproject settings
add_project_arguments(
    '-DBOOST_ASIO_DISABLE_THREADS',
    '-DBOOST_ASIO_NO_DEPRECATED',
    '-DBOOST_ASIO_SEPARATE_COMPILATION',
    '-DBOOST_BEAST_SEPARATE_COMPILATION',
    '-DBOOST_EXCEPTION_DISABLE',
    '-DBOOST_NO_EXCEPTIONS',
    language: 'cpp'
)

# Override bmcweb's werror setting and suppress specific warnings
add_project_arguments(
    '-Wno-error',
    '-Wno-null-dereference',
    language: 'cpp'
)

# Also add these flags globally to affect subprojects
add_global_arguments(
    '-Wno-null-dereference',
    language: 'cpp'
)

# Get compiler
cxx = meson.get_compiler('cpp')

# Get bmcweb as a subproject
bmcweb_proj = subproject('bmcweb')

# Get the bmcweb dependency which includes all necessary dependencies
bmcweb_dep = bmcweb_proj.get_variable('bmcweb_dependencies')

# Get the include directories from bmcweb - this is an array of include_directories objects
bmcweb_incdir = bmcweb_proj.get_variable('incdir')

# Get bmcweblib from the subproject - this already contains webserver_run.cpp
bmcweblib = bmcweb_proj.get_variable('bmcweblib')
libexec = get_option('prefix') + '/' + get_option('libexecdir')
# Build the executable
# Note: We create a simple main that calls the run() function from bmcweblib
# The webserver_run.cpp in src/ is for reference/customization
# We compile our own boost_asio.cpp to avoid linker issues with typeinfo symbols
executable(
    'ibm-bmcweb',
    ['src/webserver_main.cpp', 'src/ibmwebserver_run.cpp', 'src/boost_asio.cpp'],
    include_directories: [
        bmcweb_incdir,
        include_directories(['src','redfish']),
        include_directories('subprojects/bmcweb')
    ],
    dependencies: bmcweb_dep,
    link_with: bmcweblib,
    install: true,
    install_dir: '/usr/bin'
)
configure_file(
    input: 'service/ibm-bmweb.service.in',
    output: 'ibm-bmcweb.service',
    configuration: {
        'SYSTEMD_TARGET': 'multi-user.target'
    },
    install: true,
    install_dir: dependency('systemd').get_variable('systemdsystemunitdir'),
)