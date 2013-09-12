$LOAD_PATH.unshift File.join(File.dirname(__FILE__), 'lib')

require 'eventmachine'
require 'em-zwave'

EM.run do
  @zwave = EM::Zwave.new '/dev/ttyUSB0'
  @zwave.on_notification do |notification|
    puts "Notification in user code: #{notification}"
  end
  EM::PeriodicTimer.new(1) { puts 'R: .' }
end
