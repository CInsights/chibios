#define main usb_regression_main
#include "otg_hld.c"
#undef main
static size_t short_count;
static void receive_step(void) {
  if (USBD1.ep0state==USB_EP0_OUT_RX) {
    out_data(&USBD1,0,short_count);
    out_xfrc(&USBD1,0);
  } else {
    ep0_host_step(&USBD1);
  }
}
int main(void) {
  uint8_t buffer[7];
  assert(usb_regression_main()==0);
  fresh_device(&USBD1);
  enumerate(&USBD1,&cfg);
  for(short_count=0;short_count<=7;short_count++) {
    host_setup(&USBD1,0x21,0x20,0,0,7);
    assert(usbEp0WaitSetup(&USBD1)==MSG_OK);
    test_suspend_hook=receive_step;
    msg_t result=usbEp0Receive(&USBD1,buffer,sizeof(buffer));
    test_suspend_hook=NULL;
    assert(result==(short_count==7?MSG_OK:MSG_RESET));
    assert(USBD1.ep0state==(short_count==7?USB_EP0_STP_WAITING:USB_EP0_ERROR));
    assert(USBD1.ep0endcb==NULL);
  }
  usbStop(&USBD1);
  puts("PASS: EP0 worker rejects all 0..6-byte short writes; 7-byte write succeeds");
  return 0;
}
