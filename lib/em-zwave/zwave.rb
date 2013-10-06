module EventMachine
  class Zwave
    attr_reader :devices, :config_path

    def initialize(options = {})
      options[:devices] = [options.delete(:device)] if options.key?(:device)
      options = {
        devices: [],
        config_path: '.'
      }.merge(options)

      @devices = options[:devices].uniq
      @config_path = File.expand_path(options[:config_path])
      @stop_emzwave = false
      @scheduled_notification_count = 0

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
      @scheduled_notification_count += 1
      @queue.push(notification)
    end

    def notification_received(notification)
      callbacks.each do |cb|
        cb.call(notification)
      end

      type_callbacks(notification.type).each do |callback|
        case notification.type
        when :value_changed, :value_added, :value_removed
          callback.call(notification.value, notification)
        when :node_added, :node_removed
          callback.call(notification.node, notification)
        else
          callback.call(notification)
        end
      end

      @scheduled_notification_count -= 1
      if @stop_emzwave && @scheduled_notification_count <= 0
        invoke_shutdown_callbacks
      else
        @queue.pop(&method(:notification_received))
      end
    end

    def invoke_shutdown_callbacks
      shutdown_callbacks.each(&:call)
    end

    def invoke_initialization_callbacks
      initialization_callbacks.each(&:call)
    end

    def schedule_shutdown
      EM.schedule do
        @stop_emzwave = true
        invoke_shutdown_callbacks if @scheduled_notification_count <= 0
      end
    end

    def on(type, &block)
      type_callbacks(type) << block
    end

    def on_notification(&block)
      callbacks << block
    end

    def on_initialization_finished(&block)
      initialization_callbacks << block
    end

    def on_shutdown(&block)
      shutdown_callbacks << block
    end

    def initialization_callbacks
      @initialization_callbacks ||= []
    end

    def shutdown_callbacks
      @shutdown_callbacks ||= []
    end

    def type_callbacks(type)
      @type_callbacks ||= {}
      @type_callbacks[type] ||= []
    end

    def callbacks
      @callbacks ||= []
    end

    def all_on!; end
    def all_off!; end

  end
end
