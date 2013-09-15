require 'eventmachine'
require 'em-zwave/notification'
require 'emzwave'

require 'fileutils'

module EventMachine
  class Zwave
    def initialize(options = {})
      options[:devices] = [options.delete(:device)] if options.key?(:device)
      options = {
        devices: [],
        config_path: '.'
      }.merge(options)

      @devices = options[:devices].uniq
      @config_path = File.expand_path(options[:config_path])

      @queue = EM::Queue.new
      @queue.pop(&method(:notification_received))

      # the initialization should be run in the next tick, so that the
      # method #add_device may be called within the same block in
      # which the EM::Zwave class was instantiated.
      EM.next_tick do
        initialize_zwave
      end
    end

    def add_device(device)
      @devices << device unless @devices.include?(device)
    end

    def push_notification(notification)
      EM.next_tick do
        @queue.push(notification)
      end
    end

    def notification_received(notification)
      callbacks.each do |callback|
        callback.call(notification)
      end
      @queue.pop(&method(:notification_received))
    end

    def on_notification(&block)
      callbacks << block
    end

    def callbacks
      @callbacks ||= []
    end
  end
end
