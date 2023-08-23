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
#include "speech_synthesizer.h"
#include "tcloud_util.h"

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

//输入从官网申请的账号appid/secret_id/secret_key
std::string appid = "";
std::string secret_id = "";
std::string secret_key = "";

std::string audio_file_name = "tts_output_audio.mp3";
std::string audio_file_data;

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

void OnSynthesisStart(SpeechSynthesisResponse *rsp) {
    std::cout << gettime() << "| OnSynthesisStart | " << rsp->session_id
            << std::endl;
}
// 识别失败回调
void OnSynthesisFail(SpeechSynthesisResponse *rsp) {
    std::cout << gettime() << "| OnSynthesisFail | " << rsp->session_id << " | "
            << " code=" << rsp->code << " msg=" << rsp->message << std::endl;
}

// 文本结果回调
void OnTextResult(SpeechSynthesisResponse *rsp) {
    // 处理文本结果
    std::string session_id = rsp->session_id;
    std::string message_id = rsp->message_id;
    std::string request_id = rsp->request_id;
    std::vector<Subtitle> subtitles = rsp->result.subtitles;

    std::cout << gettime() << "| OnTextResult | " << rsp->session_id << " | "
        << " message_id=" << rsp->message_id << " request_id=" << rsp->request_id
        << " result=" << std::endl;
    for (std::vector<Subtitle>::iterator it=subtitles.begin(); it != subtitles.end(); it++) {
        std::string text = it->text;
#ifdef _WIN32
        text = utf8_to_gbk(text)
#endif
        std::cout << it->begin_index << "|" << it->end_index << "|"
                  << it->begin_time << "|" << it->end_time << "|"
                  << text << "|" << it->phoneme << std::endl;
    }
}

// 音频结果回调
void OnAudioResult(SpeechSynthesisResponse *rsp) {
    std::string audio_data = rsp->data;
    // 处理音频二进制数据
    std::cout << gettime() << "| OnAudioResult | " << rsp->session_id << " | "
        << " audio_len=" << audio_data.length() << std::endl;
    audio_file_data += audio_data;
}

// 识别完成回调
void OnSynthesisEnd(SpeechSynthesisResponse *rsp) {
    std::cout << gettime() << "| OnSynthesisEnd | " << rsp->session_id
              << std::endl;

    std::ofstream outfile(audio_file_name, std::ifstream::binary);
    outfile.write(audio_file_data.c_str(), audio_file_data.length());
    outfile.close();
    std::cout << gettime() << " write audio " << audio_file_name << std::endl;
}

void *process(void *arg) {
    //gen unique session_id
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()(); 
    std::string session_id_str = boost::uuids::to_string(a_uuid);

    SpeechSynthesizer *synthesizer =
        new SpeechSynthesizer(appid, secret_id, secret_key, session_id_str);
    synthesizer->SetOnSynthesisStart(OnSynthesisStart);
    synthesizer->SetOnSynthesisFail(OnSynthesisFail);
    synthesizer->SetOnSynthesisEnd(OnSynthesisEnd);
    synthesizer->SetOnTextResult(OnTextResult);
    synthesizer->SetOnAudioResult(OnAudioResult);

    synthesizer->SetVoiceType(101001);
    synthesizer->SetCodec("mp3");
    synthesizer->SetSampleRate(16000);
    synthesizer->SetSpeed(0);
    synthesizer->SetVolume(0);
    synthesizer->SetText("欢迎使用腾讯云语音合成，可以满足将文本转化成拟人化语音的需求，打通人机交互闭环。提供多种音色选择，支持自定义音量、语速，让发音更自然、更专业、更符合场景需求。语音合成广泛应用于语音导航、有声读物、机器人、语音助手、自动新闻播报等场景，提升人机交互体验，提高语音类应用构建效率。");
    synthesizer->SetEnableSubtitle(true);
    int ret = synthesizer->Start();
    if (ret < 0) {
        std::cout << " synthesizer start failed \n" << std::endl;
        delete synthesizer;
        return NULL;
    }
    synthesizer->Wait();

    delete synthesizer;
    return NULL;
}

// 单次合成
void process_once() {
    process(NULL);
}

// 每个合成启动一个线程
void process_multi() {
    #define THREAD_NUM 1
    pthread_t pthread_ids[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_create(&pthread_ids[i], NULL, process, NULL);
    }
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(pthread_ids[i], NULL);
    }
}
int main() { process_multi(); }
