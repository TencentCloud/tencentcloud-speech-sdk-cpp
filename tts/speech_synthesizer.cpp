
// Copyright 1998-2020 Tencent Copyright
#include "speech_synthesizer.h"
#include <stddef.h>
#include "rapidjson/document.h"
#include "tcloud_util.h"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

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


#define URL_MAX_LENGTH 65536 //1024
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

template <class P, class M> size_t my_offsetof(const M P::*member) {
    return (size_t) & (reinterpret_cast<P *>(0)->*member);
}

template <class P, class M>
P *my_container_of_impl(M *ptr, const M P::*member) {
    return reinterpret_cast<P *>(reinterpret_cast<char *>(ptr) -
                                 my_offsetof(member));
}

#define my_container_of(ptr, type, member)                                     \
    my_container_of_impl(ptr, &type::member)

void OnOpen(client *c, websocketpp::connection_hdl hdl);
void OnMessage(client *c, websocketpp::connection_hdl hdl, message_ptr msg);
void OnClose(client *c, websocketpp::connection_hdl hdl);
void OnFail(client *c, websocketpp::connection_hdl hdl);
context_ptr OnTlsInit(const char * hostname, websocketpp::connection_hdl);

class SpeechListener {
 public:
    SpeechListener(void *ptr, std::string url) {
        m_url = url;
        m_parent_ptr = ptr;
    }

    ~SpeechListener() { m_parent_ptr = NULL; }

    void Start() {
        m_endpoint.set_error_channels(websocketpp::log::elevel::all);
        m_endpoint.set_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::rerror);
        m_endpoint.clear_access_channels(
            websocketpp::log::alevel::frame_payload);
        m_endpoint.clear_access_channels(
            websocketpp::log::alevel::frame_header);
        //  如果想显示websocket的连接信息可以注释下面两行
        m_endpoint.clear_access_channels(websocketpp::log::alevel::connect);
        m_endpoint.clear_access_channels(websocketpp::log::alevel::disconnect);
        m_endpoint.clear_access_channels(websocketpp::log::alevel::control);

        m_endpoint.init_asio();
        m_endpoint.set_message_handler(
            bind(&OnMessage, &m_endpoint, ::_1, ::_2));
        m_endpoint.set_open_handler(bind(&OnOpen, &m_endpoint, _1));
        m_endpoint.set_close_handler(bind(&OnClose, &m_endpoint, _1));
        m_endpoint.set_fail_handler(bind(&OnFail, &m_endpoint, _1));
        std::string hostname = REALTIME_TTS_HOSTNAME;
        m_endpoint.set_tls_init_handler(bind(&OnTlsInit, hostname.c_str(), ::_1));
        websocketpp::lib::error_code ec;
        m_con = m_endpoint.get_connection(m_url, ec);
        m_con->add_subprotocol("janus-protocol");
        if (ec) {
            std::cout << "could not create connection because: " << ec.message()
                      << std::endl;
            return;
        }
        auto hdl = m_con->get_handle();
        m_endpoint.connect(m_con);
        m_endpoint.start_perpetual();
        m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endpoint));
    }

    int GetState() { return m_con->get_state(); }

    void Stop(std::string reason) {
        m_endpoint.stop_perpetual();
        m_endpoint.close(m_con->get_handle(), websocketpp::close::status::normal, "");
        std::cout << "client close after " << reason << std::endl;
    }

    client m_endpoint;
    std::string m_url;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

    void *m_parent_ptr;
    client::connection_ptr m_con;
};

void OnOpen(client *c, websocketpp::connection_hdl hdl) {
    std::cout << "====OnOpen=====" << std::endl;
    SpeechListener *listener = my_container_of(c, SpeechListener, m_endpoint);
    SpeechSynthesizer *synthesizer =
        reinterpret_cast<SpeechSynthesizer *>(listener->m_parent_ptr);
    SpeechSynthesisResponse rsp;
    rsp.session_id = synthesizer->m_config.session_id;
    synthesizer->SetReady();
    synthesizer->on_synthesis_start(&rsp);
}

context_ptr OnTlsInit(const char * hostname, websocketpp::connection_hdl){
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
    return ctx;
}

void OnClose(client *c, websocketpp::connection_hdl hdl) {
    // [disconnect] Disconnect close local:[1006,End of File] remote:[1006]
    std::cout << "====OnClose=====" << std::endl;
}

void OnFail(client *c, websocketpp::connection_hdl hdl) { // 连接错误
    std::cout << "====OnFail=====" << std::endl;
    SpeechListener *listener = my_container_of(c, SpeechListener, m_endpoint);
    SpeechSynthesizer *synthesizer =
        reinterpret_cast<SpeechSynthesizer *>(listener->m_parent_ptr);
    synthesizer->SetFailed();
    synthesizer->Stop("recv conn fail");
}

