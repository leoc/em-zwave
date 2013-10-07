# em-zwave

`em-zwave` wraps a subset of the OpenZwave functionality into an
eventmachine based Ruby gem.

## Functionality

Currently `em-zwave` starts an OpenZwave manager and pushes
notifications as Ruby objects to an `EventMachine::Queue`.

Those pushed items invoke callbacks that are create on the `EM::Zwave`
instance.

## Usage

```ruby
require 'eventmachine'
require 'em-zwave'

EM.run do
  @zwave = EM::Zwave.new
  @zwave.add_device('/dev/ttyUSB0')

  @zwave.on_notification do |notification|
    # do something generic with your notification
  end

  @zwave.on_initialization_finished do
    puts 'Initialization has been finished'

    # List all your nodes and their values.
    @zwave.nodes.each_value do |node|
      puts node.inspect
      node.values.each do |value|
        puts "  #{value.inspect}"
      end
    end

    # Switch on all your devices.
    @zwave.nodes.each_value do |node|
      puts node.on!
    end
  end

  @zwave.on(:value_changed) do |value|
    # Do something with your changed value.
  end

  # Shutdown the openzwave manager correctly.
  Signal.trap('INT')  { @zwave.shutdown }
  Signal.trap('TERM') { @zwave.shutdown }

  # When the openzwave part was shutdown then stop eventmachine.
  @zwave.on_shutdown do
    EM.stop
  end

  # Show that the reactor is still reacting.
  EM::PeriodicTimer.new(1) { puts 'R: tick' }
end
```

## Status of this project

I tried to implement a basic version, which can be used to interact
with a zwave network to actually execute some real world scenarios.

But because I decided to go with a different C++ service as agent for
Zwave communication, I do not really use this gem. Feel free to try it
out. If you like it and want help out, I would appreciate pull
requests or collaborators.

## Licensing

All the code published via this project is licensed under the GPLv3.
For a full text of the license see the ./LICENSE file.
