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
#include <errno.h>
#include <alsa/asoundlib.h>
#include <syslog.h>

// novation launchpad mini daemon
#define MYNAME "novalpmd"

void send_device_inquiry(snd_rawmidi_t *in, snd_rawmidi_t *out) {

  char msg[] = {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7};
  snd_rawmidi_write(out, msg, sizeof(msg));

}

bool is_programmer_mode(snd_rawmidi_t* in, snd_rawmidi_t* out) {
  char v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0xF7};
  snd_rawmidi_write(out, v, sizeof(v));
  char r[9];
  snd_rawmidi_read(in, r, sizeof(r));
  return r[7] == 1;
}

void select_programmer_mode(snd_rawmidi_t* out) {
  char v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7};
  snd_rawmidi_write(out, v, sizeof(v));
}

void select_live_mode(snd_rawmidi_t* out) {
  char v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x00, 0xF7};
  snd_rawmidi_write(out, v, sizeof(v));
}

void static_color(snd_rawmidi_t *out, uint8_t note, uint8_t color) {

  uint8_t v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x03, 0x00, 0x00, 0x00, 0xF7};
  v[7] = 0;
  v[8] = note;
  v[9] = color;
  snd_rawmidi_write(out, v, sizeof(v));

}

void static_color_xy(snd_rawmidi_t *out, uint8_t x, uint8_t y, uint8_t color) {

  uint8_t led_index  = (y * 10 + x);
  static_color(out, led_index, color);

}

void pulsing_color(snd_rawmidi_t *out, uint8_t note, uint8_t color) {

  uint8_t v[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x03, 0x00, 0x00, 0x00, 0xF7};
  v[7] = 2;
  v[8] = note;
  v[9] = color;
  snd_rawmidi_write(out, v, sizeof(v));

}

bool snd_rawmidi_avail(snd_rawmidi_t *in) {

  snd_rawmidi_status_t *status;
  snd_rawmidi_status_malloc(&status);
  snd_rawmidi_status(in, status);
  size_t len = snd_rawmidi_status_get_avail(status);
  snd_rawmidi_status_free(status);

  return len > 0;

}

void pulsing_color_xy(snd_rawmidi_t *out, uint8_t x, uint8_t y, uint8_t color) {

  uint8_t led_index  = (y * 10 + x);
  pulsing_color(out, led_index, color);

}


volatile int running = 1;

void handler(int sig, siginfo_t *info, void *ucontext) {

  switch (sig) {

    case SIGHUP:
      fprintf(stderr, "Reloading by SIGHUP.");
      break;

    case SIGTERM:
      fprintf(stderr, "Terminating by SIGTERM.");
      exit(0);
      break;

  }

}

void daemonize() {

  struct sigaction act;
  act.sa_sigaction = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;
  struct sigaction oldact;
  sigaction(SIGHUP, &act, &oldact);
  sigaction(SIGTERM, &act, &oldact);

  sd_notify(

}

int main(void) {

  daemonize();
  
  openlog(MYNAME, LOG_PID, LOG_DAEMON);

  syslog(LOG_INFO, "Started.");

  while (1) { 
    syslog(LOG_NOTICE, "Alert");
    sleep(20); 
    break;
  }

  syslog(LOG_INFO, "Stopped.");

  closelog();

  return EXIT_SUCCESS;
}

/*

   char* device = "hw:1,0,1";
   // 0x80: note off
   // 0x90: note on
   // 0xB0: control change
   // 0xF0: sysex, follewed by 1 or 3 byte manufacturer id 
   // 0xF7: end of sysex
   //
   //
   // sysex format for launchpad mini mk3
   // F0 00 20 29 02 0D <data> F7
   
   snd_rawmidi_t *in = NULL;
   snd_rawmidi_t *out = NULL;

   if (snd_rawmidi_open(&in, &out, device, 0) == 0) {

     if (!is_programmer_mode(in, out)) {
      select_programmer_mode(out);
     }

     printf("ready\n");

     while (running) {

       unsigned char ch;

       if (snd_rawmidi_avail(in) == 0) continue;

       snd_rawmidi_read(in, &ch, 1);

       // sysex
       if (ch == 0xf0) {

         printf("sysex: ");

         while (running) {
           if (snd_rawmidi_avail(in) == 0) continue;
           snd_rawmidi_read(in, &ch, 1);
           if (ch == 0xf7) break;
           printf("0x%02X ", ch);
           fflush(stdout);
         }

         printf("\n");

       // note on
       } else if (ch == 0x90) {

         unsigned char note;
         snd_rawmidi_read(in, &note, 1);

         unsigned char velocity;
         snd_rawmidi_read(in, &velocity, 1);

         printf("note %u %u\n", note, velocity);

         if (velocity == 0) {
          static_color(out, note, 0);
         } else {
          static_color(out, note, 5);
         }

       }

     }

      select_live_mode(out);

   }

   if (in) {
     snd_rawmidi_drain(in);
     snd_rawmidi_close(in);
   }

   if (out) {
     snd_rawmidi_drain(out);
     snd_rawmidi_close(out);
   }
   
   return 0;
}
   
*/
