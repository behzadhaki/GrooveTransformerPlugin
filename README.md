
# VST Implementation of the GrooveTransformer Eurorack Module
----



![image](https://github.com/behzadhaki/GrooveTransformerEurorackPlugin/assets/35939495/7e3b2b8e-e21c-4f32-9b30-9c9da3449ec8)


# Parameters

| Section                   | Parameter        | Description                               | Type          |
|---------------------------|------------------|-------------------------------------------|---------------|
| **A/B States**            | "Random A"       | Randomizes Latent A                       | Button        |
|                           | "Snap to A"      | Snap Current latent to A                  | Button        |
|                           | "Random B"       | Randomizes Latent B                       | Button        |
|                           | "Snap to B"      | Snap Current latent to B                  | Button        |
|                           | "Interpolate"    | Slider for interpolation between A and B  | HSlider        |
| **Groove Recording Controls**| "Clear Groove" | Clears the input groove                   | Button        |
|                           | "Record"         | Enables groove update                     | Toggle Button |
|                           | "Overdub"        | Overdubs incoming notes on existing groove| Toggle Button |
| **Groove Manipulation Controls**| "Groove Offset"| Offset Quantization                    | Rotary              |
|                           | "Groove Velocity"| Velocity Scaling of Groove                | Rotary               |
| **Generation Parameters** | "Follow"         | Follow amount                             | Rotary              |
|                           | "Uncertainty"    | Temperature of sampling                   | Rotary              |
|                           | "Kick Density"   | Sampling Threshold and Max Count for Kick | HSlider              |
|                           | "Kick Vel"       | Velocity gain of Kick                     | HSlider |
|                           | "Snare Density"  | Sampling Threshold and Max Count for Snare| HSlider |
|                           | "Snare Vel"      | Velocity gain of Snare                    | HSlider |
|                           | "CH Density"     | Sampling Threshold and Max Count for CH   | HSlider |
|                           | "CH Vel"         | Velocity gain of CH                       | HSlider |
|                           | "OH Density"     | Sampling Threshold and Max Count for OH   | HSlider |
|                           | "OH Vel"         | Velocity gain of OH                       | HSlider |
|                           | "Toms Density"   | Sampling Threshold and Max Count for Toms | HSlider |
|                           | "Toms Vel"       | Velocity gain of Toms                     | HSlider |
|                           | "Crash Density"  | Sampling Threshold and Max Count for Crash| HSlider |
|                           | "Crash Vel"      | Velocity gain of Crash                    | HSlider |
|                           | "Ride Density"   | Sampling Threshold and Max Count for Ride | HSlider |
|                           | "Ride Vel"       | Velocity gain of Ride                     | HSlider |
| **Midi Visualizers**      | "State A Pattern"|                                           | MIDIVisualizer |
|                           | "State B Pattern"|                                           | MIDIVisualizer |
|                           | "Generation"     |                                           | MIDIVisualizer |





# Parameters
-----

## A/B States
- [ ] "Random  A": Randomizes Latent A (Click Button)
- [ ] "Snap to A": Snap Current latent to A (Click Button)
- [ ] "Random  B": Randomizes Latent B (Click Button)
- [ ] "Snap to B": Snap Current latent to B (Click Button)
- [ ] "Interpolate": Slider for interp. btn. A and B (Slider)

## Groove Recording Controls
- [ ] "Clear Groove": Clears the input groove (Click Button)
- [ ] "Record": Enables groove update (Toggle Button)
- [ ] "Overdub": Overdubs incoming notes on top of the existing groove (Toggle Button)

## Groove Manipulation Controls
- [ ] "Groove Offset": Offset Quantization 
- [ ] "Groove Velocity": Velocity Scaling of Groove

## Generation Parameters
- [ ] "Follow": Follow amount
- [ ] "Uncertainty": Temperature of sampling
- [ ] "Kick Density": Sampling Threshold and Max Count for Kick
- [ ] "Kick Vel": Velocity gain of Kick
- [ ] "Snare Density": Sampling Threshold and Max Count for Snare
- [ ] "Snare Vel": Velocity gain of Snare
- [ ] "CH Density": Sampling Threshold and Max Count for CH
- [ ] "CH Vel": Velocity gain of CH
- [ ] "OH Density": Sampling Threshold and Max Count for OH
- [ ] "OH Vel": Velocity gain of OH
- [ ] "Toms Density": Sampling Threshold and Max Count for Toms
- [ ] "Toms Vel": Velocity gain of Toms
- [ ] "Crash Density": Sampling Threshold and Max Count for Crash
- [ ] "Crash Vel": Velocity gain of Crash
- [ ] "Ride Density": Sampling Threshold and Max Count for Ride
- [ ] "Ride Vel": Velocity gain of Ride

## Midi Visualizers
- [ ] State A Pattern
- [ ] State B Pattern
- [ ] Generation
