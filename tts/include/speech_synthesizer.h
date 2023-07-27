
// Copyright 1998-2020 Tencent Copyright
#ifndef _SPEECH_SYNTHESIZER_
#define _SPEECH_SYNTHESIZER_
#include <string>
#include <vector>
#include "tcloud_public_request_builder.h"
#define REALTIME_TTS_URL_FORMAT                                                \
    "wss://tts.cloud.tencent.com/stream_ws?%s&Signature=%s"
#define REALTIME_TTS_HOSTNAME  "tts.cloud.tencent.com"
#define REALTIME_TTS_ACTION "TextToStreamAudioWS"

typedef struct {
    std::string text;
    uint32_t begin_time;
    uint32_t end_time;
    uint32_t begin_index;
    uint32_t end_index;
    std::string phoneme;
} Subtitle;

typedef struct {
    std::vector<Subtitle> subtitles;
} SynthesisResult;

typedef struct {
    // binary
    std::string data;

    // text
    int code;
    std::string message;
    std::string session_id;
    std::string request_id;
    std::string message_id;
    uint32_t final;
    SynthesisResult result;
} SpeechSynthesisResponse;

class SpeechListener;
typedef void (*TTSCallBackFunc)(SpeechSynthesisResponse *);

typedef struct {
    std::string appid;
    std::string secret_id;
    std::string secret_key;

    std::string action;
    std::string model_type;
    std::string voice_type;
    std::string codec;
    std::string sample_rate;
    std::string speed;
    std::string volume;
    std::string session_id;
    std::string text;
    std::string enable_subtitle;
} SpeechRecognizerConfig;

class SpeechSynthesizer {
 public:
    SpeechSynthesizer(std::string appid, std::string secret_id,
                     std::string secret_key, std::string session_id);

    ~SpeechSynthesizer();

    void InitSpeechRecognizerConfig(std::string appid, std::string secret_id,
                                    std::string secret_key, std::string session_id);

    int Start();

    void Wait();

    void Stop(std::string reason);

    void SetReady();

    void SetFailed();

    void SetOnSynthesisFail(TTSCallBackFunc event);

    void SetOnSynthesisStart(TTSCallBackFunc event);

    void SetOnSynthesisEnd(TTSCallBackFunc event);

    void SetOnTextResult(TTSCallBackFunc event);

    void SetOnAudioResult(TTSCallBackFunc event);

    void SetModelType(std::string model_type);

    void SetVoiceType(uint64_t voice_type);

    void SetCodec(std::string codec);

    void SetSampleRate(int sample_rate);

    void SetSpeed(float speed);

    void SetVolume(float volume);

    void SetText(std::string text);

    void SetEnableSubtitle(bool enable_subtitle);

    SpeechListener *m_listener;
    SpeechRecognizerConfig m_config;
    RequestBuilder m_builder;

    TTSCallBackFunc on_synthesis_start;
    TTSCallBackFunc on_synthesis_end;
    TTSCallBackFunc on_synthesis_fail;
    TTSCallBackFunc on_audio_result;
    TTSCallBackFunc on_text_result;

    std::string m_session_id;

 private:
    bool ready_;
    bool failed_;

    void BuildRequest();

    std::string GetRequestURL();

    std::string GetWebsocketURL();
};

#endif
