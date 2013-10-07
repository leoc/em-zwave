module EventMachine
  class Zwave
    class Value

      attr_reader :home_id, :value_id

      def initialize(home_id, value_id)
        @home_id = home_id
        @value_id = value_id
      end

      def home_id
        @home_id
      end

      def node
        @node ||= Node.new(home_id, node_id)
      end

      def node_id; end
      def instance; end
      def index; end
      def type; end
      def command_class; end

      def label; end
      def units; end
      def help; end
      def min; end
      def max; end
      def read_only?; end
      def write_only?; end
      def set?; end
      def polled?; end

      def set(level); end
      def get; end

      def inspect
        ret = "Value[label=#{label},command_class=#{command_class},value=#{get},min=#{min},max=#{max}"
        ret << ",units=#{units}" unless units == ""
        ret << "]"
        ret
      end

    end
  end
end
