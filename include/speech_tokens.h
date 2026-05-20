#ifndef SPEECH_TOKENS_H
#define SPEECH_TOKENS_H

/*
 * speech_tokens.h
 * Token strings used by speech_task.c to emit the spoken-time protocol.
 *
 * The token protocol works as follows:
 *   1. The firmware prints "TOKEN <NAME>\r\n" for each word token.
 *   2. After all tokens for one utterance, it prints "END\r\n".
 *   3. tts_bridge.py reads these lines from QEMU stdout, collects the
 *      token text, then speaks the assembled phrase via the host TTS engine.
 *
 * Example output for 14:32:05 IST:
 *   TOKEN THE
 *   TOKEN TIME
 *   TOKEN IS
 *   TOKEN FOURTEEN
 *   TOKEN HOURS
 *   TOKEN THIRTY TWO
 *   TOKEN MINUTES
 *   TOKEN AND
 *   TOKEN FIVE
 *   TOKEN SECONDS
 *   END
 *
 * FIX: This file was missing from the original submission.
 *      The assignment specification (Section 7) requires it explicitly.
 */

/* ---- Structural tokens ------------------------------------------------ */
#define TOKEN_THE       "THE"
#define TOKEN_TIME      "TIME"
#define TOKEN_IS        "IS"
#define TOKEN_HOURS     "HOURS"
#define TOKEN_MINUTES   "MINUTES"
#define TOKEN_AND       "AND"
#define TOKEN_SECONDS   "SECONDS"

/* ---- Protocol markers ------------------------------------------------- */
/* Prefix printed before every token word */
#define TOKEN_PREFIX    "TOKEN "

/* Marker printed once after the last token of an utterance */
#define TOKEN_END_MARKER "END"

/* ---- Numeric word tokens (ones, 0-19) --------------------------------- */
#define TOKEN_ZERO      "ZERO"
#define TOKEN_ONE       "ONE"
#define TOKEN_TWO       "TWO"
#define TOKEN_THREE     "THREE"
#define TOKEN_FOUR      "FOUR"
#define TOKEN_FIVE      "FIVE"
#define TOKEN_SIX       "SIX"
#define TOKEN_SEVEN     "SEVEN"
#define TOKEN_EIGHT     "EIGHT"
#define TOKEN_NINE      "NINE"
#define TOKEN_TEN       "TEN"
#define TOKEN_ELEVEN    "ELEVEN"
#define TOKEN_TWELVE    "TWELVE"
#define TOKEN_THIRTEEN  "THIRTEEN"
#define TOKEN_FOURTEEN  "FOURTEEN"
#define TOKEN_FIFTEEN   "FIFTEEN"
#define TOKEN_SIXTEEN   "SIXTEEN"
#define TOKEN_SEVENTEEN "SEVENTEEN"
#define TOKEN_EIGHTEEN  "EIGHTEEN"
#define TOKEN_NINETEEN  "NINETEEN"

/* ---- Numeric word tokens (tens, 20-50) -------------------------------- */
#define TOKEN_TWENTY    "TWENTY"
#define TOKEN_THIRTY    "THIRTY"
#define TOKEN_FORTY     "FORTY"
#define TOKEN_FIFTY     "FIFTY"

#endif /* SPEECH_TOKENS_H */
