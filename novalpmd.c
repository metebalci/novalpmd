#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <alsa/asoundlib.h>

static char device[255] = {0};
static unsigned short send_port = 0;
static unsigned short recv_port = 0;

static int sockfd = 0;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;

static snd_rawmidi_t *in = NULL;
static snd_rawmidi_t *out = NULL;
volatile int running = 1;

void send_device_inquiry() {

  char msg[] = {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7};
  snd_rawmidi_write(out, msg, sizeof(msg));

}

bool is_programmer_mode() {
  char v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0xF7};
  snd_rawmidi_write(out, v, sizeof(v));
  char r[9];
  snd_rawmidi_read(in, r, sizeof(r));
  return r[7] == 1;
}

void select_programmer_mode() {
  fprintf(stderr, "Switching to programmer mode.\n");
  char v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7};
  snd_rawmidi_write(out, v, sizeof(v));
}

void select_live_mode() {
  fprintf(stderr, "Switching to live mode.\n");
  char v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x00, 0xF7};
  snd_rawmidi_write(out, v, sizeof(v));
}

void static_color(uint8_t note, uint8_t color) {

  uint8_t v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x03, 0x00, 0x00, 0x00, 0xF7};
  v[7] = 0;
  v[8] = note;
  v[9] = color;
  snd_rawmidi_write(out, v, sizeof(v));

}

void static_color_xy(uint8_t x, uint8_t y, uint8_t color) {

  uint8_t led_index  = (y * 10 + x);
  static_color(led_index, color);

}

void flashing_color(uint8_t note, uint8_t color1, uint8_t color2) {

  uint8_t v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x03, 0x00, 0x00, 0x00, 0x00, 0xF7};
  v[7] = 1;
  v[8] = note;
  v[9] = color1;
  v[10] = color2;
  snd_rawmidi_write(out, v, sizeof(v));

}

void flashing_color_xy(uint8_t x, uint8_t y, uint8_t color1, uint8_t color2) {

  uint8_t led_index  = (y * 10 + x);
  flashing_color(led_index, color1, color2);

}

void pulsing_color(uint8_t note, uint8_t color) {

  uint8_t v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x03, 0x00, 0x00, 0x00, 0xF7};
  v[7] = 2;
  v[8] = note;
  v[9] = color;
  snd_rawmidi_write(out, v, sizeof(v));

}

void pulsing_color_xy(uint8_t x, uint8_t y, uint8_t color) {

  uint8_t led_index  = (y * 10 + x);
  pulsing_color(led_index, color);

}

void rgb_color(uint8_t note, uint8_t red, uint8_t green, uint8_t blue) {

  uint8_t v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7};
  v[7] = 3;
  v[8] = note;
  v[9] = red;
  v[10] = green;
  v[11] = blue;
  snd_rawmidi_write(out, v, sizeof(v));

}

void rgb_color_xy(uint8_t x, uint8_t y, uint8_t red, uint8_t green, uint8_t blue) {

  uint8_t led_index  = (y * 10 + x);
  rgb_color(led_index, red, green, blue);

}


bool snd_rawmidi_avail() {

  snd_rawmidi_status_t *status;
  snd_rawmidi_status_malloc(&status);
  snd_rawmidi_status(in, status);
  size_t len = snd_rawmidi_status_get_avail(status);
  snd_rawmidi_status_free(status);

  return len > 0;

}


void close_all(void) {

  if (sockfd != 0) {
    close(sockfd);
  }

  if (in) {
    snd_rawmidi_drain(in);
    snd_rawmidi_close(in);
  }

  if (out) {
    snd_rawmidi_drain(out);
    snd_rawmidi_close(out);
  }
}

