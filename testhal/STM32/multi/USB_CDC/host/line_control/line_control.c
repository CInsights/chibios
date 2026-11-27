#define main usb_regression_main
#include "otg_hld.c"
#undef main
#include "sdu_model.h"
#include "hal_serial_usb.c"

static SerialUSBDriver sdus[5];
static unsigned notifications[5], checks;
#define CHECK(x) do { checks++; assert(x); } while (0)

static void line_notify(SerialUSBDriver *sdup, eventflags_t flags) {
  sdu_line_state_t snapshot;
  CHECK(test_locked && flags != 0U);
  sduGetLineStateI(sdup, &snapshot);
  CHECK(snapshot.control_lines == sdup->control_lines);
  notifications[sdup - sdus]++;
}

static void line_event(USBDriver *usbp, usbevent_t event) {
  event_cb(usbp,event);
  osalSysLockFromISR();
  for (unsigned i=0;i<5;i++) {
    if (sdus[i].state != SDU_READY || sdus[i].config->usbp != usbp) continue;
    switch(event) {
      case USB_EVENT_CONFIGURED: sduConfigureHookI(&sdus[i]); break;
      case USB_EVENT_RESET:
      case USB_EVENT_UNCONFIGURED:
      case USB_EVENT_SUSPEND: sduSuspendHookI(&sdus[i]); break;
      case USB_EVENT_WAKEUP: sduWakeupHookI(&sdus[i]); break;
      default: break;
    }
  }
  osalSysUnlockFromISR();
}

static const USBConfig line_cfg = {
  .event_cb=line_event,.get_descriptor_cb=get_descriptor,
  .requests_hook_cb=sduRequestsHook,.sof_cb=NULL
};

static void get_line(unsigned port,uint32_t baud,uint16_t lines) {
  sdu_line_state_t s;
  sduGetLineState(&sdus[port],&s);
  CHECK(s.baud==baud && s.control_lines==lines);
}

static void finish(USBDriver *usb) {
  for(unsigned n=0;usb->ep0state!=USB_EP0_STP_WAITING;n++) {
    CHECK(n<8 && usb->ep0state!=USB_EP0_ERROR);
    ep0_host_step(usb);
  }
}

static void set_line(unsigned port,const uint8_t data[7],size_t received) {
  USBDriver *usb=sdus[port].config->usbp;
  host_setup(usb,0x21,CDC_SET_LINE_CODING,0,sdus[port].config->control_if,7);
  CHECK(usb->ep0state==USB_EP0_OUT_RX);
  out_data(usb,0,received);
  /* The FIFO model repeats one word; provide the specified payload bytes. */
  memcpy(&sdus[port].pending_linecoding,data,received);
  out_xfrc(usb,0);
}

static void control_lines(unsigned port,uint16_t lines) {
  USBDriver *usb=sdus[port].config->usbp;
  unsigned old=notifications[port];
  unsigned in_resets=sdus[port].ibqueue.resets;
  unsigned out_resets=sdus[port].obqueue.resets;
  host_setup(usb,0x21,CDC_SET_CONTROL_LINE_STATE,lines,
             sdus[port].config->control_if,0);
  CHECK(notifications[port]==old);
  CHECK(usb->ep0state==USB_EP0_IN_SENDING_STS);
  finish(usb);
  CHECK(sdus[port].ibqueue.resets==in_resets);
  CHECK(sdus[port].obqueue.resets==out_resets);
}

