
#pragma once

#include "Source/DeploymentThreads/DeploymentThread.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <juce_audio_basics/juce_audio_basics.h>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

class AudioInManager
{
public:
    int hopSize = 2048;               // number of samples discarded after each analysis (used only if autoDiscardAllAnalyzedSamples is false)
    bool autoDiscardAllAnalyzedSamples = false; // if true, all analyzed samples are discarded
    int analysisWindowSize = 4096;    // the number of samples to analyze at a time
    juce::Array<double> timeStamps;   // time stamps of the samples in the buffer

    StaticLockFreeQueue<juce::AudioBuffer<float>, 1024> queue{};
    StaticLockFreeQueue<juce::Array<double>, 1024> times_queue{};

    AudioInManager() {}

    ~AudioInManager(){
        buffer.clear();
    }

    // Append new samples to the buffer
    void append(const juce::AudioBuffer<float>& newSamples, double start_ppq, double qpm) {

        if (newSamples.getNumChannels() <= 0) {
            return;
        }

        // Append only samples from channel 0 to the internal buffer
        const float* samplesToAdd = newSamples.getReadPointer(0);
        buffer.addArray(samplesToAdd, newSamples.getNumSamples());

        // Add the time stamps of the samples
        for (int i = 0; i < newSamples.getNumSamples(); ++i) {
            timeStamps.add(start_ppq + i * 60.0 / (qpm * 44100.0));
        }

        // add to queue until not ready anymore
        while (hasEnoughSamples()) {
            juce::AudioBuffer<float> tempBuffer;
            juce::Array<double> tempTimes;

            if (retrieve(tempBuffer, tempTimes)) {
                queue.push(tempBuffer);
                times_queue.push(tempTimes);
            }
        }
    }

    // Retrieve 4096 samples if available
    bool retrieve(juce::AudioBuffer<float>& outputBuffer, juce::Array<double>& outputTimes) {
        if (buffer.size() >= analysisWindowSize) {
            outputBuffer.setSize(1, analysisWindowSize, false);
            outputBuffer.copyFrom(0, 0, buffer.getRawDataPointer(), analysisWindowSize);
            outputTimes.addArray(timeStamps, 0, analysisWindowSize);
            if (autoDiscardAllAnalyzedSamples) {
                discardSamples(analysisWindowSize);
            } else {
                discardSamples(hopSize);
            }
            return true;
        }
        return false;
    }

    // Get the number of samples in the buffer
    [[nodiscard]] int getNumSamples() const {
        return buffer.size();
    }

    // Force clear all
    void clear() {
        buffer.clear();
        timeStamps.clear();
    }

    // bool hasEnoughSamples()
    bool hasEnoughSamples() {
        return buffer.size() >= analysisWindowSize;
    }

private:
    // Discard the first 'numSamplesToDiscard' samples and shift the rest
    void discardSamples(int numSamplesToDiscard) {
        if (buffer.size() > numSamplesToDiscard) {
            buffer.removeRange(0, numSamplesToDiscard);
            timeStamps.removeRange(0, numSamplesToDiscard);
            // Update start time based on the number of discarded samples
        } else {
            buffer.clear();
        }
    }

    juce::Array<float> buffer;
    static constexpr int sampleRate = 44100; // Fixed sample rate of 44100 Hz
};

class PluginDeploymentThread: public DeploymentThread {
public:
    AudioInManager queued_audio_buffer;
    bool no_presets_received_yet = true;

    // initialize your deployment thread here
    PluginDeploymentThread():DeploymentThread() {
        audio_frame.setSize(1, queued_audio_buffer.analysisWindowSize);

        // initialize the latent vectors to latent_g of an empty groove
        latent_g = torch::randn({ 1, latent_dim });
        latent_A = torch::randn({ 1, latent_dim });
        latent_B = torch::randn({ 1, latent_dim });
        latent_vector = torch::randn({ 1, latent_dim });
    }

    // this method runs on a per-event basis.
    // the majority of the deployment will be done here!
    std::pair<bool, bool> deploy (
        std::optional<MidiFileEvent> & new_midi_event_dragdrop,
        std::optional<EventFromHost> & new_event_from_host,
        bool gui_params_changed_since_last_call,
        bool new_preset_loaded_since_last_call,
        bool new_midi_file_dropped_on_visualizers,
        bool new_audio_file_dropped_on_visualizers) override {

        if (new_preset_loaded_since_last_call && no_presets_received_yet) {
            no_presets_received_yet = false;
        }

        if (new_event_from_host != std::nullopt &&  new_event_from_host->getBufferMetaData().sample_rate == 44100) {
            if (new_event_from_host->isFirstBufferEvent() || new_event_from_host->isPlaybackStoppedEvent() || new_event_from_host->isNewBufferEvent()) {

                if (new_event_from_host->isPlaybackStoppedEvent()) {
                    // Clear the queued audio buffer
                    queued_audio_buffer.clear();
                } else
                {
                    // Append the new audio buffer to the queued audio buffer
                    queued_audio_buffer.append(new_event_from_host->audioBuffer, new_event_from_host->getBufferMetaData().time_in_ppq, new_event_from_host->getBufferMetaData().qpm);

                }
            }
        } else if (new_event_from_host->getBufferMetaData().sample_rate != 44100) {
            cout << "Currently only supporting 44100 Hz sample rate for audio inputs!!!" << endl;
        }


        // Try loading the model if it hasn't been loaded yet
        if (!isModelLoaded) {
            // load the model
            load(model_path);

            if (no_presets_received_yet) {
                encodeGroove();
                latent_A = latent_g;
                latent_B = latent_g;
                latent_vector = latent_g;

                /*// track the initial random latent vectors --> this shouldn't be done here otherwise it will overwrite the preset (fixme later by ensuring preset data has separate fields for loaded and generated tensors)
                CustomPresetData->tensor("latent_A", latent_A);
                CustomPresetData->tensor("latent_B", latent_B);
                CustomPresetData->tensor("latent_g", latent_g);*/
            }

        }

        if (!ddc_onset_loaded) {
            ddc_onset_model = load_processing_script("DDC_Onset.pt");
            ddc_onset_loaded = true;

            // pass through ddc_onset_model
            std::vector<torch::jit::IValue> in;
            // random torch tensor
            in.emplace_back(torch::randn({ 1, queued_audio_buffer.analysisWindowSize }).toType(torch::kFloat32));
            // add difficulty level
            in.emplace_back(4);
            auto onset_saliences = ddc_onset_model.forward(in);

        }

        /*auto dropped_audio = audioVisualizersData->get_visualizer_data("AudioDroppedVisualizer");
        if ( dropped_audio != std::nullopt) {
            QueuedAudioBuffer temp_buffer;
            temp_buffer.setStartTime(0.0);
            auto [audio, fs] = *dropped_audio;
            temp_buffer.append(audio);
            while (temp_buffer.hasEnoughSamples()) {
                // access samples to process
                temp_buffer.retrieve(audio_frame);
                std::vector<float> audioData;
                audioData.reserve(audio_frame.getNumSamples());
                audioData.insert(audioData.end(), audio_frame.getReadPointer(0), audio_frame.getReadPointer(0) + audio_frame.getNumSamples());

                // pass through ddc_onset_model
                std::vector<torch::jit::IValue> in;
                in.emplace_back(torch::tensor(audioData, torch::kFloat32));
              // add difficulty level
                in.emplace_back(4);
                auto onset_saliences = ddc_onset_model.forward(in);
                // print the saliences
                auto saliences = onset_saliences.toTensor().flatten();

            }
        }*/

        // main deployment logic
        if (isModelLoaded) {
            // check if a new preset was loaded and update the tensors accordingly
            auto new_presets_loaded = new_preset_loaded_since_last_call ? loadTensorsFromPreset() : false;

            // check if model selection was changed
            bool new_model_selected = false;
            if (gui_params_changed_since_last_call) {
                auto m_path_selected = gui_params.getComboBoxSelectionText("Model");
                if (m_path_selected == "0.2Beta") {
                    m_path_selected = "mute_genre_latent_vae_beta_0_2.pt";
                } else if (m_path_selected == "0.5Beta") {
                    m_path_selected = "mute_genre_latent_vae_beta_0_5.pt";
                } else if (m_path_selected == "1.0Beta") {
                    m_path_selected = "mute_genre_latent_vae_beta_1_0.pt";
                }
                if (m_path_selected != model_path) {
                    model_path = m_path_selected;
                    load(model_path);
                    new_model_selected = true;
                }
            }

            // update groove if new host event is available
            auto groove_just_updated =
                grooveUpdated(new_event_from_host, new_presets_loaded);
            groove_has_been_updated = groove_has_been_updated || groove_just_updated || new_model_selected; // this is used to track if groove has been updated (in case of responsiveness > 0)

            // re-encode the groove
            auto finished_encoding_groove = false;
            if (groove_has_been_updated) {
                finished_encoding_groove = encodeGroove();
                if (finished_encoding_groove) {
                    groove_has_been_updated = false; // reset the flag
                }
            }

            // re-calculate the playback latent_vector if any of the above changed
            auto latents_changed = updateLatents(new_presets_loaded, finished_encoding_groove);

            // check if the latent vector has been auto moved
            latents_changed = latents_changed || latentAutoMoved(new_event_from_host);

            // update the voice mapping
            auto voice_mapping_changed = updateVoiceMapping();

            // per voice threshold and max counts changed
            auto sampling_params_changed = updatePerVoiceSamplingParams();

            // update the genre selection
            auto genre_changed = updateGenreSelection();

            // generate the pattern
            if (latents_changed || sampling_params_changed || genre_changed) {
                decodeLatent(latent_vector, playback_hits, playback_velocities, playback_offsets);
            }

            // update per voice velocity scaling mask
            // auto generation_velocity_mask_updated = updateVelocityScalingMask();

            // prepare the playback sequence and policy
            if (latents_changed || voice_mapping_changed ||
                sampling_params_changed || /*generation_velocity_mask_updated ||*/ genre_changed) {

                if (/*generation_velocity_mask_updated ||*/ sampling_params_changed || genre_changed) {
                    // update A, B, Groove Displays
                    updateAllDisplayPatterns();
                }

                preparePlaybackSequence();
                preparePlaybackPolicy();

                return {true, true};
            }

        }

        // your implementation goes here
        return {false, false};
    }

private:
    bool ddc_onset_loaded = false;
    AudioBuffer<float> audio_frame;
    torch::jit::script::Module ddc_onset_model;

