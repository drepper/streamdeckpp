#include <cinttypes>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ranges>
#include <vector>
#include <experimental/array>

#include <error.h>
#include <hidapi.h>


namespace streamdeck {

  static constexpr uint16_t vendor_elgato = 0x0fd9;

  static constexpr uint16_t product_streamdeck_original = 0x0060;
  static constexpr uint16_t product_streamdeck_original_v2 = 0x006d;
  static constexpr uint16_t product_streamdeck_mini = 0x0063;
  static constexpr uint16_t product_streamdeck_xl = 0x006c;


  struct device_type {
    enum struct image_format_type { bmp, jpeg };

    device_type(const char* path, unsigned width, unsigned height, unsigned cols, unsigned rows, image_format_type imgfmt, unsigned imgreplen, bool hflip, bool vflip);

    ~device_type();


    bool connected() const { return m_d != nullptr; }
    auto path() const { return m_path; }

    void close();


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


    virtual std::vector<bool> read() = 0;

    virtual void reset() = 0;

    virtual std::string get_serial_number() = 0;

    virtual std::string get_firmware_version() = 0;

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
    auto get_report(C& data) { return hid_get_feature_report(m_d, (unsigned char*) data.data(), data.size()); }

    auto write(const unsigned char* data, size_t len) { return hid_write(m_d, data, len); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto write(const C& data) { return hid_write(m_d, (const unsigned char*) data.data(), data.size()); }

    auto read(unsigned char* data, size_t len) { return hid_read(m_d, data, len); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto read(C& data) { return hid_read(m_d, (unsigned char*) data.data(), data.size()); }
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


  // First generation.
  struct gen1_device_type : public device_type {
    using base_type = device_type;

    const unsigned image_report_length;
    static constexpr unsigned header_length = 8;
    const unsigned payload_length;

    gen1_device_type(const char* path, unsigned width, unsigned height, unsigned cols, unsigned rows, unsigned imgreplen, bool hflip, bool vflip)
    : device_type(path, width, height, cols, rows, image_format_type::bmp, imgreplen, hflip, vflip), image_report_length(imgreplen), payload_length(imgreplen - header_length)
    {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final;

    std::vector<bool> read() override final;

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

    void reset() override final;

    std::string get_serial_number() override final;

    std::string get_firmware_version() override final;

  private:
    void _set_brightness(std::byte p) override final;

    std::string _get_string(std::byte c, size_t off);
  };


  // StreamDeck Original
  template<>
  struct specific_device_type<product_streamdeck_original> : public gen1_device_type {
    using base_type = gen1_device_type;

    static constexpr unsigned image_report_length = 8191;

    specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3, image_report_length, true, true) {}
  };


  // StreamDeck Original V2
  template<>
  struct specific_device_type<product_streamdeck_original_v2> : public gen2_device_type {
    using base_type = gen2_device_type;

    specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3) {}
  };


  // StreamDeck Mini
  template<>
  struct specific_device_type<product_streamdeck_mini> : public gen1_device_type {
    using base_type = gen1_device_type;

    static constexpr unsigned image_report_length = 1024;

    specific_device_type(const char* path) : base_type(path, 80, 80, 3, 2, image_report_length, false, true) {}
  };


  // StreamDeck XL
  template<>
  struct specific_device_type<product_streamdeck_xl> : public gen2_device_type {
    using base_type = gen2_device_type;

    specific_device_type(const char* path) : base_type(path, 96, 96, 8, 4) {}
  };


  struct context {
    context(const char* path);
    ~context();

    bool empty() const { return devinfo.empty(); }
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
    static std::unique_ptr<device_type> get_device(unsigned short product_id, const char* path);

    hid_device_info* devs = nullptr;
    std::vector<std::unique_ptr<device_type>> devinfo;
  };

} // namespace streamdeck
