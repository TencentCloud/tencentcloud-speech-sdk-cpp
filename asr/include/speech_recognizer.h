
// Copyright 1998-2020 Tencent Copyright
#ifndef _SPEECH_RECOGNIZER_
#define _SPEECH_RECOGNIZER_
#include <string>
#include <vector>
#include "tcloud_public_request_builder.h"
#define REALTIME_ASR_URL_FORMAT                                                \
    "ws://asr.cloud.tencent.com/asr/v2/%s?%s&signature=%s"
enum EventType {
    TaskFail = 0,
    RecognitionStart,
    SentenceBegin,
    RecognitionResultChanged,
    SentenceEnd,
    RecognitionComplete,
};

typedef struct {
    std::string word;
    uint32_t start_time;
    uint32_t end_time;
    uint32_t stable_flag;
} ResultWord;

typedef struct {
    uint32_t slice_type;
    int index;
    uint32_t start_time;
    uint32_t end_time;
    std::string voice_text_str;
    uint32_t word_size;
    std::vector<ResultWord> word_list;
} RecognitionResult;

typedef struct {
    int code;
    std::string message;
    std::string voice_id;
    std::string message_id;
    uint32_t final_rsp;
    RecognitionResult result;
} SpeechRecognitionResponse;

class SpeechListener;
typedef void (*ASRCallBackFunc)(SpeechRecognitionResponse *);

typedef struct {
    std::string appid;
    std::string secret_id;
    std::string engine_model_type;
    std::string voice_format;
    std::string need_vad;
    std::string filter_dirty;
    std::string filter_model;
    std::string filter_punc;
    std::string convert_num_mode;
    std::string word_info;
    std::string hotword_id;
    std::string secret_key;
    std::string voice_id;
    std::string nonce;
    std::string vad_silence_time;
} SpeechRecognizerConfig;

class SpeechRecognizer {
 public:
    SpeechRecognizer(std::string appid, std::string secret_id,
                     std::string secret_key);

    ~SpeechRecognizer();

    void InitSpeechRecognizerConfig(std::string appid, std::string secret_id,
                                    std::string secret_key);

    void Start();

    void Terminate();

    void Stop();

    void Write(void *payload, size_t len);

    void SetOnFail(ASRCallBackFunc event);

    void SetOnRecognitionStart(ASRCallBackFunc event);

    void SetOnRecognitionComplete(ASRCallBackFunc event);

    void SetOnRecognitionResultChanged(ASRCallBackFunc event);

    void SetOnSentenceBegin(ASRCallBackFunc event);

    void SetOnSentenceEnd(ASRCallBackFunc event);

    void SetVoiceId(std::string voice_id);

    void SetNonce(uint64_t nonce);

    void SetEngineModelType(std::string engine_model_type);

    void SetVoiceFormat(int voice_format);

    void SetNeedVad(int need_vad);

    void SetHotwordId(std::string hotword_id);

    void SetFilterDirty(int filter_dirty);

    void SetFilterPunc(int filter_punc);

    void SetFilterModal(int filter_modal);

    void SetConvertNumMode(int convert_num_mode);

    void SetWordInfo(int word_info);

    void SetVadSilenceTime(int vad_silence_time);

    SpeechListener *m_listener;
    SpeechRecognizerConfig m_config;
    RequestBuilder m_builder;

    ASRCallBackFunc on_task_failed;
    ASRCallBackFunc on_recognition_start;
    ASRCallBackFunc on_recognition_complete;
    ASRCallBackFunc on_recognition_result_changed;
    ASRCallBackFunc on_sentence_begin;
    ASRCallBackFunc on_sentence_end;

 private:
    void BuildRequest();

    std::string GetRequestURL();

    std::string GetWebsocketURL();
};

#endif
