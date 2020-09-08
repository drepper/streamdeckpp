#include <iostream>
#include <string>
#include "streamdeckpp.hh"

using namespace std::string_literals;


int main(int argc, char* argv[])
{
  if (argc == 1)
    return 0;

  streamdeck::context ctx(argv[0]);
  if (ctx.empty())
    error(EXIT_FAILURE, 0, "failed streamdeck::context initialization");

  if ("image"s == argv[1]) {
    int key = argc <= 2 ? 0 : atoi(argv[2]);
    const char* fname = argc <= 3 ? "test.jpg" : argv[3];
    ctx[0]->set_key_image(key, fname);
  } else if ("reset"s == argv[1])
    ctx[0]->reset();
  else if ("brightness"s == argv[1]) {
    unsigned percent = argc == 2 ? 50 : atoi(argv[2]);
    ctx[0]->set_brightness(percent);
  } else if ("serial"s == argv[1])
    std::cout << ctx[0]->get_serial_number() << std::endl;
  else if ("firmware"s == argv[1])
    std::cout << ctx[0]->get_firmware_version() << std::endl;
  else if ("read"s == argv[1]) {
    while (true) {
      auto ss = ctx[0]->read();
      for (auto s : ss)
        std::cout << ' ' << s;
      std::cout << std::endl;
    }
  } else if ("timeout"s == argv[1]) {
    auto t = argc < 3 ? 1000 : atoi(argv[2]);
    while (true) {
      auto ss = ctx[0]->read(t);
      if (ss) {
        for (auto s : *ss)
          std::cout << ' ' << s;
        std::cout << std::endl;
      } else
        std::cout << "nothing\n";
    }
  }
}
