# Chronos for Defold

High resolution monotonic timers, based on https://github.com/ldrumm/chronos.

## Installation

Add defold-chronos in your own project as a Defold library dependency. Open your game.project file and in the dependencies field under project add:
https://github.com/redoak/defold-chronos/archive/master.zip

## Example

```lua
local function profile_fn(fn)
    if chronos then
        local before = chronos.time()
        fn()
        local after = chronos.time()
        print("elapsed seconds: " .. (after - before))
    end
end
```

## Platform support

Only Windows supports the full API below.

## API

```lua
-- returns seconds, normally with a precision of 100–300ns.
chronos.time()

-- returns the frequency of the used counter, e.g. 3,125,000.
chronos.frequency()

-- returns the number of decimal values that are reliable, e.g. a frequency
-- of 10,000,000 gives a period of 100ns with 7 reliable decimals.
chronos.decimals()

-- returns the rdtsc counter value.
chronos.tsc()

-- returns seconds passed since the given tsc value, and the core frequency
-- estimated over the given wait time in milliseconds (default 100), used to
-- calculate it.
chronos.tsc_time(tsc, [wait])
```
