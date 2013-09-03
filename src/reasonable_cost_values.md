# Cost weight calculation

What are reasonable values for these?

    energy cost weight = time / energy
    data cost weight = time / data amount

That is, I'm willing to spend a certain amount of $RESOURCE to save a certain amount of time.

Let's try out some plausible values here.  Suppose I'm willing to spend a small percentage of my battery or monthly data budget to save one average wifi failover.

    mAh = mA*h
    mJ = mA*V
    
    # params from Nexus One
    average voltage = 4.118898V
    wifi failover = 5 sec
    battery capacity = 1336mAh * 3600sec/h * average voltage
    
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
      => 0.000252/(mA*V)

    battery capacity cost weight(time=wifi failover,
                                 fraction = 0.01)
      => 2.523946e-5/(mA*V)
      
    battery capacity cost weight(time=wifi failover,
                                 fraction=0.05)
      => 5.047891e-6/(mA*V)

    battery capacity cost weight(time=wifi failover,
                                 fraction = 0.10)
      => 2.523946e-6/(mA*V)



    data supply cost weight
      = data cost weight(time, data amount=fraction * monthly data cap)

    data supply cost weight(time = wifi failover, fraction=0.001)
      => 2.5e-6sec/bytes

    data supply cost weight(time = wifi failover, fraction=0.01)
      => 2.5e-7sec/bytes

    data supply cost weight(time = wifi failover, fraction=0.05)
      => 5e-8sec/bytes

    data supply cost weight(time = wifi failover, fraction=0.10)
      => 2.5e-8sec/bytes

These factors are all pretty low, since even 0.1% of 2GB is 2MB - much larger than the amount of data we're talking about for small transfers.  On average, I probably don't want to spend more than:

    hours_per_day = 24hour
    sleep_hours = 8hour
    usage_hours_per_day = hours_per_day - sleep_hours
    bytes_per_day = monthly data cap / 30 day => 66666666.6667bytes/day
    bytes_per_hour = bytes_per_day * 1day/usage_hours_per_day => 4166666.6667bytes/hour
    bytes_per_minute = bytes_per_hour * 1hour/60min => 69444.4444bytes/min
    
Therefore, `50-100 KB` is probably a reasonable lower bound for one minute, and it would be sensible to divide that further to have a few transmissions per minute, as in my experiments - say, 20KB per transfer.  Of course, this number will change based on my actual data usage (or lack thereof) over the course of the month; same applies to energy.  Also, every transfer doesn't save the same amount of time, so this is equivalent to being willing to spend a smaller amount of data to save a smaller amount of time (e.g. 4KB to save 1 second).
      
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
      => 0.000151/(mA*V)
    battery life cost weight(time=wifi failover, lifetime reduction=1min)
      => 7.571837e-5/(mA*V)
    battery life cost weight(time=wifi failover, lifetime reduction=5min)
      => 1.514367e-5/(mA*V)
    battery life cost weight(time=wifi failover, lifetime reduction=30min)
      => 2.523946e-6/(mA*V)

Or amount of data spent:

    data amount cost weight
      = data supply cost weight(time, fraction=amount/monthly data cap)

    data amount cost weight(time=wifi failover, amount=5KB) => 0.001sec/bytes
    data amount cost weight(time=wifi failover, amount=10KB) => 0.0005sec/bytes
    data amount cost weight(time=wifi failover, amount=20KB) => 0.0003sec/bytes
    data amount cost weight(time=wifi failover, amount=50KB) => 1e-4sec/bytes
    data amount cost weight(time=wifi failover, amount=100KB) => 5e-5sec/bytes
    data amount cost weight(time=wifi failover, amount=500KB) => 1e-5sec/bytes
    data amount cost weight(time=wifi failover, amount=1MB) => 5e-6sec/bytes
    data amount cost weight(time=wifi failover, amount=2MB) => 2.5e-6sec/bytes
    data amount cost weight(time=wifi failover, amount=10MB) => 5e-7sec/bytes
    

In addition to choosing one pair of values for an additional experiment, I think it may be illustrative to plot a range of these values (e.g. fix the quotient and show a parametric plot of time and {energy,data}) to show their meaning in terms of different per-operation costs.  Based on the above, I'll choose 0.5min of battery and 20KB of data, or 0.000151 and 0.0003 for my experiment, and I'll plot a range of these values.