SpeechSynthesisResponse *decode_response(std::string message) {
    rapidjson::Document doc;
    doc.Parse(message.c_str());
    SpeechSynthesisResponse *rsp = new SpeechSynthesisResponse;
    if (rsp == NULL) {
        std::cout << "new SpeechRecognizerResponse failed" << std::endl;
        return NULL;
    }
    if (doc.HasMember("code") && doc["code"].IsInt()) {
        rsp->code = doc["code"].GetInt();
    }
    if (doc.HasMember("message") && doc["message"].IsString()) {
        rsp->message = doc["message"].GetString();
    }
    if (doc.HasMember("session_id") && doc["session_id"].IsString()) {
        rsp->session_id = doc["session_id"].GetString();
    }
    if (doc.HasMember("message_id") && doc["message_id"].IsString()) {
        rsp->message_id = doc["message_id"].GetString();
    }
    if (doc.HasMember("request_id") && doc["request_id"].IsString()) {
        rsp->request_id = doc["request_id"].GetString();
    }
    if (doc.HasMember("final") && doc["final"].IsInt()) {
        rsp->final = doc["final"].GetUint();
    }
    if (doc.HasMember("result") && doc["result"].IsObject()) {
        for (rapidjson::Value::ConstMemberIterator itr = doc["result"].MemberBegin();
            itr != doc["result"].MemberEnd(); ++itr) {
            if (itr->name.GetString() == std::string("subtitles")) {
                if (itr->value.IsArray()) {
                    for (rapidjson::Value::ConstValueIterator itr_arr = itr->value.Begin();
                         itr_arr != itr->value.End(); ++itr_arr) {
                        if (itr_arr->IsObject()) {
                            Subtitle subtitle;
                            for (rapidjson::Value::ConstMemberIterator
                                itr_sub = itr_arr->MemberBegin();
                                itr_sub != itr_arr->MemberEnd(); ++itr_sub) {
                                if (itr_sub->name.GetString() == std::string("BeginTime")) {
                                    subtitle.begin_time = itr_sub->value.GetUint();
                                }
                                if (itr_sub->name.GetString() == std::string("EndTime")) {
                                    subtitle.end_time = itr_sub->value.GetUint();
                                }
                                if (itr_sub->name.GetString() == std::string("BeginIndex")) {
                                    subtitle.begin_index = itr_sub->value.GetUint();
                                }
                                if (itr_sub->name.GetString() == std::string("EndIndex")) {
                                    subtitle.end_index = itr_sub->value.GetUint();
                                }
                                if (itr_sub->name.GetString() == std::string("Text")) {
                                    subtitle.text = itr_sub->value.GetString();
                                }
                                if (itr_sub->name.GetString() == std::string("Phoneme")) {
                                    subtitle.phoneme = itr_sub->value.GetString();
                                }
                            }
                            rsp->result.subtitles.push_back(subtitle);
                        }
                    }
                }
            }
        }
    }
    return rsp;
}

void DispatchBinaryMessage(SpeechSynthesizer *synthesizer, std::string payload) {
    SpeechSynthesisResponse *rsp = new SpeechSynthesisResponse;
    rsp->session_id = synthesizer->m_session_id;
    rsp->data = payload;
    synthesizer->on_audio_result(rsp);
    
    if (rsp != NULL) delete rsp;
}


void DispatchTextMessage(SpeechSynthesizer *synthesizer, std::string payload) {
    SpeechSynthesisResponse *rsp = decode_response(payload);
    if (!rsp) {
        return;
    }
    if (rsp->code != 0) { // 业务错误
        synthesizer->Stop("recv tts fail");
        synthesizer->on_synthesis_fail(rsp);
    } else if (rsp->final == 1) {
        synthesizer->Stop("recv final");
        synthesizer->on_synthesis_end(rsp);
    } else {
        synthesizer->on_text_result(rsp);
    }

    if (rsp != NULL) delete rsp;
}

void OnMessage(client *c, websocketpp::connection_hdl hdl, message_ptr msg) {
    SpeechListener *listener = my_container_of(c, SpeechListener, m_endpoint);
    SpeechSynthesizer *synthesizer =
        reinterpret_cast<SpeechSynthesizer *>(listener->m_parent_ptr);

    if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
        //std::cout << "[DEBUG] recv binary frame, data_size=" << msg->get_payload().length() << std::endl;
        DispatchBinaryMessage(synthesizer, msg->get_payload());
    } else if (msg->get_opcode() == websocketpp::frame::opcode::text) {
        std::cout << "[DEBUG] recv text frame, text=" << msg->get_payload() << std::endl;
        DispatchTextMessage(synthesizer, msg->get_payload());
    } else {
        std::cout << "invalid opcode " << msg->get_opcode();
    }
}

SpeechSynthesizer::SpeechSynthesizer(std::string appid, std::string secret_id,
                                   std::string secret_key, std::string session_id) {
    InitSpeechRecognizerConfig(appid, secret_id, secret_key, session_id);
    m_session_id = session_id;
    ready_ = false;
    failed_ = false;
}
SpeechSynthesizer::~SpeechSynthesizer() {}

int SpeechSynthesizer::Start() {
    m_listener = new SpeechListener(this, GetWebsocketURL());
    m_listener->Start();
    int cnt = 0;
    while(!ready_) {
        if (failed_) {
            return -1;
        }
        msleep(50);
        cnt += 1;
        //waiting 1 second
        if (cnt >= 20) {
            return -2;
        }
    }
    return 0;
}

