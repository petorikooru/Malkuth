#include "malkuth_audio.h"

MalkuthAudio* MalkuthAudio::_instance = nullptr;

bool MalkuthAudio::init(uint8_t pin_bck, uint8_t pin_ws, uint8_t pin_data){
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);
    auto config = _i2s.defaultConfig(TX_MODE);
    
    _instance = this;
    _source = new AudioSourceVector<FsFile>(&file_to_stream_callback);
    _player = new AudioPlayer(*_source, _i2s, _decoder);
    _directory = new NamePrinter(*_source);

    config.pin_bck  = pin_bck;
    config.pin_ws   = pin_ws;
    config.pin_data = pin_data;

    _decoder.addDecoder(_decoder_mp3, "audio/mpeg");
    _decoder.addDecoder(_decoder_aac, "audio/aac");
    _decoder.addDecoder(_decoder_wav, "audio/wav");
    _decoder.addDecoder(_decoder_flac, "audio/flac");

    if (!_i2s.begin(config)) {
      Serial.println("I2S failed to start");
      return false;
    }
    return true;
}

FsFile* MalkuthAudio::file_to_stream_callback(const char* path, FsFile& old_file) {
    if (!_instance) return nullptr;
    return _instance->file_to_stream(path, old_file);
}


FsFile* MalkuthAudio::file_to_stream(const char* path, FsFile& old_file){
    if (old_file.isOpen()) {
      old_file.close();
    }
    reset();

    String fullPath = String(path);
    String filename = fullPath.substring(fullPath.lastIndexOf('/') + 1);
    filename.toLowerCase();

    const char* file_extension[] = { ".mp3", ".flac", ".wav" };
    bool is_supported = false;
    
    for (const char* ext : file_extension) {
      if (filename.endsWith(ext)) {
        is_supported = true;
        break;
      }
    }

    // Skipping non audio file (trick)
    if (!is_supported) {
        _audio_file.open("");
        _current_track.title  = "";
        _current_track.artist = "";
        _current_track.album  = "";
        _not_a_music = true;
        return &_audio_file;
    }

    FsFile meta_file;

    if (meta_file.open(path)) {
        _current_track = get_metadata(meta_file, path);
        meta_file.close();
    }

    if (_current_track.title.isEmpty())  
        // _current_track.title   = getFileStem(path);
        _current_track.title = "Unknown Title";

    if (_current_track.artist.isEmpty()) 
        _current_track.artist  = "Unknown Artist";
      
    if (_current_track.album.isEmpty())  
        _current_track.album   = "Unknown Album";

    if (!_audio_file.open(path)) {
        _audio_file.open("");
        _current_track.title  = "";
        _current_track.artist = "";
        _current_track.album  = "";
        _not_a_music = true;
      return &_audio_file;
    }
    _not_a_music = false;
    return &_audio_file;
  }


  AudioMetadata MalkuthAudio::get_metadata_flac_vorbis(FsFile& file, uint32_t size) { 
    AudioMetadata metadata;
    uint32_t vendor_len;
    if (file.read(&vendor_len, 4) != 4) return metadata;

    file.seek(file.position() + vendor_len);

    uint32_t comment_count;
    if (file.read(&comment_count, 4) != 4) return metadata;

    const size_t max_buffer_size = 256;
    for (uint32_t i = 0; i < comment_count; i++) {
      uint32_t len;
      if (file.read(&len, 4) != 4) break;

      size_t read_len = std::min(len, static_cast<uint32_t>(max_buffer_size));
      char buf[read_len + 1];
      if (file.read(buf, read_len) != read_len) break;
      buf[read_len] = '\0';

      String entry = String(buf);
      if      (entry.startsWith("TITLE=")) metadata.title = entry.substring(6);
      else if (entry.startsWith("title=")) metadata.title = entry.substring(6);

      if      (entry.startsWith("ARTIST=")) metadata.artist = entry.substring(7);
      else if (entry.startsWith("artist=")) metadata.artist = entry.substring(7);

      if      (entry.startsWith("ALBUM=")) metadata.album = entry.substring(6);
      else if (entry.startsWith("album=")) metadata.album = entry.substring(6);
    }

    return metadata;
  }

