/* Host-only queue/event stand-ins. USB core, USB LLD and SDU are real sources. */
#define HAL_USE_SERIAL_USB TRUE
#define HAL_RET_CONFIG_ERROR ((msg_t)-16)
#define HAL_RET_INV_STATE ((msg_t)-22)
#define HAL_RET_UNKNOWN_CTL ((msg_t)-18)
#define CHN_CTL_NOP 1
#define CHN_CTL_INVALID 0
#define TIME_INFINITE UINT32_MAX
#define CHN_CONNECTED 1U
#define CHN_DISCONNECTED 2U
#define CHN_INPUT_AVAILABLE 4U
#define CHN_OUTPUT_EMPTY 8U
#define CHN_TRANSMISSION_END 16U
typedef uint32_t eventflags_t;
typedef eventflags_t event_source_t;
#define osalEventObjectInit(p) (*(p)=0U)
#define _base_asynchronous_channel_data event_source_t event;
#define _base_asynchronous_channel_methods \
  size_t offset; \
  size_t (*write)(void *, const uint8_t *, size_t); \
  size_t (*read)(void *, uint8_t *, size_t); \
  msg_t (*put)(void *, uint8_t); msg_t (*get)(void *); \
  msg_t (*putt)(void *, uint8_t, sysinterval_t); \
  msg_t (*gett)(void *, sysinterval_t); \
  size_t (*writet)(void *, const uint8_t *, size_t, sysinterval_t); \
  size_t (*readt)(void *, uint8_t *, size_t, sysinterval_t); \
  msg_t (*ctl)(void *, unsigned, void *);
#define chnAddFlagsI(p,f) do { assert(test_locked); (p)->event |= (f); } while (0)
typedef struct { bool suspended; void *link; unsigned resets; } io_buffers_queue_t;
typedef io_buffers_queue_t input_buffers_queue_t;
typedef io_buffers_queue_t output_buffers_queue_t;
#define BQ_BUFFER_SIZE(n,s) ((n)*((s)+sizeof(size_t)))
#define bqGetLinkX(q) ((q)->link)
#define bqIsSuspendedX(q) ((q)->suspended)
#define bqSuspendI(q) ((q)->suspended=true)
#define bqResumeX(q) ((q)->suspended=false)
#define ibqResetI(q) ((q)->resets++)
#define obqResetI(q) ibqResetI(q)
#define ibqObjectInit(q,s,b,z,n,cb,p) do { \
  (void)(b); (void)(z); (void)(n); (void)(cb); \
  *(q)=(io_buffers_queue_t){(s),(p),0U}; } while (0)
#define obqObjectInit(q,s,b,z,n,cb,p) ibqObjectInit(q,s,b,z,n,cb,p)
#define ibqGetEmptyBufferI(q) ((void)(q),(uint8_t *)NULL)
#define obqGetFullBufferI(q,n) ((void)(q),(void)(n),(uint8_t *)NULL)
#define obqTryFlushI(q) ((void)(q),false)
#define obqReleaseEmptyBufferI(q) ((void)(q))
#define ibqPostFullBufferI(q,n) ((void)(q),(void)(n))
#define obqWriteTimeout(q,b,n,t) ((void)(q),(void)(b),(void)(t),(n))
#define ibqReadTimeout(q,b,n,t) ((void)(q),(void)(b),(void)(n),(void)(t),(size_t)0)
#define obqPutTimeout(q,b,t) ((void)(q),(void)(b),(void)(t),MSG_OK)
#define ibqGetTimeout(q,t) ((void)(q),(void)(t),MSG_TIMEOUT)
#include "hal_serial_usb.h"
