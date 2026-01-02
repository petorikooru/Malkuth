#include <vector>
#include <SPI.h>
#include <SdFat.h>

#define SD_FAT_TYPE 3
#define SPI_SPEED   SD_SCK_MHZ(79)

/// Taken from https://github.com/greiman/SdFat/issues/450
class ExfatSpi : public SdSpiBaseClass {
 public:
  ExfatSpi(const uint8_t pin_cs, const uint8_t pin_mosi, const uint8_t pin_miso, const uint8_t pin_clk){
    _pin_cs   = pin_cs;
    _pin_mosi = pin_mosi;
    _pin_miso = pin_miso;
    _pin_clk  = pin_clk;
  }
  
  void activate() { 
    SPI.beginTransaction(m_spiSettings); 
  }

  void begin(SdSpiConfig config) {
    (void)config;
    SPI.begin(_pin_clk, _pin_miso, _pin_mosi, -1);
  }

  void deactivate() { 
    SPI.endTransaction(); 
  }
  uint8_t receive() { 
    return SPI.transfer(0XFF); 
  }
  
  uint8_t receive(uint8_t* buf, size_t count) {
    for (size_t i = 0; i < count; i++) {
      buf[i] = SPI.transfer(0XFF);
    }
    return 0;
  }

  void send(uint8_t data) { 
    SPI.transfer(data); 
  }

  void send(const uint8_t* buf, size_t count) {
    for (size_t i = 0; i < count; i++) {
      SPI.transfer(buf[i]);
    }
  }
  
  void setSckSpeed(uint32_t maxSck) {
    m_spiSettings = SPISettings(maxSck, MSBFIRST, SPI_MODE0);
  }

 private:
  SPISettings m_spiSettings;

  uint8_t _pin_cs;
  uint8_t _pin_mosi;
  uint8_t _pin_miso;
  uint8_t _pin_clk;
};

class MalkuthFs {
    private:
        SdFs      _sd;
        FsFile    _file;
        ExfatSpi  *_exfat_spi = NULL;
        bool      _state;

        uint8_t   _pin_cs;

    public:
        bool init();
        bool init(const uint8_t pin_cs, const uint8_t pin_mosi, const uint8_t pin_miso, const uint8_t pin_clk);

        SdFs&     get_sdfs();
        FsFile&   get_file();

        // true  -> successfully initialized
        // false -> idk, something wrong i guess
        bool      get_state();

        std::vector<String> get_directory_files(const char* path);
};