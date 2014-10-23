# Instruments

Instruments is a library that helps your application incorporate
prediction uncertainty in decision-making.

### Why care about uncertainty?

Allow me to describe a situation that happens to me almost every day.
Our scene begins in a building of some kind, be it home, office,
restaurant, grocery store, etc. I am ~~wasting time~~ researching
important articles on my smartphone, and I have an awesome WiFi
connection. Seriously, it is *blazing* by my local standards.

Trouble begins, however, when I get up to leave. I'm still in the
middle of... whatever I was doing, so I keep my phone in hand as
I walk out the door. Soon, I see nothing in my browser but a white
rectangle, and the little spinny thing at the top of the screen
is taunting me. I *know* my phone should switch to LTE, but it
refuses, hanging onto the fading WiFi connection with white
knuckles. Not so smart now, are you, phone?

A lot of the time, the phone is right. It's making predictions
based on the expectation that the WiFi connection is better than
the LTE connection. More generally, you could imagine making that
decision about arbitrary networks, based on measurements of
bandwidth, latency, and the amount of data that I'm transferring.
In fact, I've already written another library - [Intentional
Networking][intnw] - that does exactly
that. The uncertainty in those measurements, along with the
possibility that either network could fail, comes into play when
deciding which of many networks to use.

When I am supremely confident about my predictions - *e.g.* that
WiFi is better - I can just choose the best option. When I am
uncertain, though, *I might want to try both* - that is, send the
same data on multiple networks and use whichever data arrives
first. Instruments helps applications balance the potential time
savings of such a strategy with the additional battery energy and
cellular data required to make it happen.

A far, far, *far* more detailed description will be published and
presented at [MobiCASE 2014][mobicase2014], under the title:
"The Future is Cloudy: Reflecting Prediction Error in Mobile
Applications."

### How to use

Prerequisites:
- [Premake4](http://industriousone.com/premake/download)
- [Mocktime](http://github.com/brettdh/mocktime)
- [LibPowertutor](http://github.com/brettdh/libpowertutor)

For tests:
- [CppUnit](http://cppunit.sourceforge.net/doc/cvs/)

Build:

    $ git clone https://github.com/brettdh/instruments
    $ premake4 gmake
    $ make  # config=release for optimized build
    $ sudo make install

Use:

For the moment, I must leave you to look at the API comments in
`include/instruments.h`. The tests under `tests/acceptance` may
also provide some usable examples, and [Intentional Networking][intnw]
is a good example of a full-fledged application (well, middleware)
that uses Instruments.

More thorough documentation is forthcoming.

### What's with the name?

Yeah, it's pretty vague, collides with a Mac development
tool, and generally sounds a lot like a working title. It is.
In [my dissertation][dissertation],
this library is called "Meatballs," after the meterological
children's fable, *Cloudy With a Chance of Meatballs.* You see
what I did there. I'm still not sure how I feel about that title,
but I like it better than leaving it anonymous. All over the build,
though, it's called `instruments`; therefore it shall remain so here.

[dissertation]: http://deepblue.lib.umich.edu/handle/2027.42/108956
[mobicase2014]: http://mobicase.org/2014/
[intnw]: http://github.com/brettdh/libcmm
