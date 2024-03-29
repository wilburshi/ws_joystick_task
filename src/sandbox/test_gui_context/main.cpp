#include "common/app.hpp"
#include "common/lever_gui.hpp"
#include "common/juice_pump_gui.hpp"
#include "common/lever_pull.hpp"
#include "common/common.hpp"
#include "common/juice_pump.hpp"
#include "common/random.hpp"
#include "training.hpp"
#include "nlohmann/json.hpp"
#include <imgui.h>

#ifdef _MSC_VER
#include <Windows.h>
#endif

#include <time.h>
#include <thread>
#include <iostream>
#include <fstream>

using json = nlohmann::json;

struct App;

void render_gui(App& app);
void task_update(App& app);
void setup(App& app);
void shutdown(App& app);

struct TrialRecord {
    double trial_start_time_stamp;  // time since the session starts
    int trial_number;
    int first_pull_id;
    int rewarded;
    int task_type; // indicate the task type 
};

struct BehaviorData {
    int trial_number;
    double time_points;
    int behavior_events;
};

struct SessionInfo {
    std::string animal1_name;
    std::string animal2_name;
    std::string experiment_date;
    int task_type;
    int task_type_block;
    int task_type_random;
    int task_type_blocklength;
    float large_juice_volume;
    float small_juice_volume;

};

struct LeverReadout {
    int trial_number;
    double readout_timepoint;
    float strain_gauge_lever;
    float potentiometer_lever;
    int pull_or_release;
    int lever_id;
}; // under construction ... -WS


struct App : public ws::App {
    ~App() override = default;
    void setup() override {
        ::setup(*this);
    }
    void gui_update() override {
        render_gui(*this);
    }
    void task_update() override {
        ::task_update(*this);
    }
    void shutdown() override {
        ::shutdown(*this);
    }

    // Variable initiation
    // Some of these variable can be changed accordingly for each session. - Weikang

    // file name
    std::string animal1_name{ "Vermelho" };
    std::string animal2_name{ "Koala" };

    std::string experiment_date{ "20230802" };

    int tasktype_block{ 1 }; // 1: if the tasktype changes as a block by block style
    int tasktype_random{ 0 }; // 1: if the task type changes trial by trial randomly
    
    int block_length{ 15 }; // initiation: the length of the (mini)block, only function if "tasktype_block" or "tasktype_random" is 1

    int tasktype{ 2 }; // initiation; 1:competing 2: delimma 
    // int tasktype{rand()%2};

    // lever force setting condition
    // bool allow_auto_lever_force_set{true}; // true, if use force as below; false, if manually select force level on the GUI. - WS
    // float normalforce{ 100.0f }; // 130
    // float releaseforce{ 350.0f }; // 350

    bool allow_automated_juice_delivery{ false };

    // int lever_force_limits[2]{0, 550};
    ws::lever::PullDetect detect_pull[2]{}; // one lever, but treat the two directions as two levers
    float lever_position_limits[4]{ 27.7e3f, 32.2e3f, 32.2e3f, 36.7e3f };  // one lever, but treat the two directions as two levers
    bool invert_lever_position[2]{ true, false }; // one lever, but treat the two directions as two levers


    //float new_delay_time{2.0f};
    double new_delay_time{ ws::urand() * 4 + 3 }; //random delay between 3 to 5 s (in unit of second)
    int juice1_delay_time{ 500 }; // from successful pulling to juice1 delivery (in unit of minisecond)
    int juice2_delay_time{1500 }; // from juice1 delivery to juice2 delivery (in unit of minisecond) - 1500 for task 1; 750 for task 2
    float pull_to_deliver_total_time{ 5000.0f }; // unit of ms, the total time from one animal pulls to the delivery of both juices at least 3250ms ~ 500ms animal delay time + 500ms juice 1 delay time + 750ms small juice delivery + 1500ms large juice delivery
    int after_delivery_time{ 5500 }; // from the juice2 delivery to the end of the trial (in unit of minisecond)

    // reward amount
    float large_juice_volume{ 0.150f };
    float small_juice_volume{ 0.020f };

