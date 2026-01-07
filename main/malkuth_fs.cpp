#include "malkuth_fs.h"

bool MalkuthFs::init(){
    _exfat_spi = new ExfatSpi(2, 42, 40, 41);
    _pin_cs = 2;

    _state = _sd.begin(SdSpiConfig(_pin_cs, DEDICATED_SPI, SPI_SPEED, _exfat_spi));
    if (!_state)
        return false;
    else
        return true;
}

bool MalkuthFs::init(uint8_t pin_cs, uint8_t pin_mosi, uint8_t pin_miso, uint8_t pin_clk){
    _exfat_spi = new ExfatSpi(pin_cs, pin_mosi, pin_miso, pin_clk);
    _pin_cs = pin_cs;

    _state = _sd.begin(SdSpiConfig(pin_cs, DEDICATED_SPI, SPI_SPEED, _exfat_spi));
    if (!_state)
        return false;
    else
        return true;
}

SdFs& MalkuthFs::get_sdfs(){
    return _sd;
}

FsFile& MalkuthFs::get_file(){
    return _file;
}

bool MalkuthFs::get_state(){
    if (digitalRead(_pin_cs) == HIGH) 
        _state = false;

    return _state;
}

std::vector<String> MalkuthFs::get_directory_files(const char* path){
    SdFile dir;
    SdFile entry;
    std::vector<String> items;

    if (!dir.open(path))
      return items;

    while (entry.openNext(&dir, O_RDONLY)){
        char filename[256];

        entry.getName(filename, sizeof(filename));

        String item = filename;
        if (entry.isDir()) item += '/';

        items.push_back(item);
        entry.close();
    }

    dir.close();
    return items;
}
