#include <cinttypes>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ranges>
#include <vector>
#include <experimental/array>

#include <error.h>
#include <hidapi.h>

#include <Magick++.h>


namespace streamdeck {

  static constexpr uint16_t vendor_elgato = 0x0fd9;

  static constexpr uint16_t product_streamdeck_original = 0x0060;
  static constexpr uint16_t product_streamdeck_original_v2 = 0x006d;
  static constexpr uint16_t product_streamdeck_mini = 0x0063;
  static constexpr uint16_t product_streamdeck_xl = 0x006c;


  namespace {

    struct blob_container {
      blob_container(Magick::Blob& blob) : front(static_cast<const char*>(blob.data())), back(front + blob.length()) {}

      auto begin() const { return front; }
      auto end() const { return back; }

      const char* front;
      const char* back;
    };

  } // anonymous namespace


  struct device_type {
    enum struct image_format_type { bmp, jpeg };

    device_type(const char* path, unsigned width, unsigned height, unsigned cols, unsigned rows, image_format_type imgfmt, unsigned imgreplen, bool hflip, bool vflip)
    : pixel_width(width), pixel_height(height), key_cols(cols), key_rows(rows), key_count(rows * height),
      key_image_format(imgfmt), image_report_length(imgreplen), key_hflip(hflip), key_vflip(vflip),
      m_path(path), m_d(hid_open_path(m_path))
    {
    }

    ~device_type()
    {
      if (m_d != nullptr)
        hid_close(m_d);
    }


    bool connected() const { return m_d != nullptr; }
    auto path() const { return m_path; }


    const unsigned pixel_width;
    const unsigned pixel_height;

    const unsigned key_cols;
    const unsigned key_rows;
    const unsigned key_count;

    const image_format_type key_image_format;
    const bool key_hflip;
    const bool key_vflip;
    const unsigned image_report_length;


    using payload_type = std::vector<std::byte>;

    template<typename C>
    int set_key_image(unsigned key, const C& data)
    {
      if (key > key_count)
        return -1;

      payload_type buffer(image_report_length);
      auto srcit = data.begin();
      unsigned page = 0;
      while (srcit != data.end()) {
        auto destit = add_header(buffer, key, data.end() - srcit, page);
        while (srcit != data.end() && destit != buffer.end())
          *destit++ = std::byte(*srcit++);

        while (destit != buffer.end())
          *destit++ = std::byte(0);

        if (int r = write(buffer); r < 0)
          return r;

        ++page;
      }

      return 0;
    }

    int set_key_image(unsigned key, const char* fname)
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

    virtual payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) = 0;


    virtual void reset() = 0;

    virtual void _set_brightness(std::byte p) = 0;

    template<typename T>
    void set_brightness(T percent)
    {
      static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
      std::byte p;
      if constexpr (std::is_integral_v<T>)
        p = std::byte(std::clamp(percent, static_cast<T>(0), static_cast<T>(100)));
      else
        p = std::byte(100.0 * std::clamp(percent, 0.0, 1.0));

      _set_brightness(p);
    }


