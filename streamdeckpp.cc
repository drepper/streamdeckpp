#include "streamdeckpp.hh"

using namespace std::string_literals;


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
    key_image_format(imgfmt), key_hflip(hflip), key_vflip(vflip), image_report_length(imgreplen),
    m_path(path), m_d(hid_open_path(m_path))
  {
  }


  device_type::~device_type()
  {
    close();
  }


  void device_type::close()
  {
    if (connected())
      hid_close(m_d);
  }


  template<typename C>
  int device_type::set_key_image(unsigned key, const C& data)
  {
    if (key > key_count)
      return -1;

    payload_type buffer(image_report_length);
    unsigned page = 0;
    for (auto srcit = data.begin(); srcit != data.end(); ++page) {
      auto destit = add_header(buffer, key, data.end() - srcit, page);
      while (srcit != data.end() && destit != buffer.end())
        *destit++ = std::byte(*srcit++);

      std::fill(destit, buffer.end(), std::byte(0));

      if (auto r = write(buffer); r < 0)
        return r;
    }

    return 0;
  }


  int device_type::set_key_image(unsigned key, Magick::Image&& image)
  {
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


  int device_type::set_key_image(unsigned key, const char* fname)
  {
    Magick::Image file(fname);
    return set_key_image(key, std::move(file));
  }


  namespace {

    // First generation.
    struct gen1_device_type : public device_type {
      using base_type = device_type;

      const unsigned image_report_length;
      static constexpr unsigned header_length = 16;
      const unsigned payload_length;

      gen1_device_type(const char* path, unsigned width, unsigned height, unsigned cols, unsigned rows, unsigned imgreplen, bool hflip, bool vflip)
      : device_type(path, width, height, cols, rows, image_format_type::bmp, imgreplen, hflip, vflip), image_report_length(imgreplen), payload_length(imgreplen - header_length)
      {}

      payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final;

      std::vector<bool> read() override final;

      std::optional<std::vector<bool>> read(int timeout = -1) override final;

      void reset() override final;

      std::string get_serial_number() override final;

      std::string get_firmware_version() override final;

    private:
      void _set_brightness(std::byte p) override final;

      std::string _get_string(std::byte c);
    };


    // Second generation.
    struct gen2_device_type : public device_type {
      using base_type = device_type;

      static constexpr unsigned image_report_length = 1024;
      static constexpr unsigned header_length = 8;
      static constexpr unsigned payload_length = image_report_length - header_length;

      gen2_device_type(const char* path, unsigned width, unsigned height, unsigned cols, unsigned rows)
      : device_type(path, width, height, cols, rows, image_format_type::jpeg, image_report_length, true, true)
      {}

      payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final;

      std::vector<bool> read() override final;

      std::optional<std::vector<bool>> read(int timeout = -1) override final;

      void reset() override final;

      std::string get_serial_number() override final;

      std::string get_firmware_version() override final;

    private:
      void _set_brightness(std::byte p) override final;

      std::string _get_string(std::byte c, size_t off);
    };


    gen1_device_type::payload_type::iterator gen1_device_type::add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page)
    {
      auto it = buffer.begin();
      *it++ = std::byte(0x02);
      *it++ = std::byte(0x01);
      *it++ = std::byte(page + 1);
      *it++ = std::byte(0x00);
      *it++ = std::byte(remaining > payload_length ? 0 : 1);
      *it++ = std::byte(key + 1);
      std::fill_n(it, 10, std::byte(0x00));

      return it;
    }


    std::vector<bool> gen1_device_type::read()
    {
      std::vector<bool> res(key_count);
      std::vector<std::byte> state(1 + key_count);
      auto n = base_type::read(state);
      std::transform(state.begin() + 1, state.begin() + n, res.begin(), [](auto v){ return v != std::byte(0); });
      return res;
    }


    std::optional<std::vector<bool>> gen1_device_type::read(int timeout)
    {
      std::vector<bool> res(key_count);
      std::vector<std::byte> state(1 + key_count);
      auto n = base_type::read(state, timeout);
      if (n == 0)
        return std::nullopt;
      std::transform(state.begin() + 1, state.begin() + n, res.begin(), [](auto v){ return v != std::byte(0); });
      return res;
    }


    void gen1_device_type::reset()
    {
      const std::array<std::byte,17> req { std::byte(0x0b), std::byte(0x63) };
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
      return len > 5 ? std::string(reinterpret_cast<const char*>(buf.data()) + 5) : "";
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
      *it++ = std::byte(0x02);
      *it++ = std::byte(0x07);
      *it++ = std::byte(key);
      if (remaining > payload_length) {
        *it++ = std::byte(0x00);
        *it++ = std::byte(payload_length & 0xff);
        *it++ = std::byte(payload_length >> 8);
      } else {
        *it++ = std::byte(0x01);
        *it++ = std::byte(remaining & 0xff);
        *it++ = std::byte(remaining >> 8);
      }
      *it++ = std::byte(page & 0xff);
      *it++ = std::byte(page >> 8);

      return it;
    }


    std::vector<bool> gen2_device_type::read()
    {
      std::vector<bool> res(key_count);
      std::vector<std::byte> state(4 + key_count);
      auto n = base_type::read(state);
      std::transform(state.begin() + 4, state.begin() + n, res.begin(), [](auto v){ return v != std::byte(0); });
      return res;
    }


    std::optional<std::vector<bool>> gen2_device_type::read(int timeout)
    {
      std::vector<bool> res(key_count);
      std::vector<std::byte> state(4 + key_count);
      auto n = base_type::read(state, timeout);
      if (n == 0)
        return std::nullopt;
      std::transform(state.begin() + 4, state.begin() + n, res.begin(), [](auto v){ return v != std::byte(0); });
      return res;
    }


    void gen2_device_type::reset()
    {
      const std::array<std::byte,32> req { std::byte(0x03), std::byte(0x02) };
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
      return len >= 0 && size_t(len) > off ? std::string(reinterpret_cast<const char*>(buf.data()) + off) : "";
    }


    std::string gen2_device_type::get_serial_number()
    {
      return _get_string(std::byte(0x06), 2);
    }


    std::string gen2_device_type::get_firmware_version()
    {
      return _get_string(std::byte(0x05), 6);
    }


    template<unsigned short D>
    struct specific_device_type;


    // StreamDeck Original
    template<>
    struct specific_device_type<product_streamdeck_original> final : public gen1_device_type {
      using base_type = gen1_device_type;

      static constexpr unsigned image_report_length = 8191;

      specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3, image_report_length, true, true) {}
    };


    // StreamDeck Original V2
    template<>
    struct specific_device_type<product_streamdeck_original_v2> final : public gen2_device_type {
      using base_type = gen2_device_type;

      specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3) {}
    };


    // StreamDeck Mini
    template<>
    struct specific_device_type<product_streamdeck_mini> final : public gen1_device_type {
      using base_type = gen1_device_type;

      static constexpr unsigned image_report_length = 1024;

      specific_device_type(const char* path) : base_type(path, 80, 80, 3, 2, image_report_length, false, true) {}
    };


    // StreamDeck XL
    template<>
    struct specific_device_type<product_streamdeck_xl> final : public gen2_device_type {
      using base_type = gen2_device_type;

      specific_device_type(const char* path) : base_type(path, 96, 96, 8, 4) {}
    };


    constexpr auto products = std::experimental::make_array(product_streamdeck_original,
                                                            product_streamdeck_original_v2,
                                                            product_streamdeck_mini,
                                                            product_streamdeck_xl);

    template<size_t N = 0>
    std::unique_ptr<device_type> get_device(unsigned short product_id, const char* path)
    {
      if constexpr (N == products.size())
        return nullptr;
      else {
        if (product_id == products[N])
          return std::make_unique<specific_device_type<products[N]>>(path);
        return get_device<N + 1>(product_id, path);
      }
    }

  } // anonymous namespace


  context::context()
  {
    if (auto r = hid_init(); r < 0)
      throw std::runtime_error("hid_init failed with "s + std::to_string(r));

    devs = hid_enumerate(vendor_elgato, 0); 
    for (auto p = devs; p != nullptr; p = p->next)
      if (auto ap = get_device(p->product_id, p->path); ap)
        devinfo.emplace_back(std::move(ap));

    Magick::InitializeMagick(nullptr);
  }


  context::~context()
  {
    devinfo.clear();
    if (devs)
      hid_free_enumeration(devs);
    hid_exit();
  }

} // namespace streamdeck
