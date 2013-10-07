module EventMachine
  class Zwave
    class Node

      attr_accessor :home_id, :node_id

      def initialize(home_id, node_id)
        @home_id = home_id
        @node_id = node_id
      end

      def values
        @values ||= []
      end

      # Stubs for documentations purpose
      #  - The following methods are defined in the C code of this library.
      def listening_device?; end
      def frequent_listening_device?; end
      def beaming_device?; end
      def routing_device?; end
      def security_device?; end

      def max_baud_rate; end
      def version; end
      def security; end

      def basic_type; end
      def generic_type; end
      def specific_type; end
      def type; end

      def manufacturer_name; end
      def manufacturer_id; end
      def product_name; end
      def product_type; end
      def product_id; end
      def name; end
      def location; end

      def on!; end
      def off!; end
      def level=; end

      def inspect
        "Node[id=#{node_id},home_id=#{home_id},product_name=#{product_name},manufacturer=#{manufacturer_name}]"
      end

    end
  end
end