    double phead = -1.0f; // the actual playhead position
    int phead_step = -1; // step index corresponding to the playhead
    float memory_2bars = 0;

    bool groove_has_been_updated = false;
    double last_generation_phead = -2.0f;
    int responsiveness = 0;

    bool autoMoveGoingToA = false;
    float auto_move_step = 0.01f;
    float auto_move_rate = 4.0f; // every quarter note update
    float slowest_auto_move_rate = 4.0f;
    float fastest_auto_move_rate = 0.0025f;
    bool auto_move_enabled = false;
    float last_move_ppq = -10.0f;

    std::string model_path = "mute_genre_latent_vae_beta_0_5.pt";

    // Ring buffer for storing the hits 2-bar previous to the playhead
    std::array<int, 16> hit_memory_buffer = {};

    // AB Interpolation Parameters
    int latent_dim = 128;
    torch::Tensor latent_A;
    torch::Tensor latent_B;
    torch::Tensor latent_g;
    torch::Tensor latent_vector;
    torch::Tensor current_genre = torch::tensor(0, torch::kLong);
    torch::Tensor kick_is_redirected = torch::tensor(0, torch::kLong);
    torch::Tensor snare_is_redirected = torch::tensor(0, torch::kLong);
    torch::Tensor hats_is_redirected = torch::tensor(0, torch::kLong);
    torch::Tensor toms_is_redirected = torch::tensor(0, torch::kLong);
    torch::Tensor cymbals_is_redirected = torch::tensor(0, torch::kLong);
    const int num_genres = 9;

    // torch::Tensor latent_means = torch::tensor({ 0.839432, 0.028829, 0.260162, 0.254081, 0.639042, 0.187314, 0.621447, 0.197336, -0.285351, 0.329711, -0.092887, -0.455925, -0.463189, 0.057764, 0.280250, 0.720521, 0.077355, -0.342063, -0.119107, -0.030940, 0.302518, 0.248664, 0.459061, -0.195744, -0.519831, 0.102035, 0.066763, 0.034689, -0.120487, 0.274302, -0.631191, 0.481393, 0.104963, -0.182254, -0.492165, 0.258259, 0.083618, -0.202616, 0.003170, -0.066522, 0.065897, -0.053037, -0.257152, 0.472176, 0.064583, -0.216026, 0.304526, 0.342737, -0.113764, 0.231140, -0.359870, -1.299364, -0.414397, -0.309337, 0.300565, 0.156175, -0.013400, -0.456266, 0.349964, 0.163904, 0.130316, 0.159019, 0.144417, -0.052901, -0.097743, 0.311448, 0.078315, -0.255022, 0.176007, -0.111901, -0.232935, 0.499125, -0.114710, 0.242778, -0.369652, 0.684986, 0.255967, -0.315600, -0.059118, 0.179055, -0.102966, 0.003597, 0.935282, 0.214915, -0.003841, -0.555301, 0.101465, 0.070278, 0.129906, 0.076528, -0.204782, 0.114680, -0.474796, 0.122893, -0.138261, -0.221124, 0.132840, -0.378143, 0.234468, -0.135605, -0.408668, -0.302455, 1.082246, -0.186170, -0.423151, 0.221860, -0.108996, -0.016629, -0.394051, 0.057468, 0.053483, -0.200230, 0.270076, -0.280409, -0.195107, -0.785390, 0.295138, 0.262804, -0.077069, -0.053791, -0.038358, -0.196589, 0.068492, 0.183207, 0.224529, -0.088620, 0.088552, 0.180825}, torch::kFloat32);
    // torch::Tensor latent_stds = torch::tensor({ 1.059557, 0.476379, 0.455557, 0.771883, 0.595493, 0.748949, 1.253432, 0.525852, 0.532861, 0.529890, 0.748481, 1.368694, 0.773256, 0.716417, 0.380626, 1.266847, 0.456420, 0.668015, 0.510973, 0.528414, 1.074564, 1.247176, 0.568159, 0.491451, 0.468460, 0.816135, 0.430404, 1.110031, 1.195483, 0.494522, 0.510693, 0.603017, 0.532994, 1.164178, 1.295650, 0.518948, 0.567530, 0.559215, 0.495713, 0.536785, 1.340392, 0.588506, 0.534702, 0.458623, 0.555272, 0.509900, 0.490725, 1.154131, 0.554384, 1.013729, 0.436478, 1.332143, 1.249049, 0.822325, 1.290635, 0.417411, 0.654302, 0.444411, 1.241091, 0.616093, 0.623776, 1.442702, 0.917861, 0.558050, 0.734744, 0.449139, 1.121078, 1.522438, 0.436208, 0.555155, 0.494030, 1.169377, 0.557718, 0.521323, 0.611856, 1.368250, 0.573745, 0.426793, 1.328254, 0.357591, 1.329778, 0.589002, 1.299140, 0.381441, 1.151230, 0.645545, 0.593227, 0.371865, 0.483050, 0.404680, 0.766436, 1.205285, 0.480315, 0.370848, 0.482896, 0.499206, 0.455180, 0.501923, 0.384923, 0.453163, 0.631352, 0.552298, 1.427073, 0.421866, 0.558387, 0.493060, 0.436864, 0.480521, 0.634422, 0.583132, 0.498595, 0.455823, 0.781391, 0.509769, 0.571237, 1.227505, 0.501293, 0.441131, 0.561747, 0.557741, 0.497247, 0.456868, 0.515491, 0.421643, 1.220361, 1.178334, 1.383365, 1.354549}, torch::kFloat32);

