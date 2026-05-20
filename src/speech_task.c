/* speech_task.c
 * Gets time_msg from the queue and speaks it over UART as tokens.
 * tts_bridge.py on the host reads TOKEN/END lines and calls the TTS engine.
 */

#include "../include/speech_tokens.h"
#include "../include/time_msg.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

extern QueueHandle_t time_queue;

/* ones[] covers 0-19; tens_words[] covers 20,30,...50 */
static const char *ones[] = {
    TOKEN_ZERO,    TOKEN_ONE,       TOKEN_TWO,      TOKEN_THREE,
    TOKEN_FOUR,    TOKEN_FIVE,      TOKEN_SIX,      TOKEN_SEVEN,
    TOKEN_EIGHT,   TOKEN_NINE,      TOKEN_TEN,      TOKEN_ELEVEN,
    TOKEN_TWELVE,  TOKEN_THIRTEEN,  TOKEN_FOURTEEN, TOKEN_FIFTEEN,
    TOKEN_SIXTEEN, TOKEN_SEVENTEEN, TOKEN_EIGHTEEN, TOKEN_NINETEEN};

static const char *tens_words[] = {"",           "",          TOKEN_TWENTY,
                                   TOKEN_THIRTY, TOKEN_FORTY, TOKEN_FIFTY};

/* Write English words for n into buf.
 * max_val: 23 for hours, 59 for minutes/seconds. */
static void num_to_words(int n, int max_val, char *buf, int buflen) {
  if (n < 0 || n > max_val) {
    snprintf(buf, (size_t)buflen, "ERROR");
    printf("[SPEECH] WARNING: value %d out of range 0-%d\r\n", n, max_val);
    return;
  }
  if (n < 20) {
    snprintf(buf, (size_t)buflen, "%s", ones[n]);
  } else {
    int t = n / 10;
    int o = n % 10;
    if (o == 0)
      snprintf(buf, (size_t)buflen, "%s", tens_words[t]);
    else
      snprintf(buf, (size_t)buflen, "%s %s", tens_words[t], ones[o]);
  }
}

static void uppercase(char *dst, const char *src, int buflen) {
  int i;
  for (i = 0; src[i] != '\0' && i < buflen - 1; ++i)
    dst[i] = (char)toupper((unsigned char)src[i]);
  dst[i] = '\0';
}

static void emit_token(const char *token) {
  printf(TOKEN_PREFIX "%s\r\n", token);
  fflush(stdout);
}

static void emit_end(void) {
  printf(TOKEN_END_MARKER "\r\n");
  fflush(stdout);
}

static void emit_number_token(int n, int max_val) {
  char words[40];
  char upper[40];
  num_to_words(n, max_val, words, sizeof(words));
  uppercase(upper, words, (int)sizeof(upper));
  emit_token(upper);
}

/* Emit the full TOKEN stream; tts_bridge.py assembles these into speech. */
static void emit_time_tokens(const time_msg *msg) {
  printf("[SPEECH] Token stream begin\r\n");
  emit_token(TOKEN_THE);
  emit_token(TOKEN_TIME);
  emit_token(TOKEN_IS);
  emit_number_token(msg->hour, 23);
  emit_token(TOKEN_HOURS);
  emit_number_token(msg->minute, 59);
  emit_token(TOKEN_MINUTES);
  emit_token(TOKEN_AND);
  emit_number_token(msg->second, 59);
  emit_token(TOKEN_SECONDS);
  emit_end();
  printf("[SPEECH] Token stream end\r\n");
}

void speech_task(void *param) {
  (void)param;
  time_msg msg;

  printf("[SPEECH] Task started – waiting for time\r\n");

  while (1) {
    if (xQueueReceive(time_queue, &msg, portMAX_DELAY) == pdTRUE) {
      printf("[SPEECH] Announcing %02d:%02d:%02d IST\r\n", msg.hour, msg.minute,
             msg.second);
      emit_time_tokens(&msg);
    }
  }
}