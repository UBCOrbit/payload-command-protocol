project('payload-file-transmission', 'c')

include = include_directories('lib')

sha_src = ['lib/sha256.c', 'sha256_utils.c']
listener_src = ['listener.c', 'commands.c', sha_src]

executable('command-listener', listener_src, include_directories : include)

if get_option('build_tests')
    executable('test-sha256', ['lib/sha256.c', 'sha256_utils.c'], include_directories : include, c_args : '-DSHA256_TEST')
endif
