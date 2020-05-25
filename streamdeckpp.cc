#include "streamdeckpp.hh"


namespace streamdeck {


  context::context(const char* path)
  {
    if (auto r = hid_init(); r < 0)
      throw std::runtime_error("hid_init");

    devs = hid_enumerate(vendor_elgato, 0); 
    if (devs == nullptr)
      throw std::runtime_error("hid_enumerate");

    for (auto p = devs; p != nullptr; p = p->next) {
      auto ap = get_device(p->product_id, p->path);
      if (ap)
        devinfo.emplace_back(std::move(ap));
    }

    Magick::InitializeMagick(path);
  }

  context::~context()
  {
    devinfo.clear();
    hid_free_enumeration(devs);
    hid_exit();
  }

} // namespace streamdeck
