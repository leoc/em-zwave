require 'eventmachine'
require 'em-zwave/notification'
require 'emzwave'

require 'fileutils'

module EventMachine
  class Zwave
    def initialize(device)
      @device = device
      @queue = EM::Queue.new
      initialize_zwave
      @queue.pop(&method(:notification_received))
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
