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

    device_type(const char* path, unsigned width, unsigned height, unsigned cols, unsigned rows, image_format_type imgfmt, unsigned imgreplen, bool hflip, bool vflip);

    ~device_type();


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

    int set_key_image(unsigned key, const char* fname);

    virtual payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) = 0;


    virtual void reset() = 0;

  private:
    virtual void _set_brightness(std::byte p) = 0;

  public:
    template<typename T>
    void set_brightness(T percent)
    {
      std::byte p;
      if constexpr (std::is_integral_v<T>)
        p = std::byte(std::clamp(percent, static_cast<T>(0), static_cast<T>(100)));
      else {
        static_assert(std::is_floating_point_v<T>);
        p = std::byte(100.0 * std::clamp(percent, static_cast<T>(0.0), static_cast<T>(1.0)));
      }

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

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final;


    void reset() override final;

  private:
    void _set_brightness(std::byte p) override final;
  };


  // StreamDeck Original V2
  template<>
  struct specific_device_type<product_streamdeck_original_v2> : public device_type {
    using base_type = device_type;

    static constexpr unsigned image_report_length = 1024;
    static constexpr unsigned header_length = 8;
    static constexpr unsigned payload_length = image_report_length - header_length;

    specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3, image_format_type::jpeg, image_report_length, true, true) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final;


    void reset() override final;

  private:
    void _set_brightness(std::byte p) override final;
  };

  // StreamDeck Mini
  template<>
  struct specific_device_type<product_streamdeck_mini> : public device_type {
    using base_type = device_type;

    static constexpr unsigned image_report_length = 1024;
    static constexpr unsigned header_length = 8;
    static constexpr unsigned payload_length = image_report_length - header_length;

    specific_device_type(const char* path) : base_type(path, 80, 80, 3, 2, image_format_type::bmp, image_report_length, false, true) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final;

    void reset() override final;

  private:
    void _set_brightness(std::byte p) override final;
  };


  // StreamDeck XL
  template<>
  struct specific_device_type<product_streamdeck_xl> : public device_type {
    using base_type = device_type;

    static constexpr unsigned image_report_length = 1024;
    static constexpr unsigned header_length = 8;
    static constexpr unsigned payload_length = image_report_length - header_length;

    specific_device_type(const char* path) : base_type(path, 96, 96, 8, 4, image_format_type::jpeg, image_report_length, true, true) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final;

    void reset() override final;

  private:
    void _set_brightness(std::byte p) override final;
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