    double interpolate_slider_value = 0.0;
    double follow_slider_value = 0.0;

    // add any member variables or methods you need here
    torch::Tensor voice_thresholds = torch::tensor({ // thresholds based on f1 analysis
        1.0f-0.51f, 1.0f-0.48f,
        1.0f-0.48f, 1.0f-0.63f,
        1.0f-0.68f, 1.0f-0.54f,
        1.0f-0.64f, 1.0f-0.63f,
        1.0f-0.47f}, torch::kFloat32);//torch::ones({ 9 }, torch::kFloat32) * 0.5f;
    torch::Tensor max_counts_allowed = torch::ones({ 9 }, torch::kInt64) * 32;
    std::map<int, int> voiceMap;

    // Velocity Mask
    torch::Tensor velocity_mask = torch::ones({ 1, 32, 9}, torch::kFloat32) * 1.0f;

    // following will be used for the inference
    torch::Tensor groove_hits = torch::zeros({1, 32, 1}, torch::kFloat32);
    torch::Tensor groove_velocities = torch::zeros({1, 32, 1}, torch::kFloat32);
    torch::Tensor groove_offsets = torch::zeros({1, 32, 1}, torch::kFloat32);
    torch::Tensor groove_hit_registrations = torch::ones({1, 32, 1}, torch::kFloat32) * -1.0f; // ppq at which the hit was registered
    double vel_scaler_val = 0.5f;
    double off_quantizer_val = 0.0f;
    double generation_off_quantizer_val = 0.0f;
    double groove_inversion = 0.0f;

    // scaled groove for inference
    torch::Tensor groove_hvo_scaled = torch::zeros({1, 32, 3}, torch::kFloat32);
    torch::Tensor groove_velocities_scaled = torch::zeros({1, 32, 1}, torch::kFloat32);
    torch::Tensor groove_offsets_scaled = torch::zeros({1, 32, 1}, torch::kFloat32);

    torch::Tensor playback_hits = torch::zeros({1, 32, 9}, torch::kFloat32);
    torch::Tensor playback_velocities = torch::zeros({1, 32, 9}, torch::kFloat32);
    torch::Tensor playback_offsets = torch::zeros({1, 32, 9}, torch::kFloat32);

    // voice map for visualization purposes only
    map<int, int> display_voice_maps = {
        {0, 36}, {1, 38}, {2, 42},
        {3, 46}, {4, 41}, {5, 48},
        {6, 45}, {7, 49}, {8, 51}
    };

    // temporary hit, velocity, offset tensors for visualization
    torch::Tensor display_hits;
    torch::Tensor display_velocities;
    torch::Tensor display_offsets;
    torch::Tensor display_offsets_scaled;


    // load the tensors from the preset
    bool loadTensorsFromPreset() {

        bool loaded = false;
        auto A = CustomPresetData->tensor("latent_A");
        if (A != std::nullopt)
        {
            latent_A = *A;
            loaded = true;
            displayPatternAt(latent_A, "MidiVisualizerA");
        }
        auto B = CustomPresetData->tensor("latent_B");
        if (B != std::nullopt)
        {
            latent_B = *B;
            loaded = true;
            displayPatternAt(latent_B, "MidiVisualizerB");
        }

//        auto groove_hits_ = CustomPresetData->tensor("groove_hits");
//        if (groove_hits_ != std::nullopt)
//        {
//            groove_hits = *groove_hits_;
//            loaded = true;
//        }
//        auto groove_velocities_ = CustomPresetData->tensor("groove_velocities");
//        if (groove_velocities_ != std::nullopt)
//        {
//            groove_velocities = *groove_velocities_;
//            loaded = true;
//        }
//        auto groove_offsets_ = CustomPresetData->tensor("groove_offsets");
//        if (groove_offsets_ != std::nullopt)
//        {
//            groove_offsets = *groove_offsets_;
//            loaded = true;
//        }

        return loaded;
    }


