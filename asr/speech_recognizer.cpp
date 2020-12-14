
// Copyright 1998-2020 Tencent Copyright
#include "speech_recognizer.h"
#include <stddef.h>
#include "rapidjson/document.h"
#include "tcloud_util.h"
#include "websocket_handler.h"


#define URL_MAX_LENGTH 1024
typedef websocketpp::client<websocketpp::config::asio_client> client;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

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

        m_endpoint.init_asio();
        m_endpoint.set_message_handler(
            bind(&OnMessage, &m_endpoint, ::_1, ::_2));
        m_endpoint.set_open_handler(bind(&OnOpen, &m_endpoint, _1));
        m_endpoint.set_close_handler(bind(&OnClose, &m_endpoint, _1));
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

    void SendText(const char *payload, size_t len) {
        m_con->send(payload, len, websocketpp::frame::opcode::text);
    }

    void SendBinary(void *payload, size_t len) {
        m_con->send(payload, len, websocketpp::frame::opcode::binary);
    }

    void Terminate() {
        m_endpoint.stop_perpetual();
        m_thread->join();
    }

    client m_endpoint;
    std::string m_url;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

    void *m_parent_ptr;
    std::string m_voice_id;
    client::connection_ptr m_con;
};

void OnOpen(client *c, websocketpp::connection_hdl hdl) {
    SpeechListener *listener = my_container_of(c, SpeechListener, m_endpoint);
    SpeechRecognizer *recognizer =
        reinterpret_cast<SpeechRecognizer *>(listener->m_parent_ptr);
    SpeechRecognitionResponse rsp;
    rsp.voice_id = recognizer->m_config.voice_id;
    recognizer->on_recognition_start(&rsp);
}

void OnClose(client *c, websocketpp::connection_hdl hdl) {}

SpeechRecognitionResponse *decode_response(std::string message) {
    rapidjson::Document doc;
    doc.Parse(message.c_str());
    SpeechRecognitionResponse *rsp = new SpeechRecognitionResponse;
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
    if (doc.HasMember("voice_id") && doc["voice_id"].IsString()) {
        rsp->voice_id = doc["voice_id"].GetString();
    }
    if (doc.HasMember("message_id") && doc["message_id"].IsInt()) {
        rsp->message_id = doc["message_id"].GetString();
    }
    if (doc.HasMember("final") && doc["final"].IsInt()) {
        rsp->final_rsp = doc["final"].GetUint();
    }
    if (doc.HasMember("result") && doc["result"].IsObject()) {
        for (rapidjson::Value::ConstMemberIterator itr =
                 doc["result"].MemberBegin();
             itr != doc["result"].MemberEnd(); ++itr) {
            if (itr->name.GetString() == std::string("slice_type")) {
                rsp->result.slice_type = itr->value.GetUint();
            }
            if (itr->name.GetString() == std::string("index")) {
                rsp->result.index = itr->value.GetInt();
            }
            if (itr->name.GetString() == std::string("start_time")) {
                rsp->result.start_time = itr->value.GetUint();
            }
            if (itr->name.GetString() == std::string("end_time")) {
                rsp->result.end_time = itr->value.GetUint();
            }
            if (itr->name.GetString() == std::string("voice_text_str")) {
                rsp->result.voice_text_str = itr->value.GetString();
            }
            if (itr->name.GetString() == std::string("word_size")) {
                rsp->result.word_size = itr->value.GetUint();
            }
            if (itr->name.GetString() == std::string("word_size")) {
                if (itr->value.IsArray()) {
                    for (rapidjson::Value::ConstValueIterator itr_arr =
                             itr->value.Begin();
                         itr_arr != itr->value.End(); ++itr) {
                        if (itr_arr->IsObject()) {
                            ResultWord result_word;
                            for (rapidjson::Value::ConstMemberIterator
                                     itr_word = itr_arr->MemberBegin();
                                 itr_word != itr_arr->MemberEnd(); ++itr) {
                                if (itr_word->name.GetString() ==
                                    std::string("start_time")) {
                                    result_word.start_time =
                                        itr_word->value.GetUint();
                                }
                                if (itr_word->name.GetString() ==
                                    std::string("end_time")) {
                                    result_word.end_time =
                                        itr_word->value.GetUint();
                                }
                                if (itr_word->name.GetString() ==
                                    std::string("stable_flag")) {
                                    result_word.stable_flag =
                                        itr_word->value.GetUint();
                                }
                                if (itr_word->name.GetString() ==
                                    std::string("word")) {
                                    result_word.word =
                                        itr_word->value.GetString();
                                }
                            }
                            rsp->result.word_list.push_back(result_word);
                        }
                    }
                }
            }
        }
    }
    return rsp;
}

