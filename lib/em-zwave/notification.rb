module EventMachine
  class Zwave
    class Notification
      attr_reader :type, :home_id, :node_id, :value_id

      attr_reader :event # only when type == :node_event
      attr_reader :group_index # only when type == :group
      attr_reader :button_id # only when type == :create_button
      attr_reader :scene_id # only when type == :scene_event
      attr_reader :notification # only when type == :notification

      def initialize(options = {})
        @type         = options[:type]
        @home_id      = options[:home_id]
        @node_id      = options[:node_id]
        @value_id     = options[:value_id]
        @event        = options[:event]        if options[:type] == :node_event
        @group_index  = options[:group_index]  if options[:type] == :group
        @button_id    = options[:button_id]    if options[:type] == :create_button
        @scene_id     = options[:scene_id]     if options[:type] == :scene_event
        @notification = options[:notification] if options[:type] == :notification
      end

      def value
        Value.new(@home_id, @value_id)
      end

      def node
        Node.new(@home_id, @node_id)
      end

      def to_s
        str = 'Notification['
        str << "type=#{type.inspect}"
        str << ", notification=#{notification.inspect}" if notification
        str << ", node_id=#{node_id.inspect}" if node_id
        str << ", home_id=#{home_id.inspect}" if home_id
        str << ", value_id=#{value_id.inspect}" if value_id
        str << ", event=#{event.inspect}" if event
        str << ", group_index=#{group_index.inspect}" if group_index
        str << ", button_id=#{button_id.inspect}" if button_id
        str << ", scene_id=#{scene_id.inspect}" if scene_id
        str << ']'
        str
      end
    end
  end
end