void SpeechSynthesizer::Wait() {
    m_listener->m_thread->join();
}

void SpeechSynthesizer::Stop(std::string reason) {
    m_listener->Stop(reason);
}

void SpeechSynthesizer::SetFailed() {
    failed_ = true;
}

void SpeechSynthesizer::SetReady() {
    ready_ = true;
}

void SpeechSynthesizer::SetOnSynthesisFail(TTSCallBackFunc event) {
    on_synthesis_fail = event;
}
void SpeechSynthesizer::SetOnSynthesisStart(TTSCallBackFunc event) {
    on_synthesis_start = event;
}

void SpeechSynthesizer::SetOnSynthesisEnd(TTSCallBackFunc event) {
    on_synthesis_end = event;
}

void SpeechSynthesizer::SetOnTextResult(TTSCallBackFunc event) {
    on_text_result = event;
}

void SpeechSynthesizer::SetOnAudioResult(TTSCallBackFunc event) {
    on_audio_result = event;
}

void SpeechSynthesizer::SetModelType(std::string model_type) {
    m_config.model_type = model_type;
}

void SpeechSynthesizer::SetVoiceType(uint64_t voice_type) {
    m_config.voice_type = std::to_string(voice_type);
}

void SpeechSynthesizer::SetCodec(std::string codec) {
    m_config.codec = codec;
}

void SpeechSynthesizer::SetSampleRate(int sample_rate) {
    m_config.sample_rate = std::to_string(sample_rate);
}

void SpeechSynthesizer::SetSpeed(float speed) {
    std::ostringstream oss;
    oss << std::setprecision(2) << speed;
    m_config.speed = oss.str();
}

void SpeechSynthesizer::SetVolume(float volume) {
    std::ostringstream oss;
    oss << std::setprecision(2) << volume;
    m_config.volume = oss.str();
}

void SpeechSynthesizer::SetText(std::string text) {
    m_config.text = text;
}

void SpeechSynthesizer::SetEnableSubtitle(bool enable_subtitle) {
    if (enable_subtitle)
        m_config.enable_subtitle = "true";
    else
        m_config.enable_subtitle = "false";
}

void SpeechSynthesizer::InitSpeechRecognizerConfig(std::string appid,
                                                  std::string secret_id,
                                                  std::string secret_key,
                                                  std::string session_id) {
    m_config.appid = appid;
    m_config.secret_id = secret_id;
    m_config.secret_key = secret_key;
    m_config.session_id = session_id;

    m_config.action = REALTIME_TTS_ACTION;
    m_config.model_type = "1";
    m_config.voice_type = "101000";
    m_config.codec = "mp3";
    m_config.sample_rate = "16000";
    m_config.speed = "0";
    m_config.volume = "0";
    m_config.text = "欢迎使用腾讯云语音合成";
    m_config.enable_subtitle = "false";
}

void SpeechSynthesizer::BuildRequest() {
    m_builder.SetKeyValue("Action", m_config.action);
    m_builder.SetKeyValue("secret_key", m_config.secret_key);
    m_builder.SetKeyValue("SecretId", m_config.secret_id);
    m_builder.SetKeyValue("AppId", m_config.appid);
    m_builder.SetKeyValue("ModelType", m_config.model_type);
    m_builder.SetKeyValue("VoiceType", m_config.voice_type);
    m_builder.SetKeyValue("Codec", m_config.codec);
    m_builder.SetKeyValue("SampleRate", m_config.sample_rate);
    m_builder.SetKeyValue("Speed", m_config.speed);
    m_builder.SetKeyValue("Volume", m_config.volume);
    m_builder.SetKeyValue("SessionId", m_config.session_id);
    m_builder.SetKeyValue("Text", m_config.text);
    m_builder.SetKeyValue("EnableSubtitle", m_config.enable_subtitle);
}

std::string SpeechSynthesizer::GetRequestURL() {
    std::string requestURL;
    std::string urlFormat = REALTIME_TTS_URL_FORMAT;

    char *pUrlBuffer = new char[URL_MAX_LENGTH];
    memset(pUrlBuffer, 0, URL_MAX_LENGTH);
    BuildRequest();
    snprintf(pUrlBuffer, URL_MAX_LENGTH - 1, urlFormat.c_str(),
             m_builder.GetURLParamString(true).c_str(),
             m_builder.GetAuthorization().c_str());
    requestURL = pUrlBuffer;
    delete[] pUrlBuffer;
    pUrlBuffer = NULL;

    std::cout << "request url=" << requestURL << std::endl;
    return requestURL;
}

std::string SpeechSynthesizer::GetWebsocketURL() {
    char str[32] = { 0 };
    memset(str, 0, 32);

    snprintf(str, sizeof(str), "%d", (uint32_t)time(NULL));
    m_builder.SetKeyValue("Timestamp", str);
    memset(str, 0, 32);
    snprintf(str, sizeof(str), "%d", (uint32_t)time(NULL) + 6000);
    m_builder.SetKeyValue("Expired", str);
    return GetRequestURL();
}
