# em-zwave

em-zwave was my attempt to implement Ruby Eventmachine bindings for
OpenZwave. Currently it is on halt. I learned a lot about how to
interface the ruby interpreter from various c threads.

Currently this lib starts the OpenZwave Manager and registers a
notification handler which produces notifications that are handed over
to the ruby interpreter in a separate consumer thread.
