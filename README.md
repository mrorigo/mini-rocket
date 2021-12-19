
# GNU Rocket C Library


## Usage

### Initialize

Either connect to rocket editor:

```
	mrocket_t *rocket = mrocket_connect("localhost", 1338);
```


.. or read from a serialized file:

```
	mrocket_t *rocket = mrocket_read_from_file("demo.rkt");
```

### Allocate tracks


```
	mrocket_t *track1 = mrocket_create_track("group1:track1");
	mrocket_t *track2 = mrocket_create_track("group1:track2");
```

### Run demo

In your main loop, call `mrocket_tick(rocket, delta_time_in_ms)` and then use `mrocket_get_value(track)` to fetch the current value for a track.