void load_config(void) {

  FILE *configfp = fopen("/etc/novalpmd.conf", "r");

  while (!feof(configfp)) {
    char name[255];
    char value[255];
    fscanf(configfp, "%s %s\n", name, value);
    fprintf(stderr, "Config: %s=%s\n", name, value);
    if (strcmp(name, "device") == 0) {
      strcpy(device, value);
    } else if (strcmp(name, "send_port") == 0) {
      send_port = atoi(value);
    } else if (strcmp(name, "recv_port") == 0) {
      recv_port = atoi(value);
    } else {
      fprintf(stderr, "unknown config parameter: %s\n", name);
    }
  }

  fclose(configfp);

  if (device[0] == 0 || send_port == 0 || recv_port == 0) {
    fprintf(stderr, "Error: configuration error !\n");
    close_all();
    exit(EXIT_FAILURE);
  }

  close_all();

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  int optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(recv_port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    fprintf(stderr, "Error: while binding to UDP socket !\n");
    close_all();
    exit(EXIT_FAILURE);
  }

  memset(&client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(send_port);
  inet_aton("127.0.0.1", &client_addr.sin_addr);

  if (snd_rawmidi_open(&in, &out, device, 0) != 0) {
    fprintf(stderr, "Error: cannot open midi device !\n");
    close_all();
    exit(EXIT_FAILURE);
  }

  if (!is_programmer_mode(in, out)) {
    select_programmer_mode(out);
  }

}

void run(void) {

  char msg[255];
  int msg_len;

  while (running) {

    struct sockaddr_in recv_addr;

    int n = recvfrom(
          sockfd, 
          msg, 
          255, 
          MSG_DONTWAIT,
          0,
          0);

    if (n > 0) {

      unsigned int op;
      unsigned int x, y;
      unsigned int velocity;
      unsigned int color1, color2;
      unsigned int r, g, b;
      sscanf(msg, "%u", &op);
      switch (op) {
        case 0:
          sscanf(msg, "%u %u %u %u\n", &op, &x, &y, &velocity);
          static_color_xy(x, y, velocity);
          break;

        case 1:
          sscanf(msg, "%u %u %u %u %u\n", &op, &x, &y, &color1, &color2);
          flashing_color_xy(x, y, color1, color2);
          break;

        case 2:
          sscanf(msg, "%u %u %u %u\n", &op, &x, &y, &velocity);
          pulsing_color_xy(x, y, velocity);
          break;

        case 3:
          sscanf(msg, "%u %u %u %u %u %u\n", &op, &x, &y, &r, &g, &b);
          rgb_color_xy(x, y, r/2, g/2, b/2);
          break;
      }

    }

    unsigned char ch;

    if (snd_rawmidi_avail(in) == 0) continue;

    snd_rawmidi_read(in, &ch, 1);

    // sysex
    if (ch == 0xf0) {

      fprintf(stderr, "sysex: 0xF0 ");

      while (running) {
        if (snd_rawmidi_avail(in) == 0) continue;
        snd_rawmidi_read(in, &ch, 1);
        if (ch == 0xf7) break;
        fprintf(stderr, "0x%02X ", ch);
        fflush(stderr);
      }

      fprintf(stderr, "0xF7\n");

    // note on
    } else if (ch == 0x90) {

      unsigned char note;
      snd_rawmidi_read(in, &note, 1);

      unsigned char velocity;
      snd_rawmidi_read(in, &velocity, 1);

      unsigned int x = note % 10;
      unsigned int y = note / 10;

      sprintf(msg, "%u %u %u\n", x, y, velocity);

      sendto(
          sockfd, 
          msg, 
          strlen(msg), 
          MSG_DONTROUTE,
          (struct sockaddr *) &client_addr,
          sizeof(client_addr));

    }

  }

}

void handler(int sig, siginfo_t *info, void *ucontext) {

  switch (sig) {

    case SIGHUP:
      fprintf(stderr, "Re-starting by SIGHUP...\n");
      load_config();
      fprintf(stderr, "Re-started.\n");
      break;

    case SIGTERM:
      fprintf(stderr, "Terminating by SIGTERM.\n");
      running = 0;
      break;

  }

}

int main(void) {

  fprintf(stderr, "Starting...\n");
  
  struct sigaction act;
  act.sa_sigaction = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;
  struct sigaction oldact;
  sigaction(SIGHUP, &act, &oldact);
  sigaction(SIGTERM, &act, &oldact);

  load_config();

  fprintf(stderr, "Started.\n");

  run();

  fprintf(stderr, "Stopping...\n");

  select_live_mode(out);
  close_all();

  fprintf(stderr, "Stopped.\n");

  return EXIT_SUCCESS;
}