    // checks if a new host event has been received and
    // updates the input tensor accordingly
    // returns true if groove is to be re-encoded
    bool grooveUpdated(std::optional<EventFromHost> & new_event, bool new_preset) {

        bool groove_changed = false;
        bool density_changed = false;

        // Check responsiveness
        checkResponsivenessValuesFromButtons();

        // map memory to 2bar ratio
        if (gui_params.wasParamUpdated("Memory")) {
            auto mem = (float)gui_params.getValueFor("Memory");
            if (mem < 0.02) {
                memory_2bars = 0.0f; // forgets everything as soon as re-entered
            } else if (mem < 0.1) {
                memory_2bars = 8.0f; // 8 ppq = 2 bars of memory
            } else if (mem < 0.95f) {
                memory_2bars = (float(int(mem/0.1)) * 8.0f); // up to 9 2-bar repeats
            } else {
                memory_2bars = 1000000.0f; // infinite memory
            }
        }

        // check if groove is to be cleared
        if (gui_params.wasButtonClicked("Clear Groove")) {
            // clear the groove
            groove_hits = torch::zeros({1, 32, 1}, torch::kFloat32);
            groove_velocities = torch::zeros({1, 32, 1}, torch::kFloat32);
            groove_offsets = torch::zeros({1, 32, 1}, torch::kFloat32);
            midiVisualizersData->clear_visualizer_data("MidiVisualizerGroove");

            encodeGroove();
            groove_changed = true;
        }

        // check if adaptive follow is enabled
        if (gui_params.getValueFor("Adaptive Follow") > 0.5f) {
            auto max_amount = (float ) gui_params.getValueFor("Adaptive Follow Amount");
            auto d_ =   groove_hits.sum().item<float>() / 32.0f * max_amount;
            d_ = std::clamp(d_, 0.0f, 1.0f);
            gui_params.setValueFor("Follow", d_);
        }

        // get the groove information from the host event
        if (gui_params.wasParamUpdated("Groove Velocity")) {
            vel_scaler_val = gui_params.getValueFor("Groove Velocity");
            groove_changed = true;
        }

        if (gui_params.wasParamUpdated("Groove Offset")) {
            off_quantizer_val = gui_params.getValueFor("Groove Offset");
            groove_changed = true;
        }

//        if (gui_params.wasParamUpdated("Groove Invert")) {
//            groove_inversion = gui_params.getValueFor("Groove Invert");
//            groove_changed = true;
//        }

        // if new event, process based on parameters
        if (new_event != std::nullopt) {
            // get the playhead position
            if (new_event != std::nullopt) {
                if (new_event->isFirstBufferEvent() || new_event->isPlaybackStoppedEvent()) {
                    last_generation_phead = -2; // reset playhead
                    if (new_event->isFirstBufferEvent()) {
                        groove_hits = torch::zeros({1, 32, 1}, torch::kFloat32);
                        groove_velocities = torch::zeros({1, 32, 1}, torch::kFloat32);
                        groove_offsets = torch::zeros({1, 32, 1}, torch::kFloat32);
                        groove_hit_registrations = torch::ones({1, 32, 1}, torch::kFloat32) * -1.0f; // ppq at which the hit was registered
                        encodeGroove(true);
                        displayGroove();
                        displayPatternAt(latent_g, "MidiVisualizerGroove2Drum");
                        groove_changed = true;
                    }
                }

                if (new_event->isNewBufferEvent()) {
                    auto new_phead = new_event->Time().inQuarterNotes();

                    // clear step as soon as entered if record on but not overdub
                    auto new_phead_step = mapPPQToTimeStep(new_phead);
                    if (abs(new_phead_step - phead_step) > 0.8f) {  // if not zero, moved to a new step
                        phead_step = new_phead_step;

                        // Rotate hit_memory_buffer to the right and prepend 0
                        std::rotate(hit_memory_buffer.rbegin(), hit_memory_buffer.rbegin() + 1, hit_memory_buffer.rend());
                        hit_memory_buffer[0] = 0;
                        if (std::abs(phead - groove_hit_registrations[0][phead_step][0].item<float>()) >= memory_2bars) {
                            if (groove_hits[0][phead_step][0].item<float>() > 0) {
                                groove_hits[0][phead_step][0] = 0;
                                groove_velocities[0][phead_step][0] = 0;
                                groove_offsets[0][phead_step][0] = 0;
                                groove_hit_registrations[0][phead_step][0] = -1.0f;
                                groove_changed = true;
                            }
                        }
                    }
                    phead_step = new_phead_step;

                    phead = new_phead;
                }
            }

            if (new_event->isNoteOnEvent()) {
                // add to memory buffer
                hit_memory_buffer[0] = 1;

                auto ppq  = new_event->Time().inQuarterNotes(); // time in ppq
                auto velocity = new_event->getVelocity(); // velocity
                auto div = round(ppq / .25f);
                auto offset = (ppq - (div * .25f)) / 0.125 * 0.5 ;
                auto grid_index = mapPPQToTimeStep(new_event->Time().inQuarterNotes());

                // check if louder if overlapping
                if (groove_hits[0][grid_index][0].item<float>() > 0 &&
                    std::abs(groove_hit_registrations[0][grid_index][0].item<float>() - phead)  <= 0.25) {
                    if (groove_velocities[0][grid_index][0].item<float>() < velocity) {
                        groove_velocities[0][grid_index][0] = velocity;
                        groove_offsets[0][grid_index][0] = offset;
                        groove_hit_registrations[0][grid_index][0] = phead;
                        groove_changed = true;
                    }
                } else {
                    groove_hits[0][grid_index][0] = 1;
                    groove_velocities[0][grid_index][0] = velocity;
                    groove_offsets[0][grid_index][0] = offset;
                    groove_hit_registrations[0][grid_index][0] = phead;
                    groove_changed = true;
                }

            }

            // audio input groove
            if (ddc_onset_loaded){
                while (queued_audio_buffer.queue.getNumReady() > 0) {
                    // access samples to process
                    audio_frame = queued_audio_buffer.queue.pop();
                    auto times = queued_audio_buffer.times_queue.pop();

                    // queued_audio_buffer.retrieve(audio_frame);
                    std::vector<float> audioData;
                    audioData.reserve(audio_frame.getNumSamples());
                    audioData.insert(audioData.end(), audio_frame.getReadPointer(0), audio_frame.getReadPointer(0) + audio_frame.getNumSamples());

                    // pass through ddc_onset_model
                    std::vector<torch::jit::IValue> in;
                    in.emplace_back(torch::tensor(audioData, torch::kFloat32));
                    // add difficulty level
                    in.emplace_back(4);
                    auto onset_saliences = ddc_onset_model.forward(in);
                    // print the saliences
                    auto saliences = onset_saliences.toTensor().flatten();
                    // get argmax of saliences
                    auto max_salience_ix = saliences.argmax().item<int>();
                    auto salience = saliences[max_salience_ix].item<float>();

                    if (salience > 0.05) {
                        // add to memory buffer
                        hit_memory_buffer[0] = 1;

                        auto ppq  = times[max_salience_ix * 441]; // time in ppq
                        auto velocity = salience; // velocity
                        auto div = round(ppq / .25f);
                        auto offset = (ppq - (div * .25f)) / 0.125 * 0.5 ;
                        auto grid_index = mapPPQToTimeStep(ppq);

                        // check if louder if overlapping
                        if (groove_hits[0][grid_index][0].item<float>() > 0 &&
                            std::abs(groove_hit_registrations[0][grid_index][0].item<float>() - ppq) <= 0.25) {
                            if (groove_velocities[0][grid_index][0].item<float>() < velocity) {
                                groove_hits[0][grid_index][0] = 1;
                                groove_velocities[0][grid_index][0] = velocity;
                                groove_offsets[0][grid_index][0] = offset;
                                groove_hit_registrations[0][grid_index][0] = ppq;
                                groove_changed = true;
                            }
                        } else {
                            groove_hits[0][grid_index][0] = 1;
                            groove_velocities[0][grid_index][0] = velocity;
                            groove_offsets[0][grid_index][0] = offset;
                            groove_hit_registrations[0][grid_index][0] = ppq;
                            groove_changed = true;
                        }
                    }
                }
            }

            // check if adaptive memory is active
            if (gui_params.getValueFor("Adaptive Memory") > 0.5f) {
                // if groove_density of hits over time is less than 0.1, then set memory to 2 bars
                auto density_ = std::accumulate(hit_memory_buffer.begin(), hit_memory_buffer.end(), 0.0f) / 14.0f;
                density_ = std::clamp(density_, 0.0f, 1.0f);
                auto old_mem = gui_params.getValueFor("Memory");
                float mem_max_allowed = 0.6f;
                auto new_mem = (1 - density_) * mem_max_allowed;

                if (abs(new_mem - old_mem) > 0.05f) {
                    gui_params.setValueFor("Memory", new_mem);
                }

            }

        }

        // scale the groove if new note or new groove params
        if (groove_changed || new_preset) {
            scaleGroove();

            // stack up the groove information into a single tensor
            groove_hvo_scaled = torch::concat(
                {
                    groove_hits,
                    groove_velocities_scaled,
                    groove_offsets_scaled
                }, 2);

        }

        // re-encode the groove if groove changed or groove_density changed or new preset
        if (groove_changed || density_changed || new_preset) {
            // track the groove
            // track the groove info
            CustomPresetData->tensor("groove_hits", groove_hits);
            CustomPresetData->tensor("groove_velocities", groove_velocities);
            CustomPresetData->tensor("groove_offsets", groove_offsets);
            displayGroove();

            return true;
        } else {
            return false;
        }
    }

