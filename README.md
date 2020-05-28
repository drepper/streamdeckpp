C++ Library to infaceface with StreamDeck devices
=================================================

This library is written to mirror the
[Python library](https://github.com/abcminiuser/python-elgato-streamdeck.git) with
similar goal.  To integrate into non-Python projects a C++ interface library is
better, though.

The interface is minimal so far.  It can be extended.  There is no program code
yet which takes advantage of the library, just an example program that is used
to test the code in the package.


Interface
---------

Using the library is simple.  The `streamdeck::context` object can be instantiated once and
only takes a path argument.  This argument is only used in the ImageMagick routes which
are used behind the scenes.  To handle USB devices `libhidapi-libusb` is used which will
create several threads and in general might prevent others users from using the opened
devices concurrently.  This is expected.  It means, though, the library has an internal
state and it is not possible, in general, to have multiple objects of type `streamdeck::context`
in use at the same time.

The constructor can fail if the USB library fails to initialize.  If no devices are found the constructor
will succeed.  The found devices, if any, can be iterated over with the usual C++ interfaces: `begin`, `end`,
`empty`.  Only devices which can be handled by the library are reported.  This might mean that new
devices might not be handled (yet).

To distinguish the devices their serial numbers can be used:

    streamdeck::context ctx(argv[0]);
    for (auto& dev : ctx)
	  std::cout << dev->get_serial_number() << std::endl;

If more than one device is reported but only one is needed the others should be closed with a call
to the `close` member function.

The `set_key_image` member function allows to set an image for a key.  One variant take a file name in
which case the ImageMaick libraries are used to transform the image as required by the device.  The image will be scale and transformed, if necessary, and converted to the right format.  Theoretically it is even possible
to use SVG files but the handling of this type of files at least for now does not work correctly.  Better
use a bitmap format, lossless or lossy.

The second variant of the interface take an container with the image data stored in it.  In this case the
library only takes care of the transport of the data to the device.  The caller is responsible to provide
the data in the correct format.

To read the state of the device the `read` member function should be used.  There is no descriptor-based
interface which can be used with `epoll` etc.  This could be constructed with a helper thread and a pipe.  The provided `read` interface returns a vector with the current state of the button *after* a change.  I.e., the
`read` interface is delayed until a button a pressed or released.  The `read` variant with a `timeout`\
parameter returns an `optional` object which, in case the timeout is reached, contains nothing.


Using the library
-----------------

As for other package following the accepted guidelines, use `pkg-config` to
determine the command line parameters to pass to the compiler:

    $ g++ -c $(pkg-config --cflags streamdeckpp) -o sduser.o sduser.cc

For linking use

    $ g++ -o sduser sduser.o $(pkg-config --libs streamdeckpp)

Note: when linking statically one has to pass `--static` to `pkg-config`.