    // session threshold
    float new_total_time{ 3600.0f }; // the time for the session (in unit of second)
    // float new_total_time{ 15.0f }; // the time for the maximal trial time - only used for mutual cooperation condition (task type == 3)
    int total_trial_number{ 500 }; // the maximal trial number of a session


    // variables that are updated every trial
    int trialnumber{ 0 };
    int first_pull_id{ 0  };
    bool getreward[2]{ false, false };
    int rewarded[2]{ 0, 0 };
    ws::TimePoint first_pull_time{};
    double other_pull_time{}; // time gap bwtween two pulls


    double timepoint{};
    ws::TimePoint trialstart_time;
    double trial_start_time_forsave;
    ws::TimePoint session_start_time;
    int behavior_event{}; // 0 - trial starts; 9 - trial ends; 1 - lever 1 is pulled; 2 - lever 2 is pulled; 3 - pump 1 delivery; 4 - pump 2 delivery; etc


    bool leverpulled[2]{ false, false };
    float leverpulledtime[2]{ 0,0 };


    // initiate auditory cues
    std::optional<ws::audio::BufferHandle> debug_audio_buffer;
    std::optional<ws::audio::BufferHandle> start_trial_audio_buffer_task1;
    std::optional<ws::audio::BufferHandle> start_trial_audio_buffer_task2;
    std::optional<ws::audio::BufferHandle> lever1_large_juice_audio_buffer; // lever2 small juice 
    std::optional<ws::audio::BufferHandle> lever1_small_juice_audio_buffer; // lever2 large juice 

    // initiate stimuli if using colored squares
    ws::Vec2f stim0_size{ 0.2f };
    ws::Vec2f stim0_offset{ -0.4f, 0.25f };
    ws::Vec3f stim0_color{ 1.0f };
    ws::Vec3f stim0_color_noreward{ 0.5f, 0.5f, 0.5f };
    ws::Vec3f stim0_color_cooper{ 1.0f, 1.0f, 0.0f };
    ws::Vec3f stim0_color_disappear{ 0.0f };
    ws::Vec2f stim1_size{ 0.2f };
    ws::Vec2f stim1_offset{ 0.4f, 0.25f };
    ws::Vec3f stim1_color{ 1.0f };
    ws::Vec3f stim1_color_noreward{ 1.0f, 1.0f, 0.0f };
    ws::Vec3f stim1_color_cooper{ 1.0f, 1.0f, 0.0f };
    ws::Vec3f stim1_color_disappear{ 0.0f };
    // initiate stimuli if using other images (non colored squares)
    std::optional<ws::gfx::TextureHandle> debug_image;

    // struct for saving data
    // std::ofstream save_trial_data_file;

    bool dont_save_data{};
    std::vector<TrialRecord> trial_records;
    std::vector<BehaviorData> behavior_data;
    std::vector<SessionInfo> session_info;
    std::vector<LeverReadout> lever_readout; // under construction

};

// save data for Trial Record
json to_json(const TrialRecord& trial) {
    json result;
    result["trial_number"] = trial.trial_number;
    result["rewarded"] = trial.rewarded;
    result["first_pull_id"] = trial.first_pull_id;
    result["task_type"] = trial.task_type;
    result["trial_starttime"] = trial.trial_start_time_stamp;
    return result;
}

json to_json(const std::vector<TrialRecord>& records) {
    json result;
    for (auto& record : records) {
        result.push_back(to_json(record));
    }
    return result;
}


// save data for behavior data
json to_json(const BehaviorData& bhv_data) {
    json result;
    result["trial_number"] = bhv_data.trial_number;
    result["time_points"] = bhv_data.time_points;
    result["behavior_events"] = bhv_data.behavior_events;
    return result;
}

json to_json(const std::vector<BehaviorData>& time_stamps) {
    json result;
    for (auto& time_stamp : time_stamps) {
        result.push_back(to_json(time_stamp));
    }
    return result;
}