AudioMetadata MalkuthAudio::get_metadata_flac(FsFile& file) {
    AudioMetadata metadata;
    float temp_duration;

    file.seek(0);
    char sig[4];
    if (file.read(sig, 4) != 4 || strncmp(sig, "fLaC", 4) != 0) {
      return metadata;
    }

    bool last_block = false;
    while (!last_block) {
      uint8_t header[4];
      if (file.read(header, 4) != 4) break;

      last_block = header[0] & 0x80;
      uint8_t block_type = header[0] & 0x7F;
      uint32_t block_size = (header[1] << 16) | (header[2] << 8) | header[3];

      if (block_type == 0) {
        uint8_t buf[34];

        if (file.read(buf, 34) != 34) return metadata;
        uint32_t sample_rate = ((uint32_t)buf[10] << 12) | (buf[11] << 4) | ((buf[12] >> 4) & 0x0F);
        uint8_t channels = ((buf[12] & 0x0E) >> 1) + 1;
        uint8_t bps = (((buf[12] & 0x01) << 4) | ((buf[13] >> 4) & 0x0F)) + 1;
        uint64_t total_samples = ((uint64_t)(buf[13] & 0x0F) << 32) | ((uint64_t)buf[14] << 24) | ((uint64_t)buf[15] << 16) | ((uint64_t)buf[16] << 8) | buf[17];

        temp_duration = (float)total_samples / sample_rate;
      } else if (block_type == 4) {
        metadata = get_metadata_flac_vorbis(file, block_size);
        metadata.duration = temp_duration;
        return metadata;
      } else {
        file.seek(file.position() + block_size);
      }
    }
    metadata.duration = temp_duration;
    return metadata;
  }

