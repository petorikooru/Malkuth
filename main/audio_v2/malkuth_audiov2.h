#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSDFAT.h>
#include <AudioTools/AudioCodecs/CodecFLACFoxen.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <AudioTools/AudioCodecs/CodecAACHelix.h>
#include <AudioTools/AudioCodecs/CodecWAV.h>
#include <AudioTools/Concurrency/RTOS.h>

#include <SdFat.h>

#include "malkuth_helper.h"

typedef struct {
    String artist;
    String title;
    String album;

    float duration;
} AudioMetadata;

enum class AudioCommandType : uint8_t {
    PLAY,
    STOP,
    TOGGLE,
    NEXT,
    PREVIOUS,
    SET_VOLUME,
    PROCESS_DIRECTORY,
};

struct AudioCommand {
    AudioCommandType type;
    union {
        uint8_t volume;
        char    path[128];
    } payload;
};

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
    if (info.sample_rate == 0) 
      return 0.0f;
    int byte_rate = info.sample_rate * info.channels * (info.bits_per_sample / 8);
    return byte_rate > 0 ? (float)bytes_written / byte_rate : 0.0f;
  }
  void resetBytesWritten() {
    bytes_written = 0;
  }
};

class MalkuthAudio {
private:
    QueueHandle_t _queue_audio = nullptr;
    TaskHandle_t  _task_audio  = nullptr;    
    TaskHandle_t  _init_waiter = nullptr;

    bool _init_ok = false;

    FsFile        _audio_file;
    SdFs*         _sd         = nullptr;
    bool          _playing    = false;
    uint8_t       _volume     = 10;
    float         _current_duration = 0.0f;
    
    // TODO :  Multiprocessing on Audio
    // BufferRTOS<uint8_t> buffer(1024 * 10);
    // QueueStream<uint8_t> queue(buffer);

    enum class CoverPriority {
        NONE        = 0,
        COVER_PNG   = 1,
        COVER_JPG   = 2,
        SMALL_COVER = 3
    };
    CoverPriority _cover_priority = CoverPriority::NONE;
    char          _cover_path[128] = {};
    ImageType     _image_type;

    uint8_t _pin_bck;
    uint8_t _pin_ws;
    uint8_t _pin_data;

    AudioSourceVector<FsFile>*  _source;
    AudioPlayer*                _player;
    NamePrinter*                _directory;

    CustomI2S _i2s;

    MultiDecoder      _decoder;
    FLACDecoderFoxen  _decoder_flac;
    MP3DecoderHelix   _decoder_mp3;
    AACDecoderHelix   _decoder_aac;
    WAVDecoder        _decoder_wav;

    AudioMetadata  _current_track;

    bool _please_update = false;
    bool _not_a_music   = false;

    static MalkuthAudio* _instance;

    static void task_audio(void* parameters);
    static void handle_command(MalkuthAudio* self, const AudioCommand& cmd);
    static void process_directory_task(MalkuthAudio* self, const AudioCommand& cmd);

    static FsFile*  file_to_stream_callback(const char* path, FsFile& old_file);
    FsFile*         file_to_stream(const char* path, FsFile& old_file);

    static AudioMetadata get_metadata(FsFile& file, const char* path);

    static AudioMetadata get_metadata_flac(FsFile& file);
    static AudioMetadata get_metadata_flac_vorbis(FsFile& file, uint32_t size);

    static AudioMetadata get_metadata_mp3(FsFile& file);
    static AudioMetadata get_metadata_mp3v1(FsFile& file);
    static float         get_metadata_mp3_duration(FsFile& file);

    static AudioMetadata get_metadata_wav(FsFile& file);

    void process_albumcover(String path);

public:
    bool init(uint8_t pin_bck = 8, uint8_t pin_ws = 17, uint8_t pin_data = 18);
    bool init(uint8_t core, uint8_t pin_bck = 8, uint8_t pin_ws = 17, uint8_t pin_data = 18);
    void reset();
  
    AudioMetadata get_metadata();

    void process_directory(const char* path);

    void set_sdfs(SdFs& sd);
    void set_volume(uint8_t percent);

    void loop();
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