// save data for session information
json to_json(const SessionInfo& session_info) {
    json result;
    result["animal1_name"] = session_info.animal1_name;
    result["animal2_name"] = session_info.animal2_name;
    result["experiment_date"] = session_info.experiment_date;
    result["task_type"] = session_info.task_type;
    result["tasktype_block"] = session_info.task_type_block;
    result["tasktype_random"] = session_info.task_type_random;
    result["tasktype_blocklength"] = session_info.task_type_blocklength;
    result["large_reward_volume"] = session_info.large_juice_volume;
    result["small_reward_volume"] = session_info.small_juice_volume;

    return result;
}

json to_json(const std::vector<SessionInfo>& session_info) {
    json result;
    for (auto& sessioninfos : session_info) {
        result.push_back(to_json(sessioninfos));
    }
    return result;
}

// save data for lever information
json to_json(const LeverReadout& lever_reading) {
    json result;
    result["trial_number"] = lever_reading.trial_number;
    result["readout_timepoint"] = lever_reading.readout_timepoint;
    result["potentiometer_lever2"] = lever_reading.potentiometer_lever;
    result["potentiometer_lever1"] = lever_reading.strain_gauge_lever;
    result["pull_or_release"] = lever_reading.pull_or_release;
    return result;
}

json to_json(const std::vector<LeverReadout>& lever_reads) {
    json result;
    for (auto& lever_read : lever_reads) {
        result.push_back(to_json(lever_read));
    }
    return result;
}


void setup(App& app) {


    auto buff_p_task1 = std::string{ WS_RES_DIR } + "/sounds/start_trial_beep_task1.wav";
    app.start_trial_audio_buffer_task1 = ws::audio::read_buffer(buff_p_task1.c_str());

    auto buff_p_task2 = std::string{ WS_RES_DIR } + "/sounds/start_trial_beep_task2.wav";
    app.start_trial_audio_buffer_task2 = ws::audio::read_buffer(buff_p_task2.c_str());

    // auto buff_p1 = std::string{ WS_RES_DIR } + "/sounds/" + app.animal1_name + "_large_juice_beep_" + std::to_string(app.tasktype) + ".wav";
    auto buff_p1 = std::string{ WS_RES_DIR } + "/sounds/" + app.animal1_name + "_large_juice_beep_1.wav";
    app.lever1_large_juice_audio_buffer = ws::audio::read_buffer(buff_p1.c_str());

    //auto buff_p2 = std::string{ WS_RES_DIR } + "/sounds/" + app.animal1_name + "_small_juice_beep_" + std::to_string(app.tasktype) + ".wav";
    auto buff_p2 = std::string{ WS_RES_DIR } + "/sounds/" + app.animal1_name + "_small_juice_beep_1.wav";
    app.lever1_small_juice_audio_buffer = ws::audio::read_buffer(buff_p2.c_str());

    // define the threshold of pulling
    const float dflt_rising_edge = 0.45f;  // 0.6f
    const float dflt_falling_edge = 0.25f; // 0.25f
    app.detect_pull[0].rising_edge = dflt_rising_edge;
    app.detect_pull[1].rising_edge = dflt_rising_edge;
    app.detect_pull[0].falling_edge = dflt_falling_edge;
    app.detect_pull[1].falling_edge = dflt_falling_edge;


}

