#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>
#include <SDL2/SDL.h>

typedef unsigned short u16;
typedef int            i32;
typedef unsigned long  u32;
typedef float          f32;

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define CHANNEL_COUNT 2

#define TERMINAL_DISPLAY_SIZE 100
#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 720
#define SCREEN_WIDTH_2 SCREEN_WIDTH / 2
#define SCREEN_WIDTH_4 SCREEN_WIDTH / 4
#define SCREEN_HEIGHT_2 SCREEN_HEIGHT / 2
#define SCREEN_HEIGHT_4 SCREEN_HEIGHT / 4
#define SCREEN_HEIGHT_8 SCREEN_HEIGHT / 8
#define BACKGROUND 0x000F
#define WHITE 0xFFFF
#define RED 0xF00F
#define GREEN 0x0F0F
#define YELLOW 0xFF0F


#define ASSERT(_e, ...) if (_e) { fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE); }

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

u16 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];

static void check_error(const PaError err)
{
  if (err != paNoError)
  {
    printf("portaudio error: %s\n", Pa_GetErrorText(err));
  }
}

static inline f32 max(const f32 a, const f32 b)
{
  return a > b ? a : b;
}

static inline f32 absolute_value(const f32 a)
{
  return a > 0 ? a : -a;
}

static inline void screen_clear(u16 *pixels, const u32 size)
{
  for (u32 i = 0; i < size; i++)
  {
    pixels[i] = 0;
  }
}

static inline void screen_shift_right(u16 *pixels, const u32 width, const u32 height)
{
  for (i32 i = 0; i < height; i++)
  {
    for (i32 j = 0; j < width; j++)
    {
      if (j == 0) continue;

      pixels[i * width + j - 1] = pixels[i * width + j];
    }
    pixels[i * width] = BACKGROUND;
  }
}

static void terminal_render(const f32 volume_left, const f32 volume_right)
{
  printf("\r"); // carriage return

  for (i32 i = 0; i < TERMINAL_DISPLAY_SIZE; i++)
  {
    f32 bar_proportion = i / (f32)TERMINAL_DISPLAY_SIZE;

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
}

static void video_render(const f32 volume_left, const f32 volume_right)
{
  /* background */
  screen_shift_right(pixels, SCREEN_WIDTH, SCREEN_HEIGHT);

  for (i32 i = 0; i < SCREEN_HEIGHT_2; i++)
  {
    // adjusting this changes the sensitivity of the input
    f32 bar_proportion = i / (f32)SCREEN_HEIGHT * 2;

    if (bar_proportion <= volume_left)
    {
      u32 index = (SCREEN_WIDTH * SCREEN_HEIGHT_2 + SCREEN_WIDTH_4) - 
        (i * SCREEN_WIDTH + SCREEN_WIDTH_2);

      u16 color = 0x000F;

      if (i < SCREEN_HEIGHT_4) color = GREEN;
      else if (i < SCREEN_HEIGHT_4 + SCREEN_HEIGHT_8) color = YELLOW;
      else color = RED;

      pixels[index] = color;
    }
    if (bar_proportion <= volume_right)
    {
      u32 index = (SCREEN_WIDTH * SCREEN_HEIGHT_2 + SCREEN_WIDTH_4) + 
        (i * SCREEN_WIDTH + SCREEN_WIDTH_2);

      u16 color = 0x000F;

      if (i < SCREEN_HEIGHT_4) color = GREEN;
      else if (i < SCREEN_HEIGHT_4 + SCREEN_HEIGHT_8) color = YELLOW;
      else color = RED;

      pixels[index] = color;
    }
  }

  /* copy pixels to texture to renderer */
  const u32 pitch = 2;
  SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * pitch);
  SDL_Rect rect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
  SDL_RenderCopy(renderer, texture, &rect, NULL);

  /* draw to screen */
  SDL_RenderPresent(renderer);
}

static i32 pa_test_callback(const void *input_buffer, 
                            void *output_buffer, 
                            u32 frames_per_buffer, 
                            const PaStreamCallbackTimeInfo *time_info,
                            PaStreamCallbackFlags status_flags,
                            void *user_data)
{
  f32 *in = (f32*)input_buffer;
  (void)output_buffer; // cast to void so compiler do not get angry 
  
  f32 *volumes = (f32*)user_data;
  
  ASSERT(volumes == NULL, "volumes is not defined\n");

  volumes[0] = 0;
  volumes[1] = 0;

  for (u32 i = 0; i < frames_per_buffer * 2; i += 2)
  {
    volumes[0] = max(volumes[0], absolute_value(in[i]));
    volumes[1] = max(volumes[1], absolute_value(in[i+1]));
  }

  return paContinue;
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
    printf("->name: %s:\n", device_info->name);
    printf("->max input channels: %d\n", device_info->maxInputChannels);
    printf("->max output channels: %d\n", device_info->maxOutputChannels);
    printf("->default sample rate: %lf\n", device_info->defaultSampleRate);
  }

  /* user input device */
  i32 device;

  printf("select a device: ");
  i32 temp = scanf("%d", &device);

  if (temp != 1)
  {
    printf("incorrect device input\n");
    exit(EXIT_FAILURE);
  }
  else if (device > num_devices)
  {
    printf("selected device does not exist\n");
    exit(EXIT_FAILURE);
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

  /* initalize SDL2 */
  ASSERT(SDL_Init(SDL_INIT_VIDEO) > 0, "failed to initialize video\n");

  /* window */
  window = SDL_CreateWindow("unknown", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                            SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
  ASSERT(window == NULL, "failed to create window\n");

  /* renderer */
  renderer = SDL_CreateRenderer(window, -1, 
                                SDL_RENDERER_ACCELERATED | 
                                SDL_RENDERER_PRESENTVSYNC);
  ASSERT(renderer == NULL, "failed to create renderer\n");

  /* texture */
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444,
                              SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                              SCREEN_HEIGHT);
  ASSERT(texture == NULL, "failed to create texture\n");

  /* volume[0] = left; volume[1] = right */
  f32 volumes[2] = { 0, 0 };

  PaStream *stream;
  err = Pa_OpenStream(&stream,
                      &input_parameters, 
                      &output_parameters,
                      SAMPLE_RATE, 
                      FRAMES_PER_BUFFER,
                      paNoFlag,
                      pa_test_callback,
                      volumes);
  check_error(err);

  /* start portaudio stream */
  err = Pa_StartStream(stream);
  check_error(err);

  /* capture audio for length of time PA_SLEEP_MS */
  //Pa_Sleep(10 * 1000);

  /* frame loop */
  u32 running = 1;
  SDL_Event event;

  while (running)
  {
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_QUIT) running = 0;
    }

    terminal_render(volumes[0], volumes[1]);
    video_render(volumes[0], volumes[1]);

    Pa_Sleep(32);
  }

  /* stop portaudio stream */
  err = Pa_StopStream(stream);
  check_error(err);

  /* close portaudio stream */
  err = Pa_CloseStream(stream);
  check_error(err);

  /* terminate portaudio */
  err = Pa_Terminate();
  check_error(err);

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyTexture(texture);
  SDL_Quit();

  return EXIT_SUCCESS;
}
