
// Copyright 1998-2020 Tencent Copyright
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#ifdef _WIN32
#include "unistd_win.h"
#include <windows.h>
#define sleep(sec) Sleep(sec * 1000)
#define msleep(msec) Sleep(msec)
#else
#include <pthread.h>
#include <unistd.h>
#define msleep(msec) usleep(msec * 1000)
#endif
#include "speech_recognizer.h"
#include "tcloud_util.h"

#define AUDIO_FILE_NUMS 2
#define AUDIO_FILE_NAME_LENGTH 32
#pragma comment(lib, "pthreadVSE2")
#pragma comment(lib, "libcrypto")
#ifdef _WIN32
std::string utf8_to_gbk(const std::string &str_utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str_utf8.c_str(), -1, NULL, 0);
    uint16_t *short_gbk = new uint16_t[len + 1];
    memset(short_gbk, 0, len * 2 + 2);

    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *> (str_utf8.c_str()),
                        -1, reinterpret_cast<wchar_t *> (short_gbk), len);

    len = WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<wchar_t *> (short_gbk),
                              -1, NULL, 0, NULL, NULL);

    char *char_gbk = new char[len + 1];
    memset(char_gbk, 0, len + 1);
    WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<wchar_t *> (short_gbk), -1,
                        char_gbk, len, NULL, NULL);

    std::string result_gbk(char_gbk);
    delete[] char_gbk;
    delete[] short_gbk;

    return result_gbk;
}

#endif

typedef struct {
    std::string audio_file;
} process_param_t;

// 开始识别回调函数
std::string gettime() {
    time_t rawtime;
    struct tm info;
    char buffer[80];
    time(&rawtime);

    localtime_r(&rawtime, &info);

    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", &info);
    return buffer;
}

void OnRecognitionStart(SpeechRecognitionResponse *rsp) {
    std::cout << gettime() << "| OnRecognitionStart | " << rsp->voice_id
            << std::endl;
}
// 识别失败回调
void OnFail(SpeechRecognitionResponse *rsp) {
    std::cout << gettime() << "| OnFail |" << rsp->code << " failed message"
              << rsp->message  << " voice_id " << rsp->voice_id << std::endl;
}

// 识别到一句话的开始
void OnSentenceBegin(SpeechRecognitionResponse *rsp) {
    std::string text = rsp->result.voice_text_str;
#ifdef _WIN32
    text = utf8_to_gbk(text);
#endif
    std::cout << gettime() << "| OnSentenceBegin | rsp text " << text
              << " voice_id " << rsp->voice_id << std::endl;
}

// 识别到一句话的结束
void OnSentenceEnd(SpeechRecognitionResponse *rsp) {
    std::string text = rsp->result.voice_text_str;
#ifdef _WIN32
    text = utf8_to_gbk(text);
#endif

    std::cout << gettime() << "| OnSentenceEnd | rsp text " << text
              << " voice_id " << rsp->voice_id << std::endl;
}

// 识别结果发生变化回调
void OnRecognitionResultChange(SpeechRecognitionResponse *rsp) {
    std::string text = rsp->result.voice_text_str;
#ifdef _WIN32
    text = utf8_to_gbk(text);
#endif

    std::cout << gettime() << "| OnRecognitionResultChange | rsp text " <<
        text << " voice_id " << rsp->voice_id << std::endl;
}
// 识别完成回调
void OnRecognitionComplete(SpeechRecognitionResponse *rsp) {
    std::cout << gettime() << "| OnRecognitionComplete | " << rsp->voice_id
              << std::endl;
}

void *process(void *arg) {
    process_param_t *param = reinterpret_cast<process_param_t *>(arg);
    //输入从官网申请的账号appid/secret_id/secret_key
    std::string appid = "";
    std::string secret_id = "";
    std::string secret_key = "";

    //gen unique voice_id
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()(); 
    std::string voice_id_str = boost::uuids::to_string(a_uuid);

    SpeechRecognizer *recognizer =
        new SpeechRecognizer(appid, secret_id, secret_key);
    recognizer->SetVoiceId(voice_id_str);
    recognizer->SetOnRecognitionStart(OnRecognitionStart);
    recognizer->SetOnFail(OnFail);
    recognizer->SetOnRecognitionComplete(OnRecognitionComplete);
    recognizer->SetOnRecognitionResultChanged(OnRecognitionResultChange);
    recognizer->SetOnSentenceBegin(OnSentenceBegin);
    recognizer->SetOnSentenceEnd(OnSentenceEnd);
    recognizer->SetEngineModelType("16k_zh");
    recognizer->SetNeedVad(1);
    recognizer->SetHotwordId("");
    recognizer->SetCustomizationId("");
    recognizer->SetFilterDirty(1);
    recognizer->SetFilterModal(1);
    recognizer->SetFilterPunc(1);
    recognizer->SetConvertNumMode(1);
    recognizer->SetWordInfo(0);
    int ret = recognizer->Start();
    if (ret < 0) {
        std::cout << " recognizer start failed \n" << std::endl;
        delete recognizer;
        return NULL;
    }

    int frame_len = 640;
    std::ifstream audio(param->audio_file.c_str(),
                        std::ios::binary | std::ios::in);
    if (!audio.is_open()) {
        std::cout << "open audio file error " << param->audio_file << std::endl;
        recognizer->Stop();
        delete recognizer;
        return NULL;
    }

    char *frame = reinterpret_cast<char *>(malloc(frame_len));
    if (frame == NULL) {
        std::cout << "malloc frame error " << std::endl;
        recognizer->Stop();
        delete recognizer;
        return NULL;
    }

    while (!audio.eof()) {
        audio.read(frame, frame_len);
        recognizer->Write(frame, frame_len);
        // 发送语音频率控制，模拟实时语音
        msleep(20);
    }
    audio.close();

    recognizer->Stop();
    free(frame);
    delete recognizer;
    return NULL;
}

// 单次识别
void process_once() {
    process_param_t param;
    param.audio_file = "test.wav";
    process(reinterpret_cast<void *>(&param));
}

// 每个识别启动一个线程
void process_multi() {
    char audio_file_name[AUDIO_FILE_NUMS][AUDIO_FILE_NAME_LENGTH] = {
        "test.wav", "test.wav"
    };
    process_param_t params[AUDIO_FILE_NUMS];
    pthread_t pthread_ids[AUDIO_FILE_NUMS];
    for (int i = 0; i < AUDIO_FILE_NUMS; i++) {
        params[i].audio_file = std::string(audio_file_name[i]);
        pthread_create(&pthread_ids[i], NULL, process,
                       reinterpret_cast<void *>(&params[i]));
    }
    for (int i = 0; i < AUDIO_FILE_NUMS; i++) {
        pthread_join(pthread_ids[i], NULL);
    }
}
int main() { process_multi(); }
