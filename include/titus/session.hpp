#pragma once
#include "titus/process.hpp"
#include <chrono>
namespace titus {
Json blues_features(bool dual);
std::string runtime_engine(const Json& request);
Json runtime_features(const std::string& game,bool dual);
int runtime_slot_count(const Json& request);
void validate_runtime_event(const Json& event,const Json& request);
void validate_runtime_result(const Json& result,const Json& request);
void validate_runtime_inspection(const Json& result,const Json& request);
void validate_engine_request(const Json& request,const fs::path& request_file);
struct LegacySources {fs::path preferences,saves;};
struct SessionOptions {LegacySources legacy;int test_ms=0;bool test_gameplay=false;std::string test_fault;bool inspect_only=false;Json primary_source=nullptr;};
class Session {
    Paths paths_;fs::path executable_,directory_,profile_,staging_;
    std::unique_ptr<ProfileLock> lock_;
    std::unique_ptr<Process> process_;
    Json request_,events_=Json::array(),inspection_,result_;
    SessionOptions options_;
    std::string partial_,failure_,message_;
    size_t offset_=0;int command_seq_=0;
    bool ready_=false,closing_=false,terminal_error_=false,confirmation_=false,cancel_=false;
    std::chrono::steady_clock::time_point last_event_,started_;
    void start_inspection(bool prepare=false);
    bool prepared_=false;
    void consume_events();void collect();void migrate();
public:
    enum class State {Inspecting,Preparing,Starting,Playing,Returning,Done,Failed};
    State state=State::Inspecting;
    Session(Paths paths,const fs::path& package_root,const Json& catalog,const std::string& installation_id,SessionOptions options={});
    ~Session();
    void poll();void request_return();void force_stop();
    bool ready()const{return ready_;}
    bool finished()const{return state==State::Done||state==State::Failed;}
    bool inspecting()const{return state==State::Inspecting;}
    bool stalled()const;
    const std::string& message()const{return message_;}
    const std::string& error()const{return failure_;}
    const fs::path& directory()const{return directory_;}
    const Json& request()const{return request_;}
};
}
