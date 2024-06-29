#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>

typedef int           i32;
typedef unsigned long u32;
typedef float         f32;

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define CHANNEL_COUNT 2
#define PA_SLEEP_MS 10 * 1000

#define DISPLAY_SIZE 100

static void check_error(PaError err)
{
  if (err != paNoError)
  {
    printf("portaudio error: %s\n", Pa_GetErrorText(err));
  }
}

static inline f32 max(f32 a, f32 b)
{
  return a > b ? a : b;
}

static inline f32 absolute_value(f32 a)
{
  return a > 0 ? a : -a;
}

static i32 pa_test_callback(const void *input_buffer, 
                            void *output_buffer, 
                            u32 frames_per_buffer, 
                            const PaStreamCallbackTimeInfo *time_info,
                            PaStreamCallbackFlags status_flags,
                            void *user_data)
{
  float *in = (float*)input_buffer;
  (void)output_buffer; // cast to void so compiler do not get angry

  printf("\r"); // carage return
  
  f32 volume_left = 0;
  f32 volume_right = 0;

  for (u32 i = 0; i < frames_per_buffer * 2; i += 2)
  {
    volume_left = max(volume_left, absolute_value(in[i]));
    volume_right = max(volume_right, absolute_value(in[i+1]));
  }

  for (i32 i = 0; i < DISPLAY_SIZE; i++)
  {
    f32 bar_proportion = i / (f32)DISPLAY_SIZE;

    if (bar_proportion <= volume_left && bar_proportion <= volume_right)
    {
      printf("█");
    }
    else if (bar_proportion <= volume_left)
    {
      printf("▀");
    }
    else if (bar_proportion <= volume_right)
    {
      printf("▄");
    }
    else 
    {
      printf(" ");
    }
  }

  fflush(stdout);

  return 0;
}

i32 main(i32 argc, char **argv)
{
  /* initalize port audio */
  PaError err;
  err = Pa_Initialize();
  check_error(err);

  i32 num_devices = Pa_GetDeviceCount();
  printf("number of devices: %d\n", num_devices);
  if (num_devices < 0)
  {
    printf("error getting device count\n");
    exit(EXIT_FAILURE);
  }
  else if (num_devices == 0)
  {
    printf("there are no devices connected\n");
    exit(EXIT_SUCCESS);
  }

  const PaDeviceInfo *device_info;
  for (i32 i = 0; i < num_devices; i++)
  {
    device_info = Pa_GetDeviceInfo(i);
    printf("device %d:\n", i);
    printf("name: %s:\n", device_info->name);
    printf("max input channels: %d\n", device_info->maxInputChannels);
    printf("max output channels: %d\n", device_info->maxOutputChannels);
    printf("default sample rate: %lf\n", device_info->defaultSampleRate);
  }

  /* user input device */
  i32 device;

  printf("select a device: ");
  i32 temp = scanf("%d", &device);

  if (temp != 1)
  {
    printf("incorrect device input\n");
    EXIT_FAILURE;
  }
  else if (device > num_devices)
  {
    printf("selected device does not exist\n");
    EXIT_FAILURE;
  }

  PaStreamParameters input_parameters;
  PaStreamParameters output_parameters;

  memset(&input_parameters, 0, sizeof(input_parameters));
  input_parameters.channelCount = CHANNEL_COUNT; // left and right
  input_parameters.device = device;
  input_parameters.hostApiSpecificStreamInfo = NULL;
  input_parameters.sampleFormat = paFloat32;
  input_parameters.suggestedLatency = Pa_GetDeviceInfo(device)->defaultLowInputLatency;

  memset(&output_parameters, 0, sizeof(output_parameters));
  output_parameters.channelCount = CHANNEL_COUNT; // left and right
  output_parameters.device = device;
  output_parameters.hostApiSpecificStreamInfo = NULL;
  output_parameters.sampleFormat = paFloat32;
  output_parameters.suggestedLatency = Pa_GetDeviceInfo(device)->defaultLowInputLatency;

  PaStream *stream;
  err = Pa_OpenStream(&stream,
                      &input_parameters, 
                      &output_parameters,
                      SAMPLE_RATE, 
                      FRAMES_PER_BUFFER,
                      paNoFlag,
                      pa_test_callback,
                      NULL);
  check_error(err);

  /* start portaudio stream */
  err = Pa_StartStream(stream);
  check_error(err);

  /* capture audio for length of time PA_SLEEP_MS */
  Pa_Sleep(PA_SLEEP_MS);

  /* stop portaudio stream */
  err = Pa_StopStream(stream);
  check_error(err);

  /* close portaudio stream */
  err = Pa_CloseStream(stream);
  check_error(err);

  /* terminate portaudio */
  err = Pa_Terminate();
  check_error(err);

  return EXIT_SUCCESS;
}
