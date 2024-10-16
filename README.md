# audio_player
This project is an audio player developed using Embedded C on the Altera MAX10 FPGA platform. It demonstrates the functionality of playing audio 
files stored on an external memory device (SD card) and outputs audio through a digital-to-analog converter (DAC). The project is 
optimized for low-latency audio playback and uses custom hardware interfaces implemented in Verilog to handle audio data streaming and signal 
processing.

# phase_2
This phase involves interfacing the FatFS Buffer to the Audio interface output FIFO's. A properly sized buffer was implemented to get audio
WAV files to play without distortion. Basic audio processing including two slide switches was implemented to allow different playback modes.

# phase_3
It is very important in the embedded world of design that the CPU is unburdened from menial tasks that other simpler hardware IP Cores can 
do. This is the case for the need to de-bounce pushbutton signals where a TIMER can be used to create a time delay.

# phase_4
Key Features:
Audio Playback: Play .wav files from an SD card without distortion.
Pushbutton Control: Debounced pushbutton input to navigate and control playback.
File Handling: Extract and use portions of the FatFS file system to list and handle .wav files.
isWav(): Identifies if a file is a .wav.
Song Indexing: Stores .wav filenames and sizes in arrays.
LCD Display: Displays filenames, SD index, and playback mode. Pushbuttons allow cycling through files.