int main(void) {
  SerialUSBConfig conf[5];
  const uint8_t coding[3][7]={{0x00,0xC2,0x01,0,0,0,8},
                             {0x00,0x84,0x03,0,2,2,7},
                             {0x80,0x25,0,0,1,1,6}};
  const uint32_t rates[3]={115200,230400,9600};
  CHECK(usb_regression_main()==0);
  for(unsigned i=0;i<5;i++) sduObjectInit(&sdus[i]);
  sduInit();
  fresh_device(&USBD1); fresh_device(&USBD2);
  for(unsigned i=0;i<4;i++) {
    conf[i]=(SerialUSBConfig){i==3?&USBD2:&USBD1,
                  i==3?1U:i+1U,i==3?1U:i+1U,0U,
                  i==3?0U:i*3U,line_notify};
    CHECK(sduStart(&sdus[i],&conf[i])==MSG_OK);
  }
  conf[4]=conf[0];
  CHECK(sduStart(&sdus[4],&conf[4])==HAL_RET_CONFIG_ERROR);
  CHECK(sdus[4].state==SDU_STOP);
  CHECK(sduStart(&sdus[0],&conf[0])==MSG_OK);
  CHECK(sduStart(&sdus[0],&conf[1])==HAL_RET_INV_STATE);
  enumerate(&USBD1,&line_cfg); enumerate(&USBD2,&line_cfg);
  for(unsigned i=0;i<4;i++) get_line(i,38400,0);

  for(unsigned cycle=0;cycle<25;cycle++) {
    for(unsigned i=0;i<3;i++) {
      unsigned n=notifications[i];
      set_line(i,coding[i],7);
      CHECK(notifications[i]==n); /* No notification before status. */
      finish(&USBD1);
      get_line(i,rates[i],cycle==0?0:3);
      sdu_line_state_t state;
      sduGetLineState(&sdus[i],&state);
      CHECK(state.stop_bits==coding[i][4] && state.parity==coding[i][5] &&
            state.data_bits==coding[i][6]);
      control_lines(i,0); control_lines(i,1); control_lines(i,3);
      CHECK(sdus[i].event & SDU_DTR_ASSERTED);
      host_setup(&USBD1,0xA1,CDC_GET_LINE_CODING,0,conf[i].control_if,7);
      CHECK(memcmp(USBD1.ep0next,coding[i],7)==0);
      finish(&USBD1);
      get_line(3,38400,0); /* Same interface number on another controller. */
    }
  }
  for(unsigned i=0;i<3;i++) get_line(i,rates[i],3);
  unsigned notices=notifications[0];
  control_lines(0,3);
  CHECK(notifications[0]==notices); /* Repeated request is not a transition. */

  const uint8_t invalid[][8]={
    {0x21,CDC_SET_CONTROL_LINE_STATE,0,0,0,0,1,0},
    {0xA1,CDC_SET_CONTROL_LINE_STATE,0,0,0,0,0,0},
    {0x20,CDC_SET_CONTROL_LINE_STATE,0,0,0,0,0,0},
    {0x21,CDC_SET_CONTROL_LINE_STATE,4,0,0,0,0,0},
    {0x21,CDC_SET_CONTROL_LINE_STATE,0,0,0,1,0,0},
    {0x21,CDC_SET_CONTROL_LINE_STATE,0,0,1,0,0,0},
    {0x21,CDC_SET_LINE_CODING,0,0,0,0,6,0},
    {0x21,CDC_SET_LINE_CODING,0,0,0,0,8,0},
    {0x21,CDC_SET_LINE_CODING,1,0,0,0,7,0},
    {0xA1,CDC_SET_LINE_CODING,0,0,0,0,7,0},
    {0x21,CDC_GET_LINE_CODING,0,0,0,0,7,0}};
  for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
    const uint8_t *r=invalid[i];
    host_setup(&USBD1,r[0],r[1],r[2]|r[3]<<8,r[4]|r[5]<<8,r[6]|r[7]<<8);
    CHECK(USBD1.ep0state==USB_EP0_ERROR && USBD1.ep0endcb==NULL);
    CHECK(notifications[0]==notices);
    get_line(0,115200,3);
  }
  for(unsigned length=0;length<7;length++) {
    set_line(0,coding[1],length);
    CHECK(USBD1.ep0state==USB_EP0_ERROR && USBD1.ep0endcb==NULL);
    CHECK(notifications[0]==notices);
    get_line(0,115200,3);
  }
  /* Complete data, but replace the request before its status completes. */
  set_line(0,coding[1],7);
  host_setup(&USBD1,0xA1,CDC_GET_LINE_CODING,0,conf[1].control_if,7);
  finish(&USBD1);
  get_line(0,115200,3);
  CHECK(notifications[0]==notices);
  host_setup(&USBD1,0x21,CDC_SET_CONTROL_LINE_STATE,0,0,0);
  host_setup(&USBD1,0x21,CDC_SET_CONTROL_LINE_STATE,1,conf[1].control_if,0);
  finish(&USBD1);
  get_line(0,115200,3); get_line(1,230400,1);
  CHECK(notifications[0]==notices);

  /* Suspend keeps settings but invalidates a staged control change. */
  host_setup(&USBD1,0x21,CDC_SET_CONTROL_LINE_STATE,0,0,0);
  test_isr=true; _usb_suspend(&USBD1); test_isr=false;
  get_line(0,115200,3);
  CHECK(sdus[0].pending_request==0);
  test_isr=true; _usb_wakeup(&USBD1); test_isr=false;
  get_line(0,115200,3);
  /* Reset after already-suspended queues must still clear settings. */
  test_isr=true; _usb_suspend(&USBD1); test_isr=false;
  bus_reset(&USBD1);
  for(unsigned i=0;i<3;i++) get_line(i,38400,0);
  CHECK(notifications[0]==notices+1);
  CHECK(sdus[0].event & SDU_DTR_DEASSERTED);
  CHECK(control(&USBD1,0x00,USB_REQ_SET_CONFIGURATION,1,0,0)==0);
  set_line(0,coding[0],7); finish(&USBD1); control_lines(0,3);
  CHECK(control(&USBD1,0x00,USB_REQ_SET_CONFIGURATION,0,0,0)==0);
  get_line(0,38400,0);
  host_setup(&USBD1,0x21,CDC_SET_CONTROL_LINE_STATE,1,0,0);
  CHECK(USBD1.ep0state==USB_EP0_ERROR); /* Unconfigured requests refused. */

  CHECK(control(&USBD1,0x00,USB_REQ_SET_CONFIGURATION,1,0,0)==0);
  set_line(0,coding[0],7);
  notices=notifications[0];
  sduStop(&sdus[0]); finish(&USBD1);
  get_line(0,38400,0); CHECK(notifications[0]==notices);
  CHECK(sduStart(&sdus[0],&conf[3])==HAL_RET_INV_STATE);
  CHECK(sduStart(&sdus[0],&conf[0])==MSG_OK);
  control_lines(0,1); get_line(0,38400,1);
  for(unsigned i=0;i<4;i++) sduStop(&sdus[i]);
  CHECK(line_drivers==NULL);
  usbStop(&USBD1); usbStop(&USBD2);
  CHECK(sduStart(&sdus[0],&conf[3])==MSG_OK);
  sduStop(&sdus[0]);
  printf("PASS: SerialUSB line-control %u checks; 25 interleaved cycles, "
         "three ports, two controllers, malformed/short/replaced requests, "
         "reset/suspend/unconfigure/stop/rebind\n",checks);
  return 0;
}
