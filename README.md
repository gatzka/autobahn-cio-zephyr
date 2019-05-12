# Meta project for building autobahn websocket test using cio on zephyr OS.

## Howto Build
```
west init -m git@github.com:gatzka/autobahn-cio-zephyr.git autobahn-zephyr
cd autobahn-zephyr
west update
west build -b native_posix autobahn-cio/
```


