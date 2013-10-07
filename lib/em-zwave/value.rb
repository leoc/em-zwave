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
      def genre; end
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

      def set(value); end
      def get; end

      def inspect
        ret = "Value[label=#{label}"
        ret << ",genre=#{genre.inspect}"
        ret << ",command_class=#{command_class.inspect}"
        ret << ",type=#{type.inspect}"
        ret << ",value=#{get.inspect}"
        ret << ",min=#{min.inspect},max=#{max.inspect}" unless min == max
        ret << ",units=#{units}" unless units == ""
        ret << "]"
        ret
      end

    end
  end
end
