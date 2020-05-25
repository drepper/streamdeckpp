#include <string>
#include "streamdeckpp.hh"

using namespace std::string_literals;


int main(int argc, char* argv[])
{
  if (argc == 1)
    return 0;

  streamdeck::context ctx(argv[0]);
  if (! ctx.any())
    error(EXIT_FAILURE, 0, "failed streamdeck::context initialization");

  if ("image"s == argv[1]) {
    int key = argc == 1 ? 0 : atoi(argv[2]);
    const char* fname = argc < 3 ? "test.jpg" : argv[3];
    ctx[0]->set_key_image(key, fname);
  } else if ("reset"s == argv[1])
    ctx[0]->reset();
  else if ("brightness"s == argv[1]) {
    unsigned percent = argc == 2 ? 50 : atoi(argv[2]);
    ctx[0]->set_brightness(percent);
  }
}