    bool latentAutoMoved(std::optional<EventFromHost>& new_event){

        // check if AutoMove button is enabled
        if (gui_params.wasParamUpdated("AB Oscillation Rate")) {
            auto_move_enabled = gui_params.getValueFor("AB Oscillation Rate") > 0.3f;
            if (auto_move_enabled) {
                auto val = gui_params.getValueFor("AB Oscillation Rate");
                auto_move_rate = slowest_auto_move_rate + (fastest_auto_move_rate - slowest_auto_move_rate) * (val-0.3f) / (10 - 0.3f);
                interpolate_slider_value = gui_params.getValueFor("Interpolate");
            }
        }


        if (auto_move_enabled && new_event->isNewBufferEvent()) {
            auto new_phead = new_event->Time().inQuarterNotes();
            if (abs(new_phead - last_move_ppq) > auto_move_rate) {
                last_move_ppq = (float) new_phead;
                if (autoMoveGoingToA) {
                    interpolate_slider_value -= auto_move_step;
                    if (interpolate_slider_value <= 0.0f) {
                        autoMoveGoingToA = false;
                        interpolate_slider_value = 0.0f;
                    }
                } else {
                    interpolate_slider_value+= auto_move_step;
                    if (interpolate_slider_value >= 1.0f) {
                        autoMoveGoingToA = true;
                        interpolate_slider_value = 1.0f;
                    }
                }
                gui_params.setValueFor("Interpolate", (float)interpolate_slider_value);
                return true;
            }
        }

        return false;
    }

    // encodes the groove into a latent vector using the encoder
    bool encodeGroove(bool force_encode = false) {

        if ((abs(phead - last_generation_phead) >= (responsiveness)) || force_encode ) {
            if ((responsiveness > 0 && fmod(int(phead), responsiveness) != 0.0f) && !force_encode) {
                // if it's not time to encode, return (depends on responsiveness)
                return false;
            }

            last_generation_phead = phead;
            // preparing the input to encode() method
            std::vector<torch::jit::IValue> enc_inputs;
            enc_inputs.emplace_back(groove_hvo_scaled);

            // get the encode method
            auto encode = model.get_method("encode_all");

            // encode the input
            auto encoder_output = encode(enc_inputs);

            // get latent vector from encoder output
            latent_g = encoder_output.toTuple()->elements()[0].toTensor();

            // display the groove
            displayPatternAt(latent_g, "MidiVisualizerGroove2Drum");
            displayPatternAt(latent_A, "MidiVisualizerA");
            displayPatternAt(latent_B, "MidiVisualizerB");

            return true;
        } else {
            return false;
        }

    }

    static float getMidPointOfVelocities(at::Tensor& t) {
        vector<float> v;
        for (int i = 0; i < 32; i++) {
            if (t[0][i][0].item<float>() > 0) {
                v.push_back(t[0][i][0].item<float>());
            }
        }
        if (v.size() < 1) {
            return -1.0f;
        } else {
            auto min = *std::min_element(v.begin(), v.end());
            auto max = *std::max_element(v.begin(), v.end());
            return (min + max) / 2.0f;
        }
    }

    // scales the groove according to the velocity and offset params
    void scaleGroove() {
        // scale the velocity

        // use distance from 1 and inverse exponentially apply gain
        // those closer to 1 will be slower to change
        // expand lower and upper ranges
        // scale using a sigmoid
        double mapped_slider_val;

        if (vel_scaler_val > 1) {
            mapped_slider_val = 1.0 + (vel_scaler_val - 1.0) * 2.0;
        } else {
            mapped_slider_val = vel_scaler_val / 2.0;
        }
        auto gain = 2.0 / (1.0 + exp(-(vel_scaler_val - 1)));

        groove_velocities_scaled = torch::pow(gain,
                                                  pow(0.8+abs(1.0 - groove_velocities), 3));
        groove_velocities_scaled = groove_velocities_scaled * groove_velocities;
        groove_velocities_scaled = torch::clamp(groove_velocities_scaled, 0.0f, 1.0f);

        // invert groove
//        if (groove_inversion > 0) {
//            // get non-zero velocity values
//            auto non_zero_vels = torch::nonzero(groove_velocities_scaled);
//            auto mid_point = getMidPointOfVelocities(groove_velocities_scaled);
//            if (mid_point > 0) {
//                groove_velocities_scaled = (mid_point - groove_velocities_scaled) * groove_inversion + groove_velocities_scaled;
//            }
//        }

        groove_offsets_scaled = groove_offsets * (1.0 - off_quantizer_val);
    }

    // update any variables that impact the latent vector
    bool updateLatents(bool new_preset, bool groove_re_encoded) {

        bool changed = false;

        // check if buttons were clicked to randomize the pattern
        auto ButtonATriggered = gui_params.wasButtonClicked("Random A");
        if (ButtonATriggered) {
            // use latent_means and latent stds to generate latent_A
            /*auto eps = torch::randn_like(latent_means);
            latent_A = eps.mul(latent_stds).add(latent_means);*/
            latent_A = torch::randn({1, latent_dim}, torch::kFloat32);
            CustomPresetData->tensor("latent_A", latent_A);
            displayPatternAt(latent_A, "MidiVisualizerA");
            changed = true;
        }

        // check if snap here button was clicked
        auto ButtonSnapHereTriggered = gui_params.wasButtonClicked("Snap To A");
        if (ButtonSnapHereTriggered) {
            latent_A = latent_vector;
            CustomPresetData->tensor("latent_A", latent_A);
            gui_params.setValueFor("Interpolate", 0.0f);
            gui_params.setValueFor("Follow", 0.0f);
            displayPatternAt(latent_A, "MidiVisualizerA");
            changed = true;
        }

        // check if buttons were clicked to randomize the pattern
        auto ButtonBTriggered = gui_params.wasButtonClicked("Random B");
        if (ButtonBTriggered) {
            /*auto eps = torch::randn_like(latent_means);
            latent_B = eps.mul(latent_stds).add(latent_means);*/
            latent_B = torch::randn({1, latent_dim}, torch::kFloat32);
            CustomPresetData->tensor("latent_B", latent_B);
            changed = true;
            displayPatternAt(latent_B, "MidiVisualizerB");

        }

        // check if snap here button was clicked
        auto ButtonSnapHereBTriggered = gui_params.wasButtonClicked("Snap To B");
        if (ButtonSnapHereBTriggered) {
            latent_B = latent_vector;
            CustomPresetData->tensor("latent_B", latent_B);
            gui_params.setValueFor("Interpolate", 1.0f);
            gui_params.setValueFor("Follow", 0.0f);
            changed = true;
            displayPatternAt(latent_B, "MidiVisualizerB");
        }

        // check if Triangle Area touched manually (if so, disable auto-follow)
        if(gui_params.isTriangleSliderDragged("Interpolation Area"))
        {
            gui_params.setValueFor("Adaptive Follow", 0.0f);
            gui_params.setValueFor("AB Oscillation Rate", 0.0f);
            auto_move_enabled = false;
            changed = true;
        }

        // get interpolate slider value if changed
        if (gui_params.wasParamUpdated("Interpolate") || changed) {
            interpolate_slider_value = gui_params.getValueFor("Interpolate");
            changed = true;
        }

        // get interpolate follow value if changed
        if (gui_params.wasParamUpdated("Follow") || changed) {
            follow_slider_value = gui_params.getValueFor("Follow");
            changed = true;
        }

        // re-calculates the latent vector if any of the above changed
        if (changed || new_preset || groove_re_encoded) {

            // use interpolate slider value to interpolate between A and B
            latent_vector = (1.0f - interpolate_slider_value) * latent_A + interpolate_slider_value * latent_B;

            // use follow slider value to interpolate between A and B
            latent_vector = (1 - follow_slider_value) * latent_vector + follow_slider_value * latent_g;

            return true;
        }
        return false;
    }