void OnMessage(client *c, websocketpp::connection_hdl hdl, message_ptr msg) {
    std::string payload = msg->get_payload();
    SpeechListener *listener = my_container_of(c, SpeechListener, m_endpoint);
    SpeechRecognizer *recognizer =
        reinterpret_cast<SpeechRecognizer *>(listener->m_parent_ptr);

    SpeechRecognitionResponse *rsp = decode_response(payload);
    if (!rsp) {
        return;
    }
    if (rsp->code != 0) {
        if (rsp->voice_id.length() == 0) {
            rsp->voice_id = recognizer->m_config.voice_id;
        }
        recognizer->on_task_failed(rsp);
        delete rsp;
        return;
    }

    if (rsp->final_rsp == 1) {
        recognizer->on_recognition_complete(rsp);
        delete rsp;
        return;
    }

    if (rsp->result.slice_type == 0) {
        if (rsp->voice_id != "") {
            recognizer->on_sentence_begin(rsp);
        }
    } else if (rsp->result.slice_type == 2) {
        recognizer->on_sentence_end(rsp);
    } else if (rsp->result.slice_type == 1) {
        recognizer->on_recognition_result_changed(rsp);
    }

    delete rsp;
}

SpeechRecognizer::SpeechRecognizer(std::string appid, std::string secret_id,
                                   std::string secret_key) {
    InitSpeechRecognizerConfig(appid, secret_id, secret_key);
}
SpeechRecognizer::~SpeechRecognizer() {}

void SpeechRecognizer::Start() {
    m_listener = new SpeechListener(this, GetWebsocketURL());
    m_listener->Start();
}

void SpeechRecognizer::Stop() {
    std::string end_str = "{\"type\":\"end\"}";
    m_listener->SendText(end_str.c_str(), end_str.length());
    m_listener->Terminate();
    delete m_listener;
}

void SpeechRecognizer::Write(void *payload, size_t len) {
    m_listener->SendBinary(payload, len);
}

void SpeechRecognizer::SetOnFail(ASRCallBackFunc event) {
    on_task_failed = event;
}
void SpeechRecognizer::SetOnRecognitionStart(ASRCallBackFunc event) {
    on_recognition_start = event;
}

void SpeechRecognizer::SetOnRecognitionComplete(ASRCallBackFunc event) {
    on_recognition_complete = event;
}

void SpeechRecognizer::SetOnRecognitionResultChanged(ASRCallBackFunc event) {
    on_recognition_result_changed = event;
}

void SpeechRecognizer::SetOnSentenceBegin(ASRCallBackFunc event) {
    on_sentence_begin = event;
}

void SpeechRecognizer::SetOnSentenceEnd(ASRCallBackFunc event) {
    on_sentence_end = event;
}

void SpeechRecognizer::SetVoiceId(std::string voice_id) {
    m_config.voice_id = voice_id;
}

void SpeechRecognizer::SetNonce(uint64_t nonce) {
    m_config.nonce = std::to_string(nonce);
}

void SpeechRecognizer::SetEngineModelType(std::string engine_model_type) {
    m_config.engine_model_type = engine_model_type;
}

void SpeechRecognizer::SetVoiceFormat(int voice_format) {
    m_config.voice_format = voice_format;
}

void SpeechRecognizer::SetNeedVad(int need_vad) {
    m_config.need_vad = std::to_string(need_vad);
}

void SpeechRecognizer::SetHotwordId(std::string hotword_id) {
    m_config.hotword_id = hotword_id;
}

void SpeechRecognizer::SetFilterDirty(int filter_dirty) {
    m_config.filter_dirty = std::to_string(filter_dirty);
}

void SpeechRecognizer::SetFilterPunc(int filter_punc) {
    m_config.filter_punc = std::to_string(filter_punc);
}

