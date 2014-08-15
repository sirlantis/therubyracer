require 'mkmf'

if RbConfig::MAKEFILE_CONFIG['CPP'] =~ /\$\(CC\)/
  gcc = RbConfig::MAKEFILE_CONFIG['CC'] =~ /g(\+\+|cc)/
elsif RbConfig::MAKEFILE_CONFIG['CPP'] =~ /g(\+\+|cc)/
  gcc = true
end

have_library('pthread')
have_library('objc') if RUBY_PLATFORM =~ /darwin/

$CPPFLAGS += " -Wall" unless $CPPFLAGS.split.include?("-Wall") || !gcc
$CPPFLAGS += " -g" unless $CPPFLAGS.split.include? "-g"

unless $CPPFLAGS.split.include?("-rdynamic") || !gcc
  $CPPFLAGS += " -rdynamic"
  $CPPFLAGS += " -fPIC" unless RUBY_PLATFORM =~ /darwin/
end

CONFIG['LDSHARED'] = '$(CXX) -shared' unless RUBY_PLATFORM =~ /darwin/
if CONFIG['warnflags']
  CONFIG['warnflags'].gsub!('-Wdeclaration-after-statement', '')
  CONFIG['warnflags'].gsub!('-Wimplicit-function-declaration', '')
end
if enable_config('debug')
  $CFLAGS += " -O0 -ggdb3"
end

LIBV8_COMPATIBILITY = '~> 3.16.14'

begin
  require 'rubygems'
  gem 'libv8', LIBV8_COMPATIBILITY
rescue Gem::LoadError
  warn "Warning! Unable to load libv8 #{LIBV8_COMPATIBILITY}."
rescue LoadError
  warn "Warning! Could not load rubygems. Please make sure you have libv8 #{LIBV8_COMPATIBILITY} installed."
ensure
  require 'libv8'
end

Libv8.configure_makefile

create_makefile('v8/init')
