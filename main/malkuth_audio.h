#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSDFAT.h>
#include <AudioTools/AudioCodecs/CodecFLACFoxen.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <AudioTools/AudioCodecs/CodecAACHelix.h>
#include <AudioTools/AudioCodecs/CodecWAV.h>
#include <AudioTools/Concurrency/RTOS.h>
#include "AudioTools/AudioLibs/Concurrency.h" 

#include <SdFat.h>

#include "malkuth_helper.h"

typedef struct {
    String artist;
    String title;
    String album;

    float duration;
} AudioMetadata;

class CustomI2S : public I2SStream {
public:
  uint64_t bytes_written = 0;

  size_t write(const uint8_t* buffer, size_t size) override {
    size_t res      = I2SStream::write(buffer, size);
    bytes_written  += res;

    return res;
  }
  float getAudioCurrentTime() {
    auto info = audioInfo();

    if (info.sample_rate == 0) return 0.0f;

    int byte_rate = info.sample_rate * info.channels * (info.bits_per_sample / 8);
    
    return byte_rate > 0 ? (float)bytes_written / byte_rate : 0.0f;
  }

  void resetBytesWritten() {
    bytes_written = 0;
  }
};


class MalkuthAudio {
private:
    FsFile    _audio_file;
    SdFs*     _sd;
    bool      _playing;
    uint8_t   _volume = 10;
    static float     _current_duration;

    AudioSourceVector<FsFile>*  _source;
    AudioPlayer*                _player;

    // TODO :  Multiprocessing on Audio
    BufferRTOS<uint8_t>  _buffer_audio;
    QueueStream<uint8_t> _queue_audio;

    CustomI2S _i2s;

    MultiDecoder      _decoder;
    FLACDecoderFoxen  _decoder_flac;
    MP3DecoderHelix   _decoder_mp3;
    AACDecoderHelix   _decoder_aac;
    WAVDecoder        _decoder_wav;

    AudioMetadata  _current_track;
    NamePrinter*   _directory;

    bool _please_update = false;
    bool _not_a_music   = false;

    static MalkuthAudio* self;

    enum class CoverPriority {
        NONE = 0,
        COVER_PNG   = 1,
        COVER_JPG   = 2,
        SMALL_COVER = 3
    };
    CoverPriority _cover_priority = CoverPriority::NONE;
    char      _cover_path[128] = {};
    ImageType _image_type;

    static FsFile*  file_to_stream_callback(const char* path, FsFile& old_file);
    FsFile*         file_to_stream(const char* path, FsFile& old_file);

    // static void  file_to_stream_callback(const char* path, FsFile& old_file);
    // void         file_to_stream(const char* path, FsFile& old_file);

    static AudioMetadata get_metadata(FsFile& file, const char* path);

    static AudioMetadata get_metadata_flac(FsFile& file);
    static AudioMetadata get_metadata_flac_vorbis(FsFile& file, uint32_t size);

    static AudioMetadata get_metadata_mp3(FsFile& file);
    static AudioMetadata get_metadata_mp3v1(FsFile& file);
    static float         get_metadata_mp3_duration(FsFile& file);

    static AudioMetadata get_metadata_wav(FsFile& file);

public:
  MalkuthAudio():
    _buffer_audio(1024 * 10),
    _queue_audio(_buffer_audio)
  {}

    bool init(uint8_t pin_bck = 8, uint8_t pin_ws = 17, uint8_t pin_data = 18);
    void reset();
  
    AudioMetadata get_metadata();

    void process_directory(const char* path);
    void process_albumcover(String path);

    void set_sdfs(SdFs& sd);
    void set_volume(uint8_t percent);

    size_t loop();
    size_t loop_all();
    void   start_loop();

    void next();
    void previous();

    void toggle();
    void toggle(bool active);

    uint8_t get_volume();
    bool    get_update();
    bool    get_status();
    float   get_position();
    char*   get_coverpath();
    ImageType get_covertype();

    bool    is_actually_audio();
    void  yeah_i_have_updated();

};