void SpeechRecognizer::SetFilterModal(int filter_modal) {
    m_config.filter_model = std::to_string(filter_modal);
}
void SpeechRecognizer::SetConvertNumMode(int convert_num_mode) {
    m_config.convert_num_mode = std::to_string(convert_num_mode);
}

void SpeechRecognizer::SetWordInfo(int word_info) {
    m_config.word_info = std::to_string(word_info);
}

void SpeechRecognizer::SetVadSilenceTime(int vad_silence_time) {
    m_config.vad_silence_time = std::to_string(vad_silence_time);
}

void SpeechRecognizer::InitSpeechRecognizerConfig(std::string appid,
                                                  std::string secret_id,
                                                  std::string secret_key) {
    m_config.secret_id = secret_id;
    m_config.appid = appid;
    m_config.secret_key = secret_key;
    m_config.engine_model_type = "16k_zh";
    m_config.voice_format = "1";
    m_config.need_vad = "0";
    m_config.filter_dirty = "0";
    m_config.filter_model = "0";
    m_config.filter_punc = "0";
    m_config.convert_num_mode = "0";
    m_config.word_info = "0";
    m_config.hotword_id = "";
    m_config.nonce = "";
    char str[32] = { 0 };
    memset(str, 0, 32);
    srand(time(0) + TCloudUtil::gettid());  // 默认根据线程号生成voice_id
    snprintf(str, sizeof(str), "%d", rand());
    m_config.voice_id = str;
    m_config.vad_silence_time = "";
}

void SpeechRecognizer::BuildRequest() {
    m_builder.SetKeyValue("secret_key", m_config.secret_key);
    m_builder.SetKeyValue("secretid", m_config.secret_id);
    m_builder.SetKeyValue("appid", m_config.appid);
    m_builder.SetKeyValue("engine_model_type", m_config.engine_model_type);
    m_builder.SetKeyValue("voice_format", m_config.voice_format);
    m_builder.SetKeyValue("filter_dirty", m_config.filter_dirty);
    m_builder.SetKeyValue("filter_modal", m_config.filter_model);
    m_builder.SetKeyValue("filter_punc", m_config.filter_punc);
    m_builder.SetKeyValue("convert_num_mode", m_config.convert_num_mode);
    m_builder.SetKeyValue("word_info", m_config.word_info);
    // 如果音频大小超过60s，需要设置needvad为1
    m_builder.SetKeyValue("needvad", m_config.need_vad);
    if (m_config.hotword_id.length() > 0) {
        m_builder.SetKeyValue("hotword_id", m_config.hotword_id);
    }
    if (m_config.nonce.length() > 0) {
        m_builder.SetKeyValue("nonce", m_config.nonce);
    }
    if (m_config.vad_silence_time.length() > 0) {
        m_builder.SetKeyValue("vad_silence_time", m_config.vad_silence_time);
    }
    m_builder.SetKeyValue("voice_id", m_config.voice_id);
}

std::string SpeechRecognizer::GetRequestURL() {
    std::string requestURL;
    std::string urlFormat = REALTIME_ASR_URL_FORMAT;

    char *pUrlBuffer = new char[URL_MAX_LENGTH];
    memset(pUrlBuffer, 0, URL_MAX_LENGTH);
    BuildRequest();
    snprintf(pUrlBuffer, URL_MAX_LENGTH - 1, urlFormat.c_str(),
             m_config.appid.c_str(), m_builder.GetURLParamString(true).c_str(),
             m_builder.GetAuthorization().c_str());
    requestURL = pUrlBuffer;
    delete[] pUrlBuffer;
    pUrlBuffer = NULL;

    return requestURL;
}

std::string SpeechRecognizer::GetWebsocketURL() {
    char str[32] = { 0 };
    memset(str, 0, 32);

    snprintf(str, sizeof(str), "%d", (uint32_t)time(NULL));
    m_builder.SetKeyValue("timestamp", str);
    if (m_config.nonce.length() == 0) {
        m_builder.SetKeyValue("nonce", str);
    }
    memset(str, 0, 32);
    snprintf(str, sizeof(str), "%d", (uint32_t)time(NULL) + 6000);
    m_builder.SetKeyValue("expired", str);
    m_builder.SetKeyValue("timeout", "5000");
    return GetRequestURL();
}