    // check responsiveness buttons
    bool checkResponsivenessValuesFromButtons()
    {
        // "Immediate" button -> responsiveness slider = 0
        // "1 Bar" button -> responsiveness slider = 1
        // "2 Bar" button -> responsiveness slider = 2

        bool changed = false;

        if (gui_params.wasButtonClicked("Immediate")) {
            changed = true;
            if (gui_params.isToggleButtonOn("Immediate")) {
                responsiveness = 0;    // 0 ppq
                gui_params.setValueFor("1 Bar", 0.0f);
                gui_params.setValueFor("2 Bar", 0.0f);
            }
        } else if (gui_params.wasButtonClicked("1 Bar")) {
            changed = true;
            if (gui_params.isToggleButtonOn("1 Bar")) {
                responsiveness = 4;     // ppqs
                gui_params.setValueFor("Immediate", 0.0f);
                gui_params.setValueFor("2 Bar", 0.0f);
            }
        } else if (gui_params.wasButtonClicked("2 Bar")) {
            changed = true;
            if (gui_params.isToggleButtonOn("2 Bar")) {
                responsiveness = 8;     // ppqs
                gui_params.setValueFor("Immediate", 0.0f);
                gui_params.setValueFor("1 Bar", 0.0f);
            }
        }

        if (changed) {
            // check if all turned off
            bool none_selected = true;
            for (int i = 0; i < 3; i++) {
                if (gui_params.isToggleButtonOn("Immediate") ||
                    gui_params.isToggleButtonOn("1 Bar") ||
                    gui_params.isToggleButtonOn("2 Bar")) {
                    none_selected = false;
                    break;
                }
            }
            if (none_selected) {
                // get current genre
                gui_params.setValueFor("1 Bar", 1.0f);
            }
        }

        return changed;

    }

    // update selected genre
    bool updateGenreSelection() {

        int selection_ix = -1;
        bool at_least_one_changed = false;
        for (int i = 0; i < num_genres; i++) {
            if (gui_params.wasParamUpdated("Genre " + std::to_string(i+1))) {
                at_least_one_changed = true;
                if (gui_params.getValueFor("Genre " + std::to_string(i+1)) > 0.5f) {
                    selection_ix = i;
                    break;
                }
            }
        }

        // check if all turned off
        if (at_least_one_changed && selection_ix == -1) {
            // check if all turned off
            bool none_selected = true;
            for (int i = 0; i < num_genres; i++) {
                if (gui_params.getValueFor("Genre " + std::to_string(i+1)) > 0.5f) {
                    none_selected = false;
                    break;
                }
            }
            if (none_selected) {
                // get current genre
                selection_ix = std::max(0, current_genre.item<int>());
                gui_params.setValueFor("Genre " + std::to_string(selection_ix+1), 1.0f);
            }

        }

        if (selection_ix >= 0) { // if a new genre is selected
            current_genre = torch::tensor(selection_ix, torch::kLong);
            // set all others to 0
            for (int i = 0; i < num_genres; i++) {
                if (i != selection_ix) {
                    gui_params.setValueFor("Genre " + std::to_string(i+1), 0.0f);
                }
            }
        }

        return selection_ix >= 0;
    }

    // update per voice midi mapping
    bool updateVoiceMapping() {
        bool changed = false;

        // --------------------- Voice Map ---------------------
        if (gui_params.wasParamUpdated("Kick")) {
            voiceMap[0] = int(gui_params.getValueFor("Kick"));
            changed = true;
        }
        if (gui_params.wasParamUpdated("Snare")) {
            voiceMap[1] = int(gui_params.getValueFor("Snare"));
            changed = true;
        }
        if (gui_params.wasParamUpdated("ClosedHat")) {
            voiceMap[2] = int(gui_params.getValueFor("ClosedHat"));
            changed = true;
        }
        if (gui_params.wasParamUpdated("OpenHat")) {
            voiceMap[3] = int(gui_params.getValueFor("OpenHat"));
            changed = true;
        }
        if (gui_params.wasParamUpdated("LowTom")) {
            voiceMap[4] = int(gui_params.getValueFor("LowTom"));
            changed = true;
        }
        if (gui_params.wasParamUpdated("MidTom")) {
            voiceMap[5] = int(gui_params.getValueFor("MidTom"));
            changed = true;
        }
        if (gui_params.wasParamUpdated("HighTom")) {
            voiceMap[6] = int(gui_params.getValueFor("HighTom"));
            changed = true;
        }
        if (gui_params.wasParamUpdated("Crash")) {
            voiceMap[7] = int(gui_params.getValueFor("Crash"));
            changed = true;
        }
        if (gui_params.wasParamUpdated("Ride")) {
            voiceMap[8] = int(gui_params.getValueFor("Ride"));
            changed = true;
        }

        return changed;
    }

    // utility for mapping a single pot value to a max count/thresh of a single voice
    static juce::Point<float> mapDensitySlider(float slider_value) {

        juce::Point<float> return_value;
        juce::Point<float> point1 = {0.0f, 0.99};   // slider = 0
        juce::Point<float> point2 = {32, 0.5};      // slider = 1
        juce::Point<float> point3 = {32, 0.05};     // slider = 2

        if (slider_value < 1.0f) {
            return_value = point1 + (point2 - point1) * slider_value;
        } else if (slider_value >= 1.0f) {
            return_value = point2 + (point3 - point2) * (slider_value - 1.0f);
        }

        return return_value;
    }