void shutdown(App& app) {
    (void)app;

    auto postfix = ws::date_string();
    std::string trialrecords_name = app.experiment_date + "_" + app.animal1_name + "_" + app.animal2_name + "_TrialRecord_" + postfix + ".json";
    std::string bhvdata_name = app.experiment_date + "_" + app.animal1_name + "_" + app.animal2_name + "_bhv_data_" + postfix + ".json";
    std::string sessioninfo_name = app.experiment_date + "_" + app.animal1_name + "_" + app.animal2_name + "_session_info_" + postfix + ".json";
    std::string leverread_name = app.experiment_date + "_" + app.animal1_name + "_" + app.animal2_name + "_lever_reading_" + postfix + ".json";

    if (!app.dont_save_data) {
        std::string file_path1 = std::string{ WS_DATA_DIR } + "/" + trialrecords_name;
        std::ofstream output_file1(file_path1);
        output_file1 << to_json(app.trial_records);

        std::string file_path2 = std::string{ WS_DATA_DIR } + "/" + bhvdata_name;
        std::ofstream output_file2(file_path2);
        output_file2 << to_json(app.behavior_data);

        // save some task information into session_info
        SessionInfo session_info{};
        session_info.animal1_name = app.animal1_name;
        session_info.animal2_name = app.animal2_name;
        session_info.experiment_date = app.experiment_date;
        session_info.task_type = app.tasktype;
        session_info.task_type_block = app.tasktype_block;
        session_info.task_type_random = app.tasktype_random;
        session_info.task_type_blocklength = app.block_length;
        session_info.large_juice_volume = app.large_juice_volume;
        session_info.small_juice_volume = app.small_juice_volume;
        app.session_info.push_back(session_info);

        std::string file_path3 = std::string{ WS_DATA_DIR } + "/" + sessioninfo_name;
        std::ofstream output_file3(file_path3);
        output_file3 << to_json(app.session_info);

        std::string file_path4 = std::string{ WS_DATA_DIR } + "/" + leverread_name;
        std::ofstream output_file4(file_path4);
        output_file4 << to_json(app.lever_readout);
    }
}


void render_lever_gui(App& app) {
    ws::gui::LeverGUIParams gui_params{};
    gui_params.serial_ports = app.ports.data();
    gui_params.num_serial_ports = int(app.ports.size());
    gui_params.num_levers = int(app.levers.size());
    gui_params.levers = app.levers.data();
    gui_params.lever_system = ws::lever::get_global_lever_system();
    auto gui_res = ws::gui::render_lever_gui(gui_params);
}


void render_juice_pump_gui(App& app) {
    ws::gui::JuicePumpGUIParams gui_params{};
    gui_params.serial_ports = app.ports.data();
    gui_params.num_ports = int(app.ports.size());
    gui_params.num_pumps = 2;
    gui_params.allow_automated_run = app.allow_automated_juice_delivery;
    auto res = ws::gui::render_juice_pump_gui(gui_params);

    if (res.allow_automated_run) {
        app.allow_automated_juice_delivery = res.allow_automated_run.value();
    }
}

std::optional<std::string> render_text_input_field(const char* field_name) {
    const auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;

    char text_buffer[50];
    memset(text_buffer, 0, 50);

    if (ImGui::InputText(field_name, text_buffer, 50, enter_flag)) {
        return std::string{ text_buffer };
    }
    else {
        return std::nullopt;
    }
}


void render_gui(App& app) {
    const auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;



    ImGui::Begin("GUI");
    if (ImGui::Button("Refresh ports")) {
        app.ports = ws::enumerate_ports();
    }
    
    if (ImGui::Button("start the trial")) {
        app.start_render = true;
    }

    if (ImGui::Button("pause the trial")) {
        app.start_render = false;
    }

    bool save_data = !app.dont_save_data;
    if (ImGui::Checkbox("SaveData", &save_data)) {
        app.dont_save_data = !save_data;
    }


    render_lever_gui(app);


    if (ImGui::TreeNode("PullDetect")) {
        //ImGui::InputFloat2("PositionLimits", app.lever_position_limits);
        float lever_position_limits0[2]{ app.lever_position_limits[0],app.lever_position_limits[1] };
        float lever_position_limits1[2]{ app.lever_position_limits[2],app.lever_position_limits[3] };
        ImGui::InputFloat2("PositionLimits0", lever_position_limits0);
        ImGui::InputFloat2("PositionLimits1", lever_position_limits1);

        auto& detect = app.detect_pull;
        if (ImGui::InputFloat("RisingEdge", &detect[0].rising_edge, 0.0f, 0.0f, "%0.3f", enter_flag)) {
            detect[1].rising_edge = detect[0].rising_edge;
        }
        if (ImGui::InputFloat("FallingEdge", &detect[0].falling_edge, 0.0f, 0.0f, "%0.3f", enter_flag)) {
            detect[1].falling_edge = detect[1].rising_edge;
        }

        ImGui::TreePop();
    }


   // if (ImGui::TreeNode("Stim0")) {
   //     ImGui::InputFloat3("Color", &app.stim0_color.x);
   //     ImGui::InputFloat2("Offset", &app.stim0_offset.x);
   //     ImGui::InputFloat2("Size", &app.stim0_size.x);
   //     ImGui::TreePop();
   // }
   // if (ImGui::TreeNode("Stim1")) {
   //     ImGui::InputFloat3("Color", &app.stim1_color.x);
   //     ImGui::InputFloat2("Offset", &app.stim1_offset.x);
   //     ImGui::InputFloat2("Size", &app.stim1_size.x);
   //     ImGui::TreePop();
   // }

    ImGui::End();


    ImGui::Begin("JuicePump");
    render_juice_pump_gui(app);
    ImGui::End();
}


