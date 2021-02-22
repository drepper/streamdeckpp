#ifndef _STREAMDECKPP_HH
#define _STREAMDECKPP_HH 1

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ranges>
#include <vector>
#include <version>
#if !__has_include(<experimental/array>)
# error "need <experimental/array> header"
#endif
#include <experimental/array>

#include <error.h>
#include <hidapi.h>
#include <Magick++.h>


static_assert(__cpp_static_assert >= 200410L, "extended static_assert missing");
static_assert(__cpp_concepts >= 201907L);
static_assert(__cpp_if_constexpr >= 201606L);
static_assert(__cpp_lib_ranges >= 201911L);
static_assert(__cpp_lib_byte >= 201603L);
static_assert(__cpp_lib_clamp >= 201603L);
static_assert(__cpp_lib_make_unique >= 201304L);
static_assert(__cpp_lib_optional >= 201606L);


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

    int register_image(Magick::Image&& image);
    int register_image(const Magick::Image& image) { return register_image(Magick::Image(image)); }
    int register_image(const char* fname);

    int set_key_image(unsigned key, Magick::Image&& image);
    int set_key_image(unsigned key, const Magick::Image& image) { return set_key_image(key, Magick::Image(image)); }
    int set_key_image(unsigned key, const char* fname);
    int set_key_image(unsigned row, unsigned col, const char* fname) { return set_key_image(row * key_cols + col, fname); }

    int set_key_image(unsigned key, int handle);
    int set_key_image(unsigned row, unsigned col, int handle) { return set_key_image(row * key_cols + col, handle); }

    virtual payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) = 0;


    virtual std::vector<bool> read() = 0;

    virtual std::optional<std::vector<bool>> read(int timeout) = 0;

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
    auto write(const C& data) { assert(data.size() == image_report_length); return hid_write(m_d, (const unsigned char*) data.data(), image_report_length); }

    auto read(unsigned char* data, size_t len) { return hid_read(m_d, data, len); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto read(C& data) { return hid_read(m_d, (unsigned char*) data.data(), data.size()); }
    auto read(unsigned char* data, size_t len, int timeout) { return hid_read_timeout(m_d, data, len, timeout); }
    template<typename C>
    requires std::ranges::contiguous_range<C>
    auto read(C& data, int timeout) { return hid_read_timeout(m_d, (unsigned char*) data.data(), data.size(), timeout); }

    template<typename C>
    int set_key_image(unsigned key, const C& data);

  private:
    Magick::Blob reformat(Magick::Image&& image);

    const char* const m_path;
    hid_device* const m_d;

    std::vector<Magick::Blob> registered;
  };


  struct context {
    context();
    ~context();

    bool empty() const { return devinfo.empty(); }
    auto size() const { return devinfo.size(); }
    auto begin() { return devinfo.begin(); }
    auto end() { return devinfo.end(); }
    auto& operator[](size_t n) { return devinfo[n]; }

  private:
    hid_device_info* devs = nullptr;
    std::vector<std::unique_ptr<device_type>> devinfo;
  };

} // namespace streamdeck

#endif // streamdeckpp.hh
