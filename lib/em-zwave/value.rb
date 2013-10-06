module EventMachine
  class Zwave
    class Value

      def initialize(home_id, value_id)
        @home_id = home_id
        @value_id = value_id
      end

      def node_id; end
      def home_id
        @home_id
      end

      def node
        Node.new(home_id, node_id)
      end

      def label; end
      def units; end
      def help; end
      def min; end
      def max; end
      def read_only?; end
      def write_only?; end
      def set?; end
      def polled?; end

      def list_selection; end
      def list_items; end
      def float_precision; end

      def set(level); end
      def get; end

      def refresh!; end
      def change_verified?; end
      def change_verified=; end
      def press_button; end
      def release_button; end

      def to_i; end
      def to_bool; end
      def to_sym; end
      def to_s

      end

      def inspect
        "Value[label=#{label},value=#{get}]"
      end

    end
  end
end
