# -*- coding: utf-8 -*-
require 'eventmachine'
require 'em-zwave/zwave'
require 'em-zwave/controller'
require 'em-zwave/value'
require 'em-zwave/node'
require 'em-zwave/notification'
require 'emzwave'

require 'fileutils'

module EventMachine
  class Zwave
    VERSION = '0.0.1'
  end
end