    // update density selections per voice
    bool check_redirect_buttons_and_density_slider_status() {
        bool changed = false;

        if (gui_params.wasButtonClicked("Redirect Kicks") ) {
            changed = true;
            if (gui_params.isToggleButtonOn("Redirect Kicks")) {
                kick_is_redirected = torch::tensor(1, torch::kLong);
                gui_params.setValueFor("Mute Kicks", 1.0f);
            } else {
                kick_is_redirected = torch::tensor(0, torch::kLong);
                gui_params.setValueFor("Mute Kicks", 0.0f);
            }
        }

        if (gui_params.wasButtonClicked("Redirect Snares")) {
            changed = true;
            if (gui_params.isToggleButtonOn("Redirect Snares")) {
                snare_is_redirected = torch::tensor(1, torch::kLong);
                gui_params.setValueFor("Mute Snares", 1.0f);
            } else {
                snare_is_redirected = torch::tensor(0, torch::kLong);
                gui_params.setValueFor("Mute Snares", 0.0f);
            }
        }

        if (gui_params.wasButtonClicked("Redirect Hats")) {
            changed = true;
            if (gui_params.isToggleButtonOn("Redirect Hats")) {
                hats_is_redirected = torch::tensor(1, torch::kLong);
                gui_params.setValueFor("Mute Hats", 1.0f);
            } else {
                hats_is_redirected = torch::tensor(0, torch::kLong);
                gui_params.setValueFor("Mute Hats", 0.0f);
            }
        }

        if (gui_params.wasButtonClicked("Redirect Toms")) {
            changed = true;
            if (gui_params.isToggleButtonOn("Redirect Toms")) {
                toms_is_redirected = torch::tensor(1, torch::kLong);
                gui_params.setValueFor("Mute Toms", 1.0f);
            } else {
                toms_is_redirected = torch::tensor(0, torch::kLong);
                gui_params.setValueFor("Mute Toms", 0.0f);
            }
        }

        if (gui_params.wasButtonClicked("Redirect Cymbals")) {
            changed = true;
            if (gui_params.isToggleButtonOn("Redirect Cymbals")) {
                cymbals_is_redirected = torch::tensor(1, torch::kLong);
                gui_params.setValueFor("Mute Cymbals", 1.0f);
            } else {
                cymbals_is_redirected = torch::tensor(0, torch::kLong);
                gui_params.setValueFor("Mute Cymbals", 0.0f);
            }
        }

        return changed;
    }

    // update voice thresholds and max counts per voice
    bool updatePerVoiceSamplingParams(){
        bool changed = false;
        changed = check_redirect_buttons_and_density_slider_status();

        if (gui_params.wasParamUpdated("Generation Quantization")) {
            generation_off_quantizer_val = gui_params.getValueFor("Generation Quantization");
            changed = true;
        }

        if (gui_params.wasParamUpdated("Mute Kicks")) {
            changed = true;
            if (gui_params.getValueFor("Mute Kicks") > 0.5f) {
                max_counts_allowed[0] = 0;
                voice_thresholds[0] = 0.99f;
            } else {
                max_counts_allowed[0] = 32;
                voice_thresholds[0] = 0.51f;
            }
        }

        if (gui_params.wasParamUpdated("Mute Snares")) {
            changed = true;
            if (gui_params.getValueFor("Mute Snares") > 0.5f) {
                max_counts_allowed[1] = 0;
                voice_thresholds[1] = 0.99f;
            } else {
                max_counts_allowed[1] = 32;
                voice_thresholds[1] = 0.47f;
            }
        }

        if (gui_params.wasParamUpdated("Mute Hats")) {
            changed = true;
            if (gui_params.getValueFor("Mute Hats") > 0.5f) {
                max_counts_allowed[2] = 0;
                voice_thresholds[2] = 0.99f;
            } else {
                max_counts_allowed[2] = 32;
                voice_thresholds[2] = 0.5f;
            }
        }

        if (gui_params.wasParamUpdated("Mute Toms")) {
            changed = true;
            if (gui_params.getValueFor("Mute Toms") > 0.5f) {
                max_counts_allowed[4] = 0;
                max_counts_allowed[5] = 0;
                max_counts_allowed[6] = 0;
                voice_thresholds[4] = 0.99f;
                voice_thresholds[5] = 0.99f;
                voice_thresholds[6] = 0.99f;
            } else {
                max_counts_allowed[4] = 32;
                max_counts_allowed[5] = 32;
                max_counts_allowed[6] = 32;
                voice_thresholds[4] = 0.5f;
                voice_thresholds[5] = 0.5f;
                voice_thresholds[6] = 0.5f;
            }
        }

        if (gui_params.wasParamUpdated("Mute Cymbals")) {
            changed = true;
            if (gui_params.getValueFor("Mute Cymbals") > 0.5f) {
                max_counts_allowed[7] = 0;
                max_counts_allowed[8] = 0;
                voice_thresholds[7] = 0.99f;
                voice_thresholds[8] = 0.99f;
            } else {
                max_counts_allowed[7] = 32;
                max_counts_allowed[8] = 32;
                voice_thresholds[7] = 0.5f;
                voice_thresholds[8] = 0.5f;
            }
        }

        return changed;
    }

    // update any variables that impact the voice map
    bool updateVelocityScalingMask() {

        bool changed = false;

        // --------------------- Voice Thresholds / Max Counts / Velocities ---------------------
        // get voice thresholds if changed
        if (gui_params.wasParamUpdated("K Vel")) {
            auto vel = gui_params.getValueFor("K Vel");
            velocity_mask.index_put_(
                {torch::indexing::Slice(), torch::indexing::Slice(), 0},
                torch::ones({32}, torch::kFloat32) * vel);
            changed = true;
        }
        if (gui_params.wasParamUpdated("S Vel")) {
            auto vel = gui_params.getValueFor("S Vel");
            velocity_mask.index_put_(
                {torch::indexing::Slice(), torch::indexing::Slice(), 1},
                torch::ones({32}, torch::kFloat32) * vel);
            changed = true;
        }
        if (gui_params.wasParamUpdated("CH Vel")) {
            auto vel = gui_params.getValueFor("CH Vel");
            velocity_mask.index_put_(
                {torch::indexing::Slice(), torch::indexing::Slice(), 2},
                torch::ones({32}, torch::kFloat32) * vel);
            changed = true;
        }
        if (gui_params.wasParamUpdated("OH Vel")) {
            auto vel = gui_params.getValueFor("OH Vel");
            velocity_mask.index_put_(
                {torch::indexing::Slice(), torch::indexing::Slice(), 3},
                torch::ones({32}, torch::kFloat32) * vel);
            changed = true;
        }
        if (gui_params.wasParamUpdated("Toms Vel")) {
            auto vel = gui_params.getValueFor("Toms Vel");
            velocity_mask.index_put_(
                {torch::indexing::Slice(), torch::indexing::Slice(), torch::indexing::Slice(4, 7)},
                torch::ones({32, 3}, torch::kFloat32) * vel);
            changed = true;
        }
        if (gui_params.wasParamUpdated("Crash Vel")) {
            auto vel = gui_params.getValueFor("Crash Vel");
            velocity_mask.index_put_(
                {torch::indexing::Slice(), torch::indexing::Slice(), 7},
                torch::ones({32}, torch::kFloat32) * vel);
            changed = true;
        }
        if (gui_params.wasParamUpdated("Ride Vel")) {
            auto vel = gui_params.getValueFor("Ride Vel");
            velocity_mask.index_put_(
                {torch::indexing::Slice(), torch::indexing::Slice(), 8},
                torch::ones({32}, torch::kFloat32) * vel);
            changed = true;
        }

        return changed;
    }

