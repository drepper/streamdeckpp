#include <cinttypes>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ranges>
#include <vector>
#include <experimental/array>

#include <error.h>
#include <fcntl.h>
#include <hidapi.h>
#include <unistd.h>
#include <sys/stat.h>


namespace streamdeck {

  static constexpr uint16_t vendor_elgato = 0x0fd9;

  static constexpr uint16_t product_streamdeck_original = 0x0060;
  static constexpr uint16_t product_streamdeck_original_v2 = 0x006d;
  static constexpr uint16_t product_streamdeck_mini = 0x0063;
  static constexpr uint16_t product_streamdeck_xl = 0x006c;


  struct device_type {
    enum struct image_format_type { bmp, jpeg };

    device_type(const char* path, unsigned width, unsigned height, unsigned cols, unsigned rows, image_format_type imgfmt, unsigned imgreplen)
    : pixel_width(width), pixel_height(height), key_cols(cols), key_rows(rows), key_count(rows * height),
      key_image_format(imgfmt), image_report_length(imgreplen),
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

    virtual payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) = 0;


    virtual void reset() = 0;


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

    static constexpr unsigned image_report_length = 1024;
    static constexpr unsigned header_length = 8;
    static constexpr unsigned payload_length = image_report_length - header_length;

    specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3, image_format_type::bmp, 8191) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final
    {
      auto it = buffer.begin();
      return it;
    }


    void reset()
    {
      const auto req = std::experimental::make_array(0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
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

    specific_device_type(const char* path) : base_type(path, 72, 72, 5, 3, image_format_type::jpeg, 1024) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final
    {
      auto it = buffer.begin();
      return it;
    }


    void reset()
    {
      const auto req = std::experimental::make_array(0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
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

    specific_device_type(const char* path) : base_type(path, 80, 80, 3, 2, image_format_type::bmp, 1024) {}

    payload_type::iterator add_header(payload_type& buffer, unsigned key, unsigned remaining, unsigned page) override final
    {
      auto it = buffer.begin();
      return it;
    }


    void reset()
    {
      const auto req = std::experimental::make_array(0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
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

    specific_device_type(const char* path) : base_type(path, 96, 96, 8, 4, image_format_type::jpeg, image_report_length) {}

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


    void reset()
    {
      const auto req = std::experimental::make_array(0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
      send_report(req);
    }

  };


  struct context {
    context();
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


  context::context()
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
  }

  context::~context()
  {
    devinfo.clear();
    hid_free_enumeration(devs);
    hid_exit();
  }

}


int main()
{
  streamdeck::context ctx;
  if (! ctx.any())
    error(EXIT_FAILURE, 0, "failed streamdeck::context initialization");

#if 1
  const char fname[] = "test.jpg";
  int fd = open(fname, O_RDONLY);
  struct stat st;
  fstat(fd, &st);
  std::vector<std::byte> img(st.st_size);
  read(fd, img.data(), img.size());
  ctx[0]->set_key_image(0, img);
  close(fd);
#else
  ctx[0]->reset();
#endif
}