AudioMetadata MalkuthAudio::get_metadata_mp3v1(FsFile& file) {
    AudioMetadata metadata;

    if (file.size() < 128) return metadata;

    file.seek(file.size() - 128);
    char tag[3];
    if (file.read(tag, 3) != 3 || strncmp(tag, "TAG", 3)) return metadata;

    char buf[125] = { 0 };
    file.read((uint8_t*)buf, 125);

    char title[31] = { 0 }, artist[31] = { 0 }, album[31] = { 0 };
    strncpy(title, buf + 3, 30);
    strncpy(artist, buf + 33, 30);
    strncpy(album, buf + 63, 30);

    if (title[0])   metadata.title = String(title);
    if (artist[0])  metadata.artist = String(artist);
    if (album[0])   metadata.album = String(album);

    return metadata;
  }

  float MalkuthAudio::get_metadata_mp3_duration(FsFile& file) {
    file.seek(0);
    uint8_t buf[1024];
    size_t read = file.read(buf, sizeof(buf));
    int sample_rate = 44100;
    int bitrate = 128000;

    for (int i = 0; i < read - 4; i++) {
      if (buf[i] == 0xFF && (buf[i + 1] & 0xE0) == 0xE0) {
        int version = (buf[i + 1] & 0x18) >> 3;
        int sr_idx = (buf[i + 2] & 0x0C) >> 2;
        int br_idx = (buf[i + 2] & 0xF0) >> 4;

        const int sr_table[3][3] = { 
          { 44100, 48000, 32000 }, 
          { 22050, 24000, 16000 }, 
          { 11025, 12000, 8000 } 
        };

        const int br_table[2][15] = {
          { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
          { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 }
        };

        sample_rate = sr_table[version == 3 ? 0 : (version == 2 ? 1 : 2)][sr_idx];
        bitrate = br_table[version == 3 ? 0 : 1][br_idx] * 1000;
        break;
      }
    }

    if (bitrate > 0) {
      return (float)file.size() * 8 / bitrate;
    }
  }

// Idk what does it do exactly, but basically just extract metadata
AudioMetadata MalkuthAudio::get_metadata_mp3(FsFile& file) {
    AudioMetadata metadata;

    file.seek(0);
    char header[10];
    if (file.read(header, 10) != 10 || strncmp(header, "ID3", 3)) {
      metadata.duration = get_metadata_mp3_duration(file);
      return metadata;
    }

    uint32_t tagsize = ((header[6] & 0x7F) << 21) | ((header[7] & 0x7F) << 14) | ((header[8] & 0x7F) << 7) | (header[9] & 0x7F);

    size_t pos = 10;
    while (pos < tagsize + 10 && pos + 10 < file.size()) {
      char frame[4];
      if (file.read((uint8_t*)frame, 4) != 4 || frame[0] == 0) break;

      uint32_t fsize;
      file.read(&fsize, 4);
      fsize = __builtin_bswap32(fsize);
      file.seek(file.position() + 2);  // skip flags

      if (fsize > 512) {
        pos += 10 + fsize;
        file.seek(pos);
        continue;
      }

      char* data = new char[fsize + 1];
      file.read((char*)data, fsize);
      data[fsize] = 0;

      uint8_t enc_byte = (uint8_t)data[0];
      int enc = enc_byte;
      char* text_start = data + 1;
      size_t avail_bytes = fsize - 1;
      bool little_endian = false;
      if (enc == 1 && avail_bytes >= 2) {
        uint8_t bom1 = (uint8_t)text_start[0];
        uint8_t bom2 = (uint8_t)text_start[1];
        if ((bom1 == 0xFF && bom2 == 0xFE) || (bom1 == 0xFE && bom2 == 0xFF)) {
          little_endian = (bom2 == 0xFE);
          text_start += 2;
          avail_bytes -= 2;
        }
      }

      size_t text_bytes;
      if (enc == 3) {
        text_bytes = avail_bytes;
      } else {
        int unit_size = (enc == 0) ? 1 : 2;
        size_t num_units = avail_bytes / unit_size;
        size_t units_to_term = 0;
        const uint8_t* p = (const uint8_t*)text_start;
        if (unit_size == 1) {
          while (units_to_term < num_units && p[units_to_term] != 0) ++units_to_term;
        } else {
          while (units_to_term < num_units && (p[2 * units_to_term] != 0 || p[2 * units_to_term + 1] != 0)) ++units_to_term;
        }
        text_bytes = units_to_term * unit_size;
      }

      String value;
      if (enc == 3) {
        value = String(text_start, text_bytes);
      } else if (enc == 0) {
        value = "";
        const uint8_t* p = (const uint8_t*)text_start;
        for (size_t i = 0; i < text_bytes; ++i) {
          uint32_t ch = p[i];
          if (ch == 0) break;
          if (ch < 128) {
            value += (char)ch;
          } else {
            value += (char)(0xC0 | (ch >> 6));
            value += (char)(0x80 | (ch & 0x3F));
          }
        }
      } else {  // UTF-16 (enc 1 or 2)
        value = "";
        const uint8_t* p = (const uint8_t*)text_start;
        for (size_t i = 0; i < text_bytes / 2; ++i) {
          uint8_t b1 = p[2 * i];
          uint8_t b2 = p[2 * i + 1];
          uint16_t ch = little_endian ? (b2 << 8 | b1) : (b1 << 8 | b2);
          if (ch == 0) break;
          if (ch < 0x80) {
            value += (char)ch;
          } else if (ch < 0x800) {
            value += (char)(0xC0 | (ch >> 6));
            value += (char)(0x80 | (ch & 0x3F));
          } else {
            value += (char)(0xE0 | (ch >> 12));
            value += (char)(0x80 | ((ch >> 6) & 0x3F));
            value += (char)(0x80 | (ch & 0x3F));
          }
        }
      }

      if      (strncmp(frame, "TIT2", 4) == 0) metadata.title  = value;
      else if (strncmp(frame, "TPE1", 4) == 0) metadata.artist = value;
      else if (strncmp(frame, "TALB", 4) == 0) metadata.album  = value;

      delete[] data;
      pos += 10 + fsize;
      file.seek(pos);
    }

    return metadata;
}

AudioMetadata MalkuthAudio::get_metadata_wav(FsFile& file) {
    AudioMetadata metadata;

    file.seek(0);
    char riff[4];
    if (file.read(riff, 4) != 4 || strncmp(riff, "RIFF", 4)) return metadata;

    file.seek(20);
    uint16_t format;
    file.read(&format, 2);
    if (format != 1) return metadata;

    uint16_t channels;
    file.read(&channels, 2);
    uint32_t sample_rate;
    file.read(&sample_rate, 4);
    uint32_t byte_rate;
    file.read(&byte_rate, 4);
    file.seek(file.position() + 6);
    uint16_t bits;
    file.read(&bits, 2);

    // Find data chunk
    while (true) {
      char chunk[4];
      if (file.read(chunk, 4) != 4) break;
      uint32_t size;
      file.read(&size, 4);

      if (strncmp(chunk, "data", 4) == 0) {
        metadata.duration = size / (float)byte_rate;
        break;
      } else if (strncmp(chunk, "LIST", 4) == 0) {
        char type[4];
        file.read(type, 4);
        if (strncmp(type, "INFO", 4) == 0) {
          size_t end = file.position() + size - 4;
          while (file.position() + 8 < end) {
            char id[4];
            file.read(id, 4);
            uint32_t len;
            file.read(&len, 4);
            char* buf = new char[len + 1];
            file.read((uint8_t*)buf, len);
            buf[len && buf[len - 1] == 0 ? len - 1 : len] = 0;
            String val = buf;
            delete[] buf;

            if (strncmp(id, "INAM", 4) == 0)      metadata.title = val;
            else if (strncmp(id, "IART", 4) == 0) metadata.artist = val;
            else if (strncmp(id, "IPRD", 4) == 0) metadata.album = val;

            if (len % 2) file.seek(file.position() + 1);
          }
        } else file.seek(file.position() + size - 4);
      } else {
        file.seek(file.position() + size);
      }
    }
    return metadata;
}

AudioMetadata MalkuthAudio::get_metadata(FsFile& file, const char* path){
    AudioMetadata metadata;

    String ext = String(path);
    ext.toLowerCase();
    ext = ext.substring(ext.lastIndexOf('.') + 1);

    if (ext == "flac") {
        metadata = get_metadata_flac(file);
    } else if (ext == "mp3") {
        metadata = get_metadata_mp3(file);
    } else if (ext == "wav") {
        metadata = get_metadata_wav(file);
    }

    return metadata;
}

AudioMetadata MalkuthAudio::get_metadata(){
    return _current_track;
}

void MalkuthAudio::reset(){
    _please_update = true;
    _i2s.resetBytesWritten();

    _current_track.artist.clear();
    _current_track.title.clear();
    _current_track.album.clear();
    _current_track.duration = 0;

    memset(_cover_path, sizeof(_cover_path), 0);
}

void MalkuthAudio::loop() {
    _player->copy();
}

void MalkuthAudio::toggle(bool active) {
  if (!active) {
    _player->play();
    _playing = true;
  }
  else {
    _player->stop();
    _playing = false;
  }
}

void MalkuthAudio::toggle() {
  if (!_playing) {
    _player->play();
    _playing = true;
  }
  else {
    _player->stop();
    _playing = false;
  }
}

void MalkuthAudio::next() {
    _player->next(); 
}

void MalkuthAudio::previous() {
    _player->previous(); 
}

uint8_t MalkuthAudio::get_volume() {
    return _volume;
}

bool MalkuthAudio::get_status(){
    if (_player->isActive())
        return true;
    else
        return false;
}

void MalkuthAudio::set_sdfs(SdFs& sd){
    _sd = &sd;
}

void MalkuthAudio::set_volume(uint8_t percent){
    if (percent > 100)
        percent = 100;

    _volume = percent;

    float real_percent = percent / 100.0f;
    _player->setVolume(real_percent);
}

void MalkuthAudio::process_directory(const char* path){
    _player->stop();

    _directory->flush();
    _source->clear();

    _audio_file = _sd->open(path, O_READ);
    if (!_audio_file) {
        return;
    }

    SdFile dir;
    SdFile entry;
    _image_type = ImageType::NONE;

    dir.open(path);

    while (entry.openNext(&dir, O_RDONLY)){
        char filename[256];

        entry.getName(filename, sizeof(filename));

        String full_path = String(path) + String(filename);

        process_albumcover(full_path);
        entry.close();
    }

    dir.close();

    _directory->setPrefix(path);
    _audio_file.rewind();
    _audio_file.ls(_directory, LS_R);
    _audio_file.close();

    if (!_player->begin()){
      Serial.println("Player failed to start");
      return;      
    }
    set_volume(_volume);
}

bool MalkuthAudio::get_update(){
    return _please_update;
}

float MalkuthAudio::get_position(){
    return _i2s.getAudioCurrentTime();
}

void MalkuthAudio::yeah_i_have_updated(){
    _please_update = false;
}

char* MalkuthAudio::get_coverpath(){
    return _cover_path;
}

ImageType MalkuthAudio::get_covertype(){
    return _image_type;
}

bool MalkuthAudio::is_actually_audio(){
    return (!_not_a_music);
}

void MalkuthAudio::process_albumcover(String path) {
    if (path.endsWith("AlbumArtSmall.jpg")) {
        if (_cover_priority < CoverPriority::SMALL_COVER) {
            strcpy(_cover_path, path.c_str());
            _image_type = ImageType::JPG;
            _cover_priority = CoverPriority::SMALL_COVER;
        }
        return;
    }

    else if (path.endsWith("Cover.jpg") || path.endsWith("cover.jpg")) {
        if (_cover_priority < CoverPriority::COVER_JPG) {
            strcpy(_cover_path, path.c_str());
            _image_type = ImageType::JPG;
            _cover_priority = CoverPriority::COVER_JPG;
        }
        return;
    }

    else if (path.endsWith("Cover.png") || path.endsWith("cover.png")) {
        if (_cover_priority < CoverPriority::COVER_PNG) {
            strcpy(_cover_path, path.c_str());
            _image_type = ImageType::PNG;
            _cover_priority = CoverPriority::COVER_PNG;
        }
        return;
    }
}