    auto send_report(const unsigned char* data, size_t len) { return hid_send_feature_report(m_d, data, len); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto send_report(const C& data) { return hid_send_feature_report(m_d, (const unsigned char*) data.data(), data.size()); }

    auto get_report(unsigned char* data, size_t len) { return hid_get_feature_report(m_d, data, len); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto get_report(C& data) { return hid_get_feature_report(m_d, data.data(), data.size()); }

    auto write(const unsigned char* data, size_t len) { return hid_write(m_d, data, len); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto write(const C& data) { return hid_write(m_d, (const unsigned char*) data.data(), data.size()); }

    auto read(unsigned char* data, size_t len) { return hid_read(m_d, data, len); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto read(C& data) { return hid_read(m_d, data.data(), data.size()); }
    auto read(unsigned char* data, size_t len, int timeout) { return hid_read_timeout(m_d, data, len, timeout); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto read(C& data, int timeout) { return hid_read_timeout(m_d, (unsigned char*) data.data(), data.size(), timeout); }

  private:
    const char* m_path;
    hid_device* m_d;
  };


  template<unsigned short D>
  struct specific_device_type;

  // StreamDeck Original
  template<>
  struct specific_device_type<product_streamdeck_original> : public device_type {
    using base_type = device_type;

    static constexpr unsigned image_report_length = 8191;
    static constexpr unsigned header_length = 8;
    static constexpr unsigned payload_length = image_report_length - header_length;

    specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3, image_format_type::bmp, image_report_length, true, true) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final
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


    void reset() override final
    {
      const std::array<std::byte,17> req = { std::byte(0x0b), std::byte(0x63) };
      send_report(req);
    }

    void _set_brightness(std::byte p) override final
    {
      const std::array<std::byte,17> req { std::byte(0x05), std::byte(0x55), std::byte(0xaa), std::byte(0xd1), std::byte(0x01), p };
      send_report(req);
    }

  };


  // StreamDeck Original V2
  template<>
  struct specific_device_type<product_streamdeck_original_v2> : public device_type {
    using base_type = device_type;

    static constexpr unsigned image_report_length = 1024;
    static constexpr unsigned header_length = 8;
    static constexpr unsigned payload_length = image_report_length - header_length;

    specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3, image_format_type::jpeg, image_report_length, true, true) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final
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


    void reset() override final
    {
      const std::array<std::byte,32> req = { std::byte(0x03), std::byte(0x02) };
      send_report(req);
    }

    void _set_brightness(std::byte p) override final
    {
      const std::array<std::byte,32> req { std::byte(0x03), std::byte(0x08), p };
      send_report(req);
    }

  };

  // StreamDeck Mini
  template<>
  struct specific_device_type<product_streamdeck_mini> : public device_type {
    using base_type = device_type;

    static constexpr unsigned image_report_length = 1024;
    static constexpr unsigned header_length = 8;
    static constexpr unsigned payload_length = image_report_length - header_length;

    specific_device_type(const char* path) : base_type(path, 80, 80, 3, 2, image_format_type::bmp, image_report_length, false, true) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final
    {
      auto it = buffer.begin();
      *it++ = std::byte(0x02);
      *it++ = std::byte(0x01);
      *it++ = std::byte(page);
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


    void reset() override final
    {
      const std::array<std::byte,17> req = { std::byte(0x0b), std::byte(0x63) };
      send_report(req);
    }

    void _set_brightness(std::byte p) override final
    {
      const std::array<std::byte,17> req { std::byte(0x05), std::byte(0x55), std::byte(0xaa), std::byte(0xd1), std::byte(0x01), p };
      send_report(req);
    }

  };

  // StreamDeck XL
  template<>
  struct specific_device_type<product_streamdeck_xl> : public device_type {
    using base_type = device_type;

    static constexpr unsigned image_report_length = 1024;
    static constexpr unsigned header_length = 8;
    static constexpr unsigned payload_length = image_report_length - header_length;

    specific_device_type(const char* path) : base_type(path, 96, 96, 8, 4, image_format_type::jpeg, image_report_length, true, true) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final
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


    void reset() override final
    {
      const std::array<std::byte,32> req { std::byte(0x03), std::byte(0x02) };
      send_report(req);
    }

    void _set_brightness(std::byte p) override final
    {
      const std::array<std::byte,32> req { std::byte(0x03), std::byte(0x08), p };
      send_report(req);
    }

  };


  struct context {
    context(const char* path);
    ~context();

    bool any() const { return ! devinfo.empty(); }
    auto size() const { return devinfo.size(); }
    auto begin() { return devinfo.begin(); }
    auto end() { return devinfo.end(); }
    auto& operator[](size_t n) { return devinfo[n]; }

  private:
    static constexpr auto products = std::experimental::make_array(product_streamdeck_original,
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

    hid_device_info* devs = nullptr;
    std::vector<std::unique_ptr<device_type>> devinfo;
  };

} // namespace streamdeck