float to_normalized(float v, float min, float max, bool inv) {
    if (min == max) {
        v = 0.0f;
    }
    else {
        v = ws::clamp(v, min, max);
        v = (v - min) / (max - min);
    }
    return inv ? 1.0f - v : v;
}


void task_update(App& app) {
    using namespace ws;

    static int state{};
    static bool entry{ true };
    static NewTrialState new_trial{};
    static DelayState delay{};
    static InnerDelayState innerdelay{};
    static bool start_session_sound{ true };



    //
    // renew for every new trial
    if (entry && state == 0) {

        if (app.tasktype_random) {
            app.tasktype = rand() % 2 + 1; // for the trial by trial randomization condition; 1: competition; 2: dilemma
        }
        if (app.tasktype_block) {
            if (!app.tasktype_random) {
                if ((app.trialnumber % app.block_length) == 0) {
                    if (app.tasktype == 1) { app.tasktype = 2; }
                    else if (app.tasktype == 2) { app.tasktype = 1; }
                }
            }
            else if (app.tasktype_random) {
                app.block_length = app.block_length;
                if (app.trialnumber % app.block_length == 0) {
                    if (app.tasktype == 1) { app.tasktype = 2; }
                    else if (app.tasktype == 2) { app.tasktype = 1; }
                }
            }
        }

        if (app.tasktype == 1) {
            app.juice2_delay_time = 1500;
        }
        else if (app.tasktype == 2) {
            app.juice2_delay_time = 750;
        }

        app.rewarded[0] = 0;
        app.rewarded[1] = 0;
        app.leverpulled[0] = false;
        app.leverpulled[1] = false;
        app.leverpulledtime[0] = 0;
        app.leverpulledtime[1] = 0;


        // sound to indicate the start of a TRIAL
        if (start_session_sound) {
            if (app.tasktype == 1) {
                ws::audio::play_buffer_both(app.start_trial_audio_buffer_task1.value(), 0.5f);
            }
            else if (app.tasktype == 2) {
                ws::audio::play_buffer_both(app.start_trial_audio_buffer_task2.value(), 0.5f);
            }
            start_session_sound = true;
        }
        // get the session start time
        if (app.trialnumber == 0) {
            app.session_start_time = now();
        }

        // end session when trialnumber or total sesison time reach the threshold
        //if (app.trialnumber > app.total_trial_number || elapsed_time(app.session_start_time, now()) > app.new_total_time) {
        //  abort;
        //}

        // push the lever force back to normal
        // if (app.allow_auto_lever_force_set) {
        //     ws::lever::set_force(ws::lever::get_global_lever_system(), app.levers[0], app.normalforce);
        //     ws::lever::set_force(ws::lever::get_global_lever_system(), app.levers[1], app.normalforce);
        // }
    }

    // check the levers
    for (int i = 0; i < 2; i++) {
        // const auto lh = app.levers[i];
        const auto lh = app.levers[0];
        auto& pd = app.detect_pull[i];
        if (auto lever_state = ws::lever::get_state(ws::lever::get_global_lever_system(), lh)) {
            ws::lever::PullDetectParams params{};
            params.current_position = to_normalized(
                lever_state.value().potentiometer_reading,
                app.lever_position_limits[2 * i],
                app.lever_position_limits[2 * i + 1],
                app.invert_lever_position[i]);
            auto pull_res = ws::lever::detect_pull(&pd, params);
            if (pull_res.pulled_lever) {


                // trial starts # 1
                // trial starts whenever one of the animal pulls
                if (!app.leverpulled[0] || !app.leverpulled[1]) {
                    app.trialnumber = app.trialnumber + 1;
                    app.first_pull_id = i + 1;
                    app.timepoint = 0;
                    app.trialstart_time = now();
                    app.trial_start_time_forsave = elapsed_time(app.session_start_time, now());
                    app.first_pull_time = now();
                    app.behavior_event = 0; // start of a trial
                    BehaviorData time_stamps{};
                    time_stamps.trial_number = app.trialnumber;
                    time_stamps.time_points = app.timepoint;
                    time_stamps.behavior_events = app.behavior_event;
                    app.behavior_data.push_back(time_stamps);
                }


                // save some behavioral events data
                app.timepoint = elapsed_time(app.trialstart_time, now());
                app.behavior_event = i + 1; // lever i+1 (1 or 2) is pulled
                app.other_pull_time = elapsed_time(app.first_pull_time, now());
                BehaviorData time_stamps2{};
                time_stamps2.trial_number = app.trialnumber;
                time_stamps2.time_points = app.timepoint;
                time_stamps2.behavior_events = app.behavior_event;
                app.behavior_data.push_back(time_stamps2);

                // save some lever information data
                LeverReadout lever_read{};
                lever_read.trial_number = app.trialnumber;
                lever_read.readout_timepoint = app.timepoint;
                lever_read.strain_gauge_lever = lever_state.value().strain_gauge;
                lever_read.potentiometer_lever = lever_state.value().potentiometer_reading;
                lever_read.lever_id = i + 1;
                lever_read.pull_or_release = int(pull_res.pulled_lever);
                app.lever_readout.push_back(lever_read);

                app.leverpulled[i] = true;


                // deliver juice accordingly
                // competition condition
                if (app.tasktype == 1) {
                    // sound cue
                    // the aninal who pulls get large reward
                    if (i == 0) {
                        // ws::audio::play_buffer_on_channel(app.lever1_large_juice_audio_buffer.value(), abs(i - 1), 0.5f
                        ws::audio::play_buffer_both(app.lever1_large_juice_audio_buffer.value(), 0.5f);
                    }
                    else if (i == 1) {
                        // ws::audio::play_buffer_on_channel(app.lever1_small_juice_audio_buffer.value(), abs(i), 0.5f);
                        ws::audio::play_buffer_both(app.lever1_small_juice_audio_buffer.value(), 0.5f);
                    }
                    // juice delivery time       
                    // the aninal who pulls get large reward      
                    // set the reward size - the delivery will happen in (states==1)
                    auto pump_handle1 = ws::pump::ith_pump(abs(i)); // pump id: 0 - pump 1; 1 - pump 2  -WS 
                    auto desired_pump_state1 = ws::pump::read_desired_pump_state(pump_handle1);
                    desired_pump_state1.volume = app.large_juice_volume;
                    ws::pump::set_dispensed_volume(pump_handle1, desired_pump_state1.volume, desired_pump_state1.volume_units);
                    //        
                    // the aninal who does not pull get small reward
                    // set the reward size - the delivery will happen in (states==1)
                    auto pump_handle2 = ws::pump::ith_pump(abs(i - 1)); // pump id: 0 - pump 1; 1 - pump 2  -WS              
                    auto desired_pump_state2 = ws::pump::read_desired_pump_state(pump_handle2);
                    desired_pump_state2.volume = app.small_juice_volume;
                    ws::pump::set_dispensed_volume(pump_handle2, desired_pump_state2.volume, desired_pump_state2.volume_units);                    
                }

                // social dilemma condition
                else if (app.tasktype == 2) {
                    // sound cue
                    // the aninal who pulls get large reward
                    if (i == 1) {
                        // ws::audio::play_buffer_on_channel(app.lever1_large_juice_audio_buffer.value(), abs(i - 1), 0.5f
                        ws::audio::play_buffer_both(app.lever1_large_juice_audio_buffer.value(), 0.5f);
                    }
                    else if (i == 0) {
                        // ws::audio::play_buffer_on_channel(app.lever1_small_juice_audio_buffer.value(), abs(i), 0.5f);
                        ws::audio::play_buffer_both(app.lever1_small_juice_audio_buffer.value(), 0.5f);
                    }

                    // juice delivery time       
                    // the aninal who pulls get large reward      
                    // set the reward size - the delivery will happen in (states==1)
                    auto pump_handle1 = ws::pump::ith_pump(abs(i)); // pump id: 0 - pump 1; 1 - pump 2  -WS 
                    auto desired_pump_state1 = ws::pump::read_desired_pump_state(pump_handle1);
                    desired_pump_state1.volume = app.small_juice_volume;
                    ws::pump::set_dispensed_volume(pump_handle1, desired_pump_state1.volume, desired_pump_state1.volume_units);
                    //        
                    // the aninal who does not pull get small reward
                    // set the reward size - the delivery will happen in (states==1)
                    auto pump_handle2 = ws::pump::ith_pump(abs(i - 1)); // pump id: 0 - pump 1; 1 - pump 2  -WS              
                    auto desired_pump_state2 = ws::pump::read_desired_pump_state(pump_handle2);
                    desired_pump_state2.volume = app.large_juice_volume;
                    ws::pump::set_dispensed_volume(pump_handle2, desired_pump_state2.volume, desired_pump_state2.volume_units);
                }

                
                // define the start of the new trial
                else if (lever_read.lever_id == app.first_pull_id) {

                    // old edition
                    app.first_pull_time = now();
                    
                }

            }


            else if (pull_res.released_lever) {
  
                // save some lever information data
                LeverReadout lever_read{};
                lever_read.trial_number = app.trialnumber;
                lever_read.readout_timepoint = elapsed_time(app.trialstart_time, now());;           
                lever_read.strain_gauge_lever = lever_state.value().strain_gauge;
                lever_read.potentiometer_lever = lever_state.value().potentiometer_reading;
                lever_read.lever_id = i + 1;
                lever_read.pull_or_release = int(pull_res.pulled_lever);
                app.lever_readout.push_back(lever_read);

                // end of trial
                state = 1;

            }

             
        }
    }


    switch (state) {
    case 0: {
        // new_trial.play_sound_on_entry = app.start_trial_audio_buffer;
        new_trial.total_time = app.new_total_time;
        new_trial.stim0_offset = app.stim0_offset;
        new_trial.stim0_size = app.stim0_size;
        new_trial.stim1_offset = app.stim1_offset;
        new_trial.stim1_size = app.stim1_size;
        if (app.tasktype == 0) {
            //auto buff_p = std::string{ WS_RES_DIR } + "/sounds/start_trial_beep.wav";
            //app.start_trial_audio_buffer = ws::audio::read_buffer(buff_p.c_str());
            // auto debug_image_p = std::string{ WS_RES_DIR } + "/images/calla_leaves.png";
            // app.debug_image = ws::gfx::read_2d_image(debug_image_p.c_str());
            //
            new_trial.stim0_image = std::nullopt;
            new_trial.stim1_image = std::nullopt;
            new_trial.stim0_color = app.stim0_color_noreward;
            new_trial.stim1_color = app.stim1_color_noreward;
        }
        else if (app.tasktype == 1) {
            //auto buff_p = std::string{ WS_RES_DIR } + "/sounds/start_trial_beep.wav";
            //app.start_trial_audio_buffer = ws::audio::read_buffer(buff_p.c_str());
            //
            new_trial.stim0_image = std::nullopt;
            new_trial.stim1_image = std::nullopt;
            new_trial.stim0_color = app.stim0_color;
            new_trial.stim1_color = app.stim1_color;
        }
        else if (app.tasktype == 2) {
            //auto buff_p = std::string{ WS_RES_DIR } + "/sounds/start_trial_beep.wav";
            //app.start_trial_audio_buffer = ws::audio::read_buffer(buff_p.c_str());
            //auto debug_image_p = std::string{ WS_RES_DIR } + "/images/blue_triangle.png";
           // app.debug_image = ws::gfx::read_2d_image(debug_image_p.c_str());
            //
            new_trial.stim0_image = app.debug_image;
            new_trial.stim1_image = app.debug_image;
        }
        


        if (entry && app.allow_automated_juice_delivery) {
            auto pump_handle = ws::pump::ith_pump(1); // pump id: 0 - pump 1; 1 - pump 2
            ws::pump::run_dispense_program(pump_handle);
        }


        auto nt_res = tick_new_trial(&new_trial, &entry);
        if (nt_res.finished) {
            state = 1;
            entry = true;
        }
        break;
    }

    case 1: {

        // juice delivery time       
        // deliver the juice for animal 1
        std::this_thread::sleep_for(std::chrono::milliseconds(app.juice1_delay_time));
        auto pump_handle1_1 = ws::pump::ith_pump(abs(app.first_pull_id - 1)); // pump id: 0 - pump 1; 1 - pump 2  -WS 
        ws::pump::run_dispense_program(pump_handle1_1);
        ws::pump::submit_commands();
        app.getreward[app.first_pull_id - 1] = true;
        app.rewarded[app.first_pull_id - 1] = 1;
        //
        app.timepoint = elapsed_time(app.trialstart_time, now());
        app.behavior_event = abs(app.first_pull_id - 1) + 3; // pump 1 or 2 deliver  
        BehaviorData time_stamps3{};
        time_stamps3.trial_number = app.trialnumber;
        time_stamps3.time_points = app.timepoint;
        time_stamps3.behavior_events = app.behavior_event;
        app.behavior_data.push_back(time_stamps3);

        // deliver the juice for animal 2
        std::this_thread::sleep_for(std::chrono::milliseconds(app.juice2_delay_time));
        auto pump_handle2_1 = ws::pump::ith_pump(abs(app.first_pull_id - 1 - 1)); // pump id: 0 - pump 1; 1 - pump 2  -WS
        ws::pump::run_dispense_program(pump_handle2_1);
        ws::pump::submit_commands();
        app.getreward[abs(app.first_pull_id - 1 - 1)] = true;
        app.rewarded[abs(app.first_pull_id - 1 - 1)] = 1;
        //
        app.timepoint = elapsed_time(app.trialstart_time, now());
        app.behavior_event = abs(app.first_pull_id - 1 - 1) + 3; // pump 1 or 2 deliver  
        BehaviorData time_stamps4{};
        time_stamps4.trial_number = app.trialnumber;
        time_stamps4.time_points = app.timepoint;
        time_stamps4.behavior_events = app.behavior_event;
        app.behavior_data.push_back(time_stamps4);

        state = 2;
        entry = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(app.after_delivery_time));
        app.timepoint = elapsed_time(app.trialstart_time, now());
        app.behavior_event = 9; // end of a trial
        BehaviorData time_stamps{};
        time_stamps.trial_number = app.trialnumber;
        time_stamps.time_points = app.timepoint;
        time_stamps.behavior_events = app.behavior_event;
        app.behavior_data.push_back(time_stamps);
        app.getreward[0] = false;
        app.getreward[1] = false;
        break;

    }

    // save some data
    case 2: {

        TrialRecord trial_record{};
        trial_record.trial_number = app.trialnumber;
        trial_record.first_pull_id = app.first_pull_id;
        trial_record.rewarded = app.rewarded[0] + app.rewarded[1];
        trial_record.task_type = app.tasktype;
        trial_record.trial_start_time_stamp = app.trial_start_time_forsave;
        //  Add to the array of trials.
        app.trial_records.push_back(trial_record);

 
        state = 0;
        entry = true;
        break;
    }

    default: {
        assert(false);
    }
    }
}



int main(int, char**) {
    srand(time(NULL));
    auto app = std::make_unique<App>();
    return app->run();


}

