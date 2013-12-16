# Cost weight calculation

What are reasonable values for these?

    energy cost weight = time / energy
    data cost weight = time / data amount

That is, I'm willing to spend a certain amount of $RESOURCE to save a certain amount of time.

Let's try out some plausible values here.  Suppose I'm willing to spend a small percentage of my battery or monthly data budget to save one average wifi failover.

    mAh = mA*h
    mJ = mA*sec*V
    
    # params from Nexus One
    average voltage = 4.118898V
    wifi failover = 5 sec
    battery capacity = 1336mAh * 3600sec/h * average voltage
        => 19,810,251.8208 mA*sec*V
        
    10 mA*sec*V / (battery capacity / 1000) * 100 => 0.0504789
    
    battery lifetime(power) = battery capacity / power
    battery lifetime(power=44mJ/sec)/(3600 sec/h) => 125.0647211 h
    125 h * 0.0005 * 60 min/h=> 3.75 min
    
    battery lifetime(power=2800mJ/sec) / (3600 sec/h)=> 1.9653028 h
    1.965 h * 0.0005 * 3600 sec/h => 3.537 sec
    
    battery lifetime(power=2395mJ/sec) / (3600 sec/h) => 2.29764 h
    2.29764 h * 0.0005 * 3600 sec/h => 4.135752 sec
    
    KB = 1000 bytes
    MB = 1000 KB
    GB = 1000 MB

    monthly data cap = 2GB
    
To keep this simple, I'll fix the numerator (time) in the cost weight fractions and vary the denominators (cost).
    
    battery capacity cost weight
      = energy cost weight(time, 
                           energy=battery capacity 
                                  * fraction)
    
    battery capacity cost weight(time=wifi failover,
                                 fraction = 0.001)
      => 0.0002524/(mA*V)

    battery capacity cost weight(time=wifi failover,
                                 fraction = 0.01)
      => 0.0000252/(mA*V)
      
    battery capacity cost weight(time=wifi failover,
                                 fraction=0.05)
      => 0.000005/(mA*V)

    battery capacity cost weight(time=wifi failover,
                                 fraction = 0.10)
      => 0.0000025/(mA*V)


energy cost weight(time=1, energy=10000) => 0.0001
cellular_energy = 5 hour * 60min/hour * 60 sec/min * 902 mJ/sec
    => 16,236,000 mA*sec*V
    
10000mJ/cellular_energy * 5 hour * 60 min/hour * 60 sec/min => 11.0864745 sec

10000/(2*10**9) * 100 => 0.0005

2 * GB / (30 day * 5 hour/day * 60 min/hour) => 222,222.2222222 bytes/min

    data supply cost weight
      = data cost weight(time, data amount=fraction * monthly data cap)

    data supply cost weight(time = wifi failover, fraction=0.001)
      => 0.0000025 sec/bytes

    data supply cost weight(time = wifi failover, fraction=0.01)
      => 0.0000003 sec/bytes

    data supply cost weight(time = wifi failover, fraction=0.05)
      => 5e-8 sec/bytes

    data supply cost weight(time = wifi failover, fraction=0.10)
      => 2.5e-8 sec/bytes

These factors are all pretty low, since even 0.1% of 2GB is 2MB - much larger than the amount of data we're talking about for small transfers.  On average, I probably don't want to spend more than:

    hours_per_day = 24hour
    sleep_hours = 8hour
    usage_hours_per_day = hours_per_day - sleep_hours
    bytes_per_day = monthly data cap / 30 day => 66,666,666.6666667 bytes/day
    bytes_per_hour = bytes_per_day * 1day/usage_hours_per_day => 4,166,666.6666667 bytes/hour
    bytes_per_minute = bytes_per_hour * 1hour/60min => 69,444.4444444 bytes/min
    
Therefore, `50-100 KB` is probably a reasonable lower bound for one minute, and it would be sensible to divide that further to have a few transmissions per minute, as in my experiments - say, 20KB per transfer.  Of course, this number will change based on my actual data usage (or lack thereof) over the course of the month; same applies to energy.  Also, every transfer doesn't save the same amount of time, so this is equivalent to being willing to spend a smaller amount of data to save a smaller amount of time (e.g. 4KB to save 1 second).

10 KB / 2 GB  * 100 => 0.0005
usage_hours_per_day/hour * 30 day * (10 KB / 2 GB) * 24 hour/day * 60 min/hour=> 3.456 min
      
How about in terms of battery lifetime?  (A little more tangible than "fraction of battery capacity")

    # Google's estimate for 3G internet use on the Nexus One
    battery lifetime = 5 hour * 60 min/hour
    battery life cost weight 
       = battery capacity cost weight(time, fraction=(lifetime reduction 
                                                      / battery lifetime))
                                                      
    # These are *very* rough and probably underestimate the 
    #  energy importance factor, since battery life is much longer
    #  if you use the cellular radio less

    battery life cost weight(time=wifi failover, lifetime reduction=0.5min)
      => 0.000151mA*V)
    battery life cost weight(time=wifi failover, lifetime reduction=1min)
      => 0.0000757/(mA*V)
    battery life cost weight(time=wifi failover, lifetime reduction=5min)
      => 0.0000151/(mA*V)
    battery life cost weight(time=wifi failover, lifetime reduction=30min)
      => 0.0000025/(mA*V)

Or amount of data spent:

    data amount cost weight
      = data supply cost weight(time, fraction=amount/monthly data cap)

    data amount cost weight(time=wifi failover, amount=5KB) => 0.001 sec/bytes
    data amount cost weight(time=wifi failover, amount=10KB) => 0.0005 sec/bytes
    data amount cost weight(time=wifi failover, amount=20KB) => 0.00025 sec/bytes
    data amount cost weight(time=wifi failover, amount=50KB) => 0.0001 sec/bytes
    data amount cost weight(time=wifi failover, amount=100KB) => 0.00005 sec/bytes
    data amount cost weight(time=wifi failover, amount=500KB) => 0.00001 sec/bytes
    data amount cost weight(time=wifi failover, amount=1MB) => 0.000005 sec/bytes
    data amount cost weight(time=wifi failover, amount=2MB) => 0.0000025 sec/bytes
    data amount cost weight(time=wifi failover, amount=10MB) => 0.0000005 sec/bytes
    

In addition to choosing one pair of values for an additional experiment, I think it may be illustrative to plot a range of these values (e.g. fix the quotient and show a parametric plot of time and {energy,data}) to show their meaning in terms of different per-operation costs.  Based on the above, I'll choose 0.5min of battery and 20KB of data, or 0.000151 and 0.0003 for my experiment, and I'll plot a range of these values.