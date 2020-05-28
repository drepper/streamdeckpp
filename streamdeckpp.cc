#include "streamdeckpp.hh"

#include <Magick++.h>


namespace streamdeck {

  namespace {

    struct blob_container {
      blob_container(Magick::Blob& blob) : front(static_cast<const char*>(blob.data())), back(front + blob.length()) {}

      auto begin() const { return front; }
      auto end() const { return back; }

      const char* front;
      const char* back;
    };

  } // anonymous namespace


  device_type::device_type(const char* path, unsigned width, unsigned height, unsigned cols, unsigned rows, image_format_type imgfmt, unsigned imgreplen, bool hflip, bool vflip)
  : pixel_width(width), pixel_height(height), key_cols(cols), key_rows(rows), key_count(rows * cols),
    key_image_format(imgfmt), image_report_length(imgreplen), key_hflip(hflip), key_vflip(vflip),
    m_path(path), m_d(hid_open_path(m_path))
  {
  }


  device_type::~device_type()
  {
    close();
  }


  void device_type::close()
  {
    if (m_d != nullptr)
      hid_close(m_d);
  }


  int device_type::set_key_image(unsigned key, const char* fname)
  {
    Magick::Image image(fname);

    if (key_hflip)
      image.transpose();
    if (key_vflip)
      image.transverse();

    auto size = image.size();
    if (size.width() != pixel_width || size.height() != pixel_height) {
      auto factor = std::min(double(pixel_width) / size.width(), double(pixel_height) / size.height());
      Magick::Geometry new_geo(size_t(size.width() * factor), size_t(size.height() * factor));
      image.scale(new_geo);

      if (new_geo.width() != pixel_width || new_geo.height() != pixel_height) {
        Magick::Geometry defgeo(pixel_width, pixel_height);
        Magick::Image newimage(defgeo, Magick::Color("black"));

        newimage.composite(image, ssize_t(pixel_width - new_geo.width()) / 2, ssize_t(pixel_height - new_geo.height()) / 2);
        image.scale(defgeo);
        image = newimage;
      }
    }

    if (key_image_format == image_format_type::jpeg)
      image.magick("JPEG");
    else if (key_image_format == image_format_type::bmp)
      image.magick("BMP");

    Magick::Blob blob;
    image.write(&blob);

    return set_key_image(key, blob_container(blob));
  }


  gen1_device_type::payload_type::iterator gen1_device_type::add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page)
  {
    auto it = buffer.begin();
    *it++ = std::byte(0x02);
    *it++ = std::byte(0x01);
    *it++ = std::byte(page + 1);
    *it++ = std::byte(0x00);
    *it++ = std::byte(remaining > payload_length ? 0 : 1);
    *it++ = std::byte(key + 1);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);
    *it++ = std::byte(0x00);

    return it;
  }


  std::vector<bool> gen1_device_type::read()
  {
    std::vector<bool> res(key_count);
    std::vector<std::byte> state(1 + key_count);
    base_type::read(state);
    std::transform(state.begin() + 1, state.end(), res.begin(), [](auto v){ return v != std::byte(0); });
    return res;
  }


  void gen1_device_type::reset()
  {
    const std::array<std::byte,17> req = { std::byte(0x0b), std::byte(0x63) };
    send_report(req);
  }


  void gen1_device_type::_set_brightness(std::byte p)
  {
    const std::array<std::byte,17> req { std::byte(0x05), std::byte(0x55), std::byte(0xaa), std::byte(0xd1), std::byte(0x01), p };
    send_report(req);
  }


  std::string gen1_device_type::_get_string(std::byte cmd)
  {
    std::array<std::byte,17> buf { cmd };
    auto len = get_report(buf);
    return len > 5 ? std::string(reinterpret_cast<char*>(buf.data()) + 5) : "";
  }


  std::string gen1_device_type::get_serial_number()
  {
    return _get_string(std::byte(0x03));
  }


  std::string gen1_device_type::get_firmware_version()
  {
    return _get_string(std::byte(0x04));
  }


  gen2_device_type::payload_type::iterator gen2_device_type::add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page)
  {
    auto it = buffer.begin();
    auto this_length = std::min(payload_length, remaining);
    *it++ = std::byte(0x02);
    *it++ = std::byte(0x07);
    *it++ = std::byte(key);
    *it++ = std::byte(remaining > payload_length ? 0 : 1);
    *it++ = std::byte(this_length & 0xff);
    *it++ = std::byte(this_length >> 8);
    *it++ = std::byte(page & 0xff);
    *it++ = std::byte(page >> 8);

    return it;
  }


  std::vector<bool> gen2_device_type::read()
  {
    std::vector<bool> res(key_count);
    std::vector<std::byte> state(4 + key_count);
    base_type::read(state);
    std::transform(state.begin() + 4, state.end(), res.begin(), [](auto v){ return v != std::byte(0); });
    return res;
  }


  void gen2_device_type::reset()
  {
    const std::array<std::byte,32> req = { std::byte(0x03), std::byte(0x02) };
    send_report(req);
  }


  void gen2_device_type::_set_brightness(std::byte p)
  {
    const std::array<std::byte,32> req { std::byte(0x03), std::byte(0x08), p };
    send_report(req);
  }


  std::string gen2_device_type::_get_string(std::byte cmd, size_t off)
  {
    std::array<std::byte,32> buf { cmd };
    auto len = get_report(buf);
    return len > off ? std::string(reinterpret_cast<char*>(buf.data()) + off) : "";
  }


  std::string gen2_device_type::get_serial_number()
  {
    return _get_string(std::byte(0x06), 2);
  }


  std::string gen2_device_type::get_firmware_version()
  {
    return _get_string(std::byte(0x05), 6);
  }


  template<size_t N = 0>
  std::unique_ptr<device_type> context::get_device(unsigned short product_id, const char* path)
  {
    if constexpr (N == products.size())
      return nullptr;
    else {
      if (product_id == products[N])
        return std::make_unique<specific_device_type<products[N]>>(path);
      return get_device<N + 1>(product_id, path);
    }
  }


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
