#ifndef TIME_MSG_H
#define TIME_MSG_H

/* Message sent from ntp_task to speech_task via the time_queue */
typedef struct {
    int hour;    /* 0-23, IST */
    int minute;  /* 0-59     */
    int second;  /* 0-59     */
} time_msg;

#endif /* TIME_MSG_H */
