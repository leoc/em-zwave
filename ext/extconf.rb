# Loads mkmf which is used to make makefiles for Ruby extensions
require 'mkmf'
require 'rbconfig'
require 'fileutils'
require 'pry'

dir_config('openzwave', '/usr/include/openzwave', '/usr/lib')

have_library('stdc++')
have_library('openzwave')

$INCFLAGS << '-I/usr/include/openzwave ' \
'-I/usr/include/openzwave/value_classes ' \
'-I/usr/include/openzwave/command_classes ' \
'-I/usr/include/openzwave/platform ' \
'-I/usr/include/openzwave/platform/unix '
$CFLAGS << ' ' << $INCFLAGS

$LIBS << ' -lopenzwave'

create_makefile('emzwave')