    void decodeLatent(at::Tensor& latent_v, at::Tensor& return_h, at::Tensor& return_v, at::Tensor& return_o) {
        // Prepare above for inference
        std::vector<torch::jit::IValue> inputs;
        inputs.emplace_back(latent_v);
        inputs.emplace_back(current_genre);
        inputs.emplace_back(kick_is_redirected);
        inputs.emplace_back(snare_is_redirected);
        inputs.emplace_back(hats_is_redirected);
        inputs.emplace_back(toms_is_redirected);
        inputs.emplace_back(cymbals_is_redirected);
        inputs.emplace_back(voice_thresholds);
        inputs.emplace_back(max_counts_allowed);
        // inputs.emplace_back(sampling_mode);

        // Get the scripted method
        auto sample_method = model.get_method("sample");

        // Run inference
        auto output = sample_method(inputs);

        // Extract the generated tensors from the output
        return_h = output.toTuple()->elements()[0].toTensor();
        return_v = output.toTuple()->elements()[1].toTensor();
        return_o = output.toTuple()->elements()[2].toTensor();

        /*// Extract the generated tensors from the output
        return_h = torch::zeros({1, 32, 9}, torch::kFloat32);
        return_h = output.toTuple()->elements()[3].toTensor(); // get logits first
        return_v = output.toTuple()->elements()[1].toTensor();
        return_o = output.toTuple()->elements()[2].toTensor();

        for (int j = 0; j < 9; j++) {
            // get the max count for this voice (only above threshold)
            auto max_count = max_counts_allowed[j];
            auto threshold = voice_thresholds[j];

        }

        for(int64_t ix = 0; ix < voice_thresholds.size(0); ++ix) {
            auto thres = voice_thresholds[ix].item<float>(); // Assuming voice_thresholds is a tensor of floats
            auto max_count = max_counts_allowed[ix].item<int64_t>(); // Assuming voice_max_count_allowed is a tensor of int64

            // Get the top 'max_count' values and their indices from '_h' in the dimension 1
            auto topk = return_h.index({"...", torch::indexing::Slice(), ix}).topk(max_count, 1);
            auto max_indices = std::get<1>(topk);

            // Update 'h' with values from '_h' where the max_indices are located
            return_h.index_put_({"...", max_indices, ix}, return_h.index({"...", max_indices, ix}));

            // Apply the threshold to 'h'
            auto mask = return_h.index({"...", torch::indexing::Slice(), ix}) > thres;
            return_h.index_put_({"...", torch::indexing::Slice(), ix}, torch::where(mask, 1, 0).to(return_h.dtype()));
        }*/
    }

    // extracts the generated pattern into a PlaybackSequence
    void preparePlaybackSequence() {
        if (!playback_hits.sizes().empty()) // check if any hits are available
        {
            // clear playback sequence
            playbackSequence.clear();

            // Add 9 notes at ppq 8.25 to ensure that display is consistent
            // this wouldn't impact the actual playback --> just for MidiOutVisualizer
            for (int i = 0; i < 9; i++) {
                playbackSequence.addNoteWithDuration(
                    10, voiceMap[i], 0.05f, 8.25f, 0.1f);
            }

            // iterate through all voices, and time steps
            int batch_ix = 0;
            for (int step_ix = 0; step_ix < 32; step_ix++)
            {
                for (int voice_ix = 0; voice_ix < 9; voice_ix++)
                {

                    // check if the voice is active at this time step
                    if (playback_hits[batch_ix][step_ix][voice_ix].item<float>() > 0.5)
                    {
                        auto midi_num = voiceMap[voice_ix];
                        auto velocity = playback_velocities[batch_ix][step_ix][voice_ix].item<float>();
                        velocity = velocity * velocity_mask[batch_ix][step_ix][voice_ix].item<float>();
                        velocity = std::clamp(velocity, 0.0f, 1.0f);
                        auto offset =
                            playback_offsets[batch_ix][step_ix][voice_ix].item<float>() * (1.0 - generation_off_quantizer_val);
                        // we are going to convert the onset time to a ratio of quarter notes
                        auto time = (step_ix + offset) * 0.25f;
                        auto duration = 0.1f;

                        if (time < 0.0f) { time += 8.0f;}
                        if (velocity > 0.0f) {
                            playbackSequence.addNoteWithDuration(
                                10, midi_num, velocity, time, duration);
                        }

                    }
                }
            }
        }
    }

    // prepares the playback policy
    void preparePlaybackPolicy() {
        // Specify the playback policy
        playbackPolicy.SetPlaybackPolicy_RelativeToAbsoluteZero();
        playbackPolicy.SetTimeUnitIsPPQ();
        playbackPolicy.SetOverwritePolicy_DeleteAllEventsInPreviousStreamAndUseNewStream(true);
        playbackPolicy.ActivateLooping(8);
    }

    // ---------------------- Visualization Utils ----------------------
    void displayGroove() {
        midiVisualizersData->clear_visualizer_data("MidiVisualizerGroove");
        for (int i = 0; i < 32; i++) {
            if (groove_hits[0][i][0].item<float>() > 0.5) {
                auto vel = groove_velocities_scaled[0][i][0].item<float>();
                auto offset = groove_offsets_scaled[0][i][0].item<float>();
                auto time = (i + offset) * 0.25f;
                if (time < 0.0f) { time += 8.0f;}
                if (vel > 0.02f) {
                    midiVisualizersData->displayNoteWithDuration("MidiVisualizerGroove", 32, vel, time, 0.1f);
                }
            }
        }
    }

    void displayPatternAt(at::Tensor& latent_v, string visualizer_name) {
        decodeLatent(latent_v, display_hits, display_velocities, display_offsets);
        display_offsets_scaled = display_offsets * (1.0 - generation_off_quantizer_val);
        midiVisualizersData->clear_visualizer_data(visualizer_name);

        for (int i = 0; i < 32; i++) {
            for (int j = 0; j < 9; j++) {
                if (display_hits[0][i][j].item<float>() > 0.5) {
                    auto vel = display_velocities[0][i][j].item<float>();
                    vel = vel * velocity_mask[0][i][j].item<float>();
                    vel = std::clamp(vel, 0.0f, 1.0f);
                    auto offset = display_offsets_scaled[0][i][j].item<float>();
                    auto time = (i + offset) * 0.25f;
                    if (time < 0.0f) { time += 8.0f;}
                    if (vel > 0.02f)
                    {
                        midiVisualizersData->displayNoteWithDuration(
                            visualizer_name, display_voice_maps[j], vel, time, 0.125f);
                    }
                }
            }
        }
    }

    void updateAllDisplayPatterns() {
        displayPatternAt(latent_A, "MidiVisualizerA");
        displayPatternAt(latent_B, "MidiVisualizerB");
        displayPatternAt(latent_g, "MidiVisualizerGroove2Drum");
    }

    // ---------------------- Utility Functions ----------------------
    // maps the playhead position to a time step
    static int mapPPQToTimeStep(double phead_) {
       /* step 0: 0.0 - 0.125 || 7.875 - 8.0
        * step 1: 0.125 - .375
        * step 2: 0.375 - 0.625
        * ...
        * step 31: 7.625 - 7.875
        * */
        phead_ = fmod(phead_, 8.0f);
        if ( 0.125f < phead_ && phead_ < 7.875f) {
            phead_ = phead_ - 0.125f;
            return int(phead_ / 0.25f) + 1;
        } else {
            return 0;
        }
    }
};